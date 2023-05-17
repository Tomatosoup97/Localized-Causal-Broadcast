use crate::conf::TASK_COMPATIBILITY;
use crate::hosts::Node;
use crate::udp::{Payload, PayloadKind};
use std::collections::{HashMap, HashSet};
use std::fs::OpenOptions;
use std::io::prelude::*;
use std::path::Path;
use std::sync::mpsc::{Receiver, Sender};
use std::sync::{Arc, Mutex};

type OwnerID = u32;
type SenderID = u32;
type PacketID = u32;

pub struct DeliveredSet {
    acked: HashMap<SenderID, HashMap<OwnerID, HashSet<PacketID>>>,
    acked_counter: HashMap<SenderID, HashMap<OwnerID, u32>>,
    total_nodes: u32,
}

pub struct AccessDeliveredSet {
    pub delivered: Arc<Mutex<DeliveredSet>>,
    pub tx_writing: Sender<LogEvent>,
}

impl AccessDeliveredSet {
    pub fn new(delivered: DeliveredSet, tx_writing: Sender<LogEvent>) -> Self {
        Self {
            delivered: Arc::new(Mutex::new(delivered)),
            tx_writing,
        }
    }

    pub fn insert(&self, payload: Payload) {
        let contents = String::from_utf8(payload.buffer).unwrap();
        let mut delivered = self.delivered.lock().unwrap();

        let acked = delivered
            .acked
            .entry(payload.sender_id)
            .or_insert(HashMap::new())
            .entry(payload.owner_id)
            .or_insert(HashSet::new());

        let already_acked = acked.contains(&payload.packet_uid);

        acked.insert(payload.packet_uid);

        if !already_acked && !payload.kind.is_ack() {
            delivered
                .acked_counter
                .entry(payload.sender_id)
                .or_insert(HashMap::new())
                .entry(payload.owner_id)
                .and_modify(|acked_counter| *acked_counter += 1)
                .or_insert(1);

            let can_deliver = match payload.kind {
                PayloadKind::Tcp => true,
                PayloadKind::Beb => true,
                PayloadKind::Urb => {
                    self.can_urb_deliver(payload.sender_id, payload.owner_id)
                }
                PayloadKind::Rb => panic!("Unsupported payload kind: Rb"),
                PayloadKind::Ack => unreachable!(),
            };

            if can_deliver {
                self.tx_writing
                    .send(LogEvent::Delivery {
                        sender_id: payload.sender_id,
                        kind: payload.kind,
                        contents,
                    })
                    .unwrap();
            }
        }
    }

    pub fn contains(
        &self,
        sender_id: SenderID,
        owner_id: OwnerID,
        packet_uid: PacketID,
    ) -> bool {
        let delivered = self.delivered.lock().unwrap();
        let acked = delivered
            .acked
            .get(&sender_id)
            .and_then(|acked| acked.get(&owner_id));
        match acked {
            Some(acked) => acked.contains(&packet_uid),
            None => false,
        }
    }

    fn can_urb_deliver(&self, sender_id: SenderID, owner_id: OwnerID) -> bool {
        let delivered = self.delivered.lock().unwrap();
        let acked_counter = delivered
            .acked_counter
            .get(&sender_id)
            .and_then(|acked_counter| acked_counter.get(&owner_id));
        match acked_counter {
            Some(acked_counter) => *acked_counter >= self.majority(),
            None => false,
        }
    }

    fn majority(&self) -> u32 {
        (self.delivered.lock().unwrap().total_nodes / 2) + 1
    }
}

impl Clone for AccessDeliveredSet {
    fn clone(&self) -> Self {
        Self {
            delivered: Arc::clone(&self.delivered),
            tx_writing: self.tx_writing.clone(),
        }
    }
}

impl DeliveredSet {
    pub fn new(total_nodes: u32) -> Self {
        Self {
            acked: HashMap::new(),
            acked_counter: HashMap::new(),
            total_nodes,
        }
    }
}

#[derive(Debug)]
pub enum LogEvent {
    Dispatch {
        recipient: Option<Node>,
        kind: PayloadKind,
        contents: String,
    },
    Delivery {
        sender_id: u32,
        kind: PayloadKind,
        contents: String,
    },
}

pub fn keep_writing_delivered_messages(
    path: &str,
    rx_writing: Receiver<LogEvent>,
) -> Result<(), Box<dyn std::error::Error>> {
    let path = Path::new(path);

    let mut file = OpenOptions::new().write(true).open(path)?;

    for log_event in rx_writing {
        match log_event {
            LogEvent::Dispatch {
                recipient,
                kind,
                contents,
            } => {
                if TASK_COMPATIBILITY {
                    writeln!(file, "b {}", contents)?;
                } else {
                    writeln!(file, "send {:?} {}", kind, contents)?;
                }
            }
            LogEvent::Delivery {
                sender_id,
                kind,
                contents,
            } => {
                if TASK_COMPATIBILITY {
                    writeln!(file, "d {} {}", sender_id, contents)?;
                } else {
                    writeln!(file, "deliver {:?} {} {}", kind, sender_id, contents)?;
                }
            }
        }
    }
    Ok(())
}

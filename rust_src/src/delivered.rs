use crate::conf::TASK_COMPATIBILITY;
use crate::hosts::Node;
use crate::udp::{Payload, PayloadKind};
use std::collections::{HashMap, HashSet};
use std::fs::OpenOptions;
use std::io::prelude::*;
use std::path::Path;
use std::sync::mpsc::{Receiver, Sender};
use std::sync::{Arc, Mutex};

type _OwnerID = u32;
type SenderID = u32;
type PacketID = u32;

pub struct DeliveredSet {
    acked: HashMap<SenderID, HashSet<PacketID>>,
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
            .or_insert(HashSet::new());

        let already_acked = acked.contains(&payload.packet_uid);

        acked.insert(payload.packet_uid);

        if !already_acked && !payload.kind.is_ack() {
            self.tx_writing
                .send(LogEvent::Delivery {
                    sender_id: payload.sender_id,
                    kind: payload.kind,
                    contents,
                })
                .unwrap();
        }
    }

    pub fn contains(&self, sender_id: SenderID, packet_uid: PacketID) -> bool {
        let delivered = self.delivered.lock().unwrap();
        let acked = delivered.acked.get(&sender_id);
        match acked {
            Some(acked) => acked.contains(&packet_uid),
            None => false,
        }
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
    pub fn new() -> Self {
        Self {
            acked: HashMap::new(),
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

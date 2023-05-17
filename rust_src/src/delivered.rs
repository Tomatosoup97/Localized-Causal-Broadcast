use crate::conf::TASK_COMPATIBILITY;
use crate::hosts::Node;
use crate::udp::{OwnerID, PacketID, Payload, PayloadKind, SenderID};
use std::collections::{HashMap, HashSet};
use std::fs::OpenOptions;
use std::io::prelude::*;
use std::path::Path;
use std::sync::mpsc::{Receiver, Sender};
use std::sync::{Arc, Mutex, MutexGuard};

#[derive(Debug)]
pub struct DeliveredSet {
    acked: HashMap<SenderID, HashMap<OwnerID, HashSet<PacketID>>>,
    acked_counter: HashMap<OwnerID, HashMap<PacketID, u32>>,
    received_up_to: HashMap<OwnerID, u32>,
    undelivered: HashMap<OwnerID, HashMap<PacketID, Payload>>,
    set: HashMap<OwnerID, HashSet<PacketID>>,
}

impl DeliveredSet {
    pub fn new() -> Self {
        Self {
            acked: HashMap::new(),
            acked_counter: HashMap::new(),
            received_up_to: HashMap::new(),
            undelivered: HashMap::new(),
            set: HashMap::new(),
        }
    }
}

#[derive(Debug)]
pub struct AccessDeliveredSet {
    pub delivered: Arc<Mutex<DeliveredSet>>,
    pub tx_writing: Sender<LogEvent>,
    total_nodes: u32,
    current_node_id: u32,
}

impl AccessDeliveredSet {
    pub fn new(
        delivered: DeliveredSet,
        tx_writing: Sender<LogEvent>,
        total_nodes: u32,
        current_node_id: u32,
    ) -> Self {
        Self {
            delivered: Arc::new(Mutex::new(delivered)),
            tx_writing,
            total_nodes,
            current_node_id,
        }
    }

    pub fn insert(&self, sender_id: SenderID, payload: &Payload) {
        let mut delivered = self.delivered.lock().unwrap();

        let acked = delivered
            .acked
            .entry(sender_id)
            .or_insert(HashMap::new())
            .entry(payload.owner_id)
            .or_insert(HashSet::new());

        let already_acked = acked.contains(&payload.packet_uid);

        acked.insert(payload.packet_uid);

        if !already_acked {
            delivered
                .acked_counter
                .entry(payload.owner_id)
                .or_insert(HashMap::new())
                .entry(payload.packet_uid)
                .and_modify(|acked_counter| *acked_counter += 1)
                .or_insert(1);
        }

        let already_delivered = delivered
            .set
            .entry(payload.owner_id)
            .or_insert(HashSet::new())
            .contains(&payload.packet_uid);

        if !already_delivered {
            delivered
                .undelivered
                .entry(payload.owner_id)
                .or_insert(HashMap::new())
                .insert(payload.packet_uid, payload.clone());

            self.try_delivering(
                delivered,
                payload.kind,
                payload.owner_id,
                payload.packet_uid,
            );
        }
    }

    fn try_delivering(
        &self,
        mut delivered: MutexGuard<DeliveredSet>,
        kind: PayloadKind,
        owner_id: OwnerID,
        packet_uid: PacketID,
    ) {
        let can_deliver = match kind {
            PayloadKind::Tcp => true,
            PayloadKind::Beb => true,
            PayloadKind::Rb => true,
            PayloadKind::Urb => self.can_urb_deliver(&delivered, owner_id, packet_uid),
            PayloadKind::Fifob => {
                self.can_fifob_deliver(&delivered, owner_id, packet_uid)
            }
        };
        if !can_deliver {
            return;
        }

        let payload = delivered
            .undelivered
            .entry(owner_id)
            .or_insert(HashMap::new())
            .remove(&packet_uid)
            .unwrap();

        if matches!(payload.kind, PayloadKind::Fifob) {
            self.try_delivering_fifob(delivered, &payload);
        } else {
            self.deliver(&mut delivered, &payload);
        }
    }

    fn try_delivering_fifob(
        &self,
        mut delivered: MutexGuard<DeliveredSet>,
        payload: &Payload,
    ) {
        let received_up_to = match delivered.received_up_to.get(&payload.owner_id) {
            Some(received_up_to) => *received_up_to,
            None => 1,
        };

        if payload.packet_uid.0 == received_up_to {
            self.deliver(&mut delivered, payload);

            delivered
                .received_up_to
                .insert(payload.owner_id, received_up_to + 1);

            self.try_delivering(
                delivered,
                payload.kind,
                payload.owner_id,
                PacketID(payload.packet_uid.0 + 1),
            );
        } else {
            // putting it back

            delivered
                .undelivered
                .entry(payload.owner_id)
                .or_insert(HashMap::new())
                .insert(payload.packet_uid, payload.clone());
        }
    }

    fn deliver(&self, delivered: &mut MutexGuard<DeliveredSet>, payload: &Payload) {
        let contents = String::from_utf8(payload.buffer.clone()).unwrap();

        delivered
            .set
            .entry(payload.owner_id)
            .or_insert(HashSet::new())
            .insert(payload.packet_uid);

        self.tx_writing
            .send(LogEvent::Delivery {
                owner_id: payload.owner_id,
                kind: payload.kind,
                contents,
            })
            .unwrap();
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

    pub fn mark_as_seen(&self, payload: &Payload) {
        self.insert(SenderID(self.current_node_id), payload)
    }

    pub fn was_seen(&self, payload: &Payload) -> bool {
        self.contains(
            SenderID(self.current_node_id),
            payload.owner_id,
            payload.packet_uid,
        )
    }

    fn can_urb_deliver(
        &self,
        delivered: &MutexGuard<DeliveredSet>,
        owner_id: OwnerID,
        packet_uid: PacketID,
    ) -> bool {
        let acked_count = delivered
            .acked_counter
            .get(&owner_id)
            .and_then(|acked_counter| acked_counter.get(&packet_uid));
        match acked_count {
            Some(acked_count) => *acked_count >= self.majority(),
            None => false,
        }
    }

    fn majority(&self) -> u32 {
        (self.total_nodes / 2) + 1
    }

    fn can_fifob_deliver(
        &self,
        delivered: &MutexGuard<DeliveredSet>,
        owner_id: OwnerID,
        packet_uid: PacketID,
    ) -> bool {
        self.can_urb_deliver(delivered, owner_id, packet_uid)
    }
}

impl Clone for AccessDeliveredSet {
    fn clone(&self) -> Self {
        Self {
            delivered: Arc::clone(&self.delivered),
            tx_writing: self.tx_writing.clone(),
            total_nodes: self.total_nodes,
            current_node_id: self.current_node_id,
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
        owner_id: OwnerID,
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
                recipient: _,
                kind,
                contents,
            } => {
                if TASK_COMPATIBILITY {
                    writeln!(file, "b {}", contents)?;
                } else {
                    writeln!(file, "sent {:?}: {}", kind, contents)?;
                }
            }
            LogEvent::Delivery {
                owner_id,
                kind,
                contents,
            } => {
                if TASK_COMPATIBILITY {
                    writeln!(file, "d {} {}", owner_id.0, contents)?;
                } else {
                    writeln!(
                        file,
                        "delivered {:?} from {}: {}",
                        kind, owner_id, contents
                    )?;
                }
            }
        }
    }
    Ok(())
}

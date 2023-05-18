use crate::conf::TASK_COMPATIBILITY;
use crate::hosts::{Node, NodeID};
use crate::udp::{OwnerID, PacketID, Payload, PayloadKind, SenderID, VectorClock};
use std::collections::{HashMap, HashSet};
use std::fs::OpenOptions;
use std::io::prelude::*;
use std::path::Path;
use std::sync::mpsc::{Receiver, Sender};
use std::sync::{Arc, Mutex, MutexGuard};

type Deliverable = (PayloadKind, OwnerID, PacketID);

#[derive(Debug)]
pub struct DeliveredSet {
    acked: HashMap<SenderID, HashMap<OwnerID, HashSet<PacketID>>>,
    acked_counter: HashMap<OwnerID, HashMap<PacketID, u32>>,
    received_up_to: HashMap<OwnerID, PacketID>,
    undelivered: HashMap<OwnerID, HashMap<PacketID, Payload>>,
    set: HashMap<OwnerID, HashSet<PacketID>>,
    vector_clock: VectorClock,
}

impl DeliveredSet {
    pub fn new(total_nodes: usize) -> Self {
        Self {
            acked: HashMap::new(),
            acked_counter: HashMap::new(),
            received_up_to: HashMap::new(),
            undelivered: HashMap::new(),
            set: HashMap::new(),
            vector_clock: vec![0; total_nodes + 1],
        }
    }
}

#[derive(Debug)]
pub struct AccessDeliveredSet {
    pub delivered: Arc<Mutex<DeliveredSet>>,
    pub tx_writing: Sender<LogEvent>,
    causality_map: Arc<HashMap<NodeID, Vec<NodeID>>>,
    inverted_causality_map: Arc<HashMap<NodeID, Vec<NodeID>>>,
    total_nodes: usize,
    current_node_id: NodeID,
}

impl AccessDeliveredSet {
    pub fn new(
        delivered: DeliveredSet,
        tx_writing: Sender<LogEvent>,
        total_nodes: usize,
        current_node_id: u32,
        causality_map: HashMap<NodeID, Vec<NodeID>>,
        inverted_causality_map: HashMap<NodeID, Vec<NodeID>>,
    ) -> Self {
        Self {
            delivered: Arc::new(Mutex::new(delivered)),
            causality_map: Arc::new(causality_map),
            inverted_causality_map: Arc::new(inverted_causality_map),
            tx_writing,
            total_nodes,
            current_node_id,
        }
    }

    pub fn clone_vector_clock(&self) -> VectorClock {
        let delivered = self.delivered.lock().unwrap();
        delivered.vector_clock.clone()
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
                &mut delivered,
                vec![(payload.kind, payload.owner_id, payload.packet_uid)],
            );
        }
    }

    fn try_delivering(
        &self,
        delivered: &mut MutexGuard<DeliveredSet>,
        mut deliverable: Vec<Deliverable>,
    ) {
        while !deliverable.is_empty() {
            let (kind, owner_id, packet_uid) = deliverable.pop().unwrap();
            let payload = delivered
                .undelivered
                .get(&owner_id)
                .and_then(|undelivered| undelivered.get(&packet_uid));

            if matches!(payload, None) {
                return;
            }
            let payload = payload.unwrap().clone();

            if !self.can_deliver(delivered, &payload) {
                return;
            }

            delivered
                .undelivered
                .entry(owner_id)
                .or_insert(HashMap::new())
                .remove(&packet_uid);

            match payload.kind {
                PayloadKind::Fifob => {
                    deliverable.append(&mut self.fifob_deliver(delivered, &payload))
                }
                PayloadKind::Lcb => {
                    deliverable.append(&mut self.lcb_deliver(delivered, &payload))
                }
                _ => self.deliver(delivered, &payload),
            }
        }
    }

    fn fifob_deliver(
        &self,
        delivered: &mut MutexGuard<DeliveredSet>,
        payload: &Payload,
    ) -> Vec<Deliverable> {
        self.deliver(delivered, payload);

        let next_packet_uid = PacketID(payload.packet_uid.0 + 1);

        delivered
            .received_up_to
            .insert(payload.owner_id, next_packet_uid);

        vec![(payload.kind, payload.owner_id, next_packet_uid)]
    }

    fn lcb_deliver(
        &self,
        delivered: &mut MutexGuard<DeliveredSet>,
        payload: &Payload,
    ) -> Vec<Deliverable> {
        let mut deliverable = self.fifob_deliver(delivered, payload);

        delivered.vector_clock[payload.owner_id.0 as usize] += 1;

        if let Some(affected_nodes) =
            self.inverted_causality_map.get(&payload.owner_id.0)
        {
            for affected_node in affected_nodes.iter() {
                let affected_owner_id = OwnerID(*affected_node);
                let received_up_to = delivered
                    .received_up_to
                    .get(&affected_owner_id)
                    .copied()
                    .unwrap_or(PacketID(1));

                deliverable.push((payload.kind, affected_owner_id, received_up_to));
            }
        }
        deliverable
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

    fn can_deliver(
        &self,
        delivered: &MutexGuard<DeliveredSet>,
        payload: &Payload,
    ) -> bool {
        match payload.kind {
            PayloadKind::Tcp => true,
            PayloadKind::Beb => true,
            PayloadKind::Rb => true,
            PayloadKind::Urb => self.can_urb_deliver(delivered, payload),
            PayloadKind::Fifob => self.can_fifob_deliver(delivered, payload),
            PayloadKind::Lcb => self.can_lcb_deliver(delivered, payload),
        }
    }

    fn can_urb_deliver(
        &self,
        delivered: &MutexGuard<DeliveredSet>,
        payload: &Payload,
    ) -> bool {
        let acked_count = delivered
            .acked_counter
            .get(&payload.owner_id)
            .and_then(|acked_counter| acked_counter.get(&payload.packet_uid));
        match acked_count {
            Some(acked_count) => *acked_count >= self.majority(),
            None => false,
        }
    }

    fn can_fifob_deliver(
        &self,
        delivered: &MutexGuard<DeliveredSet>,
        payload: &Payload,
    ) -> bool {
        let urb_happy = self.can_urb_deliver(delivered, payload);

        let received_up_to = delivered
            .received_up_to
            .get(&payload.owner_id)
            .copied()
            .unwrap_or(PacketID(1));
        let order_happy = payload.packet_uid == received_up_to;
        urb_happy && order_happy
    }

    fn can_lcb_deliver(
        &self,
        delivered: &MutexGuard<DeliveredSet>,
        payload: &Payload,
    ) -> bool {
        let fifob_happy = self.can_fifob_deliver(delivered, payload);

        let dependencies = self.causality_map.get(&payload.owner_id.0);
        let lcb_happy = match dependencies {
            Some(dependencies) => dependencies.iter().all(|dependency| {
                delivered.vector_clock[*dependency as usize]
                    >= payload.vector_clock[*dependency as usize]
            }),
            None => true,
        };
        lcb_happy && fifob_happy
    }

    fn majority(&self) -> u32 {
        (self.total_nodes as u32 / 2) + 1
    }
}

impl Clone for AccessDeliveredSet {
    fn clone(&self) -> Self {
        Self {
            delivered: Arc::clone(&self.delivered),
            causality_map: Arc::clone(&self.causality_map),
            inverted_causality_map: Arc::clone(&self.inverted_causality_map),
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

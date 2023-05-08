use std::collections::{HashMap, HashSet};
use std::sync::{Arc, Mutex};

type _OwnerID = u32;
type SenderID = u32;
type PacketID = u32;

pub struct DeliveredSet {
    acked: HashMap<SenderID, HashSet<PacketID>>,
}

pub struct AccessDeliveredSet {
    pub delivered: Arc<Mutex<DeliveredSet>>,
}

impl AccessDeliveredSet {
    pub fn new(delivered: DeliveredSet) -> Self {
        Self {
            delivered: Arc::new(Mutex::new(delivered)),
        }
    }

    pub fn insert(&self, sender_id: SenderID, packet_id: PacketID) {
        let mut delivered = self.delivered.lock().unwrap();
        let acked = delivered.acked.entry(sender_id).or_insert(HashSet::new());
        acked.insert(packet_id);
    }

    pub fn contains(&self, sender_id: SenderID, packet_id: PacketID) -> bool {
        let delivered = self.delivered.lock().unwrap();
        let acked = delivered.acked.get(&sender_id);
        match acked {
            Some(acked) => acked.contains(&packet_id),
            None => false,
        }
    }
}

impl Clone for AccessDeliveredSet {
    fn clone(&self) -> Self {
        Self {
            delivered: Arc::clone(&self.delivered),
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

use crate::hosts::Nodes;
use crate::tcp::Message;
use crate::udp::Payload;
use std::sync::mpsc;

pub fn _best_effort_broadcast(
    tx_sending_channel: mpsc::Sender<Message>,
    current_node_id: u32,
    nodes: Nodes,
    payload: Payload,
) {
    for node in nodes.values() {
        if node.id == current_node_id {
            continue;
        }
        let message = Message::new(payload.clone(), node.clone());
        tx_sending_channel.send(message).unwrap();
    }
}

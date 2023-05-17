use crate::conf::DEBUG;
use crate::hosts::Nodes;
use crate::tcp::Message;
use crate::udp::Payload;
use std::sync::mpsc;

pub fn best_effort_broadcast(
    tx_sending_channel: mpsc::Sender<Message>,
    current_node_id: u32,
    nodes: Nodes,
    payload: Payload,
) {
    if DEBUG {
        println!("Broadcasting: {}", payload);
    }
    for node in nodes.values() {
        if node.id == current_node_id {
            continue;
        }
        let message = Message::new(payload.clone(), node.clone());
        tx_sending_channel.send(message).unwrap();
    }
}

pub fn reliable_broadcast(
    tx_sending_channel: mpsc::Sender<Message>,
    current_node_id: u32,
    nodes: Nodes,
    payload: Payload,
) {
    if DEBUG {
        println!("RB: {}", payload);
    }
    best_effort_broadcast(tx_sending_channel, current_node_id, nodes, payload);
}

pub fn uniform_reliable_broadcast(
    tx_sending_channel: mpsc::Sender<Message>,
    current_node_id: u32,
    nodes: Nodes,
    payload: Payload,
) {
    if DEBUG {
        println!("URB: {}", payload);
    }
}

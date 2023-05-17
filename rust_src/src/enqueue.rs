use crate::broadcast::best_effort_broadcast;
use crate::config_parser::Config;
use crate::delivered::LogEvent;
use crate::hosts::Nodes;
use crate::tcp::Message;
use crate::udp::{Payload, PayloadKind};
use std::sync::mpsc;

pub fn enqueue_tcp_messages(
    tx_sending_channel: mpsc::Sender<Message>,
    tx_writing_channel: mpsc::Sender<LogEvent>,
    current_node_id: u32,
    config: Config,
    nodes: Nodes,
) -> Result<(), Box<dyn std::error::Error>> {
    let destination = nodes.get(&config.receiver_id).unwrap();

    if current_node_id == config.receiver_id {
        // nothing to do
        return Ok(());
    }

    for i in 1..config.messages_count + 1 {
        let contents = i.to_string();
        let kind = PayloadKind::Tcp;
        let payload = Payload {
            owner_id: current_node_id,
            sender_id: current_node_id,
            packet_uid: i,
            kind,
            vector_clock: vec![0],
            buffer: contents.as_bytes().to_vec(),
        };
        let message = Message::new(payload, destination.clone());

        tx_sending_channel.send(message)?;
        tx_writing_channel.send(LogEvent::Dispatch {
            recipient: Some(destination.clone()),
            kind,
            contents,
        })?;
    }
    Ok(())
}

pub fn enqueue_beb_messages(
    tx_sending_channel: mpsc::Sender<Message>,
    tx_writing_channel: mpsc::Sender<LogEvent>,
    current_node_id: u32,
    config: Config,
    nodes: Nodes,
) -> Result<(), Box<dyn std::error::Error>> {
    for i in 1..config.messages_count + 1 {
        let contents = i.to_string();
        let kind = PayloadKind::Beb;
        let payload = Payload {
            owner_id: current_node_id,
            sender_id: current_node_id,
            packet_uid: i,
            kind,
            vector_clock: vec![0],
            buffer: contents.as_bytes().to_vec(),
        };

        best_effort_broadcast(
            tx_sending_channel.clone(),
            current_node_id,
            nodes.clone(),
            payload,
        );
        tx_writing_channel.send(LogEvent::Dispatch {
            recipient: None,
            kind,
            contents,
        })?;
    }
    Ok(())
}

use crate::broadcast;
use crate::config_parser::Config;
use crate::delivered::LogEvent;
use crate::tcp::{Message, TcpHandler};
use crate::udp::{OwnerID, PacketID, Payload, PayloadKind, SenderID};
use std::sync::mpsc;

pub fn enqueue_tcp_messages(
    tcp_handler: &TcpHandler,
    tx_writing_channel: &mpsc::Sender<LogEvent>,
    config: &Config,
) -> Result<(), Box<dyn std::error::Error>> {
    let destination = tcp_handler.nodes.get(&config.receiver_id).unwrap();

    if tcp_handler.current_node_id == config.receiver_id {
        // nothing to do
        return Ok(());
    }

    for i in 1..config.messages_count + 1 {
        let contents = i.to_string();
        let kind = PayloadKind::Tcp;
        let payload = Payload {
            owner_id: OwnerID(tcp_handler.current_node_id),
            sender_id: SenderID(tcp_handler.current_node_id),
            packet_uid: PacketID(i),
            kind,
            is_ack: false,
            vector_clock: vec![0],
            buffer: contents.as_bytes().to_vec(),
        };
        let message = Message::new(payload, destination.clone());

        tcp_handler.tx_sending_channel.send(message)?;
        tx_writing_channel.send(LogEvent::Dispatch {
            recipient: Some(destination.clone()),
            kind,
            contents,
        })?;
    }
    Ok(())
}

pub fn enqueue_broadcast_messages(
    tcp_handler: &TcpHandler,
    tx_writing_channel: &mpsc::Sender<LogEvent>,
    messages_count: u32,
    kind: PayloadKind,
) -> Result<(), Box<dyn std::error::Error>> {
    for i in 1..messages_count + 1 {
        let contents = i.to_string();
        let payload = Payload {
            owner_id: OwnerID(tcp_handler.current_node_id),
            sender_id: SenderID(tcp_handler.current_node_id),
            packet_uid: PacketID(i),
            kind,
            is_ack: false,
            vector_clock: tcp_handler.delivered.clone_vector_clock(),
            buffer: contents.as_bytes().to_vec(),
        };

        tx_writing_channel.send(LogEvent::Dispatch {
            recipient: None,
            kind,
            contents,
        })?;
        match kind {
            PayloadKind::Beb => broadcast::best_effort_broadcast(tcp_handler, &payload),
            PayloadKind::Rb => broadcast::reliable_broadcast(tcp_handler, &payload),
            PayloadKind::Urb => {
                broadcast::uniform_reliable_broadcast(tcp_handler, &payload)
            }
            PayloadKind::Fifob => broadcast::fifo_broadcast(tcp_handler, &payload),
            PayloadKind::Lcb => {
                broadcast::localized_causal_broadcast(tcp_handler, &payload)
            }
            _ => panic!("Invalid payload kind to broadcast"),
        }
    }
    Ok(())
}

pub fn enqueue_messages(
    tcp_handler: TcpHandler,
    tx_writing_channel: mpsc::Sender<LogEvent>,
    messages_count: u32,
) -> Result<(), Box<dyn std::error::Error>> {
    // let vector clock fill up a bit
    std::thread::sleep(std::time::Duration::from_secs(2));

    enqueue_broadcast_messages(
        &tcp_handler,
        &tx_writing_channel,
        messages_count,
        PayloadKind::Fifob,
    )?;
    Ok(())
}

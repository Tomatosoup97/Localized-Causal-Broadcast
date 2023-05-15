use crate::conf::{DEBUG, RETRANSMISSION_OFFSET_MS};
use crate::config_parser::Config;
use crate::delivered::{AccessDeliveredSet, LogEvent};
use crate::hosts::{Node, Nodes};
use crate::udp::{Payload, PayloadKind};
use std::net::UdpSocket;
use std::sync::mpsc;
use std::time::{Duration, Instant};
use std::{fmt, fmt::Display, fmt::Formatter};

#[derive(Debug, Clone)]
pub struct Message {
    pub payload: Payload,
    pub destination: Node,
    pub sending_time: Instant,
}

impl Display for Message {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "Message {{ payload: {}, destination: {} }}",
            self.payload, self.destination,
        )
    }
}

impl Message {
    pub fn new(payload: Payload, destination: Node) -> Self {
        Message {
            payload,
            destination,
            sending_time: Instant::now(),
        }
    }

    fn ready_for_retransmission(&self) -> bool {
        let elapsed_time = self.sending_time.elapsed();
        elapsed_time >= Duration::from_millis(RETRANSMISSION_OFFSET_MS)
    }

    fn should_retransmit(&self) -> bool {
        !self.payload.kind.is_ack()
    }
}

pub fn keep_sending_messages(
    rx_sending_channel: mpsc::Receiver<Message>,
    tx_retrans_channel: mpsc::Sender<Message>,
    current_node_id: u32,
    socket: &UdpSocket,
) -> Result<(), Box<dyn std::error::Error>> {
    for mut message in rx_sending_channel {
        if DEBUG {
            println!("Sending to {}", message.destination);
        }
        message.payload.sender_id = current_node_id;
        message.payload.send_udp(socket, &message.destination)?;
        message.sending_time = Instant::now();

        if message.should_retransmit() {
            tx_retrans_channel.send(message)?;
        }
    }
    Ok(())
}

pub fn keep_receiving_messages(
    socket: &UdpSocket,
    tx_sending_channel: mpsc::Sender<Message>,
    nodes: Nodes,
    delivered: AccessDeliveredSet,
) -> Result<(), Box<dyn std::error::Error>> {
    loop {
        let payload = Payload::receive_udp(socket)?;

        if !payload.kind.is_ack() {
            let mut acked_payload = payload.clone();
            acked_payload.kind = PayloadKind::Ack;

            let destination = nodes.get(&payload.sender_id).unwrap().clone();
            let message = Message::new(acked_payload, destination);
            tx_sending_channel.send(message)?;
        }
        delivered.insert(payload);
    }
}

pub fn keep_retransmitting_messages(
    rx_retrans_channel: mpsc::Receiver<Message>,
    tx_sending_channel: mpsc::Sender<Message>,
    delivered: AccessDeliveredSet,
) -> Result<(), Box<dyn std::error::Error>> {
    for message in rx_retrans_channel {
        if delivered.contains(message.destination.id, message.payload.packet_uid) {
            continue;
        }

        while !message.ready_for_retransmission() {
            std::thread::sleep(Duration::from_millis(RETRANSMISSION_OFFSET_MS / 10));
        }

        if !delivered.contains(message.destination.id, message.payload.packet_uid) {
            if DEBUG {
                println!("Retransmitting {}", message);
            }
            tx_sending_channel.send(message)?;
        }
    }
    Ok(())
}

pub fn enqueue_messages(
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
        if DEBUG {
            println!("Enqueuing {}", message);
        }
        tx_sending_channel.send(message)?;
        tx_writing_channel.send(LogEvent::Dispatch {
            recipient: Some(destination.clone()),
            kind,
            contents,
        })?;
    }
    Ok(())
}

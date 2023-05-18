use crate::broadcast;
use crate::conf::{DEBUG_VERBOSE, RETRANSMISSION_OFFSET_MS};
use crate::delivered::AccessDeliveredSet;
use crate::hosts::{Node, Nodes};
use crate::udp::{Payload, PayloadKind, SenderID};
use std::net::UdpSocket;
use std::sync::mpsc;
use std::time::{Duration, Instant};
use std::{fmt, fmt::Display, fmt::Formatter};

#[derive(Debug, Clone)]
pub struct TcpHandler {
    pub nodes: Nodes,
    pub current_node_id: u32,
    pub tx_sending_channel: mpsc::Sender<Message>,
    pub delivered: AccessDeliveredSet,
}

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
        !self.payload.is_ack
    }
}

pub fn keep_sending_messages(
    tcp_handler: TcpHandler,
    rx_sending_channel: mpsc::Receiver<Message>,
    tx_retrans_channel: mpsc::Sender<Message>,
    socket: &UdpSocket,
) -> Result<(), Box<dyn std::error::Error>> {
    for mut message in rx_sending_channel {
        if DEBUG_VERBOSE {
            println!("Sending to {}", message.destination);
        }
        message.payload.sender_id = SenderID(tcp_handler.current_node_id);
        message.payload.send_udp(socket, &message.destination)?;
        message.sending_time = Instant::now();

        if message.should_retransmit() {
            tx_retrans_channel.send(message)?;
        }
    }
    Ok(())
}

pub fn keep_receiving_messages(
    tcp_handler: TcpHandler,
    socket: &UdpSocket,
) -> Result<(), Box<dyn std::error::Error>> {
    loop {
        let payload = Payload::receive_udp(socket)?;

        if !payload.is_ack {
            let mut acked_payload = payload.clone();
            acked_payload.is_ack = true;

            let destination =
                tcp_handler.nodes.get(&payload.sender_id.0).unwrap().clone();
            let message = Message::new(acked_payload, destination);
            tcp_handler.tx_sending_channel.send(message)?;
        }
        tcp_handler.delivered.insert(payload.sender_id, &payload);

        match payload.kind {
            PayloadKind::Tcp => {}
            PayloadKind::Beb => {}
            PayloadKind::Rb => {
                broadcast::reliable_broadcast(&tcp_handler, &payload);
            }
            PayloadKind::Urb => {
                broadcast::uniform_reliable_broadcast(&tcp_handler, &payload);
            }
            PayloadKind::Fifob => {
                broadcast::fifo_broadcast(&tcp_handler, &payload);
            }
            PayloadKind::Lcb => {
                broadcast::localized_causal_broadcast(&tcp_handler, &payload);
            }
        }
    }
}

pub fn keep_retransmitting_messages(
    tcp_handler: TcpHandler,
    rx_retrans_channel: mpsc::Receiver<Message>,
) -> Result<(), Box<dyn std::error::Error>> {
    for message in rx_retrans_channel {
        while !message.ready_for_retransmission() {
            std::thread::sleep(Duration::from_millis(RETRANSMISSION_OFFSET_MS / 10));
        }

        if !tcp_handler.delivered.contains(
            SenderID(message.destination.id),
            message.payload.owner_id,
            message.payload.packet_uid,
        ) {
            if DEBUG_VERBOSE {
                println!("Retransmitting {}", message);
            }
            tcp_handler.tx_sending_channel.send(message)?;
        }
    }
    Ok(())
}

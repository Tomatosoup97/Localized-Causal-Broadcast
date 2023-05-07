use crate::conf::{DEBUG, RETRANSMISSION_OFFSET_MS};
use crate::config_parser::Config;
use crate::hosts::Node;
use crate::udp::Payload;
use std::collections::HashMap;
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
    fn _should_retransmit(&self) -> bool {
        let elapsed_time = self.sending_time.elapsed();
        elapsed_time >= Duration::from_millis(RETRANSMISSION_OFFSET_MS)
    }
}

pub fn keep_sending_messages(
    rx_sending_channel: mpsc::Receiver<Message>,
    socket: &UdpSocket,
) -> Result<(), Box<dyn std::error::Error>> {
    for message in rx_sending_channel {
        if DEBUG {
            println!("Sending {}", message);
        }
        message.payload.send_udp(socket, &message.destination)?;
    }
    Ok(())
}

pub fn keep_receiving_messages(
    socket: &UdpSocket,
) -> Result<(), Box<dyn std::error::Error>> {
    loop {
        let _payload = Payload::receive_udp(socket)?;
    }
}

pub fn enqueue_messages(
    tx_sending_channel: mpsc::Sender<Message>,
    sender_id: u32,
    config: Config,
    nodes: HashMap<u32, Node>,
) -> Result<(), Box<dyn std::error::Error>> {
    let destination = nodes.get(&config.receiver_id).unwrap();

    for i in 1..config.messages_count + 1 {
        let payload = Payload {
            owner_id: sender_id,
            sender_id,
            packet_uid: i,
            is_ack: false,
            vector_clock: vec![0],
            buffer: i.to_string().as_bytes().to_vec(),
        };
        let message = Message {
            payload,
            destination: destination.clone(),
            sending_time: Instant::now(),
        };
        if DEBUG {
            println!("Enqueuing {}", message);
        }
        tx_sending_channel.send(message)?;
    }
    Ok(())
}

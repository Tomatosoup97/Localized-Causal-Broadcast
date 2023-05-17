use crate::conf::DEBUG_VERBOSE;
use crate::hosts::Node;
use bincode::serialize;
use serde::{Deserialize, Serialize};
use std::fmt::{self, Display, Formatter};
use std::net::UdpSocket;

const MAX_UDP_PAYLOAD_SIZE: usize = 65535;

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub enum PayloadKind {
    Ack,
    Tcp,
    Beb,
    Rb,
    Urb,
}

impl PayloadKind {
    pub fn is_ack(&self) -> bool {
        matches!(self, PayloadKind::Ack)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Payload {
    pub owner_id: u32,
    pub sender_id: u32,
    pub packet_uid: u32,
    pub kind: PayloadKind,
    pub vector_clock: Vec<u32>,
    pub buffer: Vec<u8>,
}

impl Display for Payload {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        let message = String::from_utf8(self.buffer.clone()).unwrap();
        write!(
            f,
            "Payload {{ kind: {:?}, owner_id: {}, sender_id: {}, packet_uid: {}, vector_clock: {:?}, buffer: {:?} }}",
            self.kind,
            self.owner_id,
            self.sender_id,
            self.packet_uid,
            self.vector_clock,
            message,
        )
    }
}

impl Payload {
    pub fn send_udp(
        &self,
        socket: &UdpSocket,
        node: &Node,
    ) -> Result<(), Box<dyn std::error::Error>> {
        if DEBUG_VERBOSE {
            println!("Sending {}", self);
        }
        let destination = format!("{}:{}", node.ip, node.port);
        let bytes = serialize(self)?;

        socket.send_to(&bytes, destination)?;
        Ok(())
    }

    pub fn receive_udp(
        socket: &UdpSocket,
    ) -> Result<Payload, Box<dyn std::error::Error>> {
        let mut buf = [0; MAX_UDP_PAYLOAD_SIZE];
        let (size, _) = socket.recv_from(&mut buf)?;
        let payload: Payload = bincode::deserialize(&buf[..size])?;

        if DEBUG_VERBOSE {
            println!("Received {}", payload);
        }

        Ok(payload)
    }
}

pub fn bind_socket(
    ip: &str,
    port: u32,
) -> Result<UdpSocket, Box<dyn std::error::Error>> {
    let socket = UdpSocket::bind(format!("{}:{}", ip, port))?;
    Ok(socket)
}

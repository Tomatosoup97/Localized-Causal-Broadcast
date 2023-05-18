use crate::conf::DEBUG_VERBOSE;
use crate::hosts::Node;
use bincode::serialize;
use serde::{Deserialize, Serialize};
use std::fmt::{self, Display, Formatter};
use std::net::UdpSocket;

const MAX_UDP_PAYLOAD_SIZE: usize = 65535;

#[derive(Debug, Clone, Serialize, Deserialize, Eq, Hash, PartialEq, Copy)]
pub struct OwnerID(pub u32);

impl Display for OwnerID {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        write!(f, "OwnerID({})", self.0)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, Eq, Hash, PartialEq, Copy)]
pub struct SenderID(pub u32);

impl Display for SenderID {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        write!(f, "SenderID({})", self.0)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, Eq, Hash, PartialEq, Copy)]
pub struct PacketID(pub u32);

impl Display for PacketID {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        write!(f, "PacketID({})", self.0)
    }
}

pub type VectorClock = Vec<u32>;

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub enum PayloadKind {
    Tcp,
    Beb,
    Rb,
    Urb,
    Fifob,
    Lcb,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Payload {
    pub owner_id: OwnerID,
    pub sender_id: SenderID,
    pub packet_uid: PacketID,
    pub kind: PayloadKind,
    pub is_ack: bool,
    pub vector_clock: VectorClock,
    pub buffer: Vec<u8>,
}

impl Display for Payload {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        let message = String::from_utf8(self.buffer.clone()).unwrap();
        write!(
            f,
            "Payload {{ is_ack: {:?}, kind: {:?}, owner_id: {}, sender_id: {}, packet_uid: {}, vector_clock: {:?}, buffer: {:?} }}",
            self.is_ack,
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

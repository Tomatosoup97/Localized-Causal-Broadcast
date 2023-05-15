#![allow(dead_code, unused_variables)]

use conf::DEBUG;
use std::sync::mpsc;
use std::thread;

mod broadcast;
mod conf;
mod config_parser;
mod delivered;
mod hosts;
mod tcp;
mod udp;

fn run() -> Result<(), Box<dyn std::error::Error>> {
    let program_args = config_parser::ProgramArgs::parse()?;
    let config = config_parser::Config::read(&program_args.config)?;
    let nodes = hosts::read_hosts(&program_args.hosts)?;
    config_parser::create_output_file(&program_args.output)?;

    let current_node = nodes.get(&program_args.id).unwrap();
    let current_node_id = current_node.id;

    if DEBUG {
        println!("------------------");
        println!("Program args: {:?}", program_args);
        println!("Config: {:?}", config);
        println!("Nodes: {:?}", nodes);
        println!("------------------");
    }

    let socket = udp::bind_socket(&current_node.ip, current_node.port)?;

    let (tx_sending, rx_sending) = mpsc::channel::<tcp::Message>();
    let (tx_retrans, rx_retrans) = mpsc::channel::<tcp::Message>();
    let (tx_writing, rx_writing) = mpsc::channel::<delivered::LogEvent>();

    let delivered_tx_writing = tx_writing.clone();
    let delivered = delivered::AccessDeliveredSet::new(
        delivered::DeliveredSet::new(),
        delivered_tx_writing,
    );

    let sender_socket = socket.try_clone().expect("couldn't clone the socket");

    let sender_thread = thread::spawn(move || {
        if let Err(e) = tcp::keep_sending_messages(
            rx_sending,
            tx_retrans,
            current_node_id,
            &sender_socket,
        ) {
            panic!("Error: {}", e)
        }
    });

    let receiver_nodes = nodes.clone();
    let receiver_socket = socket.try_clone().expect("couldn't clone the socket");
    let receiver_delivered = delivered.clone();

    let tx_sending_receiver = tx_sending.clone();
    let receiver_thread = thread::spawn(move || {
        if let Err(e) = tcp::keep_receiving_messages(
            &receiver_socket,
            tx_sending_receiver,
            receiver_nodes,
            receiver_delivered,
        ) {
            panic!("Error: {}", e)
        }
    });

    let tx_sending_retransmitter = tx_sending.clone();
    let retransmitter_delivered = delivered;
    let retransmission_thread = thread::spawn(move || {
        if let Err(e) = tcp::keep_retransmitting_messages(
            rx_retrans,
            tx_sending_retransmitter,
            retransmitter_delivered,
        ) {
            panic!("Error: {}", e)
        }
    });

    let enqueuer_config = config;
    let enqueuer_nodes = nodes.clone();

    let enqueuer_thread = thread::spawn(move || {
        if let Err(e) = tcp::enqueue_messages(
            tx_sending,
            tx_writing,
            current_node_id,
            enqueuer_config,
            enqueuer_nodes,
        ) {
            panic!("Error: {}", e)
        }
    });

    let writer_thread = thread::spawn(move || {
        if let Err(e) =
            delivered::keep_writing_delivered_messages(&program_args.output, rx_writing)
        {
            panic!("Error: {}", e)
        }
    });

    sender_thread.join().unwrap();
    receiver_thread.join().unwrap();
    enqueuer_thread.join().unwrap();
    retransmission_thread.join().unwrap();
    writer_thread.join().unwrap();

    Ok(())
}

fn main() {
    if let Err(e) = run() {
        println!("Error: {}", e);
    }
}

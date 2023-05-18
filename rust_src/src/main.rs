#![allow(dead_code, unused_variables)]

use conf::DEBUG;
use std::sync::mpsc;
use std::thread;

mod broadcast;
mod conf;
mod config_parser;
mod delivered;
mod enqueue;
mod hosts;
mod tcp;
mod udp;

fn run() -> Result<(), Box<dyn std::error::Error>> {
    let program_args = config_parser::ProgramArgs::parse()?;
    let config = config_parser::ConfigLcb::read(&program_args.config)?;
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

    let messages_count = config.messages_count;
    let causality_map = config.causality_map;
    let inverted_causality_map = config.inverted_causality_map;

    let socket = udp::bind_socket(&current_node.ip, current_node.port)?;

    let (tx_sending, rx_sending) = mpsc::channel::<tcp::Message>();
    let (tx_retrans, rx_retrans) = mpsc::channel::<tcp::Message>();
    let (tx_writing, rx_writing) = mpsc::channel::<delivered::LogEvent>();

    let delivered_tx_writing = tx_writing.clone();
    let delivered = delivered::AccessDeliveredSet::new(
        delivered::DeliveredSet::new(nodes.len()),
        delivered_tx_writing,
        nodes.len(),
        current_node_id,
        causality_map,
        inverted_causality_map,
    );

    let tcp_handler = tcp::TcpHandler {
        nodes: nodes.clone(),
        current_node_id,
        tx_sending_channel: tx_sending,
        delivered,
    };

    let sender_socket = socket.try_clone().expect("couldn't clone the socket");
    let sender_tcp_handler = tcp_handler.clone();

    let sender_thread = thread::spawn(move || {
        if let Err(e) = tcp::keep_sending_messages(
            sender_tcp_handler,
            rx_sending,
            tx_retrans,
            &sender_socket,
        ) {
            panic!("Error: {}", e)
        }
    });

    let receiver_socket = socket.try_clone().expect("couldn't clone the socket");

    let receiver_tcp_handler = tcp_handler.clone();
    let receiver_thread = thread::spawn(move || {
        if let Err(e) =
            tcp::keep_receiving_messages(receiver_tcp_handler, &receiver_socket)
        {
            panic!("Error: {}", e)
        }
    });

    let retransmitter_tcp_handler = tcp_handler.clone();
    let retransmission_thread = thread::spawn(move || {
        if let Err(e) =
            tcp::keep_retransmitting_messages(retransmitter_tcp_handler, rx_retrans)
        {
            panic!("Error: {}", e)
        }
    });

    let enqueuer_thread = thread::spawn(move || {
        if let Err(e) =
            enqueue::enqueue_messages(tcp_handler, tx_writing, messages_count)
        {
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

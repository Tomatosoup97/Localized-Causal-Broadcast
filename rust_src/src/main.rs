use conf::DEBUG;
use std::sync::mpsc;
use std::thread;

mod conf;
mod config_parser;
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
    let receiver_node = nodes.get(&config.receiver_id).unwrap();

    if DEBUG {
        println!();
        println!("Program args: {:?}", program_args);
        println!("Config: {:?}", config);
        println!("Nodes: {:?}", nodes);
    }

    let socket = udp::bind_socket(&current_node.ip, current_node.port)?;

    let (tx_sending, rx_sending) = mpsc::channel::<tcp::Message>();
    let (tx_retrans, _rx_retrans) = mpsc::channel::<tcp::Message>();

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

    let tx_sending_receiver = tx_sending.clone();
    let receiver_thread = thread::spawn(move || {
        if let Err(e) = tcp::keep_receiving_messages(
            &receiver_socket,
            tx_sending_receiver,
            receiver_nodes,
        ) {
            panic!("Error: {}", e)
        }
    });

    if current_node.id != receiver_node.id {
        let enqueuer_config = config.clone();
        let enqueuer_nodes = nodes.clone();

        let enqueuer_thread = thread::spawn(move || {
            if let Err(e) = tcp::enqueue_messages(
                tx_sending,
                current_node_id,
                enqueuer_config,
                enqueuer_nodes,
            ) {
                panic!("Error: {}", e)
            }
        });
        enqueuer_thread.join().unwrap();
    }

    sender_thread.join().unwrap();
    receiver_thread.join().unwrap();

    Ok(())
}

fn main() {
    if let Err(e) = run() {
        println!("Error: {}", e);
    }
}

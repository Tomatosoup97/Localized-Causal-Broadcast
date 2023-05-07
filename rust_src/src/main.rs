use conf::DEBUG;
use std::sync::mpsc;
// use std::sync::Arc;
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

    let myself_node = nodes.get(&program_args.id).unwrap();
    let myself_id = myself_node.id;
    let receiver_node = nodes.get(&config.receiver_id).unwrap();

    if DEBUG {
        println!();
        println!("Program args: {:?}", program_args);
        println!("Config: {:?}", config);
        println!("Nodes: {:?}", nodes);
    }

    let socket = udp::bind_socket(&myself_node.ip, myself_node.port)?;

    let (tx, rx) = mpsc::channel::<tcp::Message>();

    let sender_socket = socket.try_clone().expect("couldn't clone the socket");

    let sender_thread = thread::spawn(move || {
        if let Err(e) = tcp::keep_sending_messages(rx, &sender_socket) {
            panic!("Error: {}", e)
        }
    });

    let enqueuer_config = config.clone();
    let enqueuer_nodes = nodes.clone();

    let receiver_socket = socket.try_clone().expect("couldn't clone the socket");

    let receiver_thread = thread::spawn(move || {
        if let Err(e) = tcp::keep_receiving_messages(&receiver_socket) {
            panic!("Error: {}", e)
        }
    });

    if myself_node.id == receiver_node.id {
        if DEBUG {
            println!("I am the receiver");
        }
    } else {
        if DEBUG {
            println!("I am the sender");
        }

        let enqueuer_thread = thread::spawn(move || {
            if let Err(e) =
                tcp::enqueue_messages(tx, myself_id, enqueuer_config, enqueuer_nodes)
            {
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

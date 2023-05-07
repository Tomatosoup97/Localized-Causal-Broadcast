mod conf;
mod config_parser;
mod hosts;
mod udp;

use conf::DEBUG;

fn run() -> Result<(), Box<dyn std::error::Error>> {
    let program_args = config_parser::ProgramArgs::parse()?;
    let config = config_parser::Config::read(&program_args.config)?;
    let nodes = hosts::read_hosts(&program_args.hosts)?;
    config_parser::create_output_file(&program_args.output)?;

    let myself_node = nodes.get(&program_args.id).unwrap();
    let receiver_node = nodes.get(&config.receiver_id).unwrap();

    let socket = udp::bind_socket(&myself_node.ip, myself_node.port)?;

    if DEBUG {
        println!();
        println!("Program args: {:?}", program_args);
        println!("Config: {:?}", config);
        println!("Nodes: {:?}", nodes);
    }

    if myself_node.id == receiver_node.id {
        if DEBUG {
            println!("I am the receiver");
        }

        let _payload = udp::Payload::receive_udp(&socket)?;
    } else {
        if DEBUG {
            println!("I am the sender");
        }

        let payload = udp::Payload {
            owner_id: myself_node.id,
            sender_id: myself_node.id,
            packet_uid: 0,
            is_ack: false,
            vector_clock: vec![0; nodes.len()],
            buffer: "Hello!".as_bytes().to_vec(),
        };
        udp::Payload::send_udp(&socket, receiver_node, payload)?;
    }

    Ok(())
}

fn main() {
    if let Err(e) = run() {
        println!("Error: {}", e);
    }
}

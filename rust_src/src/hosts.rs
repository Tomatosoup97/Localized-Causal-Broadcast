use std::collections::HashMap;
use std::fs::File;
use std::io::{BufRead, BufReader};
use std::path::Path;

#[derive(Debug)]
pub struct Node {
    pub id: u32,
    pub ip: String,
    pub port: u32,
}

pub fn read_hosts(path: &str) -> Result<HashMap<u32, Node>, Box<dyn std::error::Error>> {
    let path = Path::new(path);
    let file = File::open(path)?;
    let reader = BufReader::new(file);

    let mut nodes = HashMap::new();

    for line in reader.lines() {
        let line = line?;
        let mut values = line.split_whitespace();

        let id = values.next().ok_or("Invalid input")?.parse::<u32>()?;
        let ip = values.next().ok_or("Invalid input")?.to_string();
        let port = values.next().ok_or("Invalid input")?.parse::<u32>()?;

        nodes.insert(id, Node { id, ip, port });
    }

    Ok(nodes)
}

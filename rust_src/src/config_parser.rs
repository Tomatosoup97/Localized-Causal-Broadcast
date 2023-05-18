use crate::hosts::NodeID;
use std::collections::HashMap;
use std::env;
use std::fs::File;
use std::io::{BufRead, BufReader};
use std::path::Path;

#[derive(Debug)]
pub struct ProgramArgs {
    pub id: u32,
    pub hosts: String,
    pub output: String,
    pub config: String,
}

#[derive(Debug, Clone)]
pub struct Config {
    pub messages_count: u32,
    pub receiver_id: u32,
}

impl Config {
    pub fn read(path: &str) -> Result<Config, Box<dyn std::error::Error>> {
        let path = Path::new(path);
        let file = File::open(path)?;
        let reader = BufReader::new(file);

        let line = reader.lines().next().ok_or("Empty file")??;
        let mut values = line.split_whitespace();

        let messages_count = values.next().ok_or("Invalid input")?.parse::<u32>()?;
        let receiver_id = values.next().ok_or("Invalid input")?.parse::<u32>()?;

        Ok(Config {
            messages_count,
            receiver_id,
        })
    }
}

#[derive(Debug)]
pub struct ConfigLcb {
    pub messages_count: u32,
    pub causality_map: HashMap<NodeID, Vec<NodeID>>,
    pub inverted_causality_map: HashMap<NodeID, Vec<NodeID>>,
}

impl ConfigLcb {
    pub fn read(path: &str) -> Result<ConfigLcb, Box<dyn std::error::Error>> {
        let file = File::open(Path::new(path))?;
        let reader = BufReader::new(file);

        let mut lines = reader.lines();

        let messages_count = lines
            .next()
            .ok_or("Invalid config file format")??
            .parse::<u32>()?;

        let mut causality_map = HashMap::new();
        let mut inverted_causality_map = HashMap::new();

        for (process_id, line) in lines.enumerate() {
            let line = line?;
            let dependencies: Vec<u32> = line
                .split_whitespace()
                .map(|id| id.parse::<u32>())
                .collect::<Result<Vec<u32>, _>>()?;

            for dependency in dependencies.iter() {
                inverted_causality_map
                    .entry(*dependency)
                    .or_insert_with(Vec::new)
                    .push(process_id as u32);
            }
            causality_map.insert(process_id as u32, dependencies);
        }

        Ok(ConfigLcb {
            messages_count,
            causality_map,
            inverted_causality_map,
        })
    }
}

impl ProgramArgs {
    pub fn parse() -> Result<ProgramArgs, String> {
        let args: Vec<String> = env::args().collect();

        if args.len() != 9 {
            return Err("Wrong number of arguments".to_string());
        }
        let mut program_args = ProgramArgs {
            id: 0,
            hosts: String::new(),
            output: String::new(),
            config: String::new(),
        };

        for i in 1..args.len() {
            match args[i].as_str() {
                "--id" => {
                    program_args.id = args[i + 1].parse::<u32>().unwrap();
                }
                "--hosts" => {
                    program_args.hosts = args[i + 1].clone();
                }
                "--output" => {
                    program_args.output = args[i + 1].clone();
                }
                "--config" => {
                    program_args.config = args[i + 1].clone();
                }
                _ => {}
            };
        }
        Ok(program_args)
    }
}

pub fn create_output_file(path: &str) -> Result<(), Box<dyn std::error::Error>> {
    let path = Path::new(path);
    File::create(path)?;
    Ok(())
}

mod config_parser;

const DEBUG: bool = true;


fn main() -> Result<(), Box<dyn std::error::Error>> {
    let program_args = config_parser::parse_args()?;
    let config = config_parser::read_config_file(&program_args.config)?;
    let nodes = config_parser::read_hosts(&program_args.hosts)?;
    config_parser::create_output_file(&program_args.output)?;

    if DEBUG {
        println!("Program args: {:?}", program_args);
        println!("Config: {:?}", config);
        println!("Nodes: {:?}", nodes);
    }

    Ok(())
}

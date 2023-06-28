use std::net::{SocketAddr, SocketAddrV4};

use clap::Parser;
use color_eyre::eyre::{bail, Report};
use tokio::{
    io::AsyncWriteExt,
    net::{TcpListener, TcpStream},
};

/// Simple program to greet a person
#[derive(Parser, Debug)]
#[command(author, version, about, long_about = None)]
struct Args {
    #[arg(short, long, default_value = "127.0.0.1:1024")]
    /// Address of the game
    game: SocketAddrV4,

    #[arg(short, long, default_value = "0.0.0.0:3791")]
    /// Address to listen
    listen: SocketAddrV4,
}

async fn handle_connection(
    game: SocketAddrV4,
    mut stream: TcpStream,
    addr: SocketAddr,
) -> Result<(), Report> {
    let addr = match addr {
        SocketAddr::V4(addr) => addr,
        SocketAddr::V6(addr) => bail!("Unexpected IPv6: {addr}"),
    };

    // Connect to the game
    let mut game = TcpStream::connect(game).await?;

    // Write the proxy header
    game.write_u32(u32::from(*addr.ip())).await?;

    // Start proxying
    tokio::io::copy_bidirectional(&mut stream, &mut game).await?;

    Ok(())
}

#[tokio::main(flavor = "current_thread")]
async fn main() -> Result<(), Report> {
    color_eyre::install()?;

    let args = Args::parse();
    let listener = TcpListener::bind(args.listen).await?;

    loop {
        let (stream, addr) = listener.accept().await?;
        let game = args.game.clone();

        tokio::spawn(async move {
            if let Err(err) = handle_connection(game, stream, addr).await {
                eprintln!("{err}");
            }
        });
    }
}

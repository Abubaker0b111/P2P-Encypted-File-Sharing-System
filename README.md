# SecureP2P: Encrypted Reliable UDP Transport

A decentralized, stateful Peer-to-Peer file transfer node and Bootstrap server built in C++17. This project implements a custom Reliable UDP (RUDP) transport layer, traverses NAT firewalls via UDP Hole Punching, and secures all traffic using military-grade Elliptic-Curve Cryptography.

## 🛡️ Architecture & Features

* **Custom Reliable UDP (RUDP):** Implements sequence numbering, ACKs, checksum validation, and automatic retransmission over standard UDP.
* **NAT Traversal (UDP Hole Punching):** Includes a stateful bootstrap server that tracks peer heartbeats and brokers direct connections through strict firewalls.
* **Libsodium AEAD Encryption:** Uses `XChaCha20-Poly1305` Authenticated Encryption with Associated Data. Packet headers are mathematically authenticated, and payloads are encrypted with 24-byte nonces.
* **Ephemeral Key Exchange:** Perfect Forward Secrecy is achieved by generating temporary `crypto_kx` keypairs for every session and deriving shared secrets via Diffie-Hellman.
* **Asynchronous Event Loop:** The client operates as a background node using `select()` for non-blocking I/O, allowing simultaneous keyboard input and network listening.
* **Safe Binary Chunking:** Files are streamed in secure 900-byte encrypted chunks, preventing memory overflow and ensuring safe disk I/O.

## ⚙️ Prerequisites

* **C++17** or higher
* **CMake** (3.10+)
* **Libsodium** (`sudo apt install libsodium-dev` on Debian/Ubuntu systems)

## 🚀 Build Instructions

This project uses CMake for easy compilation.

```bash
git clone https://github.com/yourusername/SecureP2P.git
cd SecureP2P
mkdir build && cd build
cmake ..
make
```

## 📖 Usage

**1. Start the Rendezvous Server**
Run this on a publicly accessible machine (e.g., an AWS EC2 instance).
```bash
./rendezvous
```
*(Note: Update the `SERVER_IP` macro in `src/main.cpp` to match your server's public IP before compiling the clients).*

**2. Start the Peer Nodes**
On machine A:
```bash
./client Alice
```

On machine B:
```bash
./client Bob
```

**3. Interactive Menu**
Once booted, the node enters a listening state. Type `1` to view online peers, or `2` to initiate a cryptographic handshake and file transfer.



## 👨‍💻 Author

**Muhammad Abubaker Rashid**

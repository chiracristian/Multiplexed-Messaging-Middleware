# Multiplexed Messaging Middleware

A high-performance, low-latency message broker designed to bridge UDP telemetry data with TCP subscribers. This middleware implements a custom application-level protocol to handle high-throughput data streams, featuring a hierarchical topic-based subscription model with advanced wildcard support.

## Key Features

* **Real-time Protocol Bridging**: Efficiently routes messages from UDP data sources to multiple concurrent TCP subscribers.
* **Hierarchical Topic Matching**: Utilizes a custom **Trie-based data structure** for efficient $O(L)$ topic lookups, where $L$ is the depth of the topic hierarchy.
* **Advanced Wildcard Support**: Implements MQTT-style wildcards for flexible data filtering:
    * `+` : Matches exactly one level in the hierarchy.
    * `*` : Matches zero or more levels (multi-level matching).
* **Low-Latency Engineering**: Optimizes delivery by disabling **Nagle’s Algorithm** (TCP_NODELAY) to ensure immediate transmission of time-critical packets.
* **Connection Persistence**: Maintains subscriber states and topic interests server-side, allowing clients to disconnect and reconnect without losing their subscription configurations.
* **I/O Multiplexing**: Leverages the `poll()` system call to manage multiple TCP connections and UDP ingress within a single-threaded, non-blocking execution model.

## Technical Stack

* **Language**: C++17
* **Networking**: POSIX Sockets (TCP/UDP)
* **Data Structures**: Custom Topic Trie, `std::unordered_map`, and `std::unordered_set` for $O(1)$ client and session management.
* **Concurrency**: Event-driven I/O multiplexing.

## Architecture

The middleware acts as a centralized **Pub/Sub Broker**:
1.  **UDP Ingress**: Receives raw datagrams containing topics and payloads from external sensors or clients.
2.  **Message Processing**: Parses structured UDP data into standardized application-layer frames.
3.  **Topic Routing**: The `TopicTrie` matches the incoming message topic against active subscription patterns.
4.  **TCP Egress**: Dispatches data to matching subscribers using a custom framing protocol to ensure message integrity over the TCP stream.

## Getting Started

### Prerequisites
* GCC/G++ compiler
* Linux-based environment (POSIX sockets)

### Installation
1. Clone the repository and navigate in its directory
2. Run `make`

## Execution

1. **Start the broker**
```bash
./server <PORT>
```
*Example*: `./server 8080`

2. **Connect a Subscriber**
```bash
./subscriber <CLIENT_ID> <SERVER_IP> <SERVER_PORT>
```
*Example*: `./subscriber C1 127.0.0.1 8080`

## Custom Protocol

Because TCP is a stream-oriented protocol, this project implements a custom framing layer to handle message boundaries

* **Message Integrity**: Uses `sendall` and `recvall` wrappers to handle partial reads and fragmentation.

* **Handshake**: Clients identify via a unique ID to facilitate session persistence.

* **Asynchronous Commands**: Supports concurrent `subscribe`, `unsubscribe`, and `exit` commands via multiplexed input from `stdin`.

# Chatroom Server/Clients

This project is a chatroom server and client system written entirely in C. It allows clients to join different chatrooms on a server and communicate with one another in real time. The implementation leverages channels, threads, and various concurrency mechanisms to handle multiple clients simultaneously.

## Features

- **Multiple Chatrooms:** Clients can join different chatrooms.
- **Real-time Communication:** Clients can send and receive messages in real time.
- **Concurrency:** Utilizes channels, threads, and other concurrency techniques to manage multiple clients.

## Technologies Used

- **C:** The entire project is implemented in the C programming language.

## Getting Started

### Prerequisites

- GCC or any C compiler
- Make

### Installation

1. Clone the repository:

    ```bash
    git clone https://github.com/yourusername/ChatroomServerClients.git
    cd ChatroomServerClients
    ```

2. Build the project using Make:

    ```bash
    make all
    ```

### Running the Server

To start the chatroom server, run:

```bash
./chat_server port_num Chat-Room-Names
```

### Connecting

To connect as a client to the server, run: 
```bash
nc machine_name port
```
You will then be prompted to create a chat username and select a room!

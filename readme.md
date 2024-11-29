# Multi-Room Chat System

A robust multi-room chat system implemented in C, featuring private messaging, room management, and clean process handling. This system demonstrates various systems programming concepts including socket programming, inter-process communication (IPC), thread management, and signal handling.

## Features

- Multi-room chat support
- Private messaging between users
- Clean process termination
- Real-time message broadcasting
- User-friendly command interface
- Robust error handling and cleanup

## Architecture

### Server (`server.c`)

The server implements a multi-process architecture with the following components:

#### Core Components:
1. **Main Process**
   - Handles incoming connections
   - Manages shared memory and semaphores
   - Coordinates child processes
   - Handles graceful shutdown

2. **Child Processes**
   - One process per client
   - Handles client message processing
   - Manages room membership
   - Processes commands

3. **Message Thread**
   - Dedicated thread for message distribution
   - Handles broadcasting to rooms
   - Manages private messages

#### Key Features:
- **Shared Memory**: Uses shared memory segment for client data
- **Semaphores**: Ensures thread-safe access to shared resources
- **Signal Handling**: Proper cleanup on SIGINT/SIGTERM
- **Room Management**: Supports multiple chat rooms
- **Command Processing**: Handles /join (join into the server), /pm (private messaging), and /exit (exit the server) commands

### Client (`client.c`)

The client uses a multi-threaded architecture:

#### Core Components:
1. **Main Thread**
   - Handles user input
   - Manages connection to server
   - Processes user commands

2. **Receive Thread**
   - Handles incoming messages
   - Updates display
   - Manages server disconnection

#### Key Features:
- **Clean UI**: Command prompt with message display
- **Signal Handling**: Easy shutdown on Ctrl+C
- **Error Handling**: Robust connection and communication error handling
- **Thread Safety**: Mutex-protected console output

## Commands

1. `/join <room>`: Join a specific chat room
2. `/pm <user> <message>`: Send a private message to a user
3. `/exit`: Leave the chat server

## Technical Implementation Details

### Server-side Implementation:

1. **Process Management**
   - Uses `fork()` for each client connection
   - Main process coordinates child processes
   - Clean termination with process group handling

2. **Memory Management**
   - Shared memory segment for client data
   - Semaphore-protected access
   - Proper cleanup on shutdown

3. **Signal Handling**
   - Uses `sigaction` for reliable signal handling
   - Prevents multiple cleanup attempts
   - Graceful shutdown sequence

### Client-side Implementation:

1. **Network Communication**
   - TCP/IP sockets for reliable communication
   - Non-blocking message reception
   - Robust connection handling

2. **User Interface**
   - Thread-safe console output
   - Clean prompt handling
   - Real-time message display

## Usage

### Starting the Server:
```bash
./server
```
Server listens on port 8888 by default.

### Starting a Client:
```bash
./client <server_ip> [port]
```
Example:
```bash
./client 127.0.0.1 8888
```

## Error Handling

Both server and client implement comprehensive error handling:

1. **Server**:
   - Connection failures
   - Resource allocation errors
   - Process termination
   - Memory management errors

2. **Client**:
   - Connection loss
   - Invalid commands
   - Server disconnection
   - Resource cleanup

## Implementation Notes

1. **Cleanup Process**:
   - Proper resource deallocation
   - Child process termination
   - Socket cleanup
   - IPC resource cleanup

2. **Thread Safety**:
   - Mutex-protected shared resources
   - Semaphore-controlled access
   - Safe process termination

3. **Performance Considerations**:
   - Non-blocking operations where possible
   - Efficient message broadcasting
   - Optimized resource usage

## Building

Compile the server and client:
```bash
gcc -o server server.c -pthread
gcc -o client client.c -pthread
```

## Dependencies
- POSIX compliant system
- pthread library
- Standard C libraries
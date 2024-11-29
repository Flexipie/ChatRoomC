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

## System Architecture Overview

### Shared Data Structures

#### Client Structure
```c
typedef struct {
    int socket;
    char username[USERNAME_SIZE];
    char current_room[ROOM_NAME_SIZE];
    bool is_active;
    pid_t handler_pid;
} Client;
```
- Maintains client connection state
- Tracks current room and activity status
- Associates process ID with client handler

#### Message Data Structure
```c
typedef struct {
    int target_socket;
    char message[BUFFER_SIZE];
} MessageData;
```
- Used for message routing between clients
- Maintains message integrity during transmission

## Server-Side Implementation

### Core Functions

#### 1. Main Process Management
`main()`:
- Initializes server socket on port 8888
- Sets up shared memory and semaphores
- Spawns message handling thread
- Accepts incoming connections
- Creates child processes for each client

#### 2. Client Connection Handling
`handle_client(int client_socket)`:
- Manages individual client connections
- Processes incoming messages
- Handles command interpretation (/join, /pm, /exit)
- Maintains client state in shared memory
- Implements error handling and cleanup

#### 3. Room Management
`join_room(int client_index, const char *new_room)`:
- Manages room transitions
- Broadcasts join/leave messages
- Updates client room status
- Handles room-specific message routing

#### 4. Message Broadcasting
`broadcast_to_room(const char *message, const char *room, int exclude_socket)`:
- Sends messages to all room members
- Excludes sender from broadcast
- Implements room-specific message filtering
- Handles message delivery failures

#### 5. Private Messaging
`send_private_message(const char *from_username, const char *to_username, const char *message)`:
- Implements direct user-to-user messaging
- Validates recipient existence
- Handles delivery confirmation
- Manages error cases

#### 6. Resource Management
`cleanup()`:
- Releases shared memory
- Closes all client connections
- Removes semaphores
- Terminates child processes
- Ensures proper system cleanup

## Client-Side Implementation

### Core Functions

#### 1. Connection Management
`main(int argc, char *argv[])`:
- Establishes server connection
- Handles command-line arguments
- Initializes threads
- Sets up signal handlers

#### 2. Message Reception
`receive_messages(void *arg)`:
- Runs in separate thread
- Handles incoming server messages
- Updates client display
- Manages connection status

#### 3. User Interface
`show_prompt()` & `clear_line()`:
- Manages terminal interface
- Handles user input
- Maintains clean display

## Inter-Process Communication (IPC)

### Shared Memory
- Used for client state management
- Protected by semaphores
- Enables cross-process communication

### Message Pipes
- Used for asynchronous message delivery
- Implements non-blocking communication
- Handles message queuing

## Thread Safety

### Semaphore Usage
- Protects shared memory access
- Prevents race conditions
- Ensures data consistency

### Signal Handling
- Graceful shutdown implementation
- Resource cleanup coordination
- Process termination management

## Error Handling

### Connection Errors
- Socket timeout management
- Connection loss recovery
- Buffer overflow prevention

### Resource Exhaustion
- Client limit enforcement
- Memory allocation checks
- File descriptor management

## Performance Considerations

### Message Processing
- Non-blocking I/O operations
- Efficient message routing
- Minimal memory copying

### Scalability
- Process-per-client model
- Shared memory optimization
- Resource pooling

## Security Implementation

### Input Validation
- Message size limits
- Command syntax checking
- Buffer overflow prevention

### Resource Protection
- Client isolation
- Memory protection
- File descriptor limits

## Known Limitations

1. Fixed maximum client count
2. Single server instance
3. Basic authentication
4. Limited room persistence

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
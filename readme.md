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

## Key Module Overview

### Error Handling
- Comprehensive error detection and recovery mechanisms
- Custom error codes and descriptive error messages
- Exception handling for network failures and resource allocation
- Graceful degradation and cleanup procedures
- Logging system for error tracking and debugging

### Pointers and Structures
- Dynamic memory allocation using malloc/free
- Complex data structures for user and room management
- Pointer-based linked lists for message queues
- Structure-based client information management
- Memory-safe pointer operations with validation

### File Handling and Preprocessor Directives
- Configuration file parsing and management
- Log file creation and management
- Header file organization with include guards
- Conditional compilation for platform-specific code
- Macro definitions for constants and debug options

### Concurrent Programming
- Multi-process architecture with fork()
- Thread management with pthreads
- Mutex locks for resource synchronization
- Semaphores for process synchronization
- Race condition prevention mechanisms

### Networking and Sockets
- TCP/IP socket programming
- Client-server communication protocol
- Socket options and configurations
- Non-blocking I/O operations
- Network buffer management

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

## Building the Project

The chat system now supports cross-platform building using CMake. Follow these instructions for your platform:

### Windows

1. Install prerequisites:
   - Install [Visual Studio](https://visualstudio.microsoft.com/) with C++ development tools
   - Install [CMake](https://cmake.org/download/)

2. Build the project:
   ```cmd
   mkdir build
   cd build
   cmake ..
   cmake --build . --config Release
   ```

3. The executables will be in `build\Release\`:
   - `client.exe`
   - `server.exe`

### Linux/macOS

1. Install prerequisites:
   ```bash
   # Ubuntu/Debian
   sudo apt-get install build-essential cmake

   # macOS (using Homebrew)
   brew install cmake
   ```

2. Build the project:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

3. The executables will be in the `build` directory:
   - `client`
   - `server`

## Platform-Specific Notes

### Windows
- The Windows Firewall may prompt you to allow the applications through. Accept this to enable network communication.
- Run the executables from Command Prompt or PowerShell with administrator privileges if needed.

### Linux/macOS
- You may need to adjust firewall settings using `ufw` (Ubuntu) or System Preferences (macOS).
- Make sure to run `chmod +x` on the executables if needed.

## Dependencies
- POSIX compliant system
- pthread library
- Standard C libraries

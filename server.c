#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>

#define PORT 8888
#define MAX_CLIENTS 50
#define BUFFER_SIZE 1024
#define USERNAME_SIZE 32
#define ROOM_NAME_SIZE 32
#define SEM_NAME "/chat_sem"

typedef struct {
    int target_socket;
    char message[BUFFER_SIZE];
} MessageData;

typedef struct {
    int socket;
    char username[USERNAME_SIZE];
    char current_room[ROOM_NAME_SIZE];
    bool is_active;
    pid_t handler_pid;
} Client;

typedef struct {
    Client clients[MAX_CLIENTS];
    int message_pipe[2];  
} SharedData;

SharedData *shared_data;
sem_t *mutex_sem;
volatile bool server_running = true;
pthread_t msg_thread;
pid_t main_pid;
int server_socket;
static volatile sig_atomic_t cleanup_in_progress = 0;

void cleanup() {
    if (getpid() != main_pid || cleanup_in_progress > 1) {
        return;
    }
    cleanup_in_progress = 2;  // Mark that cleanup is in progress
    
    printf("\nCleaning up server...\n");
    server_running = false;

    // Send termination signal to all child processes
    kill(0, SIGTERM);
    
    // Wait for all child processes to terminate with a timeout
    int status;
    pid_t pid;
    time_t start_time = time(NULL);
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0 || (time(NULL) - start_time < 5)) {
        if (pid <= 0) {
            usleep(100000);  // Sleep for 100ms if no process was reaped
        }
    }

    // Force kill any remaining processes in our process group
    kill(0, SIGKILL);

    // Cancel and wait for message thread
    pthread_cancel(msg_thread);
    pthread_join(msg_thread, NULL);
    
    sem_wait(mutex_sem);
    // Close all client sockets
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (shared_data->clients[i].is_active) {
            close(shared_data->clients[i].socket);
            shared_data->clients[i].is_active = false;
        }
    }
    sem_post(mutex_sem);
    
    // Close server socket if it's open
    if (server_socket > 0) {
        close(server_socket);
    }
    
    // Cleanup IPC resources
    sem_close(mutex_sem);
    sem_unlink(SEM_NAME);
    close(shared_data->message_pipe[0]);
    close(shared_data->message_pipe[1]);
    munmap(shared_data, sizeof(SharedData));
    
    printf("Server shutdown complete\n");
    _exit(0);  // Use _exit instead of exit to avoid calling cleanup handlers again
}

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        // Only the main process should handle cleanup
        if (getpid() == main_pid) {
            // Prevent multiple cleanups
            if (cleanup_in_progress) {
                return;
            }
            cleanup_in_progress = 1;
            printf("\nReceived shutdown signal...\n");
            cleanup();
        } else {
            // Child processes just exit
            _exit(0);
        }
    }
}

void init_shared_memory() {
    // Create shared memory using mmap
    shared_data = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, 
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    if (shared_data == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    // Create socketpair for IPC
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, shared_data->message_pipe) < 0) {
        perror("socketpair failed");
        exit(EXIT_FAILURE);
    }

    // Create named semaphore
    sem_unlink(SEM_NAME);
    mutex_sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0644, 1);
    if (mutex_sem == SEM_FAILED) {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }

    // Initialize clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        shared_data->clients[i].is_active = false;
        shared_data->clients[i].socket = -1;
        shared_data->clients[i].handler_pid = 0;
    }
}

void send_message_to_socket(int socket, const char *message) {
    MessageData msg_data;
    msg_data.target_socket = socket;
    strncpy(msg_data.message, message, BUFFER_SIZE - 1);
    msg_data.message[BUFFER_SIZE - 1] = '\0';
    
    write(shared_data->message_pipe[1], &msg_data, sizeof(MessageData));
}

void *message_handler(void *arg) {
    MessageData msg_data;
    while (server_running) {
        ssize_t n = read(shared_data->message_pipe[0], &msg_data, sizeof(MessageData));
        if (n > 0 && server_running) {
            send(msg_data.target_socket, msg_data.message, strlen(msg_data.message), MSG_NOSIGNAL);
        }
    }
    return NULL;
}

int find_free_slot() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!shared_data->clients[i].is_active) {
            return i;
        }
    }
    return -1;
}

int find_client_by_username(const char *username) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (shared_data->clients[i].is_active && 
            strcmp(shared_data->clients[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

void broadcast_to_room(const char *message, const char *room, int exclude_socket) {
    sem_wait(mutex_sem);
    printf("Broadcasting to room %s: %s", room, message);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (shared_data->clients[i].is_active && 
            strcmp(shared_data->clients[i].current_room, room) == 0 &&
            shared_data->clients[i].socket != exclude_socket) {
            send_message_to_socket(shared_data->clients[i].socket, message);
        }
    }
    sem_post(mutex_sem);
}

void send_private_message(const char *from_username, const char *to_username, const char *message) {
    sem_wait(mutex_sem);
    int to_index = find_client_by_username(to_username);
    if (to_index == -1) {
        int from_index = find_client_by_username(from_username);
        if (from_index != -1) {
            char error_msg[BUFFER_SIZE];
            snprintf(error_msg, BUFFER_SIZE, "* Error: User '%s' not found\n", to_username);
            send_message_to_socket(shared_data->clients[from_index].socket, error_msg);
        }
        sem_post(mutex_sem);
        return;
    }

    char formatted_msg[BUFFER_SIZE];
    snprintf(formatted_msg, BUFFER_SIZE, "[PM from %s]: %s\n", from_username, message);
    send_message_to_socket(shared_data->clients[to_index].socket, formatted_msg);

    int from_index = find_client_by_username(from_username);
    if (from_index != -1) {
        snprintf(formatted_msg, BUFFER_SIZE, "[PM to %s]: %s\n", to_username, message);
        send_message_to_socket(shared_data->clients[from_index].socket, formatted_msg);
    }
    sem_post(mutex_sem);
}

void join_room(int client_index, const char *new_room) {
    sem_wait(mutex_sem);
    char leave_msg[BUFFER_SIZE];
    char old_room[ROOM_NAME_SIZE];
    
    // Store the old room name
    strncpy(old_room, shared_data->clients[client_index].current_room, ROOM_NAME_SIZE);
    
    // Send leave message to old room
    snprintf(leave_msg, BUFFER_SIZE, "* %s has left the room\n", 
             shared_data->clients[client_index].username);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (shared_data->clients[i].is_active && 
            strcmp(shared_data->clients[i].current_room, old_room) == 0 &&
            i != client_index) {
            send_message_to_socket(shared_data->clients[i].socket, leave_msg);
        }
    }
    
    // Update client's room
    strncpy(shared_data->clients[client_index].current_room, new_room, ROOM_NAME_SIZE);
    
    // Send join message to new room
    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, BUFFER_SIZE, "* %s has joined the room\n", 
             shared_data->clients[client_index].username);
    
    // Send room change confirmation to the client
    char confirm_msg[BUFFER_SIZE];
    snprintf(confirm_msg, BUFFER_SIZE, "* You have joined room: %s\n", new_room);
    send_message_to_socket(shared_data->clients[client_index].socket, confirm_msg);
    
    // Notify others in the new room
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (shared_data->clients[i].is_active && 
            strcmp(shared_data->clients[i].current_room, new_room) == 0 &&
            i != client_index) {
            send_message_to_socket(shared_data->clients[i].socket, join_msg);
        }
    }
    
    sem_post(mutex_sem);
}

void handle_client_disconnect(int client_index) {
    sem_wait(mutex_sem);
    if (shared_data->clients[client_index].is_active) {
        char leave_msg[BUFFER_SIZE];
        snprintf(leave_msg, BUFFER_SIZE, "* %s has left the chat\n", 
                 shared_data->clients[client_index].username);
        broadcast_to_room(leave_msg, shared_data->clients[client_index].current_room, -1);
        
        close(shared_data->clients[client_index].socket);
        shared_data->clients[client_index].is_active = false;
        printf("Client %s disconnected\n", shared_data->clients[client_index].username);
    }
    sem_post(mutex_sem);
    
    // Exit the child process
    exit(0);
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received;
    
    sem_wait(mutex_sem);
    int client_index = find_free_slot();
    if (client_index == -1) {
        printf("No free slots for new client\n");
        close(client_socket);
        sem_post(mutex_sem);
        return;
    }
    
    shared_data->clients[client_index].socket = client_socket;
    shared_data->clients[client_index].is_active = true;
    shared_data->clients[client_index].handler_pid = getpid();
    strncpy(shared_data->clients[client_index].current_room, "general", ROOM_NAME_SIZE);
    sem_post(mutex_sem);
    
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        char username[USERNAME_SIZE];
        if (sscanf(buffer, "JOIN:%s", username) == 1) {
            sem_wait(mutex_sem);
            if (find_client_by_username(username) != -1) {
                char error_msg[BUFFER_SIZE];
                snprintf(error_msg, BUFFER_SIZE, "* Error: Username '%s' is already taken\n", username);
                send_message_to_socket(client_socket, error_msg);
                shared_data->clients[client_index].is_active = false;
                close(client_socket);
                sem_post(mutex_sem);
                return;
            }
            
            strncpy(shared_data->clients[client_index].username, username, USERNAME_SIZE);
            printf("User %s joined (socket: %d, index: %d)\n", username, client_socket, client_index);
            sem_post(mutex_sem);
            
            char welcome_msg[BUFFER_SIZE];
            snprintf(welcome_msg, BUFFER_SIZE, 
                "* Welcome to the chat, %s!\n"
                "Available commands:\n"
                "  /join <room>  - Join a chat room\n"
                "  /pm <user> <message>  - Send a private message to a user\n"
                "  /exit  - Leave the chat\n"
                "You are currently in the 'general' room.\n", 
                username);
            send_message_to_socket(client_socket, welcome_msg);
            
            char join_msg[BUFFER_SIZE];
            snprintf(join_msg, BUFFER_SIZE, "* %s has joined the chat\n", username);
            broadcast_to_room(join_msg, "general", -1);
        }
    }

    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("Received from %s: %s\n", shared_data->clients[client_index].username, buffer);

        if (strncmp(buffer, "/pm ", 4) == 0) {
            char to_username[USERNAME_SIZE];
            char message[BUFFER_SIZE];
            char *space_pos = strchr(buffer + 4, ' ');
            if (space_pos != NULL) {
                int username_len = space_pos - (buffer + 4);
                strncpy(to_username, buffer + 4, username_len);
                to_username[username_len] = '\0';
                strncpy(message, space_pos + 1, BUFFER_SIZE - 1);
                send_private_message(shared_data->clients[client_index].username, to_username, message);
            }
        }
        else if (strncmp(buffer, "/join ", 6) == 0) {
            char room_name[ROOM_NAME_SIZE];
            strncpy(room_name, buffer + 6, ROOM_NAME_SIZE - 1);
            room_name[ROOM_NAME_SIZE - 1] = '\0';
            join_room(client_index, room_name);
        }
        else if (strncmp(buffer, "/exit", 5) == 0) {
            // Send exit acknowledgment to the client
            send_message_to_socket(shared_data->clients[client_index].socket, "SERVER_EXIT_ACK\n");
            handle_client_disconnect(client_index);
            return;  // Exit the handle_client function immediately
        }
        else {
            char formatted_msg[BUFFER_SIZE];
            snprintf(formatted_msg, BUFFER_SIZE, "%s: %s\n", 
                     shared_data->clients[client_index].username, buffer);
            broadcast_to_room(formatted_msg, shared_data->clients[client_index].current_room, -1);
        }
    }

    handle_client_disconnect(client_index);
}

int main() {
    // Store the main process ID
    main_pid = getpid();
    
    struct sockaddr_in server_addr;
    
    // Set up signal handling
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    // Initialize shared memory and IPC
    init_shared_memory();
    
    // Create message handler thread
    if (pthread_create(&msg_thread, NULL, message_handler, NULL) != 0) {
        perror("Failed to create message handler thread");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to bind socket");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    // Get and display server address information
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getsockname(server_socket, (struct sockaddr *)&addr, &addr_len) == 0) {
        char host[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, host, INET_ADDRSTRLEN);
        printf("\nServer Details:\n");
        printf("Local Address: %s\n", host);
        printf("Port: %d\n", ntohs(addr.sin_port));
        printf("\nTo connect locally:\n");
        printf("./client 127.0.0.1\n");
        printf("\nIf using ngrok:\n");
        printf("1. Run: ngrok tcp 8888\n");
        printf("2. Use the ngrok address and port to connect\n");
        printf("   Example: ./client 2.tcp.ngrok.io 12345\n\n");
    }

    printf("Waiting for connections...\n");
    
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Failed to listen");
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    printf("Server is listening on port %d\n", PORT);
    printf("Press Ctrl+C to shutdown the server\n");
    
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        
        if (!server_running) break;
        
        if (client_socket < 0) {
            if (errno == EINTR) continue;  // Handle interrupted system call
            perror("Failed to accept connection");
            continue;
        }
        
        printf("New client connected\n");
        
        pid_t pid = fork();
        if (pid == 0) {  // Child process
            close(server_socket);
            // Reset signal handlers for child process
            signal(SIGINT, handle_signal);
            signal(SIGTERM, handle_signal);
            handle_client(client_socket);
            exit(EXIT_SUCCESS);
        } else if (pid < 0) {
            perror("Failed to create child process");
        }
    }
    
    close(server_socket);
    cleanup();
    return 0;
}
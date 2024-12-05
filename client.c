#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024
#define USERNAME_SIZE 32

int sock = -1;
volatile bool client_running = true;
pthread_t receive_thread;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void show_prompt() {
    printf("> ");
    fflush(stdout);
}

void clear_line() {
    printf("\r\033[K"); // Clear the current line
    fflush(stdout);
}

void cleanup_client() {
    printf("\nCleaning up client...\n");
    client_running = false;
    
    // Cancel and wait for receive thread
    if (receive_thread) {
        pthread_cancel(receive_thread);
        pthread_join(receive_thread, NULL);
    }
    
    // Close socket
    if (sock >= 0) {
        close(sock);
    }
    
    printf("Client shutdown complete\n");
}

void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\nReceived shutdown signal...\n");
        cleanup_client();
        exit(0);
    }
}

void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    while (client_running) {
        int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            if (client_running) {
                printf("\nServer disconnected\n");
                client_running = false;
                break;
            }
            break;
        }
        buffer[bytes_received] = '\0';
        
        // Handle server exit acknowledgment
        if (strcmp(buffer, "SERVER_EXIT_ACK\n") == 0) {
            printf("\nExiting chat... Press enter to exit\n");
            client_running = false;
            // Force main thread to exit by closing the socket
            if (sock >= 0) {
                shutdown(sock, SHUT_RDWR);
                close(sock);
                sock = -1;
            }
            break;
        }
        
        pthread_mutex_lock(&mutex);
        clear_line();
        printf("%s\n", buffer);
        show_prompt();
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void print_usage(char *program) {
    printf("Usage: %s <server_ip> [port]\n", program);
    printf("Example: %s 192.168.1.100 8888\n", program);
    printf("         %s example.com\n", program);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        exit(1);
    }

    char *server_ip = argv[1];
    int port = (argc > 2) ? atoi(argv[2]) : 8888;

    // Set up signal handling
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address structure
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Convert IP address from string to binary form
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        printf("Invalid address or address not supported\n");
        cleanup_client();
        exit(EXIT_FAILURE);
    }

    // Connect to server
    printf("Connecting to %s:%d...\n", server_ip, port);
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        cleanup_client();
        exit(EXIT_FAILURE);
    }
    printf("Connected to server!\n");

    // Get username
    char username[USERNAME_SIZE];
    printf("Enter your username: ");
    if (fgets(username, USERNAME_SIZE, stdin) == NULL) {
        cleanup_client();
        exit(EXIT_FAILURE);
    }
    username[strcspn(username, "\n")] = 0;

    // Send join message
    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, BUFFER_SIZE, "JOIN:%s", username);
    if (send(sock, join_msg, strlen(join_msg), 0) < 0) {
        perror("Failed to send username");
        cleanup_client();
        exit(EXIT_FAILURE);
    }

    // Create receive thread
    if (pthread_create(&receive_thread, NULL, receive_messages, NULL) != 0) {
        perror("Failed to create receive thread");
        cleanup_client();
        exit(EXIT_FAILURE);
    }

    while (client_running) {
        char input[BUFFER_SIZE];
        show_prompt();
        if (fgets(input, BUFFER_SIZE, stdin) == NULL) break;
        
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) > 0) {
            if (send(sock, input, strlen(input), 0) < 0) {
                perror("Send failed");
                break;
            }
        }
    }

    cleanup_client();
    return 0;
}
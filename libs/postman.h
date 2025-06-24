#include "header.h"
#include "security.h"

#define PORT 52120          // TCP Server Port
#define BUFFER_SIZE 1024
#define BC_PORT 52121       // UDP Broadcast Port
#define BC_DISCOVERY_MSG "DISCOVER_FILE_TRANSFER"

// Structure to pass multiple arguments to the spinner thread
typedef struct {
    int *loading_state;     // Pointer to flag that indicates when to stop the spinner
    const char *message;    // Message to be shown with the spinner animation
} spinner_args;

// Prototype functions
char *get_broadcast_address(const char *interface_name);
char *get_ip_address(const char *interface_name);
void *listen_bc(void *show_log);
int get_neighbor(const char *interface_name);

/**
 * Spinner thread function that displays a rotating animation in the terminal.
 *
 * @param arg Pointer to a spinner_args structure containing:
 *            - loading_state: shared int pointer used to stop the spinner when set to 1
 *            - message: label text to display alongside the spinner
 * @return    NULL (no return value as it is intended to be used with pthreads)
 */
void *loading_spinner(void *arg) {
    spinner_args *args = (spinner_args *)arg;
    const char spinner[] = {'-', '\\', '|', '/'};

    int index = 0;

    // Display spinner until loading is complete
    while (*(args->loading_state) == 0) {
        printf("\r%s [\e[33m%c\e[0m]", args->message, spinner[index++ % 4]);

        fflush(stdout);
        usleep(100000);  // Sleep for 100ms between frames
    }

    printf("\r");  // Clear line after done
}

/**
 * Runs the server to receive an encrypted file from a client over a socket connection.
 *
 * @param output_path A string containing the output path of the received file.
 * @return 0 on success, -1 on any failure during socket operations, file access, or decryption.
 */
int server(const char *output_path) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int address_len = sizeof(address);

    unsigned char key[KEY_LENGTH];   // 32-byte symmetric encryption key
    unsigned char iv[IV_LENGTH];     // 16-byte initialization vector

    FILE *received_file;

    // Create a TCP socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        printf("\e[31mConnectionError: Failed to create the socket\e[0m\n");
        fflush(stdout);
        
        return -1;
    }

    // Configure address and port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to the given port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("\e[31mConnectionError: Failed to bind the socket to the port\e[0m\n");
        fflush(stdout);

        return -1;
    }

    // Start listening for incoming connections
    if (listen(server_fd, 3) < 0) {
        printf("\e[31mConnectionError: Failed to listen for incoming connections\e[0m\n");
        fflush(stdout);

        return -1;
    }

    printf("\e[33mWaiting for connection...\e[0m");
    fflush(stdout);

    pthread_t listen_thread;

    pthread_create(&listen_thread, NULL, listen_bc, (void *)1);

    // Accept a client connection
    new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&address_len);

    pthread_cancel(listen_thread);
    pthread_join(listen_thread, NULL);

    printf("\r\e[32mConnected                \e[0m");
    fflush(stdout);

    // Setup spinner for receiving
    spinner_args args;
    pthread_t thread;

    int loading_state = 0;

    args.loading_state = &loading_state;
    args.message = "Receiving";

    pthread_create(&thread, NULL, loading_spinner, &args);

    if (new_socket < 0) {
        loading_state = 1;

        pthread_join(thread, NULL);

        printf("\e[31mConnectionError: Failed to receive incoming connections\e[0m\n");
        fflush(stdout);
        
        return -1;
    }

    // Receive encryption key
    if (read(new_socket, key, sizeof(key)) <= 0) {
        loading_state = 1;

        pthread_join(thread, NULL);

        printf("\e[31mConnectionError: Failed to receive the key\e[0m\n");
        fflush(stdout);

        return -1;
    }

    // Receive IV
    if (read(new_socket, iv, sizeof(iv)) <= 0) {
        loading_state = 1;

        pthread_join(thread, NULL);

        printf("\e[31mConnectionError: Failed to receive IV\e[0m\n");
        fflush(stdout);

        return -1;
    }

    // Receive encrypted file name
    unsigned char cipher_text[BUFFER_SIZE + EVP_MAX_BLOCK_LENGTH];
    int cipher_text_len;

    if ((cipher_text_len = read(new_socket, cipher_text, sizeof(cipher_text))) <= 0) {
        loading_state = 1;

        pthread_join(thread, NULL);

        printf("\e[31mConnectionError: Failed to receive the file name\e[0m\n");
        fflush(stdout);

        return -1;
    }

    // Decrypt the file name
    unsigned char file_name[256];
    int decrypted_len = decrypt(cipher_text, cipher_text_len, key, iv, file_name);

    file_name[decrypted_len] = '\0';

    // Open file for writing
    received_file = fopen((output_path != NULL) ? output_path : (char *)file_name, "wb");

    if (received_file == NULL) {
        loading_state = 1;

        pthread_join(thread, NULL);

        printf("\e[31mFileError: Failed to write received file\e[0m\n");
        fflush(stdout);
        
        return -1;
    }

    // Decrypt and receive file content
    decrypt_file(received_file, BUFFER_SIZE, new_socket, key, iv);

    // Finish spinner
    loading_state = 1;

    pthread_join(thread, NULL);

    printf("\e[32m%s successfully received\e[0m\n", (output_path != NULL) ? (unsigned char *)(strrchr(output_path, '/') + 1) : file_name);
    fflush(stdout);

    // Clean up the memory
    fclose(received_file);
    close(new_socket);
    close(server_fd);
    
    return 0;
}

/**
 * Sends an encrypted file to the server over a socket connection.
 *
 * @param server_ip A string containing the server's IPv4 address.
 * @param file_path A string containing the path to the file to be sent.
 * @return 0 on success, -1 on any failure during socket operations, file access, or encryption.
 */
int client(char *server_ip, char *file_path) {
    int client_socket = 0;
    struct sockaddr_in serv_address;
    char buffer[BUFFER_SIZE] = {0};

    unsigned char key[KEY_LENGTH];   // 32-byte AES encryption key
    unsigned char iv[IV_LENGTH];     // 16-byte initialization vector

    // Generate random key and IV
    RAND_bytes(key, sizeof(key));
    RAND_bytes(iv, sizeof(iv));

    FILE *file;

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket < 0) {
        printf("\e[31mConnectionError: Failed to create the client socket\e[0m\n");
        fflush(stdout);
        
        return -1;
    }

    // Configure server address
    serv_address.sin_family = AF_INET;
    serv_address.sin_port = htons(PORT);

    if(inet_pton(AF_INET, server_ip, &serv_address.sin_addr) <= 0) {
        printf("\e[31mConnectionError: Invalid server IP address format\e[0m\n");
        fflush(stdout);
        
        return -1;
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&serv_address, sizeof(serv_address)) < 0) {
        printf("\e[31mConnectionError: Failed to connect to the server\e[0m\n");
        fflush(stdout);
        
        return -1;
    }

    // Send key and IV
    if (send(client_socket, key, sizeof(key), 0) < 0) {
        printf("\e[31mConnectionError: Failed to send the key to the server\e[0m\n");
        fflush(stdout);

        return -1;
    }

    if (send(client_socket, iv, sizeof(iv), 0) < 0) {
        printf("\e[31mConnectionError: Failed to send IV to the server\e[0m\n");
        fflush(stdout);

        return -1;
    }

    // Setup spinner for sending
    spinner_args args;
    pthread_t thread;

    int loading_state = 0;

    args.loading_state = &loading_state;
    args.message = "Sending";

    pthread_create(&thread, NULL, loading_spinner, &args);

    // Open the file to be sent
    file = fopen(file_path, "rb");

    if (file == NULL) {
        loading_state = 1;

        pthread_join(thread, NULL);

        printf("\e[31mFileError: Failed to read the file\e[0m\n");
        fflush(stdout);
        
        return -1;
    }

    // Extract file name from the full path
    char *file_name = strrchr(file_path, '/');

    file_name = (file_name) ? file_name + 1 : file_path;

    // Encrypt and send the file name
    unsigned char cipher_text[BUFFER_SIZE + EVP_MAX_BLOCK_LENGTH];
    int cipher_text_len = encrypt((unsigned char *)file_name, strlen(file_name), key, iv, cipher_text);

    if (send(client_socket, cipher_text, cipher_text_len, 0) < 0) {
        loading_state = 1;

        pthread_join(thread, NULL);

        printf("\e[31mConnectionError: Failed to send the file name to the server\e[0m\n");
        fflush(stdout);

        return -1;
    }

    // Optional delay to ensure file name is processed
    sleep(1);

    // Encrypt and send file content
    encrypt_file(file, BUFFER_SIZE, client_socket, key, iv);

    // Finish spinner
    loading_state = 1;

    pthread_join(thread, NULL);

    printf("\e[32m%s successfully sent\e[0m\n", file_name);
    fflush(stdout);

    // Clean up the memory
    fclose(file);
    close(client_socket);
    
    return 0;
}

/**
 * Retrieves the broadcast address of a given network interface (e.g., "wlan0").
 *
 * @param interface_name A string representing the name of the network interface.
 * @return A dynamically allocated string containing the broadcast address, or NULL on failure.
 *         Caller must free the returned string.
 */
char *get_broadcast_address(const char *interface_name) {
    int temp_file;
    struct ifreq ifr;

    // Create a temporary socket to perform ioctl operations
    temp_file = socket(AF_INET, SOCK_DGRAM, 0);

    if (temp_file == -1) {
        printf("\e[31mConnectionError: Failed to create the socket\e[0m\n");

        return NULL;
    }

    // Set interface name and address family for the ioctl call
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);

    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    ifr.ifr_addr.sa_family = AF_INET;

    // Get the broadcast address of the given interface
    if (ioctl(temp_file, SIOCGIFBRDADDR, &ifr) == -1) {
        printf("\e[31mConnectionError: Failed to get IP address\e[0m\n");
        close(temp_file);

        return NULL;
    }

    close(temp_file);

    // Extract and return the broadcast address as a string
    struct sockaddr_in *broadcasr_address = (struct sockaddr_in *)&ifr.ifr_broadaddr;

    return strdup(inet_ntoa(broadcasr_address->sin_addr));
}

/**
 * Retrieves the IPv4 address of a given network interface (e.g., "wlan0").
 *
 * @param interface_name A string representing the name of the network interface.
 * @return A dynamically allocated string containing the IP address, or NULL on failure.
 *         Caller must free the returned string.
 */
char *get_ip_address(const char *interface_name) {
    int temp_file;
    struct ifreq ifr;

    // Create a temporary socket to perform ioctl operations
    temp_file = socket(AF_INET, SOCK_DGRAM, 0);

    if (temp_file == -1) {
        printf("\e[31mConnectionError: Failed to create the socket\e[0m\n");

        return NULL;
    }

    // Set interface name and address family for the ioctl call
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);

    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    ifr.ifr_addr.sa_family = AF_INET;

    // Get the IP address of the given interface
    if (ioctl(temp_file, SIOCGIFADDR, &ifr) == -1) {
        printf("\e[31mConnectionError: Failed to get IP address\e[0m\n");
        close(temp_file);

        return NULL;
    }

    close(temp_file);

    // Extract and return the IP address as a string
    struct sockaddr_in *ip_address = (struct sockaddr_in *)&ifr.ifr_addr;

    return strdup(inet_ntoa(ip_address->sin_addr));
}

/**
 * Get device information, such as hostname, IP address, and broadcast address.
 *
 * @param interface_name An optional string representing the name of the network interface.
 * @return NULL (no return value)
 */
void get_info(const char *interface_name) {
    char hostname[128];
    char *ip_address = get_ip_address((interface_name != NULL) ? interface_name : "wlan0");
    char *broadcast_address = get_broadcast_address((interface_name != NULL) ? interface_name : "wlan0");

    gethostname(hostname, sizeof(hostname));

    printf("hostname\t: \e[36m%s\e[0m\nip_address\t: \e[36m%s\e[0m\nbroadcast\t: \e[36m%s\e[0m\n", hostname, ip_address, broadcast_address);
}

/**
 * Listens for incoming UDP broadcast discovery messages and responds with this device's hostname and IP address.
 *
 * @param show_log 0 to show every message or log and 1 to hide them
 * @return NULL (no return value)
 */
void *listen_bc(void *show_log) {
    int client_socket;
    struct sockaddr_in address, client;
    char buffer[BUFFER_SIZE];
    socklen_t client_len = sizeof(client);

    // Create an UDP socket to listen for broadcast messages
    client_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (client_socket < 0) {
        printf("\e[31mConnectionError: Failed to create the socket\e[0m\n");
        fflush(stdout);
    }

    // Configure socket to accept messages from any address on the broadcast port
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons(BC_PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind socket to the specified port and address
    bind(client_socket, (struct sockaddr*)&address, sizeof(address));

    if (show_log == 0) {
        printf("\e[33mWaiting for connection...\e[0m");
        fflush(stdout);
    }

    int is_first_line = 0;

    while (1) {
        // Clear buffer and wait for incoming message
        memset(buffer, 0, BUFFER_SIZE);
        recvfrom(client_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client, &client_len);

        // Check if the message matches the expected discovery string
        if (strncmp(buffer, BC_DISCOVERY_MSG, 23) == 0) {
            char hostname[128];

            gethostname(hostname, sizeof(hostname));  // Get this device's hostname

            char reply[BUFFER_SIZE];

            // Construct reply message with hostname and IP address
            snprintf(reply, sizeof(reply), "hostname: \e[36m%s\e[0m | ip_address: \e[36m%s\e[0m", hostname, inet_ntoa(client.sin_addr));

            // Send the reply back to the broadcasting client
            sendto(client_socket, reply, strlen(reply), 0, (struct sockaddr*)&client, client_len);

            if (is_first_line == 0) {
                printf("\r");
                
                is_first_line = 1;
            }

            // Log the sender's IP address
            if (show_log == 0) {
                printf("Receive packets from \e[36m%s\e[0m\n", inet_ntoa(client.sin_addr));
                fflush(stdout);
            }
        }
    }

    close(client_socket);
}

/**
 * Sends a broadcast discovery message to the local network to find other devices.
 *
 * @param interface_name An optional string representing the name of the network interface.
 * @return 0 on success, -1 if socket creation fails or no devices respond.
 */
int get_neighbor(const char *interface_name) {
    int neighbor_socket;
    struct sockaddr_in addr, sender;
    char buffer[BUFFER_SIZE];
    socklen_t sender_len = sizeof(sender);

    // Create an UDP socket for broadcasting
    neighbor_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (neighbor_socket < 0) {
        printf("\e[31mConnectionError: Failed to create the socket\e[0m\n");
        fflush(stdout);

        return -1;
    }

    // Enable broadcast option on socket
    int broadcast_enable = 1;
    char *ip_broadcast = get_broadcast_address((interface_name != NULL) ? interface_name : (const char *)"wlan0");

    setsockopt(neighbor_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));

    // Configure broadcast address
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(BC_PORT);
    addr.sin_addr.s_addr = inet_addr(ip_broadcast);

    // Send discovery message
    sendto(neighbor_socket, BC_DISCOVERY_MSG, strlen(BC_DISCOVERY_MSG), 0,
           (struct sockaddr*)&addr, sizeof(addr));

    printf("\e[33mBroadcasting discovery...\e[0m");
    fflush(stdout);

    // Set timeout for receiving responses
    struct timeval timeout = {2, 0};

    setsockopt(neighbor_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        // Wait for response from other devices
        int devices = recvfrom(neighbor_socket, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&sender, &sender_len);

        if (devices > 0) {
            buffer[devices] = '\0';  // Null-terminate response

            printf("\r%s\n", buffer);
            fflush(stdout);
        } 
        else {
            break;  // Timeout reached, no more devices found
        }
    }

    printf("\rNo other devices were found on this network\n");
    fflush(stdout);

    close(neighbor_socket);

    return 0;
}

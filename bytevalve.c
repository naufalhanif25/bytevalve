#include "libs/postman.h"

#define VERSION "0.0.1"

/**
 * Entry point of the program. Parses command-line arguments and routes execution.
 *
 * @param argc Argument count.
 * @param argv Argument vector (array of strings representing command-line arguments).
 * @return 0 on successful execution or help display, -1 on error or invalid arguments.
 */
int main(int argc, const char *argv[]) {
    const char *name = 
        "        ___       __     _   __     __                 \n" 
        "       / _ )__ __/ /____| | / /__ _/ /  _____          \n"
        "      / _  / // / __/ -_) |/ / _ `/ / |/ / -_)         \n"
        "     /____/\\_, /\\__/\\__/|___/\\_,_/_/|___/\\__/     \n"
        "          /___/                                        \n";
    
    // Help message to display
    const char *help_message = 
        "\e[33mUsage: program [option] [arguments...]\e[0m\n\n"
        "Options:\n\n"
        "\e[32m-h or --help                           \e[0mDisplay help messages to the console\n"
        "                                       No arguments are required for this option.\n"
        "                                       Example:\n"
        "                                       \e[33mprogram -h \e[0mor \e[33mprogram --help\e[0m\n\n"
        "\e[32m-i or --info                           \e[0mDisplay the device information to the console, such as hostname, IP address, and broadcast address.\n"
        "                                       No arguments are required for this option.\n"
        "                                       Example:\n"
        "                                       \e[33mprogram -i \e[0mor \e[33mprogram --info\e[0m\n\n"
        "\e[32m-l or --listen                         \e[0mListen for broadcast discovery messages from clients.\n"
        "                                       This option can display a list of connected devices with the same network.\n"
        "                                       No arguments are required for this option.\n"
        "                                       Example:\n"
        "                                       \e[33mprogram -l \e[0mor \e[33mprogram --listen\e[0m\n\n"
        "\e[32m-n or --neighbor <INT>                 \e[0mGet neighboring networks with the same connection.\n"
        "                                       <INT> is an optional argument for the name of the network interface used.\n"
        "                                       By default, the argument of <INT> is wlan0.\n"
        "                                       Example:\n"
        "                                       \e[33mprogram -n wlan0 \e[0mor \e[33mprogram -neighbor wlan0\e[0m\n\n"
        "\e[32m-r or --receive <OUT_PATH>             \e[0mRun the program as a receiver (server mode).\n"
        "                                       <OUTPUT_PATH> is an optional argument for the output path of the received file.\n"
        "                                       By default <OUTPUT_PATH> is in the current directory.\n"
        "                                       Example:\n"
        "                                       \e[33mprogram -r /home/user/Documents/file.tar \e[0mor \e[33mprogram -receive /home/user/Documents/file.tar\e[0m\n\n"
        "\e[32m-s or --send <DEST_IP> <FILE_PATH>     \e[0mSend the file to receiver (server) IP address filled in as the <DEST_IP> argument.\n"
        "                                       The <FILE_PATH> argument is the path of the file to be sent.\n"
        "                                       Example:\n"
        "                                       \e[33mprogram -s 192.168.1.100 /home/user/Documents/file.tar \e[0mor \e[33mprogram -send 192.168.1.100 /home/user/Documents/file.tar\e[0m\n\n"
        "\e[32m-v or --version                        \e[0mDisplay the program version to the console.\n"
        "                                       No arguments are required for this option.\n"
        "                                       Example:\n"
        "                                       \e[33mprogram -v \e[0mor \e[33mprogram --version\e[0m\n\n"
        "\e[32m-w or --whoami                         \e[0mDisplay the hostname to the console.\n"
        "                                       No arguments are required for this option.\n"
        "                                       Example:\n"
        "                                       \e[33mprogram -w \e[0mor \e[33mprogram --whoami\e[0m\n\n"
        "See the GitHub page at \e[36mhttps://github.com/naufalhanif25/bytevalve.git\e[0m\n";

    if (argc >= 2) {
        // Handle help option
        if ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0)) {
            printf("\e[32m%s\e[0m\n", name);
            printf("%s", help_message);

            return 0;
        }
        // Handle get info
        else if ((strcmp(argv[1], "-i") == 0) || (strcmp(argv[1], "--info") == 0)) {
            get_info((argc == 3 && argv[2] != NULL) ? argv[2] : NULL);

            return 0;
        }
        // Handle broadcast listener
        else if ((strcmp(argv[1], "-l") == 0) || (strcmp(argv[1], "--listen") == 0)) {
            listen_bc(0);

            return 0;
        }
        // Handle get neigbor networks
        else if ((strcmp(argv[1], "-n") == 0) || (strcmp(argv[1], "--neighbor") == 0)) {
            get_neighbor((argc == 3 && argv[2] != NULL) ? argv[2] : NULL);

            return 0;
        }
        // Handle receive/server mode
        else if ((strcmp(argv[1], "-r") == 0) || (strcmp(argv[1], "--receive") == 0)) {
            const int server_return = server((argc == 3 && argv[2] != NULL) ? argv[2] : NULL);

            if (server_return == -1) return -1;
            else return 0;
        } 
        // Handle send/client mode
        else if ((strcmp(argv[1], "-s") == 0) || (strcmp(argv[1], "--send") == 0)) {
            // Ensure required arguments are provided: DEST_IP and FILE_PATH
            if (argc > 3 && argv[2] != NULL && argv[3] != NULL) {
                const int client_return = client((char *)argv[2], (char *)argv[3]);

                if (client_return == -1) return -1;
                else return 0;
            } 
            // Missing arguments for --send
            else {
                printf("\e[31mCommandError: The arguments for the '%s' option are not recognized\e[0m\n\n", argv[1]);
                printf("\e[32m%s\e[0m\n", name);
                printf("%s", help_message);

                return -1;
            }
        }
        // Handle display version
        else if ((strcmp(argv[1], "-v") == 0) || (strcmp(argv[1], "--version") == 0)) {
            printf("\e[32mByteValve\e[0m %s\n", VERSION);

            return 0;
        }
        // Handle display hostname
        else if ((strcmp(argv[1], "-w") == 0) || (strcmp(argv[1], "--whoami") == 0)) {
            char hostname[128];
            
            gethostname(hostname, sizeof(hostname));

            printf("%s\n", hostname);

            return 0;
        }
        // Handle unrecognized command
        else {
            printf("\e[31mCommandError: The ");

            // Print entire command input as part of the error message
            for (int index = 0; index < argc; index++) {
                if (index == 0) printf("'");

                printf("%s", argv[index]);

                if (index == argc - 1) printf("'");
                else printf(" ");
            }

            printf(" command is not recognized\e[0m\n\n");
            printf("\e[32m%s\e[0m\n", name);
            printf("%s", help_message);

            return -1;
        }
    } 
    // No arguments were provided
    else {
        printf("\e[31mCommandError: An option is required\e[0m\n\n");
        printf("\e[32m%s\e[0m\n", name);
        printf("%s", help_message);
        
        return -1;
    }
}
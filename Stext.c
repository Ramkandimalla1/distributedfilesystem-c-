#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PORT 9008
#define BUFFER_SIZE 1024

// function prototypes
void handle_request(int client_socket);
int create_tar(const char *tar_name, const char *dir_path, const char *filetype);
void send_file_to_smain(int client_socket, const char *file_path);
int initialize_socket();
int bind_socket(int server_socket);
void listen_for_connections(int server_socket);
void handle_dtar_command(int client_socket, char *buffer);
void handle_upload_command(int client_socket, char *buffer);
void handle_dfile_command(int client_socket, char *buffer);

int accept_connection(int server_socket, struct sockaddr_in *client_addr);

void handle_new_connection(int server_socket, int client_socket);

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr; // Structure to hold server address,client addresss
    socklen_t addr_size;
    pid_t child_pid;

   // Initialize the server socket
    server_socket = initialize_socket();
    if (server_socket < 0) {
        exit(EXIT_FAILURE);  // Exit if socket creation fails
    }
   // Bind the server socket
    if (bind_socket(server_socket) < 0) {
        close(server_socket);  // Close socket if binding fails
           // Close the server socket to release the resource
        exit(EXIT_FAILURE);
         //EXIT_FAILURE is a macro that represents a failure termination status for exit(), indicating that the program did not succeed.
      
    }
      // Listen for incoming connections
    listen_for_connections(server_socket);


    while (1)
     {
        client_socket = accept_connection(server_socket, &client_addr);
        if (client_socket < 0) {
            continue; // Continue trying to accept connections
        }
         // Fork a new process for each request
        handle_new_connection(server_socket, client_socket);
       
    }

    close(server_socket);
     // Close the server socket to release the resource
      
    return 0;
}
int accept_connection(int server_socket, struct sockaddr_in *client_addr) {
    
    socklen_t addr_size = sizeof(*client_addr); // Get the size of the client address structure
    int client_socket = accept(server_socket, (struct sockaddr*)client_addr, &addr_size);
    if (client_socket < 0) {
        perror("Connection acceptance failed");
        close(server_socket);
             // Close the server socket to release the resource
            exit(EXIT_FAILURE);
            //EXIT_FAILURE is a macro that represents a failure termination status for exit(), indicating that the program did not succeed.
    
    }
    return client_socket;
}

// Creates a socket and returns its descriptor
int initialize_socket() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        return -1;
    }
    return server_socket;
}
// Binds the created socket to a specified IP and port
int bind_socket(int server_socket) {
    struct sockaddr_in server_addr;
   server_addr.sin_family = AF_INET;// Address family
    server_addr.sin_port = htons(PORT);// Server port
    server_addr.sin_addr.s_addr = INADDR_ANY;// Server IP address

    // Bind socket
   if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Socket binding failed");
        return -1;
    }
    return 0;
}
// Listens for incoming connections on the bound socket
void listen_for_connections(int server_socket) {
    if (listen(server_socket, 10) < 0) {
        perror("Socket listening failed");
        exit(EXIT_FAILURE);
    }
    printf("Server is listening on port %d\n", PORT);
}
void handle_new_connection(int server_socket, int client_socket) {
    pid_t child_pid = fork();
    if (child_pid == 0) {
        // Child process
        close(server_socket); // Child does not need the server socket
        handle_request(client_socket);
        exit(EXIT_SUCCESS); // Exit child process after handling the request
    } else if (child_pid > 0) {
        // Parent process
        close(client_socket); // Parent does not need the client socket
    } else {
        // Fork failed
        perror("Fork failed");
        close(server_socket); // Close server socket on fork failure
        exit(EXIT_FAILURE); // Terminate program on fork failure
    }
}

void handle_request(int client_socket) {
    // Buffer to hold received data
    char buffer[BUFFER_SIZE];
    int bytes_read;

    // Read data from the client socket
    bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_read > 0) {
        // Null-terminate the received data
        buffer[bytes_read] = '\0';
        // Copy the received data into the command buffer
        char command[BUFFER_SIZE];
        strcpy(command, buffer);

        // Print the received command for debugging purposes
        printf("Received command: %s\n", command);

        // Handle the "UPLOAD" command
        if (strncmp(command, "UPLOAD", 6) == 0) {
            handle_upload_command(client_socket, buffer);
        }
        // Handle the "DFILE" command
        else if (strncmp(command, "DFILE", 5) == 0) {
         handle_dfile_command(client_socket, buffer);
        }
        // Handle the "dtar" command
        else if (strncmp(command, "dtar", 4) == 0) {
          handle_dtar_command(client_socket, buffer);
        }
    }

    // Close the client socket after handling the request
    close(client_socket);
}


int create_tar(const char *tar_name, const char *dir_path, const char *filetype) {
    // Buffer to hold the command to be executed
    char command[BUFFER_SIZE];
    // Format the command string to find files with the specified filetype and create a tar archive
    snprintf(command, sizeof(command), "find %s -name '*%s' | xargs tar -cvf %s", dir_path, filetype, tar_name);
    // Print the command for debugging purposes
    printf("%s \n", command);
    // Execute the command using the system function and store the result
    int result = system(command);
    // Print the result of the command execution
    printf("%d \n", result);
    // Return the result of the command execution
    return result;
}

void send_file_to_smain(int client_socket, const char *file_path) {
    // Open the file for reading
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        // If the file cannot be opened, print an error message
        perror("Failed to open file for reading");
        // Send an error message to the client
        send(client_socket, "Failed to fetch file", 20, 0);
        return;
    }

    // Buffer to hold file data
    char file_buffer[BUFFER_SIZE];
    int read_bytes;
    // Read the file data in a loop and send it to the client
    while ((read_bytes = read(fd, file_buffer, BUFFER_SIZE)) > 0) {
        send(client_socket, file_buffer, read_bytes, 0);
    }
    
    // Print a message indicating that the file has been sent
    printf("send to main \n");
    
    // Close the file after sending the data
    close(fd);
}
// Handles the "UPLOAD" command from a client
void handle_upload_command(int client_socket, char *command) {
    // Extract the filename, destination path, and file content from the command
            char *filename = strtok(command + 7, " ");
            char *destination_path = strtok(NULL, " ");
            char *file_content = strtok(NULL, "");
            printf("Creating file at: %s\n", destination_path);
            
            // Open the destination file for writing
            int fd = open(destination_path, O_WRONLY | O_CREAT, 0644);
            if (fd < 0) {
                // Print an error message if the file cannot be opened
                perror("File open failed");
            } else {
                // Write the file content to the destination file
                write(fd, file_content, strlen(file_content));
                printf("written");
                // Close the file
                close(fd);
            }
}

// Handles the "DFILE" command from a client
void handle_dfile_command(int client_socket, char *command) {
    // Extract the file path from the command
            char *file_path = strtok(command + 6, " ");
            // Open the file for reading
            int fd = open(file_path, O_RDONLY);
            if (fd < 0) {
                // Print an error message if the file cannot be opened
                perror("File open failed");
                // Send an error message to the client
                send(client_socket, "Failed to fetch file", 20, 0);
            } else {
                // Buffer to hold file data
                char file_buffer[BUFFER_SIZE];
                int read_bytes;
                // Read the file data in a loop and send it to the client
                while ((read_bytes = read(fd, file_buffer, BUFFER_SIZE)) > 0) {
                    send(client_socket, file_buffer, read_bytes, 0);
                    printf("sending to client \n");
                }
                // Close the file
                close(fd);
            }
}

// Handles the "dtar" command from a client
void handle_dtar_command(int client_socket, char *command) {
     // Extract the file type from the command
            char *filetype = command + 5;
            // Buffer to hold the tar file name
            char tar_name[BUFFER_SIZE];
            // Directory path to create the tar file
            const char *dir_path = "~/stext";

            // Create the tar file name
            snprintf(tar_name, sizeof(tar_name), "%s/stext/textfiles.tar", getenv("HOME"));
            printf("%s \n", tar_name);

            // Create the tar file and send it to the client if successful
            if (create_tar(tar_name, dir_path, filetype) == 0) {
                send_file_to_smain(client_socket, tar_name);
            } else {
                // Send an error message to the client if the tar file creation fails
                send(client_socket, "Failed to create tar file", 25, 0);
            }
}





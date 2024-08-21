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
#include <netdb.h>
#include <errno.h>
#include <dirent.h>  // Include this header for directory handling
#include <ftw.h>  // Include this header for file tree walks

#define PORT 9015// Port number for the server
#define BUFFER_SIZE 1024// Size of the buffer for data transmission

//function prototypes
void upload_file(int client_socket, const char *filename, const char *destination_path);
void download_file(int client_socket, const char *file_path);
const char* extract_filename(const char* file_path);
void remove_file(int client_socket, const char *file_path);
void download_tar_file(int client_socket, const char *filetype);
int create_client_socket();

int main() 
{
    int client_socket;
    struct sockaddr_in server_addr; // Structure to hold server address


    // Creating socket
    // Create a client socket
    client_socket = create_client_socket();
    if (client_socket < 0) {
        exit(EXIT_FAILURE);  // Exit if socket creation fails
    }


    server_addr.sin_family = AF_INET;  // Address family
    server_addr.sin_port = htons(PORT); // Server port
    server_addr.sin_addr.s_addr = INADDR_ANY; // Server IP address

    // Connecting to the server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("Connection failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

     char command[BUFFER_SIZE];               // Buffer to store user command
    printf("Enter command: ");
    fgets(command, BUFFER_SIZE, stdin);      // Read command from the user
    command[strcspn(command, "\n")] = 0;     // Remove newline character

   

    if (strncmp(command, "ufile", 5) == 0) // handling Transfers (uploads) filename from the PWD of the client to smain 
    {
        char *filename = strtok(command + 6, " "); 
         // strtok() is used to split a string into tokens based on a set of delimiters.
         // This specifies a space as the delimiter. This means that strtok will split the string at each space.

        char *destination_path = strtok(NULL, " "); // to get destination path from command
        if (filename && destination_path) // if both are not null then we execute upload file function will be called
         {
            upload_file(client_socket, filename, destination_path);
        } else {
            fprintf(stderr, "Invalid command format. Usage: ufile <filename> <destination_path>\n");  // invalid format
        }
    }
    else if (strncmp(command, "display", 7) == 0) //handling Transfers (downloads) filename from Smain to the PWD of the client 
    {
         // Send the command to the server
    send(client_socket, command, strlen(command), 0);

    // Receive and print the response from the server
    printf("Files in the directory:\n");
        char buffer[BUFFER_SIZE]; // to store buffer
        // continously receiving data from server
    while (1) 
    {
        // Receive data from the client socket
int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

// Check if the number of bytes received is less than or equal to 0
// If true, break the loop as it indicates an error or the connection has been closed
if (bytes_received <= 0) 
                    break;

// Null-terminate the received data to ensure it is a proper C string
buffer[bytes_received] = '\0';

// Print the received data to the console
printf("%s", buffer);

    }
    }
    else if (strncmp(command, "dfile", 5) == 0) //Transfers (downloads) filename from Smain to the PWD of the client 
    {
        char *file_path = strtok(command + 6, " ");
          // strtok() is used to split a string into tokens based on a set of delimiters.
         // This specifies a space as the delimiter. This means that strtok will split the string at each space.

        if (file_path)  // if filepath not null then we execute download file function will be called
        {
            download_file(client_socket, file_path);
        } else {
            fprintf(stderr, "Invalid command format. Usage: dfile <file_path_on_server>\n"); // invalid format
        }
    }
     else if (strncmp(command, "rmfile", 6) == 0) // Removes (deletes) filename from Smain to the PWD of the client 

     {
        char *file_path = strtok(command + 7, " ");
           // strtok() is used to split a string into tokens based on a set of delimiters.
         // This specifies a space as the delimiter. This means that strtok will split the string at each space.

        if (file_path)  // if filepath not null then we execute remove file function will be called
        {
            remove_file(client_socket, file_path);
        } else {
            fprintf(stderr, "Invalid command format. Usage: dfile <file_path_on_server>\n"); // invalid format
        }
    }
   // Creates a tar file of the specified file type and transfers (downloads) the tar file from
   //Smain to the PWD of the client 
   else if (strncmp(command, "dtar", 4) == 0) 
   {
        char *filetype = command + 5; // getting file types
        download_tar_file(client_socket, filetype); // calling this methods
    }
     else {
        fprintf(stderr, "Unknown command\n");  // unknown command
    }

    close(client_socket); // close the connection
    return 0;
}

// Function to create a client socket
int create_client_socket() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);  // Create a socket
    if (socket_fd < 0) {
        perror("Socket creation failed");
        return -1;  // Return -1 if socket creation fails
    }
    return socket_fd;  // Return the socket descriptor
}

void upload_file(int client_socket, const char *filename, const char *destination_path) 
{
    // Open the file for reading in binary mode
    FILE *file = fopen(filename, "rb");
    if (!file) {
        // If the file cannot be opened, print an error message and return
        perror("File open failed");
        return;
    }

    // Move the file pointer to the end of the file to determine the file size
    fseek(file, 0, SEEK_END);
    // Get the current file pointer position, which is the file size
    long file_size = ftell(file);
    // Move the file pointer back to the beginning of the file
    fseek(file, 0, SEEK_SET);

    // Allocate memory to hold the file content plus a null terminator
    char *file_content = malloc(file_size + 1);
    // Read the file content into the allocated memory
    fread(file_content, 1, file_size, file);
    // Null-terminate the file content
    file_content[file_size] = '\0';

    // Close the file as it is no longer needed
    fclose(file);

    // Prepare the command to be sent to the server
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "UPLOAD %s %s %s", filename, destination_path, file_content);
    // Send the command to the server through the socket
    send(client_socket, command, strlen(command), 0);

    // Buffer to receive the server's response
    char response[BUFFER_SIZE];
    // Receive the response from the server
    int bytes_received = recv(client_socket, response, BUFFER_SIZE, 0);
    if (bytes_received > 0) {
        // Null-terminate the response and print it
        response[bytes_received] = '\0';
        printf("Server response: %s\n", response);
    }

    // Free the allocated memory for the file content
    free(file_content);
}


void download_file(int client_socket, const char *file_path) {
    // Buffer to hold the command to be sent to the server
    char command[BUFFER_SIZE];
    // Format the command string with the file path
    snprintf(command, sizeof(command), "DFILE %s", file_path);
    // Send the command to the server
    send(client_socket, command, strlen(command), 0);

    // Buffer to receive data from the server
    char buffer[BUFFER_SIZE];
    int bytes_received;
    // Print a message indicating the start of the download process
    printf("download file ...\n");

    // Extract the filename from the file path
    const char* filename = extract_filename(file_path);
    // Receive the file data in a loop
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) 
    {
        // Open the file for writing, creating it if it doesn't exist, with the specified permissions
        int fd = open(filename, O_WRONLY | O_CREAT, 0644);
        // Write the received data to the standard output (optional)
      //  write(STDOUT_FILENO, buffer, bytes_received);
        // Write the received data to the file
        write(fd, buffer, bytes_received);
        // Print a message indicating data has been received
        printf("received");
        fflush(stdout);
    }

    // Check for receive errors
    if (bytes_received < 0) {
        // Print an error message if receiving data failed
        perror("Receive failed");
    }
}


// Define the function
const char* extract_filename(const char* file_path) {
    const char *slash = strrchr(file_path, '/');
    if (slash) {
        return slash + 1; // Return the character after the last '/'
    } else {
        return file_path; // Return the original path if no '/' is found
    }
}

void remove_file(int client_socket, const char *file_path) {
    // Buffer to hold the command to be sent to the server
    char command[BUFFER_SIZE];
    // Format the command string with the file path
    snprintf(command, sizeof(command), "rmfile %s", file_path);
    // Send the command to the server
    send(client_socket, command, strlen(command), 0);

    // Buffer to receive data from the server
    char buffer[BUFFER_SIZE];
    int bytes_received;
    // Print a message indicating the start of the file deletion process
    printf("Deleting file...\n");

    // Receive the server's response in a loop
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        // Write the received data to the standard output
        write(STDOUT_FILENO, buffer, bytes_received);
    }

    // Check for receive errors
    if (bytes_received < 0) {
        // Print an error message if receiving data failed
        perror("Receive failed");
    }
}

void download_tar_file(int client_socket, const char *filetype) {
    // Buffer to hold the command to be sent to the server
    char command[BUFFER_SIZE];
    // Format the command string with the file type
    snprintf(command, sizeof(command), "dtar %s", filetype);
    // Send the command to the server
    send(client_socket, command, strlen(command), 0);

    // Buffer to receive data from the server
    char file_buffer[BUFFER_SIZE];
    int bytes_received;
    // Open the file for writing in binary mode
    FILE *tar_file = fopen("downloaded_tarfile.tar", "wb");
    if (!tar_file) {
        // If the file cannot be opened, print an error message and return
        perror("Failed to open file for writing");
        return;
    }

    // Receive the tar file data in a loop
    while ((bytes_received = recv(client_socket, file_buffer, BUFFER_SIZE, 0)) > 0) {
        // Write the received data to the file
        fwrite(file_buffer, 1, bytes_received, tar_file);
    }

    // Close the file after writing is complete
    fclose(tar_file);
    // Print a message indicating successful download
    printf("Tar file downloaded successfully\n");
}






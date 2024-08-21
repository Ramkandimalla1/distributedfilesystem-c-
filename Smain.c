#include <ftw.h>  // Include this header for file tree walks
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


#define PORT 9015
#define BUFFER_SIZE 1024

// function prototypes
void handle_client(int client_socket);
void create_directories(const char *path);
void list_files(const char *path, const char *extension, char *result);
void send_file(int client_socket, const char *file_path) ;
void forward_to_server(const char *hostname, int port, const char *command, int client_socket);
int remove_file(const char *file_path) ;
void send_file_to_client(int client_socket, const char *file_path);
int initialize_socket();
int bind_socket(int server_socket);
void listen_for_connections(int server_socket);
int create_tar(const char *tar_name, const char *dir_path, const char *filetype);
void handle_new_connection(int server_socket, int client_socket);
void handle_request(int client_socket);
int accept_connection(int server_socket, struct sockaddr_in *client_addr);
void list_files_recursive(const char *dir_path, const char *file_extension, char *response);


int main() 
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;//Structure to hold server address,client addresss
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

    while (1) {
         client_socket = accept_connection(server_socket, &client_addr);
        if (client_socket < 0) {
            continue; // Continue trying to accept connections
        }
        // Fork a new process for each client  
        handle_new_connection(server_socket, client_socket);
        
    }

    close(server_socket);
    // Close the server socket to release the resource
    return 0;
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

// Creates a socket and returns its descriptor
int initialize_socket() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    // Here, 'socket' is a function that creates a socket and returns a socket descriptor
// 'AF_INET' specifies the IPv4 address family
// 'SOCK_STREAM' denotes the socket type as stream, typically used for TCP
// '0' specifies that the default protocol for the type should be used, here TCP

// Error handling for socket creation
 if (server_socket < 0) {
        perror("Socket creation failed");
        return -1;
    }
    return server_socket;
}

// Listens for incoming connections on the bound socket
void listen_for_connections(int server_socket) {
    if (listen(server_socket, 10) < 0) {
        perror("Socket listening failed");
        exit(EXIT_FAILURE);
    }
    printf("Server is listening on port %d\n", PORT);
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
         //EXIT_FAILURE is a macro that represents a failure termination status for exit(), indicating that the program did not succeed.
    
    }
}

void handle_request(int client_socket)
 {
    char buffer[BUFFER_SIZE];
    // to store data from client
    int bytes_read;
// recieve continuosly data from client
    while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) 
    {
        buffer[bytes_read] = '\0';
        char command[BUFFER_SIZE];
        strcpy(command, buffer);
       printf("Received cmd : %s\n", command);
      
       // compare the command with options 
       
        if (strncmp(command, "UPLOAD", 6) == 0) 
        {
            char *filename = strtok(command + 7, " "); // get filename
            char *destination_path = strtok(NULL, " "); // get dp
           char *file_content = strtok(NULL, "");
           char *file_content_copy = malloc(strlen(file_content) + 1);
           strcpy(file_content_copy, file_content); // copy file content
            printf("Received filename: %s\n", filename);
            printf("Received destination_path: %s\n", destination_path);
            printf("Received file_content: %s\n", file_content);

            
            char *file_extension = strrchr(filename, '.'); // get file extension

            if (file_extension && strcmp(file_extension, ".c") == 0) // if file extension is .c
            {
                char full_path[BUFFER_SIZE];// buffer to store filepath

                snprintf(full_path, sizeof(full_path), "%s/%s", getenv("HOME"), destination_path + 1);
                // Format the full path by combining the HOME directory path and the destination path,
// skipping the first character of destination_path (assuming it is a leading slash),
// and store the result in the full_path buffer.

                strcat(full_path, "/");
                strcat(full_path, filename); // Append the filename to the path
                // Ensure all subdirectories are created before the file
                char *last_slash = strrchr(full_path, '/');
                if (last_slash) {
                    *last_slash = '\0'; // Terminate the string at the last slash to get the directory path
                    create_directories(full_path); // Create necessary directories
                    *last_slash = '/'; // Restore the slash
                }
                printf("Creating file at: %s\n", full_path);
                int fd = open(full_path, O_WRONLY | O_CREAT, 0644); // crete file
                if (fd < 0) {
                    perror("File open failed");
                    printf("Error: %s\n", strerror(errno));
                    send(client_socket, "Error saving file", 17, 0); // send to client issue in saving
                } else {
                    write(fd, file_content, strlen(file_content)); // write data to file
                    close(fd); // close file descripter
                    send(client_socket, "File uploaded to Smain", 22, 0); // send to client file uploaded
                }
            }
             else if (file_extension && strcmp(file_extension, ".pdf") == 0) 
            {
                char full_path[BUFFER_SIZE];
                
                snprintf(full_path, sizeof(full_path), "%s/spdf%s", getenv("HOME"), destination_path + 6);
                // Format the full path by combining the HOME directory path, "spdf" subdirectory, and the destination path,
// skipping the first 6 characters of destination_path, and store the result in the full_path buffer.


                strcat(full_path, "/");
                strcat(full_path, filename); // Append the filename to the path
// Ensure all subdirectories are created before the file
                char *last_slash = strrchr(full_path, '/');
                if (last_slash) {
                    *last_slash = '\0'; // Terminate the string at the last slash to get the directory path
                    create_directories(full_path); // Create necessary directories
                    *last_slash = '/'; // Restore the slash
                }

                  printf("Creating file at: %s\n", full_path);
               
                printf("Received file_content: %s\n", file_content);

                snprintf(command, sizeof(command), "UPLOAD %s %s %s", filename, full_path, file_content_copy);
                  printf("%s \n ",command);
               // forward_to_server("localhost", 9009, command);
                forward_to_server("localhost", 9009, command, client_socket);

                send(client_socket, "File uploaded to Spdf", 21, 0);
            } 
            else if (file_extension && strcmp(file_extension, ".txt") == 0)
             {
                char full_path[BUFFER_SIZE];
                // snprintf(full_path, sizeof(full_path), "%s/%s", getenv("HOME"), "stext");
                // strncat(full_path, destination_path + 6, BUFFER_SIZE - strlen(full_path) - 1);
                snprintf(full_path, sizeof(full_path), "%s/stext%s", getenv("HOME"), destination_path + 6);
                // Format the full path by combining the HOME directory path, "stext" subdirectory, and the destination path,
// skipping the first 6 characters of destination_path, and store the result in the full_path buffer.
                strcat(full_path, "/");
                strcat(full_path, filename); // Append the filename to the path
                
// Ensure all subdirectories are created before the file
                char *last_slash = strrchr(full_path, '/');
                if (last_slash) {
                    *last_slash = '\0'; // Terminate the string at the last slash to get the directory path
                    create_directories(full_path); // Create necessary directories
                    *last_slash = '/'; // Restore the slash
                }
                printf("Creating file at: %s\n", full_path);
               
                printf("Received file_content: %s\n", file_content);

                snprintf(command, sizeof(command), "UPLOAD %s %s %s", filename, full_path, file_content_copy);
                printf("%s",command);
                //forward_to_server("localhost", 9008, command);
                forward_to_server("localhost", 9008, command, client_socket);

                send(client_socket, "File uploaded to Stext", 22, 0);
            } else {
                send(client_socket, "Unsupported file type", 21, 0);
            }
        }
        else  if (strncmp(command, "display", 7) == 0)
             
        {
    //         printf("in display\n");
    // char *pathname = command + 8;  // Get the pathname after "DISPLAY "

    // // Response buffer to store the results
    // char response[BUFFER_SIZE] = "";

    // // Adjust pathname to skip the leading "~smain" if present
    // if (strncmp(pathname, "~/smain", 7) == 0) {
    //     pathname += 7;  // Skip the leading "~smain"
    // }

    // // Constructing the full path for local .c files
    // char full_path[BUFFER_SIZE];
    // snprintf(full_path, sizeof(full_path), "%s/smain%s", getenv("HOME"), pathname);
    // printf("Full path for .c files: %s\n", full_path);
    // strcat(response, "Local .c files:\n");
    // list_files(full_path, ".c", response);

    // // Constructing the full path for .pdf files
    // char pdf_path[BUFFER_SIZE];
    // snprintf(pdf_path, sizeof(pdf_path), "%s/spdf%s", getenv("HOME"), pathname);
    // printf("Full path for .pdf files: %s\n", pdf_path);
    // strcat(response, "\n.pdf files:\n");
    // list_files(pdf_path, ".pdf", response);

    // // Constructing the full path for .txt files
    // char txt_path[BUFFER_SIZE];
    // snprintf(txt_path, sizeof(txt_path), "%s/stext%s", getenv("HOME"), pathname);
    // printf("Full path for .txt files: %s\n", txt_path);
    // strcat(response, "\n.txt files:\n");
    // list_files(txt_path, ".txt", response);

    // // Displaying the full response on the server console for debugging
    // printf("%s", response);


   printf("in display\n");
    char *pathname = command + 8;  // Skip the "DISPLAY " part to get the pathname

    char response[BUFFER_SIZE] = "";  // Buffer to hold the response to be sent to the client

    // Remove the leading "~smain" from pathname if present
    if (strncmp(pathname, "~/smain", 7) == 0) {
        pathname += 7;  // Adjust to point after "~smain"
    }

    // Concatenate paths for .c, .pdf, and .txt files
    char full_path[BUFFER_SIZE], pdf_path[BUFFER_SIZE], txt_path[BUFFER_SIZE];
    
    // Handling environment variable retrieval securely
    const char *home_dir = getenv("HOME");
    if (!home_dir) {
        fprintf(stderr, "Environment variable HOME not set.\n");
        return;
    }

    // Prepare paths
    snprintf(full_path, sizeof(full_path), "%s/smain%s", home_dir, pathname);
    snprintf(pdf_path, sizeof(pdf_path), "%s/spdf%s", home_dir, pathname);
    snprintf(txt_path, sizeof(txt_path), "%s/stext%s", home_dir, pathname);

    // Log paths for debugging
    printf("Full path for .c files: %s\n", full_path);
    printf("Full path for .pdf files: %s\n", pdf_path);
    printf("Full path for .txt files: %s\n", txt_path);

    // Append local .c files
    strcat(response, "Local .c files:\n");
    list_files_recursive(full_path, ".c", response);
     // Append .pdf files from Spdf
    strcat(response, "\n.pdf files:\n");
    list_files_recursive(pdf_path, ".pdf", response);

    // Append .txt files from Stext
    strcat(response, "\n.txt files:\n");
   list_files_recursive(txt_path, ".txt", response);

    // Display the full response on the server console for debugging
    printf("%s", response);

    // Send the response to the client
    send(client_socket, response, strlen(response), 0);
        }
    else 
     if(strncmp(command, "DFILE", 5) == 0)
         {
                 printf("Handling DFILE command\n");
    fflush(stdout);

    // Extract filename from the command
    char *filename = buffer + 6;  // Assuming 'dfile ' is 6 characters

    // Remove the leading "~smain" if present
    if (strncmp(filename, "~smain", 6) == 0) {
        filename += 6; // Skip the '~smain'
    }

    // Finding the last occurrence of '.' to ensure it is the extension
    char *file_extension = strrchr(filename, '.');
    if (file_extension && strlen(file_extension) > 1) {
        file_extension += 1;  // Skip the dot to get to the extension
    } else {
        printf("Invalid file extension.\n");
        return;  // No valid extension found, handle error appropriately
    }

    // Construct the full path based on file type
    char full_path[BUFFER_SIZE];
    const char *home_dir = getenv("HOME");
    if (!home_dir) {
        printf("Environment variable HOME not set.\n");
        return;
    }

    if (strcmp(file_extension, "c") == 0) {
        snprintf(full_path, sizeof(full_path), "%s/smain%s", home_dir, filename);
        printf("C File path: %s\n", full_path);
        send_file(client_socket, full_path);
    } else {
        const char *server_dir = (strcmp(file_extension, "pdf") == 0) ? "spdf" : "stext";
        int port = (strcmp(file_extension, "pdf") == 0) ? 9009 : 9008;

        // Ensure that filename is properly adjusted if not already included in the directory path
        snprintf(full_path, sizeof(full_path), "%s/%s%s", home_dir, server_dir, filename);
        printf("Other File path: %s\n", full_path);

        char command[BUFFER_SIZE];
        snprintf(command, sizeof(command), "DFILE %s", full_path);
        printf("Forwarding command: %s\n", command);

        forward_to_server("localhost", port, command, client_socket);
    }
         } 
          else if(strncmp(command, "rmfile", 6) == 0)
         {
             // Handle rmfile command
            char *filename = buffer + 7;  // Extract path from command
            char *file_extension = strrchr(filename, '.') + 1;
             // Remove the leading "~smain" from pathname if present
            if (strncmp(filename, "~smain", 6) == 0) {
                filename += 6;
            }

            char full_path[BUFFER_SIZE];
           // snprintf(full_path, BUFFER_SIZE, "%s/%s", getenv("HOME"), filename);
               snprintf(full_path, sizeof(full_path), "%s/smain%s", getenv("HOME"), filename);
            printf("%s \n ",full_path);
            if (strcmp(file_extension, "c") == 0) {
                int flag=remove_file(full_path); // Directly handle .c files
                if(flag==1){
                send(client_socket, "File removed \n ", 13, 0);
                }
                else
                {
                    send(client_socket, "File not removed \n ", 17, 0);
                }
            } else {
                // Adjust file path for .pdf and .txt and forward request
                const char *server_dir = strcmp(file_extension, "pdf") == 0 ? "spdf" : "stext";
            
                snprintf(full_path, sizeof(full_path), "%s/%s%s", getenv("HOME"), server_dir, filename ); // Adjust path
                printf("%s \n",full_path);
                  int flag =remove_file(full_path); // Directly handle .pdf files,.txt files
              if(flag==1)
              {
                send(client_socket, "File removed \n", 13, 0);
                
                }
                else
                {
                    send(client_socket, "File not removed \n", 17, 0);
                }
           
           
            }
            
         } 
         else if (strncmp(command, "dtar", 4) == 0) 
           {
                // Extract the filetype from the command
                char *filetype = command + 5;  // Get the filetype after "dtar "
                // Buffer to hold the tar file name
                char tar_name[BUFFER_SIZE];

                // Check if the filetype is ".c"
                if (strcmp(filetype, ".c") == 0) {
                    // Format the tar file name for C files and store it in tar_name
                    snprintf(tar_name, sizeof(tar_name), "%s/smain/cfiles.tar", getenv("HOME"));
                    // Create the tar file with the specified filetype in the specified directory
                    create_tar(tar_name, "~/smain", ".c");
                    // Send the created tar file to the client
                    send_file_to_client(client_socket, tar_name);
                }
                // Check if the filetype is ".pdf"
                else if (strcmp(filetype, ".pdf") == 0) {
                    // Format the tar file name for PDF files and store it in tar_name
                    snprintf(tar_name, sizeof(tar_name), "%s/spdf/pdf.tar", getenv("HOME"));
                    // Forward the request to the server at localhost on port 9009
                    forward_to_server("localhost", 9009, "dtar .pdf", client_socket);
                }
                // Check if the filetype is ".txt"
                else if (strcmp(filetype, ".txt") == 0) {
                    // Format the tar file name for text files and store it in tar_name
                    snprintf(tar_name, sizeof(tar_name), "%s/stext/text.tar", getenv("HOME"));
                    // Forward the request to the server at localhost on port 9008
                    forward_to_server("localhost", 9008, "dtar .txt", client_socket);
                }
                // If the filetype is unsupported
                else {
                    // Send an unsupported file type message to the client
                    send(client_socket, "Unsupported file type", 21, 0);
                }
      }



         
    }

    close(client_socket);   // Close the server socket to release the resource
   
}
// Function to create a tar file of the specified file type
int create_tar(const char *tar_name, const char *dir_path, const char *filetype) {
    // Buffer to hold the command to be executed
    char command[BUFFER_SIZE];
    // Format the command string to find files with the specified filetype and create a tar archive
    snprintf(command, sizeof(command), "find %s -name '*%s' | xargs tar -cvf %s", dir_path, filetype, tar_name);
    // Print the command for debugging purposes
    printf("%s", command);
    // Flush the output buffer to ensure the command is printed immediately
    fflush(stdout);
    // Execute the command using the system function and store the result
    int result = system(command);
    // Print the result of the command execution
    printf("%d", result);
    // Flush the output buffer to ensure the result is printed immediately
    fflush(stdout);
    // Return the result of the command execution
    return result;
}

// Function to send a file to the client
void send_file_to_client(int client_socket, const char *file_path) 
{
    // Open the file for reading
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        // Print an error message if the file cannot be opened
        perror("Failed to open file for reading");
        // Send an error message to the client
        send(client_socket, "Failed to fetch file", 20, 0);
        return;
    }

    // Buffer to hold file data
    char file_buffer[BUFFER_SIZE];
    int read_bytes;
    // Read the file data in a loop
    while ((read_bytes = read(fd, file_buffer, BUFFER_SIZE)) > 0) 
    {
        // Send the read data to the client
        send(client_socket, file_buffer, read_bytes, 0);
    }
    
    // Print a message indicating that the file is being sent to the client
    printf("sending tar .c files to client");
    
    // Close the file after sending the data
    close(fd);
}

void forward_to_server(const char *hostname, int port, const char *command, int client_socket) {
    int server_socket;
    struct sockaddr_in server_addr;
    struct hostent *server;

    // Create a socket for communication with the server
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        // Print an error message if socket creation fails
        perror("Socket creation failed");
        return;
    }

    // Get the server's address information
    server = gethostbyname(hostname);
    if (server == NULL) {
        // Print an error message if the host cannot be found
        fprintf(stderr, "No such host\n");
        // Close the socket to release the resource
        close(server_socket);
        return;
    }

    // Set the family of the address to AF_INET (IPv4)
    server_addr.sin_family = AF_INET;
    // Set the port number, converting it to network byte order
    server_addr.sin_port = htons(port);
    // Copy the server's IP address to the server address structure
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    // Connect to the server
    if (connect(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        // Print an error message if the connection fails
        perror("Connection to server failed");
        // Close the socket to release the resource
        close(server_socket);
        return;
    }

    // Send the command to the server

    send(server_socket, command, strlen(command), 0);

    // Buffer to hold data received from the server
    char buffer[BUFFER_SIZE];
    int bytes_read;
    // Read data from the server in a loop
    while ((bytes_read = recv(server_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        // Forward the data received from the server to the client
        send(client_socket, buffer, bytes_read, 0);
    }

    // Print a message indicating that data is being sent to the client
    printf("sending file to client\n ");
    fflush(stdout);

    // Close the server socket to release the resource
    close(server_socket);
}



void create_directories(const char *path) 
{
    // Temporary buffer to hold the path
    char tmp[BUFFER_SIZE];
    char *p = NULL;
    size_t len;

    // Copy the input path to the temporary buffer
    snprintf(tmp, sizeof(tmp), "%s", path);
    
    // Get the length of the path
    len = strlen(tmp);
    // Remove trailing slash if present
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    // Iterate over the path and create directories one by one
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0; // Temporarily terminate the string to create the directory
            printf("Creating directory: %s\n", tmp);
            mkdir(tmp, S_IRWXU); // Create the directory with read/write/execute permissions for the user
            *p = '/'; // Restore the slash
        }
    }
    
    // Create the final directory
    printf("Creating directory: %s\n", tmp);
    mkdir(tmp, S_IRWXU);
}

void send_file(int client_socket, const char *file) 
{
     printf("File path: %s\n", file);
      fflush(stdout);
    int fd = open(file, O_RDONLY);// open file in read mode
    if (fd < 0) // open fails we print below
    {
        perror("Failed to open file for reading");
        send(client_socket, "Failed to fetch file", 20, 0); // send to client failed to find file
        return;
    }
 // Buffer to hold received data
    char file_buffer[BUFFER_SIZE];
    int bytes_read;
    // Read data from the buffer
    while ((bytes_read = read(fd, file_buffer, BUFFER_SIZE)) > 0) 
    {
        send(client_socket, file_buffer, bytes_read, 0);
        // send data to client
    }
    close(fd); // close file descripter
}

// Function to remove a file at the given path
int remove_file(const char *file_path) 
{
    if (remove(file_path) == 0) // remove the file in the filename
    {
        printf("\n File deleted successfully\n");
        return 1;
    } 
    else  // if remove does not return 0 then it prints below
    {
        perror("Failed to delete the file");
         return 0;
    }
}
void list_files_recursive(const char *dir_path, const char *file_extension, char *response) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("Failed to open directory");
        return;
    }

    struct dirent *entry;
    char path[BUFFER_SIZE];
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip the special directory entries "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(path, BUFFER_SIZE, "%s/%s", dir_path, entry->d_name);

        // Check if the entry is a directory
        struct stat entry_stat;
        if (stat(path, &entry_stat) == -1) {
            perror("Failed to get file status");
            continue;
        }

        if (S_ISDIR(entry_stat.st_mode)) {
            // If it's a directory, recurse into it
            list_files_recursive(path, file_extension, response);
        } else if (S_ISREG(entry_stat.st_mode)) {
            // If it's a regular file, check the extension
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && !strcmp(ext, file_extension)) {
                strcat(response, entry->d_name);
                strcat(response, "\n");
            }
        }
    }

    closedir(dir);
}


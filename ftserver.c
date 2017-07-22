/*
Author: Joshua Kluthe
Date: 2017.03.10
Description: ftserver acts as a file server. It listens for a connection on a socket, then
receives a command asking for the directory contents or for a file. It then attempts to connect
to the client on another socket set up by the client to send this information through.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/sendfile.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/stat.h>

int send_data(char *data, int socket_fd);

int send_file_contents(char *filename, int socket_fd);

int make_server(int port);

int listen_for_client(int server_fd);

int connect_for_data();

int control_connection(int conn_fd);

int parse_command(char *command, char *filename);

//requires a port number specified on the command line
int main(int argc, char *argv[]) {
    
    int port, server_fd, connection_fd;
    
    //validate the command line arg
    if(argc < 2) {
        fprintf(stderr, "SERVER: Not enough parameters. Usage 'ftserver [PORT]'\n");
        exit(1);
    }
    if((port = atoi(argv[1])) <= 0) {
        fprintf(stderr, "SERVER: Port must be an integer greater than zero\n");
        exit(1);
    }
    
    //create and initialize server socket
    server_fd = make_server(port);
    
    //listen for connections until interrupt
    listen(server_fd, 5);
    printf("Server open on port %d\n", port);
    while(1) {
        if((connection_fd = listen_for_client(server_fd)) < 0) {
            fprintf(stderr, "SERVER: A client attempted to connect and failed\n");
            continue;
        }
        
        //exchange info and set up data connection through the control_connection
        control_connection(connection_fd);
    
        close(connection_fd);
    }
    
    close(server_fd);
    
    return 0;
}

/*
send_data() acts as a wrapper for the send() function. It sends an additional header containing
the length of the string to be sent to the socket, and then loops until all data is sent.
param char *data: the string to be sent
param int socket_fd: the socket to send the data through
return 1 on success, 0 on failure
*/
int send_data(char *data, int socket_fd) {
    
    char ack[4];
    char num_bytes[11];
    int length = strlen(data);
    int sent = 0;
    
    sprintf(num_bytes, "%d", length);
    
    //send header with the data size, wait for ACK
    send(socket_fd, num_bytes, strlen(num_bytes), 0);
    recv(socket_fd, ack, 4, 0);
    if(strcmp(ack, "ACK") != 0)
        return 0;
    
    while(sent < length) {
        sent += send(socket_fd, data, length, 0);
    }
    return 1;    
}

/*
send_file_contents() reads the contents of a file into a string and then sends it using send_data().
If the file doesn't exist, it sends an error message through the socket.
param char *filename: name of file to be sent.
param int socket_fd: socket to send data through.
return 1 on success, 0 on failure.

Used info from the following link to write file reading code
http://stackoverflow.com/questions/7856741/how-can-i-load-a-whole-file-into-a-string-in-c
*/
int send_file_contents(char *filename, int socket_fd) {
    
    char *contents;
    int chars_read;

    int file_size;
    FILE *fp = fopen(filename, "r");
    
    //verify that file opened
    if(!fp) {
        printf("File not found or inaccessible. Sending error message.\n");
        send(socket_fd, "FILE NOT FOUND", 14, 0);
        return 0;
    }
    
    //determine the size of the file
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);
    
    //allocate a string to hold the data
    contents = malloc(file_size * sizeof(char));
    
    //read file into string, seeking back to beginning and rereading if necessary.
    while(chars_read < file_size) {
        fseek(fp, SEEK_SET, 0);
        chars_read = fread(contents, sizeof(char), file_size, fp);    
    }
    
    fclose(fp);
    
    //attempt to send the data
    if(!send_data(contents, socket_fd)) {
        fprintf(stderr, "SERVER: Error sending data.\n");
        return 0;
    }
    
    free(contents);
    contents = NULL;    
    
    return 1;
}

/*
send_directory_contents() loops through the entries in the current working directory and stores the
names in a string, then calls send_data() to send the string through the socket.
param int socket_fd: the socket to send the data through.
return 1 on success, 0 on failure.

Used info from the following link to write directory entry traversal code
http://stackoverflow.com/questions/612097/how-can-i-get-the-list-of-files-in-a-directory-using-c-or-c
*/
int send_directory_contents(int socket_fd) {
    
    DIR *dp;
    struct dirent *ep;
    char cwd[1024];
    //memory is cheap so I will go ahead and allocate what is sure to be enough to store all
    //the dirent names in a string
    char dir_list[65536];
    memset(dir_list, '\0', sizeof(dir_list));
    
    //get the name of the current working directory
    if(!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, "SERVER: Error. Failed to get current working directory.\n");
        return 0;
    }
    
    //open the directory
    dp = opendir(cwd);
    if(!dp) {
        fprintf(stderr, "SERVER: Error. Failed to open directory.\n");
        return 0;
    }
    
    //read through each dirent and add the name to a string
    while(ep = readdir(dp)) {
        strcat(dir_list, ep->d_name);
        strcat(dir_list, "\n");
    }
    
    closedir(dp);
    
    //replace the final trailing newline char with a null
    dir_list[strlen(dir_list) - 1] = '\0';
    
    //send the data
    if(!send_data(dir_list, socket_fd)) {
        fprintf(stderr, "SERVER: Error sending data.\n");
        return 0;
    }

    return 1;
} 


/*
connect_for_data() attempts to connect to ftclient, which should be listening for a connection.
Upon successful connection it returns the socket file descriptor for the connection.
Otherwise, it prints an error message and returns -1.
param char *host: hostname to connect to.
param char *port: port to connect to.
*/
int connect_for_data(char *host, char *port) {
    
    struct addrinfo *p, hints, *serverinfo;
    int sockfd;
    
    //iniialize address hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    //get address info for the data server
    if(getaddrinfo(host, port, &hints, &serverinfo) != 0) {
        fprintf(stderr, "SERVER: Failed to get address information for data connection\n");
        exit(1);
    }
    
    //loop through possible addresses in serverinfo
    for(p = serverinfo; p != NULL; p = p->ai_next) {
        //attempt to create socket
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }
        //socket created, attempt to connect
        if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        //here the socket and connection are good, so break loop
        break;
    }
    //loop exits with either a connection made, or p == NULL. Check p
    if(p == NULL){
        fprintf(stderr, "SERVER: Failed to connect for data connection\n");
        sockfd = -1;
    }

    freeaddrinfo(serverinfo);
    return sockfd;
}

/*
make_server() initializes and a server socket and binds it to the appropriate address.
Return value is the socket file descriptor.
On failure, make_server prints an error message and calls exit(1).
param port: port number to bind the server socket to.
*/
int make_server(int port) {
    
    int fd;
    struct sockaddr_in server_addr;
    
    //initialize server address members
    memset((char *) &server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    //attempt to create socket
    if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "SERVER: Failed to create socket\n");
        exit(1);
    }
    
    //attempt to bind socket to address
    if(bind(fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "SERVER: Failed to bind to socket\n");
        exit(1);
    }
    
    return fd;
}

/*
connect to a client on the listening socket
param int server_fd: file descriptor of the server socket
return file descriptor for the connection.
*/
int listen_for_client(int server_fd) {
    
    struct sockaddr_in client;
    socklen_t client_size = sizeof(client);
    return accept(server_fd, (struct sockaddr *) &client, &client_size);   
}

/*
control_connection() communicates with the client and receives and validates a command.
If the command is invalid control_connection sends back an error message. Otherwise, it sends "ACK".
*/
int control_connection(int conn_fd) {
    
    char filename[1024];
    char buffer[1024];
    char address[128];
    int i = 0;
    int data_fd;
    int command_type;
    
    //zero out the buffers
    //memset(filename, '\0', sizeof(filename));
    memset(buffer, '\0', sizeof(buffer));
    memset(address, '\0', sizeof(address));
    
    //recv command and check for validity. command_type = 1 for -l, 2 for -g, and 0 on fail
    recv(conn_fd, buffer, sizeof(buffer) - 1, 0);
    if(!(command_type = parse_command(buffer, filename))) {
        send(conn_fd, "INVALID COMMAND", 15, 0);
        return 0;
    }
    
    //command is valid, so send ACK and recv data connection address
    send(conn_fd, "ACK", 3, 0);
    recv(conn_fd, address, sizeof(address) - 1, 0);
    
    //step through the address and split into host and port
    while(address[i] != '|') {
        i++;
    }
    //split so that hostname starts at address, port starts at address + i + 1
    address[i] = '\0';
    
    printf("Connection from %s.\n", address); 
    
    //wait three seconds to make sure client listening for data connection, then connect
    wait(3000);
    if((data_fd = connect_for_data(address, address + i + 1)) < 0) {
        close(data_fd);
        return 0;
    }
    
    //check the command_type and call the appropriate function
    if(command_type == 2) {
        printf("File \"%s\" requested on port %s.\n", filename, address + i + 1);
        printf("Sending \"%s\" to %s:%s.\n", filename, address, address + i + 1);
        send_file_contents(filename, data_fd);
    //else send the directory contents
    } else {
        printf("List directory requested on port %s.\n", address + i + 1);
        printf("Sending directory contents to %s:%s.\n", address, address + i + 1);
        send_directory_contents(data_fd);
    }
    
    close(data_fd);
    
    return 1;
}

/*
parse_command() checks whether the parameter command is valid.
If command is in the form '-l', set parameter filename = NULL and return 1.
If command is in the form '-g <filename>' and filename is a valid file, set parameter filename = <filename>
and return 1.
Else, return 0.
*/
int parse_command(char *command, char *filename) {
    
    //printf("comm: %s, len: %d, comp: %d\n", command, strlen(command), strcmp(command, "-l"));
    
    if((strlen(command) == 2) && (strcmp(command, "-l") == 0)) {
        return 1;
    } else if((strlen(command) > 3) && (command[0] == '-') && (command[1] == 'g')) {
        strcpy(filename, &(command[3]));
        return 2;   
    }
    return 0;
}
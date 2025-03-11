#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr;
    char buff[4096] = {0};

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        return 1;
    }
    printf("SERVER_FD: %d\n", server_fd);

    int optval = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return 1;
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        return 1;
    }
    printf("LISTENING...\n");

    // Accept client connection
    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("Accept failed");
        close(server_fd);
        return 1;
    }

    // Read client request
    ssize_t bytes_read = read(client_fd, buff, sizeof(buff) - 1);
    if (bytes_read < 0) {
        perror("Read failed");
    } else {
        buff[bytes_read] = '\0';
        printf("Received Request:\n%s\n", buff);
    }

    // Extracting headers
    char *header_start = buff;
    char *headers_end = strstr(buff, "\r\n\r\n");
    if (headers_end) {
        *headers_end = '\0';  // Temporarily null-terminate headers for correct parsing
        headers_end += 4;      // Move past "\r\n\r\n"
    }
    char method[4],path[5];
    sscanf(buff,"%s %s",method,path);
    printf("method:%s path:%s",method,path);
    printf("Headers:\n");
    char *line = strtok(header_start, "\r\n");
    while (line) {
        printf("%s\n", line);
        line = strtok(NULL, "\r\n");
    }

    // Extracting body
    if (headers_end) {
        printf("MESSAGE FROM CLIENT: %s\n", headers_end);
    } else {
        printf("NO BODY FOUND\n");
    }

    // Response
    char response[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nRECEIVED\n";
    write(client_fd, response, sizeof(response) - 1);
    
    close(client_fd);
    close(server_fd);
    return 0;
}

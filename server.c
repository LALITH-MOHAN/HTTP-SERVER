#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#define PORT 8080
#define BUFFER_SIZE 8192
#define MAX_EVENTS 10
#define WEB_ROOT "/var/www"
#define UPLOAD_DIR "/var/www/uploads"

void handle_client(int client_fd);
void handle_get_request(int client_fd, char *path);
void handle_upload(int client_fd, char *request);
void send_response(int client_fd, int status_code, const char *status_msg, const char *content_type, const char *content, int content_length);
void send_404(int client_fd);
void send_file(int client_fd, const char *filepath);

int main() 
{
    int server_fd, client_fd, epoll_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct epoll_event ev, events[MAX_EVENTS];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) 
    {
        perror("Socket creation failed");
        exit(1);
    }

    int optval = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Bind failed");
        exit(1);
    }

    if (listen(server_fd, 10) == -1) 
    {
        perror("Listen failed");
        exit(1);
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) 
    {
        perror("Epoll creation failed");
        exit(1);
    }

    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

    printf("Server is listening on port %d....\n", PORT);

    while (1) 
    {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == server_fd) {
                client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                if (client_fd == -1) {
                    perror("Accept failed");
                    continue;
                }
                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
            } else {
                handle_client(events[i].data.fd);
                close(events[i].data.fd);
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
    return 0;
}

void handle_client(int client_fd) 
{
    char buffer[BUFFER_SIZE], method[10], path[256];

    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    printf("RECEIVED:\n%s\n\n",buffer);
    if (bytes_read <= 0) return;
    buffer[bytes_read] = '\0';

    sscanf(buffer, "%s %s", method, path);

    if (strcmp(method, "GET") == 0) {
        handle_get_request(client_fd, path);
        return;
    }

    if (strcmp(method, "POST") == 0 && strcmp(path, "/upload") == 0) {
        handle_upload(client_fd, buffer);
        return;
    }

    send_response(client_fd, 405, "Method Not Allowed", "text/plain", "405 Method Not Allowed", 22);
}

void handle_get_request(int client_fd, char *path) 
{
    char full_path[512];
    if (strcmp(path, "/") == 0) 
    {
        strcpy(path, "/index.html");
    }
    {
       snprintf(full_path, sizeof(full_path), "%s%s", WEB_ROOT, path);
    }

    if (access(full_path, R_OK) != 0) {
        send_404(client_fd);
        return;
    }
    send_file(client_fd, full_path);
}

void handle_upload(int client_fd, char *request) {
    char buffer[BUFFER_SIZE], filename[256], filepath[512];
    ssize_t bytes_read;
    FILE *uploaded_file;

    // Read the full request
    bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) return;
    buffer[bytes_read] = '\0';

    // Locate the filename
    char *filename_start = strstr(buffer, "filename=\"");
    if (!filename_start) {
        send_response(client_fd, 400, "Bad Request", "text/plain", "No filename found", 16);
        return;
    }
    filename_start += 10;  // Move past 'filename="'
    char *filename_end = strchr(filename_start, '"');
    if (!filename_end) return;
    *filename_end = '\0';

    // Create file path
    snprintf(filepath, sizeof(filepath), "%s/%s", UPLOAD_DIR, filename_start);
    mkdir(UPLOAD_DIR, 0755);

    // Locate the file content
    char *file_data = strstr(filename_end + 1, "\r\n\r\n");
    if (!file_data) {
        send_response(client_fd, 400, "Bad Request", "text/plain", "Invalid file format", 19);
        return;
    }
    file_data += 4; // Move past header separator

    // Find the ending boundary (it starts with "--")
    char *end_boundary = strstr(file_data, "\r\n--");
    if (!end_boundary) {
        send_response(client_fd, 400, "Bad Request", "text/plain", "Boundary not found", 18);
        return;
    }

    // Calculate content length properly
    ssize_t content_length = end_boundary - file_data;

    // Open file for writing
    uploaded_file = fopen(filepath, "wb");
    if (!uploaded_file) {
        send_response(client_fd, 500, "Internal Server Error", "text/plain", "File write error", 16);
        return;
    }

    // Write the extracted file content to disk
    fwrite(file_data, 1, content_length, uploaded_file);
    fclose(uploaded_file);

    // Send success response
    send_response(client_fd, 200, "OK", "text/plain", "File uploaded successfully", 25);
}



void send_404(int client_fd) 
{
    const char *msg = "<html><body><h1>404 Not Found</h1></body></html>";
    send_response(client_fd, 404, "Not Found", "text/html", msg, strlen(msg));
}

void send_file(int client_fd, const char *filepath) 
{
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        send_404(client_fd);
        return;
    }

    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    rewind(file);

    char *file_content = malloc(file_size);
    fread(file_content, 1, file_size, file);
    fclose(file);

    const char *ext = strrchr(filepath, '.'); 
    const char *content_type = "application/octet-stream";  // Default content type

if (ext) 
{
    if (strcmp(ext, ".html") == 0) content_type = "text/html";
    else if (strcmp(ext, ".htm") == 0) content_type = "text/html";
    else if (strcmp(ext, ".css") == 0) content_type = "text/css";
    else if (strcmp(ext, ".js") == 0) content_type = "application/javascript";
    else if (strcmp(ext, ".json") == 0) content_type = "application/json";
    else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) content_type = "image/jpeg";
    else if (strcmp(ext, ".png") == 0) content_type = "image/png";
    else if (strcmp(ext, ".gif") == 0) content_type = "image/gif";
    else if (strcmp(ext, ".pdf") == 0) content_type = "application/pdf";
    else if (strcmp(ext, ".txt") == 0) content_type = "text/plain";
}

// Send the response with the correct Content-Type
      send_response(client_fd, 200, "OK", content_type, file_content, file_size);

    free(file_content);
}

void send_response(int client_fd, int status_code, const char *status_msg, const char *content_type, const char *content, int content_length) 
{
    char response_header[BUFFER_SIZE];
    snprintf(response_header, sizeof(response_header),
             "HTTP/1.0 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n",
             status_code, status_msg, content_type, content_length);
    send(client_fd, response_header, strlen(response_header), 0);
    send(client_fd, content, content_length, 0);
}

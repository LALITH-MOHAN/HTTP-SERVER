#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 8192
#define WEB_ROOT "/var/www"
#define UPLOAD_DIR "/var/www/uploads"

void handle_client(int client_fd);
void handle_get_request(int client_fd, char *path);
void handle_upload(int client_fd, char *request);
void send_response(int client_fd, int status_code, const char *status_msg, const char *content_type, const char *content, int content_length);
void send_404(int client_fd);
void send_file(int client_fd, const char *filepath);

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    int optval = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,&optval, sizeof(optval));
    // Bind address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(1);
    }

    // Listen for connections
    if (listen(server_fd, 10) == -1) {
        perror("Listen failed");
        exit(1);
    }

    printf("Server is listening on port %d...\n", PORT);

    // Accept clients
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            perror("Accept failed");
            continue;
        }
        handle_client(client_fd);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE], method[10], path[256];

    // Read request
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) return;
    buffer[bytes_read] = '\0';

    printf("%s", buffer);

    sscanf(buffer, "%s %s", method, path);

    // Handle GET request
    if (strcmp(method, "GET") == 0) {
        handle_get_request(client_fd, path);
        return;
    }

    // Handle POST request for file upload
    if (strcmp(method, "POST") == 0 && strcmp(path, "/upload") == 0) {
        handle_upload(client_fd, buffer);
        return;
    }

    // Unsupported method
    send_response(client_fd, 405, "Method Not Allowed", "text/plain", "405 Method Not Allowed", 22);
}

void handle_get_request(int client_fd, char *path) {
    char full_path[512];

    // Default file if root is requested
    if (strcmp(path, "/") == 0) {
        strcpy(path, "/index.html");
    }

    // Construct full file path
    snprintf(full_path, sizeof(full_path), "%s%s", WEB_ROOT, path);

    // Check if file exists and is accessible
    if (access(full_path, R_OK) != 0) {
        send_404(client_fd);
        return;
    }

    // Serve file
    send_file(client_fd, full_path);
}

void handle_upload(int client_fd, char *request) {
    char buffer[BUFFER_SIZE];
    char *boundary, *filename_start, *file_data, *file_end;
    FILE *uploaded_file;
    char filepath[512];

    // Read full request (multiple chunks if necessary)
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) return;
    buffer[bytes_read] = '\0';

    // Find boundary
    boundary = strstr(buffer, "boundary=");
    if (!boundary) {
        send_response(client_fd, 400, "Bad Request", "text/plain", "Missing boundary", 15);
        return;
    }
    boundary += 9;  // Move past "boundary="

    // Locate filename
    filename_start = strstr(buffer, "filename=\"");
    if (!filename_start) {
        send_response(client_fd, 400, "Bad Request", "text/plain", "Missing filename", 15);
        return;
    }
    filename_start += 10;
    char *filename_end = strchr(filename_start, '\"');
    if (!filename_end) return;
    *filename_end = '\0';

    // Construct file path
    snprintf(filepath, sizeof(filepath), "%s/%s", UPLOAD_DIR, filename_start);

    // Ensure upload directory exists
    mkdir(UPLOAD_DIR, 0755);

    // Locate file data
    file_data = strstr(filename_end + 1, "\r\n\r\n");
    if (!file_data) {
        send_response(client_fd, 400, "Bad Request", "text/plain", "Invalid file data", 16);
        return;
    }
    file_data += 4;  // Skip "\r\n\r\n"

    // Find the end of the file content
    file_end = strstr(file_data, boundary);
    if (!file_end) {
        file_end = file_data + strlen(file_data);
    }
    size_t file_size = file_end - file_data - 4; // Exclude trailing "\r\n--"

    // Save the file
    uploaded_file = fopen(filepath, "wb");
    if (!uploaded_file) {
        send_response(client_fd, 500, "Internal Server Error", "text/plain", "Failed to save file", 20);
        return;
    }
    fwrite(file_data, 1, file_size, uploaded_file);
    fclose(uploaded_file);

    // Send success response
    send_response(client_fd, 200, "OK", "text/plain", "File uploaded successfully", 25);
}



void send_404(int client_fd) {
    const char *msg = "<html><body><h1>404 Not Found</h1></body></html>";
    send_response(client_fd, 404, "Not Found", "text/html", msg, strlen(msg));
}

void send_file(int client_fd, const char *filepath) {
    FILE *file = fopen(filepath, "rb"); // Open in binary mode for images/PDFs
    if (!file) {
        send_404(client_fd);
        return;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    rewind(file);

    // Read file into buffer
    char *file_content = malloc(file_size);
    fread(file_content, 1, file_size, file);
    fclose(file);

    // Detect content type
    const char *ext = strrchr(filepath, '.');
    const char *content_type = "application/octet-stream"; // Default

    if (ext) {
        if (strcmp(ext, ".html") == 0) content_type = "text/html";
        else if (strcmp(ext, ".txt") == 0) content_type = "text/plain";
        else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) content_type = "image/jpeg";
        else if (strcmp(ext, ".png") == 0) content_type = "image/png";
        else if (strcmp(ext, ".pdf") == 0) content_type = "application/pdf";
    }

    // Send response
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
             "\r\n",
             status_code, status_msg, content_type, content_length);

    send(client_fd, response_header, strlen(response_header), 0);
    send(client_fd, content, content_length, 0);
}

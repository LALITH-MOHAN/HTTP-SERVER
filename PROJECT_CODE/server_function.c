
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "server.h"

void handle_client(int client_fd) 
{
    char buffer[BUFFER_SIZE], method[10], path[256];

    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) return;
    buffer[bytes_read] = '\0';
    printf("RECEIVED REQ:\n %s\n", buffer);
    sscanf(buffer, "%s %s", method, path);

    if (strcmp(method, "GET") == 0) 
    {
        handle_get_request(client_fd, path);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/upload") == 0) 
    {
        handle_upload(client_fd);
    } else if (strcmp(method, "DELETE") == 0) 
    {
        handle_delete_request(client_fd, path);
    } else 
    {
        send_response(client_fd, 405, "Method Not Allowed", "text/plain", "405 Method Not Allowed", 22);
    }
}

void handle_get_request(int client_fd, char *path) 
{
    char full_path[512];
    if (strcmp(path, "/") == 0) strcpy(path, "/index.html");

    snprintf(full_path, sizeof(full_path), "%s%s", WEB_ROOT, path);
    if (access(full_path, R_OK) != 0) 
    {
        send_404(client_fd);
        return;
    }

    send_file(client_fd, full_path);
}

void handle_delete_request(int client_fd, char *path) 
{
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", WEB_ROOT, path);

    if (access(full_path, F_OK) != 0) 
    {
        send_response(client_fd, 404, "Not Found", "text/plain", "File not found", 14);
        return;
    }

    if (remove(full_path) == 0) 
    {
        send_response(client_fd, 200, "OK", "text/plain", "Deleted successfully", 20);
    } else 
    {
        send_response(client_fd, 500, "Internal Server Error", "text/plain", "Failed to delete file", 22);
    }
}

void handle_upload(int client_fd) 
{
    char buffer[BUFFER_SIZE], filename[256], filepath[512];
    ssize_t bytes_read;
    FILE *uploaded_file;
    char *file_data, *end_boundary;
    int header_parsed = 0;

    struct timeval tv = {1, 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

    while ((bytes_read = read(client_fd, buffer, sizeof(buffer))) > 0) {
        buffer[bytes_read] = '\0';

        if (!header_parsed) 
        {
            char *filename_start = strstr(buffer, "filename=\"");
            if (!filename_start) 
            {
                send_response(client_fd, 400, "Bad Request", "text/plain", "No filename found", 16);
                return;
            }
            filename_start += 10;
            char *filename_end = strchr(filename_start, '"');
            if (!filename_end) return;
            *filename_end = '\0';

            snprintf(filepath, sizeof(filepath), "%s/%s", UPLOAD_DIR, filename_start);
            uploaded_file = fopen(filepath, "wb");
            if (!uploaded_file) 
            {
                send_response(client_fd, 500, "Internal Server Error", "text/plain", "File write error", 16);
                return;
            }

            file_data = strstr(filename_end + 1, "\r\n\r\n");
            if (!file_data)
            {
                send_response(client_fd, 400, "Bad Request", "text/plain", "Invalid file format", 19);
                fclose(uploaded_file);
                return;
            }

            file_data += 4;
            end_boundary = strstr(file_data, "\r\n--");
            if (end_boundary) 
            {
                fwrite(file_data, 1, end_boundary - file_data, uploaded_file);
                fclose(uploaded_file);
                send_response(client_fd, 200, "OK", "text/plain", "File uploaded successfully", 25);
                return;
            }
            fwrite(file_data, 1, bytes_read - (file_data - buffer), uploaded_file);
            header_parsed = 1;
        } 
        else {
            end_boundary = strstr(buffer, "\r\n--");
            if (end_boundary) {
                fwrite(buffer, 1, end_boundary - buffer, uploaded_file);
                break;
            } else {
                fwrite(buffer, 1, bytes_read, uploaded_file);
            }
        }
    }

    fclose(uploaded_file);
    send_response(client_fd, 200, "OK", "text/plain", "File uploaded successfully", 25);
}

void send_404(int client_fd) {
    const char *msg = "<html><body><h1>404 Not Found</h1></body></html>";
    send_response(client_fd, 404, "Not Found", "text/html", msg, strlen(msg));
}

void send_file(int client_fd, const char *filepath) 
{
    FILE *file = fopen(filepath, "rb");
    if (!file) 
    {
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
    const char *content_type = "application/octet-stream";
    if (ext) 
    {
        if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) content_type = "text/html";
        else if (strcmp(ext, ".txt") == 0) content_type = "text/plain";
        else if (strcmp(ext, ".png") == 0) content_type = "image/png";
    }

    send_response(client_fd, 200, "OK", content_type, file_content, file_size);
    free(file_content);
}

void send_response(int client_fd, int status_code, const char *status_msg, const char *content_type, const char *content, int content_length) 
{
    char response_header[BUFFER_SIZE];
    snprintf(response_header, sizeof(response_header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n",
             status_code, status_msg, content_type, content_length);

    send(client_fd, response_header, strlen(response_header), 0);
    send(client_fd, content, content_length, 0);
}


#ifndef SERVER_H
#define SERVER_H

#define PORT 8080
#define BUFFER_SIZE 8192
#define MAX_EVENTS 10
#define WEB_ROOT "/var/www"
#define UPLOAD_DIR "/var/www/uploads"

void handle_client(int client_fd);
void handle_get_request(int client_fd, char *path);
void handle_delete_request(int client_fd, char *path);
void handle_upload(int client_fd);
void send_response(int client_fd, int status_code, const char *status_msg, const char *content_type, const char *content, int content_length);
void send_404(int client_fd);
void send_file(int client_fd, const char *filepath);

#endif

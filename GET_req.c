#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<string.h>

#define PORT 8080
#define BUFF_SIZE 4096
#define WEB_ROOT  "/var/www"

void handle_client(int client_fd);
void send_404(int client_fd);
void send_file(int client_fd, char *filepath);
void send_response(int client_fd, int status_code, char *status_msg, char *content_type, char *content, int content_len);

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    printf("SERVER_FD:%d\n", server_fd);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    int optval=1;
    setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("BINDING");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) < 0)
    {
        perror("LISTENING FAILED");
        exit(EXIT_FAILURE);
    }
    printf("LISTENING...\n");
    while (1)
    {
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0)
        {
            printf("ACCEPT FAILED");
            continue;
        }
        handle_client(client_fd);
        close(client_fd);
    }
    close(server_fd);
    return 0;
}

void handle_client(int client_fd)
{
    char buff[BUFF_SIZE], method[10], path[256], full_path[512];
    int rec = recv(client_fd, buff, sizeof(buff) - 1, 0);
    buff[rec] = '\0';
    sscanf(buff, "%s %s", method, path);
    printf("%s",buff);
    if (strcmp(method, "GET") != 0)
    {
        send_response(client_fd, 405, "METHOD NOT ALLOWED", "text/plain", "405 METHOD NOT ALLOWED", 23);
        return;
    }
    if (strcmp(path, "/") == 0)
    {
        strcpy(path, "/index.html");
    }
    snprintf(full_path, sizeof(full_path), "%s%s", WEB_ROOT, path);

    if (access(full_path, R_OK) != 0)
    {
        send_404(client_fd);
        return;
    }
    send_file(client_fd, full_path);
}

void send_file(int client_fd, char *filepath)
{
    FILE *fp = fopen(filepath, "r");
    if (!fp)
    {
        send_404(client_fd);
        return;
    }
    fseek(fp, 0, SEEK_END);
    int file_size = ftell(fp);
    rewind(fp);
    char *file_content = malloc(file_size);
    fread(file_content, 1, file_size, fp);
    fclose(fp);
    send_response(client_fd, 200, "OK", "text/html", file_content, file_size);
    free(file_content);
}

void send_404(int client_fd)
{
    char *msg = "ERROR 404 NOT FOUND";
    send_response(client_fd, 404, "NOT FOUND", "text/plain", msg, strlen(msg));
}

void send_response(int client_fd, int status_code, char *status_msg, char *content_type, char *content, int content_len)
{
    char response_header[BUFF_SIZE];
    snprintf(response_header, sizeof(response_header), "HTTP/1.0 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", status_code, status_msg, content_type, content_len);
    send(client_fd, response_header, strlen(response_header), 0);
    send(client_fd, content, content_len, 0);
}

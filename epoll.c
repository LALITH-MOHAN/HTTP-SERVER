#include <sys/epoll.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main() {
    struct epoll_event event, events[1];
    int epfd, nfds;

    epfd = epoll_create1(0);  // Create epoll instance

    event.events = EPOLLIN;         // Monitor for input events
    event.data.fd = STDIN_FILENO;   // File descriptor for stdin
    epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &event);  // Add stdin to epoll

    printf("TYPE: ");
    fflush(stdout);  // Ensure "TYPE: " is printed before waiting

    // Wait for input event
    nfds = epoll_wait(epfd, events, 1, -1);

    if (nfds > 0) {
        char buff[100];
        int bytes_read = read(STDIN_FILENO, buff, sizeof(buff) - 1);  
        if (bytes_read > 0) {
            buff[bytes_read] = '\0';  // Null-terminate the string
            write(STDOUT_FILENO, "You typed: ", 11);
            write(STDOUT_FILENO, buff, bytes_read);
        }
    }

    close(epfd);  // Close epoll instance
    return 0;
}

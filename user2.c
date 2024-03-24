#include "msocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>

int main(int argc, char* argv[]) {

    // default arguments
    char *arg1 = "127.0.0.1";
    char *arg2 = "4000";
    char *arg3 = "127.0.0.1";
    char *arg4 = "3000";

    if (argc != 5) { // Check if the number of arguments is not 4
        printf("Usage: %s <src_ip> <src_port> <dest_ip> <dest_port>\n", argv[0]);
        printf("Using default: %s %s %s %s\n\n", arg1, arg2, arg3, arg4);
    } else { // passing arguments
        arg1 = argv[1];
        arg2 = argv[2];
        arg3 = argv[3];
        arg4 = argv[4];
    }

    int msockfd;
    msockfd = m_socket(AF_INET, SOCK_MTP, 0);
    if (msockfd < 0) {
        perror("m_socket");
        exit(EXIT_FAILURE);
    }

    char* ip1 = arg1;
    short port1 = atoi(arg2);

    char *dest = arg3;
    short port2 = atoi(arg4);

    if (m_bind(msockfd, ip1, port1, dest, port2)<0) {
        perror("m_bind");
        m_close(msockfd);
        exit(EXIT_FAILURE);
    }

    printf("Socket created and binded at port %d.\n", port1);

    char buf[1024];
    char filename[1024];

    printf("\nEnter the filename to receive: ");
    scanf("%s", filename); // get received filename from user
    getchar(); // consume newline

    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        m_close(msockfd);
        exit(EXIT_FAILURE);
    }


    printf("Receiving messages :\n");
    while (1) {
        memset(buf, 0, sizeof(buf));
        while (m_recvfrom(msockfd, buf, 1024) < 0);

        if (strncmp(buf, "\r\n", 2) == 0) { // termination symbol
            break;
        }

        printf("%s", buf);
        write(fd, buf, strlen(buf));
    }

    m_close(msockfd);

}
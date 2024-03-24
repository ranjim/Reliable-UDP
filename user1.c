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
    char *arg2 = "3000";
    char *arg3 = "127.0.0.1";
    char *arg4 = "4000";

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

    int number_of_messages = 0;
    int n_bytes;
    char filename[1024];
    char buf[1024];

    printf("\nEnter the filename to send: ");
    scanf("%s", filename); // get filename to send from user
    getchar(); // consume newline

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("file open error");
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Sending file... Please wait until further prompts.\n");
    while (1) {
        memset(buf, 0, sizeof(buf));
        n_bytes = read(fd, buf, 1024);
        if (n_bytes == 0) break;
        else if (n_bytes < 0) {
            perror("File read error");
            close(fd);
            m_close(msockfd);
            exit(EXIT_FAILURE);
        }
        
        number_of_messages++;
        while(m_sendto(msockfd, buf, n_bytes) < 0);
    }

    printf("File sent.\n");

    printf("\nPress ENTER to let user2 know you are closing.\n");
    getchar();

    // send termination symbol
    memset(buf, 0, sizeof(buf));
    strcpy(buf, "\r\n");

    number_of_messages++;
    while(m_sendto(msockfd, buf, 2) < 0);

    printf("Wait a while for user2 to close.\nPress ENTER to close.\n");
    getchar();

    printf("Number of messages generated: %d\n", number_of_messages);

    m_close(msockfd);
    printf("Socket : %d closed.\n", msockfd);
}
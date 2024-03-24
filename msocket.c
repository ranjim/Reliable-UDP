#include "msocket.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

/*
*   Global variables
*/
static key_t                       key1, key2;
static int                         shmid1, shmid2;
static sem_t                       *mtx_info = NULL;
static sem_t                       *sem_A = NULL, *sem_B = NULL;
static struct socket_descriptor    *SM = NULL;
static struct sock_info            *sock_info = NULL;

/*
*   Definitions
*/
void attach_shared() {

    key1 = ftok("/msocket.h", 65);
    key2 = ftok("/msocket.h", 66);
    
    shmid1 = shmget(key1, MAX_SOCKETS * sizeof(struct socket_descriptor), 0666 | IPC_CREAT);
    shmid2 = shmget(key2, sizeof(struct sock_info), 0666 | IPC_CREAT);
    
    SM = (struct socket_descriptor *)shmat(shmid1, (void *)0, 0);
    sock_info = (struct sock_info *)shmat(shmid2, (void *)0, 0);
}

void create_semaphores() {
    mtx_info = sem_open("/mtx_sockinfo", O_CREAT, 0666, 1);
    sem_A = sem_open("/sem_A", O_CREAT, 0666, 0);
    sem_B = sem_open("/sem_B", O_CREAT, 0666, 0);
}

void reset_socket(int sockfd) {
    
    SM[sockfd].pid = -1;
    SM[sockfd].udp_sockfd = 0;
    strcpy(SM[sockfd].dest_ip, "");
    SM[sockfd].dest_port = 0;
    SM[sockfd].swnd = (struct window){1, 1, SEND_BUFF_SIZE, {0}};
    SM[sockfd].rwnd = (struct window){1, 1, RECV_BUFF_SIZE, {0}};
    SM[sockfd].rem_buf = RECV_BUFF_SIZE;
    SM[sockfd].no_space = 0;
}

void reset_sock_info() {
    
    sock_info->sockfd = 0;
    strcpy(sock_info->ip, "");
    sock_info->port = 0;
    sock_info->error_no = 0;
    sock_info->close = 0;
}

int m_socket(int domain, int type, int protocol) {

    if (domain != AF_INET || type != SOCK_MTP || protocol != 0) {
        errno = EINVAL;
        return -1;
    }

    if (!mtx_info) {
        attach_shared();
        create_semaphores();
    }

    sem_wait(mtx_info);

    // look for free entry
    int msockfd;
    for (msockfd = 0; msockfd < MAX_SOCKETS; ++msockfd) {
        if (SM[msockfd].pid == -1) {
            break;
        }
    }

    if (msockfd == MAX_SOCKETS) {
        errno = ENOBUFS;

        sem_post(mtx_info);
        return -1;
    }

    reset_sock_info();

    sem_post(sem_A);    // signal init to create socket

    sem_wait(sem_B);    // wait for init

    // if error found
    if (sock_info->sockfd == -1) {
        errno = sock_info->error_no;

        reset_sock_info();
        sem_post(mtx_info);
        
        return -1;
    }

    // if no error
    SM[msockfd].pid = getpid();
    SM[msockfd].udp_sockfd = sock_info->sockfd;

    reset_sock_info();
    sem_post(mtx_info);

    return msockfd;
}

int m_bind(int m_sockfd, const char *ip, int port, const char *dest_ip, int dest_port) {

    sem_wait(mtx_info);

    sock_info->sockfd = SM[m_sockfd].udp_sockfd;
    strcpy(sock_info->ip, ip);
    sock_info->port = port;

    sem_post(sem_A);    // signal init to bind socket

    sem_wait(sem_B);    // wait for init

    // check error
    if (sock_info->sockfd == -1) {
        errno = sock_info->error_no;

        reset_sock_info();
        sem_post(mtx_info);
        
        return -1;
    }

    // if no error
    strcpy(SM[m_sockfd].dest_ip, dest_ip);
    SM[m_sockfd].dest_port = dest_port;

    reset_sock_info();
    sem_post(mtx_info);

    return 1;
}

int m_close(int m_sockfd) {

    sem_wait(mtx_info);

    sock_info->sockfd = SM[m_sockfd].udp_sockfd;
    sock_info->close = 1;

    sem_post(sem_A);    // signal init to close socket

    sem_wait(sem_B);    // wait for init

    // check error
    if (sock_info->sockfd == -1) {
        errno = sock_info->error_no;

        reset_sock_info();
        sem_post(mtx_info);
        return -1;
    }

    // if no error
    reset_socket(m_sockfd);

    reset_sock_info();
    sem_post(mtx_info);

    return 1;
}

int m_sendto(int m_sockfd, const void *msg, size_t len) {

    sem_wait(mtx_info);

    // check if there is space in send buffer
    if ((SM[m_sockfd].swnd.l + SEND_BUFF_SIZE) % MAX_SEQ_NUM == SM[m_sockfd].swnd.u) {
        errno = ENOBUFS;

        sem_post(mtx_info);
        return -1;
    } 

    // copy message to send buffer
    int idx = SM[m_sockfd].swnd.u % SEND_BUFF_SIZE;                     // index in send buffer
    strncpy(SM[m_sockfd].sbuf[idx], (char *)msg, len);                  // copy message
    SM[m_sockfd].swnd.W[SM[m_sockfd].swnd.u] = 0;                       // set hash value to 0
    SM[m_sockfd].swnd.u = (SM[m_sockfd].swnd.u + 1) % MAX_SEQ_NUM;      // increment upper bound

    sem_post(mtx_info);
    
    // DEBUG:
    // printf("Message added to send buffer\n");
    
    return len;
}

int m_recvfrom(int m_sockfd, void *msg, size_t len) {

    sem_wait(mtx_info);
    
    // check if there is any message in receive buffer
    if (SM[m_sockfd].rwnd.l == SM[m_sockfd].rwnd.u) {
        errno = ENOBUFS;

        sem_post(mtx_info);
        return -1;
    }

    // copy message from receive buffer
    int idx = SM[m_sockfd].rwnd.l % RECV_BUFF_SIZE;                     // index in receive buffer
    strncpy((char *)msg, SM[m_sockfd].rbuf[idx], len);                  // copy message
    SM[m_sockfd].rwnd.W[SM[m_sockfd].rwnd.l] = 0;                       // set hash value to 0
    SM[m_sockfd].rwnd.l = (SM[m_sockfd].rwnd.l + 1) % MAX_SEQ_NUM;      // increment lower bound

    sem_post(mtx_info);
    return len;
}

int dropMessage(float prob) {
    srand(time(NULL));
    return (rand() % 100) < prob * 100;
}
#include "msocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

/*
*   Global variables
*/
key_t                       key1, key2;
int                         shmid1, shmid2;
sem_t                       *mtx_info;
sem_t                       *sem_A, *sem_B;
pthread_mutex_t             mutex = PTHREAD_MUTEX_INITIALIZER;
struct socket_descriptor    *SM;
struct sock_info            *sock_info;

int                         buffer[MSG_SIZE + 1];

/*
*   Testing
*/
#ifdef VERBOSE
int number_of_transmissions = 0;
#endif

/*
*   Threads
*/
void *sender(void *);
void *receiver(void *);
void *garbage_collector(void *);
void *verbose(void *);

/*
*   Declarations
*/
void setup_shared();
void setup_mutex();
void launch_threads();
void clean_up();
void handle_error();
void construct_mtp_packet(char *buffer, struct mtp_packet *packet);
void deconstruct_mtp_packet(char *buffer, struct mtp_packet *packet);
void construct_addr(struct sockaddr_in *addr, char *dest_ip, short dest_port);



/*
*   Definitions
*/
int main() {

    signal(SIGINT, clean_up);
    signal(SIGSEGV, handle_error);
    
    setup_shared();  // setup shared memory

    #ifdef VERBOSE 
    printf("Shared memory set.\n"); 
    #endif

    setup_mutex();     // create mutex

    launch_threads();   // launch sender, receiver and garbage collector threads
    
    #ifdef VERBOSE
    printf("Threads launched.\n");
    #endif

    while(1) {

        sem_wait(sem_A);   // wait for signal from m_socket

        if (sock_info->sockfd == 0 && strcmp(sock_info->ip, "") == 0 && sock_info->port == 0 && sock_info->error_no == 0) {
            // then it is a m_socket call
            // create socket
            int sockid = socket(AF_INET, SOCK_DGRAM, 0);
            if (sockid < 0) {
                sock_info->sockfd = -1;
                sock_info->error_no = errno;
            } else {
                sock_info->sockfd = sockid;
                sock_info->error_no = 0;
            }

            #ifdef VERBOSE
            printf("Socket %d created.\n", sock_info->sockfd);
            #endif
        }

        else if (sock_info->sockfd != 0 && strcmp(sock_info->ip, "") != 0 && sock_info->port != 0) {
            // then it is a m_bind call
            int sockid = sock_info->sockfd;

            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            inet_pton(AF_INET, sock_info->ip, &addr.sin_addr);
            addr.sin_port = htons(sock_info->port);

            if (bind(sockid, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                sock_info->sockfd = -1;
                sock_info->error_no = errno;
            }

            #ifdef VERBOSE
            printf("Socket %d binded.\n", sock_info->sockfd);
            #endif
        }

        else if (sock_info->close == 1) {
            // then it is a m_close call
            if (close(sock_info->sockfd) < 0) {
                sock_info->sockfd = -1;
                sock_info->error_no = errno;
            }

            #ifdef VERBOSE
            printf("Total number of transmissions till now: %d\n", number_of_transmissions);
            printf("Socket %d closed.\n", sock_info->sockfd);
            #endif
        }

        sem_post(sem_B);    // signal for completion

    }

    return 0;
}

void setup_shared() {
    
    key1 = ftok("/msocket.h", 65);
    key2 = ftok("/msocket.h", 66);
    
    shmid1 = shmget(key1, MAX_SOCKETS * sizeof(struct socket_descriptor), 0666 | IPC_CREAT);
    shmid2 = shmget(key2, sizeof(struct sock_info), 0666 | IPC_CREAT);
    
    SM = (struct socket_descriptor *)shmat(shmid1, (void *)0, 0);
    sock_info = (struct sock_info *)shmat(shmid2, (void *)0, 0);

    for(int i=0; i<MAX_SOCKETS; i++) {
        SM[i].pid = -1;
        SM[i].udp_sockfd = 0;
        strcpy(SM[i].dest_ip, "");
        SM[i].dest_port = 0;
        SM[i].swnd = (struct window){1, 1, SEND_BUFF_SIZE, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
        SM[i].rwnd = (struct window){1, 1, RECV_BUFF_SIZE, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
        SM[i].rem_buf = RECV_BUFF_SIZE;
        SM[i].no_space = 0;
    }

    sock_info->sockfd = 0;
    strcpy(sock_info->ip, "");
    sock_info->port = 0;
    sock_info->error_no = 0;
    sock_info->close = 0;
}

void setup_mutex() {

    mtx_info = sem_open("/mtx_sockinfo", O_CREAT, 0666, 1);
    sem_A = sem_open("/sem_A", O_CREAT, 0666, 0);
    sem_B = sem_open("/sem_B", O_CREAT, 0666, 0);
    pthread_mutex_init(&mutex, NULL);
}

void launch_threads() {
    
    pthread_t stid, rtid, gtid;
    
    pthread_create(&stid, NULL, sender, NULL);
    pthread_create(&rtid, NULL, receiver, NULL);
    pthread_create(&gtid, NULL, garbage_collector, NULL);

    #ifdef VERBOSE
    pthread_t vtid;
    pthread_create(&vtid, NULL, verbose, NULL);
    #endif
}

void clean_up() {
    
    for(int i=0; i<MAX_SOCKETS; i++) {
        if (SM[i].pid == -1) 
            close(SM[i].udp_sockfd);
    }

    sem_close(mtx_info);
    sem_close(sem_A);
    sem_close(sem_B);
    pthread_mutex_destroy(&mutex);
    
    shmdt(SM);
    shmdt(sock_info);
    shmctl(shmid1, IPC_RMID, NULL);
    shmctl(shmid2, IPC_RMID, NULL);
    
    sem_unlink("/mtx_sockinfo");
    sem_unlink("/sem_A");
    sem_unlink("/sem_B");

    #ifdef VERBOSE
    printf("Cleaned up shared memory and semaphores.\n");
    #endif
    
    exit(0);
}

void handle_error() {
    #ifdef VERBOSE
    printf("Segmentation fault.\n");
    #endif
    clean_up();
}

void construct_mtp_packet(char *buffer, struct mtp_packet *packet) {
    
    memset(buffer, 0, MSG_SIZE + 1);
    buffer[0] = ((packet->type & 0xF) << 4) | (packet->seq_num & 0xF);
    strncpy(buffer + 1, packet->msg, MSG_SIZE);
}

void deconstruct_mtp_packet(char *buffer, struct mtp_packet *packet) {
    
    memset(packet, 0, sizeof(struct mtp_packet));
    packet->type = (buffer[0] >> 4) & 0xF;
    packet->seq_num = buffer[0] & 0xF;
    strncpy(packet->msg, buffer + 1, MSG_SIZE);
}

void construct_addr(struct sockaddr_in *addr, char *dest_ip, short dest_port) {
    
    addr->sin_family = AF_INET;
    inet_pton(AF_INET, dest_ip, &addr->sin_addr);
    addr->sin_port = htons(dest_port);
}

void *sender(void *arg) {
    
    struct sockaddr_in  dest_addr;
    struct mtp_packet   packet;
    char                buffer[MSG_SIZE + 1];

    while(1) {
        sleep(T/2);

        sem_wait(mtx_info);
        pthread_mutex_lock(&mutex);

        for (int i=1; i<MAX_SOCKETS; ++i) {
            
            if (SM[i].pid == -1) continue;                  // skip if not in use
            if (SM[i].rem_buf == 0) continue;               // skip if receiver cannot accept
            if (SM[i].swnd.l == SM[i].swnd.u) continue;     // skip if window is empty

            construct_addr(&dest_addr, SM[i].dest_ip, SM[i].dest_port);

            for (int j=SM[i].swnd.l;; ++j) {
                
                if (j == SM[i].swnd.u) break;
                else if (j == MAX_SEQ_NUM) j = 0; // wrap around (circular buffer)

                // fill contents of packet
                memset(&packet, 0, sizeof(packet));
                packet.type = MTP_DATA;
                packet.seq_num = j;
                strncpy(packet.msg, SM[i].sbuf[ j % SEND_BUFF_SIZE ], MSG_SIZE);

                if (SM[i].swnd.W[j] == 0) {                     // send if not sent
                    construct_mtp_packet(buffer, &packet);
                    sendto(SM[i].udp_sockfd, buffer, MSG_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                    SM[i].swnd.W[j] = time(NULL);

                    #ifdef VERBOSE
                    number_of_transmissions++;
                    // printf("[msock %d udp %d] Sent packet: %d\n", i, SM[i].udp_sockfd, packet.seq_num);
                    #endif
                }

                else if (SM[i].swnd.W[j] + T < time(NULL)) {    // resend if timeout
                    construct_mtp_packet(buffer, &packet);
                    sendto(SM[i].udp_sockfd, buffer, MSG_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                    SM[i].swnd.W[j] = time(NULL);

                    #ifdef VERBOSE
                    number_of_transmissions++;
                    // printf("[msock %d udp %d] Sent on timeout: %d\n", i, SM[i].udp_sockfd, packet.seq_num);
                    #endif
                }

            }

        }

        pthread_mutex_unlock(&mutex);
        sem_post(mtx_info);
    }
}

void *receiver(void *arg) {
    
    struct sockaddr_in  dest_addr;
    struct mtp_packet   packet;
    char                buffer[MSG_SIZE + 1];
    struct timeval      tv;
    fd_set              readfds;
    int                 maxfd;
    int                 activity;

    while(1) {

        FD_ZERO(&readfds);
        maxfd = 0;

        // set timeout
        tv.tv_sec = T;
        tv.tv_usec = 0;

        sem_wait(mtx_info);
        pthread_mutex_lock(&mutex);

        // load fd set
        for (int i=1; i<MAX_SOCKETS; ++i) {
            if (SM[i].pid == -1) continue;
            FD_SET(SM[i].udp_sockfd, &readfds);
            maxfd = SM[i].udp_sockfd > maxfd ? SM[i].udp_sockfd : maxfd;
        }

        pthread_mutex_unlock(&mutex);
        sem_post(mtx_info);

        activity = select(maxfd + 1, &readfds, NULL, NULL, &tv);

        if (activity == 0) {

            sem_wait(mtx_info);
            pthread_mutex_lock(&mutex);

            for (int i=1; i<MAX_SOCKETS; ++i) {
                if (SM[i].pid == -1) continue;

                if ((SM[i].rwnd.l + RECV_BUFF_SIZE) % MAX_SEQ_NUM == SM[i].rwnd.u) {
                    SM[i].no_space = 1;
                    continue;
                } else {
                    SM[i].no_space = 0;

                    // send ACK indicating free space to sender
                    memset(&packet, 0, sizeof(packet));
                    packet.type = MTP_ACK;
                    packet.seq_num = (SM[i].rwnd.u + MAX_SEQ_NUM - 1) % MAX_SEQ_NUM;
                    sprintf(packet.msg, "%d", (SM[i].rwnd.l + RECV_BUFF_SIZE - SM[i].rwnd.u + MAX_SEQ_NUM) % MAX_SEQ_NUM);

                    construct_addr(&dest_addr, SM[i].dest_ip, SM[i].dest_port);
                    construct_mtp_packet(buffer, &packet);
                    sendto(SM[i].udp_sockfd, buffer, MSG_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                }
            }

            pthread_mutex_unlock(&mutex);
            sem_post(mtx_info);
        }

        else if (activity > 0) {

            sem_wait(mtx_info);
            pthread_mutex_lock(&mutex);

            for (int i=1; i<MAX_SOCKETS; ++i) {

                if (SM[i].pid == -1) continue;

                if (FD_ISSET(SM[i].udp_sockfd, &readfds)) {

                    memset(buffer, 0, MSG_SIZE + 1);
                    recvfrom(SM[i].udp_sockfd, buffer, MSG_SIZE, 0, NULL, NULL);

                    if (dropMessage( p )) continue; // drop message with probability p

                    deconstruct_mtp_packet(buffer, &packet);

                    // check if packet is data or ack
                    if (packet.type == MTP_DATA) {

                        // normal case : u < l + RECV_BUFF_SIZE % MAX_SEQ_NUM
                        if (SM[i].rwnd.u < (SM[i].rwnd.l + RECV_BUFF_SIZE) % MAX_SEQ_NUM && SM[i].rwnd.W[packet.seq_num] == 0) {

                            if (packet.seq_num >= SM[i].rwnd.u && packet.seq_num < (SM[i].rwnd.l + RECV_BUFF_SIZE) % MAX_SEQ_NUM) {
                                
                                // #ifdef VERBOSE
                                // printf("[msock %d udp %d] Received data packet: %d\n", i, SM[i].udp_sockfd, packet.seq_num);
                                // #endif

                                strncpy(SM[i].rbuf[packet.seq_num % RECV_BUFF_SIZE], packet.msg, MSG_SIZE);
                                SM[i].rwnd.W[packet.seq_num] = 1;

                                if (packet.seq_num == SM[i].rwnd.u) {
                                    SM[i].rwnd.u = (SM[i].rwnd.u + 1) % MAX_SEQ_NUM;
                                }
                            }
                        }

                        // wrap around case: u > l + RECV_BUFF_SIZE % MAX_SEQ_NUM
                        else {
                            
                            if ((packet.seq_num >= SM[i].rwnd.u || packet.seq_num < (SM[i].rwnd.l + RECV_BUFF_SIZE) % MAX_SEQ_NUM) && SM[i].rwnd.W[packet.seq_num] == 0) {
                                
                                // #ifdef VERBOSE
                                // printf("[msock %d udp %d] Received data packet: %d\n", i, SM[i].udp_sockfd, packet.seq_num);
                                // #endif

                                strncpy(SM[i].rbuf[packet.seq_num % RECV_BUFF_SIZE], packet.msg, MSG_SIZE);
                                SM[i].rwnd.W[packet.seq_num] = 1;

                                if (packet.seq_num == SM[i].rwnd.u) {
                                    SM[i].rwnd.u = (SM[i].rwnd.u + 1) % MAX_SEQ_NUM;
                                }
                            }
                        }

                        // send ACK
                        memset(&packet, 0, sizeof(packet));
                        packet.type = MTP_ACK;
                        packet.seq_num = (SM[i].rwnd.u - 1 + MAX_SEQ_NUM) % MAX_SEQ_NUM;
                        sprintf(packet.msg, "%d", SM[i].rwnd.l + RECV_BUFF_SIZE - SM[i].rwnd.u);

                        construct_addr(&dest_addr, SM[i].dest_ip, SM[i].dest_port);
                        construct_mtp_packet(buffer, &packet);
                        sendto(SM[i].udp_sockfd, buffer, MSG_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

                        // update no space
                        if ((SM[i].rwnd.l + RECV_BUFF_SIZE) % MAX_SEQ_NUM == SM[i].rwnd.u) {
                            SM[i].no_space = 1;
                        } else {
                            SM[i].no_space = 0;
                        }
                    }

                    // ACK case
                    else if (packet.type == MTP_ACK) {
                        
                        SM[i].swnd.l = packet.seq_num + 1;
                        SM[i].rem_buf = atoi(packet.msg);
                    }
                }
            }

            pthread_mutex_unlock(&mutex);
            sem_post(mtx_info);
        }
    }
}

void *garbage_collector(void *arg) {

    int result;
    
    while(1) {
        sleep(GARBAGE_TIME);

        sem_wait(mtx_info);
        
        for (int i=0; i<MAX_SOCKETS; i++) {
            result = kill(SM[i].pid, 0);
            if (result == -1) {
                close(SM[i].udp_sockfd);
                SM[i].pid = -1;
                SM[i].udp_sockfd = 0;
                strcpy(SM[i].dest_ip, "");
                SM[i].dest_port = 0;
                SM[i].swnd = (struct window){1, 1, SEND_BUFF_SIZE, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
                SM[i].rwnd = (struct window){1, 1, RECV_BUFF_SIZE, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
                SM[i].rem_buf = RECV_BUFF_SIZE;
                SM[i].no_space = 0;
            }
        }
        
        sem_post(mtx_info);
    }

}

void *verbose(void *arg) {
    
    while(1) {

        printf("\nYou may press ENTER to see the current status of socket table.\n");
        printf("\nPress CTRL+C to exit.\n");

        getchar();

        sem_wait(mtx_info);
        pthread_mutex_lock(&mutex);
        printf("%3s %5s %12s %16s %10s %12s %12s %3s %3s\n", "sfd", "pid", "udp_sockfd", "dest_ip", "dest_port", "swnd {l, u}", "rwnd {l, u}", "rb", "ns");
        for (int i=1; i<MAX_SOCKETS; i++) {
            if (SM[i].pid != -1 ) printf("%3d %5d %12d %16s %10d %8d, %2d %8d, %2d %3d %3d\n", i, SM[i].pid, SM[i].udp_sockfd, SM[i].dest_ip, SM[i].dest_port, SM[i].swnd.l, SM[i].swnd.u, SM[i].rwnd.l, SM[i].rwnd.u, SM[i].rem_buf, SM[i].no_space);
        }
        pthread_mutex_unlock(&mutex);
        sem_post(mtx_info);
    }
}
#ifndef __MTP_H__
#define __MTP_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
*  MTP definitions
*/
#define SOCK_MTP        111  
#define T               5
#define p               0.1
#define GARBAGE_TIME    30

#define MAX_SOCKETS     25
#define RECV_BUFF_SIZE  5
#define SEND_BUFF_SIZE  10

#define MAX_SEQ_NUM     16
#define MSG_SIZE        1024


enum m_packet_type {
    MTP_DATA,
    MTP_ACK
};

/*
*  structs
*/
struct window{
    int l;                  /* lower bound */
    int u;                  /* upper bound */
    int n;                  /* window size */
    int W[MAX_SEQ_NUM];     /* hash window */
};

struct mtp_packet{
    int type;               /* type of message */
    int seq_num;            /* sequence number */
    char msg[MSG_SIZE];     /* message */
};

struct socket_descriptor {
    pid_t pid;                              /* process that created socket */
    int udp_sockfd;                         /* m_socket -> udp_socket id */
    char dest_ip[16];                       /* Destination IPv4 address */
    short dest_port;                        /* Destination Port number */
    char sbuf[SEND_BUFF_SIZE][MSG_SIZE];    /* send buffer */
    char rbuf[RECV_BUFF_SIZE][MSG_SIZE];    /* receive buffer */
    struct window swnd;                     /* send window */
    struct window rwnd;                     /* receive window */
    int rem_buf;                            /* remaining buffer in their receiver */
    int no_space;                           /* no space in our receive buffer */
};

struct sock_info {
    int sockfd;
    char ip[16];
    short port;
    int error_no;
    int close;
};

/*
*  Declarations
*/
int m_socket(int, int, int);
int m_bind(int, const char*, int, const char*, int);
int m_sendto(int, const void*, size_t);
int m_recvfrom(int, void*, size_t);
int m_close(int);

/*
*  Error simulation function
*/
int dropMessage(float);

#endif
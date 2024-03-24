# Assignment 5 : Documentation

This is a documentation for the assignment 5 consisting of necessary information and observations.

### Contents of the folder

The submitted folder should contain the following:

* **Makefile**
* **Header file:** msocket.h
* **C-codes:** msocket.c, initmsocket.c, user1.c, user2.c
* **Directories:** some test files to be sent and received.
    * ./test_files/
        * tiny.txt
        * bigger.txt
        * Frankenstein.txt
    * ./recv_files/
        You may use this directory to store the received files.

## Makefile

**Usage :** execute `make run` to test the code. It runs the *init process* which creates the shared memory segment and launches the threads for sender, receiver and garbage collector.
Alternatively, you can also execute `./initmsocket` to run the init process.

The following are details of the content in makefile:

* `make lib`  : creates libmsocket.a library
* `make app`  : creates executable 'user1.c' and 'user2.c'
* `make init` : creates executable to run initmsocket.c in verbose mode
* `make run`  : starts init process
* `make clean`: cleans the directory

The commands `./user1` and `./user2` will run the user processes which send and receive the files respectively.

`./user1` : Reads filename to be sent from the user and sends the file to the destination address.
`./user2` : Reads filename to be received as from user to store contents of the file received.

## Datatypes

We defined the following datatypes:

* **enum** *m_packet_type* **:** Consists of types MTP_DATA and MTP_ACT to distinguish between type of packet sent.
<br>

* **struct** *window* **:** Used to mantain window.
Fields:
    * int l - lower bound of window
    * int u - upper bound of window
    * int n - stores the size of window
    * int W - a hash array of size 16 to keep a queue mark of messages to be sent and received
<br>

* **struct** *mtp_packet* **:** Used to store the packet information.
Fields:
    * int type - stores the type of packet
    * int seq_no - stores the sequence number of packet
    * char msg[MSG_SIZE] - stores the message
<br>

* **struct** *socket_descriptor* **:** Contains the fields for the shared socket descriptor table.
Fields:
    * pid_t pid - stores the process id
    * int udp_sockfd - stores udp socket file descriptor
    * char dest_ip[16] - stores the destination ipv4 address
    * short dest_port - stores the destination port number
    * char sbuf[SEND_BUF_SIZE][MSG_SIZE] - send buffer
    * char rbuf[RECV_BUF_SIZE][MSG_SIZE] - receive buffer
    * struct window swnd - send window
    * struct window rwnd - receive window
    * int rem_buf - remaining buffer size of receiver
    * int no_space - flag to check if space is available in receive buffer
<br>

* **struct** *sock_info* **:** Contains the fields for the shared socket information which is used to pass msocket id to init process for socket create and bind calls.

## Functions

### msocket.c

The following functions are defined in the ***msocket.c*** file and provided to the user processes for communication. The functions are as follows:

* **m_socket** - creates a new socket if the socket table is not full. It loads the shared sock_info structure with the socket id and returns the socket id.

* **m_bind** - binds the specified socket to a port. It loads the shared sock_info structure with the source address and destination address.

* **m_sendto** - stores the message in the send buffer of the specified socket if space is available, else returns -1.

* **m_recvfrom** - receives the message from the receive buffer of the specified socket if message is available, else returns -1.

* **m_close** - closes the socket and frees the socket table entry.

* **drop_message** - returns 1 with a probability p, else returns 0. It is used to simulate packet loss.

Custom functions:

* **attach_shared** - attaches the shared memory segment to the process which is accessed by the functions.

* **create_semaphores** - creates the semaphores for the mutual exclusion of shared memory segment.

* **reset_socket** - resets the socket table entry by clearing its fields.

* **reset_sock_info** - resets the sock_info structure by clearing its fields.

### initmsocket.c

The following functions are defined in the ***initmsocket.c*** file:

* **setup_shared** - sets up the shared memory segment and initializes the socket table.

* **setup_mutex** - sets up the semaphores for the mutual exclusion of shared memory segment.

* **launch_threads** - launches the threads which maintains the socket table and handles the communication between the user processes.

* **clean_up** - cleans up the shared memory segment and semaphores.

* **handle_error** - used in *debugging*. Handles the segmentations error by printing the error message and exiting the process safely.

* **construct_mtp_packet** - constructs the mtp packet with the given type, sequence number and message by handling masking and copying of message content.

* **deconstruct_mtp_packet** - deconstructs the mtp packet by extracting the type, sequence number and message content from the buffer. Used for each of handling the received packets.

* **construct_addr** - constructs the address structure with the given ip address and port number.

Some of the functions have optional print statements for *debugging* purposes which are executed when the VERBOSE macro is defined.

***Threads***

The **initmsocket.c** file contains the following threads which are launched by the *launch_threads* function:

* **sender** - runs the process S which sends the packets to the destination address periodically from the send buffer.

* **receiver** - runs the process R which receives the packets from the source address and stores them in the receive buffer.

* **garbage_collector** - runs the process G which clears the socket table entries which are not in use by any user process.
<br>

Optional thread used for debugging:
* **verbose** - an optional thread which runs in *debugging* mode and prints the content of the socket table on pressing 'Enter'.

## Results

The following are the results of sending and receiving a large sample text file using the MTP with varying probabilities of packet loss.

<br>

*Average number of transmissions to send 1 message = (the total number of packets sent) / (the total number of messages generated by the file).*

<br>

| Packet Loss Probability | Transmissions | Messages | Ratio |
|-------------------------|---------------|----------|-------|
| 0.05                    | 119           | 109      | 1.09  |
| 0.10                    | 129           | 109      | 1.18  |
| 0.15                    | 139           | 109      | 1.28  |
| 0.20                    | 129           | 109      | 1.18  |
| 0.25                    | 120           | 109      | 1.10  |
| 0.30                    | 170           | 109      | 1.56  |
| 0.35                    | 211           | 109      | 1.94  |
| 0.40                    | 198           | 109      | 1.81  |
| 0.45                    | 209           | 109      | 1.92  |
| 0.50                    | 205           | 109      | 1.88  |

Tested on the file *./test_files/Frankenstein.txt* which is a large text file of size 108 kB.

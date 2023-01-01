
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <sys/un.h> // contains struct sockaddr_un
#include "consts.h"

#ifndef nmb_gaurd
#define nmb_gaurd

#define MSG_EXCEPT 020000 /* recv any msg except of specified type */
// found this value on https://ubuntuforums.org/showthread.php?t=1708832
#define MAX_MSG 100

typedef struct
{
    int sockfd;
    struct sockaddr_un dest_addr;
    struct sockaddr_un src_addr;
} MSGQ_NMB;

// container for msgs on msg queue
typedef struct
{
    long m_type;
    char m_data[MAX_MSG];
} MSGQ_MSG;

// common container for both udp and unix socket data.
typedef struct
{
    MSGQ_MSG data; // msg data. this will contain the m_type and data of the message to put on/take from the message queue.
    long type;     // type is used to differentiate different types of msg the server may receive.
                   // For UNIX packets, type
                   //      = 0 : msgsnd request
                   //      = 1 : msgrcv request
                   //      = 2 : error msg
                   // For UDP packets, type
                   //      = 0 : normal message received from other hosts
                   //      = 2 : error msg
                   // if type is 0 or 1, msg will contain the m_type of the msg to be sent/received
                   // if type is 2, the first 4 Bytes of the m_data field in data will contain the IP address of the source host,
                   // and the following bytes will contain the error message
    int flags;     // flags to pass on to msgrcv/msgsnd
} MSG_NET;

// creates a socket to communicate with the local server at /tmp/1111
// returns : instance of MSGQ_NMB which contains sockfd and address of unix domain socket of the server on success.
//           sockfd = fd of socket on success. -1 on error
//           dest_addr contains address of server on success. garbage on error
MSGQ_NMB msgget_nmb()
{
    MSGQ_NMB out;
    out.sockfd = -1;
    struct sockaddr_un svaddr, claddr;
    int sfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    // construct client address
    memset(&claddr, 0, sizeof(claddr));
    claddr.sun_family = AF_UNIX;
    snprintf(claddr.sun_path, sizeof(SV_SOCK_PATH) + 8, "/tmp/1111.%ld", (long)getpid());
    remove(claddr.sun_path);
    if (bind(sfd, (struct sockaddr *)&claddr, sizeof(struct sockaddr_un)) == -1)
    {
        if (DEBUG)
            printf("NMB: Unable to bind client UNIX domain socket. Exiting\n");
        return out;
    }
    // construct server address
    memset(&svaddr, 0, sizeof(struct sockaddr_un));
    svaddr.sun_family = AF_UNIX;
    strncpy(svaddr.sun_path, SV_SOCK_PATH, sizeof(SV_SOCK_PATH) + 1);

    out.sockfd = sfd;
    out.dest_addr = svaddr;
    out.src_addr = claddr;
    return out;
}

// protype of msgnd:
// int msgsnd(int msgid, const void *msgp, size_t msgsz, int msgflg)
// params:
//      MSGQ_NMB : struct containing fd of unix domain socket and address of local server
//      msgp     : pointer to msgdata
//      msgsz    : size of msg
//      msgflag  : flag to to be sent to msgsnd
//               : None(0) or IPC_NOWAIT. This will be passed on to msgsnd
// returns : number of bytes written on success, -1 on error
int msgsnd_nmb(MSGQ_NMB nmb, const void *msgp, size_t msgsz, int msgflag)
{
    MSG_NET msg_net;
    msg_net.type = 0;
    msg_net.flags = msgflag;
    memcpy(&(msg_net.data), msgp, msgsz + 8); // +8 for m_type
    // printf("msg_net.data: %s\n", msg_net.data.m_data);
    // 1. IPC_NOWAIT doesn't wait at any of the blocking calls.
    // To implement it completely, make all blocking calls non-blocking(sendto, recvfrom, msgsnd and msgrcv)
    int num_bytes = sendto(nmb.sockfd, &msg_net, sizeof(msg_net), 0, (struct sockaddr *)&(nmb.dest_addr), sizeof(struct sockaddr_un));
    if (num_bytes == -1)
    {
        if (DEBUG) {
            printf("NMB : Error while sending request to local server via unix socket\n");
            perror("NMB [msgsnd_nmb] ");
        }
    }
    return num_bytes;
}

// protype of msgrcv:
// int msgrcv(int msgid, const void *msgp, size_t msgsz, long msgtype, int msgflg)
// params:
//      MSGQ_NMB : struct containing fd of unix domain socket and address of local server
//      msgp     : pointer to container to store msgdata
//      msgsz    : number of bytes to read. should be < size of buffer
//      msgtype  : type of message to send. msgtype = [<4 lower bytes of recepient's ip>, <4 lower bytes of recepient's pid>]
//      msgflag  : flag to to be sent to msgrcv
//                 Note : all the flags supported by msgrcv are supported by msgrcv_nmb(IPC_NOWAIT, MSG_NOERROR) except MSG_EXCEPT.
//                 This is because unlike normal message queues, msgq_nmb only allows one message queue per system and only one m_type per process.
//                 Processes are not allowed to read any other proc's messages, and hence should not be able to use the MSG_EXCEPT flag.
// returns : number of bytes read on success, -1 on error
int msgrcv_nmb(MSGQ_NMB nmb, void *msgp, size_t msgsz, long msgtype, int msgflag)
{
    MSG_NET msg_net;
    msg_net.type = 1;
    msg_net.data.m_type = msgtype;
    msg_net.flags = msgflag & (~MSG_EXCEPT); // ignore the MSG_EXCEPT flag
    bzero(msg_net.data.m_data, sizeof(msg_net.data.m_data));

    // 1. IPC_NOWAIT doesn't wait at any of the blocking calls.
    // To implement it completely, make all blocking calls non-blocking(sendto, recvfrom, msgsnd and msgrcv)
    // 2. MSG_NOERROR just truncates the data. This will be handled by msgrcv.
    int udp_flag = 0;
    if ((msg_net.flags | IPC_NOWAIT != 0) AND STRICT_NOWAIT)
    {
        udp_flag = MSG_DONTWAIT;
    }

    // send request for msg via unix socket
    if (DEBUG)
        printf("NMB [msgrcv_nmb] : sending request from %s to %s\n", nmb.src_addr.sun_path, nmb.dest_addr.sun_path);
    int num_bytes = sendto(nmb.sockfd, &msg_net, sizeof(msg_net), udp_flag, (struct sockaddr *)&(nmb.dest_addr), sizeof(struct sockaddr_un));
    if (num_bytes == -1)
    {
        if (DEBUG)
        {
            printf("NMB : Error while sending request to local server via unix socket\n");
            perror("NMB [msgrcv_nmb] ");
        }
        return num_bytes;
    }
    // recv via unix socket
    // printf("NMB [msgrcv_nmb] : reading response from server via unix socket\n");
    num_bytes = recvfrom(nmb.sockfd, &msg_net, sizeof(msg_net), udp_flag, NULL, NULL);
    // printf("NMB [msgrcv_nmb] : response received {%s}\n", msg_net.data.m_data);
    memcpy(msgp, &(msg_net.data), msgsz);
    return num_bytes;
}

#endif
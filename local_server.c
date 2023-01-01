#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include "consts.h"
#include "nmb.c"
#include "error.c"

//////////////
// GLOBAL VARS
//////////////
int sfd_un, sfd_udp;
int msgid;

/// interrupt handler - handles ctrl+c. closes all sockets so they can be reused
void interrupt_handler (int signum) {
        close(sfd_un);
        close(sfd_udp);
        printf("Sockets closed\n");
        printf("Exiting\n");
        exit(0);
}

int main()
{
    signal(SIGINT, interrupt_handler);
    signal(SIGTERM, interrupt_handler);
    ////////////////////////////
    // create unix domain socket
    ////////////////////////////
    struct sockaddr_un svaddr_un, claddr_un;
    sfd_un = socket(AF_UNIX, SOCK_DGRAM, 0);
    // TODO: need to remove file too? done in slides.
    // YES. can only create path if it doesn't exist
    remove(SV_SOCK_PATH);
    memset(&svaddr_un, 0, sizeof(struct sockaddr_un));
    svaddr_un.sun_family = AF_UNIX;
    memcpy(svaddr_un.sun_path, SV_SOCK_PATH, sizeof(SV_SOCK_PATH));
    if (bind(sfd_un, (struct sockaddr *)&svaddr_un, sizeof(struct sockaddr_un)) != 0)
    {
        printf(RED BOLD "Local_server [UNIX socket] : Unable to bind socket\n");
        perror("Local_server");
        printf("Exiting\n" RESET);
        return -1;
    }

    ///////////////////
    // create msg queue
    ///////////////////
    key_t key = ftok("local_server.c", 0);
    msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid < 0) {
        printf(RED BOLD "Local_server : Error while create msg queue\n");
        perror("Local_server : msgget" RESET);
        return -1;
    }

    ////////////////////
    // create udp socket 
    ////////////////////
    struct sockaddr_in svaddr_udp, claddr_udp;
    struct ip_mreq mreq;
    // multicast sender
    bzero(&svaddr_udp, sizeof(svaddr_udp));
    svaddr_udp.sin_family = AF_INET;
    svaddr_udp.sin_port = htons(UDP_SERV_PORT);
    svaddr_udp.sin_addr.s_addr = inet_addr(UDP_MGROUP);
    // multicast receiver
    sfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&claddr_udp, sizeof(claddr_udp));
    claddr_udp.sin_family = AF_INET;
    claddr_udp.sin_port = htons(UDP_SERV_PORT);
    claddr_udp.sin_addr.s_addr = htonl(INADDR_ANY);
    // claddr_udp.sin_addr.s_addr = htonl(get_IP(INTERFACE));
    if (bind(sfd_udp, (struct sockaddr *)&claddr_udp, sizeof(claddr_udp))) {
        printf(RED BOLD "Local_server [UDP socket] : Unable to bind socket to client address\n");
        printf("Exiting\n" RESET);
        return -1;
    }
    mreq.imr_multiaddr.s_addr = inet_addr(UDP_MGROUP);
    // mreq.imr_interface.s_addr = htonl(get_IP(INTERFACE));
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sfd_udp, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        printf(RED BOLD "Local_server [UDP socket] : Error while joining multicast group\n");
        perror("setsockopt mreq");
        printf("Exiting\n" RESET);
        return -1;
    }

    /////////////////////////////////////////////////////////////////
    // start error process and store it's unix doman socket's address
    /////////////////////////////////////////////////////////////////
    // start error process
    if (fork() == 0) {
        error_proc();
        exit(0);
    }
    struct sockaddr_un err_addr_un;
    memset(&err_addr_un, 0, sizeof(struct sockaddr_un));
    err_addr_un.sun_family = AF_UNIX;
    snprintf(err_addr_un.sun_path, sizeof(err_addr_un.sun_path), ERROR_SOCK_PATH); // error process will always use this fixed path
    // unlink(ERROR_SOCK_PATH);
    // if (bind(sfd_un, (struct sockaddr *)&err_addr_un, sizeof(struct sockaddr_un)) == -1)
    // {
    //     perror("Local_server [UNIX socket] : Unable to bind socket to error proc\n");
    //     return -1;
    // }

    printf("------------ LOCAL SERVER UP AND RUNNING --------------\n");
    ///////////////////////////////////////////////////
    // continuously read msgs from socket and msg queue
    ///////////////////////////////////////////////////
    int maxfd;
    fd_set rset;
    FD_ZERO(&rset);
    struct sockaddr_in src_addr_udp;
    struct sockaddr_un src_addr_un;
    int src_addr_udp_len;
    int src_addr_un_len;
    for (;;)
    {
        FD_SET(sfd_udp, &rset);
        FD_SET(sfd_un, &rset);
        maxfd = sfd_udp > sfd_un ? sfd_udp : sfd_un;
        maxfd += 1;
        select(maxfd, &rset, NULL, NULL, NULL);
        if (FD_ISSET(sfd_udp, &rset))
        {
            MSG_NET msg_net;
            int n = recvfrom(sfd_udp, &msg_net, sizeof(msg_net), 0, (struct sockaddr *)&src_addr_udp, &src_addr_udp_len);
            printf(GREEN BOLD "Local_server [UDP] : Received UDP msg of type %ld\n", msg_net.type);
            printf(RESET " ");
            if (msg_net.type == 2)
            { // error message
                // forward to error proc via unix socket
                sendto(sfd_un, &(msg_net), sizeof(msg_net), 0, (struct sockaddr *)&err_addr_un, sizeof(err_addr_un));
            }
            else
            {
                // msg from some other host
                // put msg on msgqueue if this msg is for us.
                // bits 31-62(inclusive) of m_type of msg will be the IP of destination. If these match => this packet is for us
                printf("Local_server [UDP] : Our IP: %ld. Target IP: %ld\n", msg_net.data.m_type >> 31, get_IP(INTERFACE));
                if (msg_net.data.m_type >> 31 == get_IP(INTERFACE)) // >> is logical left shift so should be okay as it is
                {
                    printf("Local_server [UDP] : Putting msg on msgqueue. [m_type = %ld. m_data=%s]\n", msg_net.data.m_type, msg_net.data.m_data);
                    msgsnd(msgid, &(msg_net.data), sizeof(msg_net.data), msg_net.flags);
                }
            }
        }
        if (FD_ISSET(sfd_un, &rset))
        {
            MSG_NET msg_net;
            src_addr_un_len = sizeof(claddr_un);
            int n = recvfrom(sfd_un, &msg_net, sizeof(msg_net), 0, (struct sockaddr *)&claddr_un, &src_addr_un_len);
            if (msg_net.type == 0 OR msg_net.type == 2)
            { // msgsnd request or error message.
                // multicast using udp in either case
                int udp_flag = 0;
                if (msg_net.flags | IPC_NOWAIT != 0 AND STRICT_NOWAIT)
                {
                    udp_flag = MSG_DONTWAIT;
                }
                printf("Local_server [UNIX] : Multicasting message\n");
                n = sendto(sfd_udp, &(msg_net), sizeof(msg_net), udp_flag, (struct sockaddr *)&svaddr_udp, sizeof(svaddr_udp));
                if (n < 0) {
                    perror("Local_server [UNIX] : sendto\n");
                    continue;
                }
                printf("Local_server [UNIX] : msg multicasted\n");
            }
            else if (msg_net.type == 1)
            { // msgrcv request
                // read from msqueue and send back on unix socket
                // doing this in a separate process so current proc is not blocked by msgrcv
                if (fork() == 0) {

                    int unix_flag = 0;
                    if ((msg_net.flags | IPC_NOWAIT != 0) AND STRICT_NOWAIT)
                    {
                        unix_flag = MSG_DONTWAIT;
                    }
                    printf("Local_server [UNIX] : Reading msg from msgqueue for m_type : %ld\n", msg_net.data.m_type);
                    if (msgrcv(msgid, &(msg_net.data), sizeof(msg_net.data), msg_net.data.m_type, msg_net.flags) < 0) {
                        perror("Local_server [UNIX] : msgrcv");
                    }
                    printf("Local_server [UNIX] : msg read. msg = {%s}. Sending back response via UNIX socket to client at %s\n", msg_net.data.m_data, claddr_un.sun_path);
                    sendto(sfd_un, &msg_net, sizeof(msg_net), unix_flag, (struct sockaddr *)&claddr_un, src_addr_un_len);
                    printf("Local_server [UNIX] : Response sent\n");
                    exit(0);
                }
            }
        }
    }
}

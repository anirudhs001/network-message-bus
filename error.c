
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include "consts.h"
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include "nmb.c"

int error_proc()
{
    // create unixsocket
    struct sockaddr_un svaddr_un, claddr_un;
    int sfd_un = socket(AF_UNIX, SOCK_DGRAM, 0);
    memset(&claddr_un, 0, sizeof(struct sockaddr_un));
    claddr_un.sun_family = AF_UNIX;
    strncpy(claddr_un.sun_path, ERROR_SOCK_PATH, sizeof(ERROR_SOCK_PATH)); // unique path for error proc
    remove(claddr_un.sun_path);
    if (bind(sfd_un, (struct sockaddr *)&claddr_un, sizeof(struct sockaddr_un)) == -1)
    {
        perror(RED BOLD "Error [UNIX socket]: Unable to bind socket. Exiting\n" );
        printf(RESET " ");
        return -1;
    }
    // server
    memset(&svaddr_un, 0, sizeof(svaddr_un));
    svaddr_un.sun_family = AF_UNIX;
    strncpy(svaddr_un.sun_path, SV_SOCK_PATH, sizeof(SV_SOCK_PATH));
    // TODO

    char buf[10000];
    struct sockaddr_in cliaddr;
    int sfd_icmp, n;
    sfd_icmp = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP); // raw socket for ICMP packet sniffing, AF_INET to access IP header
    socklen_t clilen;
    clilen = sizeof(struct sockaddr_in);
    if (sfd_icmp < 0)
    {
        perror(RED BOLD "Error [RAW socket]: error while creating raw socket ");
        printf(RESET "Exiting\n");
        exit(1);
    }
    int maxfd;
    fd_set rset;
    FD_ZERO(&rset);
    while (1)
    {
        FD_SET(sfd_icmp, &rset);
        FD_SET(sfd_un, &rset);
        maxfd = sfd_icmp > sfd_un ? sfd_icmp : sfd_un;
        maxfd += 1;
        select(maxfd, &rset, NULL, NULL, NULL);
        if (FD_ISSET(sfd_icmp, &rset)) // Received icmp msg => packet dropped on some other machine. inform our local_server
        {
            n = recvfrom(sfd_icmp, buf, 10000, 0, NULL, NULL);
            struct iphdr *ip_hdr = (struct iphdr *)buf;
            struct icmphdr *icmp_hdr = (struct icmphdr *)((char *)ip_hdr + (4 * ip_hdr->ihl));
            int dest_ip = ip_hdr->daddr; // read destination IP from ip_hdr
            printf(RED BOLD "Error [ICMP] : Received ICMP msg of type %d, code %d\n" RESET, icmp_hdr->type, icmp_hdr->code);
            if (icmp_hdr->type == (uint8_t) 3)
            {
                MSG_NET msg_net;
                // FOUND CODE EQUAL TO 0 OR 3
                if (icmp_hdr->code == 0 OR icmp_hdr->code == 3)
                {
                    // NETWORK OR PORT UNREACHABLE
                    msg_net.type = 2; // error message
                    // first 4 bytes of m_data is dest-ip
                    msg_net.data.m_data[0] = (dest_ip >> 24) & 0xFF;
                    msg_net.data.m_data[1] = (dest_ip >> 16) & 0xFF;
                    msg_net.data.m_data[2] = (dest_ip >> 8) & 0xFF;
                    msg_net.data.m_data[3] = (dest_ip)&0xFF;
                    msg_net.data.m_data[4] = icmp_hdr->code; // next byte is error type
                    int length = sizeof(struct sockaddr_un);
                    n = sendto(sfd_un, &msg_net, sizeof(msg_net), 0, (struct sockaddr *)&svaddr_un, sizeof(struct sockaddr));
                    if (n < 0) {
                        perror(RED BOLD "Error [ICMP]: sendto]");
                        printf(RESET " ");
                    }
                    else {
                        printf(RED BOLD "Error [ICMP] : Message sent to local_server at %s\n", svaddr_un.sun_path);
                        printf(RESET " ");
                    }
                }
            }
        }
        if (FD_ISSET(sfd_un, &rset)) // received unix datagram msg => our local_server wants us to print the msg
        {
            MSG_NET msg;
            n = recvfrom(sfd_un, &msg, sizeof(msg), 0, NULL, NULL);
            // first 4 bytes of msg.data.m_data are ip
            char* m_data = msg.data.m_data;
            uint32_t err_ip = ((uint32_t)m_data[0] << 24) + ((uint32_t)m_data[1] << 16) + ((uint32_t)m_data[2] << 8) + (uint32_t)m_data[3];
            struct in_addr ip_addr;
            ip_addr.s_addr = err_ip;
            // next byte is error type
            int err_type = m_data[4];

            // print
            printf(RED BOLD "Error [ICMP] : Error detected at IP : %s. Type : %d\n", inet_ntoa(ip_addr), err_type);
            printf(RESET " ");
        }
    }
}

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
#include <netdb.h>
#include "nmb.c"
#include "consts.h"

#define MAX_MSG 100

MSGQ_NMB msgid;


/// interrupt handler - handles ctrl+c. closes all sockets so they can be reused
void interrupt_handler (int signum) {
    close(msgid.sockfd);
    printf("Sockets closed\n");
    printf("Exiting\n");
    exit(0);
}

int main()
{
    // demo code for our NMB
    // 1. First define the m_type for our msgs. m_type = 6 byte long where
    // [<first bit = 0> <next first 4 bytes = Host's IP address> <remaining 31 bits = current proc's pid's least significant 31 bits>]
    unsigned long ip = get_IP(INTERFACE);
    int pid = getpid() & 0x7FFFFFFF;
    unsigned long m_type = (ip << 31) + pid;

    printf("PID:  %u\n", pid);
    printf("IP :  %lu\n", ip);
    printf(YELLOW BOLD "This proc's m_type is %ld. To send msgs to this proc, use this m_type\n" RESET, m_type);

    // 2. Create the nmb similar to a normal message queue by calling msgget_nmb
    // Note : unlike msgid returned by msgget, msgget_nmb returns a special variable of type MSGQ_NMB. Use this variable in the
    // same way as msgid while passing to msgsnd_nmb and msgrcv_nmb
    MSGQ_NMB msgid = msgget_nmb();

    // 3. Send some msgs
    MSGQ_MSG msg;
    // Need a target m_type to send. Taking this from the user
    printf("Enter target user's m_type: ");
    scanf("%ld", &msg.m_type);
    // Take the message to send from the user as well
    printf("Enter the message to send: ");
    scanf("%*1[\n]");   
    scanf("%[^\n]", msg.m_data);

    printf("Sending message: %s\n", msg.m_data);
    msgsnd_nmb(msgid, &msg, sizeof(msg.m_data), 0);

    // 4. receive msgs
    msgrcv_nmb(msgid, &msg, sizeof(msg.m_data), m_type, 0);
    // show the contents of msg received
    printf("Message Received: %s\n", msg.m_data);


    // 5. flags
    // our msgqueue_nmb supports all the flags a normal message queue does. for example a non-blocking msgrcv using IPC_NOWAIT:
    // NOTE: checkout "consts.h" to have a stricter implementation of all flags.
    // bzero(&msg, sizeof(msg));
    // msgrcv_nmb(msgid, &msg, sizeof(msg.m_data), m_type, IPC_NOWAIT);
    // // this will return instantly. since there's no data on the msgqueue, this would be empty
    // // NOTE: ensure msgqueues is actually empty before starting the local_server by running "$ipcrm --all=msg".
    // //       otherwise this call would still return a message if it already existed on the msgqueue and we would look like idiots.
    // printf("Message Recieved: %s\n", msg.m_data);

    return 0;
}
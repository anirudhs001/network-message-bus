
#ifndef consts
#define consts
#include <net/if.h> // to get ip
#include <sys/ioctl.h>

#define OR ||
#define AND &&
const int DEBUG = 0;         // Print helpful messages during execution to help find bugs if DEBUG = 1
const int UDP_SERV_PORT = 1112;
const char SV_SOCK_PATH[] = "/tmp/1111.sock";
const char ERROR_SOCK_PATH[] = "/tmp/1111.error";
const char INTERFACE[] = "wlp2s0"; //change this
const char UDP_MGROUP[] = "239.0.0.1";
const int STRICT_NOWAIT = 0; /* Change this value to 1 to allow stricter effect of IPC_NOWAIT.                                                     \
                                This flag only alters the effect of IPC_NOWAIT flag passed to msgsnd_nmb and msgrcv_nmb.                           \
                                If STRICT_NOWAIT = 0 (default), on passing the IPC_NOWAIT flag, msgsnd_nmb and msgrcv_nmb will only                \
                                pass the flag as it is to msgsnd and msgrcv.                                                                       \
                                If STRICT_NOWAIT = 1, along with the default behaviour, MSG_DONTWAIT flag will be passed to all sendto/recvfrom    \
                                calls to all unix/udp sockets made during the execution of msgsnd_nmb/msgrcv_nmb. This will ensure that msgrcv_nmb \
                                is completely non-blocking, but might increase its rate of failure.                                                \
                                */

// helper function, that returns the ip address of any interface(eg "eth0").


#define RED   		"\033[0;31m"
#define GREEN 		"\033[0;32m"
#define YELLOW 		"\033[0;33m"
#define BLUE 		"\033[0;34m"
#define PURPLE 		"\033[0;35m"
#define CYAN 		"\033[0;36m"
#define INVERT		"\033[0;7m"
#define RESET  		"\e[0m" 
#define BOLD		"\e[1m"
#define ITALICS		"\e[3m"
#define UNDERLINE	"\e[4m"

long get_IP(const char *interface)
{
    int fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    /* I want to get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;

    /* I want IP address attached to "eth0" */
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);

    ioctl(fd, SIOCGIFADDR, &ifr);

    close(fd);

    /* display result */
    // printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    return ntohl(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr);
}
#endif

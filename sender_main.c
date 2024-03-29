#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h> 
#include <stdint.h>

#define BUFFMAX 140000000
#define TIMEOUT 1

#define SLOWSTART 0
#define CONGAVOI  1
#define FASERECOV 2

#define PACKSIZE 56000  
#define MAXWND 150 

#define HDR_PTR 3*sizeof(int)

typedef struct packet_struct {
    int size;
    struct timespec timer;
    int count;
    union {
        uint8_t msg[PACKSIZE];
        struct packet_header_struct {
            int ack;
            int finish;
            int seqnum;
        } hdr;
    };
} packet;

typedef struct ack_struct {
    int seqnum;
} ack;

uint8_t                 g_file_buffer[BUFFMAX];
FILE*                   g_file_ptr;
long long int           g_byte_left;

int                     g_sockfd;
struct addrinfo         g_hints, 
                        *g_servinfo, 
                        *g_p;
int                     g_rv;
char                    g_port[5];
socklen_t               g_addr_len = sizeof(struct sockaddr_in);

packet                  g_packets[MAXWND];
int                     g_ack_last = 0;
unsigned int            g_ack_prev = -1;
int                     g_ack_expect = 0;
int                     g_conn_state = 1;

int                     g_lost = 0;

int                     g_sshthresh = 64;
int                     g_ack = 0;
int                     g_dupACKcount = 0;
int                     g_cwnd;
int                     g_wnd[MAXWND];// the send queue size might be determined dynamicly by totalsize/packetsize
double                  g_acc;

struct timeval          g_timeout[1];
struct timespec         g_timer;
struct timespec         g_curr;

void initSocket(char* hostname, unsigned short int hostUDPport){
    memset(&g_hints, 0, sizeof(g_hints));
    g_hints.ai_family = AF_UNSPEC;
    g_hints.ai_socktype = SOCK_DGRAM;
    sprintf(g_port, "%d", hostUDPport);
    if ((g_rv = getaddrinfo(hostname, g_port, &g_hints, &g_servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(g_rv));
        return 1;
    }
    // loop through all the results and make a socket
    for(g_p = g_servinfo; g_p != NULL; g_p = g_p->ai_next){
        if ((g_sockfd = socket(g_p->ai_family, g_p->ai_socktype,
                g_p->ai_protocol)) == -1){
            perror("sender: socket");
            continue;
        }
        g_addr_len = g_p->ai_addrlen;
        break;
    }
    if (g_p == NULL){
        fprintf(stderr, "sender: failed to bind socket\n");
        return 2;
    }

    //
}

void endSocket(){
    packet pkt;
    while(g_conn_state != 2){
        memset(&(pkt.hdr), 0, HDR_PTR);
        pkt.hdr.finish = 1;
        pkt.hdr.seqnum = g_ack_last;
        g_ack_last++;
        g_conn_state = 3;

        sendPacket(&(pkt.hdr), HDR_PTR);

        ack last_ack;
        recvFrom(&last_ack, sizeof last_ack, TIMEOUT);
        if(pkt.hdr.ack != 1 || g_lost > last_ack.seqnum || last_ack.seqnum > g_ack_last){
            g_conn_state = 2;
        }
    }
    freeaddrinfo(g_servinfo);
    close(g_sockfd);
}

int sendPacket(void *msg, int msg_len){
    int numbytes;
    if((numbytes = sendto(g_sockfd, msg, msg_len, 0,
            g_p->ai_addr, g_p->ai_addrlen)) == -1){
        perror("sender: sendto");
        exit(1);
    }
    return numbytes;
}

packet* getPacket(int seqnum){
    return &g_packets[seqnum % MAXWND];
}

int numPacket(int seqnum){
    return seqnum % MAXWND;
}

int getMsgSize(int origin){
    return origin - HDR_PTR;
}

// write to file

void openFile(char* filename){
    g_file_ptr = fopen(filename, "rb");
    fseek(g_file_ptr, 0L, SEEK_SET);
}
int readFile(unsigned long long int bytesToBuffer){
    int i;
    char c;

    for(i = 0; i < bytesToBuffer; i++){
        c = fgetc(g_file_ptr);
        g_file_buffer[i] = c;
    }

    return i;
}
void closeFile(){
    fclose(g_file_ptr);
}

struct timeval* setTimeout(int limit){
    g_timeout->tv_sec = limit / 1000;
    g_timeout->tv_usec = (limit % 1000) * 1000;
    return &g_timeout;
}

int recvFrom(void *buff, int len, int timeout){
    int numbytes;
    struct sockaddr_in recvaddr[1];
    fd_set fdset[1];

    while(1){
        FD_ZERO(fdset);
        FD_SET(g_sockfd, fdset);

        select(FD_SETSIZE, fdset, NULL, NULL, setTimeout(timeout)); 
        if(!FD_ISSET(g_sockfd, fdset))
            return 0;

        numbytes = recvfrom(g_sockfd, buff, len, 0, (struct sockaddr *)recvaddr, &g_addr_len);  

        if(g_conn_state == 0){
            memcpy(&(g_p->ai_addr), recvaddr, g_addr_len);
            
            g_conn_state = 1;
            break;
        }
        else{
            struct sockaddr_in fd;
            memcpy(&fd, g_p->ai_addr, g_addr_len);
            if(recvaddr->sin_addr.s_addr == fd.sin_addr.s_addr)
                break;
        }
    }

    return numbytes;
}

void setTimer(){
    clock_gettime(CLOCK_MONOTONIC, &g_timer);
}

int checkTimer(struct timespec *timer){
    if(g_timer.tv_nsec - timer->tv_nsec > (TIMEOUT * 300000)){
        return 1;
    }
    return 0;
}

void adjustCwnd(){
    if(g_cwnd < 1) g_cwnd = 1;
    else if(g_cwnd > MAXWND) g_cwnd = MAXWND;
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer);

int main(int argc, char** argv)
{
    unsigned short int udpPort;
    unsigned long long int numBytes;
    
    if(argc != 5)
    {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    
    udpPort = (unsigned short int)atoi(argv[2]);
    numBytes = atoll(argv[4]);
    
    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer){
    g_byte_left = bytesToTransfer;
    openFile(filename);
    initSocket(hostname, hostUDPport);

    int state = SLOWSTART;

    while(1){
        ack recv_ack;
        int packet_fin_size = 0, trunk_curr = 0;

        int trunk_size = BUFFMAX;
        trunk_size = BUFFMAX;
        if(g_byte_left < BUFFMAX){
            trunk_size = g_byte_left;
        }
        printf("%d\n", trunk_size);
        readFile(trunk_size);
        

        uint8_t *fp = g_file_buffer;
        
        g_dupACKcount = 0;
        g_ack = 0;

        for(packet_fin_size = 0; packet_fin_size < trunk_size; ){
            packet *pkt;
            while(g_ack_last < (g_cwnd + g_lost) && trunk_curr < trunk_size){
                pkt = getPacket(g_ack_last);

                pkt->timer.tv_sec = 0;
                pkt->timer.tv_nsec = 0;

                memset(&(pkt->hdr), 0, HDR_PTR);

                int msg_size = getMsgSize(PACKSIZE);
                if((trunk_size - trunk_curr) < getMsgSize(PACKSIZE)){
                    msg_size = (trunk_size - trunk_curr);
                }

                pkt->size = msg_size + HDR_PTR;
                pkt->count = 0;

                memcpy(&pkt->hdr + 1, fp + trunk_curr, msg_size); 
                trunk_curr += msg_size;

                pkt->hdr.seqnum = g_ack_last;
                g_ack_last++;
            }

            setTimer();
            pkt = getPacket(g_lost);
            if(pkt->timer.tv_sec != 0 && checkTimer(&(pkt->timer))){
                g_sshthresh = 3 * g_cwnd / 4;
                g_cwnd = 1;

                state = SLOWSTART;
                int i;
                for (i = g_lost + 1; i < g_ack_last; i++){
                    pkt = getPacket(i);
                    pkt->timer.tv_sec = g_timer.tv_sec;
                    pkt->timer.tv_nsec = g_timer.tv_nsec;
                }
            }

            adjustCwnd();

            pkt = getPacket(g_lost);
            if(checkTimer(&(pkt->timer))){
                pkt->timer.tv_sec = 0;
            }

            int count = 0, i;
            for (i = g_lost; i < g_ack_last; i++){
                if(i >= g_lost + g_cwnd)
                    break;
                pkt = getPacket(i);
                if(checkTimer(&(pkt->timer))){
                    sendPacket(pkt->msg, pkt->size);
                    pkt->timer.tv_sec = g_timer.tv_sec;
                    pkt->timer.tv_nsec = g_timer.tv_nsec;
                    pkt->count++;
                    count++;
                }
            }
            
            int numbytes;
            if((numbytes = recvFrom(&recv_ack, sizeof(recv_ack), TIMEOUT)) == 0){
                continue;
            }

            while(numbytes != 0){

                if(g_ack_prev != recv_ack.seqnum){
                    g_dupACKcount = 0;
                    g_ack = 1;
                } 
                else {
                    g_dupACKcount++;
                    g_ack = 0;
                }
                g_ack_prev = recv_ack.seqnum;

                switch (state){
                    case SLOWSTART: //slow start
                        if(g_dupACKcount >= 3){
                            pkt = getPacket(g_lost);
                            pkt->timer.tv_sec = 0;
                            pkt->timer.tv_nsec = 0;

                            g_sshthresh = 3 * g_cwnd / 4;
                            g_cwnd = g_sshthresh;
                    
                            state = FASERECOV;
                        }
                        if(g_cwnd >= g_sshthresh){
                            state = CONGAVOI;
                        }
                        if(g_lost < recv_ack.seqnum && recv_ack.seqnum <= g_ack_last){
                            g_cwnd++;
                            adjustCwnd();
                            for (i = g_lost; i < recv_ack.seqnum; i++){
                                packet_fin_size += getMsgSize(g_packets[numPacket(i)].size);
                            }
                            g_lost = recv_ack.seqnum;
                        }
                        break;
                    case CONGAVOI: //congestion avoidance
                        if(g_dupACKcount >= 3){
                            pkt = getPacket(g_lost);
                            pkt->timer.tv_sec = 0;
                            pkt->timer.tv_nsec = 0;

                            g_sshthresh = 3 * g_cwnd / 4;
                            g_cwnd = g_sshthresh;
                    
                            state = FASERECOV;
                        }
                        if(g_lost < recv_ack.seqnum && recv_ack.seqnum <= g_ack_last){
                            g_acc += 1.0 / (double)g_cwnd;
                            if(g_acc > 1.0){
                                g_acc -= 1.0;
                                g_cwnd++;
                            }
                            adjustCwnd();
                            for (i = g_lost; i < recv_ack.seqnum; i++){
                                packet_fin_size += getMsgSize(g_packets[numPacket(i)].size);
                            }
                            g_lost = recv_ack.seqnum;
                        }
                        break;
                    case FASERECOV: //fast recovery
                        if(g_dupACKcount >= 3){
                            pkt = getPacket(g_lost);
                            pkt->timer.tv_sec = 0;
                            pkt->timer.tv_nsec = 0;

                            g_sshthresh = 3 * g_cwnd / 4;
                            g_cwnd = g_sshthresh;
                    
                            state = FASERECOV;
                        }
                        if(g_ack){
                            state = CONGAVOI;
                            g_cwnd = g_sshthresh;
                        }
                        break;
                }
                numbytes = recvFrom(&recv_ack, sizeof(recv_ack), 0);
            }
        }   

        g_byte_left -= trunk_size;
        if(g_byte_left <= 0)
            break;
            
    }
    endSocket();
    closeFile();
}
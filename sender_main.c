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

#define MAXBUFF 50000
#define PACKSIZE 1400

typedef struct packet_struct{
    char msg[PACKSIZE];
    int seqnum;
} packet;

typedef struct ack_struct{
    int seqnum;
} ack;

int     g_packets_num = 0;
int     g_packets_lastsize = 0;
int     g_seqnum = 0;
int     g_seqnum_request = 0;
int     g_sockfd;
int     g_packet_recv[MAXBUFF];
char    g_file_trunk[MAXBUFF][PACKSIZE];
char*   g_file_buffer;

int     g_base = 1;
int     g_head = 1;
int     g_window = PACKSIZE;
time_t  g_time;
double  g_longest_time = 0;

void    reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer);
void    readFile(char* filename, unsigned long long int bytesToTransfer);


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

void* acceptAck(void * null){
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    while(1){
        ack recv_ack;
        recvfrom(g_sockfd, &recv_ack, sizeof(recv_ack), 0, (struct sockaddr *)&their_addr, &addr_len);
        if(recv_ack.seqnum > g_seqnum){
            g_packet_recv[g_seqnum] = 1;
            if(recv_ack.seqnum > g_seqnum_request)
                g_seqnum_request = recv_ack.seqnum;
        }
        else if(recv_ack.seqnum == g_seqnum){
            g_dup = 1;
            g_dup_counter++;
        }
    }
    return NULL;
}

void readFile(char* filename, unsigned long long int bytesToTransfer){
    // printf("YE\n");
    FILE *fp;
    fp = fopen(filename,"rb");
    long filesize;
    fseek(fp, 0L, SEEK_END);
    // printf("YE\n");
    filesize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    // printf("YE\n");
    int i=0;
    g_file_buffer = (char *)malloc(sizeof(char)*(filesize+1));
    char c;
    for(i = 0; i < bytesToTransfer; i++)
    {   
        c = fgetc(fp);
        g_file_buffer[i]=c;
    }
    g_file_buffer[i]='\0';
    fclose (fp);
}

void binaryCopy(char *dest, char *source, int st, int len){
    int i, j;
    j = st; // if st==0 then copy from the beginning
    for(i = 0; i < len; i++){
        dest[i] = source[j];
        j++;
    }
}

int splitFile(unsigned long long int bytesToTransfer) {
    int i = 0;
    //比最大限制大的传输量
    // printf("YE\n");
    for(i = 0; bytesToTransfer > PACKSIZE; i++){
        binaryCopy(g_file_trunk[i], g_file_buffer, i * PACKSIZE, PACKSIZE);
        bytesToTransfer -= PACKSIZE;
    }
    //比最大量小或最后一个剩余量
    if(bytesToTransfer > 0){
        g_packets_lastsize = bytesToTransfer;
        binaryCopy(g_file_trunk[i], g_file_buffer, i * PACKSIZE, bytesToTransfer);
        i++;
    }
    free(g_file_buffer);
    return i;
}

void sendPacket(packet pkt, int g_sockfd, int bytesToTransfer, struct addrinfo *p){
    int numbytes;
    if ((numbytes = sendto(g_sockfd, &pkt, bytesToTransfer, 0,
             p->ai_addr, p->ai_addrlen)) == -1) {
        perror("sender: sendto");
        exit(1);
    }
}

int checkTimeout() {
    time_t curr_time = time(NULL);
    double diff = difftime(curr_time, g_time);  
    
    if(diff > g_longest_time){
        g_time = time(NULL);
        return 1;   
    }   
    return 0;
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    struct addrinfo hints, *servinfo, *p;
    int rv;
    
    char port[5];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    sprintf(port, "%d", hostUDPport);

    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((g_sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("sender: socket");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "sender: failed to bind socket\n");
        return 2;
    }

    readFile(filename, bytesToTransfer);
    g_packets_num = splitFile(bytesToTransfer);

    pthread_t tid;
    pthread_create(&tid, NULL, &acceptAck, NULL);

    packet pkt;

    pkt.seqnum = g_seqnum;
    if(g_packets_num == 1){
        binaryCopy(pkt.msg, g_file_trunk[0], 0, g_packets_lastsize);
    }
    else{
        binaryCopy(pkt.msg, g_file_trunk[0], 0, PACKSIZE);
    }
    
    sendPacket(pkt, g_sockfd, sizeof(pkt), p);

    
    int rwnd = 2500;
    int cwnd = 100;
    int ssthresh = 1000;
    int dupACKcount = 0;
    
    int state = 0;

    while(1){
        switch(state){
            time_t start = clock();
            case 0: //slow start
                if(g_dup == 0){
                    break;
                }

                break;
            case 1: //congestion avoidance

                break;
            case 2: //fast recovery

                break;
        }


    }

    // int i;
    // for(i = 0; i < g_packets_num; i++){
    //     if(i < g_packets_num - 1){
    //         sendPacket(g_file_trunk[i], g_sockfd, PACKSIZE, p);
    //     }
    //     else{
    //         sendPacket(g_file_trunk[i], g_sockfd, g_packets_lastsize, p);
    //     }
    // }

    freeaddrinfo(servinfo);
    
    close(g_sockfd);

    return 0;
}
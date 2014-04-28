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

#define PACKSIZE 1400

char*                   g_file_buffer;
FILE*                   g_file_ptr;
unsigned long long int  g_byte_left;
unsigned long int       g_seqnum;

int                     g_sockfd;
struct addrinfo         g_hints, 
                        *g_servinfo, 
                        *g_p;
int                     g_rv;
char                    g_port[5];

typedef struct packet_struct{
    char msg[PACKSIZE]; // it could be better to dynamicly 
    int seqnum;
    int size;
    int finish;
} packet;

void initSocket(char* hostname, unsigned short int hostUDPport){
    memset(&g_hints, 0, sizeof(g_hints));
    g_hints.ai_family = AF_UNSPEC;
    g_hints.ai_socktype = SOCK_DGRAM;
    sprintf(g_port, "%d", hostUDPport);
    if ((g_rv = getaddrinfo(hostname, g_port, &g_hints, &g_servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(g_rv));
        return 1;
    }
    // loop through all the results and make a socket
    for(g_p = g_servinfo; g_p != NULL; g_p = g_p->ai_next) {
        if ((g_sockfd = socket(g_p->ai_family, g_p->ai_socktype,
                g_p->ai_protocol)) == -1) {
            perror("sender: socket");
            continue;
        }
        break;
    }
    if (g_p == NULL) {
        fprintf(stderr, "sender: failed to bind socket\n");
        return 2;
    }
    freeaddrinfo(g_servinfo);
}

void endSocket(){
    close(g_sockfd);
}

void sendPacket(packet pkt){
    int numbytes;
    if ((numbytes = sendto(g_sockfd, &pkt, sizeof(pkt), 0,
             g_p->ai_addr, g_p->ai_addrlen)) == -1) {
        perror("sender: sendto");
        exit(1);
    }
}

// 按位拷贝

void binaryCopy(char *dest, char *source, int st, int len){
    int i, j;
    j = st; // if st==0 then copy from the beginning
    for(i = 0; i < len; i++){
        dest[i] = source[j];
        j++;
    }
}

// 动态的读取文件

void openFile(char* filename){
    g_file_ptr = fopen(filename,"rb");
    g_file_buffer = (char *)malloc(1);
    fseek(g_file_ptr, 0L, SEEK_SET);
}
void readFile(unsigned long long int bytesToBuffer){
    int i;
    char c;
    free(g_file_buffer);
    memset(&g_file_buffer, 0, sizeof(g_file_buffer));
    g_file_buffer = (char *)malloc(bytesToBuffer+1);
    for(i = 0; i < bytesToBuffer; i++){   
        c = fgetc(g_file_ptr);
        g_file_buffer[i] = c;
    }
}
void closeFile(){
    free(g_file_buffer);
    fclose(g_file_ptr);
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

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    g_byte_left = bytesToTransfer;
    openFile(filename);
    
    for(g_seqnum = 0; g_byte_left > 0; g_seqnum++){
        int packet_size = PACKSIZE;
        packet pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.finish = 0;

        if(g_byte_left < PACKSIZE){
            packet_size = g_byte_left;
            pkt.finish = 1;
        }

        readFile(packet_size);
        binaryCopy(pkt.msg, g_file_buffer, 0, packet_size);
        pkt.seqnum = g_seqnum;
        pkt.size = packet_size;
        // send packet
        initSocket(hostname, hostUDPport);
        sendPacket(pkt);
        endSocket();
        g_byte_left -= packet_size;
    }
   
    closeFile();

    //writeFile("output", 1000);
}
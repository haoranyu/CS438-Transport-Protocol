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

char*   g_file_buffer;
FILE*   g_file_ptr;

int                     g_sockfd;
struct addrinfo         g_hints, 
                        *g_servinfo, 
                        *g_p;
int                     g_rv;
struct sockaddr_storage g_their_addr;
socklen_t 				g_addr_len;
char                    g_port[5];


typedef struct packet_struct{
    char msg[PACKSIZE];
    int seqnum;
    int size;
    int finish;
} packet;


void initSocket(unsigned short int myUDPport){
    memset(&g_hints, 0, sizeof(g_hints));
	g_hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
	g_hints.ai_socktype = SOCK_DGRAM;
	g_hints.ai_flags = AI_PASSIVE; // use my IP
    sprintf(g_port, "%d", myUDPport);
    if ((g_rv = getaddrinfo(NULL, g_port, &g_hints, &g_servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(g_rv));
        return 1;
    }
    // loop through all the results and bind to the first we can
	for(g_p = g_servinfo; g_p != NULL; g_p = g_p->ai_next) {
		if ((g_sockfd = socket(g_p->ai_family, g_p->ai_socktype,
				g_p->ai_protocol)) == -1) {
			perror("receiver: socket");
			continue;
		}

		if (bind(g_sockfd, g_p->ai_addr, g_p->ai_addrlen) == -1) {
			close(g_sockfd);
			perror("receiver: bind");
			continue;
		}

		break;
	}

    if (g_p == NULL) {
		fprintf(stderr, "receiver: failed to bind socket\n");
		return 2;
	}
	freeaddrinfo(g_servinfo);
	g_addr_len = sizeof g_their_addr;
}

void endSocket(){
    close(g_sockfd);
}

packet recvPacket(){
    int numbytes;
    packet pkt;
    if ((numbytes = recvfrom(g_sockfd, &pkt, sizeof(pkt), 0,
    		 (struct sockaddr *)&g_their_addr, &g_addr_len)) == -1) {
		perror("recvfrom");
		exit(1);
	}
	return pkt;
}

///////////////////////

void binaryCopy(char *dest, char *source, int st, int len){
    int i, j;
    j = st; // if st==0 then copy from the beginning
    for(i = 0; i < len; i++){
        dest[i] = source[j];
        j++;
    }
}

///////////////
void reliablyReceive(unsigned short int myUDPport, char* destinationFile);


// 动态的写入文件
void openFile(char* filename){
    g_file_ptr = fopen(filename,"wb");
    g_file_buffer = (char *)malloc(1);
    fseek(g_file_ptr, 0L, SEEK_SET);
}
void writeFile(unsigned long long int bytesFromBuffer){
    int i;
    char c;
    fseek(g_file_ptr, 0L, SEEK_END);
    for(i = 0; i < bytesFromBuffer; i++){
        c = g_file_buffer[i];
        fputc(c, g_file_ptr);
    }
}
void closeFile(){
    free(g_file_buffer);
    fclose(g_file_ptr);
}
////////////

int main(int argc, char** argv)
{
	unsigned short int udpPort;
	
	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}
	
	udpPort = (unsigned short int)atoi(argv[1]);
	
	reliablyReceive(udpPort, argv[2]);
}
void reliablyReceive(unsigned short int myUDPport, char* destinationFile){
	openFile(destinationFile);
	initSocket(myUDPport);
	while(1){
		packet pkt;
		pkt = recvPacket();
		g_file_buffer = (char *)malloc(pkt.size+1);

		binaryCopy(g_file_buffer, pkt.msg, 0, pkt.size);
		//printf("%s\n", pkt.msg);
		writeFile(pkt.size);
		free(g_file_buffer);
		if(pkt.finish == 1){
			break;
		}
	}
	//writeFile(unsigned long long int bytesFromBuffer);

	fclose(g_file_ptr);
}
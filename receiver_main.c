#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#define MAXBUFF 1048576
#define PACKSIZE 1400

typedef struct packet_struct{
	char msg[PACKSIZE];
	int seqnum;
} packet;

typedef struct ack_struct{
	int seqnum;
} ack;

char*   g_file_buffer;

void 	reliablyReceive(unsigned short int myUDPport, char* destinationFile);
void   	writeFile(char* filename, char *content);
void*	get_in_addr(struct sockaddr *sa);

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

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void writeFile(char* filename, char *content){
    int fd, size;
	fd = open(filename, O_RDWR|O_CREAT|O_APPEND);
	write(fd, content, strlen(content));
	close(fd);
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile){
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr_storage their_addr;
	char file[MAXBUFF];
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];
	char *port;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP
	sprintf(port, "%d", myUDPport);

	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("receiver: socket");
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("receiver: bind");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "receiver: failed to bind socket\n");
		return 2;
	}
printf("YE\n");
	freeaddrinfo(servinfo);

	addr_len = sizeof their_addr;
	if ((numbytes = recvfrom(sockfd, file, MAXBUFF-1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
		perror("recvfrom");
		exit(1);
	}
printf("%s\n", file);
	file[numbytes] = '\0';
	writeFile(destinationFile, file);

	close(sockfd);
	return 0;
}

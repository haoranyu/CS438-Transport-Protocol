#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

void reliablyReceive(unsigned short int myUDPport, char* destinationFile);

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
	
}

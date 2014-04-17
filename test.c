#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
char buffer[1500];
char* reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer){
    int fd, size;
	fd = open(filename, O_RDONLY);
	size = read(fd, buffer, bytesToTransfer);
	close(fd);
    return buffer;
}

int main(){
	printf("%s", reliablyTransfer("xxx", 100, "file", 200));
}
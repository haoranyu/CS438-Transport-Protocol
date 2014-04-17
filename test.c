#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
char buffer[1500];
void writeFile(char* filename, char *content){
    int fd, size;
	fd = open(filename, O_RDWR|O_APPEND);
	write(fd, content, strlen(content));
	close(fd);
}

int main(){
	char file[1024];
	file[0] = 'y';
	file[1] = '\0';
	writeFile("file", file);
}
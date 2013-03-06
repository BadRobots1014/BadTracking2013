#include "network.h"

void socket_release(int socket) {
	close(socket);
}

int socket_open(char* hostname, char* port_str) {
	struct addrinfo* info;
	struct addrinfo hints;
	
	int port = atoi(port_str);
	hints.ai_family = AF_UNSPEC;
	hints.ai_protocol = 0;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;

	int r = getaddrinfo(hostname, port_str, &hints, &info);
	if(r != 0) {
		printf("Unable to get host information...\n");
		printf("%s\n", gai_strerror(r));
		return -1;
	}
	
	int fd = socket(info->ai_family, info->ai_socktype,
						info->ai_protocol);
	if(connect(fd, info->ai_addr, info->ai_addrlen) == -1) {
		printf("Unable to connect\n");
		socket_release(fd);
		return -1;
	}

	freeaddrinfo(info);

	return fd;
}

void socket_write(int socket, char* buffer, int length) {
	write(socket, buffer, length);
}
void socket_write_float(int socket, float f) {
	//convert to Big-Endian (DataInputStream in java expects it)
	//<insert stupid assumption here>
	float result;
	char* buffer = (char*)&f;
	char* resultBuffer = (char*)&result;

	resultBuffer[0] = buffer[3];
	resultBuffer[1] = buffer[2];
	resultBuffer[2] = buffer[1];
	resultBuffer[3] = buffer[0];

	write(socket, &f, sizeof(f));
}

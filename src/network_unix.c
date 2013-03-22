#include "network.h"

#ifndef WINDOWS

void socket_release(socket_t* socket) {
	int fd = socket->internal.int_fd;
	close(fd);
	free(socket);
}

socket_t* socket_open(char* hostname, char* port_str) {
	socket_t* network_socket = (socket_t*) malloc(sizeof(socket_t));
	struct addrinfo* info;
	struct addrinfo hints;
	int r;
	int fd;

	hints.ai_family   = AF_UNSPEC;
	hints.ai_protocol = 0;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags    = 0;

	r = getaddrinfo(hostname, port_str, &hints, &info);
	if(r != 0) {
		printf("Unable to get host information...\n");
		printf("%s\n", gai_strerror(r));
		free(network_socket);
		return NULL;
	}

	fd = socket(info->ai_family, info->ai_socktype,
						info->ai_protocol);
	if(connect(fd, info->ai_addr, info->ai_addrlen) == -1) {
		printf("Unable to connect\n");
		socket_release(network_socket);
		return NULL;
	}
	network_socket->internal.int_fd = fd;

	freeaddrinfo(info);
	return network_socket;
}

int  socket_write(socket_t* socket, char* buffer, int length) {
	int fd = socket->internal.int_fd;
	return write(fd, buffer, length);
}
int socket_write_float(socket_t* socket, float f) {
	//convert to Big-Endian (DataInputStream in java expects it)
	//<insert stupid assumption here>
	float result;
	char* buffer = (char*)&f;
	char* result_buffer = (char*)&result;
	int fd = socket->internal.int_fd;

	result_buffer[0] = buffer[3];
	result_buffer[1] = buffer[2];
	result_buffer[2] = buffer[1];
	result_buffer[3] = buffer[0];

	return write(fd, &result, sizeof(result));
}

#endif

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
		printf("%s\n", strerror(errno));
		socket_release(network_socket);
		return NULL;
	}
	network_socket->internal.int_fd = fd;

	freeaddrinfo(info);
	return network_socket;
}

socket_t* socket_create(int socket_type) {
	int fd;
	fd = socket(AF_INET, socket_type, 0);

	socket_t* result = (socket_t*) malloc(sizeof(socket_t));
	result->internal.int_fd = fd;
	return result;
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

void socket_bind(socket_t* socket, char* port_str, int backlog) {
	int fd = socket->internal.int_fd;
	int port = atoi(port_str);
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	//addr.sin_addr = INADDR_ANY;
	if(bind(fd, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
		printf("socket_bind() error binding socket: %s\n", strerror(errno));
		return;
	}
	if(listen(fd, backlog) < 0) {
		printf("socket_bind() error flagging the socket for listening: %s\n", strerror(errno));
		return;
	}
}
socket_t* socket_accept(socket_t* server_socket) {
	int fd = server_socket->internal.int_fd;
	unsigned int addr_len;
	int peerfd;
	struct sockaddr_in addr;

	addr_len = sizeof(addr);

	if((peerfd = accept(fd, (struct sockaddr*) &addr, &addr_len)) < 0) {
		printf("socket_accept() error accepting connection: %s\n", strerror(errno));
		return (socket_t*)NULL;
	}

	socket_t* result_socket = (socket_t*)malloc(sizeof(socket_t));
	result_socket->internal.int_fd = peerfd;
	return result_socket;
}

int socket_read(socket_t* socket, char* buffer, int length) {
	int fd;

	fd = socket->internal.int_fd;

	return read(fd, buffer, length);
}

//Expects Big-Endian format
float socket_read_float(socket_t* socket) {
	float value;
	int fd;

	fd = socket->internal.int_fd;
	read(fd, (char*)&value, sizeof(float));
	
	float result;
	char* buffer = (char*)&value;
	char* result_buffer = (char*)&result;
	fd = socket->internal.int_fd;

	result_buffer[0] = buffer[3];
	result_buffer[1] = buffer[2];
	result_buffer[2] = buffer[1];
	result_buffer[3] = buffer[0];

	return result;
}
#endif

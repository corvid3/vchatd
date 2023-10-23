#pragma once

#include "log.h"
#include <errno.h>
#include<netinet/ip.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct vcd_connection {
	int32_t sockfd;
	int32_t addr;
};

// writes a vcd_connection into a file-descriptor
 static inline bool vcd_connection_serialize(int fd, struct vcd_connection con) {
 	if(write(fd, &con.sockfd, sizeof(con.sockfd)) < 0) {
		return false;
 	}

 	if(write(fd, &con.addr, sizeof(con.addr)) < 0) {
		return false;
 	}

	return true;
 }

/// reads a vcd_connection from a file-descriptor
/// blocking, waits for entire message
static inline bool vcd_connection_deserialize(int fd, struct vcd_connection* con ) {
	if(read(fd, &con->sockfd, sizeof(con->sockfd)) < 0) {
		return false;
	}
	
	if(read(fd, &con->addr, sizeof(con->addr)) < 0) {
		return false;
	}

	return true;
}

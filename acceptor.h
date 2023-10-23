#pragma once

#include "config.h"
#include <netinet/in.h>
#include<pthread.h>
#include"connection.h"

struct vcd_acceptor_return {
	/* 
		primary form of communication from the acceptor
			whenever the acceptor gains an incoming connection

		PROTOCOL: per incoming connection,
			u64: socket filedescriptor 
			u32: ip address
	*/
	int incoming_pipe_rx;

	pthread_t thread;

	int acceptor_fd_copy;
};

extern struct vcd_acceptor_return start_acceptor(struct vcd_config* config);

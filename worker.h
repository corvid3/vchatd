#pragma once

#include"config.h"

#include"pthread.h"
#include<pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

struct vcd_worker_handle {
	pthread_mutex_t* num_cons_mutex;
	// is accepting new connections
	// TODO: implement this, i'm going to sleep
	bool* accepting;
	int *num_cons;

  // pipe<struct vcd_message*>
	int con_msg_rx;

	// pipe<struct vcd_connection*>
	int new_con_tx;

	pthread_t thread;
};

extern struct vcd_worker_handle worker_spawn(const struct vcd_config* config, int id);
extern void vcd_worker_cleanup(struct vcd_worker_handle* handle);

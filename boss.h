#pragma once

#include<pthread.h>
#include "config.h"

/*
  boss.h
  the worker boss manages the routing of messages
    between the acceptor, logic, and the workers
  the boss manages the sorting of messages to the different workers
    and makes sure the right messages get to the right locations
*/

struct boss_args {
  const struct vcd_config* conf;
  int acceptor_rx;
};

// spawns the boss in a new thread
extern pthread_t boss_spawn(struct boss_args);

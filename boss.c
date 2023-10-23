#include "boss.h"
#include "config.h"
#include "connection.h"
#include "log.h"
#include "worker.h"
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <threads.h>
#include <unistd.h>

bool running = true;

void
sigint(int sig)
{
  (void)sig;
  running = false;
  printf("killing server...\n");
}

struct boss
{
  int acceptor_rx;
  const struct vcd_config* conf;
  struct vcd_worker_handle* handles;
  struct pollfd* pollfds;
};

static struct boss
boss_init(struct boss_args args)
{
  struct boss boss = {
    .conf = args.conf,
    .acceptor_rx = args.acceptor_rx,

    .handles =
      calloc(args.conf->server.num_workers,
             sizeof(struct vcd_worker_handle) * args.conf->server.num_workers),

    // allocate an extra pollfd for the acceptor
    .pollfds = calloc(args.conf->server.num_workers + 1, sizeof(struct pollfd)),
  };

  for (int i = 0; i < boss.conf->server.num_workers; i++)
    boss.handles[i] = worker_spawn(boss.conf, i);

  boss.pollfds[0].fd = boss.acceptor_rx;
  boss.pollfds[0].events = POLLIN;

  for (int i = 0; i < boss.conf->server.num_workers; i++) {
    boss.pollfds[i + 1].fd = boss.handles[i].con_msg_rx;
    boss.pollfds[i + 1].events = POLLIN;
  }

  return boss;
}

// TODO: when workers are saturated, push incoming connections into a queue
//    if the queue is saturated (e.g. >2048 cons in the queue), start dropping
//    inbound connections

// @return : bool
//    false on no space for new connection, true on space for new connection
static bool
boss_add_to_suitable_worker(struct boss* boss, struct vcd_connection con)
{
  const int max_cons_per_worker =
    boss->conf->connections.max_connections / boss->conf->server.num_workers;

  int widx = -1;
  int prev_min = max_cons_per_worker;

  for (int i = 0; i < boss->conf->server.num_workers; i++) {
    struct vcd_worker_handle* handle = &boss->handles[i];
    pthread_mutex_lock(handle->num_cons_mutex);

    // add together the currently handled connections
    // and any connections waiting to be added to the worker
    // e.g. 32 max cons per worker
    // 28 handled + 4 incoming = 32 in total, can't push a new one
    const int num_cons = *handle->num_cons + *handle->num_incoming_cons;

    // we only send a new connection if the worker has space
    if (num_cons < prev_min) {
      // printf("sending\n");
      widx = i;
      prev_min = num_cons;
    }

    pthread_mutex_unlock(handle->num_cons_mutex);

    if (num_cons == 0)
      break;
  }

  if (widx < 0) {
    return false;
  }

  struct vcd_worker_handle* handle = &boss->handles[widx];

  pthread_mutex_lock(handle->num_cons_mutex);

  if (!vcd_connection_serialize(handle->new_con_tx, con)) {
    VC_LOG_ERR("failed to serialize new connection to a worker as boss");
    VC_LOG_ERR_CONT("ERRNO %i: %s", errno, strerror(errno));
    raise(SIGTRAP);
  }
  fflush(stdout);

  *handle->num_incoming_cons += 1;

  pthread_mutex_unlock(handle->num_cons_mutex);

  return true;
}

static void*
boss_logic(void* vargs)
{
  struct boss boss = *(struct boss*)vargs;
  free(vargs);

  const int num_workers = boss.conf->server.num_workers;
  const int num_pollfds = num_workers + 1;

  signal(SIGINT, sigint);

  // const int mcons_per_worker =
  //   conf->connections.max_connections / conf->server.num_workers;

  while (running) {
    /// every 2 seconds, we check if the server is not running
    int rp = poll(boss.pollfds, num_pollfds, 2000);
    if (rp < 0) {
      if (errno == EINTR)
        continue;
      VC_LOG_ERR("poll error");
    }

    if (rp == 0)
      continue;

    if (boss.pollfds[0].revents & POLLERR) {
      VC_LOG_ERR("boss acceptor died\n");
      exit(1);
    }

    /* on incoming new-connections... */
    if (boss.pollfds[0].revents & POLLIN) {
      // find a suitable worker, and forward the incoming connection to it
      struct vcd_connection con;
      if (!vcd_connection_deserialize(boss.pollfds[0].fd, &con)) {
        VC_LOG_ERR(
          "failed to deserialize incoming connection from acceptor as boss");
        raise(SIGTRAP);
      }
      boss_add_to_suitable_worker(&boss, con);
    }

    for (int i = 0; i < num_workers; i++) {
      struct pollfd* pollfd = &boss.pollfds[i + 1];
      if (pollfd->revents & POLLIN) {
      }
    }
  }

  return NULL;
}

extern pthread_t
boss_spawn(struct boss_args args)
{
  struct boss boss = boss_init(args);
  struct boss* boss_ptr = malloc(sizeof(struct boss));
  *boss_ptr = boss;

  pthread_t thread;
  pthread_create(&thread, NULL, boss_logic, boss_ptr);

  return thread;
}

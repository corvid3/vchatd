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
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

bool running = true;

void
sigsegv(int sig)
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

static void
boss_add_to_suitable_worker(struct boss* boss, struct vcd_connection con)
{
  for (int i = 0; i < boss->conf->server.num_workers; i++) {
    struct vcd_worker_handle* handle = &boss->handles[i];
    pthread_mutex_lock(handle->num_cons_mutex);
    if (*handle->accepting) {
      vcd_connection_serialize(handle->new_con_tx, con);
      *handle->accepting = false;
    }
    pthread_mutex_unlock(handle->num_cons_mutex);
  }
}

static struct boss
boss_init(struct boss_args args)
{
  struct boss boss = {
    .conf = args.conf,
    .acceptor_rx = args.acceptor_rx,

    .handles = calloc(args.conf->server.num_workers,
                      sizeof(args.conf->server.num_workers)),

    // allocate an extra pollfd for the acceptor
    .pollfds = calloc(args.conf->server.num_workers + 1, sizeof(struct pollfd)),
  };

  for (int i = 0; i < boss.conf->server.num_workers; i++)
    boss.handles[i] = worker_spawn(boss.conf, i);

  sleep(4);

  boss.pollfds[0].fd = boss.acceptor_rx;
  boss.pollfds[0].events = POLLIN;

  for (int i = 0; i < boss.conf->server.num_workers; i++) {
    boss.pollfds[i + 1].fd = boss.handles[i].con_msg_rx;
    boss.pollfds[i + 1].events = POLLIN;
  }

  return boss;
}

static void*
boss_logic(void* vargs)
{
  struct boss boss = *(struct boss*)vargs;
  free(vargs);

  const int num_workers = boss.conf->server.num_workers;
  const int num_pollfds = num_workers + 1;

  signal(SIGSEGV, sigsegv);

  // const int mcons_per_worker =
  //   conf->connections.max_connections / conf->server.num_workers;

  while (running) {
    if (poll(boss.pollfds, num_pollfds, -1) < 0) {
      if (errno == EINTR)
        continue;
      VC_LOG_ERR("poll error");
    }

    /* on incoming new-connections... */
    if (boss.pollfds[0].revents & POLLIN) {
      // find a suitable worker, and forward the incoming connection to it
      struct vcd_connection con =
        vcd_connection_deserialize(boss.pollfds[0].fd);
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
  struct boss* boss_ptr = malloc(sizeof boss);
  *boss_ptr = boss;

  pthread_t thread;
  pthread_create(&thread, NULL, boss_logic, boss_ptr);

  return thread;
}

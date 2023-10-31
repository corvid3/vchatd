#include "acceptor.h"
#include "args.h"
#include "boss.h"
#include "config.h"
#include "worker.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

bool kill = false;

void
sighandler(int sigval)
{
  (void)sigval;
  printf("killing server\n");
  kill = true;
}

int
main(int argc, const char** argv)
{
  signal(SIGINT, sighandler);

  struct vcd_arguments args = get_arguments(argc, argv);
  struct vcd_config conf = config_parse_path(args.conf_filepath);
  config_dump(stdout, &conf);

  // spin up workers
  struct vcd_worker_handle* workers =
    calloc(conf.server.num_workers, sizeof(struct vcd_worker_handle));
  for (int i = 0; i < conf.server.num_workers; i++)
    workers[i] = worker_spawn(&conf, i);

  // start acceptor
  struct vcd_acceptor_args acc_args = {
    .config = &conf,
    .worker_handles = workers,
  };

  struct vcd_acceptor_return accept = start_acceptor(acc_args);

  while (!kill)
    sleep(1);

  // shut down the acceptor
  pthread_cancel(accept.thread);

  close(accept.acceptor_fd_copy);

  // destroy the config
  config_destroy(&conf);
}

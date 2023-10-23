#include "acceptor.h"
#include "args.h"
#include "boss.h"
#include "config.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, const char** argv)
{
  struct vcd_arguments args = get_arguments(argc, argv);
  struct vcd_config conf = config_parse_path(args.conf_filepath);
  config_dump(stdout, &conf);

  // start acceptor
  struct vcd_acceptor_return accept = start_acceptor(&conf);

  // start logic
  // TODO:

  // spin up boss, boss spins up workers and handles message brokering
  struct boss_args boss_args = {
    .acceptor_rx = accept.incoming_pipe_rx,
    .conf = &conf,
  };
  pthread_t thread = boss_spawn(boss_args);
  pthread_join(thread, NULL);

  // shut down the acceptor
  pthread_cancel(accept.thread);

  close(accept.acceptor_fd_copy);

  // destroy the config
  config_destroy(&conf);
}

#include "args.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CONIFIG_FILE_VAL 1

static void print_help(void) {
  printf("\tvchatd\n"
         "\t\tvirtual chat daemon\n"
         "options:\n"
         "\t-c [config] | run with config file");
}

extern struct vcd_arguments get_arguments(const int argc, const char **argv) {
  int opt = 0;

  const char *conf_path = NULL;

  for (;;) {
    // cast const away, but we disallow mutation so it's fine
    opt = getopt(argc, (char **)argv, "+:c:");

    switch (opt) {
    case '?':
      fprintf(stderr, "unknown option: %c\n", optopt);
      break;

    case ':':
      fprintf(stderr, "expected an argument after opt: %c\n", optopt);
      break;

    case 'c':
      printf("using config file: %s\n", optarg);
      conf_path = optarg;
      break;

    case -1:
      goto parse_end;
    }
  }

parse_end:
  if (optind == 1) {
    print_help();
    exit(0);
  }

  struct vcd_arguments args = {
      .conf_filepath = conf_path,
  };

  return args;
}

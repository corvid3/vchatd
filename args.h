#pragma once

struct vcd_arguments {
  const char *conf_filepath;
};

extern struct vcd_arguments get_arguments(const int argc, const char **argv);

#pragma once

enum command_type {
  // anonymous connection signing up for an account
  CMD_AnonSignup,

  // anonymous connection tries to log in to account
  CMD_AnonLogin,
  
  CMD_ClientSay,

  // client joins a server
  CMD_ClientJoinServ,

  // server itself kick the connection off the network
  CMD_HostKick,
};
struct command {
  
};

struct logic_worker_args {
  // the response file descriptor
  int write_fd;
};

#pragma once

#define VC_LOG_ERR(v) fprintf(stderr, "\x1b[31;mERR\x1b[39;m:"\
																			v " at " __FILE__ ":%d\n", __LINE__)
#define VC_LOG_ERR_CONT(v, ...) fprintf(stderr, "|   " v "\n", __VA_ARGS__)

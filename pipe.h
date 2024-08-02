#ifndef PIPE_H
#define PIPE_H

#include <unistd.h>

pid_t bidir_pipe_cmd(int [2], const char *);

#endif

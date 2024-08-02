#ifndef READBACK_H
#define READBACK_H

#include <stdio.h>
#include <unistd.h>

struct readback_context {
	int objdir_fd;
	int pipe_fd[2];
	pid_t pipe_pid;
};

FILE *readback_get_by_hash(struct readback_context *, const unsigned char *, size_t *);
void readback_close(FILE *f);
int readback_init_command(struct readback_context *, const char *);

#endif

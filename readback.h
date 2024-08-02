#ifndef READBACK_H
#define READBACK_H

#include <stdio.h>

struct readback_context {
	int objdir_fd;
};

FILE *readback_get_by_hash(struct readback_context *, const unsigned char *, size_t *);
void readback_close(FILE *f);

#endif

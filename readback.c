#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include "readback.h"
#include "store.h"
#include "binhex.h"

FILE *readback_get_by_hash(struct readback_context *rbc, const unsigned char *hash, size_t *size)
{
	char name[BLOBNAME_SIZE];
	gen_blob_name(name, hash);

	int fd = openat(rbc->objdir_fd, name, O_RDONLY|O_CLOEXEC);
	if (fd<0) return 0;
	struct stat st;
	if (fstat(fd, &st) || st.st_size > PTRDIFF_MAX) {
		close(fd);
		return 0;
	}

	FILE *f = fdopen(fd, "rb");
	if (!f) {
		close(fd);
		return 0;
	}

	*size = st.st_size;
	return f;
}

void readback_close(FILE *f)
{
	fclose(f);
}

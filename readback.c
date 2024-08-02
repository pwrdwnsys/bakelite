#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include "readback.h"
#include "store.h"
#include "binhex.h"

static FILE *get_from_objdir(struct readback_context *rbc, const unsigned char *hash, size_t *size)
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

static FILE *get_from_interactive_pipe(struct readback_context *rbc, const unsigned char *hash, size_t *size)
{
	int fd = fcntl(rbc->pipe_fd[0], F_DUPFD_CLOEXEC, 0);
	if (fd < 0) return 0;
	FILE *f = fdopen(fd, "rb");
	if (!f) {
		close(fd);
		return 0;
	}

	char hashstr[2*HASHLEN];
	bin2hex(hashstr, hash, HASHLEN);
	dprintf(rbc->pipe_fd[1], "%s\n", hashstr);

	char buf[128], *pend;
	if (!fgets(buf, sizeof buf, f)) {
		fclose(f);
		return 0;
	}
	size_t st_size = strtoull(buf, &pend, 10);
	if (*pend != '\n' || st_size > PTRDIFF_MAX) {
		fclose(f);
		return 0;
	}
	*size = st_size;
	return f;
}

FILE *readback_get_by_hash(struct readback_context *rbc, const unsigned char *hash, size_t *size)
{
	if (rbc->objdir_fd>=0) return get_from_objdir(rbc, hash, size);
	else return get_from_interactive_pipe(rbc, hash, size);
}

void readback_close(FILE *f)
{
	fclose(f);
}

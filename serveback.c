#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "store.h"
#include "binhex.h"

static int send_hash(FILE *f, const unsigned char *hash) {
	char name[BLOBNAME_SIZE];
	gen_blob_name(name, hash);

	int fd = open(name, O_RDONLY|O_CLOEXEC);
	if (fd<0) return -1;
	struct stat st;
	if (fstat(fd, &st) || st.st_size > PTRDIFF_MAX) {
		close(fd);
		return -1;
	}
	fprintf(f, "%llu\n", (long long)st.st_size);
	char buf[32768];
	ssize_t l;
	size_t n = st.st_size;
	while (n) {
		l = read(fd, buf, n > sizeof buf ? sizeof buf : n);
		if (l < 0) l = 0;
		fwrite(buf, 1, l, f);
		n -= l;
	}
	close(fd);
	fflush(f);
	return 0;
}

int serveback_main(int argc, char **argv)
{
	char buf[128];
	while (fgets(buf, sizeof buf, stdin)) {
		unsigned char hash[HASHLEN];
		if (!hex2bin(hash, buf, HASHLEN)) {
			puts("?");
			fflush(stdout);
			continue;
		}
		if (send_hash(stdout, hash)) {
			puts("?");
			fflush(stdout);
		}
	}
	return 0;
}

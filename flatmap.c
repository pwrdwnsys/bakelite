#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <endian.h>
#include <errno.h>
#include "flatmap.h"

#include <stdio.h>

static ssize_t pread_wrap(int fd, void *buf, size_t len, off_t off)
{
	for (size_t cnt, i=0; i<len; i+=cnt) {
		cnt = pread(fd, (char *)buf + i, len-i, off+i);
		if (cnt <= 0) {
			if (!cnt) return i;
			return -1;
		}
	}
	return len;
}

static ssize_t pwrite_wrap(int fd, const void *buf, size_t len, off_t off)
{
	for (size_t cnt, i=0; i<len; i+=cnt) {
		cnt = pwrite(fd, (const char *)buf + i, len-i, off+i);
		if (cnt < 0) return -1;
	}
	return len;
}

#define N(k,i) ( ((k)[(i)/2]>>4*((i)%2)) % 16 )

static int64_t search(const struct flatmap *m, const unsigned char *k, size_t l, unsigned char *tail, size_t *pdepth, off_t *plast)
{
	if (l > 255) return -1;
	off_t off = 0;
	int64_t next = m->off0;
	size_t i;
	ssize_t cnt;

	for (i=0; i<=2*l; i++) {
		int c = i<2*l ? N(k,i) : 16;
		off = next + c * sizeof next;
		cnt = pread_wrap(m->fd, &next, sizeof next, off);
		if (cnt != sizeof next) return -1;
		next = le64toh(next);
		if (next < 0) break;
	}
	if (pdepth) *pdepth = i;
	if (plast) *plast = off;

	if (next == -1) return 0;
	next &= INT64_MAX;
	cnt = pread_wrap(m->fd, tail, l+1+1, next); // one extra char to update table
	if (cnt < 0) return cnt;
	if (tail[0]==l && cnt < l+2) return -1; // truncated file?

	return next;
}

off_t flatmap_get(const struct flatmap *m, const unsigned char *k, size_t kl, void *val, size_t vl)
{
	unsigned char tail[257];
	int64_t off = search(m, k, kl, tail, 0, 0);
	if (off<0) return -1;
	if (!off || tail[0] != kl || memcmp(k, tail+1, kl))
		return 0;
	if (pread_wrap(m->fd, val, vl, off+tail[0]+1) != vl)
		return -1;
	return off+tail[0]+1;
}

int flatmap_set(struct flatmap *m, const unsigned char *k, size_t kl, const void *val, size_t vl)
{
	unsigned char tail[257];
	int64_t last;
	size_t depth;
	int64_t off = search(m, k, kl, tail, &depth, &last);
	off_t nextpos = lseek(m->fd, 0, SEEK_END);
	size_t i=0, j;
	uint64_t table[17];
	memset(table, -1, sizeof table);

	if (off < 0) return -1;

	if (off > 0) {
		/* Measure matching prefix length. */
		for (i=0; i<2*kl && i<2*tail[0] && N(k,i) == N(tail+1,i); i++);
		i -= depth;

		/* If we found the key already present, we will just replace it;
		 * there is no matching prefix to split at. */
		if (tail[0] == kl && i+depth==2*kl) {
			off = 0;
		}
	}

	off_t new = nextpos;
	unsigned char buf[256];
	buf[0] = kl;
	memcpy(buf+1, k, kl);
	pwrite_wrap(m->fd, buf, kl+1, nextpos);
	nextpos += kl+1;
	pwrite_wrap(m->fd, val, vl, nextpos);
	nextpos += vl;

	if (off == 0) {
		pwrite_wrap(m->fd, &(uint64_t){ htole64(new|INT64_MIN) }, 8, last);
		return 0;
	}

//printf("prefix %d\n", (int)(depth+i));
//printf("depth=%zd i=%zu kl=%zu\n", depth, i, kl);
	off_t split = nextpos;
	for (j=0; j+1<i; j++) {
		int c = N(tail+1,depth+1+j);
		table[c] = htole64(nextpos + sizeof table);
		pwrite_wrap(m->fd, table, sizeof table, nextpos);
		table[c] = -1;
		nextpos += sizeof table;
	}
	table[depth+i<2*kl ? N(k,depth+i) : 16] = htole64(new|INT64_MIN);
//printf("[%c]\n", tail[1+i]);
	table[depth+i<2*tail[0] ? N(tail+1,depth+i) : 16] = htole64(off|INT64_MIN);
	pwrite_wrap(m->fd, table, sizeof table, nextpos);
	pwrite_wrap(m->fd, &(uint64_t){ htole64(split) }, 8, last);
	return 0;
}

struct header {
	char magic[16];
	uint64_t start;
};

#define MAGIC "flatmap\xff\2\0\0\0\0\0\0\0"

int flatmap_create(struct flatmap *m, int fd, const void *comment, size_t comment_len)
{
	if (lseek(fd, 0, SEEK_END) != 0) return -1;
	struct header header = {
		.magic = MAGIC,
		.start = htole64(sizeof header + comment_len)
	};
	pwrite_wrap(fd, &header, sizeof header, 0);
	if (comment_len) pwrite_wrap(fd, comment, comment_len, sizeof header);
	uint64_t table[17];
	memset(table, -1, sizeof table);
	pwrite_wrap(fd, table, sizeof table, sizeof header + comment_len);
	m->fd = fd;
	m->off0 = sizeof header + comment_len;
	return 0;
}

int flatmap_open(struct flatmap *m, int fd)
{
	struct header header;
	ssize_t cnt = pread_wrap(fd, &header, sizeof header, 0);
	if (cnt < 0) return cnt;
	if (cnt < sizeof header || memcmp(header.magic, MAGIC, sizeof header.magic) ||
	    le64toh(header.start) >= 0x100000000) {
		errno = EINVAL;
		return -1;
	}
	m->fd = fd;
	m->off0 = le64toh(header.start);
	return 0;
}

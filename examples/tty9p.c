#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

void
exits(char *unused)
{
	exit(-1);
}

int
readn(int fd, unsigned char *buf, int len)
{
	int r;
	int i = 0;

	while ((r = read(fd, &buf[i], len - i)) > 0)
		i += r;

	return i;
}

int
main(int argc, char **argv)
{
	int pid;
	int fd;
	unsigned char in[16384];
	unsigned char out;
	int inlen, outlen;

	if (argc < 2)
		exits("args");

	fd = open(argv[1], O_RDWR);
	if (fd < 0)
		exits("%r");

	pid = fork();
	if (pid < 0)
		exits("%r");
	else if (pid == 0) {
		while(1) {
			if (readn(fd, in, 4) != 4)
				exits("%r");
			inlen = in[0] | in[1] << 8 | in[2] << 16 | in[3] << 24;
			inlen -= 4;
			if (readn(fd, &in[4], inlen) != inlen)
				exits("%r");
			inlen += 4;
			write(1, in, inlen);
		}
	}
	else {
		while(read(0, &out, 1) == 1) {
			if (write(fd, &out, 1) != 1)
				exits("%r");
		}
	}
}


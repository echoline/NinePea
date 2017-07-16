#include <u.h>
#include <libc.h>

void
main(int argc, char **argv)
{
	int pid;
	int fd;
	unsigned char in[16384];
	unsigned char out[16384];
	int inlen, outlen;

	if (argc < 2)
		exits("args");

	fd = open(argv[1], ORDWR);
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
		while(1) {
			if (readn(0, out, 4) != 4)
				exits("%r");
			outlen = out[0] | out[1] << 8 | out[2] << 16 | out[3] << 24;
			outlen -= 4;
			if (readn(0, &out[4], outlen) != outlen)
				exits("%r");
			outlen += 4;
			write(fd, out, outlen);
		}
	}
}


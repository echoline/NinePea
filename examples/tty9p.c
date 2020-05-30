#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <stdio.h>

void
exits(char *arg)
{
	fprintf(stderr, "%s\n", arg);
	exit(-1);
}

int
readn(int fd, unsigned char *buf, int len)
{
	int r;
	int i = 0;

	while ((i < len) && ((r = read(fd, &buf[i], len - i)) >= 0)) {
		if (r == 0)
			usleep(100);
		i += r;
	}

	return i;
}

int
main(int argc, char **argv)
{
	int pid;
	int fd;
	unsigned char in[16384];
	unsigned char out[16384];
	int inlen, outlen;
	int r;
	struct termios tty;

	if (argc < 2)
		exits("args");

	fd = open(argv[1], O_RDWR);
	if (fd < 0)
		exits("open");

	if (tcgetattr(fd, &tty) != 0)
		exit(-2);

	cfsetspeed(&tty, B115200);
	tty.c_cflag &= ~PARENB;
	tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;
        tty.c_cflag &= ~CRTSCTS;
        tty.c_cc[VMIN] = 1;
        tty.c_cc[VTIME] = 5;
        tty.c_cflag |= CREAD | CLOCAL;
        cfmakeraw(&tty);
        tcflush(fd, TCIFLUSH);

	if (tcsetattr(fd, TCSANOW, &tty) != 0)
		exit(-3);

	pid = fork();
	if (pid < 0)
		exits("fork");
	else if (pid == 0) {
		while(1) {
			if (readn(fd, in, 4) != 4) {
				exits("readn in 1");
			}	
			inlen = in[0] | in[1] << 8 | in[2] << 16 | in[3] << 24;
			inlen -= 4;
			if (readn(fd, &in[4], inlen) != inlen)
				exits("readn in 2");
			inlen += 4;
			write(1, in, inlen);
			fflush(stdout);
		}
	}
	else {
		while(1) {
			if (readn(0, out, 4) != 4)
				exits("readn out 1");
			outlen = out[0] | out[1] << 8 | out[2] << 16 | out[3] << 24;
			outlen -= 4;
			if (readn(0, &out[4], outlen) != outlen)
				exits("readn out 2");
			outlen += 4;
			write(fd, out, outlen);
		}
	}
}


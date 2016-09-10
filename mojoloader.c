/* gcc -std=gnu99 -o mojoloader mojoloader.c */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <inttypes.h>

#define BUF_SIZE 256

int setup_serial(int fd)
{
	struct termios tty;
	memset(&tty, 0, sizeof tty);
	tcgetattr(fd, &tty);
	cfsetospeed(&tty, B115200);
	cfsetispeed(&tty, B115200);
	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
	tty.c_iflag &= ~IGNBRK;
	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_cc[VMIN]  = 0;
	tty.c_cc[VTIME] = 5;
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);
	tty.c_cflag |= (CLOCAL | CREAD);
	tty.c_cflag &= ~(PARENB | PARODD);
	tty.c_cflag |= 0;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	return tcsetattr(fd, TCSANOW, &tty);
}

void restart_mojo(int fd)
{
	int status;
	ioctl(fd, TIOCMGET, &status);
	status &= ~TIOCM_DTR;
	ioctl(fd, TIOCMSET, &status);
	usleep(5*1000);

	for(int i = 0; i < 5; i++) {
		status &= ~TIOCM_DTR;
		ioctl(fd, TIOCMSET, &status);
		usleep(5*1000);
		status |= TIOCM_DTR;
		ioctl(fd, TIOCMSET, &status);
		usleep(5*1000);
	}
}

int main(int argc, char *argv[])
{
	int c, fd, fd2, flash_size = 0;
	ssize_t numRead;
	char *portname = NULL;
	char *binpath = NULL;
	int clearflash = 0, verify = 0;
	char buf[BUF_SIZE], len[4];
	struct stat statbuf;

	while ((c = getopt (argc, argv, "cvd:f:")) != -1)
		switch (c) {
			case 'c':
				clearflash = 1;
				break;
			case 'd':
				portname = optarg;
				break;
			case 'f':
				binpath = optarg;
				break;
			case 'v':
				verify = 1;
				break;
			case '?':
				if (optopt == 'c')
					fprintf (stderr, "Option -%c requires path to binary.\n", optopt);
				else if (isprint (optopt))
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
				return EXIT_FAILURE;
			default:
				fprintf(stderr, "%s: -d <dev> [-c] [-v] [-f <bin>]\n", argv[0]);
				abort ();
		}

	if(!portname) {
		fprintf(stderr, "%s: -d <dev> [-c] [-f <bin>]\n", argv[0]);
		return EXIT_FAILURE;
	}


	if( (fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC)) < 0) {
		fprintf(stderr, "Failed opening %s\n", portname);
		return EXIT_FAILURE;
	}

	if(setup_serial(fd)) {
		fprintf(stderr, "Failed setting attributes of %s\n", portname);
		return EXIT_FAILURE;
	}

	restart_mojo(fd);

	if(clearflash) {
		printf("Erasing flash...\n");
		write(fd, "E", 1);
		if(read(fd, buf, 1) == 1 && buf[0] == 'D')
			printf("Erasing done. Read %c\n", buf[0]);
		return EXIT_SUCCESS;	
	}
	
	if(!binpath) {
		fprintf(stderr, "Error: No flash file specified\n");
		return EXIT_FAILURE;
	}

	if( (fd2 = open(binpath, O_RDONLY)) < 0 || (stat(binpath, &statbuf) == -1)) {
		fprintf(stderr, "Failed opening %s\n", binpath);
		return EXIT_FAILURE;
	}
	
	write(fd, verify ? "V" : "F", 1);
	if(read(fd, buf, 1) != 1 || buf[0] != 'R') {
		printf("Phase 1: Mojo didn't respond. Read %c\n", buf[0]);
		return EXIT_FAILURE;
	}

	for (int i = 0; i < 4; i++) {
		len[i] = ((intmax_t) statbuf.st_size >> (i * 8) & 0xff);
	}
	
	write(fd, len, 4);
	if(read(fd, buf, 1) != 1 || buf[0] != 'O') {
		printf("Phase 2: Mojo didn't respond. Read %c\n", buf[0]);
		return EXIT_FAILURE;
	}

	while((numRead = read(fd2, buf, BUF_SIZE)) > 0)
		if (write(fd, buf, numRead) != numRead) {
			fprintf(stderr, "Error flashing file %s\n", binpath);
			return EXIT_FAILURE;
		}

	if(read(fd, buf, 1) != 1 || buf[0] != 'D') {
		printf("Phase 3: Mojo didn't respond. Read %c\n", buf[0]);
		return EXIT_FAILURE;
	}

	if (verify) {
		printf("Verifying...");
		write(fd, "S", 1);
		if (read(fd, buf, 5) != 5 || buf[0] !=  '\xaa') {
			printf("Failed. Mojo didn't not send valid header. Read %c\n", buf[0]);			
			return EXIT_FAILURE;
		}

		for (int i = 0; i < 4; i++) {
			flash_size |= ((intmax_t)buf[i+1] & 0xff) << (i * 8);
		}
		// substract 5 due to start byte (\xaa + 4 bytes of prepended length)
		if ((flash_size - 5) != statbuf.st_size) {
			printf("Failed. Size mismatch. %d vs %d\n", flash_size - 5, statbuf.st_size);
			return EXIT_FAILURE;
		}
		lseek(fd2, 0, SEEK_SET);
		int need = flash_size - 5;
		int want = (need > BUF_SIZE) ? BUF_SIZE : need;
		int loc = 1;
		while((numRead = read(fd, buf, want)) > 0) {
			char tmp[BUF_SIZE];
			need -= numRead;
			want = (need > BUF_SIZE) ? BUF_SIZE : need;
			read(fd2, tmp, numRead);
			for(int i = 0; i < numRead; i++, loc++) {
				if (buf[i] != tmp[i]) {
					printf("Failed. Data mismatch. Got %c expected %c @ offset %d\n", buf[i], tmp[i], loc);
					return EXIT_FAILURE;
				}
			}
		}
		printf("OK\n");
	}

	write(fd, "L", 1);
	usleep(1000000);
	if(read(fd, buf, 1) != 1 || buf[0] != 'D') {
		printf("Phase 4: Mojo didn't respond. Read %c\n", buf[0]);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


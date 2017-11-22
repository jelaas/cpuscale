CC=musl-gcc-x86_32
CFLAGS=-Wall
all:	cpuscale
cpuscale:	cpuscale.o daemonize.o
rpm:	cpuscale
	strip cpuscale
	bar -c --license=GPLv2+ --version 1.0 --release 1 --name cpuscale --fgroup=root --fuser=root cpuscale-1.0-1.rpm prefix/usr/bin::cpuscale prefix/usr/lib/systemd/system::cpuscale.service
clean:
	rm -f *.o cpuscale *.rpm

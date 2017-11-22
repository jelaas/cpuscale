CC=musl-gcc-x86_32
CFLAGS=-Wall
all:	cpuscale
cpuscale:	cpuscale.o daemonize.o
rpm:	cpuscale
	strip cpuscale
	bar -c --license=GPLv2+ --version 1.0 --release 1 --name cpuscale --prefix=/usr/bin --fgroup=root --fuser=root cpuscale-1.0-1.rpm cpuscale
clean:
	rm -f *.o cpuscale *.rpm

/*
 * File: cpuscale.c
 * Implements: cpu scaling for virtual guests
 *
 * Copyright: Jens Låås, Uppsala University, 2017
 * Copyright license: According to GPL, see file COPYING in this directory.
 *
 */

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include "daemonize.h"

#define NSTAT 7

struct {
	int debug;
	int delay;
	int low,high;
	int daemon;
	int mincpu;
	int loadtest;
} conf;

struct {
	int disabledelay;
	int sleeptime;
	int checkcount;
	int enabled;
	int cpus;
} var;

int cpus_online()
{
	int fd, i=0, cpus = 1;
	ssize_t n;
        char fn[64], buf[4];
	
	for(i=1;;i++) {
		snprintf(fn, sizeof(fn), "/sys/devices/system/cpu/cpu%d/online", i);
		fd = open(fn, O_RDONLY);
		if(fd == -1) break;
		n = read(fd, buf, 1);
		close(fd);
		if(n != 1) continue;
		if(buf[0] == '1') cpus++; 
	}
	return cpus;
}

int cmd_cpu(int start, char match, char output)
{
	int fd, i=0;
	ssize_t n;
        char fn[64], buf[4], cmd[1];
	
	cmd[0] = output;
	
	for(i=start;;i++) {
		snprintf(fn, sizeof(fn), "/sys/devices/system/cpu/cpu%d/online", i);
		fd = open(fn, O_RDONLY);
		if(fd == -1) return -1;
		n = read(fd, buf, 1);
		close(fd);
		if(n != 1) continue;
		if(buf[0] == match) {
			fd = open(fn, O_RDWR);
			if(fd == -1) continue;
			if(conf.debug > 1) printf("Writing cpu command '%c'\n", cmd[0]);
			if(write(fd, cmd, 1) != 1) {
				if(conf.debug) printf("Writing cpu command failed\n");
				close(fd);
				return -1;
			}
			close(fd);
			return 0;
		}
	}
	return -1;
}

int enable_cpu()
{
	int rc;
	
	var.disabledelay = 0;
	if(conf.debug) 	printf("enabling CPU\n");
	rc = cmd_cpu(1, '0', '1');
	if(!rc) var.cpus++;
	return rc;
}

int disable_cpu()
{
	var.disabledelay++;
	if(var.disabledelay > conf.delay) {
		int rc;
		if(conf.debug) 	printf("disabling CPU\n");
		var.disabledelay = 0;
		rc = cmd_cpu(conf.mincpu, '1', '0');
		if(!rc) var.cpus--;
		return rc;
	}
	return 0;
}

int c(uint64_t *v)
{
	int fd, i=0;
	ssize_t n;
	char buf[256];
	
	fd = open("/proc/stat", O_RDONLY);
	if(fd == -1) return -1;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if(n <= 0) return -1;
	
	buf[n] = 0;
	{
		char *p;
		
		p = strchr(buf, ' ');
		while(p) {
			p++;
			v[i++] = strtoull(p, (void*) 0, 10);
			if(i>(NSTAT-1)) break;
			p = strchr(p, ' ');
		}
	}
	v[1] = 0;
	return 0;
}

int subv(uint64_t *v1, const uint64_t *v2)
{
	int i;
	for(i=0;i<NSTAT;i++) {
		if(v2[i] <= v1[i])
			v1[i] = v1[i] - v2[i];
		else
			v1[i] = 0;
	}
	return 0;
}
int copyv(uint64_t *v1, const uint64_t *v2)
{
	int i;
	for(i=0;i<NSTAT;i++) {
		v1[i] = v2[i];
	}
	return 0;
}
uint64_t sumv(uint64_t *v)
{
	int i;
	uint64_t sum = 1;
	for(i=0;i<NSTAT;i++) {
		sum += v[i];
	}
	return sum;
}

int check_enabled()
{
	var.checkcount++;

	if(var.checkcount > 10) {
		struct stat buf;

		var.cpus = cpus_online();

		var.checkcount = 0;
		if(stat("/etc/platform/disable_cpuscale", &buf)) {
			var.sleeptime = 1;
			var.enabled = 1;
		} else {
			var.sleeptime = 30;
			var.enabled = 0;
		}
	}
	if(var.enabled) return 0;
	return -1;
}

int main(int argc, char **argv)
{
	uint64_t v[NSTAT];
	uint64_t v2[NSTAT];
	uint64_t t[NSTAT];

	conf.delay = 30;
	conf.debug = 0;
	conf.low = 20;
	conf.high = 100;
	conf.mincpu = 1;

	{
		int i;
		for(i=1;i<argc;i++) {
			if(!strcmp(argv[i], "-D")) conf.daemon = 1;
			if(!strcmp(argv[i], "-v")) conf.debug++;
			if(!strcmp(argv[i], "-L")) conf.low = atoi(argv[i+1]);
			if(!strcmp(argv[i], "-H")) conf.high = atoi(argv[i+1]);
			if(!strcmp(argv[i], "-delay")) conf.delay = atoi(argv[i+1]);
			if(!strcmp(argv[i], "-loadtest")) conf.loadtest = 1;
			if(!strcmp(argv[i], "-mincpu")) conf.mincpu = atoi(argv[i+1]);
			if(!strcmp(argv[i], "-h")) {
				printf("cpuscale [-D] [-v] [-L N] [-H N] [-delay N]\n"
				       " -D          daemonize, disabled -v\n"
				       " -v          increase verbosity/debugging\n"
				       " -L N        set idle low threshold to integer N [20]\n"
				       " -H N        set idle high threshold to integer N [100]\n"
				       " -delay N    set cpu disable delay to integer N [30]\n"
				       " -mincpu N   set minimum number enabled cpus to N [1]\n"
					);
				exit(0);
			}
		}

	}
	if(conf.delay < 0) conf.delay = 0;
	if(conf.low < 1) conf.low = 1;
	if(conf.high < 1) conf.high = 50;
	if(conf.low > conf.high) conf.low = conf.high;
	if(conf.mincpu < 1) conf.mincpu = 1;

	if(conf.daemon) {
		conf.debug = 0;
		daemonize();
	}


	var.sleeptime = 1;
	var.cpus = cpus_online();
	if(var.cpus < 1) var.cpus = 1;

	if(conf.loadtest) {
		int fd;
		char buf[1];
		fd = open("/dev/urandom", O_RDONLY);
		if(fd == -1) {
			fprintf(stderr, "Failed to open /dev/urandom\n");
			exit(1);
		}
		while(1) {
			if(read(fd, buf, 1)!= 1) sleep(1);
			if(buf[0] & 0x1) enable_cpu(); else disable_cpu(); buf[0] >>= 1;
			if(buf[0] & 0x1) enable_cpu(); else disable_cpu(); buf[0] >>= 1;
			if(buf[0] & 0x1) enable_cpu(); else disable_cpu(); buf[0] >>= 1;
			if(buf[0] & 0x1) enable_cpu(); else disable_cpu(); buf[0] >>= 1;
			if(buf[0] & 0x1) enable_cpu(); else disable_cpu(); buf[0] >>= 1;
			if(buf[0] & 0x1) enable_cpu(); else disable_cpu(); buf[0] >>= 1;
			if(buf[0] & 0x1) enable_cpu(); else disable_cpu(); buf[0] >>= 1;
			if(buf[0] & 0x1) enable_cpu(); else disable_cpu(); buf[0] >>= 1;
		}
	}

	c(v);
	while(1) {
		uint64_t idle;
		
		sleep(var.sleeptime);
		if(check_enabled()) continue;
		c(v2);
		copyv(t, v2);
		subv(t, v);
		copyv(v, v2);
		if(conf.debug > 1) {
			int i;
			for(i=0;i<NSTAT;i++) printf("%llu ", t[i]);
		}
		idle = ((t[3] + t[4]) * 100) / sumv(t);
		if(conf.debug > 1) printf("\nidle %llu of %llu: %llu %d %d\n", t[3] + t[4], sumv(t), idle, var.disabledelay, var.cpus);
		if(idle < conf.low/var.cpus) {
			if(enable_cpu()) {
				if(conf.debug) printf("enable CPU failed\n");
			}
		}
		if(idle > conf.high/var.cpus) {
			if(disable_cpu()) {
				if(conf.debug) printf("disable CPU failed\n");
			}
		} else {
			var.disabledelay = 0;
		}
	}
	exit(0);
}

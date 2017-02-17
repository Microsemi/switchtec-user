/*
 * Microsemi Switchtec(tm) PCIe Management Command Line Interface
 * Copyright (c) 2017, Microsemi Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include "progress.h"

#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static struct timeval start_time;

static void timeval_subtract(struct timeval *result,
			     struct timeval *x,
			     struct timeval *y)
{
	if (x->tv_usec < y->tv_usec) {
		int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;

		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}

	if (x->tv_usec - y->tv_usec > 1000000) {
		int nsec = (x->tv_usec - y->tv_usec) / 1000000;

		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}

	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;
}

static void print_bar(int cur, int total)
{
	int bar_width;
	struct winsize w;
	int i;
	float progress = cur * 100.0 / total;
	int pos;

	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

	bar_width = w.ws_col - 23;
	pos = bar_width * cur / total;

	printf(" %3.0f%% [", progress);
	for (i = 0; i < bar_width; i++) {
		if (i < pos)
			putchar('=');
		else if (i == pos)
			putchar('>');
		else
			putchar(' ');
	}
	printf("] ");
}

static void print_time(struct timeval *interval)
{
	div_t minsec, hrmin;

	minsec = div(interval->tv_sec, 60);
	hrmin = div(minsec.quot, 60);

	printf("%d:%02d:%02d", hrmin.quot, hrmin.rem, minsec.rem);
}

static int calc_eta(int cur, int total, struct timeval *eta)
{
	struct timeval now;
	struct timeval elapsed;
	double elaps_sec;

	if (cur == 0)
		return -1;

	gettimeofday(&now, NULL);
	timeval_subtract(&elapsed, &now, &start_time);
	elaps_sec = elapsed.tv_sec + elapsed.tv_usec * 1e-6;

	elaps_sec = (elaps_sec / cur) * (total - cur);

	eta->tv_sec = elaps_sec;

	return 0;
}

void progress_start(void)
{
	gettimeofday(&start_time, NULL);
}

void progress_update(int cur, int total)
{
	struct timeval eta;

	print_bar(cur, total);

	printf("ETA:  ");
	if (calc_eta(cur, total, &eta))
		printf("-:--:--");
	else
		print_time(&eta);

	printf("\r");
	fflush(stdout);
}

void progress_finish(void)
{
	struct timeval now;
	struct timeval elapsed;

	gettimeofday(&now, NULL);
	timeval_subtract(&elapsed, &now, &start_time);

	print_bar(100, 100);
	printf("Time: ");
	print_time(&elapsed);
	printf("\n");
}

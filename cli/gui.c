/*
 * Microsemi Switchtec(tm) PCIe Management Library
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

#include "gui.h"
#include "switchtec/switchtec.h"
#include <switchtec/utils.h>
#include "suffix.h"

#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <curses.h>

#define WINBORDER  '|', '|', '-', '-', 0, 0, 0, 0
#define WINPORTX 20
#define WINPORTY 15
#define WINPORTSIZE WINPORTY, WINPORTX

struct portloc {
	unsigned startx;
	unsigned starty;
};

  /* Generate a port based string for the port windows */

static void
portid_str(char *str, struct switchtec_port_id *port_id)
{
	sprintf(str, "%s (%d-%d-%d-%d)", port_id->upstream ? "^ " : "v ",
		port_id->phys_id, port_id->partition, port_id->stack,
		port_id->stk_id);
}

  /* Determine positioning for the port windows. Upstream ports at the
   * top, down-stream ports at the bottom. */

static void
get_portlocs(struct portloc *portlocs, struct switchtec_status *status, int ret)
{
	unsigned p, nup, ndown, iup, idown;
	nup = ndown = iup = idown = 0;
	for (p = 0; p < ret; p++)
		if (status[p].port.upstream)
			nup++;
		else
			ndown++;

	for (p = 0; p < ret; p++) {
		if (status[p].port.upstream) {
			portlocs[p].startx =
			    (iup + 1) * COLS / (nup + 1) - WINPORTY / 2;
			portlocs[p].starty = 1;
			iup++;
		} else {
			portlocs[p].startx =
			    (idown + 1) * COLS / (ndown + 1) - WINPORTY / 2;
			portlocs[p].starty = LINES - WINPORTY - 1;
			idown++;
		}
	}

}

  /* Draw a window for the port. */

static WINDOW *
gui_portwin(struct portloc *portlocs,
	    struct switchtec_status *s,
	    struct switchtec_bwcntr_res *bw_data)
{
	WINDOW *portwin;
	char str[256];
	double bw_val;
	const char *bw_suf;

	portwin = newwin(WINPORTSIZE, portlocs->starty, portlocs->startx);
	wborder(portwin, WINBORDER);
	portid_str(&str[0], &s->port);
	mvwaddstr(portwin, 1, 1, &str[0]);

	sprintf(&str[0], "Link %s", s->link_up ? "UP" : "DOWN");
	mvwaddstr(portwin, 2, 1, &str[0]);

	sprintf(&str[0], "%s-x%d", s->ltssm_str, s->cfg_lnk_width);
	mvwaddstr(portwin, 3, 1, &str[0]);
	if (!s->link_up)
		goto out;

	sprintf(&str[0], "x%d-Gen%d - %g GT/s",
		s->neg_lnk_width, s->link_rate,
		switchtec_gen_transfers[s->link_rate]);
	mvwaddstr(portwin, 4, 1, &str[0]);

	bw_val = switchtec_bwcntr_tot(&bw_data->egress);
	bw_suf = suffix_si_get(&bw_val);
	sprintf(&str[0], "%-.3g %sB", bw_val, bw_suf);
	mvwaddstr(portwin, 6, 1, &str[0]);

/*
    bw_val = switchtec_bwcntr_tot(&bw_data[p].ingress);
    bw_suf = suffix_si_get(&bw_val);
    mvwaddstr("\tIn Bytes:        \t%-.3g %sB\n", bw_val, bw_suf);
*/
out:
	wrefresh(portwin);
	return portwin;
}

  /* Main GUI window. */

int
gui_main(struct switchtec_dev *dev)
{

	WINDOW *mainwin;

	if ((mainwin = initscr()) == NULL) {
		fprintf(stderr, "Error initialising ncurses.\n");
		exit(EXIT_FAILURE);
	}
	wborder(mainwin, WINBORDER);
	wrefresh(mainwin);

	int ret;
	struct switchtec_status *status;
	struct switchtec_bwcntr_res bw_data[SWITCHTEC_MAX_PORTS];
	int port_ids[SWITCHTEC_MAX_PORTS];

	ret = switchtec_status(dev, &status);
	if (ret < 0) {
		switchtec_perror("status");
		return ret;
	}

	unsigned p;
	struct portloc portlocs[ret];

	get_portlocs(portlocs, status, ret);

	for (p = 0; p < ret; p++)
		port_ids[p] = status[p].port.phys_id;

	ret = switchtec_bwcntr_many(dev, ret, port_ids, 0, bw_data);
	if (ret < 0) {
		switchtec_perror("bwcntr");
		free(status);
		return ret;
	}

	for (p = 0; p < ret; p++) {
		struct switchtec_status *s = &status[p];
		gui_portwin(&portlocs[p], s, &bw_data[p]);
		sleep(0.1);
	}

	/*  Clean up after ourselves  */
	sleep(10);
	delwin(mainwin);
	endwin();
	refresh();

	return EXIT_SUCCESS;

	return 0;
}

/*
 * Microsemi Switchtec(tm) PCIe Management Command Line Interface
 * Copyright (c) 2021, Microsemi Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "config.h"
#include "commands.h"
#include "argconfig.h"
#include "common.h"
#include "progress.h"
#include "graph.h"

#include <switchtec/switchtec.h>
#include <switchtec/utils.h>

#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

struct diag_common_cfg {
	struct switchtec_dev *dev;
	struct switchtec_status port;
	enum switchtec_diag_end end;
	enum switchtec_diag_link link;
	int port_id;
	int far_end;
	int prev;
};

#define DEFAULT_DIAG_COMMON_CFG {	\
	.port_id = -1,			\
}

#define PORT_OPTION {							\
	"port", 'p', "PORT_ID", CFG_NONNEGATIVE, &cfg.port_id,		\
	required_argument, "physical port ID to dump data for",		\
}
#define FAR_END_OPTION {						\
	"far-end", 'f', "", CFG_NONE, &cfg.far_end, no_argument,	\
	"get the far-end coefficients instead of the local ones",	\
}
#define PREV_OPTION {							\
	"prev", 'P', "", CFG_NONE, &cfg.prev, no_argument,		\
	"return the data for the previous link",			\
}

static int get_port(struct switchtec_dev *dev, int port_id,
		    struct switchtec_status *port)
{
	struct switchtec_status *status;
	int i, ports;

	ports = switchtec_status(dev, &status);
	if (ports < 0) {
		switchtec_perror("status");
		return ports;
	}

	for (i = 0; i < ports; i++) {
		if (status[i].port.phys_id == port_id ||
		    (port_id == -1 && status[i].port.upstream)) {
			*port = status[i];
			switchtec_status_free(status, ports);
			return 0;
		}
	}

	fprintf(stderr, "Invalid physical port id: %d\n", port_id);
	switchtec_status_free(status, ports);
	return -1;
}

static int diag_parse_common_cfg(int argc, char **argv, const char *desc,
				 struct diag_common_cfg *cfg,
				 const struct argconfig_options *opts)
{
	int ret;

	argconfig_parse(argc, argv, desc, opts, cfg, sizeof(*cfg));

	ret = get_port(cfg->dev, cfg->port_id, &cfg->port);
	if (ret)
		return ret;

	cfg->port_id = cfg->port.port.phys_id;

	if (cfg->far_end)
		cfg->end = SWITCHTEC_DIAG_FAR_END;
	else
		cfg->end = SWITCHTEC_DIAG_LOCAL;

	if (cfg->prev)
		cfg->link = SWITCHTEC_DIAG_LINK_PREVIOUS;
	else
		cfg->link = SWITCHTEC_DIAG_LINK_CURRENT;

	return 0;
}

#define CMD_DESC_LTSSM_LOG "Display LTSSM log"
static int ltssm_log(int argc, char **argv) {
	struct diag_common_cfg cfg = DEFAULT_DIAG_COMMON_CFG;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION, PORT_OPTION, {}
	};

	int ret;
	int port;
	int i;

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_LTSSM_LOG,
				    &cfg, opts);
	if (ret)
		return ret;
	
	int log_count = 512;
	if (switchtec_is_gen4(cfg.dev))
		log_count = 128;
	
	struct switchtec_diag_ltssm_log output[log_count];

	if (switchtec_is_gen3(cfg.dev)) {
		fprintf (stderr,
			 "This command is not supported on Gen3 devices\n");
		return 0;
	}
	port = cfg.port_id;
	ret = switchtec_diag_ltssm_log(cfg.dev, port, &log_count, output);
	if (ret) {
		switchtec_perror("ltssm_log");
		return ret;
	}

	printf("LTSSM Log for Physical Port %d (autowrap ON)\n\n", port);
	printf("Idx\tDelta Time\tPCIe Rate\tState\n");
	for(i = 0; i < log_count ; i++) {
		printf("%3d\t", i);
		printf("%09x\t", output[i].timestamp);
		printf("%.1fG\t\t", output[i].link_rate);
		printf("%s\n", switchtec_ltssm_str(output[i].link_state, 1));
	}

	return ret;
}

static const struct argconfig_choice eye_modes[] = {
	{"RAW", SWITCHTEC_DIAG_EYE_RAW,
	 "raw data mode (slow, more accurate)"},
	{"RATIO", SWITCHTEC_DIAG_EYE_RATIO,
	 "ratio data mode (faster, less accurate)"},
	{}
};

enum output_format {
	FMT_CSV,
	FMT_TEXT,
	FMT_CURSES,
};

static const struct argconfig_choice output_fmt_choices[] = {
#if defined(HAVE_LIBCURSES) || defined(HAVE_LIBNCURSES)
	#define FMT_DEFAULT FMT_CURSES
	#define FMT_DEFAULT_STR "curses"
	{"curses", FMT_CURSES, "Display data in a curses scrollable window"},
#else
	#define FMT_DEFAULT FMT_TEXT
	#define FMT_DEFAULT_STR "text"
#endif
	{"text", FMT_TEXT, "Display data in a simplified text format"},
	{"csv", FMT_CSV, "Raw Data in CSV format"},
};

static double *load_eye_csv(FILE *f, struct range *X, struct range *Y,
			    char *title, size_t title_sz, int *interval)
{
	size_t pixel_cnt;
	char line[2000];
	double *pixels;
	int i = 0, y;
	char *tok;
	int rc;

	tok = fgets(title, title_sz, f);
	if (!tok)
		return NULL;

	if (title[strlen(title) - 1] == '\n')
		title[strlen(title) - 1] = 0;

	/* Parse the header line for the X range */
	tok = fgets(line, sizeof(line), f);
	if (!tok)
		return NULL;

	rc = sscanf(line, "interval_ms, %d\n", interval);
	if (rc == 1) {
		tok = fgets(line, sizeof(line), f);
		if (!tok)
			return NULL;
	}

	tok = strtok(line, ",");
	if (!tok)
		return NULL;
	X->start = atoi(tok);
	if (X->start < 0 || X->start > 63)
		return NULL;

	tok = strtok(NULL, ",");
	if (!tok)
		return NULL;
	X->end = atoi(tok);
	if (X->start < 0 || X->start > 63)
		return NULL;

	X->step = X->end - X->start;
	if (X->step <= 0)
		return NULL;

	while ((tok = strtok(NULL, ",")))
		X->end = atoi(tok);

	/* Parse the first column for the Y range */
	tok = fgets(line, sizeof(line), f);
	if (!tok)
		return NULL;
	Y->start = atoi(line);
	if (Y->start < -255 || Y->start > 255)
		return NULL;

	tok = fgets(line, sizeof(line), f);
	if (!tok)
		return NULL;
	Y->end = atoi(line);
	if (Y->end < -255 || Y->end > 255)
		return NULL;

	Y->step = Y->end - Y->start;
	if (Y->step <= 0)
		return NULL;

	while ((tok = fgets(line, sizeof(line), f)))
		Y->end = atoi(line);

	rewind(f);

	/* Load the data */
	pixel_cnt = RANGE_CNT(X);
	pixel_cnt *= RANGE_CNT(Y);
	pixels = calloc(pixel_cnt, sizeof(*pixels));
	if (!pixels) {
		perror("allocating pixels");
		return NULL;
	}

	/* Read the Title line */
	tok = fgets(line, sizeof(line), f);
	if (!tok)
		goto out_err;

	if (rc == 1) {
		/* Read the Interval line */
		tok = fgets(line, sizeof(line), f);
		if (!tok)
			goto out_err;
	}

	/* Read the Header line */
	tok = fgets(line, sizeof(line), f);
	if (!tok)
		goto out_err;

	for (y = 0; y < RANGE_CNT(Y); y++) {
		if (i != RANGE_CNT(X) * y)
			goto out_err;

		tok = fgets(line, sizeof(line), f);
		if (!tok)
			goto out_err;

		tok = strtok(line, ",");
		if (!tok)
			goto out_err;

		while ((tok = strtok(NULL, ","))) {
			if (i >= pixel_cnt)
				goto out_err;
			pixels[i++] = strtod(tok, NULL);
		}
	}

	return pixels;

out_err:
	free(pixels);
	return NULL;
}

static void print_eye_csv(FILE *f, struct range *X, struct range *Y,
			  double *pixels, const char *title, int interval)
{
	size_t stride = RANGE_CNT(X);
	int x, y, i, j = 0;

	fprintf(f, "%s\n", title);
	fprintf(f, "interval_ms, %d\n", interval);

	for_range(x, X)
		fprintf(f, ", %d", x);
	fprintf(f, "\n");

	for_range(y, Y) {
		fprintf(f, "%d", y);
		i = 0;
		for_range(x, X)  {
			fprintf(f, ", %e", pixels[j * stride + i]);
			i++;
		}
		fprintf(f, "\n");
		j++;
	}
}

static void eye_set_title(char *title, int port, int lane, int gen)
{
	sprintf(title, "Eye Observation, Port %d, Lane %d, Gen %d",
		port, lane, gen);
}

static void write_eye_csv_files(int port_id, int lane_id, int num_lanes,
				int interval_ms, int gen, struct range *X,
				struct range *Y, double *pixels)
{
	int stride = RANGE_CNT(X) * RANGE_CNT(Y);
	char title[128], fname[128];
	FILE *f;
	int l;

	for (l = 0; l < num_lanes; l++) {
		eye_set_title(title, port_id, lane_id + l, gen);

		snprintf(fname, sizeof(fname), "eye_port%d_lane%d.csv",
			 port_id, lane_id + l);
		f = fopen(fname, "w");
		if (!f) {
			fprintf(stderr, "Unable to write CSV file '%s': %m\n",
				fname);
			continue;
		}

		print_eye_csv(f, X, Y, &pixels[l * stride], title, interval_ms);
		fclose(f);

		fprintf(stderr, "Wrote %s\n", fname);
	}
}

static void eye_graph_data(struct range *X, struct range *Y, double *pixels,
			   int *data, int *shades)
{
	size_t pixel_cnt = RANGE_CNT(X) * RANGE_CNT(Y);
	int i, val;

	for (i = 0; i < pixel_cnt; i++) {
		if (pixels[i] == 0) {
			data[i] = '.';
			shades[i] = 0;
		} else {
			val = ceil(-log10(pixels[i]));
			if (val >= 9)
				val = 9;
			data[i] = '0' + val;
			shades[i] = GRAPH_SHADE_MAX - val - 3;
		}
	}
}

struct crosshair_chars {
	int hline, vline, plus;
};

static const int ch_left = 28, ch_right = 36;

static void crosshair_plot(struct range *X, struct range *Y, int *data,
			   int *shades, struct switchtec_diag_cross_hair *ch,
			   const struct crosshair_chars *chars)
{
	size_t stride = RANGE_CNT(X);
	int i, j, start, end;

	if (ch->eye_right_lim != INT_MAX) {
		if (ch->eye_left_lim != INT_MAX)
			start = RANGE_TO_IDX(X, ch->eye_left_lim);
		else
			start = RANGE_TO_IDX(X, 31);
		end =  RANGE_TO_IDX(X, ch->eye_right_lim);
		j = RANGE_TO_IDX(Y, 0) * stride;

		for (i = start; i < end; i++) {
			data[j + i] = chars->hline;
			shades[j + i] |= GRAPH_SHADE_HIGHLIGHT;
		}
	}

	if (ch->eye_top_left_lim != INT_MAX) {
		j = RANGE_TO_IDX(X, ch_left);
		if (ch->eye_bot_left_lim != INT_MAX)
			start = RANGE_TO_IDX(Y, ch->eye_bot_left_lim);
		else
			start = RANGE_TO_IDX(Y, 0);
		end = RANGE_TO_IDX(Y, ch->eye_top_left_lim);

		for (i = start; i < end; i++) {
			data[i * stride + j] = chars->vline;
			shades[i * stride + j] |= GRAPH_SHADE_HIGHLIGHT;
		}

		data[RANGE_TO_IDX(Y, 0) * stride + ch_left] = chars->plus;
	}

	if (ch->eye_top_right_lim != INT_MAX) {
		j = RANGE_TO_IDX(X, ch_right);
		if (ch->eye_bot_right_lim != INT_MAX)
			start = RANGE_TO_IDX(Y, ch->eye_bot_right_lim);
		else
			start = RANGE_TO_IDX(Y, 0);
		end = RANGE_TO_IDX(Y, ch->eye_top_right_lim);

		for (i = start; i < end; i++) {
			data[i * stride + j] = chars->vline;
			shades[i * stride + j] |= GRAPH_SHADE_HIGHLIGHT;
		}

		data[RANGE_TO_IDX(Y, 0) * stride + ch_right] = chars->plus;
	}
}

static int crosshair_w2h(struct switchtec_diag_cross_hair *ch)
{
	return (ch->eye_right_lim - ch->eye_left_lim) *
		(ch->eye_top_right_lim - ch->eye_bot_right_lim +
		 ch->eye_top_left_lim - ch->eye_bot_right_lim);
}

struct crosshair_anim_data {
	struct switchtec_dev *dev;
	struct switchtec_diag_cross_hair ch_int;
	const struct crosshair_chars *chars;
	int last_data, last_shade;
	int last_x_pos, last_y_pos;
	int lane;

	double *pixels;
	int eye_interval;
};

static void crosshair_set_status(struct crosshair_anim_data *cad,
				 struct switchtec_diag_cross_hair *ch,
				 char *status)
{
	switch (ch->state) {
	case SWITCHTEC_DIAG_CROSS_HAIR_FIRST_ERROR_RIGHT:
		sprintf(status, "First Error Right           (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_ERROR_FREE_RIGHT:
		sprintf(status, "Error Free Right            (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_FINAL_RIGHT:
		sprintf(status, "Final Right                 (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_FIRST_ERROR_LEFT:
		sprintf(status, "First Error Left            (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_ERROR_FREE_LEFT:
		sprintf(status, "Error Free Left             (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_FINAL_LEFT:
		sprintf(status, "Final Left                  (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_FIRST_ERROR_TOP_RIGHT:
		sprintf(status, "First Error Top Right       (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_ERROR_FREE_TOP_RIGHT:
		sprintf(status, "Error Free Top Right        (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_FINAL_TOP_RIGHT:
		sprintf(status, "Final Top Right             (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_FIRST_ERROR_BOT_RIGHT:
		sprintf(status, "First Error Bottom Right    (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_ERROR_FREE_BOT_RIGHT:
		sprintf(status, "Error Free Bottom Right     (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_FINAL_BOT_RIGHT:
		sprintf(status, "Final Bottom Right          (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_FIRST_ERROR_TOP_LEFT:
		sprintf(status, "First Error Top Left        (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_ERROR_FREE_TOP_LEFT:
		sprintf(status, "Error Free Top Left         (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_FINAL_TOP_LEFT:
		sprintf(status, "Final Top Left              (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_FIRST_ERROR_BOT_LEFT:
		sprintf(status, "First Error Bottom Left     (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_ERROR_FREE_BOT_LEFT:
		sprintf(status, "Error Free Bottom Left      (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_FINAL_BOT_LEFT:
		sprintf(status, "Final Bottom Left           (%d, %d)",
			ch->x_pos, ch->y_pos);
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_DONE:
		if (cad && cad->pixels) {
			sprintf(status,
				"Done    W2H=%d   Dwell Time: crosshair=200ms, eye=%dms",
				crosshair_w2h(ch), cad->eye_interval);
		} else {
			sprintf(status,
				"Done    W2H=%d   Dwell Time: crosshair=200ms",
				crosshair_w2h(ch));
		}
		break;
	case SWITCHTEC_DIAG_CROSS_HAIR_ERROR:
		sprintf(status, "Error Occurred");
		break;
	default:
		strcpy(status, "");
		break;
	}
}

static int crosshair_anim_step(struct range *X, struct range *Y, int *data,
		int *shades, char *status, bool *redraw, void *opaque)
{
	struct switchtec_diag_cross_hair ch = {};
	struct crosshair_anim_data *cad = opaque;
	size_t stride = RANGE_CNT(X);
	int x, y, i;
	int ret;

	usleep(100000);

	ret = switchtec_diag_cross_hair_get(cad->dev, cad->lane, 1, &ch);
	if (ret) {
		switchtec_perror("Unable to get cross hair");
		return -1;
	}

	if (ch.state == SWITCHTEC_DIAG_CROSS_HAIR_ERROR) {
		fprintf(stderr, "Error in cross hair: previous state: %d\n",
			ch.prev_state);
		return -1;
	}

	if (ch.state <= SWITCHTEC_DIAG_CROSS_HAIR_WAITING)
		return 0;

	if (cad->last_x_pos >= 0 && cad->last_y_pos >= 0) {
		i = cad->last_y_pos * stride + cad->last_x_pos;
		data[i] = cad->last_data;
		shades[i] = cad->last_shade;

		cad->last_x_pos = -1;
		cad->last_y_pos = -1;
	}

	if (ch.state < SWITCHTEC_DIAG_CROSS_HAIR_DONE) {
		x = RANGE_TO_IDX(X, ch.x_pos);
		y = RANGE_TO_IDX(Y, ch.y_pos);

		cad->last_data = data[y * stride + x];
		cad->last_shade = shades[y * stride + x];

		data[y * stride + x] = 'X';
		shades[y * stride + x] |= GRAPH_SHADE_HIGHLIGHT;

		cad->last_x_pos = x;
		cad->last_y_pos = y;
		*redraw = true;

		if (cad->ch_int.state != ch.state)
			crosshair_plot(X, Y, data, shades, &cad->ch_int,
				       cad->chars);
		cad->ch_int.state = ch.state;

		switch (ch.state) {
		case SWITCHTEC_DIAG_CROSS_HAIR_FINAL_RIGHT:
			cad->ch_int.eye_right_lim = ch.x_pos;
			break;
		case SWITCHTEC_DIAG_CROSS_HAIR_FINAL_LEFT:
			cad->ch_int.eye_left_lim = ch.x_pos;
			break;
		case SWITCHTEC_DIAG_CROSS_HAIR_FINAL_TOP_RIGHT:
			cad->ch_int.eye_top_right_lim = ch.y_pos;
			break;
		case SWITCHTEC_DIAG_CROSS_HAIR_FINAL_BOT_RIGHT:
			cad->ch_int.eye_bot_right_lim = ch.y_pos;
			break;
		case SWITCHTEC_DIAG_CROSS_HAIR_FINAL_TOP_LEFT:
			cad->ch_int.eye_top_left_lim = ch.y_pos;
			break;
		case SWITCHTEC_DIAG_CROSS_HAIR_FINAL_BOT_LEFT:
			cad->ch_int.eye_bot_left_lim = ch.y_pos;
			break;
		default:
			break;
		}
	} else if (ch.state == SWITCHTEC_DIAG_CROSS_HAIR_DONE) {
		crosshair_plot(X, Y, data, shades, &ch, cad->chars);
		*redraw = true;
	}

	crosshair_set_status(cad, &ch, status);
	return ch.state >= SWITCHTEC_DIAG_CROSS_HAIR_DONE;
}

static void crosshair_init_pixels(struct range *X, struct range *Y, int *data,
				  int *shades, double *pixels)
{
	size_t pixel_cnt = RANGE_CNT(X) * RANGE_CNT(Y);
	int i;

	if (!pixels) {
		for (i = 0; i < pixel_cnt; i++) {
			data[i] = '.';
			shades[i] = 0;
		}
	} else {
		eye_graph_data(X, Y, pixels, data, shades);
	}
}

static int crosshair_graph(struct switchtec_dev *dev,
		struct switchtec_diag_cross_hair *ch, struct range *X,
		struct range *Y, int lane, double *pixels, const char *title,
		int eye_interval)
{
	struct crosshair_chars chars_curses;
	struct crosshair_anim_data cad = {
		.dev = dev,
		.lane = lane,
		.chars = &chars_curses,
		.last_x_pos = -1,
		.last_y_pos = -1,
		.pixels = pixels,
		.eye_interval = eye_interval,
		.ch_int = {
			.eye_left_lim = INT_MAX,
			.eye_right_lim = INT_MAX,
			.eye_bot_left_lim = INT_MAX,
			.eye_bot_right_lim = INT_MAX,
			.eye_top_left_lim = INT_MAX,
			.eye_top_right_lim = INT_MAX,
		},
	};
	size_t pixel_cnt = RANGE_CNT(X) * RANGE_CNT(Y);
	int data[pixel_cnt], shades[pixel_cnt];
	char status[100] = "";

	graph_init();
	chars_curses.hline = GRAPH_HLINE;
	chars_curses.vline = GRAPH_VLINE;
	chars_curses.plus = GRAPH_PLUS;

	crosshair_init_pixels(X, Y, data, shades, pixels);

	if (ch) {
		crosshair_plot(X, Y, data, shades, ch, &chars_curses);
		if (pixels)
			sprintf(status,
				" W2H=%d   Dwell Time: crosshair=200ms, eye=%dms",
				crosshair_w2h(ch), eye_interval);
		else
			sprintf(status,
				" W2H=%d   Dwell Time: crosshair=200ms",
				crosshair_w2h(ch));

		return graph_draw_win(X, Y, data, shades, title, 'T', 'V',
				      status, NULL, NULL);
	} else {
		return graph_draw_win(X, Y, data, shades, title, 'T', 'V',
				      status, crosshair_anim_step, &cad);
	}
}

static int crosshair_capture(struct switchtec_dev *dev, int lane,
		struct switchtec_diag_cross_hair *ch, const char *title)
{
	int ret, i, num_lanes, l, n;
	char status[100];

	fprintf(stderr, "Capturing %s\n", title);

	if (lane == SWITCHTEC_DIAG_CROSS_HAIR_ALL_LANES) {
		lane = 0;
		num_lanes = SWITCHTEC_MAX_LANES;
	} else {
		num_lanes = 1;
	}

	while (true) {
		usleep(100000);

		for (l = 0; l < num_lanes;
		     l += SWITCHTEC_DIAG_CROSS_HAIR_MAX_LANES) {
			n = num_lanes - l;
			if (n > SWITCHTEC_DIAG_CROSS_HAIR_MAX_LANES)
				n = SWITCHTEC_DIAG_CROSS_HAIR_MAX_LANES;

			ret = switchtec_diag_cross_hair_get(dev, lane + l, n,
							    ch + l);
			if (ret) {
				switchtec_perror("Unable to get cross hair");
				return -1;
			}
		}

		for (i = 0; i < num_lanes; i++) {
			if (ch[i].state == SWITCHTEC_DIAG_CROSS_HAIR_DISABLED ||
			    ch[i].state == SWITCHTEC_DIAG_CROSS_HAIR_DONE)
				continue;
			crosshair_set_status(NULL, &ch[i], status);
			fprintf(stderr, "\rLane %-2d  %-60s\r",
				ch[i].lane_id, status);
		}

		for (i = 0; i < num_lanes; i++) {
			if (ch[i].state == SWITCHTEC_DIAG_CROSS_HAIR_ERROR) {
				crosshair_set_status(NULL, &ch[i], status);
				fprintf(stderr, "\rLane %-2d  %-60s\n",
					ch[i].lane_id, status);
				return -1;
			}
		}

		for (i = 0; i < num_lanes; i++) {
			if (ch[i].state != SWITCHTEC_DIAG_CROSS_HAIR_DISABLED &&
			    ch[i].state != SWITCHTEC_DIAG_CROSS_HAIR_DONE)
				break;
		}

		if (i == num_lanes)
			break;
	}

	fprintf(stderr, "\r%-60s\r", "");
	return 0;
}

static const struct crosshair_chars *crosshair_text_chars(void)
{
	static const struct crosshair_chars crosshair_chars_utf8 = {
		.hline = GRAPH_TEXT_HLINE,
		.vline = GRAPH_TEXT_VLINE,
		.plus = GRAPH_TEXT_PLUS,
	};
	static const struct crosshair_chars crosshair_chars_text = {
		.hline = '-',
		.vline = '|',
		.plus = '+',
	};
	const char *locale;

	locale = setlocale(LC_ALL, "");
	if (locale && strstr(locale, "UTF-8"))
		return &crosshair_chars_utf8;
	else
		return &crosshair_chars_text;
}

static int crosshair_text(struct switchtec_diag_cross_hair *ch,
			  struct range *X, struct range *Y, double *pixels,
			  const char *title, int eye_interval)
{
	size_t pixel_cnt = RANGE_CNT(X) * RANGE_CNT(Y);
	int data[pixel_cnt], shades[pixel_cnt];

	crosshair_init_pixels(X, Y, data, shades, pixels);
	crosshair_plot(X, Y, data, shades, ch, crosshair_text_chars());
	graph_draw_text(X, Y, data, title, 'T', 'V');
	if (pixels)
		printf("\n       W2H=%d   Dwell Time: crosshair=200ms, eye=%dms\n",
		       crosshair_w2h(ch), eye_interval);
	else
		printf("\n       W2H=%d   Dwell Time: crosshair=200ms\n",
		       crosshair_w2h(ch));

	return 0;
}

static void crosshair_csv(FILE *f, struct switchtec_diag_cross_hair *ch,
			  const char *title)
{
	fprintf(f, "%s\n", title);
	fprintf(f, ", T, V\n");
	fprintf(f, "left_limit, %d, %d\n", ch->eye_left_lim, 0);
	fprintf(f, "right_limit, %d, %d\n", ch->eye_right_lim, 0);
	fprintf(f, "top_left_limit, %d, %d\n", ch_left, ch->eye_top_left_lim);
	fprintf(f, "bottom_left_limit, %d, %d\n", ch_left,
		ch->eye_bot_left_lim);
	fprintf(f, "top_right_limit, %d, %d\n", ch_right,
		ch->eye_top_right_lim);
	fprintf(f, "bottom_right_limit, %d, %d\n", ch_right,
		ch->eye_bot_right_lim);
	fprintf(f, "interval_ms, 200\n");
	fprintf(f, "w2h, %d\n", crosshair_w2h(ch));
}

static void crosshair_set_title(char *title, int port, int lane, int gen)
{
	sprintf(title, "Crosshair - Port %d, Lane %d, Gen %d",
		port, lane, gen);
}

static void crosshair_write_all_csv(struct switchtec_dev *dev,
				    struct switchtec_diag_cross_hair *ch)
{
	struct switchtec_status status;
	char fname[100], title[100];
	int i, port, lane, rc;
	FILE *f;

	for (i = 0; i < SWITCHTEC_MAX_LANES; i++) {
		if (ch[i].state != SWITCHTEC_DIAG_CROSS_HAIR_DONE)
			continue;

		rc = switchtec_calc_port_lane(dev, ch[i].lane_id, &port, &lane,
					      &status);
		if (rc) {
			fprintf(stderr,
				"Unable to get port information for lane: %d\n",
				ch[i].lane_id);
			continue;
		}

		snprintf(fname, sizeof(fname), "crosshair_port%d_lane%d.csv",
			 port, lane);

		f = fopen(fname, "w");
		if (!f) {
			fprintf(stderr, "Unable to write '%s': %m\n", fname);
			continue;
		}

		crosshair_set_title(title, port, lane, status.link_rate);
		crosshair_csv(f, &ch[i], title);
		fclose(f);
		fprintf(stderr, "Wrote %s\n", fname);
	}
}

static int crosshair_write_csv(const char *title,
			       struct switchtec_diag_cross_hair *ch)
{
	int port, lane, gen;
	char fname[100];
	FILE *f;

	sscanf(title, "Crosshair - Port %d, Lane %d, Gen %d",
	       &port, &lane, &gen);

	snprintf(fname, sizeof(fname), "crosshair_port%d_lane%d.csv",
		 port, lane);

	f = fopen(fname, "w");
	if (!f) {
		fprintf(stderr, "Unable to write '%s': %m\n", fname);
		return -1;
	}

	crosshair_csv(f, ch, title);
	fclose(f);
	fprintf(stderr, "Wrote %s\n", fname);

	return 0;
}

static int load_crosshair_csv(FILE *f, struct switchtec_diag_cross_hair *ch,
			      char *title, size_t title_sz)
{
	char *line;
	int x, ret;

	line = fgets(title, title_sz, f);
	if (!line)
		return 1;
	if (title[strlen(title) - 1] == '\n')
		title[strlen(title) - 1] = 0;

	ret = fscanf(f, ", %lc, %lc\n", (wchar_t *)&x, (wchar_t *)&x);
	if (ret != 2 || x != 'V')
		return 1;

	ret = fscanf(f, "left_limit, %d, 0\n", &ch->eye_left_lim);
	if (ret != 1)
		return 1;

	ret = fscanf(f, "right_limit, %d, 0\n", &ch->eye_right_lim);
	if (ret != 1)
		return 1;

	ret = fscanf(f, "top_left_limit, %d, %d\n", &x, &ch->eye_top_left_lim);
	if (ret != 2 || x != ch_left)
		return 1;

	ret = fscanf(f, "bottom_left_limit, %d, %d\n", &x,
		     &ch->eye_bot_left_lim);
	if (ret != 2 || x != ch_left)
		return 1;

	ret = fscanf(f, "top_left_limit, %d, %d\n", &x,
		     &ch->eye_top_right_lim);
	if (ret != 2 || x != ch_right)
		return 1;

	ret = fscanf(f, "bottom_left_limit, %d, %d\n", &x,
		     &ch->eye_bot_right_lim);
	if (ret != 2 || x != ch_right)
		return 1;

	return 0;
}

#define CMD_DESC_CROSS_HAIR "Measure Eye Cross Hair"

static int crosshair(int argc, char **argv)
{
	struct switchtec_diag_cross_hair ch[SWITCHTEC_MAX_LANES] = {};
	struct switchtec_diag_cross_hair *ch_ptr = NULL;
	struct switchtec_status status;
	double *pixels = NULL;
	char title[128], subtitle[50];
	int eye_interval = 1;
	int ret, lane = -1;

	static struct {
		int all;
		int fmt;
		struct switchtec_dev *dev;
		int port_id;
		int lane_id;
		struct range x_range;
		struct range y_range;
		FILE *plot_file;
		const char *plot_filename;
		FILE *crosshair_file;
		const char *crosshair_filename;
	} cfg = {
		.fmt = FMT_DEFAULT,
		.port_id = -1,
		.lane_id = 0,
		.x_range.start = 0,
		.x_range.end = 63,
		.x_range.step = 1,
		.y_range.start = -255,
		.y_range.end = 255,
		.y_range.step = 5,
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_OPTIONAL,
		{"all", 'a', "", CFG_NONE, &cfg.all, no_argument,
		 "capture all lanes, format must be csv"},
		{"crosshair", 'C', "FILE", CFG_FILE_R, &cfg.crosshair_file,
		 required_argument,
		 "load crosshair data from a previously saved file"},
		{"format", 'f', "FMT", CFG_CHOICES, &cfg.fmt, required_argument,
		 "output format (default: " FMT_DEFAULT_STR ")",
		 .choices=output_fmt_choices},
		{"lane", 'l', "LANE_ID", CFG_NONNEGATIVE, &cfg.lane_id,
		 required_argument, "lane id within the port to observe"},
		{"port", 'p', "PORT_ID", CFG_NONNEGATIVE, &cfg.port_id,
		 required_argument, "physical port ID to observe"},
		{"plot", 'P', "FILE", CFG_FILE_R, &cfg.plot_file,
		 required_argument,
		 "optionally, plot a CSV file from an eye capture as the background"},
		{"t-start", 't', "NUM", CFG_NONNEGATIVE, &cfg.x_range.start,
		 required_argument, "start time (0 to 63)"},
		{"t-end", 'T', "NUM", CFG_NONNEGATIVE, &cfg.x_range.end,
		 required_argument, "end time (t-start to 63)"},
		{"t-step", 's', "NUM", CFG_NONNEGATIVE, &cfg.x_range.step,
		 required_argument, "time step (default 1)"},
		{"v-start", 'v', "NUM", CFG_INT, &cfg.y_range.start,
		 required_argument, "start voltage (-255 to 255)"},
		{"v-end", 'V', "NUM", CFG_INT, &cfg.y_range.end,
		 required_argument, "end voltage (v-start to 255)"},
		{"v-step", 'S', "NUM", CFG_NONNEGATIVE, &cfg.y_range.step,
		 required_argument, "voltage step (default: 5)"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_CROSS_HAIR, opts, &cfg,
			sizeof(cfg));

	if (cfg.plot_file) {
		pixels = load_eye_csv(cfg.plot_file, &cfg.x_range,
				&cfg.y_range, subtitle, sizeof(subtitle),
				&eye_interval);
		if (!pixels) {
			fprintf(stderr, "Unable to parse CSV file: %s\n",
				cfg.plot_filename);
			return -1;
		}
	}

	if (cfg.crosshair_file) {
		ret = load_crosshair_csv(cfg.crosshair_file, ch, subtitle,
					 sizeof(subtitle));
		if (ret) {
			fprintf(stderr, "Unable to parse crosshair CSV file: %s\n",
				cfg.crosshair_filename);
			return -1;
		}

		ch_ptr = ch;

		if (pixels)
			snprintf(title, sizeof(title) - 1, "%s (%s / %s)",
				 subtitle, cfg.crosshair_filename,
				 cfg.plot_filename);
		else
			snprintf(title, sizeof(title) - 1, "%s (%s)",
				 subtitle, cfg.crosshair_filename);

	} else {
		if (!cfg.dev) {
			fprintf(stderr,
				"Must specify a switchtec device if not using -C\n");
			return -1;
		}

		if (cfg.all) {
			if (cfg.lane_id) {
				fprintf(stderr,
					"Must not specify both --all/-a and --lane/-l\n");
				return -1;
			}

			if (cfg.fmt != FMT_CSV) {
				fprintf(stderr,
					"Must use --format=CSV with --all/-a\n");
				return -1;
			}
		} else if (cfg.port_id < 0) {
			fprintf(stderr, "Must specify a port ID with --port/-p\n");
			return -1;
		}

		if (!cfg.all) {
			lane = switchtec_calc_lane_id(cfg.dev, cfg.port_id,
						      cfg.lane_id, &status);
			if (lane < 0) {
				switchtec_perror("Invalid lane");
				return -1;
			}

			crosshair_set_title(subtitle, cfg.port_id, cfg.lane_id,
					    status.link_rate);

		} else {
			lane = SWITCHTEC_DIAG_CROSS_HAIR_ALL_LANES;
			snprintf(subtitle, sizeof(subtitle) - 1,
				 "Crosshair - All Lanes");
		}

		if (pixels)
			snprintf(title, sizeof(title) - 1, "%s (%s)",
				 subtitle, cfg.plot_filename);
		else
			snprintf(title, sizeof(title) - 1, "%s", subtitle);

		switchtec_diag_cross_hair_disable(cfg.dev);

		ret = switchtec_diag_cross_hair_enable(cfg.dev, lane);
		if (ret) {
			switchtec_perror("Unable to enable cross hair");
			goto out;
		}

		if (cfg.fmt != FMT_CURSES) {
			ret = crosshair_capture(cfg.dev, lane, ch, title);
			if (ret)
				return ret;
		}
	}

	switch (cfg.fmt) {
	case FMT_CURSES:
		ret = crosshair_graph(cfg.dev, ch_ptr, &cfg.x_range,
				      &cfg.y_range, lane, pixels, title,
				      eye_interval);
		break;
	case FMT_TEXT:
		ret = crosshair_text(ch, &cfg.x_range, &cfg.y_range,
				     pixels, title,
				     eye_interval);
		break;
	case FMT_CSV:
		if (cfg.all)
			crosshair_write_all_csv(cfg.dev, ch);
		else
			crosshair_write_csv(subtitle, ch);
		break;
	}

out:
	free(pixels);
	return ret;
}

static double *eye_observe_dev(struct switchtec_dev *dev, int port_id,
			       int lane_id, int num_lanes, int mode, int interval,
			       struct range *X, struct range *Y, int *gen)
{
	size_t stride = RANGE_CNT(X) * RANGE_CNT(Y);
	size_t pixel_cnt = stride * num_lanes;
	struct switchtec_status status;
	int i, ret, first_lane, lane;
	size_t lane_cnt[num_lanes];
	int lane_mask[4] = {};
	double tmp[500];
	double *pixels;

	ret = switchtec_calc_lane_mask(dev, port_id, lane_id, num_lanes,
				       lane_mask, &status);
	if (ret < 0) {
		switchtec_perror("Invalid lane");
		return NULL;
	}

	for (i = 0; i < 4; i++) {
		first_lane = ffs(lane_mask[i]);
		if (first_lane)
			break;
	}

	pixels = calloc(pixel_cnt, sizeof(*pixels));
	if (!pixels) {
		perror("allocating pixels");
		return NULL;
	}

	switchtec_diag_eye_cancel(dev);

	ret = switchtec_diag_eye_set_mode(dev, mode);
	if (ret) {
		switchtec_perror("eye_set_mode");
		goto out_err;
	}

	ret = switchtec_diag_eye_start(dev, lane_mask, X, Y, interval);
	if (ret) {
		switchtec_perror("eye_start");
		goto out_err;
	}

	if (num_lanes > 1)
		fprintf(stderr, "Observing Port %d, Lane %d to %d, Gen %d\n",
			port_id, lane_id, lane_id + num_lanes - 1,
			status.link_rate);
	else
		fprintf(stderr, "Observing Port %d, Lane %d, Gen %d\n",
			port_id, lane_id, status.link_rate);

	*gen = status.link_rate;

	memset(lane_cnt, 0, sizeof(lane_cnt));
	progress_start();
	for (i = 0; i < pixel_cnt; i += ret) {
		ret = switchtec_diag_eye_fetch(dev, tmp, ARRAY_SIZE(tmp),
					       &lane);
		if (ret == 0) {
			fprintf(stderr, "No data for specified lane.\n");
			goto out_err;
		}

		if (ret < 0) {
			switchtec_perror("eye_fetch");
			goto out_err;
		}

		if (ret > ARRAY_SIZE(tmp)) {
			fprintf(stderr, "Not enough pixels allocated!\n");
			goto out_err;
		}

		lane -= first_lane;

		if (status.lane_reversal)
			lane = num_lanes - lane - 1;

		memcpy(&pixels[lane * stride + lane_cnt[lane]], tmp,
		       ret * sizeof(double));
		lane_cnt[lane] += ret;

		progress_update_norate(i, pixel_cnt);
	}

	progress_finish(false);
	fprintf(stderr, "\n");
	return pixels;

out_err:
	free(pixels);
	return NULL;
}

static int eye_graph(enum output_format fmt, struct range *X, struct range *Y,
		     double *pixels, const char *title,
		     struct switchtec_diag_cross_hair *ch)
{
	size_t pixel_cnt = RANGE_CNT(X) * RANGE_CNT(Y);
	int data[pixel_cnt], shades[pixel_cnt];
	const struct crosshair_chars *chars;
	struct crosshair_chars chars_curses;
	char status[50], *status_ptr = NULL;

	eye_graph_data(X, Y, pixels, data, shades);

	if (ch) {
		if (fmt == FMT_CURSES) {
			graph_init();
			chars_curses.hline = GRAPH_HLINE;
			chars_curses.vline = GRAPH_VLINE;
			chars_curses.plus = GRAPH_PLUS;
			chars = &chars_curses;
		} else {
			chars = crosshair_text_chars();
		}

		crosshair_plot(X, Y, data, shades, ch, chars);

		sprintf(status, " W2H=%d", crosshair_w2h(ch));
		status_ptr = status;
	}

	if (fmt == FMT_TEXT) {
		graph_draw_text(X, Y, data, title, 'T', 'V');
		if (status_ptr)
			printf("\n      %s\n", status_ptr);
		return 0;
	}

	return graph_draw_win(X, Y, data, shades, title, 'T', 'V',
			      status_ptr, NULL, NULL);
}

#define CMD_DESC_EYE "Capture PCIe Eye Errors"

static int eye(int argc, char **argv)
{
	struct switchtec_diag_cross_hair ch = {}, *ch_ptr = NULL;
	char title[128], subtitle[50];
	double *pixels = NULL;
	int ret, gen;

	static struct {
		struct switchtec_dev *dev;
		int fmt;
		int port_id;
		int lane_id;
		int num_lanes;
		int mode;
		struct range x_range, y_range;
		int step_interval;
		FILE *plot_file;
		const char *plot_filename;
		FILE *crosshair_file;
		const char *crosshair_filename;
	} cfg = {
		.fmt = FMT_DEFAULT,
		.port_id = -1,
		.lane_id = 0,
		.num_lanes = 1,
		.mode = SWITCHTEC_DIAG_EYE_RAW,
		.x_range.start = 0,
		.x_range.end = 63,
		.x_range.step = 1,
		.y_range.start = -255,
		.y_range.end = 255,
		.y_range.step = 5,
		.step_interval = 1,
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_OPTIONAL,
		{"crosshair", 'C', "FILE", CFG_FILE_R, &cfg.crosshair_file,
		 required_argument,
		 "optionally, superimpose a crosshair CSV onto the result"},
		{"format", 'f', "FMT", CFG_CHOICES, &cfg.fmt, required_argument,
		 "output format (default: " FMT_DEFAULT_STR ")",
		 .choices=output_fmt_choices},
		{"lane", 'l', "LANE_ID", CFG_NONNEGATIVE, &cfg.lane_id,
		 required_argument, "lane id within the port to observe"},
		{"mode", 'm', "MODE", CFG_CHOICES, &cfg.mode,
		 required_argument, "data mode for the capture",
		 .choices=eye_modes},
		{"num-lanes", 'n', "NUM", CFG_POSITIVE, &cfg.num_lanes,
		 required_argument,
		 "number of lanes to capture, if greater than one, format must be csv (default: 1)"},
		{"port", 'p', "PORT_ID", CFG_NONNEGATIVE, &cfg.port_id,
		 required_argument, "physical port ID to observe"},
		{"plot", 'P', "FILE", CFG_FILE_R, &cfg.plot_file,
		 required_argument, "plot a CSV file from an earlier capture"},
		{"t-start", 't', "NUM", CFG_NONNEGATIVE, &cfg.x_range.start,
		 required_argument, "start time (0 to 63)"},
		{"t-end", 'T', "NUM", CFG_NONNEGATIVE, &cfg.x_range.end,
		 required_argument, "end time (t-start to 63)"},
		{"t-step", 's', "NUM", CFG_NONNEGATIVE, &cfg.x_range.step,
		 required_argument, "time step (default 1)"},
		{"v-start", 'v', "NUM", CFG_INT, &cfg.y_range.start,
		 required_argument, "start voltage (-255 to 255)"},
		{"v-end", 'V', "NUM", CFG_INT, &cfg.y_range.end,
		 required_argument, "end voltage (v-start to 255)"},
		{"v-step", 'S', "NUM", CFG_NONNEGATIVE, &cfg.y_range.step,
		 required_argument, "voltage step (default: 5)"},
		{"interval", 'i', "NUM", CFG_NONNEGATIVE, &cfg.step_interval,
		 required_argument, "step interval in ms (default: 1ms)"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_EYE, opts, &cfg,
			sizeof(cfg));

	if (cfg.crosshair_file) {
		ret = load_crosshair_csv(cfg.crosshair_file, &ch, subtitle,
					 sizeof(subtitle));
		if (ret) {
			fprintf(stderr, "Unable to parse crosshair CSV file: %s\n",
				cfg.crosshair_filename);
			return -1;
		}

		ch_ptr = &ch;
	}

	if (cfg.plot_file) {
		pixels = load_eye_csv(cfg.plot_file, &cfg.x_range,
				&cfg.y_range, subtitle, sizeof(subtitle),
				&cfg.step_interval);
		if (!pixels) {
			fprintf(stderr, "Unable to parse CSV file: %s\n",
				cfg.plot_filename);
			return -1;
		}

		gen = 0;
		sscanf(subtitle, "Eye Observation, Port %d, Lane %d, Gen %d",
		       &cfg.port_id, &cfg.lane_id, &gen);

		if (cfg.crosshair_filename)
			snprintf(title, sizeof(title) - 1, "%s (%s / %s)",
				 subtitle, cfg.plot_filename,
				 cfg.crosshair_filename);
		else
			snprintf(title, sizeof(title), "%s (%s)", subtitle,
				 cfg.plot_filename);
	} else {
		if (!cfg.dev) {
			fprintf(stderr,
				"Must specify a switchtec device if not using -P\n");
			return -1;
		}
		if (cfg.port_id < 0) {
			fprintf(stderr, "Must specify a port ID with --port/-p\n");
			return -1;
		}
	}

	if (cfg.x_range.start > 63) {
		fprintf(stderr, "Start time (--t-start/-t) is out of range (0, 63)\n");
		return -1;
	}

	if (cfg.x_range.end > 63 || cfg.x_range.end <= cfg.x_range.start) {
		fprintf(stderr, "End time (--t-end/-T) is out of range (t-start, 63)\n");
		return -1;
	}

	if (cfg.y_range.start < -255 || cfg.y_range.start > 255) {
		fprintf(stderr, "Start voltage (--v-start/-v) is out of range (-255, 255)\n");
		return -1;
	}

	if (cfg.y_range.end > 255 || cfg.y_range.end <= cfg.y_range.start) {
		fprintf(stderr, "End voltage (--v-end/-V) is out of range (v-start, 255)\n");
		return -1;
	}

	if (cfg.num_lanes > 1 && cfg.fmt != FMT_CSV) {
		fprintf(stderr, "--format/-f must be CSV if --num-lanes/-n is greater than 1\n");
		return -1;
	}

	if (!pixels) {
		pixels = eye_observe_dev(cfg.dev, cfg.port_id, cfg.lane_id,
				cfg.num_lanes, cfg.mode, cfg.step_interval,
				&cfg.x_range, &cfg.y_range, &gen);
		if (!pixels)
			return -1;

		eye_set_title(title, cfg.port_id, cfg.lane_id, gen);
	}

	if (cfg.fmt == FMT_CSV) {
		write_eye_csv_files(cfg.port_id, cfg.lane_id, cfg.num_lanes,
				    cfg.step_interval, gen, &cfg.x_range,
				    &cfg.y_range, pixels);
		free(pixels);
		return 0;
	}

	ret = eye_graph(cfg.fmt, &cfg.x_range, &cfg.y_range, pixels, title,
			ch_ptr);

	free(pixels);
	return ret;
}

static const struct argconfig_choice loopback_ltssm_speeds[] = {
	{"GEN1", SWITCHTEC_DIAG_LTSSM_GEN1, "GEN1 LTSSM Speed"},
	{"GEN2", SWITCHTEC_DIAG_LTSSM_GEN2, "GEN2 LTSSM Speed"},
	{"GEN3", SWITCHTEC_DIAG_LTSSM_GEN3, "GEN3 LTSSM Speed"},
	{"GEN4", SWITCHTEC_DIAG_LTSSM_GEN4, "GEN4 LTSSM Speed"},
	{}
};

static int print_loopback_mode(struct switchtec_dev *dev, int port_id)
{
	enum switchtec_diag_ltssm_speed speed;
	const struct argconfig_choice *s;
	int ret, b = 0, enable;
	const char *speed_str;
	char buf[100];

	ret = switchtec_diag_loopback_get(dev, port_id, &enable, &speed);
	if (ret) {
		switchtec_perror("loopback_get");
		return -1;
	}

	if (!enable)
		b += snprintf(&buf[b], sizeof(buf) - b, "DISABLED, ");
	if (enable & SWITCHTEC_DIAG_LOOPBACK_RX_TO_TX)
		b += snprintf(&buf[b], sizeof(buf) - b, "RX->TX, ");
	if (enable & SWITCHTEC_DIAG_LOOPBACK_TX_TO_RX)
		b += snprintf(&buf[b], sizeof(buf) - b, "TX->RX, ");
	if (enable & SWITCHTEC_DIAG_LOOPBACK_LTSSM)
		b += snprintf(&buf[b], sizeof(buf) - b, "LTSSM, ");

	/* Drop trailing comma */
	buf[b - 2] = '\0';

	speed_str = "";
	if (enable & SWITCHTEC_DIAG_LOOPBACK_LTSSM) {
		for (s = loopback_ltssm_speeds; s->name; s++) {
			if (s->value == speed) {
				speed_str = s->name;
				break;
			}
		}
	}

	printf("Port: %d    %-30s %s\n", port_id, buf, speed_str);

	return 0;
}

#define CMD_DESC_LOOPBACK "Enable Loopback on specified ports"

static int loopback(int argc, char **argv)
{
	int ret, enable = 0;

	struct {
		struct switchtec_dev *dev;
		struct switchtec_status port;
		int port_id;
		int disable;
		int enable_tx_to_rx;
		int enable_rx_to_tx;
		int enable_ltssm;
		int speed;
	} cfg = {
		.port_id = -1,
		.speed = SWITCHTEC_DIAG_LTSSM_GEN4,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"port", 'p', "PORT_ID", CFG_NONNEGATIVE, &cfg.port_id,
		 required_argument, "physical port ID to set/get loopback for"},
		{"disable", 'd', "", CFG_NONE, &cfg.disable, no_argument,
		 "Disable all loopback modes"},
		{"ltssm", 'l', "", CFG_NONE, &cfg.enable_ltssm, no_argument,
		 "Enable LTSSM loopback mode"},
		{"rx-to-tx", 'r', "", CFG_NONE, &cfg.enable_rx_to_tx, no_argument,
		 "Enable RX->TX loopback mode"},
		{"tx-to-rx", 't', "", CFG_NONE, &cfg.enable_tx_to_rx, no_argument,
		 "Enable TX->RX loopback mode"},
		{"speed", 's', "GEN", CFG_CHOICES, &cfg.speed, required_argument,
		 "LTSSM Speed (if enabling the LTSSM loopback mode), default: GEN4",
		 .choices = loopback_ltssm_speeds},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_LOOPBACK, opts, &cfg, sizeof(cfg));

	if (cfg.port_id < 0) {
		fprintf(stderr, "Must specify -p / --port_id\n");
		return -1;
	}

	if (cfg.disable && (cfg.enable_rx_to_tx || cfg.enable_tx_to_rx ||
			    cfg.enable_ltssm)) {
		fprintf(stderr,
			"Must not specify -d / --disable with an enable flag\n");
		return -1;
	}

	ret = get_port(cfg.dev, cfg.port_id, &cfg.port);
	if (ret)
		return ret;

	if (cfg.disable || cfg.enable_rx_to_tx || cfg.enable_tx_to_rx ||
	    cfg.enable_ltssm) {
		if (cfg.enable_rx_to_tx)
			enable |= SWITCHTEC_DIAG_LOOPBACK_RX_TO_TX;
		if (cfg.enable_tx_to_rx)
			enable |= SWITCHTEC_DIAG_LOOPBACK_TX_TO_RX;
		if (cfg.enable_ltssm)
			enable |= SWITCHTEC_DIAG_LOOPBACK_LTSSM;

		ret = switchtec_diag_loopback_set(cfg.dev, cfg.port_id,
						  enable, cfg.speed);
		if (ret) {
			switchtec_perror("loopback_set");
			return -1;
		}
	}

	return print_loopback_mode(cfg.dev, cfg.port_id);
}

static const struct argconfig_choice pattern_types[] = {
	{"PRBS7",   SWITCHTEC_DIAG_PATTERN_PRBS_7,  "PRBS 7"},
	{"PRBS11",  SWITCHTEC_DIAG_PATTERN_PRBS_11, "PRBS 11"},
	{"PRBS23",  SWITCHTEC_DIAG_PATTERN_PRBS_23, "PRBS 23"},
	{"PRBS31",  SWITCHTEC_DIAG_PATTERN_PRBS_31, "PRBS 31"},
	{"PRBS9",   SWITCHTEC_DIAG_PATTERN_PRBS_9,  "PRBS 9"},
	{"PRBS15",  SWITCHTEC_DIAG_PATTERN_PRBS_15, "PRBS 15"},
	{}
};

static const char *pattern_to_str(enum switchtec_diag_pattern type)
{
	const struct argconfig_choice *s;

	for (s = pattern_types; s->name; s++) {
		if (s->value == type)
			return s->name;
	}

	return "UNKNOWN";
}

static int print_pattern_mode(struct switchtec_dev *dev,
		struct switchtec_status *port, int port_id)
{
	enum switchtec_diag_pattern gen_pat, mon_pat;
	unsigned long long err_cnt;
	int ret, lane_id;

	ret = switchtec_diag_pattern_gen_get(dev, port_id, &gen_pat);
	if (ret) {
		switchtec_perror("pattern_gen_get");
		return -1;
	}

	ret = switchtec_diag_pattern_mon_get(dev, port_id, 0, &mon_pat, &err_cnt);
	if (ret) {
		switchtec_perror("pattern_mon_get");
		return -1;
	}

	printf("Port: %d\n", port_id);
	if (gen_pat == SWITCHTEC_DIAG_PATTERN_PRBS_DISABLED)
		printf("  Generator: Disabled\n");
	else
		printf("  Generator: %s\n", pattern_to_str(gen_pat));

	if (mon_pat == SWITCHTEC_DIAG_PATTERN_PRBS_DISABLED) {
		printf("  Monitor: Disabled\n");
	} else {
		printf("  Monitor: %-20s\n", pattern_to_str(mon_pat));
		printf("    Lane %-2d    Errors: 0x%llx\n", 0, err_cnt);
		for (lane_id = 1; lane_id < port->cfg_lnk_width; lane_id++) {
			ret = switchtec_diag_pattern_mon_get(dev, port_id,
					lane_id, NULL, &err_cnt);
			printf("    Lane %-2d    Errors: 0x%llx\n", lane_id,
			       err_cnt);
			if (ret) {
				switchtec_perror("pattern_mon_get");
				return -1;
			}
		}
	}

	return 0;
}

#define CMD_DESC_PATTERN "Enable pattern generation and monitor"

static int pattern(int argc, char **argv)
{
	int ret;

	struct {
		struct switchtec_dev *dev;
		struct switchtec_status port;
		int port_id;
		int disable;
		int generate;
		int monitor;
		int pattern;
		int inject_errs;
	} cfg = {
		.port_id = -1,
		.pattern = SWITCHTEC_DIAG_PATTERN_PRBS_31,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"port", 'p', "PORT_ID", CFG_NONNEGATIVE, &cfg.port_id,
		 required_argument, "physical port ID to set/get loopback for"},
		{"disable", 'd', "", CFG_NONE, &cfg.disable, no_argument,
		 "Disable all generators and monitors"},
		{"inject", 'i', "NUM", CFG_NONNEGATIVE, &cfg.inject_errs,
		 required_argument,
		 "Inject the specified number of errors into all lanes of the TX port"},
		{"generate", 'g', "", CFG_NONE, &cfg.generate, no_argument,
		 "Enable Pattern Generator on specified port"},
		{"monitor", 'm', "", CFG_NONE, &cfg.monitor, no_argument,
		 "Enable Pattern Monitor on specified port"},
		{"pattern", 't', "PATTERN", CFG_CHOICES, &cfg.pattern,
		 required_argument,
		 "pattern to generate or monitor for (default: PRBS31)",
		 .choices = pattern_types},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_PATTERN, opts, &cfg, sizeof(cfg));

	if (cfg.port_id < 0) {
		fprintf(stderr, "Must specify -p / --port_id\n");
		return -1;
	}

	if (cfg.disable && (cfg.generate || cfg.monitor)) {
		fprintf(stderr,
			"Must not specify -d / --disable with an enable flag\n");
		return -1;
	}

	ret = get_port(cfg.dev, cfg.port_id, &cfg.port);
	if (ret)
		return ret;

	if (cfg.disable) {
		cfg.generate = 1;
		cfg.monitor = 1;
		cfg.pattern = SWITCHTEC_DIAG_PATTERN_PRBS_DISABLED;
	}

	if (cfg.monitor) {
		ret = switchtec_diag_pattern_mon_set(cfg.dev, cfg.port_id,
						     cfg.pattern);
		if (ret) {
			switchtec_perror("pattern_mon_set");
			return -1;
		}
	}

	if (cfg.generate) {
		ret = switchtec_diag_pattern_gen_set(cfg.dev, cfg.port_id,
						     cfg.pattern);
		if (ret) {
			switchtec_perror("pattern_gen_set");
			return -1;
		}
	}

	if (cfg.inject_errs > 1000) {
		fprintf(stderr, "Too many errors to inject. --inject / -i must be less than 1000\n");
		return -1;
	}

	if (cfg.inject_errs) {
		ret = switchtec_diag_pattern_inject(cfg.dev, cfg.port_id,
						    cfg.inject_errs);
		if (ret) {
			switchtec_perror("pattern_inject");
			return -1;
		}
		printf("Injected %d errors\n", cfg.inject_errs);
		return 0;
	}

	return print_pattern_mode(cfg.dev, &cfg.port, cfg.port_id);
}

#define CMD_DESC_LIST_MRPC "List permissible MRPC commands"

static int list_mrpc(int argc, char **argv)
{
	struct switchtec_mrpc table[MRPC_MAX_ID];
	int i, ret;

	static struct {
		struct switchtec_dev *dev;
		int all;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"all", 'a', "", CFG_NONE, &cfg.all, no_argument,
		 "print all MRPC commands, including ones that are unknown"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_LIST_MRPC, opts, &cfg,
			sizeof(cfg));
	ret = switchtec_diag_perm_table(cfg.dev, table);
	if (ret) {
		switchtec_perror("perm_table");
		return -1;
	}

	for (i = 0; i < MRPC_MAX_ID; i++) {
		if (!table[i].tag)
			continue;
		if (!cfg.all && table[i].reserved)
			continue;

		printf("  0x%03x  %-25s  %s\n", i, table[i].tag,
		       table[i].desc);
	}

	return 0;
}

#define CMD_DESC_PORT_EQ_TXCOEFF "Dump port equalization coefficients"

static int port_eq_txcoeff(int argc, char **argv)
{
	struct diag_common_cfg cfg = DEFAULT_DIAG_COMMON_CFG;
	struct switchtec_port_eq_coeff coeff;
	int i, ret;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION, FAR_END_OPTION, PORT_OPTION, PREV_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_PORT_EQ_TXCOEFF,
				    &cfg, opts);
	if (ret)
		return ret;

	ret = switchtec_diag_port_eq_tx_coeff(cfg.dev, cfg.port_id, cfg.end,
					      cfg.link, &coeff);
	if (ret) {
		switchtec_perror("port_eq_coeff");
		return -1;
	}

	printf("%s TX Coefficients for physical port %d %s\n\n",
	       cfg.far_end ? "Far End" : "Local", cfg.port_id,
	       cfg.prev ? "(Previous Link-Up)" : "");
	printf("Lane  Pre-Cursor  Post-Cursor\n");

	for (i = 0; i < coeff.lane_cnt; i++) {
		printf("%4d  %7d      %8d\n", i, coeff.cursors[i].pre,
		       coeff.cursors[i].post);
	}

	return 0;
}

#define CMD_DESC_PORT_EQ_TXFSLF "Dump FS/LF output data"

static int port_eq_txfslf(int argc, char **argv)
{
	struct diag_common_cfg cfg = DEFAULT_DIAG_COMMON_CFG;
	struct switchtec_port_eq_tx_fslf data;
	int i, ret;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION, FAR_END_OPTION, PORT_OPTION, PREV_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_PORT_EQ_TXFSLF,
				    &cfg, opts);
	if (ret)
		return ret;

	printf("%s Equalization FS/LF data for physical port %d %s\n\n",
	       cfg.far_end ? "Far End" : "Local", cfg.port_id,
	       cfg.prev ? "(Previous Link-Up)" : "");
	printf("Lane    FS    LF\n");

	for (i = 0; i < cfg.port.neg_lnk_width; i++) {
		ret = switchtec_diag_port_eq_tx_fslf(cfg.dev, cfg.port_id, i,
				cfg.end, cfg.link, &data);
		if (ret) {
			switchtec_perror("port_eq_fs_ls");
			return -1;
		}

		printf("%4d  %4d  %4d\n", i, data.fs, data.lf);
	}

	return 0;
}

#define CMD_DESC_PORT_EQ_TXTABLE "Dump far end port equalization table"

static int port_eq_txtable(int argc, char **argv)
{
	struct diag_common_cfg cfg = DEFAULT_DIAG_COMMON_CFG;
	struct switchtec_port_eq_table table;
	int i, ret;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION, PORT_OPTION, PREV_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_PORT_EQ_TXTABLE,
				    &cfg, opts);
	if (ret)
		return ret;

	ret = switchtec_diag_port_eq_tx_table(cfg.dev, cfg.port_id,
					      cfg.link, &table);
	if (ret) {
		switchtec_perror("port_eq_table");
		return -1;
	}

	printf("Far End TX Equalization Table for physical port %d, lane %d %s\n\n",
	       cfg.port_id, table.lane_id, cfg.prev ? "(Previous Link-Up)" : "");
	printf("Step  Pre-Cursor  Post-Cursor  FOM  Pre-Up  Post-Up  Error  Active  Speed\n");

	for (i = 0; i < table.step_cnt; i++) {
		printf("%4d  %10d  %11d  %3d  %6d  %7d  %5d  %6d  %5d\n",
		       i, table.steps[i].pre_cursor, table.steps[i].post_cursor,
		       table.steps[i].fom, table.steps[i].pre_cursor_up,
		       table.steps[i].post_cursor_up, table.steps[i].error_status,
		       table.steps[i].active_status, table.steps[i].speed);
	}

	return 0;
}

#define CMD_DESC_RCVR_OBJ "Dump analog RX coefficients/adaptation objects"

static int rcvr_obj(int argc, char **argv)
{
	struct diag_common_cfg cfg = DEFAULT_DIAG_COMMON_CFG;
	struct switchtec_rcvr_obj obj;
	int i, j, ret;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION, PORT_OPTION, PREV_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_RCVR_OBJ,
				    &cfg, opts);
	if (ret)
		return ret;

	printf("Coefficients for physical port %d %s\n\n", cfg.port_id,
	       cfg.prev ? "(Previous Link-Up)" : "");
	printf("Lane  CTLE  Tgt-Amp  Spec-DFE  DFE0 DFE1 DFE2 DFE3 DFE4 DFE5 DFE6\n");

	for (i = 0; i < cfg.port.neg_lnk_width; i++) {
		ret = switchtec_diag_rcvr_obj(cfg.dev, cfg.port_id, i,
					      cfg.link, &obj);
		if (ret) {
			switchtec_perror("rcvr_obj");
			return -1;
		}

		printf("%4d  %4d  %6d   %7d   ", i, obj.ctle,
		       obj.target_amplitude, obj.speculative_dfe);
		for (j = 0; j < ARRAY_SIZE(obj.dynamic_dfe); j++)
			printf("%4d ", obj.dynamic_dfe[j]);
		printf("\n");
	}

	return 0;
}

#define CMD_DESC_RCVR_EXTENDED "Dump RX mode and DTCLK"

static int rcvr_extended(int argc, char **argv)
{
	struct diag_common_cfg cfg = DEFAULT_DIAG_COMMON_CFG;
	struct switchtec_rcvr_ext ext;
	int i, ret;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION, PORT_OPTION, PREV_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_RCVR_EXTENDED,
				    &cfg, opts);
	if (ret)
		return ret;

	printf("Mode and DTCLCK for physical port %d %s\n\n",
	       cfg.port_id, cfg.prev ? "(Previous Link-Up)" : "");
	printf("Lane      MODE   DTCLK_5  DTCLK_8_6  DTCLK_9\n");

	for (i = 0; i < cfg.port.neg_lnk_width; i++) {
		ret = switchtec_diag_rcvr_ext(cfg.dev, cfg.port_id, i,
					      cfg.link, &ext);
		if (ret) {
			switchtec_perror("rx_mode");
			return -1;
		}

		printf("%4d  %#8x  %7d  %9d  %7d\n", i, ext.ctle2_rx_mode,
		       ext.dtclk_5, ext.dtclk_8_6, ext.dtclk_9);
	}

	return 0;
}

#define CMD_DESC_REF_CLK "Enable or disable the output reference clock of a stack"

static int refclk(int argc, char **argv)
{
	int ret;

	static struct {
		struct switchtec_dev *dev;
		int stack_id;
		int enable;
		int disable;
	} cfg = {
		.stack_id = -1
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"disable", 'd', "", CFG_NONE, &cfg.disable, no_argument,
		 "disable the rfclk output"},
		{"enable", 'e', "", CFG_NONE, &cfg.enable, no_argument,
		 "enable the rfclk output"},
		{"stack", 's', "NUM", CFG_NONNEGATIVE, &cfg.stack_id,
		required_argument, "stack to operate on"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_REF_CLK, opts, &cfg,
			sizeof(cfg));

	if (!cfg.enable && !cfg.disable) {
		fprintf(stderr, "Must set either --enable or --disable\n");
		return -1;
	}

	if (cfg.enable && cfg.disable) {
		fprintf(stderr, "Must not set both --enable and --disable\n");
		return -1;
	}

	if (cfg.stack_id == -1) {
		fprintf(stderr, "Must specify stack ID using --stack or -s\n");
		return -1;
	}

	ret = switchtec_diag_refclk_ctl(cfg.dev, cfg.stack_id, cfg.enable);
	if (ret) {
		switchtec_perror("refclk_ctl");
		return -1;
	}

	printf("REFCLK Output %s for Stack %d\n",
	       cfg.enable ? "Enabled" : "Disabled", cfg.stack_id);

	return 0;
}

static int convert_str_to_dwords(char *str, uint32_t **dwords, int *num_dwords)
{
	*num_dwords = 0;
	const char *ptr = str;
	while (*ptr != '\0') {
		if (*ptr == '0' && *(ptr + 1) == 'x') {
			(*num_dwords)++;
			ptr += 2;
		}
		while (*ptr != ' ' && *ptr != '\0')
			ptr++;
		if (*ptr == ' ')
			ptr++;
	}

	*dwords = (uint32_t *)malloc(*num_dwords * sizeof(uint32_t));
	if (*dwords == NULL)
		return -1;

	ptr = str;
	for (int i = 0; i < *num_dwords; i++) {
		char *endptr;
		(*dwords)[i] = (uint32_t)strtoul(ptr, &endptr, 0);
		if (endptr == ptr || (*endptr != ' ' && *endptr != '\0')) {
			free(*dwords);
			return -1;
		}
		ptr = endptr;
		while (*ptr == ' ')
			ptr++;
	}

	return 0;
}

#define CMD_TLP_INJECT "Inject a raw TLP"

static int tlp_inject (int argc, char **argv)
{
	int ret = 0;
	uint32_t * raw_tlp_dwords = NULL;
	int num_dwords = 0;
	static struct {
		struct switchtec_dev *dev;
		int port_id;
		int tlp_type;
		int ecrc;
		char * raw_tlp_data;
	} cfg = {
		.tlp_type = 0
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"port", 'p', "PORT_ID", CFG_NONNEGATIVE, &cfg.port_id, 
			required_argument, "destination port ID"},
		{"tlp_type", 't', "TYPE", CFG_NONNEGATIVE, &cfg.tlp_type, 
			required_argument, "tlp type:\n0: P  - Posted\n1: NP - Non-posted\n2: CP - Completion\n(default 0)"},
		{"enable_ecrc", 'e', "", CFG_NONE, &cfg.ecrc, no_argument, 
			"Enable the ecrc to be included at the end of the input data (Default: disabled)"},
		{"tlp_data", 'd', "\"DW0 DW1 ... DW131\"", CFG_STRING, 
			&cfg.raw_tlp_data, required_argument, 
			"DWs to be sent as part of the raw TLP (Maximum 132 DWs)"},
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_TLP_INJECT, opts, &cfg, sizeof(cfg));

	if (cfg.raw_tlp_data == NULL) {
		fprintf(stderr, "Must set tlp data --tlp_data -d \n");
		return -1;
	}
	ret = convert_str_to_dwords(cfg.raw_tlp_data, &raw_tlp_dwords, 
				    &num_dwords);
	if (ret) {
		fprintf(stderr, "Error with tlp data provided \n");
		return -1;
	}
	if (num_dwords > 132) {
		fprintf(stderr, "TLP data cannot exceed 132 dwords \n");
		free(raw_tlp_dwords);
		return -1;
	}

	ret = switchtec_tlp_inject(cfg.dev, cfg.port_id, cfg.tlp_type, 
				   num_dwords, cfg.ecrc, raw_tlp_dwords);
	if (ret != 0) {
		switchtec_perror("tlp_inject");
		return -1;
	}

	return 0;
}

static const struct cmd commands[] = {
	CMD(crosshair,		CMD_DESC_CROSS_HAIR),
	CMD(eye,		CMD_DESC_EYE),
	CMD(list_mrpc,		CMD_DESC_LIST_MRPC),
	CMD(loopback,		CMD_DESC_LOOPBACK),
	CMD(pattern,		CMD_DESC_PATTERN),
	CMD(port_eq_txcoeff,	CMD_DESC_PORT_EQ_TXCOEFF),
	CMD(port_eq_txfslf,	CMD_DESC_PORT_EQ_TXFSLF),
	CMD(port_eq_txtable,	CMD_DESC_PORT_EQ_TXTABLE),
	CMD(rcvr_extended,	CMD_DESC_RCVR_EXTENDED),
	CMD(rcvr_obj,		CMD_DESC_RCVR_OBJ),
	CMD(refclk,		CMD_DESC_REF_CLK),
	CMD(ltssm_log,		CMD_DESC_LTSSM_LOG),
	CMD(tlp_inject,		CMD_TLP_INJECT),
	{}
};

static struct subcommand subcmd = {
	.name = "diag",
	.cmds = commands,
	.desc = "Diagnostic Information",
	.long_desc = "These functions provide diagnostic information from "
		"the switch",
};

REGISTER_SUBCMD(subcmd);

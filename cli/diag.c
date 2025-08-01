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
#include <switchtec/endian.h>
#include <switchtec/errors.h>

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
	int prev_speed;
};

static const struct argconfig_choice port_eq_prev_speeds[] = {
	{"GEN3", PCIE_LINK_RATE_GEN3, "GEN3 Previous Speed"},
	{"GEN4", PCIE_LINK_RATE_GEN4, "GEN4 Previous Speed"},
	{"GEN5", PCIE_LINK_RATE_GEN5, "GEN5 Previous Speed"},
	{}
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

#define PREV_SPEED_OPTION {						\
	"prev_rate", 'r', "RATE", CFG_CHOICES, &cfg.prev_speed, 	\
	required_argument, "return the data for the previous link at the specified link rate\n(supported on Gen 5 switchtec devices only)", \
	.choices=port_eq_prev_speeds					\
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
		printf("%s\n", switchtec_ltssm_str(output[i].link_state, 1, 
						   cfg.dev));
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

	if (interval > -1)
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

	ret = switchtec_diag_eye_start(dev, lane_mask, X, Y, interval, 0);
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
		     struct switchtec_diag_cross_hair *ch,
		     struct switchtec_dev *dev)
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
		if (switchtec_is_gen5(dev))
			graph_draw_text_no_invert(X, Y, data, title, 'P', 'B');
		else
			graph_draw_text(X, Y, data, title, 'T', 'V');
		if (status_ptr)
			printf("\n      %s\n", status_ptr);
		return 0;
	}

	if (switchtec_is_gen5(dev))
		return graph_draw_win(X, Y, data, shades, title, 'P', 'B',
				      status_ptr, NULL, NULL);
	else
		return graph_draw_win(X, Y, data, shades, title, 'T', 'V',
				      status_ptr, NULL, NULL);
}

static double *eye_capture_dev_gen5(struct switchtec_dev *dev,
				    int port_id, int lane_id, int num_lanes,
				    int capture_depth, int* num_phases, int* gen)
{
	int bin, j, ret, first_lane, num_phases_l, stride;
	int lane_mask[4] = {};
	struct switchtec_status sw_status;
	double tmp[60];
	double* ber_data = NULL;

	ret = switchtec_calc_lane_mask(dev, port_id, lane_id, num_lanes,
				       lane_mask, &sw_status);
	if (ret < 0) {
		switchtec_perror("Invalid lane");
		return NULL;
	}

	ret = switchtec_diag_eye_start(dev, lane_mask, NULL, NULL, 0, 
				       capture_depth);
	if (ret) {
		switchtec_perror("eye_run");
		return NULL;
	}

	first_lane = switchtec_calc_lane_id(dev, port_id, lane_id, NULL);
	for (j = 0; j < num_lanes; j++) {
		for (bin = 0; bin < 64; bin++) {
			ret = switchtec_diag_eye_read(dev, first_lane + j, bin, 
						      &num_phases_l, tmp);
			if (ret) {
				switchtec_perror("eye_read");
				if (ber_data)
					free(ber_data);
				return NULL;
			}

			if (!ber_data) {
				stride = 64 * num_phases_l;
				ber_data = calloc(num_lanes * stride, 
						  sizeof(double));
				if (!ber_data) {
					perror("allocating BER data");
					return NULL;
				}
			}

			memcpy(&ber_data[(j * stride) + (bin * num_phases_l)], 
			       tmp, num_phases_l * sizeof(double));
		}
	}

	*gen = sw_status.link_rate;
	*num_phases = num_phases_l;
	return ber_data;
}

#define CMD_DESC_EYE "Capture PCIe Eye Errors"

static int eye(int argc, char **argv)
{
	struct switchtec_diag_cross_hair ch = {}, *ch_ptr = NULL;
	char title[128], subtitle[50];
	double *pixels = NULL;
	int num_phases, ret, gen;

	static struct {
		struct switchtec_dev *dev;
		int fmt;
		int port_id;
		int lane_id;
		int capture_depth;
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
		.capture_depth = 24,
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
		 "optionally, superimpose a crosshair CSV onto the result (Not supported in Gen 5)"},
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
		{"capture-depth", 'd', "NUM", CFG_POSITIVE, &cfg.capture_depth,
		 required_argument, "capture depth (6 to 40; default: 24)"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_EYE, opts, &cfg,
			sizeof(cfg));

	if (cfg.dev != NULL && switchtec_is_gen5(cfg.dev)) {
		cfg.y_range.start = 0;
		cfg.y_range.end = 63;
		cfg.y_range.step = 1;
	}
	
	if (cfg.crosshair_file) {
		if (switchtec_is_gen5(cfg.dev)) {
			fprintf(stderr, "Crosshair superimpose not suppored in Gen 5\n");
			return -1;
		}
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
		if (switchtec_is_gen5(cfg.dev)) {
			pixels = eye_capture_dev_gen5(cfg.dev, cfg.port_id, 
						      cfg.lane_id, cfg.num_lanes, 
						      cfg.capture_depth, 
						      &num_phases, &gen);
			if (!pixels)
				return -1;

			cfg.x_range.end = num_phases - 1;
		}
		else {
			pixels = eye_observe_dev(cfg.dev, cfg.port_id, 
						 cfg.lane_id, cfg.num_lanes, 
						 cfg.mode, cfg.step_interval, 
						 &cfg.x_range, &cfg.y_range, 
						 &gen);
			if (!pixels)
				return -1;
		}
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
			ch_ptr, cfg.dev);

	free(pixels);
	return ret;
}

static const struct argconfig_choice loopback_ltssm_speeds[] = {
	{"GEN1", SWITCHTEC_DIAG_LTSSM_GEN1, "GEN1 LTSSM Speed"},
	{"GEN2", SWITCHTEC_DIAG_LTSSM_GEN2, "GEN2 LTSSM Speed"},
	{"GEN3", SWITCHTEC_DIAG_LTSSM_GEN3, "GEN3 LTSSM Speed"},
	{"GEN4", SWITCHTEC_DIAG_LTSSM_GEN4, "GEN4 LTSSM Speed"},
	{"GEN5", SWITCHTEC_DIAG_LTSSM_GEN5, "GEN5 LTSSM Speed"},
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
	if (switchtec_is_gen5(dev)) {
		if (enable & SWITCHTEC_DIAG_LOOPBACK_RX_TO_TX)
			b += snprintf(&buf[b], sizeof(buf) - b, "PARALLEL, ");
		if (enable & SWITCHTEC_DIAG_LOOPBACK_TX_TO_RX)
			b += snprintf(&buf[b], sizeof(buf) - b, "EXTERNAL, ");
	} else {
		if (enable & SWITCHTEC_DIAG_LOOPBACK_RX_TO_TX)
			b += snprintf(&buf[b], sizeof(buf) - b, "RX->TX, ");
		if (enable & SWITCHTEC_DIAG_LOOPBACK_TX_TO_RX)
			b += snprintf(&buf[b], sizeof(buf) - b, "TX->RX, ");
	}
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
		int enable_parallel;
		int enable_external;
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
		 "Enable LTSSM loopback mode (Gen 4 / Gen 5)"},
		{"rx-to-tx", 'r', "", CFG_NONE, &cfg.enable_rx_to_tx, 
		 no_argument, "Enable RX->TX loopback mode (Gen 4)"},
		{"tx-to-rx", 't', "", CFG_NONE, &cfg.enable_tx_to_rx, 
		 no_argument, "Enable TX->RX loopback mode (Gen 4)"},
		{"parallel", 'P', "", CFG_NONE, &cfg.enable_parallel, 
		 no_argument, "Enable parallel datapath loopback mode in SERDES digital layer (Gen 5)"},
		{"external", 'e', "", CFG_NONE, &cfg.enable_external, 
		 no_argument, "Enable external datapath loopback mode in physical layer (Gen 5)"},
		{"speed", 's', "GEN", CFG_CHOICES, &cfg.speed, 
		 required_argument, "LTSSM Speed (if enabling the LTSSM loopback mode), default: GEN4",
		 .choices = loopback_ltssm_speeds},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_LOOPBACK, opts, &cfg, sizeof(cfg));

	if ((cfg.enable_external || cfg.enable_parallel) && 
	    (cfg.enable_rx_to_tx || cfg.enable_tx_to_rx)) {
		fprintf(stderr, "Cannot enable both Gen4 and Gen5 loopback settings. Use \'--help\' to see full list and support for each.\n");
		return -1;
	}

	if (cfg.port_id < 0) {
		fprintf(stderr, "Must specify -p / --port_id\n");
		return -1;
	}

	if (cfg.disable && (cfg.enable_rx_to_tx || cfg.enable_tx_to_rx ||
			    cfg.enable_ltssm || cfg.enable_external ||
			    cfg.enable_parallel)) {
		fprintf(stderr,
			"Must not specify -d / --disable with an enable flag\n");
		return -1;
	}

	ret = get_port(cfg.dev, cfg.port_id, &cfg.port);
	if (ret)
		return ret;

	if (cfg.disable || cfg.enable_rx_to_tx || cfg.enable_tx_to_rx ||
	    cfg.enable_ltssm || cfg.enable_external || cfg.enable_parallel) {
		if (cfg.enable_rx_to_tx)
			enable |= SWITCHTEC_DIAG_LOOPBACK_RX_TO_TX;
		if (cfg.enable_tx_to_rx)
			enable |= SWITCHTEC_DIAG_LOOPBACK_TX_TO_RX;
		if (cfg.enable_ltssm)
			enable |= SWITCHTEC_DIAG_LOOPBACK_LTSSM;

		if (switchtec_is_gen5(cfg.dev)) {
			if (cfg.enable_rx_to_tx || cfg.enable_tx_to_rx) {
				fprintf(stderr, "Cannot enable Gen 4 settings \'-r\' \'--rx-to-tx\' or \'-t\' \'--tx-to-rx\' on Gen 5 system. \n");
				return -1;
			}
		}
		ret = switchtec_diag_loopback_set(cfg.dev, cfg.port_id, enable,
						  cfg.enable_parallel, 
						  cfg.enable_external, 
						  cfg.enable_ltssm, cfg.speed);
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
	{"PRBS5",   SWITCHTEC_DIAG_GEN_5_PATTERN_PRBS_5,  "PRBS 5 (Gen 5)"},
	{"PRBS20",  SWITCHTEC_DIAG_GEN_5_PATTERN_PRBS_20, "PRBS 20 (Gen 5)"},
	{}
};

static const struct argconfig_choice pat_gen_link_speeds[] = {
	{"GEN1", SWITCHTEC_DIAG_PAT_LINK_GEN1, "GEN1 Pattern Generator Speed"},
	{"GEN2", SWITCHTEC_DIAG_PAT_LINK_GEN2, "GEN2 Pattern Generator Speed"},
	{"GEN3", SWITCHTEC_DIAG_PAT_LINK_GEN3, "GEN3 Pattern Generator Speed"},
	{"GEN4", SWITCHTEC_DIAG_PAT_LINK_GEN4, "GEN4 Pattern Generator Speed"},
	{"GEN5", SWITCHTEC_DIAG_PAT_LINK_GEN5, "GEN5 Pattern Generator Speed"},
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

static const char *link_speed_to_str(enum switchtec_diag_pattern_link_rate type)
{
	const struct argconfig_choice *s;

	for (s = pat_gen_link_speeds; s->name; s++) {
		if (s->value == type)
			return s->name;
	}

	return "UNKNOWN";
}

static int print_pattern_mode(struct switchtec_dev *dev,
		struct switchtec_status *port, int port_id)
{
	enum switchtec_diag_pattern gen_pat, mon_pat;
	int gen_pat_gen5, mon_pat_gen5;
	unsigned long long err_cnt;
	int ret, lane_id;
	int err = 0;

	ret = switchtec_diag_pattern_gen_get(dev, port_id, &gen_pat);
	if (ret) {
		switchtec_perror("pattern_gen_get");
		return -1;
	}
	gen_pat_gen5 = gen_pat;
	if (gen_pat_gen5 == SWITCHTEC_DIAG_GEN_5_PATTERN_PRBS_DISABLED) {
		fprintf(stderr, "!! The pattern generator is disabled on either the TX or RX port\n");
		err = 1;
	}

	ret = switchtec_diag_pattern_mon_get(dev, port_id, 0, &mon_pat, 
					     &err_cnt);
	mon_pat_gen5 = mon_pat;
	if (ret == ERR_PAT_MON_IS_DISABLED || mon_pat_gen5 == SWITCHTEC_DIAG_GEN_5_PATTERN_PRBS_DISABLED) {
		fprintf(stderr, "!! The pattern monitor is disabled on either the TX or RX port\n");
		err = 1;
	}
	if (err) {
		fprintf(stderr, "Unable to print additional pattern information until both monitor and generator are enabled correctly\n");
		return -1;
	}

	if (ret) {
		switchtec_perror("pattern_mon_get");
		return -1;
	}

	printf("Port: %d\n", port_id);
	if (gen_pat == SWITCHTEC_DIAG_PATTERN_PRBS_DISABLED && switchtec_is_gen4(dev))
		printf("  Generator: Disabled\n");
	else
		printf("  Generator: %s\n", pattern_to_str(gen_pat));

	if (mon_pat == SWITCHTEC_DIAG_PATTERN_PRBS_DISABLED && switchtec_is_gen4(dev)) {
		printf("  Monitor: Disabled\n");
	} else {
		printf("  Monitor: %-20s\n", pattern_to_str(mon_pat));
		printf("    Lane %-2d    Errors: 0x%llx\n", 0, err_cnt);
		for (lane_id = 1; lane_id < port->cfg_lnk_width; lane_id++) {
			ret = switchtec_diag_pattern_mon_get(dev, port_id,
					lane_id, NULL, &err_cnt);
			if (ret == 0x70b02) {
				printf("    Lane %d has the pattern monitor disabled.\n", 
				       lane_id);
			} else if (ret) {
				switchtec_perror("pattern_mon_get");
				return -1;
			} else {
				printf("    Lane %-2d    Errors: 0x%llx\n", 
				       lane_id, err_cnt);
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
		int link_speed;
	} cfg = {
		.port_id = -1,
		.pattern = SWITCHTEC_DIAG_PATTERN_PRBS_31,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"port", 'p', "PORT_ID", CFG_NONNEGATIVE, &cfg.port_id,
		 required_argument, "physical port ID to set/get loopback for"},
		{"disable", 'd', "", CFG_NONE, &cfg.disable, no_argument,
		 "Without any accompanying flags this will disable both monitor and generator."\
		 " When included with either -m --monitor or -g --generator it will disable the selected type."},
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
		{"speed", 's', "SPEED", CFG_CHOICES, &cfg.link_speed,
		 required_argument, 
		 "link speed that applies to the pattern generator (default: GEN1)",
		 .choices = pat_gen_link_speeds},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_PATTERN, opts, &cfg, sizeof(cfg));

	if (cfg.link_speed && cfg.monitor) {
		fprintf(stderr,
			"Cannot enable link speed -s / --speed on pattern monitor\n");
		return -1;
	}
	
	if (!cfg.link_speed) {
		if (switchtec_is_gen5(cfg.dev))
			cfg.link_speed = SWITCHTEC_DIAG_PAT_LINK_GEN1;
		else
			cfg.link_speed = SWITCHTEC_DIAG_PAT_LINK_DISABLED;
	}

	if (cfg.port_id < 0) {
		fprintf(stderr, "Must specify -p / --port_id\n");
		return -1;
	}
	
	ret = get_port(cfg.dev, cfg.port_id, &cfg.port);
	if (ret)
		return ret;

	if (cfg.disable) {
		if (!cfg.generate && !cfg.monitor) {
			cfg.generate = 1;
			cfg.monitor = 1;
		}
		if (switchtec_is_gen5(cfg.dev))
			cfg.pattern = SWITCHTEC_DIAG_GEN_5_PATTERN_PRBS_DISABLED;
		else
			cfg.pattern = SWITCHTEC_DIAG_PATTERN_PRBS_DISABLED;
	}

	if (cfg.monitor) {
		ret = switchtec_diag_pattern_mon_set(cfg.dev, cfg.port_id,
						     cfg.pattern);
		if (ret) {
			switchtec_perror("pattern_mon_set");
			return -1;
		}
		if (cfg.disable)
			printf("Disabled pattern monitor on port %d\n",
				cfg.port_id);
		else
			printf("Pattern monitor set for port %d with pattern type %s\n", 
				cfg.port_id, pattern_to_str(cfg.pattern));
	}

	if (cfg.generate) {
		ret = switchtec_diag_pattern_gen_set(cfg.dev, cfg.port_id,
						     cfg.pattern, cfg.link_speed);
		if (ret) {
			switchtec_perror("pattern_gen_set");
			return -1;
		}
		if (cfg.disable)
			printf("Disabled pattern generator on port %d\n",
				cfg.port_id);
		else
			printf("Pattern generator set for port %d with pattern type %s at %s\n", 
				cfg.port_id, pattern_to_str(cfg.pattern), 
				link_speed_to_str(cfg.link_speed));
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
		DEVICE_OPTION, FAR_END_OPTION, PORT_OPTION, PREV_OPTION, 
		PREV_SPEED_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_PORT_EQ_TXCOEFF,
				    &cfg, opts);
	if (ret)
		return ret;

	if (!switchtec_is_gen5(cfg.dev) && cfg.prev_speed) {
		fprintf(stderr, "Selecting a previous rate is not supported on Gen 4 or below switchtec devices.\n");
		return -1;
	} else if ((switchtec_is_gen5(cfg.dev) && cfg.prev) && !cfg.prev_speed) {
		fprintf(stderr, "Previous rate -r is required on Gen 5 switchtec devices.\n");
		return -1;
	}

	ret = switchtec_diag_port_eq_tx_coeff(cfg.dev, cfg.port_id, cfg.prev_speed, cfg.end,
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
	int i, ret, lnk_width;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION, FAR_END_OPTION, PORT_OPTION, PREV_OPTION, 
		PREV_SPEED_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_PORT_EQ_TXFSLF,
				    &cfg, opts);
	if (ret)
		return ret;

	if (!switchtec_is_gen5(cfg.dev) && cfg.prev_speed) {
		fprintf(stderr, "Selecting a previous rate is not supported on Gen 4 or below switchtec devices.\n");
		return -1;
	} else if ((switchtec_is_gen5(cfg.dev) && cfg.prev) && !cfg.prev_speed) {
		fprintf(stderr, "Previous rate -r is required on Gen 5 switchtec devices.\n");
		return -1;
	}

	printf("%s Equalization FS/LF data for physical port %d %s\n\n",
	       cfg.far_end ? "Far End" : "Local", cfg.port_id,
	       cfg.prev ? "(Previous Link-Up)" : "");
	printf("Lane    FS    LF\n");

	if (switchtec_is_gen5(cfg.dev))
		lnk_width = cfg.port.cfg_lnk_width;
	else 
		lnk_width = cfg.port.neg_lnk_width;

	for (i = 0; i < lnk_width; i++) {
		ret = switchtec_diag_port_eq_tx_fslf(cfg.dev, cfg.port_id, cfg.prev_speed, i,
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
		DEVICE_OPTION, PORT_OPTION, PREV_OPTION, PREV_SPEED_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_PORT_EQ_TXTABLE,
				    &cfg, opts);
	if (ret)
		return ret;

	if (!switchtec_is_gen5(cfg.dev) && cfg.prev_speed) {
		fprintf(stderr, "Selecting a previous rate is not supported on Gen 4 or below switchtec devices.\n");
		return -1;
	} else if ((switchtec_is_gen5(cfg.dev) && cfg.prev) && !cfg.prev_speed) {
		fprintf(stderr, "Previous rate -r is required on Gen 5 switchtec devices.\n");
		return -1;
	}

	ret = switchtec_diag_port_eq_tx_table(cfg.dev, cfg.port_id, cfg.prev_speed,
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

static int convert_hex_str(char *str, uint32_t **output, int *num_hex_words, 
			   int hex_len_max)
{
	*num_hex_words = 0;
	const char *ptr = str;
	int len = 0;
	while (*ptr != '\0') {
		if (*ptr == '0' && *(ptr + 1) == 'x') {
			(*num_hex_words)++;
			ptr += 2;
			len = 0;
		}
		while (*ptr != ' ' && *ptr != '\0') {
			ptr++;
			len++;
		}
		if (len > hex_len_max) {
			printf("Entered dword longer than allowed\n");
			return -1;
		}
		if (*ptr == ' ')
			ptr++;
	}

	*output = (uint32_t *)malloc(*num_hex_words * sizeof(uint32_t));
	if (*output == NULL)
		return -1;

	ptr = str;
	for (int i = 0; i < *num_hex_words; i++) {
		char *endptr;
		(*output)[i] = (uint32_t)strtoul(ptr, &endptr, 0);
		if (endptr == ptr || (*endptr != ' ' && *endptr != '\0')) {
			free(*output);
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
			"DWs to be sent as part of the raw TLP (Maximum 132 DWs)"\
			", surrounded by quotations. Every DW must start with \'0x\'\nEx. -d \"0x1 0x2 0x3\""},
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_TLP_INJECT, opts, &cfg, sizeof(cfg));

	if (cfg.raw_tlp_data == NULL) {
		fprintf(stderr, "Must set tlp data --tlp_data -d \n");
		return -1;
	}
	ret = convert_hex_str(cfg.raw_tlp_data, &raw_tlp_dwords, 
			      &num_dwords, 8);
	if (ret) {
		fprintf(stderr, "Error with tlp data provided \n");
		return -1;
	}
	if (num_dwords > SWITCHTEC_DIAG_MAX_TLP_DWORDS) {
		fprintf(stderr, "TLP data cannot exceed %d dwords \n", 
			SWITCHTEC_DIAG_MAX_TLP_DWORDS);
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

static int convert_bitfield(char * bits)
{
	int total = 0;
	char * sep_bits = strtok(bits, ",");

	while (sep_bits != NULL) {
		total += 0x1 << atoi(sep_bits);
		sep_bits = strtok(NULL, ",");
    	}
	return total;
}

#define CMD_DESC_AER_EVENT_GEN "Generate an AER Error Event"

static int aer_event_gen(int argc, char **argv)
{
	int ret, aer_bitfield;
	static struct {
		struct switchtec_dev *dev;
		int port_id;
		char * aer_error_id;
		int trigger_event;
	} cfg = {};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"port", 'p', "", CFG_NONNEGATIVE, &cfg.port_id, 
		 required_argument, "port ID"},
		{"ce_event", 'e', "", CFG_STRING, &cfg.aer_error_id, 
		 required_argument, "aer CE event - 0,6,7,8,12,14,15"},
		{"trigger", 't', "", CFG_NONNEGATIVE, &cfg.trigger_event, 
		 required_argument, "trigger event (only CE events supported-0x1)"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_AER_EVENT_GEN, opts, &cfg, 
			sizeof(cfg));
	aer_bitfield = convert_bitfield(cfg.aer_error_id);

	ret = switchtec_aer_event_gen(cfg.dev, cfg.port_id, aer_bitfield, 
				      cfg.trigger_event);

	if (ret != 0) {
		switchtec_perror("aer event generation");
		return 1;
	}

	return 0;
}

#define CMD_DESC_LNKERR_INJECT "Inject a link error"

static int linkerr_inject(int argc, char ** argv)
{
	int ret = 0;
	uint32_t * dllp_data_dword = NULL;
	int num_dwords = 0;
	static struct {
		struct switchtec_dev *dev;
		int inject_dllp;
		int inject_dllp_crc;
		int inject_tlp_lcrc;
		int inject_tlp_seq;
		int inject_nack;
		int inject_cto;
		uint8_t enable;
		uint8_t count;
		uint16_t dllp_rate;
		uint16_t tlp_rate;
		uint16_t seq_num;
		char * dllp_data;
		int phy_port;
	} cfg = {};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"dllp", 'd', "", CFG_NONE, &cfg.inject_dllp, 
		 no_argument, "Inject a DLLP"},
		{"dllp-crc", 'D', "", CFG_NONE, &cfg.inject_dllp_crc, 
		 no_argument, "Inject a DLLP CRC error"},
		{"tlp-lcrc", 'l', "", CFG_NONE, &cfg.inject_tlp_lcrc, 
		 no_argument, "Inject a TLP LCRC error"},
		{"tlp-seq", 's', "", CFG_NONE, &cfg.inject_tlp_seq, 
		 no_argument, "Inject a TLP Sequence Number error"},
		{"nack", 'n', "", CFG_NONE, &cfg.inject_nack, no_argument,
		 "Inject an ACK to NACK error"},
		{"cto", 't', "", CFG_NONE, &cfg.inject_cto, no_argument,
		 "Inject a TLP Credit Timeout"},
		{"port", 'p', "", CFG_NONNEGATIVE, &cfg.phy_port, 
		 required_argument, "physical port ID, default: port 0"},
		{"enable", 'e', "", CFG_NONNEGATIVE, &cfg.enable, 
		 required_argument, "enable DLLP CRC Error Injection or TLP LCRC Error Injection, default: 0"},
		{"data", 'i', "", CFG_STRING, &cfg.dllp_data, required_argument,
		 "DLLP data to inject, a single dword in hex prefixed with \"0x\""},
		{"seq_num", 'S', "", CFG_NONNEGATIVE, &cfg.seq_num, 
		 required_argument, "sequence number of ACK to be replaced by NACK (0-4095)"},
		{"count", 'c', "", CFG_NONNEGATIVE, &cfg.count, 
		 required_argument, "number of times to replace ACK with NACK (0-255)"},
		{"dllp-crc-rate", 'r', "", CFG_NONNEGATIVE, &cfg.dllp_rate, 
		 required_argument, "valid range (0-4096). errors are injected at intervals of rate x 256 x clk "},
		{"tlp-lcrc-rate", 'R', "", CFG_NONNEGATIVE, &cfg.tlp_rate, 
		 required_argument, "valid range (0-7). Ex. rate = 1 -> every other TLP has an error"},
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_DESC_LNKERR_INJECT, opts, &cfg, 
			sizeof(cfg));

	uint8_t *ptr = (uint8_t *)&cfg + 5;
	int total_en = 0;
	for (size_t i = 0; i < 6; i++) {
		ptr += 3;
		if (ptr[i] == 1)
			total_en++;
	}
	if (total_en > 1) {
		fprintf(stderr, "Cannot enable more than one link error injection command at a time.\n");
		return -1;
	}
	if (total_en == 0) {
		fprintf(stderr, "Must enable one link error injection command.\n");
		return -1;
	}

	if (cfg.enable && !(cfg.inject_dllp_crc || cfg.inject_tlp_lcrc))
		printf("Ignoring -e enable flag, not valid for the currently selected command.\n");
	if (!cfg.inject_nack && cfg.count)
		printf("Ignoring -c flag, not valid for the currently selected command.\n");
	if (!cfg.inject_nack && cfg.seq_num)
		printf("Ignoring -S flag, not valid for the currently selected command.\n");
	if (!cfg.inject_tlp_lcrc && cfg.tlp_rate)
		printf("Ignoring -R flag, not valid for the currently selected command.\n");
	if (!cfg.inject_dllp_crc && cfg.dllp_rate)
		printf("Ignoring -r flag, not valid for the currently selected command.\n");
	if (!cfg.inject_dllp && cfg.dllp_data) {
		printf("Ignoring -i flag, not valid for the currently selected command.\n");
	} else if (cfg.inject_dllp && cfg.dllp_data) {
		ret = convert_hex_str(cfg.dllp_data, &dllp_data_dword, 
				    	    &num_dwords, 8);
		if (ret) {
			fprintf(stderr, "Error with DLLP data provided\n");
			return -1;
		}
		if (num_dwords == 0) {
			fprintf(stderr, "Must provide a single valid DLLP data dword\n");
			free(dllp_data_dword);
			return -1;
		}
	}

	if (cfg.dllp_rate && cfg.tlp_rate) {
		fprintf(stderr, "Cannot enable both rate configurations.\n");
		return -1;
	}

	if (cfg.inject_dllp) {
		ret = switchtec_inject_err_dllp(cfg.dev, cfg.phy_port, 
						cfg.dllp_data != NULL ? *dllp_data_dword : 0);
		free(dllp_data_dword);
	}
	if (cfg.inject_dllp_crc) {
		if (cfg.dllp_rate > 4096) {
			fprintf(stderr, "DLLP CRC rate out of range. Valid range is 0-4095.\n");
			return -1;
		}
		ret = switchtec_inject_err_dllp_crc(cfg.dev, cfg.phy_port, 
						    cfg.enable, cfg.dllp_rate);
	}
	if (cfg.inject_tlp_lcrc) {
		if (cfg.tlp_rate > 7) {
			fprintf(stderr, "TLP LCRC rate out of range. Valid range is 0-7.\n");
			return -1;
		}
		ret = switchtec_inject_err_tlp_lcrc(cfg.dev, cfg.phy_port, 
						    cfg.enable, cfg.tlp_rate);
		if (ret)
			return -1;
	}
	if (cfg.inject_tlp_seq)
		ret = switchtec_inject_err_tlp_seq_num(cfg.dev, cfg.phy_port);
	if (cfg.inject_nack) {
		if (cfg.seq_num > 4095) {
			fprintf(stderr, "Sequence number out of range. Valid range is 0-4095).\n");
			return -1;
		}
		if (cfg.count > 255) {
			fprintf(stderr, "Count out of range. Valid range is 0-255\n");
			return -1;
		}
		ret = switchtec_inject_err_ack_nack(cfg.dev, cfg.phy_port, 
						    cfg.seq_num, cfg.count);
	}
	if (cfg.inject_cto) {
		if (!switchtec_is_gen5(cfg.dev)) {
			fprintf(stderr, "Credit timeout error injection is only supported on Gen5.\n");
			return -1;
		}
		ret = switchtec_inject_err_cto(cfg.dev, cfg.phy_port);
	}

	switchtec_perror("linkerr-inject");
	return ret;
}

#define CMD_ORDERED_SET_ANALYZER "Ordered set analyzer"

static int osa(int argc, char **argv)
{
	int ret = 0;
	static struct {
		struct switchtec_dev *dev;
		int stack_id;
		int operation;
	} cfg;
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"stack_id", 's', "STACK_ID", CFG_INT, &cfg.stack_id, 
		required_argument,"ID of the stack (0-5), 7 for mangement stack"},
		{"operation", 'o', "0/1/2/3/4/5", CFG_INT, &cfg.operation, 
		required_argument,"operations:\n- stop:0\n- start:1\n- trigger:2\n- reset:3\n- release:4\n- status:5"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_ORDERED_SET_ANALYZER, opts, &cfg, 
			sizeof(cfg));

	if (cfg.stack_id < 0 || (cfg.stack_id > 5 && cfg.stack_id != 7)) {
		fprintf(stderr, "Invalid stack ID.\n");
		return -1;
	}

	if (cfg.operation > 5 || cfg.operation < 0) {
		printf("Invalid operation!\n");
		return -1;
	}

	ret = switchtec_osa(cfg.dev, cfg.stack_id, cfg.operation);
	if (ret) {
		switchtec_perror("osa");
		return -1;
	}
	return 0;
}

#define CMD_ORDERED_SET_ANALYZER_CONF "Ordered set analyzer configure type"

static int osa_config_type(int argc, char **argv)
{
	int ret = 0;
	uint32_t * lane_mask = NULL;
	uint32_t * direction_mask = NULL;
	uint32_t * link_rate_mask = NULL;
	uint32_t * os_type_mask = NULL;
	int num_dwords = 0;
	static struct {
		struct switchtec_dev *dev;
		int stack_id;
		char *lane_mask;
		char *direction;
		char *link_rate;
		char *os_types;
	} cfg;
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"stack_id", 's', "STACK_ID", CFG_INT, &cfg.stack_id, 
		required_argument,"ID of the stack (0-5), 7 for mangement stack"},
		{"lane_mask", 'm', "LANE_MASK", CFG_STRING, &cfg.lane_mask, 
		required_argument,
		"16 bit lane mask, 1 enables the triggering for that specified lane. " \
		"(If left blank defaults to all bits set to 0). Input as a hexidecimal value prefixed with 0x"},
		{"direction", 'd', "DIRECTION", CFG_STRING, &cfg.direction, 
		required_argument,
		"3 bit mask for the direction, 1 enables the correisponding direction. " \
		"(If left blank defaults to all bits set to 0). Input as a hexidecimal value prefixed with 0x\nBit 0 : tx\nBit 1 : rx"},
		{"link_rate", 'r', "LINK_RATE", CFG_STRING, &cfg.link_rate, 
		required_argument,
		"5 bit mask for link rate, 1 enables the corrisponding link rate. " \
		"(If left blank defaults to all bits set to 0). Input as a hexidecimal value prefixed with " \
		"0x\nBit 0 : Gen1\nBit 1 : Gen2\nBit 2 : Gen3\nBit 3 : Gen4\nBit 4 : Gen5"},
		{"os_types", 't', "OS_TYPES", CFG_STRING, &cfg.os_types, 
		required_argument,
		"4 bit mask for OS types, 1 enables the corrisponding OS type. "\
		"(If left blank defaults to all bits set to 0). Input as a hexidecimal value prefixed with "\
		"0x\nBit 0 : TS1\nBit 1 : TS2\nBit 2 : FTS\nBit 3 : CTL_SKP"},
		{NULL}};
	
	argconfig_parse(argc, argv, CMD_ORDERED_SET_ANALYZER_CONF, opts, &cfg, 
			sizeof(cfg));

	if (cfg.stack_id < 0 || (cfg.stack_id > 5 && cfg.stack_id != 7)) {
		fprintf(stderr, "Invalid stack ID.\n");
		return -1;
	}

	if (cfg.lane_mask) {
		ret = convert_hex_str(cfg.lane_mask, &lane_mask, &num_dwords, 4);
		if (ret) {
			fprintf(stderr, "Error with lane mask.\n");
			return -1;
		}
	}
	if (cfg.direction) {
		ret = convert_hex_str(cfg.direction, &direction_mask, 
				      &num_dwords, 1);
		if (ret) {
			fprintf(stderr, "Error with direction mask.\n");
			return -1;
		}
		if (*direction_mask > 3) {
			fprintf(stderr, "Direction mask cannot be greater than 0x3.\n");
			free(lane_mask);
			free(direction_mask);
			return -1;
		}
	}
	if (cfg.link_rate) {
		ret = convert_hex_str(cfg.link_rate, &link_rate_mask, 
				      &num_dwords, 2);
		if (ret) {
			fprintf(stderr, "Error with link rate mask.\n");
			return -1;
		}
		if (*link_rate_mask > 31) {
			fprintf(stderr, "Link rate cannot be greater than 0x1F.\n");
			free(lane_mask);
			free(direction_mask);
			free(link_rate_mask);
			return -1;
		}
	}
	if (cfg.os_types) {
		ret = convert_hex_str(cfg.os_types, &os_type_mask, 
				      &num_dwords, 1);
		if (ret) {
			fprintf(stderr, "Error with OS type mask.\n");
			free(lane_mask);
			free(direction_mask);
			free(link_rate_mask);
			return -1;
		}
	}

	ret = switchtec_osa_config_type(cfg.dev, cfg.stack_id, 
					direction_mask != NULL ? *direction_mask : 0, 
					lane_mask != NULL ? *lane_mask : 0, 
					link_rate_mask != NULL ? *link_rate_mask : 0, 
					os_type_mask != NULL ? *os_type_mask : 0);
	free(lane_mask);
	free(direction_mask);
	free(link_rate_mask);
	free(os_type_mask);

	if (ret) {
		switchtec_perror("osa_config_type");
		return -1;
	}
	return 0;
}

#define CMD_ORDERED_SET_ANALYZER_PAT_CONF "Ordered set analyzer configure pattern"

static int osa_config_pat(int argc, char **argv)
{
	int ret = 0;
	uint32_t * value_dwords_arr = NULL;
	uint32_t * mask_dwords_arr = NULL;
	uint32_t * lane_mask = NULL;
	uint32_t * direction_mask = NULL;
	uint32_t * link_rate_mask = NULL;
	int num_dwords = 0;
	int total_dwords = 0;
	static struct {
		struct switchtec_dev *dev;
		int stack_id;
		char *direction;
		char *lane_mask;
		char *link_rate;
		char *value_dwords;
		char *mask_dwords;
	} cfg;
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"stack_id", 's', "STACK_ID", CFG_NONNEGATIVE, &cfg.stack_id, 
		required_argument,"ID of the stack (0-5), 7 for mangement stack"},
		{"lane_mask", 'm', "LANE_MASK", CFG_STRING, &cfg.lane_mask, 
		required_argument,
		"16 bit lane mask, 1 enables the triggering for that specified lane. "\
		"(If left blank defaults to all bits set to 0). Input as a hexidecimal value prefixed with 0x"},
		{"direction", 'd', "DIRECTION", CFG_STRING, &cfg.direction, 
		required_argument,
		"3 bit mask for the direction, 1 enables the correisponding direction. "\
		"(If left blank defaults to all bits set to 0). Input as a hexidecimal value prefixed with 0x\nBit 0 : tx\nBit 1 : rx"},
		{"link_rate", 'r', "LINK_RATE", CFG_STRING, &cfg.link_rate, 
		required_argument,
		"5 bit mask for link rate, 1 enables the corrisponding link rate. "\
		"(If left blank defaults to all bits set to 0). Input as a hexidecimal value "\
		"prefixed with 0x\nBit 0 : Gen1\nBit 1 : Gen2\nBit 2 : Gen3\nBit 3 : Gen4\nBit 4 : Gen5"},
		{"dwords_value", 'V', "\"val_dword0 val_dword1 etc.\"", CFG_STRING, 
		&cfg.value_dwords, required_argument, 
		"(Maximum 4 DWs) Dwords should be surrounded by quotations, each "\
		"dword must begine with \"0x\" and each dword must have a space between them."},
		{"dwords_mask", 'M', "\"val_dword0 val_dword1 etc.\"", CFG_STRING, 
		&cfg.mask_dwords, required_argument, 
		"(Maximum 4 DWs) Dwords should be surrounded by quotations, and "\
		"each dword must begine with \"0x\" and each dword must have a space between them."},
		{NULL}};

	argconfig_parse(argc, argv, CMD_ORDERED_SET_ANALYZER_CONF, opts, &cfg, 
			sizeof(cfg));
	
	if (cfg.stack_id < 0 || (cfg.stack_id > 5 && cfg.stack_id != 7)) {
		fprintf(stderr, "Invalid stack ID.\n");
		return -1;
	}

	if (cfg.lane_mask) {
		ret = convert_hex_str(cfg.lane_mask, &lane_mask, &num_dwords, 4);
		if (ret) {
			fprintf(stderr, "Error with lane mask.\n");
			return -1;
		}
	}
	if (cfg.direction) {
		ret = convert_hex_str(cfg.direction, &direction_mask, 
				      &num_dwords, 1);
		if (ret) {
			fprintf(stderr, "Error with direction mask.\n");
			return -1;
		}
		if (*direction_mask > 3) {
			fprintf(stderr, "Direction mask cannot be greater than 0x3.\n");
			free(lane_mask);
			free(direction_mask);
			return -1;
		}
	}
	if (cfg.link_rate) {
		ret = convert_hex_str(cfg.link_rate, &link_rate_mask, 
				      &num_dwords, 2);
		if (ret) {
			fprintf(stderr, "Error with link rate mask.\n");
			return -1;
		}
		if (*link_rate_mask > 31) {
			fprintf(stderr, "Link rate cannot be greater than 0x1F.\n");
			free(lane_mask);
			free(direction_mask);
			free(link_rate_mask);
			return -1;
		}
	}
	num_dwords = 0;
	if (cfg.value_dwords == NULL) {
		fprintf(stderr, "Must set value dword data --dwords_value -V \n");
		return -1;
	}
	if (cfg.mask_dwords == NULL) {
		fprintf(stderr, "Must set mask dword data --dwords_mask -M \n");
		return -1;
	}
	ret = convert_hex_str(cfg.value_dwords, &value_dwords_arr, 
			      &num_dwords, 8);
	if (ret) {
		fprintf(stderr, "Error with data provided \n");
		return -1;
	}
	total_dwords += num_dwords;
	num_dwords = 0;
	ret = convert_hex_str(cfg.mask_dwords, &mask_dwords_arr, 
				    &num_dwords, 8);
	total_dwords += num_dwords;
	if (ret) {
		fprintf(stderr, "Error with data provided \n");
		return -1;
	}
	if (total_dwords > 8) {
		fprintf(stderr, "Total data (values + mask) cannot exceed 8 dwords \n");
		free(value_dwords_arr);
		free(mask_dwords_arr);
		return -1;
	}

	ret = switchtec_osa_config_pattern(cfg.dev, cfg.stack_id, 
					   direction_mask != NULL ? *direction_mask : 0, 
					   lane_mask != NULL ? *lane_mask : 0, 
					   link_rate_mask != NULL ? *link_rate_mask : 0, 
					   value_dwords_arr, mask_dwords_arr);
	free(lane_mask);
	free(direction_mask);
	free(link_rate_mask);
	free(value_dwords_arr);
	free(mask_dwords_arr);
	if (ret) {
		switchtec_perror("osa_config_pat");
		return -1;
	}
	return 0;
}

#define CMD_ORDERED_SET_ANALYZER_MISC_CONF "Ordered set analyzer configure misc"

static int osa_config_misc(int argc, char **argv)
{
	int ret = 0;
	uint32_t * trigger_mask = NULL;
	int num_dwords = 0;
	static struct {
		struct switchtec_dev *dev;
		int stack_id;
		char *trigger_en;
	} cfg;
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"stack_id", 's', "STACK_ID", CFG_INT, &cfg.stack_id, 
		required_argument,"ID of the stack (0-5), 7 for mangement stack"},
		{"trigger_en", 't', "ENABLED", CFG_STRING, &cfg.trigger_en,
		required_argument,
		"3 bit mask for trigger enable, 1 enables the correisponding trigger. "\
		"(If left blank defaults to all bits set to 0). Input as a hexidecimal "\
		"value prefixed with 0x\nBit 0 : LTMON/other hardware blocks\nBit 1 : Reserved\nBit 2 : General purpose input"}, 
		{NULL}};
	
	argconfig_parse(argc, argv, CMD_ORDERED_SET_ANALYZER_MISC_CONF, 
			opts, &cfg, sizeof(cfg));

	if (cfg.stack_id < 0 || (cfg.stack_id > 5 && cfg.stack_id != 7)) {
		fprintf(stderr, "Invalid stack ID.\n");
		return -1;
	}

	if (cfg.trigger_en) {
		ret = convert_hex_str(cfg.trigger_en, &trigger_mask, 
				      &num_dwords, 1);
		if (ret) {
			fprintf(stderr, "Error with trigger mask \n");
			return -1;
		}
		if (*trigger_mask > 7) {
			fprintf(stderr, "Trigger mask cannot be greater than 0x7.\n");
			free(trigger_mask);
			return -1;
		}
	}
	ret = switchtec_osa_config_misc(cfg.dev, cfg.stack_id, 
					trigger_mask != NULL ? *trigger_mask : 0);
	free(trigger_mask);
	if (ret) {
		switchtec_perror("osa_config_misc");
		return -1;
	}
	return 0;
}

#define CMD_ORDERED_SET_ANALYZER_CAP_CTRL "Ordered set analyzer capture control"

static int osa_capture_contol(int argc, char **argv)
{
	int ret = 0;
	uint32_t * os_type_mask = NULL;
	uint32_t * lane_mask = NULL;
	uint32_t * direction_mask = NULL;
	int num_dwords = 0;
	static struct {
		struct switchtec_dev *dev;
		int stack_id;
		char *lane_mask;
		char *direction;
		int drop_single_os;
		int stop_mode;
		int snapshot_mode;
		int post_trig_entries;
		char *os_types;
	} cfg;

	cfg.stop_mode = 0;
	cfg.drop_single_os = 0;
	cfg.snapshot_mode = 0;
	cfg.post_trig_entries = 0;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"stack_id", 's', "STACK_ID", CFG_INT, &cfg.stack_id, 
		required_argument,"ID of the stack (0-5), 7 for mangement stack"},
		{"lane_mask", 'm', "LANE_MASK", CFG_STRING, &cfg.lane_mask, 
		required_argument,
		"16 bit lane mask, 1 enables the triggering for that specified lane. "\
		"(If left blank defaults to all bits set to 0). Input as a hexidecimal value prefixed with 0x"},
		{"direction", 'd', "DIRECTION", CFG_STRING, &cfg.direction, 
		required_argument,
		"3 bit mask for the direction, 1 enables the correisponding direction. "\
		"(If left blank defaults to all bits set to 0). Input as a hexidecimal value prefixed with 0x\nBit 0 : tx\nBit 1 : rx"},
		{"drop_single_os", 'o', "", CFG_NONE, &cfg.drop_single_os, 
		no_argument, 
		"When set to 1, the single TS1, TS2, FTS, and CTL_SKP OS's are excluded from the capture."},
		{"stop_mode", 'S', "", CFG_NONE, &cfg.stop_mode, 
		no_argument, 
		"Controls when the OSA stops capturing. disabled: any lane has stopped, enabled: all lanes have stopped. (Default: disabled)"},
		{"snapshot_mode", 's', "", CFG_NONE, &cfg.snapshot_mode, 
		no_argument, 
		"Enable the snapshot mode setting. When enabled, OS's are captured until the RAM is full. "\
		"If disabled the OS's captured is dictated by the number of Post-Trigger Entries. (default disabled)"},
		{"post_trig_entries", 'p', "POST_TRIG_ENTRIES", CFG_INT, &cfg.post_trig_entries, 
		required_argument, 
		"Number of post trigger OS entries to be captured. Not valid if snapshot_mode is enabled. "\
		"Max 256 entries.\n(Required if disabling --snapshot_mode -s)"},
		{"os_types", 't', "OS_TYPES", CFG_STRING, &cfg.os_types, 
		required_argument,
		"8 bit mask for OS types, 1 enables the corrisponding OS type. "\
		"(If left blank defaults to all bits set to 0). Input as a hexidecimal value prefixed "\
		"with 0x\nBit 0 : TS1\nBit 1 : TS2\nBit 2 : FTS\nBit 3 : CTL_SKP\nBit 4 : SKP\nBit 5 : EIEOS\nBit 6 : EIOS\nBit 7 : ERR_OS"},
		{NULL}};
	
	argconfig_parse(argc, argv, CMD_ORDERED_SET_ANALYZER_CAP_CTRL, opts, &cfg, sizeof(cfg));

	if (cfg.stack_id < 0 || (cfg.stack_id > 5 && cfg.stack_id != 7)) {
		fprintf(stderr, "Invalid stack ID.\n");
		return -1;
	}

	if (cfg.post_trig_entries && cfg.snapshot_mode) {
		fprintf(stderr, "Cannot enable snapshot mode and set the number of post trigger entries.\n");
		fprintf(stderr, "Snapshot mode --snapshot_mode -s enables capturing until the RAM is full, --post_trig_entries -p sets a specified number of entries to capture.\n");
		return -1;
	} else if (cfg.post_trig_entries == 0 && cfg.snapshot_mode == 0) {
		fprintf(stderr, "Must specify a number of OS entries to capture.\n");
		return -1;
	}

	if (cfg.lane_mask) {
		ret = convert_hex_str(cfg.lane_mask, &lane_mask, &num_dwords, 4);
		if (ret) {
			fprintf(stderr, "Error with lane mask.\n");
			return -1;
		}
	}
	if (cfg.direction) {
		ret = convert_hex_str(cfg.direction, &direction_mask, 
				      &num_dwords, 1);
		if (ret) {
			fprintf(stderr, "Error with direction mask.\n");
			return -1;
		}
		if (*direction_mask > 3) {
			fprintf(stderr, "Direction mask cannot be greater than 0x3.\n");
			free(lane_mask);
			free(direction_mask);
			return -1;
		}
	}
	if (cfg.os_types) {
		ret = convert_hex_str(cfg.os_types, &os_type_mask, 
				      &num_dwords, 2);
		if (ret) {
			fprintf(stderr, "Error with OS type mask.\n");
			free(lane_mask);
			free(direction_mask);
			return -1;
		}
	}

	ret = switchtec_osa_capture_control(cfg.dev, cfg.stack_id, 
					    lane_mask != NULL ? *lane_mask : 0, 
					    direction_mask != NULL ? *direction_mask : 0,
					    cfg.drop_single_os, cfg.stop_mode, 
					    cfg.snapshot_mode, cfg.post_trig_entries, 
					    os_type_mask != NULL ? *os_type_mask : 0);
	free(os_type_mask);
	free(lane_mask);
	free(direction_mask);
	if (ret) {
		switchtec_perror("osa_capture_control");
		return -1;
	}
	return 0;
}

#define CMD_ORDERED_SET_ANALYZER_DUMP_CONF "dump osa config"

static int osa_dump_config(int argc, char **argv)
{
	int ret = 0;
	static struct {
		struct switchtec_dev *dev;
		int stack_id;
		int lane_id;
		int direction;
	} cfg;
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"stack_id", 's', "STACK_ID", CFG_INT, &cfg.stack_id, 
		required_argument,"ID of the stack (0-5), 7 for mangement stack"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_ORDERED_SET_ANALYZER, opts, &cfg, 
			sizeof(cfg));

	if (cfg.stack_id < 0 || (cfg.stack_id > 5 && cfg.stack_id != 7)) {
		fprintf(stderr, "Invalid stack ID.\n");
		return -1;
	}

	ret = switchtec_osa_dump_conf(cfg.dev, cfg.stack_id);
	if (ret) {
		switchtec_perror("osa_dump_config");
		return -1;
	}
	return 0;
}

#define CMD_ORDERED_SET_ANALYZER_DUMP_DATA "dump osa data"

static int osa_dump_data(int argc, char **argv)
{
	int ret = 0;
	static struct {
		struct switchtec_dev *dev;
		int stack_id;
		int lane;
		int direction;
	} cfg;
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"stack_id", 's', "STACK_ID", CFG_INT, &cfg.stack_id, 
		required_argument,"ID of the stack (0-5), 7 for mangement stack"},
		{"lane", 'l', "lane", CFG_INT, &cfg.lane, 
		required_argument,"lane ID"},
		{"direction", 'd', "0/1", CFG_INT, &cfg.direction, 
		required_argument,"direction tx: 0 rx: 1"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_ORDERED_SET_ANALYZER, opts, &cfg, 
			sizeof(cfg));
	
	if (cfg.stack_id < 0 || (cfg.stack_id > 5 && cfg.stack_id != 7)) {
		fprintf(stderr, "Invalid stack ID.\n");
		return -1;
	}

	if (cfg.direction > 1) {
		fprintf(stderr, "Direction must be either 0 or 1\n");
		return -1;
	}
	
	ret = switchtec_osa_capture_data(cfg.dev, cfg.stack_id, cfg.lane, 
					 cfg.direction);
	if (ret) {
		switchtec_perror("osa_dump_data");
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
	CMD(aer_event_gen,	CMD_DESC_AER_EVENT_GEN),
	CMD(linkerr_inject,	CMD_DESC_LNKERR_INJECT),
	CMD(osa, 		CMD_ORDERED_SET_ANALYZER),
	CMD(osa_config_type, 	CMD_ORDERED_SET_ANALYZER_CONF),
	CMD(osa_config_pat,	CMD_ORDERED_SET_ANALYZER_PAT_CONF),
	CMD(osa_config_misc,	CMD_ORDERED_SET_ANALYZER_MISC_CONF),
	CMD(osa_capture_contol, CMD_ORDERED_SET_ANALYZER_CAP_CTRL),
	CMD(osa_dump_config,	CMD_ORDERED_SET_ANALYZER_DUMP_CONF),
	CMD(osa_dump_data,	CMD_ORDERED_SET_ANALYZER_DUMP_DATA),
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

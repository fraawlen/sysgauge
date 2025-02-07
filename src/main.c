/**
 * Copyright © 2024 Fraawlen <fraawlen@posteo.net>
 *
 * This file is part of the program.
 *
 * This library is free software; you can redistribute it and/or modify it either under the terms of the GNU
 * Affero General Public License as published by the Free Software Foundation; either version 3.0  of the
 * License or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the LGPL for the specific language governing rights and limitations.
 *
 * You should have received a copy of the GNU Lesser General Public License along with this program. If not,
 * see <http://www.gnu.org/licenses/>.
 */

/************************************************************************************************************/
/************************************************************************************************************/
/************************************************************************************************************/

#include <cassette/cgui.h>
#include <cassette/cobj.h>
#include <float.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <unistd.h>

/************************************************************************************************************/
/************************************************************************************************************/
/************************************************************************************************************/

#define PROGRAM "sysgauges"
#define VERSION "v.2.0.0"
#define GB      * data.mem_unit / 1073741824.0

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

struct row
{
	const char *name;
	const char *unit;
	const int precision;
	const bool custom_max;
	cgui_cell *label;
	cgui_cell *gauge;
	cgui_cell *max;
};

/************************************************************************************************************/
/************************************************************************************************************/
/************************************************************************************************************/

static void  help        (void);
static void  on_exit     (void);
static void  on_run      (void);
static void  options     (int, char**);
static void  resize      (void);
static void  row_destroy (struct row *);
static void  row_setup   (struct row *, double);
static void  row_update  (struct row *, double, double);
static void *thread      (void *);
static void  update_all  (void);

/************************************************************************************************************/
/************************************************************************************************************/
/************************************************************************************************************/

/* - User parameters - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

static bool          show_max = false;
static bool          verbose  = false;
static double        alert    = 0.95;
static unsigned int  delay    = 1;
static unsigned long width    = 0;
static unsigned long height   = 0;
static long          x        = 20;
static long          y        = 20;

/* GUI components  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

static cgui_window *window = CGUI_WINDOW_PLACEHOLDER;
static cgui_grid   *grid   = CGUI_GRID_PLACEHOLDER;
static cstr        *str    = CSTR_PLACEHOLDER;

static struct row cpu = { "CPU", "%",  1, false, CGUI_CELL_PLACEHOLDER, CGUI_CELL_PLACEHOLDER, CGUI_CELL_PLACEHOLDER };
static struct row mem = { "MEM", "GB", 1, true,  CGUI_CELL_PLACEHOLDER, CGUI_CELL_PLACEHOLDER, CGUI_CELL_PLACEHOLDER };
static struct row swp = { "SWP", "GB", 1, true,  CGUI_CELL_PLACEHOLDER, CGUI_CELL_PLACEHOLDER, CGUI_CELL_PLACEHOLDER };

static size_t pos = 0;
static pthread_t t;

/************************************************************************************************************/
/* MAIN *****************************************************************************************************/
/************************************************************************************************************/

int
main(int argc, char **argv)
{
	struct sysinfo data;

	/* Setup */

	cgui_init(argc, argv);
	options(argc, argv);
	sysinfo(&data);

	window = cgui_window_create();
	grid   = cgui_grid_create(3, data.totalswap > 0 ? 3 : 2);
	str    = cstr_create();

	/* Grid configuration */

	cgui_grid_set_row_flex(grid, 0, 1.0);
	cgui_grid_set_row_flex(grid, 1, 1.0);
	cgui_grid_set_row_flex(grid, 2, 1.0);
	cgui_grid_set_col_flex(grid, 1, 1.0);

	cgui_grid_resize_col(grid, 0, 3);
	cgui_grid_resize_col(grid, 1, 6);
	cgui_grid_resize_col(grid, 2, 6);

	/* Rows configuration */

	row_setup(&cpu, 100.0);
	row_setup(&mem, data.totalram  GB);
	row_setup(&swp, data.totalswap GB);

	/* Window configuration */

	cgui_window_push_grid(window, grid);
	resize();

	cgui_window_rename(window, "sysmeter");
	cgui_window_set_type(window, CGUI_WINDOW_UNDERLAY);
	cgui_window_activate(window);

	/* Run */

	cgui_on_run(on_run);
	cgui_on_exit(on_exit);
	cgui_run();

	/* Cleanup & end */

	cgui_window_destroy(window);
	cgui_grid_destroy(grid);
	cstr_destroy(str);

	row_destroy(&cpu);
	row_destroy(&mem);
	row_destroy(&swp);

	cgui_reset();

	return 0;
}

/************************************************************************************************************/
/* STATIC ***************************************************************************************************/
/************************************************************************************************************/

static void
help(void)
{
	printf(
		PROGRAM " " VERSION "\n"
		"usage: " PROGRAM " [option] <value>\n"
		"\t-a <0.0..1.0> : alert threshold\n"
		"\t-h            : print this help\n"
		"\t-H <ulong>    : custom height\n"
		"\t-i <uint>     : update interval in seconds\n"
		"\t-m            : show max MEM and SWP values\n"
		"\t-v            : print extra information (window width and height)\n"
		"\t-w <ulong>    : custom width\n"
		"\t-x <long>     : custom x coordinate\n"
		"\t-y <long>     : custom y coordinate\n");
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

static void
on_exit(void)
{
	pthread_join(t, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

static void
on_run(void)
{
	pthread_create(&t, NULL, thread, NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

static void
options(int argc, char **argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "a:hH:i:mvw:x:y:")) != -1)
	{
		switch (opt)
		{
			case 'a':
				alert = strtod(optarg, NULL);
				break;

			case 'h':
				help();
				exit(0);

			case 'H':
				height = strtoul(optarg, NULL, 0);
				break;

			case 'i':
				delay = strtoul(optarg, NULL, 0);
				break;

			case 'm':
				show_max = true;
				break;

			case 'v':
				verbose = true;
				break;

			case 'w':
				width = strtoul(optarg, NULL, 0);
				break;

			case 'x':
				x = strtol(optarg, NULL, 0);
				break;

			case 'y':
				y = strtol(optarg, NULL, 0);
				break;

			case '?':
			default :
				fprintf(stderr, "try \'" PROGRAM " -h\' for more information\n");
				exit(0);
		}
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

static void
resize(void)
{
	cgui_window_resize(window, width, height);
	cgui_window_move_smart(window, x, y, x, y);

	if (verbose)
	{
		printf("window size updated\n");
		printf("width  = %.0f\n", cgui_window_width(window));
		printf("height = %.0f\n", cgui_window_height(window));
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

static void
row_destroy(struct row *r)
{
	cgui_cell_destroy(r->label);
	cgui_cell_destroy(r->gauge);
	cgui_cell_destroy(r->max);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

static void
row_setup(struct row *r, double max)
{
	r->label = cgui_beacon_create();
	r->gauge = cgui_gauge_create();
	r->max   = cgui_label_create();

	if (max <= DBL_EPSILON)
	{
		return;
	}

	cstr_clear(str);
	cstr_set_precision(str, r->precision);
	cstr_append(str, max);
	cstr_append(str, r->unit);

	cgui_gauge_set_precision(r->gauge, r->precision);
	cgui_gauge_set_units(r->gauge, r->unit);
	cgui_gauge_clamp_value(r->gauge, 0.0, max);
	cgui_label_set(r->max, cstr_chars(str));
	cgui_beacon_set_label(r->label, r->name);

	cgui_grid_assign_cell(grid, r->label, 0, pos, 1, 1);
	if (show_max && r->custom_max)
	{
		cgui_grid_assign_cell(grid, r->gauge, 1, pos, 1, 1);
		cgui_grid_assign_cell(grid, r->max,   2, pos, 1, 1);
	}
	else
	{
		cgui_grid_assign_cell(grid, r->gauge, 1, pos, 2, 1);
	}

	pos++;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

static void
row_update(struct row *r, double val, double high)
{
	cgui_gauge_set_value(r->gauge, val);
	cgui_beacon_set_state(r->label, val >= high ? CGUI_BEACON_ON : CGUI_BEACON_OFF);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

static void *
thread(void *params)
{
	bool run = true;

	(void)params;

	while (run)
	{
		cgui_lock();
		update_all();
		run = cgui_is_running();
		cgui_unlock();
		sleep(delay);
	}

	pthread_exit(NULL);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

static void
update_all(void)
{
	struct sysinfo data;

	sysinfo(&data);

	row_update(&cpu, data.loads[0] * 100.0 / get_nprocs() / (1 << SI_LOAD_SHIFT), 100.0 * alert);
	row_update(&mem, (data.totalram  - data.freeram)  GB, data.totalram  GB * alert);
	row_update(&swp, (data.totalswap - data.freeswap) GB, data.totalswap GB * alert);
}

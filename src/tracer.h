/*
 * Copyright © 2014 Boyan Ding
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef TRACER_H
#define TRACER_H

#include <stdio.h>
#include "wayland-util.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define TRACER_SERVER_SIDE 0
#define TRACER_CLIENT_SIDE 1

#define TRACER_MODE_SINGLE 0
#define TRACER_MODE_SERVER 1

#define TRACER_OUTPUT_RAW 0
#define TRACER_OUTPUT_INTERPRET 1

#define tracer_log(...) tracer_log_impl(instance, __VA_ARGS__)
#define tracer_log_cont(...) tracer_log_cont_impl(instance, __VA_ARGS__)
#define tracer_log_end() tracer_log_end_impl(instance)

struct tracer;
struct tracer_instance;

struct tracer_connection {
	struct wl_connection *wl_conn;
	struct tracer_connection *peer;
	struct tracer_instance *instance;
	int side;
};

struct tracer_frontend_interface {
	int (*init)(struct tracer *);
	int (*data)(struct tracer_connection *, int);
};

struct tracer_instance {
	int id;
	struct tracer_connection *client_conn;
	struct tracer_connection *server_conn;
	struct tracer *tracer;
	struct wl_list link;
	struct wl_map map;
};

struct tracer_socket;

struct protocol_file {
	const char *loc;
	struct wl_list link;
};

struct tracer_options {
	int mode;
	int output_format;
	char **spawn_args;
	char *socket;
	const char *outfile;
	struct wl_list protocol_file_list;
};

struct tracer {
	struct tracer_socket *socket;
	int32_t epollfd;
	int next_id;
	struct wl_list instance_list;
	struct wl_list protocol_list;
	struct tracer_frontend_interface *frontend;
	void *frontend_data;
	FILE *outfp;
	struct tracer_options *options;
};

void tracer_print(struct tracer *tracer, const char *fmt, ...);
void tracer_vprint(struct tracer *tracer, const char *fmt, va_list ap);
void tracer_log_impl(struct tracer_instance *instance, const char *fmt, ...);
void tracer_log_cont_impl(struct tracer_instance *instance, const char *fmt, ...);
void tracer_log_end_impl(struct tracer_instance *instance);

#ifdef __cplusplus
}
#endif

#endif

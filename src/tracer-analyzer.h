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

#ifndef TRACER_ANALYZER_H
#define TRACER_ANALYZER_H

#include "wayland-util.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct location {
	const char *filename;
	int line_number;
};

struct tracer_message;

struct tracer_interface {
	struct location loc;
	char *name;
	int type_index;
	struct wl_list request_list;
	struct wl_list event_list;
	struct wl_list link;
	struct tracer_message **methods;
	struct tracer_message **events;
	int method_count, event_count;
};

struct tracer_message {
	struct location loc;
	char *name;
	struct wl_list arg_list;
	struct wl_list link;
	int arg_count;
	int new_id_count;
	int destructor;
	char *new_interface_name;
	struct tracer_interface **types;
	char *signature;
};

struct parse_context;

struct tracer_analyzer {
	struct tracer_interface **interfaces;
	struct tracer_interface *display_interface;
	struct parse_context *ctx;
	struct wl_list interface_list;
};

struct tracer_analyzer *tracer_analyzer_create(void);

int tracer_analyzer_add_protocol(struct tracer_analyzer *analyzer,
				 const char *filename);

struct tracer_interface **
tracer_analyzer_lookup_type(struct tracer_analyzer *analyzer, char *type_name);

int tracer_analyzer_finalize(struct tracer_analyzer *analyzer);


#ifdef __cplusplus
}
#endif

#endif

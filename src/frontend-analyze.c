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

#include <stdio.h>
#include <string.h>

#include "wayland-private.h"
#include "wayland-util.h"
#include "tracer.h"
#include "frontend-analyze.h"
#include "tracer-analyzer.h"

#define DIV_ROUNDUP(n, a) ( ((n) + ((a) - 1)) / (a) )

static int
analyze_init(struct tracer *tracer)
{
	struct tracer_analyzer *analyzer;
	struct protocol_file *file;
	struct tracer_options *options = tracer->options;

	analyzer = tracer_analyzer_create();
	if (analyzer == NULL) {
		fprintf(stderr, "Failed to create analyzer: %m\n");
		return -1;
	}

	wl_list_for_each(file, &options->protocol_file_list, link) {
		if (tracer_analyzer_add_protocol(analyzer,
						 file->loc) != 0) {
			fprintf(stderr, "failed to add file %s\n",
				file->loc);
			return -1;
		}
	}

	if (tracer_analyzer_finalize(analyzer) != 0)
		return -1;

	tracer->frontend_data = analyzer;

	return 0;
}

static int
analyze_protocol(struct tracer_connection *connection,
		 uint32_t size,
		 struct wl_map *objects,
		 struct tracer_interface *target,
		 uint32_t id,
		 struct tracer_message *message)
{
	uint32_t length, new_id, name;
	int fd;
	unsigned int i, count;
	const char *signature;
	char *type_name;
	char buf[4096];
	uint32_t *p = (uint32_t *) buf + 2;
	struct tracer_connection *peer = connection->peer;
	struct tracer_instance *instance = connection->instance;
	struct tracer *tracer = connection->instance->tracer;
	struct tracer_analyzer *analyzer;
	struct tracer_interface *type;
	struct tracer_interface **ptype;

	analyzer = (struct tracer_analyzer *) tracer->frontend_data;

	wl_connection_copy(connection->wl_conn, buf, size);
	if (target == NULL)
		goto finish;

	count = strlen(message->signature);

	tracer_log("%s %s@%u.%s(",
		   connection->side == TRACER_CLIENT_SIDE ? "<=" : "=>",
		   target->name,
		   id,
		   message->name);

	signature = message->signature;
	for (i = 0; i < count; i++) {
		if (i != 0)
			tracer_log_cont(", ");

		switch (*signature) {
		case 'u':
			tracer_log_cont("%u", *p++);
			break;
		case 'i':
			tracer_log_cont("%i", *p++);
			break;
		case 'f':
			tracer_log_cont("%lf", wl_fixed_to_double(*p++));
			break;
		case 's':
			length = *p++;

			if (length == 0)
				tracer_log_cont("(null)");
			else
				tracer_log_cont("\"%s\"", (char *) p);
			p = p + DIV_ROUNDUP(length, sizeof *p);
			break;
		case 'o':
			tracer_log_cont("obj %u", *p++);
			break;
		case 'n':
			new_id = *p++;
			if (new_id != 0) {
				wl_map_reserve_new(objects, new_id);
				wl_map_insert_at(objects, 0, new_id, message->types[0]);
			}
			tracer_log_cont("new_id %u", new_id);
			break;
		case 'a':
			length = *p++;
			tracer_log_cont("array: %u", length);
			p = p + DIV_ROUNDUP(length, sizeof *p);
			break;
		case 'h':
			wl_buffer_copy(&connection->wl_conn->fds_in,
				       &fd,
				       sizeof fd);
			connection->wl_conn->fds_in.tail += sizeof fd;
			tracer_log_cont("fd %d", fd);
			wl_connection_put_fd(peer->wl_conn, fd);
			break;
		case 'N': /* N = sun */
			length = *p++;
			if (length != 0)
				type_name = (char *) p;
			else
				type_name = NULL;
			p = p + DIV_ROUNDUP(length, sizeof *p);

			name = *p++;

			new_id = *p++;
			if (new_id != 0) {
				wl_map_reserve_new(objects, new_id);
				ptype = tracer_analyzer_lookup_type(analyzer,
								    type_name);
				type = ptype == NULL ? NULL : *ptype;
				wl_map_insert_at(objects, 0, new_id, type);
			}
			tracer_log_cont("new_id %u[%s,%u]",
					new_id, type_name, name);
			break;
		}

		signature++;
	}

	tracer_log_cont(")");
	tracer_log_end();
finish:
	wl_connection_write(peer->wl_conn, buf, size);
	wl_connection_consume(connection->wl_conn, size);

	return 0;
}

static int
analyze_handle_data(struct tracer_connection *connection, int len)
{
	uint32_t p[2], id;
	int opcode, size;
	struct tracer_instance *instance = connection->instance;
	struct tracer_interface *interface;
	struct tracer_message *message;

	wl_connection_copy(connection->wl_conn, p, sizeof p);
	id = p[0];
	opcode = p[1] & 0xffff;
	size = p[1] >> 16;
	if (len < size)
		return 0;

	interface = wl_map_lookup(&instance->map, id);

	if (interface != NULL) {
		if (connection->side == TRACER_SERVER_SIDE)
			message = interface->events[opcode];
		else
			message = interface->methods[opcode];
	} else {
		tracer_log("Unknown object %u opcode %u, size %u",
			   id, opcode, size);
		tracer_log_cont("\nWarning: we can't guarentee the following result");
		tracer_log_end();
	}

	analyze_protocol(connection, size, &instance->map,
				interface, id, message);

	if (interface != NULL && !strcmp(message->name, "destroy"))
		wl_map_remove(&instance->map, id);

	return size;
}

struct tracer_frontend_interface tracer_frontend_analyze = {
	.init = analyze_init,
	.data = analyze_handle_data
};

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

#include "wayland-private.h"
#include "tracer.h"
#include "frontend-bin.h"

static int
bin_init(struct tracer *tracer)
{
	return 0;
}

static int
bin_handle_data(struct tracer_connection *connection, int rlen)
{
	int i, len, fdlen, fd;
	char buf[4096];
	struct wl_connection *wl_conn= connection->wl_conn;
	struct tracer_connection *peer = connection->peer;
	struct tracer_instance *instance = connection->instance;
	struct tracer *tracer = instance->tracer;

	len = wl_buffer_size(&wl_conn->in);
	if (len == 0)
		return 0;

	wl_connection_copy(wl_conn, buf, len);

	tracer_log("%s Data dumped: %d bytes:\n",
	           connection->side == TRACER_SERVER_SIDE ? "=>" : "<=",
		   len);
	for (i = 0; i < len; i++)
		tracer_log_cont("%02x ", (unsigned char)buf[i]);
	tracer_log_cont("\n");
	wl_connection_consume(wl_conn, len);
	wl_connection_write(peer->wl_conn, buf, len);

	fdlen = wl_buffer_size(&wl_conn->fds_in);

	wl_buffer_copy(&wl_conn->fds_in, buf, fdlen);
	fdlen /= sizeof(int32_t);

	if (fdlen != 0)
		tracer_log_cont("%d Fds in control data:", fdlen);

	for (i = 0; i < fdlen; i++) {
		fd = ((int *) buf)[i];
		tracer_log_cont("%d ", fd);
		wl_connection_put_fd(peer->wl_conn, fd);
	}
	tracer_log_end();

	wl_conn->fds_in.tail += fdlen * sizeof(int32_t);

	return len;
}

struct tracer_frontend_interface tracer_frontend_bin = {
	.init = bin_init,
	.data = bin_handle_data
};

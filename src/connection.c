/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2013 Jason Ekstrand
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

#define _GNU_SOURCE

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/uio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>

#include "wayland-util.h"
#include "wayland-private.h"
#include "wayland-os.h"

#define DIV_ROUNDUP(n, a) ( ((n) + ((a) - 1)) / (a) )

#define MASK(i) ((i) & 4095)

#define MAX_FDS_OUT	28
#define CLEN		(CMSG_LEN(MAX_FDS_OUT * sizeof(int32_t)))

static int
wl_buffer_put(struct wl_buffer *b, const void *data, size_t count)
{
	uint32_t head, size;

	if (count > sizeof(b->data)) {
		wl_log("Data too big for buffer (%d > %d).\n",
		       count, sizeof(b->data));
		errno = E2BIG;
		return -1;
	}

	head = MASK(b->head);
	if (head + count <= sizeof b->data) {
		memcpy(b->data + head, data, count);
	} else {
		size = sizeof b->data - head;
		memcpy(b->data + head, data, size);
		memcpy(b->data, (const char *) data + size, count - size);
	}

	b->head += count;

	return 0;
}

static void
wl_buffer_put_iov(struct wl_buffer *b, struct iovec *iov, int *count)
{
	uint32_t head, tail;

	head = MASK(b->head);
	tail = MASK(b->tail);
	if (head < tail) {
		iov[0].iov_base = b->data + head;
		iov[0].iov_len = tail - head;
		*count = 1;
	} else if (tail == 0) {
		iov[0].iov_base = b->data + head;
		iov[0].iov_len = sizeof b->data - head;
		*count = 1;
	} else {
		iov[0].iov_base = b->data + head;
		iov[0].iov_len = sizeof b->data - head;
		iov[1].iov_base = b->data;
		iov[1].iov_len = tail;
		*count = 2;
	}
}

static void
wl_buffer_get_iov(struct wl_buffer *b, struct iovec *iov, int *count)
{
	uint32_t head, tail;

	head = MASK(b->head);
	tail = MASK(b->tail);
	if (tail < head) {
		iov[0].iov_base = b->data + tail;
		iov[0].iov_len = head - tail;
		*count = 1;
	} else if (head == 0) {
		iov[0].iov_base = b->data + tail;
		iov[0].iov_len = sizeof b->data - tail;
		*count = 1;
	} else {
		iov[0].iov_base = b->data + tail;
		iov[0].iov_len = sizeof b->data - tail;
		iov[1].iov_base = b->data;
		iov[1].iov_len = head;
		*count = 2;
	}
}

void
wl_buffer_copy(struct wl_buffer *b, void *data, size_t count)
{
	uint32_t tail, size;

	tail = MASK(b->tail);
	if (tail + count <= sizeof b->data) {
		memcpy(data, b->data + tail, count);
	} else {
		size = sizeof b->data - tail;
		memcpy(data, b->data + tail, size);
		memcpy((char *) data + size, b->data, count - size);
	}
}

uint32_t
wl_buffer_size(struct wl_buffer *b)
{
	return b->head - b->tail;
}

struct wl_connection *
wl_connection_create(int fd)
{
	struct wl_connection *connection;

	connection = malloc(sizeof *connection);
	if (connection == NULL)
		return NULL;
	memset(connection, 0, sizeof *connection);
	connection->fd = fd;

	return connection;
}

static void
close_fds(struct wl_buffer *buffer, int max)
{
	int32_t fds[sizeof(buffer->data) / sizeof(int32_t)], i, count;
	size_t size;

	size = buffer->head - buffer->tail;
	if (size == 0)
		return;

	wl_buffer_copy(buffer, fds, size);
	count = size / sizeof fds[0];
	if (max > 0 && max < count)
		count = max;
	for (i = 0; i < count; i++)
		close(fds[i]);
	buffer->tail += size;
}

void
wl_connection_destroy(struct wl_connection *connection)
{
	close_fds(&connection->fds_out, -1);
	close_fds(&connection->fds_in, -1);
	close(connection->fd);
	free(connection);
}

void
wl_connection_copy(struct wl_connection *connection, void *data, size_t size)
{
	wl_buffer_copy(&connection->in, data, size);
}

void
wl_connection_consume(struct wl_connection *connection, size_t size)
{
	connection->in.tail += size;
}

static void
build_cmsg(struct wl_buffer *buffer, char *data, int *clen)
{
	struct cmsghdr *cmsg;
	size_t size;

	size = buffer->head - buffer->tail;
	if (size > MAX_FDS_OUT * sizeof(int32_t))
		size = MAX_FDS_OUT * sizeof(int32_t);

	if (size > 0) {
		cmsg = (struct cmsghdr *) data;
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(size);
		wl_buffer_copy(buffer, CMSG_DATA(cmsg), size);
		*clen = cmsg->cmsg_len;
	} else {
		*clen = 0;
	}
}

static int
decode_cmsg(struct wl_buffer *buffer, struct msghdr *msg)
{
	struct cmsghdr *cmsg;
	size_t size, max, i;
	int overflow = 0;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL;
	     cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (cmsg->cmsg_level != SOL_SOCKET ||
		    cmsg->cmsg_type != SCM_RIGHTS)
			continue;

		size = cmsg->cmsg_len - CMSG_LEN(0);
		max = sizeof(buffer->data) - wl_buffer_size(buffer);
		if (size > max || overflow) {
			overflow = 1;
			size /= sizeof(int32_t);
			for (i = 0; i < size; i++)
				close(((int*)CMSG_DATA(cmsg))[i]);
		} else if (wl_buffer_put(buffer, CMSG_DATA(cmsg), size) < 0) {
				return -1;
		}
	}

	if (overflow) {
		errno = EOVERFLOW;
		return -1;
	}

	return 0;
}

int
wl_connection_flush(struct wl_connection *connection)
{
	struct iovec iov[2];
	struct msghdr msg;
	char cmsg[CLEN];
	int len = 0, count, clen;
	uint32_t tail;

	if (!connection->want_flush)
		return 0;

	tail = connection->out.tail;
	while (connection->out.head - connection->out.tail > 0) {
		wl_buffer_get_iov(&connection->out, iov, &count);

		build_cmsg(&connection->fds_out, cmsg, &clen);

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = iov;
		msg.msg_iovlen = count;
		msg.msg_control = cmsg;
		msg.msg_controllen = clen;
		msg.msg_flags = 0;

		do {
			len = sendmsg(connection->fd, &msg,
				      MSG_NOSIGNAL | MSG_DONTWAIT);
		} while (len == -1 && errno == EINTR);

		if (len == -1)
			return -1;

		close_fds(&connection->fds_out, MAX_FDS_OUT);

		connection->out.tail += len;
	}

	connection->want_flush = 0;

	return connection->out.head - tail;
}

int
wl_connection_read(struct wl_connection *connection)
{
	struct iovec iov[2];
	struct msghdr msg;
	char cmsg[CLEN];
	int len, count, ret;

	if (wl_buffer_size(&connection->in) >= sizeof(connection->in.data)) {
		errno = EOVERFLOW;
		return -1;
	}

	wl_buffer_put_iov(&connection->in, iov, &count);

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = count;
	msg.msg_control = cmsg;
	msg.msg_controllen = sizeof cmsg;
	msg.msg_flags = 0;

	do {
		len = wl_os_recvmsg_cloexec(connection->fd, &msg, MSG_DONTWAIT);
	} while (len < 0 && errno == EINTR);

	if (len <= 0)
		return len;

	ret = decode_cmsg(&connection->fds_in, &msg);
	if (ret)
		return -1;

	connection->in.head += len;

	return connection->in.head - connection->in.tail;
}

int
wl_connection_write(struct wl_connection *connection,
		    const void *data, size_t count)
{
	if (connection->out.head - connection->out.tail +
	    count > ARRAY_LENGTH(connection->out.data)) {
		connection->want_flush = 1;
		if (wl_connection_flush(connection) < 0)
			return -1;
	}

	if (wl_buffer_put(&connection->out, data, count) < 0)
		return -1;

	connection->want_flush = 1;

	return 0;
}

int
wl_connection_queue(struct wl_connection *connection,
		    const void *data, size_t count)
{
	if (connection->out.head - connection->out.tail +
	    count > ARRAY_LENGTH(connection->out.data)) {
		connection->want_flush = 1;
		if (wl_connection_flush(connection) < 0)
			return -1;
	}

	return wl_buffer_put(&connection->out, data, count);
}

int
wl_connection_put_fd(struct wl_connection *connection, int32_t fd)
{
	if (wl_buffer_size(&connection->fds_out) == MAX_FDS_OUT * sizeof fd) {
		connection->want_flush = 1;
		if (wl_connection_flush(connection) < 0)
			return -1;
	}

	return wl_buffer_put(&connection->fds_out, &fd, sizeof fd);
}

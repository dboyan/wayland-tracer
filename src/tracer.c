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
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/un.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include "wayland-os.h"
#include "wayland-private.h"
#include "wayland-util.h"
#include "tracer-analyzer.h"

#define DIV_ROUNDUP(n, a) ( ((n) + ((a) - 1)) / (a) )

#define TRACER_SERVER_SIDE 0
#define TRACER_CLIENT_SIDE 1

#define TRACER_MODE_SINGLE 0
#define TRACER_MODE_SERVER 1

#define TRACER_OUTPUT_RAW 0
#define TRACER_OUTPUT_INTERPRET 1

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

#define LOCK_SUFFIX ".lock"
#define LOCK_SUFFIXLEN 5

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

typedef int (*tracer_dump_func)(struct tracer_connection *, int);

struct tracer_instance {
	int id;
	struct tracer_connection *client_conn;
	struct tracer_connection *server_conn;
	struct tracer *tracer;
	struct wl_list link;
	struct wl_map map;
};

/* A simple copy of wl_socket in wayland-server.c */
struct tracer_socket {
	int fd;
	int fd_lock;
	struct sockaddr_un addr;
	char lock_addr[UNIX_PATH_MAX + LOCK_SUFFIXLEN];
};

struct tracer {
	struct tracer_socket *socket;
	int32_t epollfd;
	int next_id;
	struct wl_list instance_list;
	struct wl_list protocol_list;
	struct tracer_analyzer *analyzer;
	FILE *outfp;
	tracer_dump_func dumper;
};

struct tracer_options {
	int mode;
	int output_format;
	char **spawn_args;
	char *socket;
	const char *outfile;
	struct wl_list protocol_file_list;
};

struct protocol_file {
	const char *loc;
	struct wl_list link;
};

static void
tracer_print(struct tracer *tracer, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(tracer->outfp, fmt, ap);
	va_end(ap);
}

static void
tracer_vprint(struct tracer *tracer, const char *fmt, va_list ap)
{
	vfprintf(tracer->outfp, fmt, ap);
}

static void
tracer_log_impl(struct tracer_instance *instance, const char *fmt, ...)
{
	struct timespec tp;
	unsigned int time;
	struct tracer *tracer = instance->tracer;
	va_list ap;

	clock_gettime(CLOCK_REALTIME, &tp);
	time = (tp.tv_sec * 1000000L) + (tp.tv_nsec / 1000);

	tracer_print(tracer, "[%10.3f] ", time / 1000.0);

	if (tracer->socket != NULL)
		tracer_print(tracer, "%d: ", instance->id);

	va_start(ap, fmt);
	tracer_vprint(tracer, fmt, ap);
	va_end(ap);
}

static void
tracer_log_cont_impl(struct tracer_instance *instance, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	tracer_vprint(instance->tracer, fmt, ap);
	va_end(ap);
}

static void
tracer_log_end_impl(struct tracer_instance *instance)
{
	struct tracer *tracer = instance->tracer;

	tracer_print(tracer, "\n");
	fflush(tracer->outfp);
}

static int
tracer_dump_bin(struct tracer_connection *connection, int rlen)
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

static int
tracer_analyze_protocol(struct tracer_connection *connection,
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
	struct tracer_analyzer *analyzer = tracer->analyzer;
	struct tracer_interface *type;
	struct tracer_interface **ptype;

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
tracer_dump_analyzed(struct tracer_connection *connection, int len)
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
	}

	tracer_analyze_protocol(connection, size, &instance->map,
				interface, id, message);

	if (interface != NULL && !strcmp(message->name, "destroy"))
		wl_map_remove(&instance->map, id);

	return size;
}

/* The following two functions are taken from wayland-client.c*/
static int
tracer_connect_to_socket(const char *name)
{
	struct sockaddr_un addr;
	socklen_t size;
	const char *runtime_dir;
	int name_size, fd;

	runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		fprintf(stderr, "error: XDG_RUNTIME_DIR not set in the environment.\n");
		/* to prevent programs reporting
		 * "failed to create display: Success" */
		errno = ENOENT;
		return -1;
	}

	if (name == NULL)
		name = getenv("WAYLAND_DISPLAY");
	if (name == NULL)
		name = "wayland-0";

	fd = wl_os_socket_cloexec(PF_LOCAL, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_LOCAL;
	name_size =
		snprintf(addr.sun_path, sizeof addr.sun_path,
			 "%s/%s", runtime_dir, name) + 1;

	assert(name_size > 0);
	if (name_size > (int)sizeof addr.sun_path) {
		fprintf(stderr, "error: socket path \"%s/%s\" plus null terminator"
		       " exceeds 108 bytes\n", runtime_dir, name);
		close(fd);
		/* to prevent programs reporting
		 * "failed to add socket: Success" */
		errno = ENAMETOOLONG;
		return -1;
	};

	size = offsetof (struct sockaddr_un, sun_path) + name_size;

	if (connect(fd, (struct sockaddr *) &addr, size) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

static int
tracer_connect_server(const char *name)
{
	char *connection, *end;
	int flags, fd;

	connection = getenv("WAYLAND_SOCKET");
	if (connection) {
		fd = strtol(connection, &end, 0);
		if (*end != '\0')
			return -1;

		flags = fcntl(fd, F_GETFD);
		if (flags != -1)
			fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
		unsetenv("WAYLAND_SOCKET");
	} else
		fd = tracer_connect_to_socket(name);

	return fd;
}

static struct tracer_connection*
tracer_connection_create(int fd, int side)
{
	struct tracer_connection *connection;

	connection = malloc(sizeof *connection);
	if (connection == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	connection->wl_conn = wl_connection_create(fd);
	if (connection->wl_conn == NULL)
		return NULL;

	connection->side = side;

	return connection;
}

static void
tracer_connection_destroy(struct tracer_connection *connection)
{
	struct wl_connection *wl_conn = connection->wl_conn;
	struct tracer *tracer = connection->instance->tracer;

	epoll_ctl(tracer->epollfd, EPOLL_CTL_DEL, wl_conn->fd, NULL);
	wl_connection_destroy(connection->wl_conn);
	free(connection);
}

static int
tracer_epoll_add_fd(struct tracer *tracer, int fd, void *userdata)
{
	struct epoll_event ev;

	ev.events = EPOLLIN;
	ev.data.ptr = userdata;

	return epoll_ctl(tracer->epollfd, EPOLL_CTL_ADD, fd, &ev);
}

static int
tracer_instance_create(struct tracer *tracer, int clientfd)
{
	int serverfd;
	struct tracer_instance *instance;

	instance = malloc(sizeof *instance);
	if (instance == NULL) {
		errno = ENOMEM;
		return -1;
	}

	if (tracer->socket == NULL)
		serverfd = tracer_connect_server(NULL);
	else
		serverfd = tracer_connect_to_socket(NULL);

	if (serverfd < 0)
		goto err_server;

	instance->server_conn = tracer_connection_create(serverfd,
							 TRACER_SERVER_SIDE);
	if (instance->server_conn == NULL)
		goto err_conn;

	instance->client_conn = tracer_connection_create(clientfd,
							 TRACER_CLIENT_SIDE);
	if (instance->client_conn == NULL)
		goto err_conn;

	instance->server_conn->peer = instance->client_conn;
	instance->client_conn->peer = instance->server_conn;

	instance->server_conn->instance = instance;
	instance->client_conn->instance = instance;

	wl_map_init(&instance->map, WL_MAP_CLIENT_SIDE);

	if (tracer->analyzer != NULL) {
		wl_map_insert_new(&instance->map, 0, NULL);
		wl_map_insert_new(&instance->map, 0,
				  tracer->analyzer->display_interface);
	}

	tracer_epoll_add_fd(tracer, serverfd, instance->server_conn);
	tracer_epoll_add_fd(tracer, clientfd, instance->client_conn);

	instance->tracer = tracer;
	instance->id = tracer->next_id;
	tracer->next_id++;

	wl_list_insert(&tracer->instance_list, &instance->link);
	return 0;

err_server:
	close(clientfd);
	free(instance);
	return -1;

err_conn:
	close(clientfd);
	close(serverfd);
	free(instance);
	return -1;
}

static void
tracer_instance_destroy(struct tracer_instance *instance)
{
	tracer_connection_destroy(instance->server_conn);
	tracer_connection_destroy(instance->client_conn);

	wl_list_remove(&instance->link);

	free(instance);
}

static void
tracer_handle_hup(struct tracer_connection *connection)
{
	tracer_instance_destroy(connection->instance);
}

static void
tracer_handle_data(struct tracer_connection *connection)
{
	int total, rem, size;
	struct tracer *tracer = connection->instance->tracer;
	struct tracer_connection *peer = connection->peer;

	total = wl_connection_read(connection->wl_conn);

	for (rem = total; rem >= 8; rem -= size) {
		size = tracer->dumper(connection, rem);
		if (size == 0)
			break;
	}
	wl_connection_flush(peer->wl_conn);
}

static void
tracer_handle_client(struct tracer *tracer)
{
	struct tracer_socket *s = tracer->socket;
	struct sockaddr_un name;
	socklen_t length;
	int clientfd;

	length = sizeof name;
	clientfd = wl_os_accept_cloexec(s->fd, (struct sockaddr *) &name,
					 &length);

	if (clientfd < 0)
		fprintf(stderr, "failed to accept(): %m\n");
	else if (tracer_instance_create(tracer, clientfd) < 0) {
		fprintf(stderr, "failed to create instance\n");
		close(clientfd);
	}
}

static int
tracer_run(struct tracer *tracer)
{
	struct epoll_event ev;
	struct tracer_connection *connection;
	int nfds;

	for (;;) {
		nfds = epoll_wait(tracer->epollfd, &ev, 1, -1);

		if (nfds < 0) {
			fprintf(stderr, "Failed to poll: %m\n");
			return -1;
		}

		connection = (struct tracer_connection *) ev.data.ptr;

		if (ev.events & EPOLLIN) {
			if (connection == NULL)
				tracer_handle_client(tracer);
			else
				tracer_handle_data(connection);
		}

		if (ev.events & EPOLLHUP) {
			tracer_handle_hup(connection);

			if (tracer->socket == NULL) {
				fprintf(stderr, "Child hups, exiting\n");
				break;
			}
		}
	}

	return 0;
}

/* Following two functions adapted from wayland-server.c */
static int
get_socket_lock(struct tracer_socket *socket)
{
	struct stat socket_stat;
	int fd_lock;

	snprintf(socket->lock_addr, sizeof socket->lock_addr,
		 "%s%s", socket->addr.sun_path, LOCK_SUFFIX);

	fd_lock = open(socket->lock_addr, O_CREAT | O_CLOEXEC,
	               (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP));

	if (fd_lock < 0) {
		fprintf(stderr, "unable to open lockfile %s check permissions\n",
			socket->lock_addr);
		return -1;
	}

	if (flock(fd_lock, LOCK_EX | LOCK_NB) < 0) {
		fprintf(stderr, "unable to lock lockfile %s, maybe another compositor is running\n",
			socket->lock_addr);
		close(fd_lock);
		return -1;
	}

	if (stat(socket->addr.sun_path, &socket_stat) < 0 ) {
		if (errno != ENOENT) {
			fprintf(stderr, "did not manage to stat file %s\n",
				socket->addr.sun_path);
			close(fd_lock);
			return -1;
		}
	} else if (socket_stat.st_mode & S_IWUSR ||
		   socket_stat.st_mode & S_IWGRP) {
		unlink(socket->addr.sun_path);
	}

	return fd_lock;
}

static int
tracer_create_socket(struct tracer *tracer, const char *name)
{
	struct tracer_socket *s;
	socklen_t size;
	int name_size;
	const char *runtime_dir;

	s = malloc(sizeof *s);
	if (s == NULL)
		return -1;

	runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		wl_log("error: XDG_RUNTIME_DIR not set in the environment\n");

		/* to prevent programs reporting
		 * "failed to add socket: Success" */
		errno = ENOENT;
		return -1;
	}

	s->fd = wl_os_socket_cloexec(PF_LOCAL, SOCK_STREAM, 0);
	if (s->fd < 0)
		return -1;

	if (name == NULL)
		name = getenv("WAYLAND_DISPLAY");
	if (name == NULL)
		name = "wayland-0";

	memset(&s->addr, 0, sizeof s->addr);
	s->addr.sun_family = AF_LOCAL;
	name_size = snprintf(s->addr.sun_path, sizeof s->addr.sun_path,
			     "%s/%s", runtime_dir, name) + 1;

	assert(name_size > 0);
	if (name_size > (int)sizeof s->addr.sun_path) {
		fprintf(stderr, "error: socket path \"%s/%s\" plus null "
		       "terminator exceeds 108 bytes\n", runtime_dir, name);
		close(s->fd);
		free(s);
		/* to prevent programs reporting
		 * "failed to add socket: Success" */
		errno = ENAMETOOLONG;
		return -1;
	};

	s->fd_lock = get_socket_lock(s);
	if (s->fd_lock < 0) {
		close(s->fd);
		free(s);
		return -1;
	}

	size = offsetof (struct sockaddr_un, sun_path) + name_size;
	if (bind(s->fd, (struct sockaddr *) &s->addr, size) < 0) {
		fprintf(stderr, "bind() failed with error: %m\n");
		close(s->fd);
		unlink(s->lock_addr);
		close(s->fd_lock);
		free(s);
		return -1;
	}

	if (listen(s->fd, 1) < 0) {
		fprintf(stderr, "listen() failed with error: %m\n");
		unlink(s->addr.sun_path);
		close(s->fd);
		unlink(s->lock_addr);
		close(s->fd_lock);
		free(s);
		return -1;
	}

	tracer_epoll_add_fd(tracer, s->fd, NULL);
	tracer->socket = s;

	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "wayland-tracer: a wayland protocol dumper\n"
		"Usage:\twayland-tracer [OPTIONS] -- file ...\n"
		"\twayland-tracer -S NAME [OPTIONS]\n\n"
		"Options:\n\n"
		"  -S NAME\t\tMake wayland-tracer run under server mode\n"
		"\t\t\tand make the name of server socket NAME (such as\n"
		"\t\t\twayland-0)\n"
		"  -o FILE\t\tDump output to FILE\n"
		"  -d FILE\t\tAdd an xml protocol file\n"
		"\t\t\twayland-tracer will output readable format according\n"
		"\t\t\tto the protocols given if -d is specified\n"
		"  -h\t\t\tThis help message\n\n");
}

static int
tracer_add_protocol(struct tracer_options *options, const char *file)
{
	struct protocol_file *protocol_file;

	protocol_file = malloc(sizeof *protocol_file);
	if (protocol_file == NULL) {
		fprintf(stderr, "Failed to alloc for protocol: %m\n");
		return -1;
	}

	protocol_file->loc = file;
	wl_list_insert(&options->protocol_file_list, &protocol_file->link);

	return 0;
}

static struct tracer_options*
tracer_parse_args(int argc, char *argv[])
{
	int i;
	struct tracer_options *options;

	options = malloc(sizeof *options);
	if (options == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	options->spawn_args = NULL;
	options->mode = TRACER_MODE_SINGLE;
	wl_list_init(&options->protocol_file_list);
	options->output_format = TRACER_OUTPUT_RAW;

	if (argc == 1) {
		usage();
		exit(EXIT_SUCCESS);
	}

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h")) {
			usage();
			exit(EXIT_SUCCESS);
		} else if (!strcmp(argv[i], "-S")) {
			i++;
			if (i == argc) {
				fprintf(stderr, "Socket not specified\n");
				exit(EXIT_FAILURE);
			}
			options->mode = TRACER_MODE_SERVER;
			options->socket = argv[i];
		} else if (!strcmp(argv[i], "--")) {
			i++;
			if (i == argc) {
				fprintf(stderr, "Program not specified\n");
				exit(EXIT_FAILURE);
			}
			options->spawn_args = &argv[i];
			break;
		} else if (!strcmp(argv[i], "-o")) {
			i++;
			if (i == argc) {
				fprintf(stderr, "Output file not specified\n");
				exit(EXIT_FAILURE);
			}
			options->outfile = argv[i];
		} else if (!strcmp(argv[i], "-d")) {
			i++;
			if (i == argc) {
				fprintf(stderr, "Protocol file not specfied\n");
				exit(EXIT_FAILURE);
			}
			if (tracer_add_protocol(options, argv[i]) != 0)
				exit(EXIT_FAILURE);
			options->output_format = TRACER_OUTPUT_INTERPRET;
		} else {
			fprintf(stderr, "Unknown argument '%s'\n", argv[i]);
			usage();
			exit(EXIT_FAILURE);
		}
	}

	if (options->mode == TRACER_MODE_SINGLE &&
	    options->spawn_args == NULL) {
		fprintf(stderr, "No client specified in single mode\n");
		exit(EXIT_FAILURE);
	}
	return options;
}

static struct tracer*
tracer_create(struct tracer_options *options)
{
	int sock_vec[2], ret;
	pid_t pid;
	struct tracer *tracer;
	char sockfdstr[12];
	struct protocol_file *file;

	tracer = malloc(sizeof *tracer);
	if (tracer == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	if (options->outfile != NULL) {
		tracer->outfp = fopen(options->outfile, "w");
		if (tracer->outfp == NULL) {
			fprintf(stderr,
				"Failed to open output file %s: %m\n",
				options->outfile);
			exit(EXIT_FAILURE);
		}
	} else
		tracer->outfp = stdout;

	wl_list_init(&tracer->instance_list);
	tracer->next_id = 0;
	if (options->output_format == TRACER_OUTPUT_INTERPRET) {
		tracer->analyzer = tracer_analyzer_create();
		if (tracer->analyzer == NULL) {
			fprintf(stderr, "failed to create analyzer");
			exit(EXIT_FAILURE);
		}
		wl_list_for_each(file, &options->protocol_file_list, link) {
			if (tracer_analyzer_add_protocol(tracer->analyzer,
							 file->loc) != 0) {
				fprintf(stderr, "failed to add file %s\n",
					file->loc);
				exit(EXIT_FAILURE);
			}
		}
		if (tracer_analyzer_finalize(tracer->analyzer) != 0) {
			exit(EXIT_FAILURE);
		}
	}

	if (options->output_format == TRACER_OUTPUT_INTERPRET)
		tracer->dumper = tracer_dump_analyzed;
	else
		tracer->dumper = tracer_dump_bin;
	// Spawn child if we're in single mode
	if (options->mode == TRACER_MODE_SINGLE) {
		tracer->socket = NULL;
		ret = socketpair(PF_LOCAL, SOCK_STREAM, 0, sock_vec);
		if (ret != 0) {
			fprintf(stderr, "Failed to create socketpair: %m\n");
			goto err_socketpair;
		}

		pid = fork();
		if (pid < 0) {
			fprintf(stderr, "Failed to fork: %m\n");
			goto err_fork;
		}

		if (pid == 0) {
			close(sock_vec[0]);
			fclose(tracer->outfp);
			sprintf(sockfdstr, "%d", sock_vec[1]);
			setenv("WAYLAND_SOCKET", sockfdstr, 1);

			execvp(options->spawn_args[0],
			       options->spawn_args);

			// Only when exec fails can we reach here
			close(sock_vec[1]);
			exit(EXIT_FAILURE);
		}
	}

	tracer->epollfd = epoll_create1(0);
	if (tracer->epollfd < 0) {
		fprintf(stderr, "Failed to create epollfd: %m\n");
		goto err_epoll_create;
	}

	if (options->mode == TRACER_MODE_SINGLE) {
		close(sock_vec[1]);
		ret = tracer_instance_create(tracer, sock_vec[0]);
		if (ret < 0) {
			fprintf(stderr, "Failed to init instance\n");
			goto err_instance;
		}
	} else {
		ret = tracer_create_socket(tracer, "wayland-1");
		if (ret < 0)
			exit(EXIT_FAILURE);
	}

	return tracer;

err_socketpair:
	free(tracer);
	return NULL;

err_fork:
	close(sock_vec[0]);
	close(sock_vec[1]);
	free(tracer);
	return NULL;

err_epoll_create:
	if (options->mode == TRACER_MODE_SINGLE) {
		close(sock_vec[0]);
		close(sock_vec[1]);
	}
	free(tracer);
	return NULL;

err_instance:
	close(tracer->epollfd);
	close(sock_vec[0]);
	free(tracer);
	return NULL;
}

int
main(int argc, char *argv[])
{
	int ret;
	struct tracer *tracer;
	struct tracer_options *options;

	options = tracer_parse_args(argc, argv);
	if (options == NULL) {
		fprintf(stderr, "Failed to parse command line: %m\n");
		exit(EXIT_FAILURE);
	}

	tracer = tracer_create(options);

	if (tracer == NULL) {
		fprintf(stderr, "Failed to create tracer, exiting!\n");
		exit(EXIT_FAILURE);
	}

	ret = tracer_run(tracer);

	if (ret == 0)
		exit(EXIT_SUCCESS);
	else
		exit(EXIT_FAILURE);
}

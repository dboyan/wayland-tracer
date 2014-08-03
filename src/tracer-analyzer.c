/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
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
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <expat.h>

#include "tracer-analyzer.h"
#include "wayland-util.h"

#define XML_BUFFER_SIZE 4096

struct tracer_protocol {
	char *name;
	struct wl_list interface_list;
};

enum arg_type {
	NEW_ID,
	INT,
	UNSIGNED,
	FIXED,
	STRING,
	OBJECT,
	ARRAY,
	FD
};

struct tracer_arg {
	char *name;
	enum arg_type type;
	char *interface_name;
	struct wl_list link;
};

struct parse_context {
	struct location loc;
	XML_Parser parser;
	struct tracer_protocol *protocol;
	struct tracer_interface *interface;
	struct tracer_message *message;
	char character_data[8192];
	unsigned int character_data_length;
};

static void *
fail_on_null(void *p)
{
	if (p == NULL) {
		fprintf(stderr, "xml-parser: out of memory\n");
		exit(EXIT_FAILURE);
	}

	return p;
}

static void *
xmalloc(size_t s)
{
	return fail_on_null(malloc(s));
}


static char *
xstrdup(const char *s)
{
	return fail_on_null(strdup(s));
}

static void
fail(struct location *loc, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	fprintf(stderr, "%s:%d: error: ",
		loc->filename, loc->line_number);
	vfprintf(stderr, msg, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(EXIT_FAILURE);
}

static void
start_element(void *data, const char *element_name, const char **atts)
{
	struct parse_context *ctx = data;
	struct tracer_interface *interface;
	struct tracer_message *message;
	struct tracer_arg *arg;
	struct entry *entry;
	const char *name, *type, *interface_name, *value;
	char *end;
	int i, version;

	ctx->loc.line_number = XML_GetCurrentLineNumber(ctx->parser);
	name = NULL;
	type = NULL;
	version = 0;
	interface_name = NULL;
	value = NULL;
	for (i = 0; atts[i]; i += 2) {
		if (strcmp(atts[i], "name") == 0)
			name = atts[i + 1];
		if (strcmp(atts[i], "type") == 0)
			type = atts[i + 1];
		if (strcmp(atts[i], "value") == 0)
			value = atts[i + 1];
		if (strcmp(atts[i], "interface") == 0)
			interface_name = atts[i + 1];
	}

	ctx->character_data_length = 0;
	if (strcmp(element_name, "protocol") == 0) {
		if (name == NULL)
			fail(&ctx->loc, "no protocol name given");

		ctx->protocol->name = xstrdup(name);
	} else if (strcmp(element_name, "copyright") == 0) {

	} else if (strcmp(element_name, "interface") == 0) {
		if (name == NULL)
			fail(&ctx->loc, "no interface name given");

		interface = xmalloc(sizeof *interface);
		interface->loc = ctx->loc;
		interface->name = xstrdup(name);
		wl_list_init(&interface->request_list);
		wl_list_init(&interface->event_list);
		wl_list_insert(ctx->protocol->interface_list.prev,
			       &interface->link);
		ctx->interface = interface;
	} else if (strcmp(element_name, "request") == 0 ||
		   strcmp(element_name, "event") == 0) {
		if (name == NULL)
			fail(&ctx->loc, "no request name given");

		message = xmalloc(sizeof *message);
		message->loc = ctx->loc;
		message->name = xstrdup(name);
		wl_list_init(&message->arg_list);
		message->arg_count = 0;
		message->new_id_count = 0;
		message->new_interface_name = NULL;

		if (strcmp(element_name, "request") == 0)
			wl_list_insert(ctx->interface->request_list.prev,
				       &message->link);
		else
			wl_list_insert(ctx->interface->event_list.prev,
				       &message->link);

		if (type != NULL && strcmp(type, "destructor") == 0)
			message->destructor = 1;
		else
			message->destructor = 0;

		if (strcmp(name, "destroy") == 0 && !message->destructor)
			fail(&ctx->loc, "destroy request should be destructor type");

		ctx->message = message;
	} else if (strcmp(element_name, "arg") == 0) {
		if (name == NULL)
			fail(&ctx->loc, "no argument name given");

		arg = xmalloc(sizeof *arg);
		arg->name = xstrdup(name);

		if (strcmp(type, "int") == 0)
			arg->type = INT;
		else if (strcmp(type, "uint") == 0)
			arg->type = UNSIGNED;
		else if (strcmp(type, "fixed") == 0)
			arg->type = FIXED;
		else if (strcmp(type, "string") == 0)
			arg->type = STRING;
		else if (strcmp(type, "array") == 0)
			arg->type = ARRAY;
		else if (strcmp(type, "fd") == 0)
			arg->type = FD;
		else if (strcmp(type, "new_id") == 0) {
			arg->type = NEW_ID;
		} else if (strcmp(type, "object") == 0) {
			arg->type = OBJECT;
		} else {
			fail(&ctx->loc, "unknown type (%s)", type);
		}

		switch (arg->type) {
		case NEW_ID:
			ctx->message->new_id_count++;
			if (ctx->message->new_id_count > 1)
				fail(&ctx->loc, "there can't be more than one new_id's in one message");

			ctx->message->new_interface_name = interface_name ?
							   xstrdup(interface_name) :
							   NULL;

			/* Fall through to OBJECT case. */

		case OBJECT:
			if (interface_name)
				arg->interface_name = xstrdup(interface_name);
			else
				arg->interface_name = NULL;
			break;
		default:
			if (interface_name != NULL)
				fail(&ctx->loc, "interface attribute not allowed for type %s", type);
			break;
		}

		wl_list_insert(ctx->message->arg_list.prev, &arg->link);
		ctx->message->arg_count++;
	} else if (strcmp(element_name, "enum") == 0) {
		/* We just omit enum */
	} else if (strcmp(element_name, "entry") == 0) {
		/* Enum entry omitted too */
	} else if (strcmp(element_name, "description") == 0) {
		/* Description is omitted */
	}
}

static void
end_element(void *data, const XML_Char *name)
{
	struct parse_context *ctx = data;

	/* We don't care about others! ;) */
	if (strcmp(name, "request") == 0 ||
		   strcmp(name, "event") == 0) {
		ctx->message = NULL;
	}
}

static void
character_data(void *data, const XML_Char *s, int len)
{
	struct parse_context *ctx = data;

	if (ctx->character_data_length + len > sizeof (ctx->character_data)) {
		fprintf(stderr, "too much character data");
		exit(EXIT_FAILURE);
	    }

	memcpy(ctx->character_data + ctx->character_data_length, s, len);
	ctx->character_data_length += len;
}

struct tracer_analyzer *
tracer_analyzer_create(void)
{
	struct tracer_analyzer *analyzer;
	struct parse_context *ctx;

	analyzer = malloc(sizeof *analyzer);
	if (analyzer == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	ctx = malloc(sizeof *ctx);
	if (ctx == NULL) {
		errno = ENOMEM;
		free(analyzer);
		return NULL;
	}
	analyzer->ctx = ctx;

	wl_list_init(&analyzer->interface_list);

	return analyzer;
}

int
tracer_analyzer_add_protocol(struct tracer_analyzer *analyzer,
			     const char *filename)
{
	struct tracer_protocol protocol;
	struct parse_context *ctx = analyzer->ctx;
	void *buf;
	int len;
	FILE *fp;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		fprintf(stderr, "Unable to open protocol file: %s\n",filename);
		return -1;
	}

	wl_list_init(&protocol.interface_list);
	memset(ctx, 0, sizeof *ctx);
	ctx->protocol = &protocol;

	ctx->loc.filename = filename;
	ctx->parser = XML_ParserCreate(NULL);
	XML_SetUserData(ctx->parser, ctx);
	if (ctx->parser == NULL) {
		fprintf(stderr, "failed to create parser\n");
		return -1;
	}

	XML_SetElementHandler(ctx->parser, start_element, end_element);
	XML_SetCharacterDataHandler(ctx->parser, character_data);

	do {
		buf = XML_GetBuffer(ctx->parser, XML_BUFFER_SIZE);
		len = fread(buf, 1, XML_BUFFER_SIZE, fp);
		if (len < 0) {
			fprintf(stderr, "fread: %m\n");
			return -1;
		}
		XML_ParseBuffer(ctx->parser, len, len == 0);

	} while (len > 0);

	fclose(fp);
	XML_ParserFree(ctx->parser);

	wl_list_insert_list(analyzer->interface_list.prev,
			    &protocol.interface_list);

	return 0;
}

struct tracer_interface **
tracer_analyzer_lookup_type(struct tracer_analyzer *analyzer, char *type_name)
{
	struct tracer_interface **types = analyzer->interfaces;

	if (type_name == NULL)
		return NULL;

	while (*types != NULL) {
		if (strcmp((*types)->name, type_name) == 0)
			return types;
		types++;
	}

	return NULL;
}

static int
generate_signature(struct tracer_message *message)
{
	char *signature;
	int length, i;
	struct tracer_arg *arg;

	length = wl_list_length(&message->arg_list);
	signature = malloc(length);
	if (signature == NULL) {
		errno = ENOMEM;
		return -1;
	}
	message->signature = signature;

	i = 0;
	wl_list_for_each(arg, &message->arg_list, link) {
		switch (arg->type) {
		case INT:	signature[i] = 'i'; break;
		case UNSIGNED:	signature[i] = 'u'; break;
		case FIXED:	signature[i] = 'f'; break;
		case STRING:	signature[i] = 's'; break;
		case OBJECT:	signature[i] = 'o'; break;
		case ARRAY:	signature[i] = 'a'; break;
		case FD:	signature[i] = 'h'; break;
		case NEW_ID:
			if (arg->interface_name != NULL)
				signature[i] = 'n';
			else
				signature[i] = 'N';
			break;
		}
		i++;
	}
	signature[length] = '\0';
}

int
tracer_analyzer_finalize(struct tracer_analyzer *analyzer)
{
	int count, message_count, i, j;
	struct tracer_interface *interface;
	struct tracer_interface **interfaces;
	struct tracer_interface **display_type;
	struct tracer_message **messages;
	struct tracer_message *message;

	count = wl_list_length(&analyzer->interface_list);
	interfaces = calloc(count + 1, sizeof *interfaces);
	if (interfaces == NULL) {
		errno = ENOMEM;
		return -1;
	}
	analyzer->interfaces = interfaces;

	i = 0;
	wl_list_for_each(interface, &analyzer->interface_list, link) {
		interfaces[i] = interface;
		i++;
	}

	for (i = 0; i < count; i++) {
		interface = interfaces[i];
		message_count = wl_list_length(&interface->request_list);
		messages = calloc(message_count, sizeof *messages);
		if (messages == NULL) {
			errno = ENOMEM;
			return -1;
		}
		interface->method_count = message_count;
		interface->methods = messages;
		j = 0;
		wl_list_for_each(message, &interface->request_list, link) {
			messages[j] = message;
			message->types = tracer_analyzer_lookup_type(analyzer, message->new_interface_name);
			if (message->new_interface_name != NULL &&
			    message->types == NULL) {
				fprintf(stderr, "interface %s not found\n",
					message->new_interface_name);
				return -1;
			}
			if (generate_signature(message) < 0)
				return -1;
			j++;
		}

		message_count = wl_list_length(&interface->event_list);
		messages = calloc(message_count, sizeof *messages);
		if (messages == NULL) {
			errno = ENOMEM;
			return -1;
		}
		interface->event_count = message_count;
		interface->events = messages;
		j = 0;
		wl_list_for_each(message, &interface->event_list, link) {
			messages[j] = message;
			message->types = tracer_analyzer_lookup_type(analyzer, message->new_interface_name);
			if (message->new_interface_name != NULL &&
			    message->types == NULL) {
				fprintf(stderr, "interface %s not found\n",
					message->new_interface_name);
				return -1;
			}
			if (generate_signature(message) < 0)
				return -1;
			j++;
		}
	}

	display_type = tracer_analyzer_lookup_type(analyzer, "wl_display");
	if (display_type == NULL) {
		fprintf(stderr, "You should at least have wl_display!\n");
		return -1;
	}
	analyzer->display_interface = *display_type;

	free(analyzer->ctx);

	return 0;
}

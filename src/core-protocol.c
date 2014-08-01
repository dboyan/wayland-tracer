/*
 * Copyright © 2008-2013 Kristian Høgsberg
 * Copyright © 2013      Rafael Antognolli
 * Copyright © 2013      Jasper St. Pierre
 * Copyright © 2010-2013 Intel Corporation
 * Copyright © 2012-2013 Collabora, Ltd.
 * Copyright © 2014      Boyan Ding
 *
 * Permission to use, copy, modify, distribute, and sell this
 * software and its documentation for any purpose is hereby granted
 * without fee, provided that the above copyright notice appear in
 * all copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of
 * the copyright holders not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */

/* This file is a hard-coded protocol used for analyzer, will be subsituted
   by runtime generated ones in the future with xml parser. Also we'll move
   away from wayland representation of interfaces for more accurate outputs
 */
#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"
#include "core-protocol.h"

static const struct wl_interface wl_buffer_interface;
static const struct wl_interface wl_callback_interface;
static const struct wl_interface wl_data_device_interface;
static const struct wl_interface wl_data_offer_interface;
static const struct wl_interface wl_data_source_interface;
static const struct wl_interface wl_keyboard_interface;
static const struct wl_interface wl_output_interface;
static const struct wl_interface wl_pointer_interface;
static const struct wl_interface wl_region_interface;
static const struct wl_interface wl_registry_interface;
static const struct wl_interface wl_seat_interface;
static const struct wl_interface wl_shell_surface_interface;
static const struct wl_interface wl_shm_pool_interface;
static const struct wl_interface wl_subsurface_interface;
static const struct wl_interface wl_surface_interface;
static const struct wl_interface wl_touch_interface;

static const struct wl_interface *types[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&wl_callback_interface,
	&wl_registry_interface,
	&wl_surface_interface,
	&wl_region_interface,
	&wl_buffer_interface,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&wl_shm_pool_interface,
	NULL,
	NULL,
	&wl_data_source_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	NULL,
	&wl_data_source_interface,
	NULL,
	&wl_data_offer_interface,
	NULL,
	&wl_surface_interface,
	NULL,
	NULL,
	&wl_data_offer_interface,
	&wl_data_offer_interface,
	&wl_data_source_interface,
	&wl_data_device_interface,
	&wl_seat_interface,
	&wl_shell_surface_interface,
	&wl_surface_interface,
	&wl_seat_interface,
	NULL,
	&wl_seat_interface,
	NULL,
	NULL,
	&wl_surface_interface,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&wl_output_interface,
	&wl_seat_interface,
	NULL,
	&wl_surface_interface,
	NULL,
	NULL,
	NULL,
	&wl_output_interface,
	&wl_buffer_interface,
	NULL,
	NULL,
	&wl_callback_interface,
	&wl_region_interface,
	&wl_region_interface,
	&wl_output_interface,
	&wl_output_interface,
	&wl_pointer_interface,
	&wl_keyboard_interface,
	&wl_touch_interface,
	NULL,
	&wl_surface_interface,
	NULL,
	NULL,
	NULL,
	&wl_surface_interface,
	NULL,
	NULL,
	NULL,
	&wl_surface_interface,
	NULL,
	&wl_surface_interface,
	NULL,
	NULL,
	&wl_surface_interface,
	NULL,
	NULL,
	&wl_surface_interface,
	NULL,
	NULL,
	NULL,
	&wl_subsurface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
};

static const struct wl_message wl_display_requests[] = {
	{ "sync", "n", types + 8 },
	{ "get_registry", "n", types + 9 },
};

static const struct wl_message wl_display_events[] = {
	{ "error", "ous", types + 0 },
	{ "delete_id", "u", types + 0 },
};

static const struct wl_interface wl_display_interface = {
	"wl_display", 1,
	2, wl_display_requests,
	2, wl_display_events,
};

static const struct wl_message wl_registry_requests[] = {
	{ "bind", "uN", types + 0 },
};

static const struct wl_message wl_registry_events[] = {
	{ "global", "usu", types + 0 },
	{ "global_remove", "u", types + 0 },
};

static const struct wl_interface wl_registry_interface = {
	"wl_registry", 1,
	1, wl_registry_requests,
	2, wl_registry_events,
};

static const struct wl_message wl_callback_events[] = {
	{ "done", "u", types + 0 },
};

static const struct wl_interface wl_callback_interface = {
	"wl_callback", 1,
	0, NULL,
	1, wl_callback_events,
};

static const struct wl_message wl_compositor_requests[] = {
	{ "create_surface", "n", types + 10 },
	{ "create_region", "n", types + 11 },
};

static const struct wl_interface wl_compositor_interface = {
	"wl_compositor", 3,
	2, wl_compositor_requests,
	0, NULL,
};

static const struct wl_message wl_shm_pool_requests[] = {
	{ "create_buffer", "niiiiu", types + 12 },
	{ "destroy", "", types + 0 },
	{ "resize", "i", types + 0 },
};

static const struct wl_interface wl_shm_pool_interface = {
	"wl_shm_pool", 1,
	3, wl_shm_pool_requests,
	0, NULL,
};

static const struct wl_message wl_shm_requests[] = {
	{ "create_pool", "nhi", types + 18 },
};

static const struct wl_message wl_shm_events[] = {
	{ "format", "u", types + 0 },
};

static const struct wl_interface wl_shm_interface = {
	"wl_shm", 1,
	1, wl_shm_requests,
	1, wl_shm_events,
};

static const struct wl_message wl_buffer_requests[] = {
	{ "destroy", "", types + 0 },
};

static const struct wl_message wl_buffer_events[] = {
	{ "release", "", types + 0 },
};

static const struct wl_interface wl_buffer_interface = {
	"wl_buffer", 1,
	1, wl_buffer_requests,
	1, wl_buffer_events,
};

static const struct wl_message wl_data_offer_requests[] = {
	{ "accept", "u?s", types + 0 },
	{ "receive", "sh", types + 0 },
	{ "destroy", "", types + 0 },
};

static const struct wl_message wl_data_offer_events[] = {
	{ "offer", "s", types + 0 },
};

static const struct wl_interface wl_data_offer_interface = {
	"wl_data_offer", 1,
	3, wl_data_offer_requests,
	1, wl_data_offer_events,
};

static const struct wl_message wl_data_source_requests[] = {
	{ "offer", "s", types + 0 },
	{ "destroy", "", types + 0 },
};

static const struct wl_message wl_data_source_events[] = {
	{ "target", "?s", types + 0 },
	{ "send", "sh", types + 0 },
	{ "cancelled", "", types + 0 },
};

static const struct wl_interface wl_data_source_interface = {
	"wl_data_source", 1,
	2, wl_data_source_requests,
	3, wl_data_source_events,
};

static const struct wl_message wl_data_device_requests[] = {
	{ "start_drag", "?oo?ou", types + 21 },
	{ "set_selection", "?ou", types + 25 },
};

static const struct wl_message wl_data_device_events[] = {
	{ "data_offer", "n", types + 27 },
	{ "enter", "uoff?o", types + 28 },
	{ "leave", "", types + 0 },
	{ "motion", "uff", types + 0 },
	{ "drop", "", types + 0 },
	{ "selection", "?o", types + 33 },
};

static const struct wl_interface wl_data_device_interface = {
	"wl_data_device", 1,
	2, wl_data_device_requests,
	6, wl_data_device_events,
};

static const struct wl_message wl_data_device_manager_requests[] = {
	{ "create_data_source", "n", types + 34 },
	{ "get_data_device", "no", types + 35 },
};

static const struct wl_interface wl_data_device_manager_interface = {
	"wl_data_device_manager", 1,
	2, wl_data_device_manager_requests,
	0, NULL,
};

static const struct wl_message wl_shell_requests[] = {
	{ "get_shell_surface", "no", types + 37 },
};

static const struct wl_interface wl_shell_interface = {
	"wl_shell", 1,
	1, wl_shell_requests,
	0, NULL,
};

static const struct wl_message wl_shell_surface_requests[] = {
	{ "pong", "u", types + 0 },
	{ "move", "ou", types + 39 },
	{ "resize", "ouu", types + 41 },
	{ "set_toplevel", "", types + 0 },
	{ "set_transient", "oiiu", types + 44 },
	{ "set_fullscreen", "uu?o", types + 48 },
	{ "set_popup", "ouoiiu", types + 51 },
	{ "set_maximized", "?o", types + 57 },
	{ "set_title", "s", types + 0 },
	{ "set_class", "s", types + 0 },
};

static const struct wl_message wl_shell_surface_events[] = {
	{ "ping", "u", types + 0 },
	{ "configure", "uii", types + 0 },
	{ "popup_done", "", types + 0 },
};

static const struct wl_interface wl_shell_surface_interface = {
	"wl_shell_surface", 1,
	10, wl_shell_surface_requests,
	3, wl_shell_surface_events,
};

static const struct wl_message wl_surface_requests[] = {
	{ "destroy", "", types + 0 },
	{ "attach", "?oii", types + 58 },
	{ "damage", "iiii", types + 0 },
	{ "frame", "n", types + 61 },
	{ "set_opaque_region", "?o", types + 62 },
	{ "set_input_region", "?o", types + 63 },
	{ "commit", "", types + 0 },
	{ "set_buffer_transform", "2i", types + 0 },
	{ "set_buffer_scale", "3i", types + 0 },
};

static const struct wl_message wl_surface_events[] = {
	{ "enter", "o", types + 64 },
	{ "leave", "o", types + 65 },
};

static const struct wl_interface wl_surface_interface = {
	"wl_surface", 3,
	9, wl_surface_requests,
	2, wl_surface_events,
};

static const struct wl_message wl_seat_requests[] = {
	{ "get_pointer", "n", types + 66 },
	{ "get_keyboard", "n", types + 67 },
	{ "get_touch", "n", types + 68 },
};

static const struct wl_message wl_seat_events[] = {
	{ "capabilities", "u", types + 0 },
	{ "name", "2s", types + 0 },
};

static const struct wl_interface wl_seat_interface = {
	"wl_seat", 3,
	3, wl_seat_requests,
	2, wl_seat_events,
};

static const struct wl_message wl_pointer_requests[] = {
	{ "set_cursor", "u?oii", types + 69 },
	{ "release", "3", types + 0 },
};

static const struct wl_message wl_pointer_events[] = {
	{ "enter", "uoff", types + 73 },
	{ "leave", "uo", types + 77 },
	{ "motion", "uff", types + 0 },
	{ "button", "uuuu", types + 0 },
	{ "axis", "uuf", types + 0 },
};

static const struct wl_interface wl_pointer_interface = {
	"wl_pointer", 3,
	2, wl_pointer_requests,
	5, wl_pointer_events,
};

static const struct wl_message wl_keyboard_requests[] = {
	{ "release", "3", types + 0 },
};

static const struct wl_message wl_keyboard_events[] = {
	{ "keymap", "uhu", types + 0 },
	{ "enter", "uoa", types + 79 },
	{ "leave", "uo", types + 82 },
	{ "key", "uuuu", types + 0 },
	{ "modifiers", "uuuuu", types + 0 },
};

static const struct wl_interface wl_keyboard_interface = {
	"wl_keyboard", 3,
	1, wl_keyboard_requests,
	5, wl_keyboard_events,
};

static const struct wl_message wl_touch_requests[] = {
	{ "release", "3", types + 0 },
};

static const struct wl_message wl_touch_events[] = {
	{ "down", "uuoiff", types + 84 },
	{ "up", "uui", types + 0 },
	{ "motion", "uiff", types + 0 },
	{ "frame", "", types + 0 },
	{ "cancel", "", types + 0 },
};

static const struct wl_interface wl_touch_interface = {
	"wl_touch", 3,
	1, wl_touch_requests,
	5, wl_touch_events,
};

static const struct wl_message wl_output_events[] = {
	{ "geometry", "iiiiissi", types + 0 },
	{ "mode", "uiii", types + 0 },
	{ "done", "2", types + 0 },
	{ "scale", "2i", types + 0 },
};

static const struct wl_interface wl_output_interface = {
	"wl_output", 2,
	0, NULL,
	4, wl_output_events,
};

static const struct wl_message wl_region_requests[] = {
	{ "destroy", "", types + 0 },
	{ "add", "iiii", types + 0 },
	{ "subtract", "iiii", types + 0 },
};

static const struct wl_interface wl_region_interface = {
	"wl_region", 1,
	3, wl_region_requests,
	0, NULL,
};

static const struct wl_message wl_subcompositor_requests[] = {
	{ "destroy", "", types + 0 },
	{ "get_subsurface", "noo", types + 90 },
};

static const struct wl_interface wl_subcompositor_interface = {
	"wl_subcompositor", 1,
	2, wl_subcompositor_requests,
	0, NULL,
};

static const struct wl_message wl_subsurface_requests[] = {
	{ "destroy", "", types + 0 },
	{ "set_position", "ii", types + 0 },
	{ "place_above", "o", types + 93 },
	{ "place_below", "o", types + 94 },
	{ "set_sync", "", types + 0 },
	{ "set_desync", "", types + 0 },
};

static const struct wl_interface wl_subsurface_interface = {
	"wl_subsurface", 1,
	6, wl_subsurface_requests,
	0, NULL,
};

static const struct wl_interface xdg_popup_interface;
static const struct wl_interface xdg_surface_interface;

static const struct wl_interface *types_xdg[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	&xdg_surface_interface,
	&wl_surface_interface,
	&xdg_popup_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_seat_interface,
	NULL,
	NULL,
	NULL,
	NULL,
	&wl_surface_interface,
	&wl_seat_interface,
	NULL,
	NULL,
	NULL,
	&wl_seat_interface,
	NULL,
	&wl_seat_interface,
	NULL,
	NULL,
	&wl_output_interface,
};

static const struct wl_message xdg_shell_requests[] = {
	{ "use_unstable_version", "i", types_xdg + 0 },
	{ "get_xdg_surface", "no", types_xdg + 4 },
	{ "get_xdg_popup", "nooouiiu", types_xdg + 6 },
	{ "pong", "u", types_xdg + 0 },
};

static const struct wl_message xdg_shell_events[] = {
	{ "ping", "u", types_xdg + 0 },
};

static const struct wl_interface xdg_shell_interface = {
	"xdg_shell", 1,
	4, xdg_shell_requests,
	1, xdg_shell_events,
};

static const struct wl_message xdg_surface_requests[] = {
	{ "destroy", "", types_xdg + 0 },
	{ "set_parent", "?o", types_xdg + 14 },
	{ "set_margin", "iiii", types_xdg + 0 },
	{ "set_title", "s", types_xdg + 0 },
	{ "set_app_id", "s", types_xdg + 0 },
	{ "show_window_menu", "ouii", types_xdg + 15 },
	{ "move", "ou", types_xdg + 19 },
	{ "resize", "ouu", types_xdg + 21 },
	{ "ack_configure", "u", types_xdg + 0 },
	{ "set_maximized", "", types_xdg + 0 },
	{ "unset_maximized", "", types_xdg + 0 },
	{ "set_fullscreen", "?o", types_xdg + 24 },
	{ "unset_fullscreen", "", types_xdg + 0 },
	{ "set_minimized", "", types_xdg + 0 },
};

static const struct wl_message xdg_surface_events[] = {
	{ "configure", "iiau", types_xdg + 0 },
	{ "close", "", types_xdg + 0 },
};

static const struct wl_interface xdg_surface_interface = {
	"xdg_surface", 1,
	14, xdg_surface_requests,
	2, xdg_surface_events,
};

static const struct wl_message xdg_popup_requests[] = {
	{ "destroy", "", types_xdg + 0 },
};

static const struct wl_message xdg_popup_events[] = {
	{ "popup_done", "u", types_xdg + 0 },
};

static const struct wl_interface xdg_popup_interface = {
	"xdg_popup", 1,
	1, xdg_popup_requests,
	1, xdg_popup_events,
};

const struct wl_interface *core_interfaces[] = {
	&wl_display_interface,
	&wl_registry_interface,
	&wl_callback_interface,
	&wl_compositor_interface,
	&wl_shm_pool_interface,
	&wl_shm_interface,
	&wl_buffer_interface,
	&wl_data_offer_interface,
	&wl_data_source_interface,
	&wl_data_device_interface,
	&wl_data_device_manager_interface,
	&wl_shell_interface,
	&wl_shell_surface_interface,
	&wl_surface_interface,
	&wl_seat_interface,
	&wl_pointer_interface,
	&wl_keyboard_interface,
	&wl_touch_interface,
	&wl_output_interface,
	&wl_region_interface,
	&wl_subcompositor_interface,
	&wl_subsurface_interface,
	&xdg_shell_interface,
	&xdg_popup_interface,
	&xdg_surface_interface,
	NULL
};

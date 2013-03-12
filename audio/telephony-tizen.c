/*
 *  Bluetooth protocol stack for Linux
 *
 * Copyright (c) 2000 - 2010 Samsung Electronics Co., Ltd.
 *
 * Contact: Chethan  T N <chethan.tn@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *//*:Associate with "Bluetooth" */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>

#include <dbus/dbus.h>
#include <gdbus.h>

#include "log.h"
#include "telephony.h"
#include "error.h"


/* CSD CALL plugin D-Bus definitions */
#define CSD_CALL_BUS_NAME	"org.tizen.csd"
#define CSD_CALL_INTERFACE	"org.tizen.csd.Call"
#define CSD_CALL_INSTANCE	"org.tizen.csd.Call.Instance"
#define CSD_CALL_CONFERENCE_PATH "/org/tizen/csd/call/conference"
#define CSD_DEVICE_INTERFACE	"org.tizen.device"

/* Phonebook definitions */
#define PHONEBOOK_BUS_NAME	"org.bluez.pb_agent"
#define PHONEBOOK_PATH		"/org/bluez/pb_agent"
#define PHONEBOOK_INTERFACE	"org.bluez.PbAgent.At"

#define CALL_FLAG_NONE	0
#define CALL_FLAG_PRESENTATION_ALLOWED		0x01
#define CALL_FLAG_PRESENTATION_RESTRICTED	0x02

#define TELEPHONY_CSD_INTERFACE	"org.tizen.telephony.csd"
#define TELEPHONY_CSD_OBJECT_PATH	"/org/tizen/csd"

#define HFP_AGENT_SERVICE "org.bluez.hfp_agent"
#define HFP_AGENT_PATH "/org/bluez/hfp_agent"
#define HFP_AGENT_INTERFACE "Org.Hfp.Bluez.Interface"

/* Call status values as exported by the CSD CALL plugin */
#define CSD_CALL_STATUS_IDLE			0
#define CSD_CALL_STATUS_CREATE			1
#define CSD_CALL_STATUS_COMING			2
#define CSD_CALL_STATUS_PROCEEDING		3
#define CSD_CALL_STATUS_MO_ALERTING		4
#define CSD_CALL_STATUS_MT_ALERTING		5
#define CSD_CALL_STATUS_WAITING			6
#define CSD_CALL_STATUS_ANSWERED		7
#define CSD_CALL_STATUS_ACTIVE			8
#define CSD_CALL_STATUS_MO_RELEASE		9
#define CSD_CALL_STATUS_MT_RELEASE		10
#define CSD_CALL_STATUS_HOLD_INITIATED		11
#define CSD_CALL_STATUS_HOLD			12
#define CSD_CALL_STATUS_RETRIEVE_INITIATED	13
#define CSD_CALL_STATUS_RECONNECT_PENDING	14
#define CSD_CALL_STATUS_TERMINATED		15
#define CSD_CALL_STATUS_SWAP_INITIATED		16


#define PREFFERED_MESSAGE_STORAGE_LIST "(\"ME\",\"MT\",\"SM\",\"SR\"),(\"ME\",\"MT\",\"SM\",\"SR\"),(\"ME\",\"MT\",\"SM\",\"SR\")"

#define PREFFERED_MESSAGE_STORAGE_MAX 500
#define CALL_LOG_COUNT_MAX 30
#define PHONEBOOK_COUNT_MAX 1000

#define PHONEBOOK_NAME_MAX_LENGTH 20
#define PHONEBOOK_NUMBER_MAX_LENGTH 20
#define PHONEBOOK_MAX_CHARACTER_LENGTH	20

#define PHONEBOOK_READ_RESP_LENGTH 20

enum {
	CHARSET_UTF_8 = 0,
	CHARSET_IRA
};

static const char *character_set_list[] = {
	"\"UTF-8\"", "\"IRA\""
};

static const char *phonebook_store_list[] =  {
	"\"ME\"", "\"DC\"", "\"MC\"", "\"RC\""
};

#define CHARACTER_SET_LIST_SIZE (sizeof(character_set_list)/sizeof(const char *))
#define PHONEBOOK_STORE_LIST_SIZE (sizeof(phonebook_store_list)/sizeof(const char *))

typedef struct {
	uint8_t utf_8;
	uint8_t	gsm;
} GsmUnicodeTable;


 /*0xC3 charcterset*/
const GsmUnicodeTable gsm_unicode_C3[] = {
	{0xA8,0x04}, {0xA9,0x05}, {0xB9,0x06}, {0xAC,0x07}, {0xB2,0x08},
	{0xB7,0x09}, {0x98,0x0B}, {0xB8,0x0C}, {0x85,0x0E}, {0xA5,0x0F},
	{0x86,0x1C}, {0xA6,0x1D}, {0x9F,0x1E}, {0x89,0x1F}, {0x84,0x5B},
	{0x96,0x5C}, {0x91,0x5D}, {0x9C,0x5E}, {0x80,0x5F}, {0xA4,0x7B},
	{0xB6,0x7C}, {0xB1,0x7D}, {0xBC,0x7E}, {0xA0,0x7F},
};

/*0xCE charcterset*/
const GsmUnicodeTable gsm_unicode_CE[] = {
	{0x85,0x14}, {0xA1,0x50}, {0x98,0x19}, {0xA0,0x16}, {0x94,0x10},
	{0xA6,0x12}, {0x93,0x13}, {0x9E,0x1A}, {0x9B,0x14}, {0xA8,0x17},
	{0xA9,0x15},
};

#define GSM_UNI_MAX_C3	(sizeof(gsm_unicode_C3)/sizeof(GsmUnicodeTable))
#define GSM_UNI_MAX_CE	(sizeof(gsm_unicode_CE)/sizeof(GsmUnicodeTable))


static DBusConnection *ag_connection = NULL;

static GSList *calls = NULL;

static gboolean events_enabled = FALSE;
static uint32_t callerid = 0;

/* Reference count for determining the call indicator status */
static GSList *active_calls = NULL;
static GSList *sender_paths = NULL;

typedef struct {
	gchar *sender;
	gchar *path;
	unsigned int watch_id;
} sender_path_t;

static struct indicator telephony_ag_indicators[] =
{
	{ "battchg",	"0-5",	5,	TRUE },
	/* signal strength in terms of bars */
	{ "signal",	"0-5",	0,	TRUE },
	{ "service",	"0,1",	0,	TRUE },
	{ "call",	"0,1",	0,	TRUE },
	{ "callsetup",	"0-3",	0,	TRUE },
	{ "callheld",	"0-2",	0,	FALSE },
	{ "roam",	"0,1",	0,	TRUE },
	{ NULL }
};

static char *call_status_str[] = {
	"IDLE",
	"CREATE",
	"COMING",
	"PROCEEDING",
	"MO_ALERTING",
	"MT_ALERTING",
	"WAITING",
	"ANSWERED",
	"ACTIVE",
	"MO_RELEASE",
	"MT_RELEASE",
	"HOLD_INITIATED",
	"HOLD",
	"RETRIEVE_INITIATED",
	"RECONNECT_PENDING",
	"TERMINATED",
	"SWAP_INITIATED",
	"???"
};

enum net_registration_status {
	NETWORK_REG_STATUS_HOME,
	NETWORK_REG_STATUS_ROAMING,
	NETWORK_REG_STATUS_OFFLINE,
	NETWORK_REG_STATUS_SEARCHING,
	NETWORK_REG_STATUS_NO_SIM,
	NETWORK_REG_STATUS_POWEROFF,
	NETWORK_REG_STATUS_POWERSAFE,
	NETWORK_REG_STATUS_NO_COVERAGE,
	NETWORK_REG_STATUS_REJECTED,
	NETWORK_REG_STATUS_UNKOWN
};

struct csd_call {
	char *path;
	int status;
	gboolean originating;
	gboolean emergency;
	gboolean on_hold;
	gboolean conference;
	char *number;
	gboolean setup;
	uint32_t call_id;
	char *sender;
};

static struct {
	char *operator_name;
	uint8_t status;
	int32_t signal_bars;
} net = {
	.operator_name = NULL,
	.status = NETWORK_REG_STATUS_UNKOWN,
	/* Init as 0 meaning inactive mode. In modem power off state
	 * can be be -1, but we treat all values as 0s regardless
	 * inactive or power off. */
	.signal_bars = 0,
};

/* Supported set of call hold operations */
/*static const char *telephony_chld_str = "0,1,1x,2,2x,3,4";*/
static const char *telephony_chld_str = "0,1,2,3";


static char *subscriber_number = NULL;	/* Subscriber number */


static struct {
	int32_t path_id;
	int32_t charset_id;
} ag_pb_info = {
	.path_id = 0,
	.charset_id = 0,
};

static void call_set_status(struct csd_call *call, dbus_uint32_t status);

static void free_sender_path(sender_path_t *s_path)
{
	if (s_path == NULL)
		return;

	g_free(s_path->path);
	g_free(s_path->sender);
	g_free(s_path);
}

static void free_sender_list()
{
	GSList *l;

	for (l = sender_paths; l != NULL; l = l->next) {
		free_sender_path(l->data);
	}
	g_slist_free(sender_paths);
	sender_paths = NULL;
	return;
}

static int telephony_remove_from_sender_list(const char *sender, const char *path)
{
	GSList *l;
	sender_path_t *s_path;

	if (sender == NULL || path == NULL)
		return  -EINVAL;

	for (l = sender_paths; l != NULL; l = l->next) {
		s_path = l->data;
		if (s_path == NULL)
			return -ENOENT;
		if (g_strcmp0(s_path->path, path) == 0) {
			g_dbus_remove_watch(ag_connection, s_path->watch_id);
			sender_paths = g_slist_remove(sender_paths, s_path);
			free_sender_path(s_path);

			/*Free sender_paths if no application is registered*/
			if (0 == g_slist_length(sender_paths)) {
				g_slist_free(sender_paths);
				sender_paths = NULL;
			}
			return 0;
		}
	}
	return -ENOENT;
}

static void remove_call_with_sender(gchar *sender)
{
	GSList *l;

	DBG("+\n");

	if (sender == NULL)
		return;

	for (l = calls; l != NULL; l = l->next) {
		struct csd_call *call = l->data;

		if (g_strcmp0(call->sender, sender) == 0)
			/* Release the Call and inform headset */
			call_set_status(call, CSD_CALL_STATUS_MT_RELEASE);
	}

	DBG("-\n");
}

static void telephony_app_exit_cb(DBusConnection *conn, void *user_data)
{
	sender_path_t *s_path = (sender_path_t *)user_data;

	DBG("+\n");

	if (s_path == NULL)
		return;

	/* check any active call from application */
	remove_call_with_sender(s_path->sender);

	if (!telephony_remove_from_sender_list(s_path->sender, s_path->path))
		DBG("Application removed \n");
	else
		DBG("Application not removed \n");
	DBG("-\n");
}

static gboolean telephony_is_registered(const char *path)
{
	GSList *l;
	sender_path_t *s_path;

	if (path == NULL || sender_paths == NULL)
		return FALSE;

	for (l = sender_paths; l != NULL; l = l->next) {
		s_path = l->data;
		if (s_path == NULL)
			break;

		if (g_strcmp0(s_path->path, path) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean telephony_is_call_allowed(const char *path)
{
	GSList *l;

	if (path == NULL)
		return FALSE;

	/*if call list doesn't exist the call should be allowed, since its a new call*/
	if (!calls)
		return TRUE;

	for (l = calls; l != NULL; l = l->next) {
		struct csd_call *call = l->data;

		if (g_strcmp0(call->path, path) == 0)
			return TRUE;

	}
	return FALSE;
}

static int telephony_add_to_sender_list(const char *sender, const char *path)
{
	sender_path_t *s_path;

	if (sender == NULL || path == NULL)
		return -EINVAL;

	/*check if already registered*/
	if (telephony_is_registered(path)) {
		return -EEXIST;
	}

	s_path = g_new0(sender_path_t, 1);
	s_path->path = g_strdup(path);
	s_path->sender = g_strdup(sender);
	s_path->watch_id = g_dbus_add_disconnect_watch(ag_connection, sender,
					telephony_app_exit_cb, s_path, NULL);
	sender_paths = g_slist_append(sender_paths, s_path);
	return 0;
}

static struct csd_call *find_call_with_id(uint32_t call_id)

{
	GSList *l;

	for (l = calls; l != NULL; l = l->next) {
		struct csd_call *call = l->data;

		if (call->call_id == call_id)
			return call;
	}

	return NULL;
}

static struct csd_call *find_non_held_call(void)
{
	GSList *l;

	for (l = calls; l != NULL; l = l->next) {
		struct csd_call *call = l->data;

		if (call->status == CSD_CALL_STATUS_IDLE)
			continue;

		if (call->status != CSD_CALL_STATUS_HOLD)
			return call;
	}

	return NULL;
}

static struct csd_call *find_non_idle_call(void)
{
	GSList *l;

	for (l = calls; l != NULL; l = l->next) {
		struct csd_call *call = l->data;

		if (call->status != CSD_CALL_STATUS_IDLE)
			return call;
	}

	return NULL;
}

static struct csd_call *find_call_with_status(int status)
{
	GSList *l;

	if (NULL != calls) {
		for (l = calls; l != NULL; l = l->next) {
			struct csd_call *call = l->data;

			if (call->status == status)
				return call;
		}
	}
	return NULL;
}

static void csd_call_free(struct csd_call *call)
{
	if (!call)
		return;

	g_free(call->path);
	g_free(call->number);
	g_free(call->sender);

	g_free(call);
}

static int reject_call(struct csd_call *call)
{
	DBusMessage *msg;

	DBG("+\n");

	DBG("telephony-tizen: reject_call ");

	msg = dbus_message_new_method_call(HFP_AGENT_SERVICE, HFP_AGENT_PATH,
					HFP_AGENT_INTERFACE, "RejectCall");
	if (!msg) {
		error("Unable to allocate new D-Bus message");
		return -ENOMEM;
	}
	DBG(" Path =[ %s] and Call id = [%d]\n", call->path, call->call_id);
	if (!dbus_message_append_args(msg, DBUS_TYPE_UINT32, &call->call_id,
			DBUS_TYPE_STRING, &call->path,
			DBUS_TYPE_STRING, &call->sender,
			DBUS_TYPE_INVALID)) {

		DBG("dbus_message_append_args -ENOMEM\n");
		dbus_message_unref(msg);
		return -ENOMEM;
	}

	g_dbus_send_message(ag_connection, msg);

	DBG("-\n");

	return 0;

}

static int release_call(struct csd_call *call)
{
	DBusMessage *msg;

	DBG("+\n");

	DBG("telephony-tizen: release_call ");

	msg = dbus_message_new_method_call(HFP_AGENT_SERVICE, HFP_AGENT_PATH,
					HFP_AGENT_INTERFACE, "ReleaseCall");
	if (!msg) {
		error("Unable to allocate new D-Bus message");
		return -ENOMEM;
	}
	DBG("Path =[ %s] and Call id = [%d]\n", call->path, call->call_id);
	if (!dbus_message_append_args(msg, DBUS_TYPE_UINT32, &call->call_id,
			DBUS_TYPE_STRING, &call->path,
			DBUS_TYPE_STRING, &call->sender,
			DBUS_TYPE_INVALID)) {

		DBG("dbus_message_append_args -ENOMEM\n");
		dbus_message_unref(msg);
		return -ENOMEM;
	}

	g_dbus_send_message(ag_connection, msg);

	DBG("-\n");

	return 0;
}

static int release_conference(void)
{
	GSList *l;

	DBG("+\n");

	for (l = calls; l != NULL; l = l->next) {
		struct csd_call *call = l->data;

		if (call->conference)
			release_call(call);
	}

	DBG("-\n");

	return 0;
}

static int dbus_method_call_send(const char *dest, const char *path,
				const char *interface, const char *method,
				DBusPendingCallNotifyFunction cb,
				void *user_data, int type, ...)
{
	DBusMessage *msg;
	DBusPendingCall *call;
	va_list args;

	DBG("+\n");

	msg = dbus_message_new_method_call(dest, path, interface, method);
	if (!msg) {
		error("Unable to allocate new D-Bus %s message", method);
		return -ENOMEM;
	}

	va_start(args, type);

	if (!dbus_message_append_args_valist(msg, type, args)) {
		dbus_message_unref(msg);
		va_end(args);
		return -EIO;
	}

	va_end(args);

	if (!cb) {
		g_dbus_send_message(ag_connection, msg);
		return 0;
	}

	if (!dbus_connection_send_with_reply(ag_connection, msg, &call, -1)) {
		error("Sending %s failed", method);
		dbus_message_unref(msg);
		return -EIO;
	}

	dbus_pending_call_set_notify(call, cb, user_data, NULL);
	dbus_message_unref(msg);
	DBG("-\n");

	return 0;
}

static int answer_call(struct csd_call *call)
{
	DBusMessage *msg;
	DBG("+\n");

	msg = dbus_message_new_method_call(HFP_AGENT_SERVICE, HFP_AGENT_PATH,
					HFP_AGENT_INTERFACE, "AnswerCall");
	if (!msg) {
		error("Unable to allocate new D-Bus message");
		return -ENOMEM;
	}

	if (!dbus_message_append_args(msg, DBUS_TYPE_UINT32, &call->call_id,
			DBUS_TYPE_STRING, &call->path,
			DBUS_TYPE_STRING, &call->sender,
			DBUS_TYPE_INVALID)) {

		DBG("dbus_message_append_args -ENOMEM\n");
		dbus_message_unref(msg);
		return -ENOMEM;
	}
	g_dbus_send_message(ag_connection, msg);
	DBG("-\n");
	return 0;
}

static int reject_accept_call(struct csd_call *call, void *telephony_device)
{
	uint32_t chld_value = 1;

	return dbus_method_call_send(HFP_AGENT_SERVICE, HFP_AGENT_PATH,
			HFP_AGENT_INTERFACE, "ThreewayCall",
			NULL, telephony_device,
			DBUS_TYPE_UINT32, &chld_value,
			DBUS_TYPE_STRING, &call->path,
			DBUS_TYPE_STRING, &call->sender,
			DBUS_TYPE_INVALID);
}

static int number_type(const char *number)
{
	if (number == NULL)
		return NUMBER_TYPE_TELEPHONY;

	if (number[0] == '+' || strncmp(number, "00", 2) == 0)
		return NUMBER_TYPE_INTERNATIONAL;

	return NUMBER_TYPE_TELEPHONY;
}

/* Since we dont have support from voice call regarding call conference this */
/* function will handle both join and split scenarios */

/* This function checks the status of each call in the list and set/unset */
/* conference status based on below algorithm */
/* If more that one active/held calls are there, conf flag of those calls will be set */
/* If only one active/held call is there, conf flag of those calls will be unset */
static void handle_conference(void)
{
	GSList *l;
	struct csd_call *first_active_call = NULL;
	struct csd_call *first_held_call = NULL;
	int active_call_count = 0;
	int held_call_count = 0;

	for (l = calls; l != NULL; l = l->next) {
		struct csd_call *call = l->data;

		if (call->status == CSD_CALL_STATUS_ACTIVE) {
			if (first_active_call == NULL)
				first_active_call = call;

			active_call_count++;

			if (active_call_count >= 2) {
				if (!first_active_call->conference)
					first_active_call->conference = TRUE;
				call->conference = TRUE;
			}

		} else if (call->status == CSD_CALL_STATUS_HOLD) {
			if (first_held_call == NULL)
				first_held_call = call;

			held_call_count++;

			if (held_call_count >= 2) {
				if (!first_held_call->conference)
					first_held_call->conference = TRUE;
				call->conference = TRUE;
			}
		}
	}

	if (active_call_count == 1) {
		if (first_active_call->conference)
			first_active_call->conference = FALSE;
	}

	if (held_call_count == 1) {
		if (first_held_call->conference)
			first_held_call->conference = FALSE;
	}
}

static void call_set_status(struct csd_call *call, dbus_uint32_t status)
{
	dbus_uint32_t prev_status;
	int callheld = 0;

	DBG("+\n");

	callheld = telephony_get_indicator(telephony_ag_indicators, "callheld");

	prev_status = call->status;
	DBG("Call %s Call id %d changed from %s to %s", call->path, call->call_id,
		call_status_str[prev_status], call_status_str[status]);

	if (prev_status == status) {
		DBG("Ignoring CSD Call state change to existing state");
		return;
	}

	call->status = (int) status;

	switch (status) {
	case CSD_CALL_STATUS_IDLE:
		if (call->setup) {
			telephony_update_indicator(telephony_ag_indicators,
							"callsetup",
							EV_CALLSETUP_INACTIVE);
			if (!call->originating)
				telephony_calling_stopped_ind();
		}

		g_free(call->number);
		call->number = NULL;
		call->originating = FALSE;
		call->emergency = FALSE;
		call->on_hold = FALSE;
		call->conference = FALSE;
		call->setup = FALSE;
		break;
	case CSD_CALL_STATUS_CREATE:
		call->originating = TRUE;
		call->setup = TRUE;
		break;
	case CSD_CALL_STATUS_COMING:
		call->originating = FALSE;
		call->setup = TRUE;
		break;
	case CSD_CALL_STATUS_PROCEEDING:
		break;
	case CSD_CALL_STATUS_MO_ALERTING:
		telephony_update_indicator(telephony_ag_indicators, "callsetup",
						EV_CALLSETUP_ALERTING);
		break;
	case CSD_CALL_STATUS_MT_ALERTING:
		/* Some headsets expect incoming call notification before they
		 * can send ATA command. When call changed status from waiting
		 * to alerting we need to send missing notification. Otherwise
		 * headsets like Nokia BH-108 or BackBeat 903 are unable to
		 * answer incoming call that was previously waiting. */
		if (prev_status == CSD_CALL_STATUS_WAITING)
			telephony_incoming_call_ind(call->number,
						number_type(call->number));
		break;
	case CSD_CALL_STATUS_WAITING:
		break;
	case CSD_CALL_STATUS_ANSWERED:
		break;
	case CSD_CALL_STATUS_ACTIVE:
		if (call->on_hold) {
			call->on_hold = FALSE;
			if (find_call_with_status(CSD_CALL_STATUS_HOLD))
				telephony_update_indicator(telephony_ag_indicators,
							"callheld",
							EV_CALLHELD_MULTIPLE);
			else
				telephony_update_indicator(telephony_ag_indicators,
							"callheld",
							EV_CALLHELD_NONE);
		} else {
			if (!g_slist_find(active_calls, call))
				active_calls = g_slist_prepend(active_calls, call);
			if (g_slist_length(active_calls) == 1)
				telephony_update_indicator(telephony_ag_indicators,
								"call",
								EV_CALL_ACTIVE);
			/* Upgrade callheld status if necessary */
			if (callheld == EV_CALLHELD_ON_HOLD)
				telephony_update_indicator(telephony_ag_indicators,
							"callheld",
							EV_CALLHELD_MULTIPLE);
			telephony_update_indicator(telephony_ag_indicators,
							"callsetup",
							EV_CALLSETUP_INACTIVE);
			if (!call->originating)
				telephony_calling_stopped_ind();
			call->setup = FALSE;
		}
		break;
	case CSD_CALL_STATUS_MO_RELEASE:
	case CSD_CALL_STATUS_MT_RELEASE:
		active_calls = g_slist_remove(active_calls, call);
		if (g_slist_length(active_calls) == 0)
			telephony_update_indicator(telephony_ag_indicators, "call",
							EV_CALL_INACTIVE);

		if ((prev_status == CSD_CALL_STATUS_MO_ALERTING) ||
			(prev_status == CSD_CALL_STATUS_COMING) ||
			(prev_status == CSD_CALL_STATUS_CREATE) ||
			(prev_status == CSD_CALL_STATUS_WAITING)) {
				telephony_update_indicator(telephony_ag_indicators,
							"callsetup",
							EV_CALLSETUP_INACTIVE);
		}

		if (prev_status == CSD_CALL_STATUS_COMING) {
			if (!call->originating)
				telephony_calling_stopped_ind();
		}
		calls = g_slist_remove(calls, call);
		csd_call_free(call);

		break;
	case CSD_CALL_STATUS_HOLD_INITIATED:
		break;
	case CSD_CALL_STATUS_HOLD:
		call->on_hold = TRUE;
		if (find_non_held_call())
			telephony_update_indicator(telephony_ag_indicators,
							"callheld",
							EV_CALLHELD_MULTIPLE);
		else
			telephony_update_indicator(telephony_ag_indicators,
							"callheld",
							EV_CALLHELD_ON_HOLD);
		break;
	case CSD_CALL_STATUS_RETRIEVE_INITIATED:
		break;
	case CSD_CALL_STATUS_RECONNECT_PENDING:
		break;
	case CSD_CALL_STATUS_TERMINATED:
		if (call->on_hold &&
				!find_call_with_status(CSD_CALL_STATUS_HOLD)) {
			telephony_update_indicator(telephony_ag_indicators,
							"callheld",
							EV_CALLHELD_NONE);
			return;
		}

		if (callheld == EV_CALLHELD_MULTIPLE &&
				find_call_with_status(CSD_CALL_STATUS_HOLD) &&
				!find_call_with_status(CSD_CALL_STATUS_ACTIVE))
			telephony_update_indicator(telephony_ag_indicators,
							"callheld",
							EV_CALLHELD_ON_HOLD);
		break;
	case CSD_CALL_STATUS_SWAP_INITIATED:
		break;
	default:
		error("Unknown call status %u", status);
		break;
	}

	/* Update the conference status of each call */
	handle_conference();

	DBG("-\n");
}

static void telephony_chld_reply(DBusPendingCall *call, void *data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	DBusError derr;

	DBG("redial_reply");

	dbus_error_init(&derr);
	if (!dbus_set_error_from_message(&derr, reply)) {
		DBG("chld  reply: cmd is valid");
		telephony_dial_number_rsp(data, CME_ERROR_NONE);
		goto done;
	}

	DBG("chld_reply reply: %s", derr.message);

	dbus_error_free(&derr);
	telephony_dial_number_rsp(data, CME_ERROR_AG_FAILURE);

done:
	dbus_message_unref(reply);
}

void telephony_call_hold_req(void *telephony_device, const char *cmd)
{
	const char *idx;
	struct csd_call *call;
	int err = 0;
	uint32_t chld_value;
	GSList *l = NULL;
	sender_path_t *s_path = NULL;

	DBG("+\n");

	DBG("telephony-tizen: got call hold request %s", cmd);

	/* Find any Ongoing call, in active/held/waiting */
	if (NULL == (call = find_call_with_status(CSD_CALL_STATUS_ACTIVE)))
		if (NULL == (call = find_call_with_status(
						CSD_CALL_STATUS_HOLD)))
			if (NULL == (call = find_call_with_status(
						CSD_CALL_STATUS_WAITING))) {
				DBG("No Onging Call \n");
				telephony_call_hold_rsp(telephony_device,
							CME_ERROR_AG_FAILURE);
				return;
			}

	/*Get sender path using call path*/
	for (l = sender_paths; l != NULL; l = l->next) {
		s_path = l->data;
		if (s_path == NULL) {
			telephony_call_hold_rsp(telephony_device,
							CME_ERROR_AG_FAILURE);
			return;
		}
		if (g_strcmp0(s_path->path, call->path) == 0)
			break;
	}

	if (s_path == NULL) {
		telephony_call_hold_rsp(telephony_device,
						CME_ERROR_AG_FAILURE);
		return;
	}

	idx = &cmd[0];
	chld_value = strtoul(idx, NULL, 0);

	DBG("Sender = %s path = %s \n", s_path->sender, s_path->path);

	err = dbus_method_call_send(HFP_AGENT_SERVICE, HFP_AGENT_PATH,
				HFP_AGENT_INTERFACE, "ThreewayCall",
				telephony_chld_reply, telephony_device,
				DBUS_TYPE_UINT32, &chld_value,
				DBUS_TYPE_STRING, &call->path,
				DBUS_TYPE_STRING, &call->sender,
				DBUS_TYPE_INVALID);

	if (err)
		telephony_call_hold_rsp(telephony_device,
					CME_ERROR_AG_FAILURE);
	DBG("-\n");
}

static int update_registration_status(uint8_t status)
{
	uint8_t new_status;
	int ret = 0;
	DBG("+\n");

	new_status = status;

	if (net.status == new_status)
		return ret;

	switch (new_status) {
	case NETWORK_REG_STATUS_HOME:
		ret = telephony_update_indicator(telephony_ag_indicators, "roam",
							EV_ROAM_INACTIVE);

		if (net.status > NETWORK_REG_STATUS_ROAMING) {
			ret = telephony_update_indicator(telephony_ag_indicators,
							"service",
							EV_SERVICE_PRESENT);
		}
		break;
	case NETWORK_REG_STATUS_ROAMING:
		ret = telephony_update_indicator(telephony_ag_indicators, "roam",
							EV_ROAM_ACTIVE);

		if (net.status > NETWORK_REG_STATUS_ROAMING) {
			ret = telephony_update_indicator(telephony_ag_indicators,
							"service",
							EV_SERVICE_PRESENT);
		}
		break;
	case NETWORK_REG_STATUS_OFFLINE:
	case NETWORK_REG_STATUS_SEARCHING:
	case NETWORK_REG_STATUS_NO_SIM:
	case NETWORK_REG_STATUS_POWEROFF:
	case NETWORK_REG_STATUS_POWERSAFE:
	case NETWORK_REG_STATUS_NO_COVERAGE:
	case NETWORK_REG_STATUS_REJECTED:
	case NETWORK_REG_STATUS_UNKOWN:
		if (net.status < NETWORK_REG_STATUS_OFFLINE) {
			ret = telephony_update_indicator(telephony_ag_indicators,
							"service",
							EV_SERVICE_NONE);
		}
		break;
	}

	net.status = new_status;

	DBG("telephony-tizen: registration status changed: %d", status);
	DBG("-\n");

	return ret;
}

static int update_signal_strength(int32_t signal_bars)
{
	DBG("+\n");

	if (signal_bars < 0) {
		DBG("signal strength smaller than expected: %d < 0",
				signal_bars);
		signal_bars = 0;
	} else if (signal_bars > 5) {
		DBG("signal strength greater than expected: %d > 5",
				signal_bars);
		signal_bars = 5;
	}

	if (net.signal_bars == signal_bars)
		return 0;

	net.signal_bars = signal_bars;
	DBG("telephony-tizen: signal strength updated: %d/5", signal_bars);

	return telephony_update_indicator(telephony_ag_indicators, "signal", signal_bars);
}

static int update_battery_strength(int32_t battery_level)
{
	int current_battchg = 0;

	DBG("+\n");

	current_battchg = telephony_get_indicator(telephony_ag_indicators, "battchg");

	if (battery_level < 0) {
		DBG("Battery strength smaller than expected: %d < 0",
								battery_level);
		battery_level = 0;
	} else if (battery_level > 5) {
		DBG("Battery strength greater than expected: %d > 5",
								battery_level);
		battery_level = 5;
	}
	if (current_battchg == battery_level)
		return 0;

	DBG("telephony-tizen: battery strength updated: %d/5", battery_level);
	DBG("-\n");

	return telephony_update_indicator(telephony_ag_indicators,
			"battchg", battery_level);
}


static int update_operator_name(const char *name)
{
	DBG("+\n");
	if (name == NULL)
		return -EINVAL;

	g_free(net.operator_name);
	net.operator_name = g_strndup(name, 16);
	DBG("telephony-tizen: operator name updated: %s", name);
	DBG("-\n");
	return 0;
}

static int update_subscriber_number(const char *number)
{
	DBG("+\n");
	if (number == NULL)
		return -EINVAL;

	g_free(subscriber_number);
	subscriber_number = g_strdup(number);
	DBG("telephony-tizen: subscriber_number updated: %s", subscriber_number);
	DBG("-\n");
	return 0;
}

static DBusMessage *telephony_error_reply(DBusMessage *msg, int error)
{
	switch (error) {
	case -ENOENT:
		return btd_error_not_available(msg);
	case -ENODEV:
		return btd_error_not_connected(msg);
	case -EBUSY:
		return btd_error_busy(msg);
	case -EINVAL:
		return btd_error_invalid_args(msg);
	case -EEXIST:
		return btd_error_already_exists(msg);
	case -ENOMEM:
		return btd_error_failed(msg, "No memory");
	case -EIO:
		return btd_error_failed(msg, "I/O error");
	default:
		return dbus_message_new_method_return(msg);
	}
}

static struct csd_call *create_call(DBusMessage *msg, const char *path,
					const char *number, uint32_t call_id,
					const char *sender)

{
	struct csd_call *call;

	call = find_call_with_id(call_id);
	if (!call) {
		call = g_new0(struct csd_call, 1);
		call->path = g_strdup(path);
		call->call_id = call_id;
		call->number = g_strdup(number);
		call->sender = g_strdup(sender);
		calls = g_slist_append(calls, call);
	}
	return call;
}

static DBusMessage *incoming(DBusConnection *conn, DBusMessage *msg,
					void *data)
{
	const char *number, *call_path;
	const char *sender;
	struct csd_call *call;
	uint32_t call_id;
	int ret;
	DBG("+\n");
	DBG("telephony_incoming()\n");

	if (!dbus_message_get_args(msg, NULL,
					DBUS_TYPE_STRING, &call_path,
					DBUS_TYPE_STRING, &number,
					DBUS_TYPE_UINT32, &call_id,
					DBUS_TYPE_STRING, &sender,
					DBUS_TYPE_INVALID)) {

		return btd_error_invalid_args(msg);
	}

	ret = telephony_is_registered(call_path);
	if (!ret)
		return telephony_error_reply(msg,  -ENOENT);

	/*Check in the active call list, if any of the call_path exists if not don't allow
	the call since it is initated by other applicatoin*/

	ret = telephony_is_call_allowed(call_path);
	if (!ret)
		return telephony_error_reply(msg,  -ENOENT);


	call = create_call(msg, call_path, number, call_id, sender);

	DBG("Incoming call to %s from number %s call id %d", call_path, number, call_id);
	ret = telephony_update_indicator(telephony_ag_indicators, "callsetup",
					EV_CALLSETUP_INCOMING);
	if (ret)
		return telephony_error_reply(msg, ret);

	if (find_call_with_status(CSD_CALL_STATUS_ACTIVE) ||
			find_call_with_status(CSD_CALL_STATUS_HOLD)) {
		ret = telephony_call_waiting_ind(call->number,
						number_type(call->number));
		if (ret)
			return telephony_error_reply(msg, ret);

		call_set_status(call, CSD_CALL_STATUS_WAITING);
	} else {
		ret = telephony_incoming_call_ind(call->number,
						number_type(call->number));
		if (ret)
			return telephony_error_reply(msg, ret);

		call_set_status(call, CSD_CALL_STATUS_COMING);
	}
	DBG("-\n");
	return dbus_message_new_method_return(msg);
}

static DBusMessage *outgoing(DBusConnection *conn, DBusMessage *msg,
					void *data)
{
	const char *number, *call_path;
	const char *sender;
	struct csd_call *call;
	uint32_t call_id;
	int ret;

	DBG("+\n");
	DBG("telephony_outgoing_call()\n");

	if (!dbus_message_get_args(msg, NULL,
					DBUS_TYPE_STRING, &call_path,
					DBUS_TYPE_STRING, &number,
					DBUS_TYPE_UINT32, &call_id,
					DBUS_TYPE_STRING, &sender,
					DBUS_TYPE_INVALID)) {
		return btd_error_invalid_args(msg);
	}

	ret = telephony_is_registered(call_path);
	if (!ret)
		return telephony_error_reply(msg, -ENOENT);

	/*Check in the active call list, if any of the call_path exists if not don't allow
	the call since it is initated by other applicatoin*/

	ret = telephony_is_call_allowed(call_path);
	if (!ret)
		return telephony_error_reply(msg, -ENOENT);

	call = create_call(msg, call_path, number, call_id, sender);

	DBG("Outgoing call to %s from number %s call id %d", call_path, number, call_id);

	call_set_status(call, CSD_CALL_STATUS_CREATE);

	ret = telephony_update_indicator(telephony_ag_indicators, "callsetup",
					EV_CALLSETUP_OUTGOING);
	if (ret)
		return telephony_error_reply(msg, ret);

	DBG("-\n");
	return dbus_message_new_method_return(msg);
}

static DBusMessage *set_call_status(DBusConnection *conn, DBusMessage *msg,
					void *data)
{
	struct csd_call *call;
	dbus_uint32_t status, call_id;
	const char *call_path;
	const char *sender;
	int ret;

	DBG("+\n");

	if (!dbus_message_get_args(msg, NULL,
					DBUS_TYPE_STRING, &call_path,
					DBUS_TYPE_UINT32, &status,
					DBUS_TYPE_UINT32, &call_id,
					DBUS_TYPE_STRING, &sender,
					DBUS_TYPE_INVALID)) {
		error("Unexpected paramters in Instance.CallStatus() signal");
		return btd_error_invalid_args(msg);
	}

	if (status > 16) {
		return btd_error_invalid_args(msg);
	}

	ret = telephony_is_registered(call_path);
	if (!ret)
		return telephony_error_reply(msg, -ENOENT);

	ret = telephony_is_call_allowed(call_path);
	if (!ret)
		return telephony_error_reply(msg, -ENOENT);

	DBG("status = [%d] and call_id = [%d] \n", status, call_id);

	call = find_call_with_id(call_id);
	if (!call) {
/*
	call_path is equal to CSD_CALL_PATH then we should update the call list
	since the call_path is sent from native AG applicaton

	Added for updation of the call status if the call is not added in the call list
*/
		call = create_call(msg, call_path, NULL, call_id, sender);
	}
	call_set_status(call, status);
	DBG("-\n");
	return dbus_message_new_method_return(msg);
}

static DBusMessage *register_telephony_agent(DBusConnection *conn, DBusMessage *msg,
					void *data)
{
	gboolean flag;
	const char *sender;
	const char *path;
	int ret;

	DBG("+\n");

	if (!dbus_message_get_args(msg, NULL,
					DBUS_TYPE_BOOLEAN, &flag,
					DBUS_TYPE_STRING, &path,
					DBUS_TYPE_STRING, &sender,
					DBUS_TYPE_INVALID)) {
		error("Unexpected parameters in RegisterSenderPath");
		return btd_error_invalid_args(msg);
	}

	DBG("flag = %d \n", flag);
	DBG("Sender = %s \n", sender);
	DBG("path = %s \n", path);

	if (flag)
		ret = telephony_add_to_sender_list(sender, path);
	else
		ret = telephony_remove_from_sender_list(sender, path);

	if (ret)
		return telephony_error_reply(msg, ret);

	DBG("-\n");

	return dbus_message_new_method_return(msg);
}

static DBusMessage *set_property(DBusConnection *conn, DBusMessage *msg,
					void *data)
{
	const char *property;
	DBusMessageIter iter;
	DBusMessageIter sub;
	int ret;

	if (!dbus_message_iter_init(msg, &iter))
		return btd_error_invalid_args(msg);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
		return btd_error_invalid_args(msg);

	dbus_message_iter_get_basic(&iter, &property);
	dbus_message_iter_next(&iter);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT)
		return btd_error_invalid_args(msg);
	dbus_message_iter_recurse(&iter, &sub);

	if (g_str_equal("RegistrationChanged", property)) {
		uint8_t value;
		if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_BYTE)
			return btd_error_invalid_args(msg);

		dbus_message_iter_get_basic(&sub, &value);

		ret = update_registration_status(value);
		if (ret)
			return telephony_error_reply(msg, ret);
	} else if (g_str_equal("OperatorNameChanged", property)) {
		const char *name;
		if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_STRING)
			return btd_error_invalid_args(msg);

		dbus_message_iter_get_basic(&sub, &name);

		ret = update_operator_name(name);
		if (ret)
			return telephony_error_reply(msg, ret);
	} else if (g_str_equal("SignalBarsChanged", property)) {
		int32_t value;
		if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INT32)
			return btd_error_invalid_args(msg);

		dbus_message_iter_get_basic(&sub, &value);
		ret = update_signal_strength(value);
		if (ret)
			return telephony_error_reply(msg, ret);

	} else if (g_str_equal("BatteryBarsChanged", property)) {
		int32_t value;
		if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INT32)
			return btd_error_invalid_args(msg);

		dbus_message_iter_get_basic(&sub, &value);
		ret = update_battery_strength(value);
		if (ret)
			return telephony_error_reply(msg, ret);

	} else if (g_str_equal("SubscriberNumberChanged", property)) {
		const char *number;
		if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_STRING)
			return btd_error_invalid_args(msg);

		dbus_message_iter_get_basic(&sub, &number);
		ret = update_subscriber_number(number);
		if (ret)
			return telephony_error_reply(msg, ret);
	}

	return dbus_message_new_method_return(msg);
}

static GDBusMethodTable telephony_methods[] = {
	{ GDBUS_METHOD("Incoming",
			GDBUS_ARGS({ "path", "s" }, { "number", "s" },
					{ "id", "u" }, { "sender", "s" }),
			NULL,
			incoming) },
	{ GDBUS_METHOD("Outgoing",
			GDBUS_ARGS({ "path", "s" }, { "number", "s" },
					{ "id", "u" }, { "sender", "s" }),
			NULL,
			outgoing) },
	{ GDBUS_METHOD("SetCallStatus",
			GDBUS_ARGS({ "path", "s" }, { "status", "u" },
					{ "id", "u" }, { "sender", "s" }),
			NULL,
			set_call_status) },
	{ GDBUS_METHOD("RegisterTelephonyAgent",
			GDBUS_ARGS({ "flag", "b" }, { "path", "s" },
					{ "sender", "s" }),
			NULL,
			register_telephony_agent) },
	{ GDBUS_METHOD("SetProperty",
			GDBUS_ARGS({ "name", "s" }, { "property", "v" }),
			NULL,
			set_property) },
	{ }
};

static void path_unregister(void *data)
{
	DBG("+\n");
	g_dbus_unregister_interface(ag_connection, TELEPHONY_CSD_OBJECT_PATH,
					TELEPHONY_CSD_INTERFACE);
	DBG("-\n");
}

/*API's that shall be ported*/
int telephony_init(void)
{
	uint32_t features = AG_FEATURE_EC_ANDOR_NR |
				AG_FEATURE_REJECT_A_CALL |
				AG_FEATURE_ENHANCED_CALL_STATUS |
				AG_FEATURE_THREE_WAY_CALLING |
				AG_FEATURE_VOICE_RECOGNITION;
	int i;

	DBG("");

	ag_connection = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);

	if (!g_dbus_register_interface(ag_connection, TELEPHONY_CSD_OBJECT_PATH,
					TELEPHONY_CSD_INTERFACE,
					telephony_methods, NULL, NULL,
					NULL, path_unregister)) {
		error("D-Bus failed to register %s interface", TELEPHONY_CSD_INTERFACE);
		return -1;
	}

	/* Reset indicators */
	for (i = 0; telephony_ag_indicators[i].desc != NULL; i++) {
		if (g_str_equal(telephony_ag_indicators[i].desc, "battchg"))
			telephony_ag_indicators[i].val = 5;
		else
			telephony_ag_indicators[i].val = 0;
	}

	/*Initializatoin of the indicators*/
	telephony_ready_ind(features, telephony_ag_indicators,
					BTRH_NOT_SUPPORTED,
					telephony_chld_str);

	return 0;
}

void telephony_exit(void)
{
	DBG("");

	g_free(net.operator_name);
	net.operator_name = NULL;

	g_free(subscriber_number);
	subscriber_number = NULL;

	net.status = NETWORK_REG_STATUS_UNKOWN;
	net.signal_bars = 0;

	g_slist_free(active_calls);
	active_calls = NULL;

	g_slist_foreach(calls, (GFunc) csd_call_free, NULL);
	g_slist_free(calls);
	calls = NULL;

	free_sender_list();

	g_dbus_unregister_interface(ag_connection, TELEPHONY_CSD_OBJECT_PATH,
						TELEPHONY_CSD_INTERFACE);

	dbus_connection_unref(ag_connection);
	ag_connection = NULL;

	telephony_deinit();
}

void telephony_device_connected(void *telephony_device)
{
	DBG("telephony-tizen: device %p connected", telephony_device);
}

void telephony_device_disconnected(void *telephony_device)
{
	DBG("telephony-tizen: device %p disconnected", telephony_device);
	events_enabled = FALSE;
}

void telephony_event_reporting_req(void *telephony_device, int ind)
{
	events_enabled = ind == 1 ? TRUE : FALSE;

	telephony_event_reporting_rsp(telephony_device, CME_ERROR_NONE);
}

void telephony_response_and_hold_req(void *telephony_device, int rh)
{
	DBG("telephony-tizen: response_and_hold_req - device %p disconnected",
		telephony_device);

	telephony_response_and_hold_rsp(telephony_device,
						CME_ERROR_NOT_SUPPORTED);
}

static void telephony_dial_number_reply(DBusPendingCall *call, void *data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	DBusError derr;

	DBG("redial_reply");

	dbus_error_init(&derr);
	if (!dbus_set_error_from_message(&derr, reply)) {
		DBG("hfg  reply: dial done  successfully");
		telephony_dial_number_rsp(data, CME_ERROR_NONE);
		goto done;
	}

	DBG("dial_reply reply: %s", derr.message);

	dbus_error_free(&derr);
	telephony_dial_number_rsp(data, CME_ERROR_AG_FAILURE);

done:
	dbus_message_unref(reply);
}

void telephony_dial_number_req(void *telephony_device, const char *number)
{
	uint32_t flags = callerid;

	DBG("telephony-tizen: dial request to %s", number);

	if (strncmp(number, "*31#", 4) == 0) {
		number += 4;
		flags = CALL_FLAG_PRESENTATION_ALLOWED;
	} else if (strncmp(number, "#31#", 4) == 0) {
		number += 4;
		flags = CALL_FLAG_PRESENTATION_RESTRICTED;
	} else if (number[0] == '>') {
		int location = strtol(&number[1], NULL, 0);

		if (0 != dbus_method_call_send(HFP_AGENT_SERVICE,
				HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
				"DialMemory",
				telephony_dial_number_reply, telephony_device,
				DBUS_TYPE_INT32, &location,
				DBUS_TYPE_INVALID)) {
			telephony_dial_number_rsp(telephony_device,
							CME_ERROR_AG_FAILURE);
		}
		return;
	}

	if (0 != dbus_method_call_send(HFP_AGENT_SERVICE, HFP_AGENT_PATH,
				HFP_AGENT_INTERFACE, "DialNum",
				NULL, NULL,
				DBUS_TYPE_STRING, &number,
				DBUS_TYPE_UINT32, &flags,
				DBUS_TYPE_INVALID)) {
		telephony_dial_number_rsp(telephony_device,
						CME_ERROR_AG_FAILURE);
		return;
	}

	telephony_dial_number_rsp(telephony_device, CME_ERROR_NONE);
}


void telephony_terminate_call_req(void *telephony_device)
{
	struct csd_call *call;
	struct csd_call *alerting;
	int err;

	DBG("+\n");

	call = find_call_with_status(CSD_CALL_STATUS_ACTIVE);
	if (!call)
		call = find_non_idle_call();

	if (!call) {
		error("No active call");
		telephony_terminate_call_rsp(telephony_device,
						CME_ERROR_NOT_ALLOWED);
		return;
	}

	if (NULL != find_call_with_status(CSD_CALL_STATUS_WAITING))
		err = reject_accept_call(call, telephony_device);
	else if (NULL != (alerting = find_call_with_status(CSD_CALL_STATUS_CREATE)))
		err = reject_call(alerting);
	else if (NULL != (alerting = find_call_with_status(CSD_CALL_STATUS_MO_ALERTING)))
		err = reject_call(alerting);
	else if (NULL != (alerting = find_call_with_status(CSD_CALL_STATUS_COMING)))
		err = reject_call(alerting);
	else if (call->conference)
		err = release_conference();
	else
		err = release_call(call);

	if (err < 0)
		telephony_terminate_call_rsp(telephony_device,
						CME_ERROR_AG_FAILURE);
	else
		telephony_terminate_call_rsp(telephony_device, CME_ERROR_NONE);
	DBG("-\n");

}

void telephony_answer_call_req(void *telephony_device)
{
	struct csd_call *call;

	call = find_call_with_status(CSD_CALL_STATUS_COMING);
	if (!call)
		call = find_call_with_status(CSD_CALL_STATUS_MT_ALERTING);

	if (!call)
		call = find_call_with_status(CSD_CALL_STATUS_PROCEEDING);

	if (!call)
		call = find_call_with_status(CSD_CALL_STATUS_WAITING);

	if (!call) {
		telephony_answer_call_rsp(telephony_device,
						CME_ERROR_NOT_ALLOWED);
		return;
	}

	if (answer_call(call) < 0)
		telephony_answer_call_rsp(telephony_device,
						CME_ERROR_AG_FAILURE);
	else
		telephony_answer_call_rsp(telephony_device, CME_ERROR_NONE);

}

void telephony_key_press_req(void *telephony_device, const char *keys)
{
	struct csd_call *active, *waiting;
	int err;

	DBG("telephony-tizen: got key press request for %s", keys);

	waiting = find_call_with_status(CSD_CALL_STATUS_COMING);
	if (!waiting)
		waiting = find_call_with_status(CSD_CALL_STATUS_MT_ALERTING);
	if (!waiting)
		waiting = find_call_with_status(CSD_CALL_STATUS_PROCEEDING);

	active = find_call_with_status(CSD_CALL_STATUS_ACTIVE);

	if (waiting)
		err = answer_call(waiting);
	else if (active)
		err = release_call(active);
	else {
		/*As per the HSP1.2 specification, for user action - Cellular phone can perform
		predefined user action. In our case we shall support Last no. dial*/
		dbus_method_call_send(HFP_AGENT_SERVICE, HFP_AGENT_PATH,
				HFP_AGENT_INTERFACE, "DialLastNum",
				telephony_dial_number_reply, telephony_device,
				DBUS_TYPE_INVALID);
		return;
	}

	if (err < 0)
		telephony_key_press_rsp(telephony_device,
							CME_ERROR_AG_FAILURE);
	else
		telephony_key_press_rsp(telephony_device, CME_ERROR_NONE);
}

void telephony_last_dialed_number_req(void *telephony_device)
{
	DBG("telephony-tizen: last dialed number request");

	if (0 != dbus_method_call_send(HFP_AGENT_SERVICE, HFP_AGENT_PATH,
				HFP_AGENT_INTERFACE, "DialLastNum",
				telephony_dial_number_reply, telephony_device,
				DBUS_TYPE_INVALID)) {
		telephony_dial_number_rsp(telephony_device,
						CME_ERROR_AG_FAILURE);
	}
}

void telephony_transmit_dtmf_req(void *telephony_device, char tone)
{
	char buf[2] = { tone, '\0' }, *buf_ptr = buf;
	struct csd_call *call;

	DBG("telephony-tizen: transmit dtmf: %s", buf);

	/* Find any Ongoing call, in active/held/waiting */
	if (NULL == (call = find_call_with_status(CSD_CALL_STATUS_ACTIVE)))
		if (NULL == (call = find_call_with_status(
						CSD_CALL_STATUS_HOLD)))
			if (NULL == (call = find_call_with_status(
						CSD_CALL_STATUS_WAITING))) {
				DBG("No Onging Call \n");
				telephony_transmit_dtmf_rsp(telephony_device,
						CME_ERROR_AG_FAILURE);
				return;
			}

	if (0 != dbus_method_call_send(HFP_AGENT_SERVICE, HFP_AGENT_PATH,
				HFP_AGENT_INTERFACE, "SendDtmf",
				NULL, NULL,
				DBUS_TYPE_STRING, &buf_ptr,
				DBUS_TYPE_STRING, &call->path,
				DBUS_TYPE_STRING, &call->sender,
				DBUS_TYPE_INVALID)) {
		telephony_transmit_dtmf_rsp(telephony_device,
						CME_ERROR_AG_FAILURE);
		return;
	}

	telephony_transmit_dtmf_rsp(telephony_device, CME_ERROR_NONE);
}

static int csd_status_to_hfp(struct csd_call *call)
{
	switch (call->status) {
	case CSD_CALL_STATUS_IDLE:
	case CSD_CALL_STATUS_MO_RELEASE:
	case CSD_CALL_STATUS_MT_RELEASE:
	case CSD_CALL_STATUS_TERMINATED:
		return -1;
	case CSD_CALL_STATUS_CREATE:
		return CALL_STATUS_DIALING;
	case CSD_CALL_STATUS_WAITING:
		return CALL_STATUS_WAITING;
	case CSD_CALL_STATUS_PROCEEDING:
		/* PROCEEDING can happen in outgoing/incoming */
		if (call->originating)
			return CALL_STATUS_DIALING;
		/*
		 * PROCEEDING is followed by WAITING CSD status, therefore
		 * second incoming call status indication is set immediately
		 * to waiting.
		 */
		if (g_slist_length(active_calls) > 0)
			return CALL_STATUS_WAITING;

		return CALL_STATUS_INCOMING;
	case CSD_CALL_STATUS_COMING:
		if (g_slist_length(active_calls) > 0)
			return CALL_STATUS_WAITING;

		return CALL_STATUS_INCOMING;
	case CSD_CALL_STATUS_MO_ALERTING:
		return CALL_STATUS_ALERTING;
	case CSD_CALL_STATUS_MT_ALERTING:
		return CALL_STATUS_INCOMING;
	case CSD_CALL_STATUS_ANSWERED:
	case CSD_CALL_STATUS_ACTIVE:
	case CSD_CALL_STATUS_RECONNECT_PENDING:
	case CSD_CALL_STATUS_SWAP_INITIATED:
	case CSD_CALL_STATUS_HOLD_INITIATED:
		return CALL_STATUS_ACTIVE;
	case CSD_CALL_STATUS_RETRIEVE_INITIATED:
	case CSD_CALL_STATUS_HOLD:
		return CALL_STATUS_HELD;
	default:
		return -1;
	}
}

void telephony_list_current_calls_req(void *telephony_device)
{
	GSList *l;
	int i;

	DBG("telephony-tizen: list current calls request");

	for (l = calls, i = 1; l != NULL; l = l->next, i++) {
		struct csd_call *call = l->data;
		int status, direction, multiparty;

		status = csd_status_to_hfp(call);
		if (status < 0)
			continue;

		direction = call->originating ?
				CALL_DIR_OUTGOING : CALL_DIR_INCOMING;

		multiparty = call->conference ?
				CALL_MULTIPARTY_YES : CALL_MULTIPARTY_NO;

		telephony_list_current_call_ind(i, direction, status,
						CALL_MODE_VOICE, multiparty,
						call->number,
						number_type(call->number));
	}

	telephony_list_current_calls_rsp(telephony_device, CME_ERROR_NONE);

}

static void telephony_operator_reply(DBusPendingCall *call, void *telephony_device)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	DBusError derr;
	const gchar *operator_name;

	DBG("telephony_operator_reply\n");

	dbus_error_init(&derr);

	if (dbus_set_error_from_message(&derr, reply)) {
		DBG("telephony_operator_reply error:%s", derr.message);
		goto failed;
	}

	if (!dbus_message_get_args(reply, NULL,
				DBUS_TYPE_STRING, &operator_name,
				DBUS_TYPE_INVALID))
		goto failed;

	DBG("telephony_operator_reply -operator_name:%s", operator_name);
	net.operator_name = g_strndup(operator_name, 16);

	telephony_operator_selection_ind(OPERATOR_MODE_AUTO,
				operator_name);
	telephony_operator_selection_rsp(telephony_device, CME_ERROR_NONE);
	return;

failed:
	dbus_error_free(&derr);
	telephony_operator_selection_ind(OPERATOR_MODE_AUTO, "UNKOWN");
	telephony_operator_selection_rsp(telephony_device, CME_ERROR_NONE);
}

void telephony_operator_selection_req(void *telephony_device)
{
	int err = 0;

	err = dbus_method_call_send(HFP_AGENT_SERVICE, HFP_AGENT_PATH,
				HFP_AGENT_INTERFACE, "GetOperatorName",
				telephony_operator_reply, telephony_device,
				DBUS_TYPE_INVALID);
	if (err)
		telephony_operator_selection_rsp(telephony_device,
				CME_ERROR_AG_FAILURE);
}

void telephony_nr_and_ec_req(void *telephony_device, gboolean enable)
{
	DBG("telephony-tizen: got %s NR and EC request",
			enable ? "enable" : "disable");

	if (0 != dbus_method_call_send(HFP_AGENT_SERVICE, HFP_AGENT_PATH,
				HFP_AGENT_INTERFACE, "NrecStatus",
				NULL, NULL, DBUS_TYPE_BOOLEAN, &enable,
				DBUS_TYPE_INVALID)) {
		telephony_nr_and_ec_rsp(telephony_device,
						CME_ERROR_AG_FAILURE);
		return;
	}

	telephony_nr_and_ec_rsp(telephony_device, CME_ERROR_NONE);
}

void telephony_voice_dial_req(void *telephony_device, gboolean enable)
{

	DBG("telephony-tizen: got %s voice dial request",
				enable ? "enable" : "disable");

	if (0 != dbus_method_call_send(HFP_AGENT_SERVICE, HFP_AGENT_PATH,
				HFP_AGENT_INTERFACE, "VoiceDial",
				NULL, NULL, DBUS_TYPE_BOOLEAN, &enable,
				DBUS_TYPE_INVALID)) {
		telephony_voice_dial_rsp(telephony_device,
						CME_ERROR_AG_FAILURE);
		return;
	}

	telephony_voice_dial_rsp(telephony_device, CME_ERROR_NONE);
}

void telephony_subscriber_number_req(void *telephony_device)
{
	DBG("telephony-tizen: subscriber number request");
	if (subscriber_number)
		telephony_subscriber_number_ind(subscriber_number,
						number_type(subscriber_number),
						SUBSCRIBER_SERVICE_VOICE);
	telephony_subscriber_number_rsp(telephony_device, CME_ERROR_NONE);
}

static char *get_supported_list(const char *list[], unsigned int size)
{
	GString *str;
	int i = 0;

	if (list == NULL || size == 0)
		return NULL;

	str = g_string_new("(");
	while (i < size) {
		if (i > 0)
			g_string_append(str, ",");

		g_string_append(str, list[i]);
		i++;
	}

	g_string_append(str, ")");

	return g_string_free(str, FALSE);
}

static int convert_utf8_gsm(uint8_t ascii, uint8_t utf_8, uint8_t *gsm)
{
	uint32_t i;

	if (ascii == 0xC3) {
		for (i = 0; i < GSM_UNI_MAX_C3 ; i++) {
			if (gsm_unicode_C3[i].utf_8 == utf_8) {
				*gsm = gsm_unicode_C3[i].gsm;
				return 0;
			}
		}
	} else if (ascii == 0xCE) {
		for (i = 0; i < GSM_UNI_MAX_CE ; i++) {
			if (gsm_unicode_CE[i].utf_8 == utf_8) {
				*gsm = gsm_unicode_CE[i].gsm;
				return 0;
			}
		}
	}

	return 1;
}

static void get_unicode_string(const char *name, char *unicodename)
{
	if (NULL != name && NULL != unicodename) {
		int len = strlen(name);
		int x, y;

		if (len == 0)
			return;

		if (len > PHONEBOOK_MAX_CHARACTER_LENGTH)
			len = PHONEBOOK_MAX_CHARACTER_LENGTH;

		for (x = 0, y = 0 ; x < len ; x++, y++) {
			if (x < (len - 1)) {
				if (convert_utf8_gsm(name[x], name[x+1],
						(uint8_t *)&unicodename[y])) {
					x++;
					continue;
				}
			}

			if (name[x] == '_') {
				 unicodename[y] = ' ';
				 continue;
			}

			unicodename[y] = name[x];
		}
	}
	return;
}

static int get_phonebook_count(const char *path, uint32_t *max_size,
				uint32_t *used)
{
	DBusConnection *conn;
	DBusMessage *message, *reply;
	DBusError error;

	uint32_t max = 0;
	uint32_t size = 0;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		DBG("Can't get on system bus");
		return -1;
	}

	message = dbus_message_new_method_call(PHONEBOOK_BUS_NAME,
					PHONEBOOK_PATH,
					PHONEBOOK_INTERFACE,
					"GetPhonebookSizeAt");
	if (!message) {
		DBG("Can't allocate new message");
		dbus_connection_unref(conn);
		return -1;
	}

	dbus_message_append_args(message, DBUS_TYPE_STRING, &path,
				DBUS_TYPE_INVALID);

	dbus_error_init(&error);

	reply = dbus_connection_send_with_reply_and_block(conn,
			message, -1, &error);

	if (!reply) {
		if (dbus_error_is_set(&error) == TRUE) {
			DBG("%s", error.message);
			dbus_error_free(&error);
		} else {
			DBG("Failed to get contacts");
		}
		dbus_message_unref(message);
		dbus_connection_unref(conn);
		return -1;
	}

	if (!dbus_message_get_args(reply, &error,
				DBUS_TYPE_UINT32, &size,
				DBUS_TYPE_INVALID)) {
		DBG("Can't get reply arguments\n");
		if (dbus_error_is_set(&error)) {
			DBG("%s\n", error.message);
			dbus_error_free(&error);
		}
		dbus_message_unref(reply);
		dbus_message_unref(message);
		dbus_connection_unref(conn);
		return -1;
	}

	if ((g_strcmp0(path, "\"SM\"") == 0) ||
			(g_strcmp0(path, "\"ME\"") == 0)) {
		max = PHONEBOOK_COUNT_MAX;
	}
	if ((g_strcmp0(path, "\"DC\"") == 0) ||
			(g_strcmp0(path, "\"MC\"") == 0) ||
			(g_strcmp0(path, "\"RC\"") == 0)) {
		max = CALL_LOG_COUNT_MAX;
	}

	if (max_size)
		*max_size = max;

	if (used) {
		if (size > max)
			*used = max;
		else
			*used = size;
	}

	dbus_message_unref(reply);
	dbus_message_unref(message);
	dbus_connection_unref(conn);

	return 0;
}

static int read_phonebook_entries(int start_index, int end_index)
{
	DBusConnection *conn;
	DBusMessage *message, *reply;
	DBusMessageIter iter, iter_struct;
	DBusError error;

	int count = 0;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		DBG("Can't get on system bus");
		return -1;
	}

	message = dbus_message_new_method_call(PHONEBOOK_BUS_NAME,
					PHONEBOOK_PATH,
					PHONEBOOK_INTERFACE,
					"GetPhonebookEntriesAt");
	if (!message) {
		DBG("Can't allocate new message");
		dbus_connection_unref(conn);
		return -1;
	}

	dbus_message_append_args(message,
				DBUS_TYPE_STRING,
				&phonebook_store_list[ag_pb_info.path_id],
				DBUS_TYPE_INT32, &start_index,
				DBUS_TYPE_INT32, &end_index,
				DBUS_TYPE_INVALID);

	dbus_error_init(&error);

	reply = dbus_connection_send_with_reply_and_block(conn,
			message, -1, &error);

	if (!reply) {
		if (dbus_error_is_set(&error) == TRUE) {
			DBG("%s", error.message);
			dbus_error_free(&error);
		} else {
			DBG("Failed to get contacts");
		}

		dbus_message_unref(message);
		dbus_connection_unref(conn);

		return -1;
	}

	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &iter_struct);

	while(dbus_message_iter_get_arg_type(&iter_struct) ==
			DBUS_TYPE_STRUCT) {
		const char *name = NULL;
		const char *number = NULL;

		char *uni_name;
		char *uni_number;

		uint32_t handle = 0;

		DBusMessageIter entry_iter;

		dbus_message_iter_recurse(&iter_struct,&entry_iter);

		dbus_message_iter_get_basic(&entry_iter, &name);
		dbus_message_iter_next(&entry_iter);
		dbus_message_iter_get_basic(&entry_iter, &number);
		dbus_message_iter_next(&entry_iter);
		dbus_message_iter_get_basic(&entry_iter, &handle);
		dbus_message_iter_next(&entry_iter);

		dbus_message_iter_next(&iter_struct);

		uni_name = g_strndup(name, PHONEBOOK_NAME_MAX_LENGTH);
		uni_number = g_strndup(number, PHONEBOOK_NAME_MAX_LENGTH);

		telephony_read_phonebook_entries_ind(uni_name,
				uni_number, handle);

		count++;

		g_free(uni_name);
		g_free(uni_number);
	}

	dbus_message_unref(message);
	dbus_message_unref(reply);
	dbus_connection_unref(conn);

	return count;
}

static int find_phonebook_entries(const char *str)
{
	DBusConnection *conn;
	DBusMessage *message, *reply;
	DBusMessageIter iter, iter_struct;
	DBusError error;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		DBG("Can't get on system bus");
		return -1;
	}

	message = dbus_message_new_method_call(PHONEBOOK_BUS_NAME,
					PHONEBOOK_PATH,
					PHONEBOOK_INTERFACE,
					"GetPhonebookEntriesFindAt");
	if (!message) {
		DBG("Can't allocate new message");
		dbus_connection_unref(conn);
		return -1;
	}

	dbus_message_append_args(message,
				DBUS_TYPE_STRING,
				&phonebook_store_list[ag_pb_info.path_id],
				DBUS_TYPE_STRING, &str,
				DBUS_TYPE_INVALID);

	dbus_error_init(&error);

	reply = dbus_connection_send_with_reply_and_block(conn,
			message, -1, &error);

	if (!reply) {
		if (dbus_error_is_set(&error) == TRUE) {
			DBG("%s", error.message);
			dbus_error_free(&error);
		} else {
			DBG("Failed to get contacts");
		}

		dbus_message_unref(message);
		dbus_connection_unref(conn);

		return -1;
	}

	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &iter_struct);

	while(dbus_message_iter_get_arg_type(&iter_struct) ==
			DBUS_TYPE_STRUCT) {
		const char *name = NULL;
		const char *number = NULL;

		char *uni_name;
		char *uni_number;

		uint32_t handle = 0;

		DBusMessageIter entry_iter;

		dbus_message_iter_recurse(&iter_struct,&entry_iter);

		dbus_message_iter_get_basic(&entry_iter, &name);
		dbus_message_iter_next(&entry_iter);
		dbus_message_iter_get_basic(&entry_iter, &number);
		dbus_message_iter_next(&entry_iter);
		dbus_message_iter_get_basic(&entry_iter, &handle);
		dbus_message_iter_next(&entry_iter);

		dbus_message_iter_next(&iter_struct);

		uni_name = g_strndup(name, PHONEBOOK_NAME_MAX_LENGTH);
		uni_number = g_strndup(number, PHONEBOOK_NAME_MAX_LENGTH);

		telephony_find_phonebook_entries_ind(uni_name, uni_number, handle);

		g_free(uni_name);
		g_free(uni_number);
	}

	dbus_message_unref(message);
	dbus_message_unref(reply);
	dbus_connection_unref(conn);

	return 0;
}

void telephony_select_phonebook_memory_status(void *telephony_device)
{
	int32_t path_id = ag_pb_info.path_id;
	uint32_t used;
	uint32_t max_size;

	cme_error_t err = CME_ERROR_NONE;


	DBG("telephony-tizen: telephony_read_phonebook_store\n");

	if (path_id < 0 || path_id >= PHONEBOOK_STORE_LIST_SIZE)
		path_id = 0;

	if (get_phonebook_count(phonebook_store_list[path_id],
				&max_size, &used))
		err = CME_ERROR_AG_FAILURE;

	telephony_select_phonebook_memory_status_rsp(telephony_device,
			phonebook_store_list[path_id],
			max_size, used,
			err);
}

void telephony_select_phonebook_memory_list(void *telephony_device)
{
/*
	For Blue & Me car kit we may have to add  the
	patch here(similar to the patch done in H1 and H2 )
*/
	char *str;

	str = get_supported_list(phonebook_store_list,
			PHONEBOOK_STORE_LIST_SIZE);

	DBG("telephony-tizen: telephony_select_phonebook_memory_list %d :%s\n",
			PHONEBOOK_STORE_LIST_SIZE, str);

	telephony_select_phonebook_memory_list_rsp(telephony_device,
			str, CME_ERROR_NONE);

	g_free(str);
}

void telephony_select_phonebook_memory(void *telephony_device, const gchar *path)
{

	int i = 0;
	cme_error_t err;

	DBG("telephony-tizen: telephony_select_phonebook_memory\n");
	DBG("set phonebook type to [%s]\n", path);

	while (i < PHONEBOOK_STORE_LIST_SIZE) {
		if (strcmp(phonebook_store_list[i], path) == 0)
			break;

		i++;
	}

	if  (i >= 0 && i < PHONEBOOK_STORE_LIST_SIZE) {
		err = CME_ERROR_NONE;
		ag_pb_info.path_id = i;
	} else {
		err = CME_ERROR_INVALID_TEXT_STRING;
	}
	telephony_select_phonebook_memory_rsp(telephony_device, err);
}

void telephony_read_phonebook_entries_list(void *telephony_device)
{
	cme_error_t err = CME_ERROR_NONE;

	int32_t path_id = ag_pb_info.path_id;
	uint32_t used;

	DBG("telephony-tizen: telephony_read_phonebook_entries_list\n");

	if (path_id < 0 || path_id >= PHONEBOOK_STORE_LIST_SIZE)
		err = CME_ERROR_INVALID_INDEX;
	else {
		if (get_phonebook_count(phonebook_store_list[path_id],
					NULL, &used) != 0) {
			err = CME_ERROR_NOT_ALLOWED;
		}
	}

	telephony_read_phonebook_entries_list_rsp(telephony_device, used,
			PHONEBOOK_NUMBER_MAX_LENGTH, PHONEBOOK_NAME_MAX_LENGTH,
			err);
}

void telephony_read_phonebook_entries(void *telephony_device, const char *cmd)
{
	int start_index = 0;
	int end_index = 0;

	int count;

	char *str = NULL;
	char *next = NULL;

	cme_error_t err;

	DBG("telephony-tizen: telephony_read_phonebook_entries\n");

	if (cmd == NULL)
		return;

	str = g_strdup(cmd);
	next = strchr(str, ',');

	if (next) {
		*next = '\0';
		next++;

		end_index = strtol(next, NULL, 10);
	}

	start_index = strtol(str, NULL, 10);

	g_free(str);

	count = read_phonebook_entries(start_index, end_index);

	if (count < 0)
		err = CME_ERROR_AG_FAILURE;
	else if (count == 0)
		err = CME_ERROR_INVALID_INDEX;
	else
		err = CME_ERROR_NONE;

	telephony_read_phonebook_entries_rsp(telephony_device, err);
}

void telephony_find_phonebook_entries_status(void *telephony_device)
{
	telephony_find_phonebook_entries_status_ind(
			PHONEBOOK_NUMBER_MAX_LENGTH,
			PHONEBOOK_NAME_MAX_LENGTH);

	telephony_find_phonebook_entries_status_rsp(telephony_device,
						CME_ERROR_NONE);
}

void telephony_find_phonebook_entries(void *telephony_device, const char *cmd)
{
	gchar *st = NULL;
	gchar *unquoted = NULL;

	cme_error_t err = CME_ERROR_NONE;

	DBG("telephony-tizen: telephony_find_phonebook_entry\n");

	/* remove quote and compress */
	st = strchr(cmd, '"');
	if (st == NULL)
		unquoted = g_strdup(cmd);
	else {
		gchar *end = NULL;

		end = strrchr(cmd, '"');
		if(end == NULL)
			unquoted = g_strdup(cmd);
		else
			unquoted = g_strndup(st + 1, end - st - 1);
	}

	if (find_phonebook_entries(unquoted))
		err = CME_ERROR_AG_FAILURE;

	telephony_find_phonebook_entries_rsp(telephony_device, err);

	g_free(unquoted);
}

void telephony_get_preffered_store_capacity(void *telephony_device)
{
	DBG("telephony-tizen: telephony_list_preffered_store_capcity\n");

	telephony_get_preffered_store_capacity_rsp(telephony_device,
			PREFFERED_MESSAGE_STORAGE_MAX,
			CME_ERROR_NONE);
}

void telephony_list_preffered_store(void *telephony_device)
{
	DBG("telephony-tizen: telephony_list_preffered_store_capcity\n");

	telephony_list_preffered_store_rsp(telephony_device,
			PREFFERED_MESSAGE_STORAGE_LIST,
			CME_ERROR_NONE);
}

/*
void telephony_set_preffered_store_capcity(void *telephony_device, const char *cmd)
{
}
*/

void telephony_get_character_set(void *telephony_device)
{
	DBG("telephony-tizen: telephony_get_character_set\n");

	telephony_supported_character_generic_rsp(telephony_device,
			(char *)character_set_list[ag_pb_info.charset_id],
			CME_ERROR_NONE);

}

void telephony_list_supported_character(void *telephony_device)
{
	char *str;

	str = get_supported_list(character_set_list,
			CHARACTER_SET_LIST_SIZE);

	DBG("telephony-tizen: telephony_list_supported_character_set %d :%s\n",
			CHARACTER_SET_LIST_SIZE, str);

	telephony_supported_character_generic_rsp(telephony_device,
			str, CME_ERROR_NONE);

	g_free(str);
}

void telephony_set_characterset(void *telephony_device, const char *cmd)
{
	DBG("telephony-tizen: telephony_set_characterset [%s]\n", cmd);

	int i = 0;

	while (i < CHARACTER_SET_LIST_SIZE) {
		if (strcmp(character_set_list[i], cmd) == 0) {
			telephony_set_characterset_generic_rsp(telephony_device,
					CME_ERROR_NONE);
			ag_pb_info.charset_id = i;
			return;
		}

		i++;
	}

	telephony_set_characterset_generic_rsp(telephony_device,
			CME_ERROR_NOT_SUPPORTED);
	return;

}

static void telephony_get_battery_property_reply(
			DBusPendingCall *call, void *data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	DBusError derr;
	int32_t bcs = 0;
	int32_t bcl = 0;

	DBG("battery_property_reply");

	dbus_error_init(&derr);
	if (dbus_set_error_from_message(&derr, reply)) {
		DBG("battery_property_reply: %s", derr.message);
		dbus_error_free(&derr);
		telephony_battery_charge_status_rsp(data, bcs,
			bcl, CME_ERROR_AG_FAILURE);
		goto done;
	}

	if (dbus_message_get_args(reply, NULL,
			DBUS_TYPE_INT32, &bcs,
			DBUS_TYPE_INT32, &bcl,
			DBUS_TYPE_INVALID) == FALSE) {
		DBG("get_signal_quality_reply: Invalid arguments");
		telephony_battery_charge_status_rsp(data, bcs,
			bcl, CME_ERROR_AG_FAILURE);
		goto done;

	}

	telephony_battery_charge_status_rsp(data, bcs,
		bcl, CME_ERROR_NONE);

done:
	dbus_message_unref(reply);
}

void telephony_get_battery_property(void *telephony_device)
{
	DBG("telephony-tizen: telephony_get_battery_property\n");

	if (0 != dbus_method_call_send(HFP_AGENT_SERVICE, HFP_AGENT_PATH,
				HFP_AGENT_INTERFACE, "GetBatteryStatus",
				telephony_get_battery_property_reply,
				telephony_device, DBUS_TYPE_INVALID)) {
		telephony_battery_charge_status_rsp(telephony_device, 0, 0,
						CME_ERROR_AG_FAILURE);
	}
}

static void telephony_get_signal_quality_reply(DBusPendingCall *call,
			void *data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	DBusError derr;
	int32_t rssi = 0;
	int32_t ber = 0;

	DBG("get_signal_quality_reply");

	dbus_error_init(&derr);
	if (dbus_set_error_from_message(&derr, reply)) {
		DBG("get_signal_quality_reply: %s", derr.message);
		dbus_error_free(&derr);
		telephony_signal_quality_rsp(data, rssi,
			ber, CME_ERROR_AG_FAILURE);
		goto done;
	}

	if (dbus_message_get_args(reply, NULL,
			DBUS_TYPE_INT32, &rssi,
			DBUS_TYPE_INT32, &ber,
			DBUS_TYPE_INVALID) == FALSE) {
		DBG("get_signal_quality_reply: Invalid arguments");
		telephony_signal_quality_rsp(data, rssi,
			ber, CME_ERROR_AG_FAILURE);
		goto done;

	}

	telephony_signal_quality_rsp(data, rssi,
		ber, CME_ERROR_NONE);

done:
	dbus_message_unref(reply);
}

void telephony_get_signal_quality(void *telephony_device)
{
	DBG("telephony-tizen: telephony_get_signal_quality\n");

	if (0 != dbus_method_call_send(HFP_AGENT_SERVICE, HFP_AGENT_PATH,
				HFP_AGENT_INTERFACE, "GetSignalQuality",
				telephony_get_signal_quality_reply,
				telephony_device, DBUS_TYPE_INVALID)) {
		telephony_signal_quality_rsp(telephony_device, 0, 0,
						CME_ERROR_AG_FAILURE);
	}

}

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
#define CSD_CALL_CONFERENCE	"org.tizen.csd.Call.Conference"
#define CSD_CALL_PATH	"/org/tizen/csd/call"
#define CSD_CALL_CONFERENCE_PATH "/org/tizen/csd/call/conference"
#define CSD_CALL_SENDER_PATH	"/org/tizen/csd/call/sender"

/* libcsnet D-Bus definitions */
#define CSD_CSNET_BUS_NAME	"org.tizen.csd.CSNet"
#define CSD_CSNET_PATH	"/org/tizen/csd/csnet"
#define CSD_CSNET_IFACE	"org.tizen.csd.CSNet"
#define CSD_CSNET_REGISTRATION	"org.tizen.csd.CSNet.NetworkRegistration"
#define CSD_CSNET_OPERATOR	"org.tizen.csd.CSNet.NetworkOperator"
#define CSD_CSNET_SIGNAL	"org.tizen.csd.CSNet.SignalStrength"
#define CSD_TELEPHONE_BATTERY	"org.tizen.csd.CSNet.BatteryStrength"
#define CSD_CSNET_SUBSCRIBER	"org.tizen.csd.CSNet.SubscriberNumber"


#define CALL_FLAG_NONE	0
#define CALL_FLAG_PRESENTATION_ALLOWED		0x01
#define CALL_FLAG_PRESENTATION_RESTRICTED	0x02

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

#define CALL_FLAG_NONE				0
#define CALL_FLAG_PRESENTATION_ALLOWED		0x01
#define CALL_FLAG_PRESENTATION_RESTRICTED	0x02


#define DBUS_STRUCT_STRING_STRING_UINT (dbus_g_type_get_struct ("GValueArray",\
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_INVALID))

#define PHONEBOOK_STORE_LIST "(\"SM\",\"ME\",\"DC\",\"MC\",\"RC\")"
#define PHONEBOOK_STORE_LIST_BLUENME "(\"ME\",\"DC\",\"MC\",\"RC\")"
#define PREFFERED_MESSAGE_STORAGE_LIST "(\"ME\",\"MT\",\"SM\",\"SR\"),(\"ME\",\"MT\",\"SM\",\"SR\"),(\"ME\",\"MT\",\"SM\",\"SR\")"
#define PHONEBOOK_CHARACTER_SET_LIST "(\"IRA\",\"GSM\",\"UCS2\")"
#define PHONEBOOK_CHARACTER_SET_SUPPORTED "\"GSM\""

#define PREFFERED_MESSAGE_STORAGE_MAX 500
#define CALL_LOG_COUNT_MAX 30
#define PHONEBOOK_COUNT_MAX 1000

#define PHONEBOOK_PATH_LENGTH 5
#define PHONEBOOK_NAME_MAX_LENGTH 20
#define PHONEBOOK_NUMBER_MAX_LENGTH 20
#define PHONEBOOK_MAX_CHARACTER_LENGTH	20

#define PHONEBOOK_READ_RESP_LENGTH 20

typedef struct {
	uint8_t utf_8;
	uint8_t	gsm;
} GsmUnicodeTable;


 /*0xC3 charcterset*/
const GsmUnicodeTable gsm_unicode_C3[] = {
	{0xA8,0x04},{0xA9,0x05},{0xB9,0x06},{0xAC,0x07},{0xB2,0x08},
	{0xB7,0x09},{0x98,0x0B},{0xB8,0x0C},{0x85,0x0E},{0xA5,0x0F},
	{0x86,0x1C},{0xA6,0x1D},{0x9F,0x1E},{0x89,0x1F},{0x84,0x5B},
	{0x96,0x5C},{0x91,0x5D},{0x9C,0x5E},{0x80,0x5F},{0xA4,0x7B},
	{0xB6,0x7C},{0xB1,0x7D},{0xBC,0x7E},{0xA0,0x7F},
};

/*0xCE charcterset*/
const GsmUnicodeTable gsm_unicode_CE[] = {
	{0x85,0x14},{0xA1,0x50},{0x98,0x19},{0xA0,0x16},{0x94,0x10},
	{0xA6,0x12},{0x93,0x13},{0x9E,0x1A},{0x9B,0x14},{0xA8,0x17},
	{0xA9,0x15},
};

#define GSM_UNI_MAX_C3	(sizeof(gsm_unicode_C3)/sizeof(GsmUnicodeTable))
#define GSM_UNI_MAX_CE	(sizeof(gsm_unicode_CE)/sizeof(GsmUnicodeTable))


static DBusConnection *ag_connection = NULL;

static GSList *calls = NULL;
static GSList *watches = NULL;

static gboolean events_enabled = FALSE;
static uint32_t callerid = 0;

/* Reference count for determining the call indicator status */
static GSList *active_calls = NULL;

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
	char *object_path;
	int status;
	gboolean originating;
	gboolean emergency;
	gboolean on_hold;
	gboolean conference;
	char *number;
	gboolean setup;
	uint32_t call_id;
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
	char path[PHONEBOOK_PATH_LENGTH];
	uint32_t type;
	uint32_t max_size;
	uint32_t used;
} ag_pb_info = {
	.path = {0,},
	.type = 0,
	.max_size = 0,
	.used =0,
};


static struct csd_call *find_call(uint32_t call_id)

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

static void foreach_call_with_status(int status,
					int (*func)(struct csd_call *call))
{
	GSList *l;

	for (l = calls; l != NULL; l = l->next) {
		struct csd_call *call = l->data;

		if (call->status == status)
			func(call);
	}
}

static void csd_call_free(struct csd_call *call)
{
	if (!call)
		return;

	g_free(call->object_path);
	g_free(call->number);

	g_free(call);
}

static int release_conference(void)
{
	DBusMessage *msg;

	DBG("+\n");

	msg = dbus_message_new_method_call(CSD_CALL_BUS_NAME,
						CSD_CALL_CONFERENCE_PATH,
						CSD_CALL_INSTANCE,
						"Release");
	if (!msg) {
		error("Unable to allocate new D-Bus message");
		return -ENOMEM;
	}

	g_dbus_send_message(ag_connection, msg);

	DBG("-\n");

	return 0;
}

static int reject_call(struct csd_call *call)
{
	DBusMessage *msg;

	DBG("+\n");

	msg = dbus_message_new_method_call(CSD_CALL_BUS_NAME,
						call->object_path,
						CSD_CALL_INSTANCE,
						"Reject");
	if (!msg) {
		error("Unable to allocate new D-Bus message");
		return -ENOMEM;
	}
	DBG(" Object Path =[ %s] and Call id = [%d]\n", call->object_path, call->call_id);
	if (!dbus_message_append_args(msg, DBUS_TYPE_UINT32, &call->call_id,
			DBUS_TYPE_INVALID)) {

		DBG("dbus_message_append_args -ERROR\n");
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

	msg = dbus_message_new_method_call(CSD_CALL_BUS_NAME,
						call->object_path,
						CSD_CALL_INSTANCE,
						"Release");
	if (!msg) {
		error("Unable to allocate new D-Bus message");
		return -ENOMEM;
	}
	DBG(" Object Path =[ %s] and Call id = [%d]\n", call->object_path, call->call_id);
	if (!dbus_message_append_args(msg, DBUS_TYPE_UINT32, &call->call_id,
			DBUS_TYPE_INVALID)) {

		DBG("dbus_message_append_args -ERROR\n");
		dbus_message_unref(msg);
		return -ENOMEM;
	}

	g_dbus_send_message(ag_connection, msg);

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

	msg = dbus_message_new_method_call(CSD_CALL_BUS_NAME,
						call->object_path,
						CSD_CALL_INSTANCE,
						"Answer");
	if (!msg) {
		error("Unable to allocate new D-Bus message");
		return -ENOMEM;
	}

	if (!dbus_message_append_args(msg, DBUS_TYPE_UINT32, &call->call_id,
			DBUS_TYPE_INVALID)) {

		DBG("dbus_message_append_args -ERROR\n");
		dbus_message_unref(msg);
		return -ENOMEM;
	}
	g_dbus_send_message(ag_connection, msg);
	DBG("-\n");
	return 0;
}

static int number_type(const char *number)
{
	if (number == NULL)
		return NUMBER_TYPE_TELEPHONY;

	if (number[0] == '+' || strncmp(number, "00", 2) == 0)
		return NUMBER_TYPE_INTERNATIONAL;

	return NUMBER_TYPE_TELEPHONY;
}

static void call_set_status(struct csd_call *call, dbus_uint32_t status)
{
	dbus_uint32_t prev_status;
	int callheld = 0;

	DBG("+\n");

	callheld = telephony_get_indicator(telephony_ag_indicators, "callheld");

	prev_status = call->status;
	DBG("Call %s Call id %d changed from %s to %s", call->object_path, call->call_id,
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
	DBG("-\n");
}

/**
 * This API shall invoke a dbus method call to bluetooth framework to split the call
 *
 * @return		This function returns zero on success.
 * @param[in]		call	Pointer to the Call information structure.
 * @param[out]	 	NONE.
 */
static int split_call(struct csd_call *call)
{
	DBusMessage *msg;

	DBG("+n");

	if (NULL != call) {
		msg = dbus_message_new_method_call(CSD_CALL_BUS_NAME,
							call->object_path,
							CSD_CALL_INSTANCE,
							"Split");
		if (!msg) {
			error("Unable to allocate new D-Bus message");
			return -ENOMEM;
		}

		g_dbus_send_message(ag_connection, msg);
	}
	DBG("-\n");

	return 0;
}
/**
 * This API shall invoke a dbus method call to bluetooth framework to swap the calls
 *
 * @return		This function returns zero on success.
 * @param[in]		NONE.
 * @param[out]	 	NONE.
 */
static int swap_calls(void)
{
	DBusMessage *msg;
	DBG("+\n");

	msg = dbus_message_new_method_call(CSD_CALL_BUS_NAME, CSD_CALL_PATH,
						CSD_CALL_INTERFACE,
						"Swap");
	if (!msg) {
		error("Unable to allocate new D-Bus message");
		return -ENOMEM;
	}

	g_dbus_send_message(ag_connection, msg);
	DBG("-\n");

	return 0;
}

/**
 * This API shall invoke a dbus method call to bluetooth framework to hold the call
 *
 * @return		This function returns zero on success.
 * @param[in]		call	Pointer to the Call information structure.
 * @param[out]	 	NONE.
 */
static int hold_call(struct csd_call *call)
{
	DBusMessage *msg;
	DBG("+\n");

	msg = dbus_message_new_method_call(CSD_CALL_BUS_NAME, CSD_CALL_PATH,
						CSD_CALL_INTERFACE,
						"Hold");
	if (!msg) {
		error("Unable to allocate new D-Bus message");
		return -ENOMEM;
	}
	if (NULL != call) {
		DBG(" Object Path =[ %s] and Call id = [%d]\n", call->object_path, call->call_id);

		if (!dbus_message_append_args(msg, DBUS_TYPE_UINT32, &call->call_id,
				DBUS_TYPE_INVALID)) {

			DBG("dbus_message_append_args -ERROR\n");
			dbus_message_unref(msg);
			return -ENOMEM;
		}

		g_dbus_send_message(ag_connection, msg);
	}
	DBG("-\n");

	return 0;
}


/**
 * This API shall invoke a dbus method call to bluetooth framework to unhold the call
 *
 * @return		This function returns zero on success.
 * @param[in]		call	Pointer to the Call information structure.
 * @param[out]	 	NONE.
 */
static int unhold_call(struct csd_call *call)
{
	DBusMessage *msg;
	DBG("+\n");

	msg = dbus_message_new_method_call(CSD_CALL_BUS_NAME, CSD_CALL_PATH,
						CSD_CALL_INTERFACE,
						"Unhold");
	if (!msg) {
		error("Unable to allocate new D-Bus message");
		return -ENOMEM;
	}

	if (NULL != call) {
		DBG(" Object Path =[ %s] and Call id = [%d]\n", call->object_path, call->call_id);

		if (!dbus_message_append_args(msg, DBUS_TYPE_UINT32, &call->call_id,
				DBUS_TYPE_INVALID)) {

			DBG("dbus_message_append_args -ERROR\n");
			dbus_message_unref(msg);
			return -ENOMEM;
		}

		g_dbus_send_message(ag_connection, msg);
	}
	DBG("-\n");

	return 0;
}

/**
 * This API shall invoke a dbus method call to bluetooth framework to create conmference calls
 *
 * @return		This function returns zero on success.
 * @param[in]		NONE.
 * @param[out]	 	NONE.
 */
static int create_conference(void)
{
	DBusMessage *msg;
	DBG("+\n");

	msg = dbus_message_new_method_call(CSD_CALL_BUS_NAME, CSD_CALL_PATH,
						CSD_CALL_INTERFACE,
						"Conference");
	if (!msg) {
		error("Unable to allocate new D-Bus message");
		return -ENOMEM;
	}

	g_dbus_send_message(ag_connection, msg);
	DBG("-\n");

	return 0;
}
/**
 * This API shall invoke a dbus method call to bluetooth framework to transfer the call
 *
 * @return		This function returns zero on success.
 * @param[in]		NONE.
 * @param[out]	 	NONE.
 */
static int call_transfer(void)
{
	DBusMessage *msg;
	DBG("+\n");

	msg = dbus_message_new_method_call(CSD_CALL_BUS_NAME, CSD_CALL_PATH,
						CSD_CALL_INTERFACE,
						"Transfer");
	if (!msg) {
		error("Unable to allocate new D-Bus message");
		return -ENOMEM;
	}

	g_dbus_send_message(ag_connection, msg);
	DBG("-\n");

	return 0;
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
	int chld_value = 0;

	DBG("+\n");

	if (strlen(cmd) > 1)
		idx = &cmd[1];
	else
		idx = NULL;

	if (idx)
		call = g_slist_nth_data(calls, strtol(idx, NULL, 0) - 1);
	else
		call = NULL;
#if 0
	switch (cmd[0]) {
	case '0':
		if (find_call_with_status(CSD_CALL_STATUS_WAITING))
			foreach_call_with_status(CSD_CALL_STATUS_WAITING,
								release_call);
		else
			foreach_call_with_status(CSD_CALL_STATUS_HOLD,
								release_call);
		break;
	case '1':
		if (idx) {
			if (call)
				err = release_call(call);
			break;
		}
		foreach_call_with_status(CSD_CALL_STATUS_ACTIVE, release_call);
		call = find_call_with_status(CSD_CALL_STATUS_WAITING);
		if (call) {
			err = answer_call(call);
		}
		else {
			struct csd_call *held;
			held = find_call_with_status(CSD_CALL_STATUS_HOLD);
			if(held)
				err = unhold_call(held);
		}
		break;
	case '2':
		if (idx) {
			if (call)
				err = split_call(call);
		} else {
			struct csd_call *held, *wait;

			call = find_call_with_status(CSD_CALL_STATUS_ACTIVE);
			held = find_call_with_status(CSD_CALL_STATUS_HOLD);
			wait = find_call_with_status(CSD_CALL_STATUS_WAITING);

			if (wait)
				err = answer_call(wait);
			else if (call && held)
				err = swap_calls();
			else {
				if (call)
					err = hold_call(call);
				if (held)
					err = unhold_call(held);
			}
		}
		break;
	case '3':
		if (find_call_with_status(CSD_CALL_STATUS_HOLD) ||
				find_call_with_status(CSD_CALL_STATUS_WAITING))
			err = create_conference();
		break;
	case '4':
		err = call_transfer();
		break;
	default:
		DBG("Unknown call hold request");
		break;
	}

	if (err)
		telephony_call_hold_rsp(telephony_device,
					CME_ERROR_AG_FAILURE);
	else
		telephony_call_hold_rsp(telephony_device, CME_ERROR_NONE);
#else
	idx = &cmd[0];
	chld_value = strtol(idx, NULL, 0);

	err = dbus_method_call_send(CSD_CALL_BUS_NAME, CSD_CALL_PATH,
					CSD_CALL_INSTANCE, "Threeway",
					telephony_chld_reply, telephony_device,
					DBUS_TYPE_INT32, &chld_value,
					DBUS_TYPE_INVALID);

	if (err)
		telephony_call_hold_rsp(telephony_device,
					CME_ERROR_AG_FAILURE);
#endif
	DBG("-\n");
}

static void handle_incoming_call(DBusMessage *msg)
{

	const char *number, *call_path;
	struct csd_call *call;
	uint32_t call_id;

	DBG("+\n");
	DBG("handle_incoming_call()\n");

	if (!dbus_message_get_args(msg, NULL,
					DBUS_TYPE_OBJECT_PATH, &call_path,
					DBUS_TYPE_STRING, &number,
					DBUS_TYPE_UINT32, &call_id,
					DBUS_TYPE_INVALID)) {
		error("Unexpected parameters in Call.Coming() signal");
		return;
	}

	call = g_new0(struct csd_call, 1);
	call->object_path = g_strdup(call_path);
	call->call_id = call_id;
	call->number = g_strdup(number);
	calls = g_slist_append(calls, call);

	DBG("Incoming call to %s from number %s call id %d", call_path, number, call_id);

	if (find_call_with_status(CSD_CALL_STATUS_ACTIVE) ||
			find_call_with_status(CSD_CALL_STATUS_HOLD)) {
		telephony_call_waiting_ind(call->number,
						number_type(call->number));
		call_set_status(call, CSD_CALL_STATUS_WAITING);
	} else {
		telephony_incoming_call_ind(call->number,
						number_type(call->number));

		call_set_status(call, CSD_CALL_STATUS_COMING);
	}
	telephony_update_indicator(telephony_ag_indicators, "callsetup",
					EV_CALLSETUP_INCOMING);
	DBG("-\n");
}

static void update_registration_status(uint8_t status)
{
	uint8_t new_status;
	DBG("+\n");

	new_status = status;

	if (net.status == new_status)
		return;

	switch (new_status) {
	case NETWORK_REG_STATUS_HOME:
		telephony_update_indicator(telephony_ag_indicators, "roam",
							EV_ROAM_INACTIVE);
		if (net.status > NETWORK_REG_STATUS_ROAMING)
			telephony_update_indicator(telephony_ag_indicators,
							"service",
							EV_SERVICE_PRESENT);
		break;
	case NETWORK_REG_STATUS_ROAMING:
		telephony_update_indicator(telephony_ag_indicators, "roam",
							EV_ROAM_ACTIVE);
		if (net.status > NETWORK_REG_STATUS_ROAMING)
			telephony_update_indicator(telephony_ag_indicators,
							"service",
							EV_SERVICE_PRESENT);
		break;
	case NETWORK_REG_STATUS_OFFLINE:
	case NETWORK_REG_STATUS_SEARCHING:
	case NETWORK_REG_STATUS_NO_SIM:
	case NETWORK_REG_STATUS_POWEROFF:
	case NETWORK_REG_STATUS_POWERSAFE:
	case NETWORK_REG_STATUS_NO_COVERAGE:
	case NETWORK_REG_STATUS_REJECTED:
	case NETWORK_REG_STATUS_UNKOWN:
		if (net.status < NETWORK_REG_STATUS_OFFLINE)
			telephony_update_indicator(telephony_ag_indicators,
							"service",
							EV_SERVICE_NONE);
		break;
	}

	net.status = new_status;

	DBG("-\n");
}

static void update_signal_strength(int32_t signal_bars)
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
		return;

	telephony_update_indicator(telephony_ag_indicators, "signal", signal_bars);

	net.signal_bars = signal_bars;

	DBG("-\n");
}

static void update_battery_strength(int32_t battery_level)
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
		return;

	telephony_update_indicator(telephony_ag_indicators,
			"battchg", battery_level);


	DBG("-\n");
}


static void handle_outgoing_call(DBusMessage *msg)
{
	const char *number, *call_path;
	struct csd_call *call;
	uint32_t call_id;

	DBG("+\n");
	DBG("handle_outgoing_call()\n");

	if (!dbus_message_get_args(msg, NULL,
					DBUS_TYPE_OBJECT_PATH, &call_path,
					DBUS_TYPE_STRING, &number,
					DBUS_TYPE_UINT32, &call_id,
					DBUS_TYPE_INVALID)) {
		error("Unexpected parameters in Call.Coming() signal");
		return;
	}

	call = g_new0(struct csd_call, 1);
	call->object_path = g_strdup(call_path);
	call->call_id = call_id;
	call->number = g_strdup(number);
	calls = g_slist_append(calls, call);

	DBG("Outgoing call to %s from number %s call id %d", call_path, number, call_id);

	call_set_status(call, CSD_CALL_STATUS_CREATE);

	telephony_update_indicator(telephony_ag_indicators, "callsetup",
					EV_CALLSETUP_OUTGOING);
	DBG("-\n");
}

static void handle_create_requested(DBusMessage *msg)
{
	DBG("+\n");
	DBG("handle_create_requested()\n");
	DBG("-\n");
}

static void handle_call_status(DBusMessage *msg, const char *call_path)
{
	struct csd_call *call;
	dbus_uint32_t status, call_id;
	DBG("+\n");

	if (!dbus_message_get_args(msg, NULL,
					DBUS_TYPE_UINT32, &status,
					DBUS_TYPE_UINT32, &call_id,
					DBUS_TYPE_INVALID)) {
		error("Unexpected paramters in Instance.CallStatus() signal");
		return;
	}

	DBG("status = [%d] and call_id = [%d]\n", status, call_id);
	call = find_call(call_id);
	if (!call) {
/*
	call_path is equal to CSD_CALL_PATH then we should update the call list
	since the call_path is sent from native AG applicaton

	Added for updation of the call status if the call is not added inthe call list
*/
		if (g_str_equal(CSD_CALL_PATH, call_path)) {
			call = g_new0(struct csd_call, 1);
			call->object_path = g_strdup(call_path);
			call->call_id = call_id;
			calls = g_slist_append(calls, call);
		}
	}

	if (status > 16) {
		error("Invalid call status %u", status);
		return;
	}

	call_set_status(call, status);
	DBG("-\n");
}

static void update_operator_name(const char *name)
{
	DBG("+\n");
	if (name == NULL)
		return;

	g_free(net.operator_name);
	net.operator_name = g_strndup(name, 16);
	DBG("-\n");
}

static void update_subscriber_number(const char *number)
{
	DBG("+\n");
	if (number == NULL)
		return;

	g_free(subscriber_number);
	subscriber_number = g_strdup(number);
	DBG("-\n");
}

static void handle_conference(DBusMessage *msg, gboolean joined)
{
	DBG("+\n");
	DBG("handle_conference()\n");
	DBG("-\n");
}

static void handle_registration_changed(DBusMessage *msg)
{
	uint8_t status;

	DBG("+\n");
	DBG("handle_registration_changed()\n");

	if (!dbus_message_get_args(msg, NULL,
					DBUS_TYPE_BYTE, &status,
					DBUS_TYPE_INVALID)) {
		error("Unexpected parameters in RegistrationChanged");
		return;
	}

	update_registration_status((uint8_t) status);
	DBG("-\n");
}

static void handle_operator_name_changed(DBusMessage *msg)
{
	const char *name;

	DBG("+\n");

	if (!dbus_message_get_args(msg, NULL,
					DBUS_TYPE_STRING, &name,
					DBUS_TYPE_INVALID)) {
		error("Unexpected parameters in OperatorNameChanged");
		return;
	}

	update_operator_name(name);
	DBG("-\n");
}

static void handle_signal_bars_changed(DBusMessage *msg)
{
	int32_t signal_bars;

	DBG("+\n");

	if (!dbus_message_get_args(msg, NULL,
					DBUS_TYPE_INT32, &signal_bars,
					DBUS_TYPE_INVALID)) {
		error("Unexpected parameters in SignalBarsChanged");
		return;
	}

	update_signal_strength(signal_bars);
	DBG("-\n");
}

static void handle_battery_bars_changed(DBusMessage *msg)
{
	int32_t battery_level;

	DBG("+\n");

	if (!dbus_message_get_args(msg, NULL,
					DBUS_TYPE_INT32, &battery_level,
					DBUS_TYPE_INVALID)) {
		error("Unexpected parameters in SignalBarsChanged");
		return;
	}

	update_battery_strength(battery_level);
	DBG("-\n");
}

static void handle_subscriber_number_changed(DBusMessage *msg)
{
	const char *number;

	DBG("+\n");

	if (!dbus_message_get_args(msg, NULL,
					DBUS_TYPE_STRING, &number,
					DBUS_TYPE_INVALID)) {
		error("Unexpected parameters in SubscriberNumberChanged");
		return;
	}

	update_subscriber_number(number);
	DBG("-\n");
}
static gboolean signal_filter(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	const char *path = NULL;

	DBG("+\n");

	path = dbus_message_get_path(msg);

	if (dbus_message_is_signal(msg, CSD_CALL_INTERFACE, "Coming"))
		handle_incoming_call(msg);
	else if (dbus_message_is_signal(msg, CSD_CALL_INTERFACE, "Created"))
		handle_outgoing_call(msg);
	else if (dbus_message_is_signal(msg, CSD_CALL_INTERFACE,
							"CreateRequested"))
		handle_create_requested(msg);
	else if (dbus_message_is_signal(msg, CSD_CALL_INSTANCE, "CallStatus"))
		handle_call_status(msg, path);
	else if (dbus_message_is_signal(msg, CSD_CALL_CONFERENCE, "Joined"))
		handle_conference(msg, TRUE);
	else if (dbus_message_is_signal(msg, CSD_CALL_CONFERENCE, "Left"))
		handle_conference(msg, FALSE);
	else if (dbus_message_is_signal(msg, CSD_CSNET_REGISTRATION,
				"RegistrationChanged"))
		handle_registration_changed(msg);
	else if (dbus_message_is_signal(msg, CSD_CSNET_OPERATOR,
				"OperatorNameChanged"))
		handle_operator_name_changed(msg);
	else if (dbus_message_is_signal(msg, CSD_CSNET_SIGNAL,
				"SignalBarsChanged"))
		handle_signal_bars_changed(msg);
	else if (dbus_message_is_signal(msg, CSD_TELEPHONE_BATTERY,
				"BatteryBarsChanged"))
		handle_battery_bars_changed(msg);
	else if (dbus_message_is_signal(msg, CSD_CSNET_SUBSCRIBER,
				"SubscriberNumberChanged"))
		handle_subscriber_number_changed(msg);

	DBG("-\n");
	return TRUE;
}

static void dbus_add_watch(const char *sender, const char *path,
				const char *interface, const char *member)
{
	guint watch;

	watch = g_dbus_add_signal_watch(ag_connection, sender, path, interface,
					member, signal_filter, NULL, NULL);

	watches = g_slist_prepend(watches, GUINT_TO_POINTER(watch));
}

static const char *telephony_memory_dial_lookup(int location)
{
	/*memory dial not supported*/
	if (location == 1)
		return NULL;
	else
		return NULL;
}

/*API's that shall be ported*/

int telephony_init(void)
{
	uint32_t features = AG_FEATURE_EC_ANDOR_NR |
				AG_FEATURE_REJECT_A_CALL |
				AG_FEATURE_ENHANCED_CALL_STATUS |
				AG_FEATURE_THREE_WAY_CALLING;
	int i;

	DBG("");

	ag_connection = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);

	dbus_add_watch(NULL, CSD_CALL_PATH, CSD_CALL_INTERFACE, NULL);
	dbus_add_watch(NULL, CSD_CALL_PATH, CSD_CALL_INSTANCE, NULL);
	dbus_add_watch(NULL, CSD_CALL_PATH, CSD_CALL_CONFERENCE, NULL);
	dbus_add_watch(NULL, CSD_CSNET_PATH, CSD_CSNET_REGISTRATION,
			"RegistrationChanged");
	dbus_add_watch(NULL, CSD_CSNET_PATH, CSD_CSNET_OPERATOR,
			"OperatorNameChanged");
	dbus_add_watch(NULL, CSD_CSNET_PATH, CSD_CSNET_SIGNAL,
			"SignalBarsChanged");
	dbus_add_watch(NULL, CSD_CSNET_PATH, CSD_TELEPHONE_BATTERY,
			"BatteryBarsChanged");
	dbus_add_watch(NULL, CSD_CSNET_PATH, CSD_CSNET_SUBSCRIBER,
			"SubscriberNumberChanged");

	/* Reset indicators */
	for (i = 0; telephony_ag_indicators[i].desc != NULL; i++) {
		if (g_str_equal(telephony_ag_indicators[i].desc, "battchg"))
			telephony_ag_indicators[i].val = 5;
		else
			telephony_ag_indicators[i].val = 0;
	}

	/*Initializatoin of the indicators*/
	telephony_ready_ind(features, telephony_ag_indicators, BTRH_NOT_SUPPORTED,
								telephony_chld_str);

	return 0;
}

static void remove_watch(gpointer data)
{
	g_dbus_remove_watch(ag_connection, GPOINTER_TO_UINT(data));
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

	g_slist_foreach(watches, (GFunc) remove_watch, NULL);
	g_slist_free(watches);
	watches = NULL;

	dbus_connection_unref(ag_connection);
	ag_connection = NULL;

	telephony_deinit();
}

void telephony_device_connected(void *telephony_device)
{
}

void telephony_device_disconnected(void *telephony_device)
{
	events_enabled = FALSE;
}

void telephony_event_reporting_req(void *telephony_device, int ind)
{
	events_enabled = ind == 1 ? TRUE : FALSE;

	telephony_event_reporting_rsp(telephony_device, CME_ERROR_NONE);
}

void telephony_response_and_hold_req(void *telephony_device, int rh)
{
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

	if (strncmp(number, "*31#", 4) == 0) {
		number += 4;
		flags = CALL_FLAG_PRESENTATION_ALLOWED;
	} else if (strncmp(number, "#31#", 4) == 0) {
		number += 4;
		flags = CALL_FLAG_PRESENTATION_RESTRICTED;
	} else if (number[0] == '>') {
		int location = strtol(&number[1], NULL, 0);

		if (0 != dbus_method_call_send(CSD_CALL_BUS_NAME, CSD_CALL_PATH,
					CSD_CALL_INTERFACE, "DialMemory",
					telephony_dial_number_reply, telephony_device,
					DBUS_TYPE_INT32, &location,
					DBUS_TYPE_INVALID)) {
			telephony_dial_number_rsp(telephony_device,
							CME_ERROR_AG_FAILURE);
		}
		return;
	}

	if (0 != dbus_method_call_send(CSD_CALL_BUS_NAME, CSD_CALL_PATH,
				CSD_CALL_INTERFACE, "DialNo",
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

	if (NULL != (alerting = find_call_with_status(CSD_CALL_STATUS_CREATE)))
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
	else
		err = 0;

	if (err < 0)
		telephony_key_press_rsp(telephony_device,
							CME_ERROR_AG_FAILURE);
	else
		telephony_key_press_rsp(telephony_device, CME_ERROR_NONE);
}

void telephony_last_dialed_number_req(void *telephony_device)
{
	if (0 != dbus_method_call_send(CSD_CALL_BUS_NAME, CSD_CALL_PATH,
				CSD_CALL_INTERFACE, "DialLastNo",
				telephony_dial_number_reply, telephony_device,
				DBUS_TYPE_INVALID)) {
		telephony_dial_number_rsp(telephony_device,
						CME_ERROR_AG_FAILURE);
	}
}

void telephony_transmit_dtmf_req(void *telephony_device, char tone)
{
	char buf[2] = { tone, '\0' }, *buf_ptr = buf;

	if (0 != dbus_method_call_send(CSD_CALL_BUS_NAME, CSD_CALL_PATH,
				CSD_CALL_INTERFACE, "SendDtmf",
				NULL, NULL,
				DBUS_TYPE_STRING, &buf_ptr,
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

void telephony_operator_selection_req(void *telephony_device)
{
	telephony_operator_selection_ind(OPERATOR_MODE_AUTO,
				net.operator_name ? net.operator_name : "");
	telephony_operator_selection_rsp(telephony_device, CME_ERROR_NONE);
}

void telephony_nr_and_ec_req(void *telephony_device, gboolean enable)
{
	telephony_nr_and_ec_rsp(telephony_device, CME_ERROR_NONE);
}

void telephony_voice_dial_req(void *telephony_device, gboolean enable)
{
	telephony_voice_dial_rsp(telephony_device, CME_ERROR_NOT_SUPPORTED);
}

void telephony_subscriber_number_req(void *telephony_device)
{
	if (subscriber_number)
		telephony_subscriber_number_ind(subscriber_number,
						number_type(subscriber_number),
						SUBSCRIBER_SERVICE_VOICE);
	telephony_subscriber_number_rsp(telephony_device, CME_ERROR_NONE);
}

void telephony_list_phonebook_store(void *telephony_device)
{
/*
	For Blue & Me car kit we may have to add  the
	patch here(similar to the patch done in H1 and H2 )
*/
	telephony_list_phonebook_store_rsp(telephony_device,
			PHONEBOOK_STORE_LIST, CME_ERROR_NONE);
}

static int get_phonebook_count(const char *path, uint32_t *max_size,
										uint32_t *used)
{
	DBusConnection *conn;
	DBusMessageIter iter;
	DBusMessageIter value;
	DBusMessage *message, *reply;
	DBusError error;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		DBG("Can't get on system bus");
		return -1;
	}

	message = dbus_message_new_method_call("org.bluez.pb_agent",
										"/org/bluez/pb_agent",
										"org.bluez.PbAgent",
										"GetCallLogSize");
	if (!message) {
		DBG("Can't allocate new message");
		dbus_connection_unref(conn);
		return -1;
	}
	dbus_message_iter_init_append(message, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &path);

	dbus_message_iter_close_container(&iter, &value);
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
				DBUS_TYPE_UINT32, used,
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

	if ((g_strcmp0(path, "SM") == 0) || (g_strcmp0(path, "ME") == 0)) {
		*max_size = PHONEBOOK_COUNT_MAX;

		if (*used > PHONEBOOK_COUNT_MAX)
			*used = PHONEBOOK_COUNT_MAX;
	}
	if ((g_strcmp0(path, "DC") == 0) || (g_strcmp0(path, "MC") == 0) ||
			(g_strcmp0(path, "RC") == 0)) {
		*max_size = CALL_LOG_COUNT_MAX;

		if (*used > CALL_LOG_COUNT_MAX)
			*used = CALL_LOG_COUNT_MAX;
	}

	dbus_message_unref(reply);
	dbus_message_unref(message);
	dbus_connection_unref(conn);

	return 0;
}

void telephony_read_phonebook_store(void *telephony_device)
{
	if ('\0' != ag_pb_info.path[0]) {
/*
	Check the phone path ag_pb_info.path[] to which path it was set.
	If the path is NULL then set to "SM" and get the max_size and used
	counts from the phonebook type through dbus call to pbap agent
*/
		if (!get_phonebook_count(ag_pb_info.path, &ag_pb_info.max_size,
						&ag_pb_info.used))
			telephony_read_phonebook_store_rsp(telephony_device,
					ag_pb_info.path,
					ag_pb_info.max_size,
					ag_pb_info.used,
					CME_ERROR_NONE);
		else
			telephony_read_phonebook_store_rsp(telephony_device,
					ag_pb_info.path,
					ag_pb_info.max_size,
					ag_pb_info.used,
					CME_ERROR_AG_FAILURE);
	}
}

void telephony_set_phonebook_store(void *telephony_device, const char *path)
{
	if (NULL != path) {
		DBG("set phonebook type to [%s]\n", path);
		g_strlcpy(ag_pb_info.path, path, sizeof(ag_pb_info.path));
	}
}

void telephony_read_phonebook_attributes(void *telephony_device)
{
	uint32_t total_count = 0;
	uint32_t used_count = 0;

	if ('\0' != ag_pb_info.path[0]) {
/*
	Check the phone path ag_pb_info.path[] to which path it was set.
	If the path is NULL then set to "SM" and get the max_size and used
	counts from the phonebook type through dbus call to pbap agent
*/
		telephony_read_phonebook_attributes_rsp(telephony_device,
			ag_pb_info.max_size, 	PHONEBOOK_NUMBER_MAX_LENGTH,
			PHONEBOOK_NAME_MAX_LENGTH, CME_ERROR_NONE);
	}
}

static int convert_utf8_gsm(uint8_t ascii, uint8_t utf_8, uint8_t *gsm)
{
	uint32_t i = 0;

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
}

static void get_unicode_string(const char *name, char *unicodename)
{
	if (NULL != name || NULL != unicodename) {
		int len = strlen(name);
		if (len > 0) {
			int x = 0;
			int y = 0;
			if (len > PHONEBOOK_MAX_CHARACTER_LENGTH)
				len = PHONEBOOK_MAX_CHARACTER_LENGTH;
			for (x = 0, y = 0 ; x < len ; x++, y++) {
				if (x < (len - 1)) {
					if (convert_utf8_gsm(name[x], name[x+1] ,
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
	}
	return;
}

static int send_read_phonebook_resp(void *telephony_device, int32_t index,
					const char *name, const char *number)
{
	gchar *msg = NULL;
	int ret = -1;

	msg =  g_new0(gchar, PHONEBOOK_NAME_MAX_LENGTH +
				PHONEBOOK_NUMBER_MAX_LENGTH + PHONEBOOK_READ_RESP_LENGTH + 3);

	if (NULL != msg) {
		char nm[PHONEBOOK_NAME_MAX_LENGTH + 1] = {0,};
		char nb[PHONEBOOK_NAME_MAX_LENGTH + 1] = {0,};

		get_unicode_string(name, nm);
		get_unicode_string(number, nb);

		snprintf(msg, PHONEBOOK_NAME_MAX_LENGTH +
			PHONEBOOK_NUMBER_MAX_LENGTH + PHONEBOOK_READ_RESP_LENGTH + 3,
			"%d,\"%s\",0,\"%s\"", index, nb, nm);

		ret = telephony_read_phonebook_rsp(telephony_device, msg,
				CME_ERROR_NONE);

		g_free(msg);
	}
	return ret;
}

static int get_phonebook_list(void *telephony_device, const char* path,
					int32_t start_index, int32_t end_index)
{
	DBusConnection *conn;
	DBusMessage *message, *reply;
	DBusError error;
	DBusMessageIter iter, iter_struct, entry;
	int32_t idx = 0;
	if ((start_index > (int) ag_pb_info.max_size) || (start_index <= 0) ||
			(start_index > PHONEBOOK_COUNT_MAX)) {
		return -1;
	}

	if (end_index > (int) ag_pb_info.max_size)
		end_index = PHONEBOOK_COUNT_MAX ;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		DBG("Can't get on system bus");
		return -1;
	}

	message = dbus_message_new_method_call("org.bluez.pb_agent",
						"/org/bluez/pb_agent",
						"org.bluez.PbAgent",
						"GetPhonebookList");
	if (!message) {
		DBG("Can't allocate new message");
		return -1;
	}
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
		return -1;
	}

	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &iter_struct);

	idx = start_index;
	while (dbus_message_iter_get_arg_type(&iter_struct) == DBUS_TYPE_STRUCT) {
		const char *name = NULL;
		const char *tel = NULL;
		uint32_t handle = 0;

		dbus_message_iter_recurse(&iter_struct, &entry);

		dbus_message_iter_get_basic(&entry, &name);
		dbus_message_iter_next(&entry);
		dbus_message_iter_get_basic(&entry, &tel);
		dbus_message_iter_next(&entry);
		dbus_message_iter_get_basic(&entry, &handle);

		DBG("[%d] handle:%d name:%s tel:%s]\n", handle, name, tel);

		/*form the packet and sent to the remote headset*/
		if (-1 == end_index) {
			if (send_read_phonebook_resp(telephony_device,
					start_index, name, tel))
				DBG("send_read_phonebook_resp - ERROR\n");
			break;
		} else {
			if (idx >= start_index || idx <= end_index) {
				if (send_read_phonebook_resp(telephony_device, idx, name, tel)) {
					DBG("send_read_phonebook_resp - ERROR\n");
					telephony_read_phonebook_rsp(telephony_device, NULL,
							CME_ERROR_AG_FAILURE);

					dbus_message_unref(message);
					dbus_message_unref(reply);
					dbus_connection_unref(conn);

					return -1;
				}
				idx++;
			}
		}
		dbus_message_iter_next(&iter_struct);
	}

	telephony_read_phonebook_rsp(telephony_device, NULL, CME_ERROR_NONE);

	dbus_message_unref(message);
	dbus_message_unref(reply);
	dbus_connection_unref(conn);

	/*Process the List and send response*/
	return 0;
}

static int get_call_log_list(void *telephony_device, char* path ,
			int32_t start_index, int32_t end_index)
{
	DBusConnection *conn;
	DBusMessage *message = NULL, *reply;
	DBusError error;
	DBusMessageIter iter, iter_struct, entry;
	int32_t idx = 0;

	if ((start_index > (int) ag_pb_info.max_size) || (start_index <= 0) ||
			(start_index > CALL_LOG_COUNT_MAX)) {
		return -1;
	}

	if (end_index > (int) ag_pb_info.max_size)
		end_index = CALL_LOG_COUNT_MAX ;


	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		DBG("Can't get on system bus");
		return -1;
	}

	if (g_strcmp0(ag_pb_info.path, "DC") == 0) {
		message = dbus_message_new_method_call("org.bluez.pb_agent",
							"/org/bluez/pb_agent",
							"org.bluez.PbAgent",
							"GetOutgoingCallsList");
	} else if (g_strcmp0(ag_pb_info.path, "MC") == 0) {
		message = dbus_message_new_method_call("org.bluez.pb_agent",
							"/org/bluez/pb_agent",
							"org.bluez.PbAgent",
							"GetMissedCallsList");
	} else if (g_strcmp0(ag_pb_info.path, "RC") == 0) {
		message = dbus_message_new_method_call("org.bluez.pb_agent",
							"/org/bluez/pb_agent",
							"org.bluez.PbAgent",
							"GetIncomingCallsList");
	}
	if (!message) {
		DBG("Can't allocate new message");
		dbus_connection_unref(conn);
		return -1;
	}
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

	idx = start_index;
	while (dbus_message_iter_get_arg_type(&iter_struct) == DBUS_TYPE_STRUCT) {
		const char *name = NULL;
		const char *tel = NULL;
		uint32_t handle = 0;

		dbus_message_iter_recurse(&iter_struct, &entry);

		dbus_message_iter_get_basic(&entry, &name);
		dbus_message_iter_next(&entry);
		dbus_message_iter_get_basic(&entry, &tel);
		dbus_message_iter_next(&entry);
		dbus_message_iter_get_basic(&entry, &handle);

		DBG("[%d] handle:%d name:%s tel:%s]\n", handle, name, tel);

		/*form the packet and sent to the remote headset*/
		if (-1 == end_index) {
			if (send_read_phonebook_resp(telephony_device,
					start_index, name, tel))
				DBG("send_read_phonebook_resp - ERROR\n");
			break;
		} else {
			if (idx >= start_index || idx <= end_index) {
				/* Need to form the time stamp pkt  also */
				if (send_read_phonebook_resp(telephony_device, idx, name, tel)) {
					DBG("send_read_phonebook_resp - ERROR\n");
					telephony_read_phonebook_rsp(telephony_device, NULL,
							CME_ERROR_AG_FAILURE);

					dbus_message_unref(message);
					dbus_message_unref(reply);
					dbus_connection_unref(conn);

					return -1;
				}
				idx++;
			}
		}
		dbus_message_iter_next(&iter_struct);
	}

	telephony_read_phonebook_rsp(telephony_device, NULL, CME_ERROR_NONE);

	dbus_message_unref(message);
	dbus_message_unref(reply);
	dbus_connection_unref(conn);

	/*Process the List and send response*/
	return 0;

}
void telephony_read_phonebook(void *telephony_device, const char *cmd)
{
	char *ptr = 0;

	if (NULL != cmd) {
		int32_t start_index;
		int32_t end_index;
		ptr = (char *) strchr(cmd, (int32_t)',');
		if (NULL == ptr) {
			start_index = strtol(cmd, NULL, 0);
			end_index = -1;
			DBG("start_index = [%d] \n", start_index);
		} else {
			ptr++;
			start_index = strtol(cmd, NULL, 0);
			end_index = strtol(ptr, NULL, 0);
			DBG("start_index = [%d], end_index = [%d] \n",
					start_index, end_index);
		}

		if ((g_strcmp0(ag_pb_info.path, "SM") == 0) ||
				(g_strcmp0(ag_pb_info.path, "ME") == 0)) {
			if (get_phonebook_list(telephony_device, ag_pb_info.path,
					start_index, end_index)) {
				telephony_read_phonebook_rsp(telephony_device, NULL,
					CME_ERROR_AG_FAILURE);
				return;
			}
		}
		if ((g_strcmp0(ag_pb_info.path, "DC") == 0) ||
				(g_strcmp0(ag_pb_info.path, "MC") == 0) ||
				(g_strcmp0(ag_pb_info.path, "RC") == 0)) {
			if (get_call_log_list(telephony_device, ag_pb_info.path,
					start_index, end_index)) {
				telephony_read_phonebook_rsp(telephony_device, NULL,
						CME_ERROR_AG_FAILURE);
				return;
			}
		}

/*
	Using the start and end index get the contact list from the pbap agent and
	send the data to remote headset.
*/
	}
 }

void telephony_find_phonebook_entry_properties(void *telephony_device)
{
	telephony_find_phonebook_entry_properties_rsp(telephony_device,
			PHONEBOOK_NUMBER_MAX_LENGTH,
			PHONEBOOK_NAME_MAX_LENGTH,
			CME_ERROR_NONE);

}

void telephony_find_phonebook_entry(void *telephony_device, const char *cmd)
{
/*
	Get the contact that matches with the string "cmd" and send it back to the
	remote headset Need a dbus API to pbap agent that does the above operation
*/

}
void telephony_get_preffered_store_capacity(void *telephony_device)
{
	telephony_get_preffered_store_capacity_rsp(telephony_device,
			PREFFERED_MESSAGE_STORAGE_MAX,
			CME_ERROR_NONE);
}

void telephony_list_preffered_store(void *telephony_device)
{
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
	telephony_supported_character_generic_rsp(telephony_device,
			PHONEBOOK_CHARACTER_SET_SUPPORTED,
			CME_ERROR_NONE);

}

void telephony_list_supported_character(void *telephony_device)
{
	telephony_supported_character_generic_rsp(telephony_device,
			PHONEBOOK_CHARACTER_SET_LIST,
			CME_ERROR_NONE);
}

/*
void telephony_set_characterset(void *telephony_device, const char *cmd)
{
}
*/

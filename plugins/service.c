/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2006-2010  Nokia Corporation
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include <gdbus.h>

#include "sdpd.h"
#include "sdp-xml.h"
#include "plugin.h"
#include "adapter.h"
#include "error.h"
#include "log.h"

#define SERVICE_INTERFACE "org.bluez.Service"

static DBusConnection *connection;

struct record_data {
	uint32_t handle;
	char *sender;
	guint listener_id;
	struct service_adapter *serv_adapter;
};

struct context_data {
	sdp_record_t *record;
	sdp_data_t attr_data;
	struct sdp_xml_data *stack_head;
	uint16_t attr_id;
};

struct pending_auth {
	DBusConnection *conn;
	DBusMessage *msg;
	char *sender;
	bdaddr_t dst;
	char uuid[MAX_LEN_UUID_STR];
};

struct service_adapter {
	struct btd_adapter *adapter;
	GSList *pending_list;
	GSList *records;
};

static struct service_adapter *serv_adapter_any = NULL;

static int compute_seq_size(sdp_data_t *data)
{
	int unit_size = data->unitSize;
	sdp_data_t *seq = data->val.dataseq;

	for (; seq; seq = seq->next)
		unit_size += seq->unitSize;

	return unit_size;
}

static void element_start(GMarkupParseContext *context,
		const gchar *element_name, const gchar **attribute_names,
		const gchar **attribute_values, gpointer user_data, GError **err)
{
	struct context_data *ctx_data = user_data;

	if (!strcmp(element_name, "record"))
		return;

	if (!strcmp(element_name, "attribute")) {
		int i;
		for (i = 0; attribute_names[i]; i++) {
			if (!strcmp(attribute_names[i], "id")) {
				ctx_data->attr_id = strtol(attribute_values[i], 0, 0);
				break;
			}
		}
		DBG("New attribute 0x%04x", ctx_data->attr_id);
		return;
	}

	if (ctx_data->stack_head) {
		struct sdp_xml_data *newelem = sdp_xml_data_alloc();
		newelem->next = ctx_data->stack_head;
		ctx_data->stack_head = newelem;
	} else {
		ctx_data->stack_head = sdp_xml_data_alloc();
		ctx_data->stack_head->next = NULL;
	}

	if (!strcmp(element_name, "sequence"))
		ctx_data->stack_head->data = sdp_data_alloc(SDP_SEQ8, NULL);
	else if (!strcmp(element_name, "alternate"))
		ctx_data->stack_head->data = sdp_data_alloc(SDP_ALT8, NULL);
	else {
		int i;
		/* Parse value, name, encoding */
		for (i = 0; attribute_names[i]; i++) {
			if (!strcmp(attribute_names[i], "value")) {
				int curlen = strlen(ctx_data->stack_head->text);
				int attrlen = strlen(attribute_values[i]);

				/* Ensure we're big enough */
				while ((curlen + 1 + attrlen) > ctx_data->stack_head->size) {
					sdp_xml_data_expand(ctx_data->stack_head);
				}

				memcpy(ctx_data->stack_head->text + curlen,
						attribute_values[i], attrlen);
				ctx_data->stack_head->text[curlen + attrlen] = '\0';
			}

			if (!strcmp(attribute_names[i], "encoding")) {
				if (!strcmp(attribute_values[i], "hex"))
					ctx_data->stack_head->type = 1;
			}

			if (!strcmp(attribute_names[i], "name")) {
				ctx_data->stack_head->name = strdup(attribute_values[i]);
			}
		}

		ctx_data->stack_head->data = sdp_xml_parse_datatype(element_name,
				ctx_data->stack_head, ctx_data->record);

		if (ctx_data->stack_head->data == NULL)
			error("Can't parse element %s", element_name);
	}
}

static void element_end(GMarkupParseContext *context,
		const gchar *element_name, gpointer user_data, GError **err)
{
	struct context_data *ctx_data = user_data;
	struct sdp_xml_data *elem;

	if (!strcmp(element_name, "record"))
		return;

	if (!strcmp(element_name, "attribute")) {
		if (ctx_data->stack_head && ctx_data->stack_head->data) {
			int ret = sdp_attr_add(ctx_data->record, ctx_data->attr_id,
							ctx_data->stack_head->data);
			if (ret == -1)
				DBG("Could not add attribute 0x%04x",
							ctx_data->attr_id);

			ctx_data->stack_head->data = NULL;
			sdp_xml_data_free(ctx_data->stack_head);
			ctx_data->stack_head = NULL;
		} else {
			DBG("No data for attribute 0x%04x", ctx_data->attr_id);
		}
		return;
	}

	if (!strcmp(element_name, "sequence")) {
		ctx_data->stack_head->data->unitSize = compute_seq_size(ctx_data->stack_head->data);

		if (ctx_data->stack_head->data->unitSize > USHRT_MAX) {
			ctx_data->stack_head->data->unitSize += sizeof(uint32_t);
			ctx_data->stack_head->data->dtd = SDP_SEQ32;
		} else if (ctx_data->stack_head->data->unitSize > UCHAR_MAX) {
			ctx_data->stack_head->data->unitSize += sizeof(uint16_t);
			ctx_data->stack_head->data->dtd = SDP_SEQ16;
		} else {
			ctx_data->stack_head->data->unitSize += sizeof(uint8_t);
		}
	} else if (!strcmp(element_name, "alternate")) {
		ctx_data->stack_head->data->unitSize = compute_seq_size(ctx_data->stack_head->data);

		if (ctx_data->stack_head->data->unitSize > USHRT_MAX) {
			ctx_data->stack_head->data->unitSize += sizeof(uint32_t);
			ctx_data->stack_head->data->dtd = SDP_ALT32;
		} else if (ctx_data->stack_head->data->unitSize > UCHAR_MAX) {
			ctx_data->stack_head->data->unitSize += sizeof(uint16_t);
			ctx_data->stack_head->data->dtd = SDP_ALT16;
		} else {
			ctx_data->stack_head->data->unitSize += sizeof(uint8_t);
		}
	}

	if (ctx_data->stack_head->next && ctx_data->stack_head->data &&
					ctx_data->stack_head->next->data) {
		switch (ctx_data->stack_head->next->data->dtd) {
		case SDP_SEQ8:
		case SDP_SEQ16:
		case SDP_SEQ32:
		case SDP_ALT8:
		case SDP_ALT16:
		case SDP_ALT32:
			ctx_data->stack_head->next->data->val.dataseq =
				sdp_seq_append(ctx_data->stack_head->next->data->val.dataseq,
								ctx_data->stack_head->data);
			ctx_data->stack_head->data = NULL;
			break;
		}

		elem = ctx_data->stack_head;
		ctx_data->stack_head = ctx_data->stack_head->next;

		sdp_xml_data_free(elem);
	}
}

static GMarkupParser parser = {
	element_start, element_end, NULL, NULL, NULL
};

static sdp_record_t *sdp_xml_parse_record(const char *data, int size)
{
	GMarkupParseContext *ctx;
	struct context_data *ctx_data;
	sdp_record_t *record;

	ctx_data = malloc(sizeof(*ctx_data));
	if (!ctx_data)
		return NULL;

	record = sdp_record_alloc();
	if (!record) {
		free(ctx_data);
		return NULL;
	}

	memset(ctx_data, 0, sizeof(*ctx_data));
	ctx_data->record = record;

	ctx = g_markup_parse_context_new(&parser, 0, ctx_data, NULL);

	if (g_markup_parse_context_parse(ctx, data, size, NULL) == FALSE) {
		error("XML parsing error");
		g_markup_parse_context_free(ctx);
		sdp_record_free(record);
		free(ctx_data);
		return NULL;
	}

	g_markup_parse_context_free(ctx);

	free(ctx_data);

	return record;
}

static struct record_data *find_record(struct service_adapter *serv_adapter,
					uint32_t handle, const char *sender)
{
	GSList *list;

	for (list = serv_adapter->records; list; list = list->next) {
		struct record_data *data = list->data;
		if (handle == data->handle && !strcmp(sender, data->sender))
			return data;
	}

	return NULL;
}

static struct pending_auth *next_pending(struct service_adapter *serv_adapter)
{
	GSList *l = serv_adapter->pending_list;

	if (l) {
		struct pending_auth *auth = l->data;
		return auth;
	}

	return NULL;
}

static struct pending_auth *find_pending_by_sender(
			struct service_adapter *serv_adapter,
			const char *sender)
{
	GSList *l = serv_adapter->pending_list;

	for (; l; l = l->next) {
		struct pending_auth *auth = l->data;
		if (g_str_equal(auth->sender, sender))
			return auth;
	}

	return NULL;
}

static void exit_callback(DBusConnection *conn, void *user_data)
{
	struct record_data *user_record = user_data;
	struct service_adapter *serv_adapter = user_record->serv_adapter;
	struct pending_auth *auth;

	DBG("remove record");

	serv_adapter->records = g_slist_remove(serv_adapter->records,
						user_record);

	auth = find_pending_by_sender(serv_adapter, user_record->sender);
	if (auth) {
		serv_adapter->pending_list = g_slist_remove(serv_adapter->pending_list,
							auth);
		g_free(auth);
	}

	remove_record_from_server(user_record->handle);

	g_free(user_record->sender);
	g_free(user_record);
}

static int add_xml_record(DBusConnection *conn, const char *sender,
			struct service_adapter *serv_adapter,
			const char *record, dbus_uint32_t *handle)
{
	struct record_data *user_record;
	sdp_record_t *sdp_record;
	bdaddr_t src;

	sdp_record = sdp_xml_parse_record(record, strlen(record));
	if (!sdp_record) {
		error("Parsing of XML service record failed");
		return -EIO;
	}

	if (serv_adapter->adapter)
		adapter_get_address(serv_adapter->adapter, &src);
	else
		bacpy(&src, BDADDR_ANY);

	if (add_record_to_server(&src, sdp_record) < 0) {
		error("Failed to register service record");
		sdp_record_free(sdp_record);
		return -EIO;
	}

	user_record = g_new0(struct record_data, 1);
	user_record->handle = sdp_record->handle;
	user_record->sender = g_strdup(sender);
	user_record->serv_adapter = serv_adapter;
	user_record->listener_id = g_dbus_add_disconnect_watch(conn, sender,
					exit_callback, user_record, NULL);

	serv_adapter->records = g_slist_append(serv_adapter->records,
								user_record);

	DBG("listener_id %d", user_record->listener_id);

	*handle = user_record->handle;

	return 0;
}

static DBusMessage *update_record(DBusConnection *conn, DBusMessage *msg,
		struct service_adapter *serv_adapter,
		dbus_uint32_t handle, sdp_record_t *sdp_record)
{
	bdaddr_t src;
	int err;

	if (remove_record_from_server(handle) < 0) {
		sdp_record_free(sdp_record);
		return btd_error_not_available(msg);
	}

	if (serv_adapter->adapter)
		adapter_get_address(serv_adapter->adapter, &src);
	else
		bacpy(&src, BDADDR_ANY);

	sdp_record->handle = handle;
	err = add_record_to_server(&src, sdp_record);
	if (err < 0) {
		sdp_record_free(sdp_record);
		error("Failed to update the service record");
		return btd_error_failed(msg, strerror(-err));
	}

	return dbus_message_new_method_return(msg);
}

static DBusMessage *update_xml_record(DBusConnection *conn,
				DBusMessage *msg,
				struct service_adapter *serv_adapter)
{
	struct record_data *user_record;
	sdp_record_t *sdp_record;
	const char *record;
	dbus_uint32_t handle;
	int len;

	if (dbus_message_get_args(msg, NULL,
				DBUS_TYPE_UINT32, &handle,
				DBUS_TYPE_STRING, &record,
				DBUS_TYPE_INVALID) == FALSE)
		return NULL;

	len = (record ? strlen(record) : 0);
	if (len == 0)
		return btd_error_invalid_args(msg);

	user_record = find_record(serv_adapter, handle,
				dbus_message_get_sender(msg));
	if (!user_record)
		return btd_error_not_available(msg);

	sdp_record = sdp_xml_parse_record(record, len);
	if (!sdp_record) {
		error("Parsing of XML service record failed");
		sdp_record_free(sdp_record);
		return btd_error_failed(msg,
					"Parsing of XML service record failed");
	}

	return update_record(conn, msg, serv_adapter, handle, sdp_record);
}

static int remove_record(DBusConnection *conn, const char *sender,
			struct service_adapter *serv_adapter,
			dbus_uint32_t handle)
{
	struct record_data *user_record;

	DBG("remove record 0x%x", handle);

	user_record = find_record(serv_adapter, handle, sender);
	if (!user_record)
		return -1;

	DBG("listner_id %d", user_record->listener_id);

	g_dbus_remove_watch(conn, user_record->listener_id);

	exit_callback(conn, user_record);

	return 0;
}

static DBusMessage *add_service_record(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct service_adapter *serv_adapter = data;
	DBusMessage *reply;
	const char *sender, *record;
	dbus_uint32_t handle;
	int err;

	if (dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &record, DBUS_TYPE_INVALID) == FALSE)
		return NULL;

	sender = dbus_message_get_sender(msg);
	err = add_xml_record(conn, sender, serv_adapter, record, &handle);
	if (err < 0)
		return btd_error_failed(msg, strerror(-err));

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	dbus_message_append_args(reply, DBUS_TYPE_UINT32, &handle,
							DBUS_TYPE_INVALID);

	return reply;
}

static DBusMessage *update_service_record(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct service_adapter *serv_adapter = data;

	return update_xml_record(conn, msg, serv_adapter);
}

static DBusMessage *remove_service_record(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct service_adapter *serv_adapter = data;
	dbus_uint32_t handle;
	const char *sender;

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &handle,
						DBUS_TYPE_INVALID) == FALSE)
		return NULL;

	sender = dbus_message_get_sender(msg);

	if (remove_record(conn, sender, serv_adapter, handle) < 0)
		return btd_error_not_available(msg);

	return dbus_message_new_method_return(msg);
}

static void auth_cb(DBusError *derr, void *user_data)
{
	struct service_adapter *serv_adapter = user_data;
	DBusMessage *reply;
	struct pending_auth *auth;
	bdaddr_t src;

	auth = next_pending(serv_adapter);
	if (auth == NULL) {
		info("Authorization cancelled: Client exited");
		return;
	}

	if (derr) {
		error("Access denied: %s", derr->message);

		reply = btd_error_not_authorized(auth->msg);
		dbus_message_unref(auth->msg);
		g_dbus_send_message(auth->conn, reply);
		goto done;
	}

	g_dbus_send_reply(auth->conn, auth->msg,
			DBUS_TYPE_INVALID);

done:
	dbus_connection_unref(auth->conn);

	serv_adapter->pending_list = g_slist_remove(serv_adapter->pending_list,
									auth);
	g_free(auth);

	auth = next_pending(serv_adapter);
	if (auth == NULL)
		return;

	if (serv_adapter->adapter)
		adapter_get_address(serv_adapter->adapter, &src);
	else
		bacpy(&src, BDADDR_ANY);

	btd_request_authorization(&src, &auth->dst,
					auth->uuid, auth_cb, serv_adapter);
}

static DBusMessage *request_authorization(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	struct record_data *user_record;
	struct service_adapter *serv_adapter = data;
	sdp_record_t *record;
	sdp_list_t *services;
	const char *sender;
	dbus_uint32_t handle;
	const char *address;
	struct pending_auth *auth;
	char uuid_str[MAX_LEN_UUID_STR];
	uuid_t *uuid, *uuid128;
	bdaddr_t src;

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &address,
					DBUS_TYPE_UINT32, &handle,
					DBUS_TYPE_INVALID) == FALSE)
		return NULL;

	sender = dbus_message_get_sender(msg);
	if (find_pending_by_sender(serv_adapter, sender))
		return btd_error_does_not_exist(msg);
#ifdef __TIZEN_PATCH__


	info("handle %x\n", handle);
	if(1 != handle)
	{

	user_record = find_record(serv_adapter, handle, sender);
	if (!user_record) {
		user_record = find_record(serv_adapter_any, handle, sender);
		if (!user_record)
			return btd_error_not_authorized(msg);
	}

	record = sdp_record_find(user_record->handle);
	if (record == NULL)
		return btd_error_not_authorized(msg);

	if (sdp_get_service_classes(record, &services) < 0) {
		sdp_record_free(record);
		return btd_error_not_authorized(msg);
	}

	if (services == NULL)
		return btd_error_not_authorized(msg);

	uuid = services->data;
	uuid128 = sdp_uuid_to_uuid128(uuid);

	sdp_list_free(services, bt_free);

	if (sdp_uuid2strn(uuid128, uuid_str, MAX_LEN_UUID_STR) < 0) {
		bt_free(uuid128);
		return btd_error_not_authorized(msg);
	}
	bt_free(uuid128);
	}
#else
	user_record = find_record(serv_adapter, handle, sender);
	if (!user_record) {
		user_record = find_record(serv_adapter_any, handle, sender);
		if (!user_record)
			return not_authorized(msg);
	}

	record = sdp_record_find(user_record->handle);
	if (record == NULL)
		return not_authorized(msg);

	if (sdp_get_service_classes(record, &services) < 0) {
		sdp_record_free(record);
		return not_authorized(msg);
	}

	if (services == NULL)
		return not_authorized(msg);

	uuid = services->data;
	uuid128 = sdp_uuid_to_uuid128(uuid);

	sdp_list_free(services, bt_free);

	if (sdp_uuid2strn(uuid128, uuid_str, MAX_LEN_UUID_STR) < 0) {
		bt_free(uuid128);
		return not_authorized(msg);
	}
	bt_free(uuid128);
#endif
	auth = g_new0(struct pending_auth, 1);
	auth->msg = dbus_message_ref(msg);
	auth->conn = dbus_connection_ref(connection);
#ifdef __TIZEN_PATCH__
	if(1 != handle)
	{
		auth->sender = user_record->sender;
		memcpy(auth->uuid, uuid_str, MAX_LEN_UUID_STR);
	}
	else
	{
		auth->sender = (char *)sender;
		char* uuid_l2cap = "00000100-0000-1000-8000-00805f9b34fb";
		memset(auth->uuid, 0, MAX_LEN_UUID_STR);
		memcpy(auth->uuid, uuid_l2cap, strlen(uuid_l2cap));
	}
#else
	auth->sender = user_record->sender;
	memcpy(auth->uuid, uuid_str, MAX_LEN_UUID_STR);

#endif
	str2ba(address, &auth->dst);

	serv_adapter->pending_list = g_slist_append(serv_adapter->pending_list,
									auth);

	auth = next_pending(serv_adapter);
	if (auth == NULL)
		return btd_error_does_not_exist(msg);

	if (serv_adapter->adapter)
		adapter_get_address(serv_adapter->adapter, &src);
	else
		bacpy(&src, BDADDR_ANY);

	if (btd_request_authorization(&src, &auth->dst, auth->uuid, auth_cb,
							serv_adapter) < 0) {
		serv_adapter->pending_list = g_slist_remove(serv_adapter->pending_list,
									auth);
		g_free(auth);
		return btd_error_not_authorized(msg);
	}

	return NULL;
}

static DBusMessage *cancel_authorization(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	DBusMessage *reply;
	struct service_adapter *serv_adapter = data;
	struct pending_auth *auth;
	const gchar *sender;
	bdaddr_t src;

	sender = dbus_message_get_sender(msg);

	auth = find_pending_by_sender(serv_adapter, sender);
	if (auth == NULL)
		return btd_error_does_not_exist(msg);

	if (serv_adapter->adapter)
		adapter_get_address(serv_adapter->adapter, &src);
	else
		bacpy(&src, BDADDR_ANY);

	btd_cancel_authorization(&src, &auth->dst);

	reply = btd_error_not_authorized(auth->msg);
	dbus_message_unref(auth->msg);
	g_dbus_send_message(auth->conn, reply);

	dbus_connection_unref(auth->conn);

	serv_adapter->pending_list = g_slist_remove(serv_adapter->pending_list,
									auth);
	g_free(auth);

	auth = next_pending(serv_adapter);
	if (auth == NULL)
		goto done;

	if (serv_adapter->adapter)
		adapter_get_address(serv_adapter->adapter, &src);
	else
		bacpy(&src, BDADDR_ANY);

	btd_request_authorization(&src, &auth->dst,
					auth->uuid, auth_cb, serv_adapter);

done:
	return dbus_message_new_method_return(msg);
}

#ifdef __TIZEN_PATCH__
extern sdp_session_t *g_cached_session;

static DBusMessage *siso_add_service_record_pdu(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	info("siso_add_service_record_pdu() +\n");
	DBusMessage *reply;
	const char *sender, *record;
	uint32_t len;
	dbus_uint32_t handle;
	int err;
	sdp_record_t *sdp_record;
	int scanned;
	int ret = -1;
	DBusMessageIter iter, array;
	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_recurse(&iter, &array);
	dbus_message_iter_get_fixed_array(&array, &record, &len);
	if (len <= 0) {
		info("Error!!!! ... Invalid args....\n");
		return btd_error_invalid_args(msg);
	}
	info("\n");
	sdp_record = (sdp_record_t *)sdp_extract_pdu_safe(record, len, &scanned);
	if (!sdp_record) {
		info("Error!!! ---------Parsing of service record failed--------\n");
		return btd_error_invalid_args(msg);
	}
	if (scanned != len) {
		info("Warning!!!! Size mismatch of service record, scanned = %d, len = %d\n",
							scanned, len);
		/*The JSRapp service record seesm to be wrong!!!!.
		 * Hence its returning the value 49 and 35.
		 * Its a missmatch. Once its is corrected uncomment below code.*/
		/* sdp_record_free(sdp_record);
		   return -1;
		 */
	}

	if (sdp_record->handle < 0x10000) {
		DBG("Invalid record handle 0x%05x", sdp_record->handle);
		sdp_record_free(sdp_record);
		return btd_error_invalid_args(msg);
	}

	if (add_record_to_server(BDADDR_ANY, sdp_record) < 0) {
		info("Error !!!!!!!!      add_record_to_server() \n");
		info("Failed to register service record for handle 0x%x\n", sdp_record->handle);
		sdp_record_free(sdp_record);
		return btd_error_invalid_args(msg);
	}

	handle = sdp_record->handle;

	info("Received handler = 0x%x\n", handle);



	reply = dbus_message_new_method_return(msg);
	if (!reply) {
		info("Error in reply\n");
		return btd_error_invalid_args(msg);
	}

	dbus_message_append_args(reply, DBUS_TYPE_UINT32, &handle,
							DBUS_TYPE_INVALID);
	info("-\n");

	return reply;

}


static DBusMessage *siso_remove_service_record(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	info("\n\n\n\n++++++++++++++++++ (RX) siso_remove_service_record +++++++++++++++++++++++\n\n\n\n");
	struct service_adapter *serv_adapter = data;
	dbus_uint32_t handle;
	sdp_record_t *rec;
	DBusMessage *reply;
	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &handle,
						DBUS_TYPE_INVALID) == FALSE)
	{
		info("\nError!!!! ---------------- Invalid arguments ................................\n");
		return btd_error_invalid_args(msg);
	}
	DBG("RRemoving record with handle 0x%05x", handle);

	rec = sdp_record_find(handle);
	if (!rec)
	{
		info("\n---------------- Record Not found for the handler 0x%x-----------\n", handle);
		return btd_error_invalid_args(msg);
	}
	if (sdp_record_remove(handle) == 0) {
		update_db_timestamp();
		update_svclass_list(BDADDR_ANY);
	}

	sdp_record_free(rec);
	info("\nFreed SDP record\n");

//	return g_dbus_create_reply(msg, DBUS_TYPE_INVALID);

	reply = dbus_message_new_method_return(msg);
	if (!reply)
	{
		info("Error in reply\n");
		return btd_error_invalid_args(msg);
	}
	info("-\n");

	return reply;

}

static volatile sig_atomic_t __io_finished;

static void __dbus_sdp_callback_multiple_handle_complete__cb(uint8_t type, uint16_t status,
				uint8_t *rsp, size_t size, void *udata)
{
	unsigned int i;
	int err = -1;
	int tsrc, csrc, tsrc_count, index_count = 0;
	uint32_t s_handle;
	DBusConnection	*conn;
	//DBusMessage *reply;
	//DBusMessageIter iter, array_iter;
	service_dbus_ctxt_t* ctxt = (service_dbus_ctxt_t*)udata;

	info("__dbus_sdp_callback_multiple_handle_complete__cb.. +\n");
#if 0 //Service search
      if( SDP_ERROR_RSP == status)
      	{
      		info(" sdp timed out \n");
		__io_finished = 1;
		return failed_strerror(ctxt->msg, err);
      	}
	printf("\n");

	if (type == SDP_ERROR_RSP) {
		__io_finished = 1; /* We have to come of the loop when type == SDP_ERROR_RSP */
 		return failed_strerror(ctxt->msg, err);
	}

	info(" after  SDP_ERROR_RSP check \n");

       if(NULL == udata)
       {
		     info(" sdp search is over \n");

	}
      else
      	{
		info(" sdp search continues \n");
	reply = dbus_message_new_method_return(ctxt->msg);
		info( " sdp got a reply \n");

		if(reply)
		{
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
				DBUS_TYPE_UINT32_AS_STRING, &array_iter);
		}
		else
		{
			info(" reply is NULL \n");
		}
      	}

	info(" after  iteration \n");
#else
       info("type!!!!!!!!!    %d\n",type);
	printf("\n");
	if(type == 0 )
	{
		info("timed outttttttt\n");

          __io_finished =1;
	       return;
	}

	if (type == SDP_ERROR_RSP) {
		__io_finished = 1;
        err = -EIO;
		info("Dbus Failed err =%d\n", err);
 		return;
	}

#endif
	uint8_t *pdata = rsp;
	tsrc = ntohs(bt_get_unaligned((uint16_t *) pdata));
	if ( tsrc <= 0)
	{
		info("tsrc not found");
//		s_handle = 0xFE000000; /*Record not found*/
		goto done;
	}
	else
	{


		pdata += sizeof(uint16_t);
		csrc = ntohs(bt_get_unaligned((uint16_t *) pdata));
		if ( csrc <= 0)
		{
			info("csrc not found");
//			s_handle = 0xFE000000; /*Record not found*/
			goto done;

		}
		else
		{
			info("Total service record found = %d, CSR = %d", tsrc, tsrc);
			pdata += sizeof(uint16_t);
			tsrc_count = tsrc;
			index_count = 0;
			do
			{
				s_handle = ntohl(bt_get_unaligned((uint32_t*)pdata));
				pdata += sizeof(uint32_t);
#if 0 //Service search
	 			dbus_message_iter_append_basic(&array_iter,
								DBUS_TYPE_UINT32, &s_handle);
#else
	 			dbus_message_iter_append_basic(ctxt->array_iter,
								DBUS_TYPE_UINT32, &s_handle);
#endif
				++index_count;
				DBG("got handle 0x%x count %d", s_handle, index_count);
				if(index_count >= MAX_REMOTE_SERVICES) break;
			}while(--tsrc_count);

		}
	}



	info( "********Received handles = 0X%X ***********, total rx size = %d\n", index_count, size);


done:
	__io_finished = 1;
#if 0 //Service search
	dbus_message_iter_close_container(&iter, &array_iter);
	dbus_connection_send(ctxt->conn, reply, NULL);
	dbus_message_unref(reply);

	if(ctxt != NULL)
	{
		//dbus_connection_unref(ctxt->conn);
		dbus_message_unref(ctxt->msg);
		free(ctxt);
		ctxt = NULL;

	}
#endif

	info( "__socket_sdp_callback_multiple_handle_complete__cb -\n");
	return;
}


static void get_remote_service_cb(sdp_list_t *recs, int err, gpointer user_data)
{
	sdp_list_t *seq;
	int ii = 0, index_count=0;
	int struct_len;
	DBusMessage *reply;
	DBusMessageIter iter, array_iter;

	service_dbus_ctxt_t* service_search_ctxt = (service_dbus_ctxt_t*) user_data;

	info("get_remote_service_cb+");

	if (err < 0) {
		error("Unable to get service record: %s (%d)", strerror(-err), -err);
		goto fail;
	}

	if (!recs || !recs->data) {
		info("No records found\n");
		//error("No records found");
		//goto fail;
	}

	reply = dbus_message_new_method_return(service_search_ctxt->msg);

	if(reply)
	{
		dbus_message_iter_init_append(reply, &iter);
		dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_UINT32_AS_STRING, &array_iter);
	}
	else
	{
		info(" reply is NULL \n");
		goto fail;
	}

	for (seq = recs; seq; seq = seq->next)
	{
		sdp_record_t *rec = (sdp_record_t *) seq->data;
		GString *result;

		if (!rec)
			break;

		dbus_message_iter_append_basic(&array_iter, DBUS_TYPE_UINT32, &rec->handle);

		++index_count;
	}


	info( "********Received handles = 0X%X ***********\n", index_count);


done:
	dbus_message_iter_close_container(&iter, &array_iter);
	dbus_connection_send(service_search_ctxt->conn, reply, NULL);
	dbus_message_unref(reply);

fail:
	if(service_search_ctxt != NULL)
	{
		//dbus_connection_unref(service_search_ctxt->conn);
		dbus_message_unref(service_search_ctxt->msg);
		free(service_search_ctxt);
		service_search_ctxt = NULL;
	}

	info("get_remote_service_cb-");
	return;

}


static DBusMessage *get_remote_service_handles(DBusConnection *conn, DBusMessage *msg, void *data)
{
	int err = 0;
	uuid_t uuid;
	char * addr, *match;
	bdaddr_t bdadd;
	service_dbus_ctxt_t* service_search_ctxt = NULL;


	info("get_remote_service_handles");

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &addr, DBUS_TYPE_STRING, &match,
						DBUS_TYPE_INVALID) == FALSE)
	{
		info("\n---------------- Invalid arguments ................................\n");
		return btd_error_invalid_args(msg);
	}
	info( "Addr: %s\n", addr);
	info( "UUID: %s\n", match);
	str2ba(addr, &bdadd);

	if (strlen(match) > 0)
	{
		if (bt_string2uuid(&uuid, match) < 0)
		{
			error("Invalid service class name");
			return btd_error_invalid_args(msg);
		}
		sdp_uuid128_to_uuid(&uuid);
	}

	service_search_ctxt = g_try_malloc0(sizeof(service_dbus_ctxt_t));
	if (!service_search_ctxt) {
		info("malloc() failed");
		return btd_error_failed(msg, "No Memory");
	}
	service_search_ctxt->conn = conn;
	service_search_ctxt->msg = dbus_message_ref(msg);

	err = bt_search_service(BDADDR_ANY, &bdadd, &uuid, get_remote_service_cb, service_search_ctxt, NULL);
	if (err < 0)
	{
			error("Invalid service class name");
			return btd_error_failed(msg, "Invalid service class name");
	}
 	return NULL;
}

static DBusMessage *siso_get_remote_service_handles(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	info("\n\n\n\n++++++++++++++++++ (RX) siso_get_remote_service_handles +++++++++++++++++++++++\n\n\n\n");
	struct service_adapter *serv_adapter = data;
	dbus_uint32_t handle;
	sdp_record_t *rec;
	DBusMessage *reply;

	char * addr, *match;
	bdaddr_t bdadd;
	sdp_list_t *search, *attrids;
	uint32_t range = 0x0000ffff;
	uuid_t uuid;
	service_dbus_ctxt_t* ctxt = NULL;
	DBusMessageIter iter, array_iter;


	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &addr, DBUS_TYPE_STRING, &match,
						DBUS_TYPE_INVALID) == FALSE)
	{
		info("\nError!!!! ---------------- Invalid arguments ................................\n");
		return btd_error_invalid_args(msg);
	}
	info( "Before sdp_connect with bdaddr %s\n", addr);
	str2ba(addr, &bdadd);
	if (strlen(match) > 0)
	{
		if (bt_string2uuid(&uuid, match) < 0)
		{
			error("Invalid service class name");
			return btd_error_invalid_args(msg);
		}
	}

	if(NULL == g_cached_session)
	{
		g_cached_session = sdp_connect(BDADDR_ANY, &bdadd, 0);
		if (!g_cached_session)
		{
			info("Can't connect to SDP service\n");
			goto error_done;
			//break;
		}
	}

	info( "Before sdp_set_notify\n");
	__io_finished = 0;
	ctxt = malloc(sizeof(service_dbus_ctxt_t));

	if(NULL == ctxt)
	{
		info("!!!!!Memory allocation failed for ctxt\n");

		goto error_done;
	}

	ctxt->conn = conn;
	ctxt->msg = dbus_message_ref(msg);

	reply = dbus_message_new_method_return(ctxt->msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
				DBUS_TYPE_UINT32_AS_STRING, &array_iter);

	ctxt->array_iter = &array_iter;

	sdp_set_notify(g_cached_session, __dbus_sdp_callback_multiple_handle_complete__cb, ctxt);


	search = sdp_list_append(NULL, &uuid);

	info( "Before sdp_list_append\n");

	attrids = sdp_list_append(NULL, &range);

	info( "Before sdp_service_search_async\n");

	if(0 != sdp_service_search_async(g_cached_session, search, 0xffff))
	{
		error("Error : sdp_service_search_async()");
		goto error_done;
	}

	info( "Before sdp_list_free\n");

	sdp_list_free(attrids, NULL);

	info( "Before sdp_list_free\n");

	sdp_list_free(search, NULL);

	while (!__io_finished)
	{
		printf(". ");
		info(" calling sdp_process [ enter] : %d\n", __io_finished);
		 if (sdp_process(g_cached_session) == -1)
		 {
			info( "Search Completed : error\n");

		 }

		 info(" calling sdp_process [leave]: %d\n", __io_finished);
	}
	dbus_message_iter_close_container(&iter, &array_iter);

	free(ctxt);

	return reply;
error_done:
	info( "Error done .. before ctx free\n");

	if(ctxt != NULL)
	{
		free(ctxt);
		ctxt = NULL;
	}

	info( "Error done .. After ctx free\n");

	info("-\n");

	return btd_error_invalid_args(msg);
}

static void __sdp_callback_xml_complete_dbus__cb(uint8_t type, uint16_t err, uint8_t *rsp, size_t size, void *udata)
{
	info("__sdp_callback_xml_complete_dbus__cb() +\n");
	service_dbus_ctxt_t *ctxt = (service_dbus_ctxt_t*) udata;
	sdp_record_t *rec;
	int scanned;
	GString *result;
	DBusMessage *reply, *msg;
	DBusMessageIter iter, array_iter;

	reply = dbus_message_new_method_return(ctxt->msg);

	if (err == 0xffff)
	{
		error("Invalid session!");
		goto failed;
	}
	if (type == SDP_ERROR_RSP)
	{
		error("SDP_ERROR_RSP!");
		goto failed;
	}

	/* check response PDU ID */
	if (type != SDP_SVC_ATTR_RSP)
	{
		error("SDP_SVC_ATTR_RSP!");
		goto failed;
	}

	rec = (sdp_record_t *)sdp_extract_pdu_safe(rsp, size, &scanned);
	if (rec == NULL || size != scanned)
	{
		error("Invalid service record!");
		goto failed;
	}

	result = g_string_new(NULL);

	DBG("size %d\n", size);

	convert_sdp_record_to_xml(rec, result, (void *) g_string_append);

	sdp_record_free(rec);

	info( "XML Converted buffer length %d\n", result->len);

	dbus_message_append_args(reply,
			DBUS_TYPE_STRING, result->str,
			DBUS_TYPE_INVALID);
	g_string_free(result, TRUE);

	dbus_connection_send(ctxt->conn, reply, NULL);
	dbus_message_unref(reply);

	if(NULL != ctxt)
	{
		free(ctxt);
		ctxt = NULL;
	}
	__io_finished = 1;
	info("__sdp_callback_xml_complete_dbus__cb-\n");
	return;
failed:

	dbus_connection_send(ctxt->conn, reply, NULL);
	dbus_message_unref(reply);

	if(ctxt != NULL)
	{
		dbus_connection_unref(ctxt->conn);
		dbus_message_unref(ctxt->msg);
		free(ctxt);
		ctxt = NULL;

	}
}

static DBusMessage * siso_adapter_get_remote_svc_xml(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	info( "siso_adapter_get_remote_svc_xml() +\n");
	sdp_session_t *session;
	sdp_list_t *attrids;
	uint32_t range = 0x0000ffff;
	const char *dst;
	uint32_t handle;
	int err;
	bdaddr_t  bdadd;
	DBusMessage *reply;
	service_dbus_ctxt_t* ctxt = NULL;

	if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &dst,
			DBUS_TYPE_UINT32, &handle,
			DBUS_TYPE_INVALID))
		return btd_error_invalid_args(msg);

	str2ba(dst, &bdadd);

	session = sdp_connect(BDADDR_ANY, &bdadd, 0);
	if (!session)
	{
	            info("Can't connect to SDP service\n");
	            goto error_done;
	}
	ctxt = malloc(sizeof(service_dbus_ctxt_t));

	if(NULL == ctxt)
	{
		info("!!!!!Memory allocation failed for ctxt\n");
		goto error_done;
	}

	ctxt->conn = conn;
	ctxt->msg = dbus_message_ref(msg);

	info( "Before sdp_set_notify +++\n");
	if (sdp_set_notify(session, __sdp_callback_xml_complete_dbus__cb, ctxt) < 0)
	{
                sdp_close(session);
		goto error_done;
	}

	attrids = sdp_list_append(NULL, &range);
	if (sdp_service_attr_async(session, handle, SDP_ATTR_REQ_RANGE, attrids) < 0)
	{
		sdp_list_free(attrids, NULL);
                sdp_close(session);
		goto error_done;
	}
	sdp_list_free(attrids, NULL);
	__io_finished = 0;
	    while (!__io_finished)
	    {
	                printf(". ");
	                sdp_process(session);
	    }

	    sdp_close(session);
	    return 0;

error_done:
	if(ctxt != NULL)
	{
		free(ctxt);
		ctxt = NULL;
	}
	info("-\n");

	return btd_error_invalid_args(msg);

}

static void sdp_get_pdu_by_handle_dbus_rsp(uint8_t type, uint16_t err,
			uint8_t *rsp, size_t size, void *udata)
{
	sdp_record_t *rec;
	int scanned;
	DBusMessage *reply, *msg;
	DBusMessageIter iter, array_iter;
	service_dbus_ctxt_t* ctxt = NULL;
	info("sdp_get_pdu_by_handle_rsp+\n");

	if (err == 0xffff)
	{
		error("Invalid session!");
		goto failed;
	}

	if (type == SDP_ERROR_RSP)
	{
		error("SDP_ERROR_RSP!");
		goto failed;
	}

	/* check response PDU ID */
	if (type != SDP_SVC_ATTR_RSP)
	{
		error("SDP_SVC_ATTR_RSP!");
		goto failed;
	}

	ctxt = (service_dbus_ctxt_t*)udata;
	rec = (sdp_record_t *)sdp_extract_pdu_safe(rsp, size, &scanned);
	if (rec == NULL || size != scanned) {
		error("Invalid service record!");
		goto failed;
	}

	//sdp_store_record(ctxt->src, ctxt->dst, rec->handle, rsp, size);

	sdp_record_free(rec);

	DBG("size %d", size);
	dbus_message_iter_append_fixed_array(ctxt->array_iter,
			DBUS_TYPE_BYTE, &rsp, size);

	__io_finished = 1;
	info("sdp_get_pdu_by_handle_rsp -\n");

	return;

failed:
	info("Failed to get the service record");

	__io_finished = 1;
	return;
}

static DBusMessage *adapter_get_remote_svc(DBusConnection *conn,
					DBusMessage *msg, void *data)
{

	//sdp_session_t *session;
	sdp_list_t *attrids;
	uint32_t range = 0x0000ffff;
	const char *dst;
	uint32_t handle;
	bdaddr_t bdadd;
	int struct_len;
	service_dbus_ctxt_t* ctxt = NULL;
#if 0 //Service search
#else
	DBusMessage *reply;
	DBusMessageIter iter, array_iter;
#endif
	info("...__get_sdp_record_by_handle_send_response....\n");

	if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &dst,
			DBUS_TYPE_UINT32, &handle,
			DBUS_TYPE_INVALID))
		return btd_error_invalid_args(msg);

	info("address %s, handle %d", dst, handle);

	str2ba(dst, &bdadd);

	if(NULL == g_cached_session)
	{
		g_cached_session = sdp_connect(BDADDR_ANY, &bdadd, 0);
		if (!g_cached_session)
		{
			info("Can't connect to SDP service\n");
			goto error_done;
			//break;
		}
	}
	info( "Before sdp_set_notify,,,,,......\n");

	ctxt = malloc(sizeof(service_dbus_ctxt_t));

	if(NULL == ctxt)
	{
		goto error_done;
	}
	__io_finished = 0;
	ctxt->conn = conn;
	ctxt->msg = dbus_message_ref(msg);
	reply = dbus_message_new_method_return(ctxt->msg);
	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
			DBUS_TYPE_BYTE_AS_STRING, &array_iter);

	ctxt->array_iter = &array_iter;
	if (sdp_set_notify(g_cached_session, sdp_get_pdu_by_handle_dbus_rsp, ctxt) < 0)
		goto error_done;


	attrids = sdp_list_append(NULL, &range);

	if (sdp_service_attr_async(g_cached_session, handle,
				SDP_ATTR_REQ_RANGE, attrids) < 0) {
		sdp_list_free(attrids, NULL);
		goto error_done;
	}

	sdp_list_free(attrids, NULL);

	while (!__io_finished)
	{
		printf(". ");
		sdp_process(g_cached_session);
	}
	dbus_message_iter_close_container(&iter, &array_iter);

	free(ctxt);
	return reply;


error_done:
	if(ctxt != NULL)
	{
		free(ctxt);
		ctxt =  NULL;
	}
	info("__get_sdp_record_by_handle_send_response failed");

	return btd_error_invalid_args(msg);

}

static DBusMessage *close_sdp_session(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	info("close_sdp_session");
	if(NULL != g_cached_session)
	{
		info("closing sdp cached session");
		sdp_close(g_cached_session);
		g_cached_session = NULL;
	}

	return dbus_message_new_method_return(msg);
}


static GDBusMethodTable service_methods[] = {
	{ "AddRecord",		"s",	"u",	add_service_record	},
	{ "AddServiceRecord",		"ay",	"u",	siso_add_service_record_pdu	},
	{ "RemoveServiceRecord",	"u",	"",	siso_remove_service_record	},
	{"GetRemoteServiceHandles", "ss",    "ay",	siso_get_remote_service_handles, G_DBUS_METHOD_FLAG_ASYNC},
//	{"GetRemoteServiceHandles", "ss",    "ay",	get_remote_service_handles, G_DBUS_METHOD_FLAG_ASYNC},
	{ "GetRemoteServiceRecord",		"su",	"ay", adapter_get_remote_svc, G_DBUS_METHOD_FLAG_ASYNC},
	{ "CloseSdpSession",	"",	"",	close_sdp_session},
	{ "GetRemoteServiceRecordAsXML",	"su",	"s",	siso_adapter_get_remote_svc_xml,	G_DBUS_METHOD_FLAG_ASYNC	},
	{ "UpdateRecord",	"us",	"",	update_service_record	},
	{ "RemoveRecord",	"u",	"",	remove_service_record	},
	{ "RequestAuthorization","su",	"",	request_authorization,
						G_DBUS_METHOD_FLAG_ASYNC},
	{ "CancelAuthorization", "",	"",	cancel_authorization	},
	{ }
};
#else
static GDBusMethodTable service_methods[] = {
	{ "AddRecord",		"s",	"u",	add_service_record	},
	{ "UpdateRecord",	"us",	"",	update_service_record	},
	{ "RemoveRecord",	"u",	"",	remove_service_record	},
	{ "RequestAuthorization","su",	"",	request_authorization,
						G_DBUS_METHOD_FLAG_ASYNC},
	{ "CancelAuthorization", "",	"",	cancel_authorization	},
	{ }
};

#endif
static void path_unregister(void *data)
{
	struct service_adapter *serv_adapter = data;
	GSList *l, *next = NULL;

	for (l = serv_adapter->records; l != NULL; l = next) {
		struct record_data *user_record = l->data;

		next = l->next;

		g_dbus_remove_watch(connection, user_record->listener_id);
		exit_callback(connection, user_record);
	}

	g_free(serv_adapter);
}

static int register_interface(const char *path, struct btd_adapter *adapter)
{
	info("\nEntering register_interface()... +\n");
	struct service_adapter *serv_adapter;

	DBG("path %s", path);

	serv_adapter = g_try_new0(struct service_adapter, 1);
	if (serv_adapter == NULL)
		return -ENOMEM;

	serv_adapter->adapter = adapter;
	serv_adapter->pending_list = NULL;

	if (g_dbus_register_interface(connection, path, SERVICE_INTERFACE,
				service_methods, NULL, NULL, serv_adapter,
						path_unregister) == FALSE) {
		error("D-Bus failed to register %s interface",
							SERVICE_INTERFACE);
		g_free(serv_adapter);
		return -EIO;
	}

	DBG("Registered interface %s on path %s", SERVICE_INTERFACE, path);

	if (serv_adapter->adapter == NULL)
		serv_adapter_any = serv_adapter;

	return 0;
}

static void unregister_interface(const char *path)
{
	DBG("path %s", path);

	g_dbus_unregister_interface(connection, path, SERVICE_INTERFACE);
}

static int service_probe(struct btd_adapter *adapter)
{
	register_interface(adapter_get_path(adapter), adapter);

	return 0;
}

static void service_remove(struct btd_adapter *adapter)
{
	unregister_interface(adapter_get_path(adapter));
}

static struct btd_adapter_driver service_driver = {
	.name	= "service",
	.probe	= service_probe,
	.remove	= service_remove,
};

static const char *any_path;

static int service_init(void)
{
	int err;

	connection = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (connection == NULL)
		return -EIO;

	any_path = btd_adapter_any_request_path();
	if (any_path != NULL) {
		if (register_interface(any_path, NULL) < 0) {
			btd_adapter_any_release_path();
			any_path = NULL;
		}
	}

	err = btd_register_adapter_driver(&service_driver);
	if (err < 0) {
		dbus_connection_unref(connection);
		return err;
	}

	return 0;
}

static void service_exit(void)
{
	btd_unregister_adapter_driver(&service_driver);

	if (any_path != NULL) {
		unregister_interface(any_path);

		btd_adapter_any_release_path();
		any_path = NULL;
	}

	dbus_connection_unref(connection);
}

BLUETOOTH_PLUGIN_DEFINE(service, VERSION,
		BLUETOOTH_PLUGIN_PRIORITY_HIGH, service_init, service_exit)

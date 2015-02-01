/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2014  Instituto Nokia de Tecnologia - INdT
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

#include <stdint.h>
#include <errno.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <gdbus/gdbus.h>

#include "adapter.h"
#include "device.h"
#include "lib/uuid.h"
#include "dbus-common.h"
#include "log.h"

#include "error.h"
#include "attrib/gattrib.h"
#include "attrib/att.h"
#include "attrib/gatt.h"
#include "gatt.h"
#include "gatt-dbus.h"

#define GATT_MGR_IFACE		"org.bluez.GattManager1"
#define GATT_SERVICE_IFACE	"org.bluez.GattService1"
#define GATT_CHR_IFACE		"org.bluez.GattCharacteristic1"
#define GATT_DESCRIPTOR_IFACE	"org.bluez.GattDescriptor1"

struct external_service {
	char *owner;
	char *path;
	DBusMessage *reg;
	GDBusClient *client;
	GSList *proxies;
	struct btd_attribute *service;
};

struct proxy_write_data {
	btd_attr_write_result_t result_cb;
#ifdef __TIZEN_PATCH__
	uint8_t data[0];
	size_t len;
#endif
	void *user_data;
};

/*
 * Attribute to Proxy hash table. Used to map incoming
 * ATT operations to its external characteristic proxy.
 */
static GHashTable *proxy_hash;

static GSList *external_services;


static int external_service_path_cmp(gconstpointer a, gconstpointer b)
{
	const struct external_service *esvc = a;
	const char *path = b;

	return g_strcmp0(esvc->path, path);
}

static gboolean external_service_destroy(void *user_data)
{
	struct external_service *esvc = user_data;

	g_dbus_client_unref(esvc->client);

	if (esvc->reg)
		dbus_message_unref(esvc->reg);

	g_free(esvc->owner);
	g_free(esvc->path);
	g_free(esvc);

	return FALSE;
}

static void external_service_free(void *user_data)
{
	struct external_service *esvc = user_data;

	/*
	 * Set callback to NULL to avoid potential race condition
	 * when calling remove_service and GDBusClient unref.
	 */
	g_dbus_client_set_disconnect_watch(esvc->client, NULL, NULL);

	external_service_destroy(user_data);
}


static int proxy_path_cmp(gconstpointer a, gconstpointer b)
{
	GDBusProxy *proxy1 = (GDBusProxy *) a;
	GDBusProxy *proxy2 = (GDBusProxy *) b;
	const char *path1 = g_dbus_proxy_get_path(proxy1);
	const char *path2 = g_dbus_proxy_get_path(proxy2);

	return g_strcmp0(path1, path2);
}

static void remove_service(DBusConnection *conn, void *user_data)
{
	struct external_service *esvc = user_data;

	external_services = g_slist_remove(external_services, esvc);

	if (esvc->service)
		btd_gatt_remove_service(esvc->service);

	/*
	 * Do not run in the same loop, this may be a disconnect
	 * watch call and GDBusClient should not be destroyed.
	 */
	g_idle_add(external_service_destroy, esvc);
}

static uint8_t flags_string2int(const char *proper)
{
	uint8_t value;

	/* Regular Properties: See core spec 4.1 page 2183 */
	if (!strcmp("broadcast", proper))
		value = GATT_CHR_PROP_BROADCAST;
	else if (!strcmp("read", proper))
		value = GATT_CHR_PROP_READ;
	else if (!strcmp("write-without-response", proper))
		value = GATT_CHR_PROP_WRITE_WITHOUT_RESP;
	else if (!strcmp("write", proper))
		value = GATT_CHR_PROP_WRITE;
	else if (!strcmp("notify", proper))
		value = GATT_CHR_PROP_NOTIFY;
	else if (!strcmp("indicate", proper))
		value = GATT_CHR_PROP_INDICATE;
	else if (!strcmp("authenticated-signed-writes", proper))
		value = GATT_CHR_PROP_AUTH;
	else
		value = 0;

	/* TODO: Extended properties. Ref core spec 4.1 page 2185  */

	return value;
}


static uint8_t flags_get_bitmask(DBusMessageIter *iter)
{
	DBusMessageIter istr;
	uint8_t propmask = 0, prop;
	const char *str;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		goto fail;

	dbus_message_iter_recurse(iter, &istr);

	do {
		if (dbus_message_iter_get_arg_type(&istr) != DBUS_TYPE_STRING)
			goto fail;

		dbus_message_iter_get_basic(&istr, &str);
		prop = flags_string2int(str);
		if (!prop)
			goto fail;

		propmask |= prop;
	} while (dbus_message_iter_next(&istr));

	return propmask;

fail:
	error("Characteristic Flags: Invalid argument!");

	return 0;
}

static void proxy_added(GDBusProxy *proxy, void *user_data)
{
	struct external_service *esvc = user_data;
	const char *interface, *path;

	interface = g_dbus_proxy_get_interface(proxy);
	path = g_dbus_proxy_get_path(proxy);

	if (!g_str_has_prefix(path, esvc->path))
		return;

	if (g_strcmp0(interface, GATT_CHR_IFACE) != 0 &&
			g_strcmp0(interface, GATT_SERVICE_IFACE) != 0 &&
			g_strcmp0(interface, GATT_DESCRIPTOR_IFACE) != 0)
		return;

	DBG("path %s iface %s", path, interface);

	/*
	 * Object path follows a hierarchical organization. Add the
	 * proxies sorted by path helps the logic to register the
	 * object path later.
	 */
	esvc->proxies = g_slist_insert_sorted(esvc->proxies, proxy,
							proxy_path_cmp);
}

static void proxy_removed(GDBusProxy *proxy, void *user_data)
{
	struct external_service *esvc = user_data;
	const char *interface, *path;

	interface = g_dbus_proxy_get_interface(proxy);
	path = g_dbus_proxy_get_path(proxy);

	DBG("path %s iface %s", path, interface);

	esvc->proxies = g_slist_remove(esvc->proxies, proxy);
}

#ifdef __TIZEN_PATCH__
static int register_external_service(struct external_service *esvc,
							GDBusProxy *proxy)
#else
static int register_external_service(const struct external_service *esvc,
							GDBusProxy *proxy)
#endif
{
	DBusMessageIter iter;
	const char *str, *path, *iface;
	bt_uuid_t uuid;

	path = g_dbus_proxy_get_path(proxy);
	iface = g_dbus_proxy_get_interface(proxy);
	if (g_strcmp0(esvc->path, path) != 0 ||
			g_strcmp0(iface, GATT_SERVICE_IFACE) != 0)
		return -EINVAL;

	if (!g_dbus_proxy_get_property(proxy, "UUID", &iter))
		return -EINVAL;

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
		return -EINVAL;

	dbus_message_iter_get_basic(&iter, &str);

	if (bt_string_to_uuid(&uuid, str) < 0)
		return -EINVAL;

#ifdef __TIZEN_PATCH__
	esvc->service = btd_gatt_add_service(&uuid);
	if (esvc->service == NULL)
#else
	if (btd_gatt_add_service(&uuid) == NULL)
#endif
		return -EINVAL;
	return 0;
}

#ifdef __TIZEN_PATCH__
static void proxy_prop_changed(GDBusProxy *proxy, const char *name,
					DBusMessageIter *iter, void *user_data)
{
	DBusMessageIter iter_uuid, array;
	const char *str;
	bt_uuid_t uuid;
	uint8_t *value;
	size_t len;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return;

	dbus_message_iter_recurse(iter, &array);
	dbus_message_iter_get_fixed_array(&array, &value, &len);

	if (!g_dbus_proxy_get_property(proxy, "UUID", &iter_uuid))
		return;

	if (dbus_message_iter_get_arg_type(&iter_uuid) != DBUS_TYPE_STRING)
		return;

	dbus_message_iter_get_basic(&iter_uuid, &str);

	if (bt_string_to_uuid(&uuid, str) < 0)
		return;

	dbus_message_iter_next(iter);
	/* Update the charateristics in attrib database and
	 * send notification or indication if CCC is set */
	btd_gatt_update_char(&uuid, value, len);
}
#endif

#ifdef __TIZEN_PATCH__
static uint8_t proxy_read_cb(struct btd_attribute *attr,
				btd_attr_read_result_t result, void *user_data)
#else
static void proxy_read_cb(struct btd_attribute *attr,
				btd_attr_read_result_t result, void *user_data)
#endif
{
	DBusMessageIter iter, array;
	GDBusProxy *proxy;
	uint8_t *value;
	int len;

	/*
	 * Remote device is trying to read the informed attribute,
	 * "Value" should be read from the proxy. GDBusProxy tracks
	 * properties changes automatically, it is not necessary to
	 * get the value directly from the GATT server.
	 */
	proxy = g_hash_table_lookup(proxy_hash, attr);
	if (!proxy) {
#ifdef __TIZEN_PATCH__
		return result(-ENOENT, NULL, 0, user_data);
#else
		result(-ENOENT, NULL, 0, user_data);
		return;
#endif
	}

	if (!g_dbus_proxy_get_property(proxy, "Value", &iter)) {
		/* Unusual situation, read property will checked earlier */
#ifdef __TIZEN_PATCH__
		return result(-EPERM, NULL, 0, user_data);
#else
		result(-EPERM, NULL, 0, user_data);
		return;
#endif
	}

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
		DBG("External service inconsistent!");
#ifdef __TIZEN_PATCH__
		return result(-EPERM, NULL, 0, user_data);
#else
		result(-EPERM, NULL, 0, user_data);
		return;
#endif
	}

	dbus_message_iter_recurse(&iter, &array);
	dbus_message_iter_get_fixed_array(&array, &value, &len);

	DBG("attribute: %p read %d bytes", attr, len);

#ifdef __TIZEN_PATCH__
	return result(0, value, len, user_data);
#else
	result(0, value, len, user_data);
#endif
}

static void proxy_write_reply(const DBusError *derr, void *user_data)
{
	struct proxy_write_data *wdata = user_data;
	int err;

	/*
	 * Security requirements shall be handled by the core. If external
	 * applications returns an error, the reasons will be restricted to
	 * invalid argument or application specific errors.
	 */

	if (!dbus_error_is_set(derr)) {
		err = 0;
		goto done;
	}

	DBG("Write reply: %s", derr->message);

	if (dbus_error_has_name(derr, DBUS_ERROR_NO_REPLY))
		err = -ETIMEDOUT;
	else if (dbus_error_has_name(derr, ERROR_INTERFACE ".InvalidArguments"))
		err = -EINVAL;
	else
		err = -EPROTO;

done:
#ifdef __TIZEN_PATCH__
	if (wdata && wdata->result_cb) {
		void *ptr = (uint8_t *)wdata->data;
		uint8_t value_buf[wdata->len];
		memcpy(value_buf, ptr, wdata->len);
		wdata->result_cb(err, value_buf, wdata->len, wdata->user_data);
	}
#else
	if (wdata && wdata->result_cb)
		wdata->result_cb(err, wdata->user_data);
#endif
}

#ifdef __TIZEN_PATCH__
static uint8_t proxy_write_cb(struct btd_attribute *attr,
					const uint8_t *value, size_t len,
					btd_attr_write_result_t result,
					void *user_data)
#else
static void proxy_write_cb(struct btd_attribute *attr,
					const uint8_t *value, size_t len,
					btd_attr_write_result_t result,
					void *user_data)
#endif
{
	GDBusProxy *proxy;

	proxy = g_hash_table_lookup(proxy_hash, attr);
	if (!proxy) {
#ifdef __TIZEN_PATCH__
		return result(-ENOENT, NULL, 0, user_data);
#else
		result(-ENOENT, user_data);
		return;
#endif
	}

	/*
	 * "result" callback defines if the core wants to receive the
	 * operation result, allowing to select ATT Write Request or Write
	 * Command. Descriptors requires Write Request operation. For
	 * Characteristics, the implementation will define which operations
	 * are allowed based on the properties/flags.
	 * TODO: Write Long Characteristics/Descriptors.
	 */

	if (result) {
		struct proxy_write_data *wdata;

		wdata = g_new0(struct proxy_write_data, 1);
		wdata->result_cb = result;
#ifdef __TIZEN_PATCH__
		memcpy(wdata->data, value, len);
		wdata->len = len;
#endif
		wdata->user_data = user_data;

#ifdef __TIZEN_PATCH__
		btd_gatt_set_notify_indicate_flag(attr, FALSE);
#endif

		if (!g_dbus_proxy_set_property_array(proxy, "Value",
						DBUS_TYPE_BYTE, value, len,
						proxy_write_reply,
						wdata, g_free)) {
			g_free(wdata);
#ifdef __TIZEN_PATCH__
			return result(-ENOENT, NULL, 0, user_data);
#else
			result(-ENOENT, user_data);
			return;
#endif
		}
	} else {
		/*
		 * Caller is not interested in the Set method call result.
		 * This flow implements the ATT Write Command scenario, where
		 * the remote doesn't receive ATT response.
		 */
		g_dbus_proxy_set_property_array(proxy, "Value", DBUS_TYPE_BYTE,
						value, len, proxy_write_reply,
						NULL, NULL);
	}

	DBG("Server: Write attribute callback %s",
					g_dbus_proxy_get_path(proxy));
#ifdef __TIZEN_PATCH__
		return result(0, value, len, user_data);
#endif

}

static int add_char(GDBusProxy *proxy, const bt_uuid_t *uuid)
{
	DBusMessageIter iter;
	struct btd_attribute *attr;
	btd_attr_write_t write_cb;
	btd_attr_read_t read_cb;
	uint8_t propmask = 0;

	/*
	 * Optional property. If is not informed, read and write
	 * procedures will be allowed. Upper-layer should handle
	 * characteristic requirements.
	 */
	if (g_dbus_proxy_get_property(proxy, "Flags", &iter))
		propmask = flags_get_bitmask(&iter);
	else
		propmask = GATT_CHR_PROP_WRITE_WITHOUT_RESP
						| GATT_CHR_PROP_WRITE
						| GATT_CHR_PROP_READ;
	if (!propmask)
		return -EINVAL;

	if (propmask & GATT_CHR_PROP_READ)
		read_cb = proxy_read_cb;
	else
		read_cb = NULL;

	if (propmask & (GATT_CHR_PROP_WRITE | GATT_CHR_PROP_WRITE_WITHOUT_RESP))
		write_cb = proxy_write_cb;
	else
		write_cb = NULL;

	attr = btd_gatt_add_char(uuid, propmask, read_cb, write_cb);
	if (!attr)
		return -ENOMEM;

	g_hash_table_insert(proxy_hash, attr, g_dbus_proxy_ref(proxy));

	return 0;
}

static int add_char_desc(GDBusProxy *proxy, const bt_uuid_t *uuid)
{
	struct btd_attribute *attr;

	attr = btd_gatt_add_char_desc(uuid, proxy_read_cb, proxy_write_cb);
	if (!attr)
		return -ENOMEM;

	g_hash_table_insert(proxy_hash, attr, g_dbus_proxy_ref(proxy));

	return 0;
}

static int register_external_characteristics(GSList *proxies)

{
	GSList *list;

#ifdef __TIZEN_PATCH__
	proxy_hash = g_hash_table_new_full(g_str_hash, g_direct_equal,
					 NULL, g_free);
#endif

	for (list = proxies; list; list = g_slist_next(list)) {
		GDBusProxy *proxy = list->data;
		DBusMessageIter iter;
		bt_uuid_t uuid;
		const char *path, *iface, *str;
		int ret;

		/* Mandatory property */
		if (!g_dbus_proxy_get_property(proxy, "UUID", &iter))
			return -EINVAL;

		if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
			return -EINVAL;

		dbus_message_iter_get_basic(&iter, &str);

		if (bt_string_to_uuid(&uuid, str) < 0)
			return -EINVAL;

		iface = g_dbus_proxy_get_interface(proxy);
		path = g_dbus_proxy_get_path(proxy);

		if (!strcmp(GATT_CHR_IFACE, iface))
			ret = add_char(proxy, &uuid);
		else
			ret = add_char_desc(proxy, &uuid);

		if (ret < 0)
			return ret;

		DBG("Added GATT: %s (%s)", path, str);
	}

	return 0;
}


static void client_ready(GDBusClient *client, void *user_data)
{
	struct external_service *esvc = user_data;
	GDBusProxy *proxy;
	DBusConnection *conn = btd_get_dbus_connection();
	DBusMessage *reply;

	if (!esvc->proxies)
		goto fail;

	proxy = esvc->proxies->data;
	if (register_external_service(esvc, proxy) < 0)
		goto fail;

	if (register_external_characteristics(g_slist_next(esvc->proxies)) < 0)
		goto fail;

	DBG("Added GATT service %s", esvc->path);

	reply = dbus_message_new_method_return(esvc->reg);
	g_dbus_send_message(conn, reply);

	dbus_message_unref(esvc->reg);
	esvc->reg = NULL;
#ifdef __TIZEN_PATCH__
	btd_gatt_update_attr_db();
#endif

	return;

fail:
	error("Could not register external service: %s", esvc->path);

	/*
	 * Set callback to NULL to avoid potential race condition
	 * when calling remove_service and GDBusClient unref.
	 */
	g_dbus_client_set_disconnect_watch(esvc->client, NULL, NULL);

	remove_service(conn, esvc);

	reply = btd_error_invalid_args(esvc->reg);
	g_dbus_send_message(conn, reply);
}

static struct external_service *external_service_new(DBusConnection *conn,
					DBusMessage *msg, const char *path)
{
	struct external_service *esvc;
	GDBusClient *client;
	const char *sender = dbus_message_get_sender(msg);

	client = g_dbus_client_new(conn, sender, "/");
	if (client == NULL)
		return NULL;

	esvc = g_new0(struct external_service, 1);

	esvc = g_new0(struct external_service, 1);
	esvc->owner = g_strdup(sender);
	esvc->reg = dbus_message_ref(msg);
	esvc->client = client;
	esvc->path = g_strdup(path);

	g_dbus_client_set_disconnect_watch(client,
					remove_service, esvc);

	g_dbus_client_set_proxy_handlers(client, proxy_added,
					proxy_removed,
#ifdef __TIZEN_PATCH__
					proxy_prop_changed, esvc);
#else
					NULL, esvc);
#endif

	g_dbus_client_set_ready_watch(client, client_ready, esvc);

	return esvc;
}

static DBusMessage *register_service(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	struct external_service *esvc;
	DBusMessageIter iter;
	const char *path;

	if (!dbus_message_iter_init(msg, &iter))
		return btd_error_invalid_args(msg);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_OBJECT_PATH)
		return btd_error_invalid_args(msg);

	dbus_message_iter_get_basic(&iter, &path);

	if (g_slist_find_custom(external_services, path, external_service_path_cmp))
		return btd_error_already_exists(msg);

	esvc = external_service_new(conn, msg, path);
	if (esvc == NULL)
		return btd_error_failed(msg, "Not enough resources");

	external_services = g_slist_prepend(external_services, esvc);

	DBG("New app %p: %s", esvc, path);

	return NULL;
}

static DBusMessage *unregister_service(DBusConnection *conn,
					DBusMessage *msg, void *user_data)
{
	struct external_service *esvc;
	DBusMessageIter iter;
	const char *path;
	GSList *list;

	if (!dbus_message_iter_init(msg, &iter))
		return btd_error_invalid_args(msg);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_OBJECT_PATH)
		return btd_error_invalid_args(msg);

	dbus_message_iter_get_basic(&iter, &path);

	list = g_slist_find_custom(external_services, path,
						external_service_path_cmp);
	if (!list)
		return btd_error_does_not_exist(msg);

	esvc = list->data;

#ifdef __TIZEN_PATCH__
	if (strcmp(dbus_message_get_sender(msg), esvc->owner))
#else
	if (!strcmp(dbus_message_get_sender(msg), esvc->owner))
#endif
		return btd_error_does_not_exist(msg);

	/*
	 * Set callback to NULL to avoid potential race condition
	 * when calling remove_service and GDBusClient unref.
	 */
	g_dbus_client_set_disconnect_watch(esvc->client, NULL, NULL);

	remove_service(conn, esvc);

	return dbus_message_new_method_return(msg);
}

static const GDBusMethodTable methods[] = {
	{ GDBUS_EXPERIMENTAL_ASYNC_METHOD("RegisterService",
				GDBUS_ARGS({ "service", "o"},
						{ "options", "a{sv}"}),
				NULL, register_service) },
	{ GDBUS_EXPERIMENTAL_METHOD("UnregisterService",
				GDBUS_ARGS({"service", "o"}),
				NULL, unregister_service) },
	{ }
};

gboolean gatt_dbus_manager_register(void)
{
	return g_dbus_register_interface(btd_get_dbus_connection(),
					"/org/bluez", GATT_MGR_IFACE,
					methods, NULL, NULL, NULL, NULL);
}

void gatt_dbus_manager_unregister(void)
{
	g_dbus_unregister_interface(btd_get_dbus_connection(), "/org/bluez",
							GATT_MGR_IFACE);
}

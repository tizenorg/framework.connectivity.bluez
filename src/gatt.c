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

#include <glib.h>
#include <stdbool.h>
#ifdef __TIZEN_PATCH__
#include <errno.h>
#endif

#include "log.h"
#include "lib/uuid.h"
#include "attrib/att.h"
#include "src/shared/util.h"

#include "gatt-dbus.h"
#include "gatt.h"
#ifdef __TIZEN_PATCH__
#include "adapter.h"
#include "attrib/gatt-service.h"
#include "attrib/att-database.h"
#include "attrib/gattrib.h"
#include "attrib/gatt.h"
#include "src/attrib-server.h"
#include "src/device.h"
#endif

/* Common GATT UUIDs */
static const bt_uuid_t primary_uuid  = { .type = BT_UUID16,
					.value.u16 = GATT_PRIM_SVC_UUID };

static const bt_uuid_t chr_uuid = { .type = BT_UUID16,
					.value.u16 = GATT_CHARAC_UUID };
#ifdef __TIZEN_PATCH__
static const bt_uuid_t chr_uuid32 = { .type = BT_UUID32,
					.value.u16 = GATT_CHARAC_UUID };
static const bt_uuid_t chr_uuid128 = { .type = BT_UUID128,
					.value.u16 = GATT_CHARAC_UUID };


#define ATT_CHAR_READ_VALUE 0x01
#define ATT_CHAR_RD_WRT_CB 0x02
#define GATT_TYPE_PRIM_SVC 0x01
#define GATT_TYPE_CHARAC_SVC 0x02
#define GATT_TYPE_CHARAC_VALUE 0x03
#define GATT_TYPE_DESC_CCC 0x04
#define GATT_TYPE_DESC_USR 0x05
#endif

struct btd_attribute {
	uint16_t handle;
#ifdef __TIZEN_PATCH__
	uint16_t attr_start_handle;
	uint16_t attr_end_handle;
	bool notify_indicate;
#endif
	bt_uuid_t type;
	btd_attr_read_t read_cb;
	btd_attr_write_t write_cb;
	uint16_t value_len;
	uint8_t value[0];
};

#ifdef __TIZEN_PATCH__
struct pending_hndl {
	uint16_t handle;
	uint16_t type;
	struct pending_hndl *next;
};

static struct pending_hndl *hndl_list_head = NULL;
static struct pending_hndl *hndl_list = NULL;
#endif

static GList *local_attribute_db;
static uint16_t next_handle = 0x0001;

static inline void put_uuid_le(const bt_uuid_t *src, void *dst)
{
	if (src->type == BT_UUID16)
		put_le16(src->value.u16, dst);
	else if (src->type == BT_UUID32)
		put_le32(src->value.u32, dst);
	else
		/* Convert from 128-bit BE to LE */
		bswap_128(&src->value.u128, dst);
}

/*
 * Helper function to create new attributes containing constant/static values.
 * eg: declaration of services/characteristics, and characteristics with
 * fixed values.
 */
static struct btd_attribute *new_const_attribute(const bt_uuid_t *type,
							const uint8_t *value,
							uint16_t len)
{
	struct btd_attribute *attr;

	attr = malloc0(sizeof(struct btd_attribute) + len);
	if (!attr)
		return NULL;

	attr->type = *type;
	memcpy(&attr->value, value, len);
	attr->value_len = len;

	return attr;
}

static struct btd_attribute *new_attribute(const bt_uuid_t *type,
						btd_attr_read_t read_cb,
						btd_attr_write_t write_cb)
{
	struct btd_attribute *attr;

	attr = new0(struct btd_attribute, 1);
	if (!attr)
		return NULL;

	attr->type = *type;
	attr->read_cb = read_cb;
	attr->write_cb = write_cb;
#ifdef __TIZEN_PATCH__
	attr->notify_indicate = TRUE;
#endif

	return attr;
}

static bool is_service(const struct btd_attribute *attr)
{
	if (attr->type.type != BT_UUID16)
		return false;

	if (attr->type.value.u16 == GATT_PRIM_SVC_UUID ||
			attr->type.value.u16 == GATT_SND_SVC_UUID)
		return true;

	return false;
}

static int local_database_add(uint16_t handle, struct btd_attribute *attr)
{
	attr->handle = handle;

	local_attribute_db = g_list_append(local_attribute_db, attr);

	return 0;
}

#ifdef __TIZEN_PATCH__
static int attr_uuid_cmp(gconstpointer a, gconstpointer b)
{
	const struct btd_attribute *attrib1 = a;
	const bt_uuid_t *uuid = b;

	if (attrib1->type.value.u16 != GATT_PRIM_SVC_UUID) {
		return bt_uuid_cmp(&attrib1->type, uuid);
	} else {
		bt_uuid_t prim_uuid;
		prim_uuid = att_get_uuid(attrib1->value, attrib1->value_len);

		return bt_uuid_cmp(&prim_uuid, uuid);
	}
}

static int handle_cmp(gconstpointer a, gconstpointer b)
{
	const struct btd_attribute *attrib = a;
	uint16_t handle = GPOINTER_TO_UINT(b);

	return attrib->handle - handle;
}

static int attribute_cmp(gconstpointer a1, gconstpointer a2)
{
	const struct btd_attribute *attrib1 = a1;
	const struct btd_attribute *attrib2 = a2;

	return attrib1->handle - attrib2->handle;
}

static void store_start_end_handle(bt_uuid_t *uuid, uint16_t start_handle,
						uint16_t end_handle)
{
	GList *l;
	struct btd_attribute *attrib;

	l = g_list_find_custom(local_attribute_db, GUINT_TO_POINTER(uuid),
							attr_uuid_cmp);

	if (!l)
		return;

	attrib = l->data;
	attrib->attr_start_handle = start_handle;
	attrib->attr_end_handle = end_handle;

	g_list_insert_sorted (local_attribute_db, attrib, attribute_cmp);

	return;
}

static void get_start_end_handle(bt_uuid_t *uuid, uint16_t *start_handle,
						uint16_t *end_handle)
{
	GList *l;
	struct btd_attribute *attrib;

	l = g_list_find_custom(local_attribute_db, GUINT_TO_POINTER(uuid),
							attr_uuid_cmp);


	if (!l)
		return;

	attrib = l->data;
	*start_handle = attrib->attr_start_handle;
	*end_handle = attrib->attr_end_handle;
}

static struct btd_attribute *find_local_attr(bt_uuid_t *uuid)
{
	GList *l;

	l = g_list_find_custom(local_attribute_db,
				GUINT_TO_POINTER(uuid),
				attr_uuid_cmp);

	if (!l)
		return NULL;

	return l->data;
}

static uint8_t read_result(int err, uint8_t *value, size_t len,
						void *user_data)
{
	struct attribute *a = user_data;
	struct btd_adapter *adapter;
	uint8_t status = 0;

	adapter = btd_adapter_get_default();
	if (!adapter)
		return FALSE;

	if (!err)
		status = attrib_db_update(adapter, a->handle,
					NULL, value, len, NULL);
	else {
		return err;
	}
	return status;
}

static uint8_t write_result(int err, uint8_t *value, size_t len,
							void *user_data)
{
	GList *l;
	struct attribute *a = user_data;
	struct btd_attribute *attrib;

	l = g_list_find_custom(local_attribute_db, GUINT_TO_POINTER(&a->uuid),
								attr_uuid_cmp);

	if (!l)
		return NULL;
	attrib = l->data;


	return err;
}

static uint8_t read_desc_attr_db_value(struct attribute *a,
						struct btd_device *device,
						gpointer user_data)
{
	/* TODO: Add implemenation to call read callback */
	return 0;
}

static uint8_t read_char_attr_db_value(struct attribute *a,
						struct btd_device *device,
						gpointer user_data)
{
	struct btd_attribute *local_attr;
	uint8_t status = 0;

	local_attr = find_local_attr(&a->uuid);
	if (local_attr && local_attr->read_cb) {
		status = local_attr->read_cb(local_attr, read_result, a);
	} else
		return -ENOENT;

	return status;
}

static uint8_t write_char_attr_db_value(struct attribute *a,
						struct btd_device *device,
						gpointer user_data)
{
	struct btd_attribute *local_attr;
	uint8_t status = 0;
	void *ptr = (uint8_t *)a->data;

	local_attr = find_local_attr(&a->uuid);
	if (local_attr && local_attr->write_cb) {
		if (a->data) {
		status = local_attr->write_cb(local_attr, ptr,
							a->len, write_result, a);
		}
	} else
		return -ENOENT;

	return status;
}

static void free_pending_hndl_list(void)
{
	struct pending_hndl *temp_pending_hndl_list = NULL;

	/* Traverse through handl list and free the list
	   after atribute data base is updated */
	if (hndl_list_head) {
		hndl_list = hndl_list_head;
		temp_pending_hndl_list = hndl_list_head;
		while(temp_pending_hndl_list) {
			hndl_list = hndl_list->next;
			g_free(temp_pending_hndl_list);
			temp_pending_hndl_list = hndl_list;
		}
	}
}

static unsigned int get_attr_char_size(void)
{
	GList *l;
	uint16_t nex_hndl = 0x0001;
	struct pending_hndl *temp_list;
	struct btd_attribute *local_attr;
	unsigned int attr_size = 0;

	/* Calculate the size of the char attributes to be added to attribute DB */
	for (temp_list = hndl_list_head; temp_list != NULL;
					temp_list = temp_list->next) {
		l = g_list_find_custom(local_attribute_db,
					GUINT_TO_POINTER(nex_hndl),
					handle_cmp);
		if (l) {
			local_attr = l->data;
			if (temp_list->type == GATT_TYPE_CHARAC_SVC) {
				if (local_attr->value[0] &
						(ATT_CHAR_PROPER_NOTIFY |
						ATT_CHAR_PROPER_INDICATE))
					attr_size += 1;
				attr_size += 2;
			}
		}
		if (temp_list->next)
			nex_hndl = temp_list->next->handle;
	}
	return attr_size;
}

static unsigned int get_attr_desc_size(void)
{
	GList *l;
	uint16_t nex_hndl = 0x0001;
	struct pending_hndl *temp_list;
	struct btd_attribute *local_attr;
	unsigned int attr_size = 0;

	for (temp_list = hndl_list_head; temp_list != NULL;
					temp_list = temp_list->next) {
		l = g_list_find_custom(local_attribute_db,
					GUINT_TO_POINTER(nex_hndl),
					handle_cmp);
		if (l) {
			local_attr = l->data;

			if (temp_list->type == GATT_TYPE_DESC_USR)
					attr_size += 1;
		}
		if (temp_list->next)
			nex_hndl = temp_list->next->handle;
	}
	return attr_size;
}

static int is_connected(gpointer a1, gpointer a2)
{
    const struct btd_device *dev = a1;

    if (device_get_gatt_connected(dev))
        return 0;
    else
        return -1;
}

bool gatt_send_service_changed_ind(struct btd_adapter *adapter, bt_uuid_t *uuid,
                    uint16_t start_handle, uint16_t end_handle)
{
    GAttrib *attrib;
    GList *l, *connections;
	size_t len;
    struct btd_device *dev;

	DBG("+");
    connections = (GList *)btd_adapter_get_connections(adapter);

    do {
		l = g_list_find_custom(connections, GUINT_TO_POINTER(NULL),
						is_connected);

		if (l) {
			dev = l->data;
			if (device_is_trusted(dev)) {
				attrib = attrib_from_device(dev);
				/* Send the service changed indication */
				len = (sizeof(uint16_t) * (end_handle - start_handle));
				attrib_send_sc_ind(dev, attrib, start_handle,
							end_handle, len);
				g_attrib_unref(attrib);
			}
			connections = (GList *)g_slist_next(connections);
		} else
			break;
	} while(l);

	return TRUE;
}

uint16_t send_sc_indication(uint16_t start_handle, uint16_t end_handle, size_t vlen,
						uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(uint16_t);

	if (pdu == NULL)
		return 0;

	if (len < (vlen + min_len))
		return 0;

	pdu[0] = ATT_OP_HANDLE_IND;
	att_put_u16(start_handle, &pdu[1]);
	att_put_u16(end_handle, &pdu[3]);

	return vlen + min_len;
}

bool btd_gatt_update_attr_db(void)
{
	GList *l;
	uint16_t nex_hndl = 0x0001;
	struct pending_hndl *temp_list;
	struct btd_attribute *local_attr;
	struct btd_attribute *temp_att = NULL;
	gboolean svc_added;
	bt_uuid_t prim_uuid;
	bt_uuid_t char_uuid;
	struct btd_adapter *adapter;
	uint8_t read_char_db_pending = 0x00;
	uint8_t char_attr_size = 0, usr_desc_attr_size = 0, total_attr_size = 0;
	uint16_t start_handle, end_handle, chr_handle, desc_handle;
	bool new_service_add = FALSE;
	bool new_char_desc_add = FALSE;

	adapter = btd_adapter_get_default();
	if (!adapter)
		return FALSE;

	nex_hndl = hndl_list_head->handle;
	for (temp_list = hndl_list_head; temp_list != NULL; temp_list = temp_list->next) {
		l = g_list_find_custom(local_attribute_db, GUINT_TO_POINTER(nex_hndl),
								handle_cmp);
		if (l) {
		local_attr = l->data;
			/* get the characterisitic attribute, from local DB */
			if (local_attr->type.value.u16 == GATT_PRIM_SVC_UUID &&
				temp_list->type == GATT_TYPE_PRIM_SVC) {
				new_service_add = FALSE;
				prim_uuid = att_get_uuid(local_attr->value,
							local_attr->value_len);
				char_attr_size = get_attr_char_size();
				usr_desc_attr_size = get_attr_desc_size();
				total_attr_size = char_attr_size + usr_desc_attr_size + 1;
				/* Store the primary service */
				chr_handle = gatt_prim_service_add(adapter,
						GATT_PRIM_SVC_UUID, &prim_uuid,
						total_attr_size, &start_handle);
				new_char_desc_add = TRUE;
			} else if ((local_attr->type.value.u16 == GATT_CHARAC_UUID ||
				temp_list->type == GATT_TYPE_CHARAC_VALUE) && new_char_desc_add)  {
				/* get the characterisitic attribute, from local DB */
				if (!temp_att) {
					void *ptr = (uint8_t *)local_attr->value;
					uint8_t value[local_attr->value_len];
					memcpy(value, ptr, local_attr->value_len);
					temp_att = new_const_attribute(&local_attr->type,
							value, local_attr->value_len);
					temp_att->value_len = local_attr->value_len;
					if (local_attr->type.type == BT_UUID16)
						char_uuid = att_get_uuid16(&temp_att->value[3]);
					if (local_attr->type.type == BT_UUID32)
						char_uuid = att_get_uuid32(&temp_att->value[3]);
					if (local_attr->type.type == BT_UUID128)
						char_uuid = att_get_uuid128(&temp_att->value[3]);
					read_char_db_pending |= ATT_CHAR_READ_VALUE;
				}
				if (!temp_att->write_cb) {
					read_char_db_pending |= ATT_CHAR_RD_WRT_CB;
					temp_att->write_cb = local_attr->write_cb;
				}
				if (!temp_att->read_cb) {
					temp_att->read_cb= local_attr->read_cb;
					read_char_db_pending |= ATT_CHAR_RD_WRT_CB;
				}
				if (read_char_db_pending == (ATT_CHAR_READ_VALUE | ATT_CHAR_RD_WRT_CB) &&
								temp_list->type == GATT_TYPE_CHARAC_VALUE) {
					/* Store the Characteristic attribute */
					if (char_uuid.type == BT_UUID16) {
						svc_added = gatt_add_characteristic(adapter,
							&chr_handle, start_handle,
							GATT_OPT_CHR_UUID16, &char_uuid,
							GATT_OPT_CHR_PROPS,
								temp_att->value[0],
							GATT_OPT_CHR_VALUE_CB, ATTRIB_READ,
								read_char_attr_db_value, adapter,
							GATT_OPT_CHR_VALUE_CB, ATTRIB_WRITE,
								write_char_attr_db_value, adapter,
							GATT_OPT_INVALID);
					} else {
						svc_added = gatt_add_characteristic(adapter,
							&chr_handle, start_handle,
							GATT_OPT_CHR_UUID, &char_uuid,
							GATT_OPT_CHR_PROPS,
								temp_att->value[0],
							GATT_OPT_CHR_VALUE_CB, ATTRIB_READ,
								read_char_attr_db_value, adapter,
							GATT_OPT_CHR_VALUE_CB, ATTRIB_WRITE,
								write_char_attr_db_value, adapter,
							GATT_OPT_INVALID);
					}
					end_handle = chr_handle - 1;
					if (!svc_added) {
						new_service_add = FALSE;
						new_char_desc_add = FALSE;
						break;
					} else {
						new_service_add = TRUE;
					}
					read_char_db_pending = 0x00;
					free(temp_att);
				}
			} else if (temp_list->type == GATT_TYPE_DESC_USR && new_char_desc_add) {
					bt_uuid_t desc_uuid;
					desc_handle = chr_handle;
					if (local_attr->type.type == BT_UUID16)
						desc_uuid = att_get_uuid16(&local_attr->type.value.u16);
					else if (local_attr->type.type == BT_UUID32)
						desc_uuid = att_get_uuid32(local_attr->type.value.u32);
					else
						desc_uuid = att_get_uuid128(&local_attr->type.value.u128);
					if (desc_uuid.type == BT_UUID16) {
						svc_added = gatt_add_descriptor(adapter,
							&desc_handle, chr_handle,
							GATT_OPT_DESC_UUID16, &desc_uuid,
							GATT_OPT_DESC_VALUE_CB, ATTRIB_READ,
								read_desc_attr_db_value, adapter,
							GATT_OPT_INVALID);
					} else {
						svc_added = gatt_add_descriptor(adapter,
							&desc_handle, chr_handle,
							GATT_OPT_DESC_UUID, &desc_uuid,
							GATT_OPT_DESC_VALUE_CB, ATTRIB_READ,
								read_desc_attr_db_value, adapter,
							GATT_OPT_INVALID);
					}
					end_handle = desc_handle - 1;
					if (!svc_added) {
						new_service_add = FALSE;
						break;
					} else {
						new_service_add = TRUE;
					}
			}
		} else {
			DBG("No Match");
		}
		if (temp_list->next)
			nex_hndl = temp_list->next->handle;
	}
	if (new_service_add) {
		DBG("Service Added");
		store_start_end_handle(&prim_uuid, start_handle, end_handle);
	} else {
		DBG("Failed to add service");
		start_handle = 0;
		end_handle = 0;
		/* Remove the service from attrib database */
		get_start_end_handle(&prim_uuid, &start_handle, &end_handle);
		attrib_remove_service(adapter, start_handle, end_handle);
	}
	gatt_send_service_changed_ind(adapter, &prim_uuid, start_handle, end_handle);
	free_pending_hndl_list();

	return TRUE;
}

static void remove_attr_service(struct btd_attribute *service)
{
	uint16_t start_handle, end_handle;
	bt_uuid_t prim_uuid;
	struct btd_adapter *adapter;

	adapter = btd_adapter_get_default();
	if (!adapter)
		return;

	prim_uuid = att_get_uuid(service->value, service->value_len);
	get_start_end_handle(&prim_uuid, &start_handle, &end_handle);
	attrib_remove_service(adapter, start_handle, end_handle);
	gatt_send_service_changed_ind(adapter, &prim_uuid, start_handle, end_handle);
}

void btd_gatt_set_notify_indicate_flag(struct btd_attribute *attrib,
						bool notify_indicate)
{
	attrib->notify_indicate = notify_indicate;
	g_list_insert_sorted (local_attribute_db, attrib,
					attribute_cmp);
}
#endif

struct btd_attribute *btd_gatt_add_service(const bt_uuid_t *uuid)
{
	uint16_t len = bt_uuid_len(uuid);
	struct btd_attribute *attr = g_malloc0(sizeof(struct btd_attribute) +
									len);

	/*
	 * Service DECLARATION
	 *
	 *   TYPE         ATTRIBUTE VALUE
	 * +-------+---------------------------------+
	 * |0x2800 | 0xYYYY...                       |
	 * | (1)   | (2)                             |
	 * +------+----------------------------------+
	 * (1) - 2 octets: Primary/Secondary Service UUID
	 * (2) - 2 or 16 octets: Service UUID
	 */

	attr->type = primary_uuid;

	att_put_uuid(*uuid, attr->value);
	attr->value_len = len;

#ifdef __TIZEN_PATCH__
	/* Store the primary handle in linkedlist,
	  * later this list shall be used to add the attributes to DB */
	if (!hndl_list) {
		hndl_list = g_new0(struct pending_hndl, 1);
		hndl_list_head = hndl_list;
		hndl_list->type = GATT_TYPE_PRIM_SVC;
		hndl_list->handle = next_handle;
		hndl_list->next = NULL;
	}
#endif

	if (local_database_add(next_handle, attr) < 0) {
		g_free(attr);
		return NULL;
	}

	/* TODO: missing overflow checking */
	next_handle = next_handle + 1;

	return attr;
}

void btd_gatt_remove_service(struct btd_attribute *service)
{
	GList *list = g_list_find(local_attribute_db, service);
	bool first_node;
#ifdef __TIZEN_PATCH__
	struct btd_attribute *attrib_rmv = list->data;
#endif

	if (!list)
		return;

#ifdef __TIZEN_PATCH__
	/* Remove the service from Attrib database */
	remove_attr_service(attrib_rmv);
#endif

	first_node = local_attribute_db == list;

	/* Remove service declaration attribute */
	free(list->data);
	list = g_list_delete_link(list, list);

	/* Remove all characteristics until next service declaration */
	while (list && !is_service(list->data)) {
#ifndef __TIZEN_PATCH__
		free(list->data);
#endif
		list = g_list_delete_link(list, list);
	}

	/*
	 * When removing the first node, local attribute database head
	 * needs to be updated. Node removed from middle doesn't change
	 * the list head address.
	 */
	if (first_node)
		local_attribute_db = list;
}

#ifdef __TIZEN_PATCH__
bool btd_gatt_update_char(const bt_uuid_t *uuid, uint8_t *value, size_t len)
{
	GList *l;
	struct btd_attribute *attrib;
	struct btd_adapter *adapter;
	uint8_t properties;
	bool notify_indicate;

	adapter = btd_adapter_get_default();
	if (!adapter)
		return FALSE;

	l = g_list_find_custom(local_attribute_db, GUINT_TO_POINTER(uuid),
							attr_uuid_cmp);

	if (!l)
		return FALSE;

	attrib = l->data;
	attrib->value_len = len;
	memcpy(&attrib->value, value, len);
	notify_indicate = attrib->notify_indicate;
	g_list_insert_sorted (local_attribute_db, attrib, attribute_cmp);

	l = g_list_find_custom(local_attribute_db,
				GUINT_TO_POINTER(attrib->handle - 1),
				handle_cmp);

	if (!l)
		return FALSE;

	attrib = l->data;
	properties = attrib->value[0];

	if (gatt_update_db(adapter, uuid, value, len)) {
		/* Attrib database is updated successfully */

		/* check if the characteristic properties are
		 * set to send notification or indication */
		if (properties & (GATT_CHR_PROP_NOTIFY
				| GATT_CHR_PROP_INDICATE)) {
			/* Since characteristic is updated, and the
			 * property is set to notify to indicate, find
			 * the descreiptor UUID for notification and indication */
			if (notify_indicate == FALSE) {
				l = g_list_find_custom(local_attribute_db,
						GUINT_TO_POINTER(attrib->handle + 1),
						handle_cmp);

				if (!l)
					return FALSE;
				attrib = l->data;
				btd_gatt_set_notify_indicate_flag(attrib, TRUE);
			} else {
				gatt_send_noty_ind(adapter, uuid, value, len);
			}
		}
	}
	return TRUE;
}
#endif

struct btd_attribute *btd_gatt_add_char(const bt_uuid_t *uuid,
						uint8_t properties,
						btd_attr_read_t read_cb,
						btd_attr_write_t write_cb)
{
	struct btd_attribute *char_decl, *char_value = NULL;

	/* Attribute value length */
	uint16_t len = 1 + 2 + bt_uuid_len(uuid);
	uint8_t value[len];

	/*
	 * Characteristic DECLARATION
	 *
	 *	 TYPE		  ATTRIBUTE VALUE
	 * +-------+---------------------------------+
	 * |0x2803 | 0xXX 0xYYYY 0xZZZZ...			 |
	 * | (1)   |  (2)	(3)   (4)				 |
	 * +------+----------------------------------+
	 * (1) - 2 octets: Characteristic declaration UUID
	 * (2) - 1 octet : Properties
	 * (3) - 2 octets: Handle of the characteristic Value
	 * (4) - 2 or 16 octets: Characteristic UUID
	 */

	value[0] = properties;

	/*
	 * Since we don't know yet the characteristic value attribute
	 * handle, we skip and set it later.
	 */
#ifdef __TIZEN_PATCH__
	att_put_uuid(*uuid, &value[3]);
#else
	put_uuid_le(uuid, &value[3]);
#endif

#ifndef __TIZEN_PATCH__
	char_decl = new_const_attribute(&chr_uuid, value, len);
#else
	if (uuid->type == BT_UUID16)
		char_decl = new_const_attribute(&chr_uuid, value, len);
	else if (uuid->type == BT_UUID32)
		char_decl = new_const_attribute(&chr_uuid32, value, len);
	else
		char_decl = new_const_attribute(&chr_uuid128, value, len);
#endif
	if (!char_decl)
		goto fail;

	char_value = new_attribute(uuid, read_cb, write_cb);
	if (!char_value)
		goto fail;

	if (local_database_add(next_handle, char_decl) < 0)
		goto fail;

#ifdef __TIZEN_PATCH__
	/* Store the char decl handle in linkedlist,
	  * later this list shall be used to add the attributes to DB */
	if (hndl_list && hndl_list->next == NULL) {
		hndl_list->next = g_new0(struct pending_hndl, 1);
		hndl_list = hndl_list->next;
		hndl_list->type = GATT_TYPE_CHARAC_SVC;
		hndl_list->handle = next_handle;
		hndl_list->next = NULL;
	}
#endif

	next_handle = next_handle + 1;

	/*
	 * Characteristic VALUE
	 *
	 *	 TYPE		  ATTRIBUTE VALUE
	 * +----------+---------------------------------+
	 * |0xZZZZ... | 0x...							|
	 * |  (1)	  |  (2)							|
	 * +----------+---------------------------------+
	 * (1) - 2 or 16 octets: Characteristic UUID
	 * (2) - N octets: Value is read dynamically from the service
	 * implementation (external entity).
	 */

	if (local_database_add(next_handle, char_value) < 0)
		/* TODO: remove declaration */
		goto fail;

#ifdef __TIZEN_PATCH__
	/* Store the char value handle in linkedlist,
	  * later this list shall be used to add the attributes to DB */
	if (hndl_list && hndl_list->next == NULL) {
		hndl_list->next = g_new0(struct pending_hndl, 1);
		hndl_list = hndl_list->next;
		hndl_list->type = GATT_TYPE_CHARAC_VALUE;
		hndl_list->handle = next_handle;
		hndl_list->next = NULL;
	}
#endif

	next_handle = next_handle + 1;

	/*
	 * Update characteristic value handle in characteristic declaration
	 * attribute. For local attributes, we can assume that the handle
	 * representing the characteristic value will get the next available
	 * handle. However, for remote attribute this assumption is not valid.
	 */
	put_le16(char_value->handle, &char_decl->value[1]);

	return char_value;

fail:
	free(char_decl);
	free(char_value);

	return NULL;
}

struct btd_attribute *btd_gatt_add_char_desc(const bt_uuid_t *uuid,
						btd_attr_read_t read_cb,
						btd_attr_write_t write_cb)
{
	struct btd_attribute *attr;

	/*
	 * From Core SPEC 4.1 page 2184:
	 * "Characteristic descriptor declaration permissions are defined by a
	 * higher layer profile or are implementation specific. A client shall
	 * not assume all characteristic descriptor declarations are readable."
	 *
	 * The read/write callbacks presence will define the descriptor
	 * permissions managed directly by the core. The upper layer can define
	 * additional permissions constraints.
	 */

	attr = new_attribute(uuid, read_cb, write_cb);
	if (!attr)
		return NULL;

	if (local_database_add(next_handle, attr) < 0) {
		free(attr);
		return NULL;
	}

#ifdef __TIZEN_PATCH__
	/* Store the store handle in linkedlist,
	  * later this list shall be used to add the attributes to DB */
	if (hndl_list && hndl_list->next == NULL) {
		hndl_list->next = g_new0(struct pending_hndl, 1);
		hndl_list = hndl_list->next;
		hndl_list->type = GATT_TYPE_DESC_USR;
		hndl_list->handle = next_handle;;
		hndl_list->next = NULL;
	}
#endif

	next_handle = next_handle + 1;

	return attr;
}

void gatt_init(void)
{
	DBG("Starting GATT server");

	gatt_dbus_manager_register();
}

void gatt_cleanup(void)
{
	DBG("Stopping GATT server");

	gatt_dbus_manager_unregister();
}

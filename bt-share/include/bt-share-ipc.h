/*
 * bluetooth-share
 *
 * Copyright (c) 2012-2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *              http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef __DEF_BLUETOOTH_SHARE_IPC_H_
#define __DEF_BLUETOOTH_SHARE_IPC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "bt-share-main.h"

#define BT_ADDRESS_LENGTH_MAX 6

#define BT_IPC_SENDER_NAME		"org.projectx.bluetooth"
#define BT_UG_IPC_RESPONSE_OBJECT "/org/projectx/response_event"
#define BT_UG_IPC_EVENT_CONNECTED "Connected"
#define BT_UG_IPC_EVENT_DISCONNECTED	"Disconnected"

#define BT_SYSPOPUP_IPC_RESPONSE_OBJECT "/org/projectx/bt_syspopup_res"
#define BT_SYSPOPUP_INTERFACE "User.Bluetooth.syspopup"
#define BT_SYSPOPUP_METHOD_RESPONSE "Response"

#define BT_UG_IPC_REQUEST_OBJECT "/org/projectx/connect_device"
#define BT_UG_IPC_INTERFACE "User.Bluetooth.UG"
#define BT_UG_IPC_METHOD_SEND "Send"

#define BT_SHARE_UI_INTERFACE "User.BluetoothShare.UI"
#define BT_SHARE_UI_SIGNAL_OPPABORT "opp_abort"
#define BT_SHARE_UI_SIGNAL_GET_TR_DATA "get_data"
#define BT_SHARE_UI_SIGNAL_SEND_FILE "send_file"
#define BT_SHARE_UI_SIGNAL_INFO_UPDATE "info_update"

#define BT_SHARE_ENG_INTERFACE "User.BluetoothShare.Engine"
#define BT_SHARE_ENG_OBJECT "/org/projectx/transfer_info"
#define BT_SHARE_ENG_SIGNAL_PROGRESS "transfer_progress"
#define BT_SHARE_ENG_SIGNAL_ERROR "error_type"
#define BT_SHARE_ENG_SIGNAL_UPDATE_VIEW "update_view"

#define BT_SHARE_FRWK_INTERFACE "User.Bluetooth.FRWK"
#define BT_SHARE_FRWK_SIGNAL_DEINIT "deinit"

#define BT_IPC_STRING_SIZE 256
#define BT_ADDR_STR_LEN_MAX	18

#define BT_MIME_TYPE_MAX_LEN   20

#define BT_INBOUND_TABLE	"inbound"
#define BT_OUTBOUND_TABLE	"outbound"


typedef struct {
	int param1;
	int param2;
	char param3[BT_IPC_STRING_SIZE];
} __attribute__ ((packed)) bt_ug_param_t;


typedef enum {
	BT_AG_CONNECT_RESPONSE,
	BT_AG_DISCONNECT_RESPONSE,
	BT_AV_CONNECT_RESPONSE,
	BT_AV_DISCONNECT_RESPONSE,
	BT_HID_CONNECT_RESPONSE,
	BT_HID_DISCONNECT_RESPONSE,
	BT_SENDING_RESPONSE,
	BT_BROWSING_RESPONSE,
	BT_PRINTING_RESPONSE,
	BT_PAIRING_RESPONSE,
} bt_app_response_t;

typedef enum {
	BT_APP_SUCCESS,
	BT_APP_FAIL,
} bt_app_result_t;

typedef enum {
	BT_TR_ONGOING = -1,
	BT_TR_SUCCESS,
	BT_TR_FAIL
} bt_app_tr_status_t;

typedef struct {
	int file_cnt;
	char addr[BLUETOOTH_ADDRESS_LENGTH];
	char name[BLUETOOTH_DEVICE_NAME_LENGTH_MAX];
	char *mode;
	char **file_path;
	char **content;
	char *type;
	unsigned int size;
} opc_transfer_info_t;

typedef struct {
	int uid;
	int tr_status;
	char *device_name;
	char *filepath;
	int timestamp;
	char *addr;
} bt_tr_info_t;

typedef enum {
	BT_TR_OUTBOUND,
	BT_TR_INBOUND
} bt_share_tr_type_t;


typedef struct _BtShareMethod BtShareMethod;
typedef struct _BtShareMethodClass BtShareMethodClass;

struct _BtShareMethod {
	GObject parent;
};

struct _BtShareMethodClass {
	GObjectClass parent_class;
};

gboolean _bt_init_dbus_signal(void);
void _free_transfer_info(opc_transfer_info_t *node);
void _remove_transfer_info(opc_transfer_info_t *node);
int _request_file_send(opc_transfer_info_t *node);
void _bt_create_warning_popup(int error_type);

gboolean _bt_update_sent_data_status(int uid, bt_app_tr_status_t status);

void _bt_rm_all_send_data(void);
void _bt_rm_all_recv_data(void);
void _bt_update_transfer_list_view(char *db);
gboolean _bt_add_recv_transfer_status_data(char *device_name,
			char *filepath, char *type,
			unsigned int size, int status);

#ifdef __cplusplus
}
#endif
#endif				/* __DEF_BLUETOOTH_SHARE_IPC_H_ */

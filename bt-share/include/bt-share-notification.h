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

#ifndef __DEF_BLUETOOTH_SHARE_NOTI_H_
#define __DEF_BLUETOOTH_SHARE_NOTI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <notification.h>

#include "bt-share-main.h"

#define BT_SHARE_BIN_PATH "/usr/bin/bluetooth-share"

#define QP_NO_APP_LAUNCH	NOTIFICATION_PROP_DISABLE_APP_LAUNCH
#define QP_NO_TICKER		NOTIFICATION_PROP_DISABLE_TICKERNOTI
#define QP_NO_DELETE		(NOTIFICATION_PROP_DISABLE_AUTO_DELETE | NOTIFICATION_PROP_VOLATILE_DISPLAY)


#define NOTIFICATION_TEXT_LEN_MAX 100
#define NOTI_TR_TYPE_IN "inbound"
#define NOTI_TR_TYPE_OUT "outbound"

/* Priv_id should be unique. */
enum {
	SEND_PRIV_ID,			/* ongoing type for opp client */
	RECV_NOTI_PRIV_ID = 99999,	/* notification type for obex server */
	SEND_NOTI_PRIV_ID	/* notification type for obex client */
};

typedef enum {
	BT_NOTI_T = NOTIFICATION_TYPE_NOTI,
	BT_ONGOING_T = NOTIFICATION_TYPE_ONGOING,
} bt_qp_type_t;

typedef enum {
	CREATE_PROGRESS,
	CREATE_TR_LIST
} bt_qp_launch_type_t;


notification_h _bt_create_notification(bt_qp_type_t type);

int _bt_insert_notification(notification_h noti,
				char *title,
				char *content,
				char *icon_path,
				char *indicator_icon_path);

int _bt_update_notification(notification_h noti,
				char *title,
				char *content,
				char *icon_path);

int _bt_update_notification_progress(void *handle,
				int id,
				int val);


gboolean _bt_get_notification_text(int priv_id, char *str);

int _bt_delete_notification(notification_h noti);

int _bt_set_notification_app_launch(notification_h noti,
					bt_qp_launch_type_t type,
					const char *transfer_type,
					const char *filename,
					const char *progress_cnt,
					int transfer_id);
int _bt_set_notification_property(notification_h noti, int flag, int applist);

void _bt_register_notification_cb(struct bt_appdata *ad);

void _bt_unregister_notification_cb(struct bt_appdata *ad);



#ifdef __cplusplus
}
#endif
#endif				/* __DEF_BLUETOOTH_SHARE_NOTI_H_ */

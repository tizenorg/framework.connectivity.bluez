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

#ifndef __DEF_BLUETOOTH_SHARE_SYSPOPUP_H_
#define __DEF_BLUETOOTH_SHARE_SYSPOPUP_H_

#ifdef __cplusplus
extern "C" {
#endif

#define BT_GLOBALIZATION_STR_LENGTH	256
#define BT_FILE_NAME_LEN_MAX 255
#define BT_POPUP_TEXT_LEN \
		(BT_GLOBALIZATION_STR_LENGTH + BT_FILE_NAME_LEN_MAX)


/* This value are used in the N-prj */
#define BT_APP_MSGBOX_TIMEOUT 3
#define BT_APP_AUTHENTICATION_TIMEOUT 35
#define BT_APP_AUTHORIZATION_TIMEOUT 15
#define BT_APP_SECURITY_POPUP_TIMEOUT 10
#define BT_APP_MSGBOX_FTP_TIMEOUT 10
#define BT_APP_POPUP_SIZE 100
#define BT_APP_POPUP_LAUNCH_TIMEOUT 100

typedef enum {
	POPUP_BUTTON_OK = 1,
	POPUP_BUTTON_OK_AND_CANCEL,
	POPUP_BUTTON_CANCEL,
	POPUP_NO_BUTTON
} bt_button_type_t;

typedef enum {
	BT_POPUP_NONE = 0,
	BT_POPUP_TEXT,
	BT_POPUP_TITLE_TEXT,
	BT_POPUP_TEXT_BTN,
	BT_POPUP_TITLE_TEXT_BTN,
	BT_POPUP_MAX,
} bt_popup_types_t;

typedef enum {
	BT_BTN_NONE = 0,
	BT_BTN_OK,
	BT_BTN_CANCEL,
	BT_BTN_OK_CANCEL,
	BT_BTN_MAX,
} bt_btn_types_t;

typedef enum {
	BT_APP_EVENT_CONFIRM_MODE_REQUEST = 0x0001,
	BT_APP_EVENT_FILE_RECEIVED = 0x0002,
	BT_APP_EVENT_INFORMATION = 0x0004,
	BT_APP_EVENT_OVERWRITE_REQUEST = 0x0008
} bt_app_event_type_t;

typedef enum {
	POPUP_RESPONSE_OK,
	POPUP_RESPONSE_CANCEL,
	POPUP_RESPONSE_TIMEOUT
} bt_popup_response;


typedef struct {
	char *title;
	char *type;
	char *file;
	char *device_name;
} bt_app_sys_popup_params_t;



int _bt_launch_system_popup(bt_app_event_type_t event_type,
			    bt_app_sys_popup_params_t *popup_params,
			    void *cb,
			    void *data);

gboolean _bt_app_popup_memoryfull(gpointer user_data);

#ifdef __cplusplus
}
#endif
#endif				/* __DEF_BLUETOOTH_SHARE_SYSPOPUP_H_ */

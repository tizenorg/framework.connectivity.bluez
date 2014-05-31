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

#ifndef __DEF_BLUETOOTH_SHARE_RES_H_
#define __DEF_BLUETOOTH_SHARE_RES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <libintl.h>

/*==============  String ================= */
#define BT_COMMON_PKG		"ug-setting-bluetooth-efl"
#define BT_COMMON_RES		"/usr/ug/res/locale"

#define BT_STR_MEMORY_FULL	\
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_MEMORYFULL")
#define BT_STR_UNABLE_TO_SEND	\
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_SENDINGFAIL")
#define BT_TR_STATUS \
	dgettext(BT_COMMON_PKG, "IDS_BT_BODY_PD_SUCCESSFUL_PD_FAILED")
#define BT_STR_SENDING \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_SENDING_ING")
#define BT_STR_RECEIVING \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_RECEIVING_ING")
#define BT_STR_RECEIVED \
	dgettext(BT_COMMON_PKG, "IDS_BT_BODY_RECEIVED")
#define BT_STR_SENT \
	dgettext(BT_COMMON_PKG, "IDS_BT_BODY_SENT")
#define BT_STR_SHARE \
	dgettext(BT_COMMON_PKG, "IDS_BT_BODY_BLUETOOTH_SHARE")
#define BT_STR_BLUETOOTH_ON \
	dgettext(BT_COMMON_PKG, "IDS_BT_POP_ACTIVATED")
#define BT_STR_BLUETOOTH_AVAILABLE \
	dgettext(BT_COMMON_PKG, "IDS_BT_BODY_BLUETOOTH_AVAILABLE")

/*==============  Image ================= */
#define ICONDIR	"/usr/ug/res/images/ug-setting-bluetooth-efl"
#define BT_ICON_PATH_MAX	256

#define BT_ICON_QP_BT_ON			ICONDIR"/Q02_Notification_bluetooth.png"

#define BT_ICON_NOTIFICATION_SENDING           ICONDIR"/Q02_Notification_Bluetooth_file_sending.png"
#define BT_ICON_NOTIFICATION_SENDING_INDICATOR          "reserved://indicator/ani/uploading"

#define BT_ICON_NOTIFICATION_SENT              ICONDIR"/Q02_Notification_Bluetooth_file_sent.png"
#define BT_ICON_NOTIFICATION_SENT_INDICATOR             ICONDIR"/B03_Processing_upload_ani_06.png"

#define BT_ICON_NOTIFICATION_RECEIVING         ICONDIR"/Q02_Notification_bluetooth_file_receiving.png"
#define BT_ICON_NOTIFICATION_RECEIVING_INDICATOR               "reserved://indicator/ani/downloading"

#define BT_ICON_NOTIFICATION_RECEIVED_INDICATOR         ICONDIR"/B03_Processing_download_ani_06.png"
#define BT_ICON_NOTIFICATION_RECEIVED          ICONDIR"/Q02_Notification_Bluetooth_file_received.png"



#ifdef __cplusplus
}
#endif
#endif				/* __DEF_BLUETOOTH_SHARE_RES_H_ */

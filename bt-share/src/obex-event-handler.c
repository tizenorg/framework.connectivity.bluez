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

#include <sys/vfs.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <app_service.h>
#include <vconf-keys.h>
#include <calendar2.h>
#include <vconf.h>
#include <Ecore_File.h>
#include <bluetooth-share-api.h>
#include <notification.h>
#include <media_content.h>
#include <sys/stat.h>
#include <aul.h>

#include "applog.h"
#include "bluetooth-api.h"
#include "obex-event-handler.h"
#include "bt-share-main.h"
#include "bt-share-syspopup.h"
#include "bt-share-resource.h"
#include "bt-share-ipc.h"
#include "bt-share-notification.h"
#include "bt-share-common.h"
#include "bt-share-noti-handler.h"

extern struct bt_appdata *app_state;

typedef struct {
	void *noti_handle;
	int transfer_id;
	int noti_id;
} bt_noti_data_t;

typedef struct {
	char *file_path;
	bt_file_type_t file_type;
} bt_file_info_t;

bt_obex_server_authorize_into_t server_auth_info;
extern GSList *bt_transfer_list;
GSList *bt_receive_noti_list;

void _bt_obex_writeclose(bt_file_info_t *info);

static void __bt_obex_file_push_auth(bt_obex_server_authorize_into_t *server_auth_info);

static bt_noti_data_t *__bt_get_noti_data_by_transfer_id(int transfer_id)
{
	GSList *l;
	bt_noti_data_t *data;

	for (l = bt_receive_noti_list; l != NULL; l = l->next) {
		data = l->data;

		if (data == NULL)
			continue;

		if (data->transfer_id == transfer_id)
			return data;
	}

	return NULL;
}

static char *__get_file_name(int cnt, char **path)
{
	char *pfilename;
	if (path == NULL) {
		DBG("Path is invalid");
		return NULL;
	}
	pfilename = strrchr(path[cnt], '/') + 1;
	DBG("File name[%d] : %s \n", cnt, pfilename);
	return pfilename;
}

static void __delete_notification(gpointer data, gpointer user_data)
{
	bt_noti_data_t *noti_data = data;

	if (noti_data == NULL)
		return;

	_bt_delete_notification(noti_data->noti_handle);
	g_free(noti_data);
}

int _bt_get_transfer_id_by_noti_id(int noti_id)
{
	GSList *l;
	bt_noti_data_t *data;

	for (l = bt_receive_noti_list; l != NULL; l = l->next) {
		data = l->data;

		if (data == NULL)
			continue;

		if (data->noti_id == noti_id)
			return data->transfer_id;
	}

	return -1;
}

 void _bt_clear_receive_noti_list(void)
{
	if (bt_receive_noti_list) {
		g_slist_foreach(bt_receive_noti_list,
				(GFunc)__delete_notification,
				NULL);
		g_slist_free(bt_receive_noti_list);
		bt_receive_noti_list = NULL;
	}
}

char * __get_dest_file_path(const char *path)
{
	char file_path[BT_TEMP_FILE_PATH_LEN_MAX] = { 0, };

	/* Media updation is failing if we give FTP folder path.
		So we have to map the FTP folder path like below
		/opt/share/bt-ftp/Media/XX -> /opt/usr/media/XX
		/opt/share/bt-ftp/SD_External/XX -> /opt/storage/sdcard/XX
	*/
	if (g_str_has_prefix(path, BT_FTP_FOLDER_PHONE)) {
			snprintf(file_path, sizeof(file_path), "%s/%s",
			BT_DOWNLOAD_PHONE_ROOT,
			path + strlen(BT_FTP_FOLDER_PHONE));
	} else if (g_str_has_prefix(path, BT_FTP_FOLDER_MMC)) {
			snprintf(file_path, sizeof(file_path), "%s/%s",
			BT_DOWNLOAD_MMC_ROOT,
			path + strlen(BT_FTP_FOLDER_MMC));
	} else {
			snprintf(file_path, sizeof(file_path), "%s", path);
	}

	DBG_SECURE("File path %s", file_path);

	return g_strdup(file_path);
}

static void __free_file_info(bt_file_info_t *info)
{
	g_free(info->file_path);
	g_free(info);
}

static bt_file_type_t __get_file_type(char *extn)
{
	DBG("exten : %s \n", extn);

	if (NULL != extn) {
	if (!strcmp(extn, "vcf"))
		return BT_FILE_VCARD;
		else if (!strcmp(extn, "vcs"))
			return BT_FILE_VCAL;
		else if (!strcmp(extn, "vbm"))
			return BT_FILE_VBOOKMARK;
	}
	return BT_FILE_OTHER;
}

static gboolean __bt_scan_media_file(char *file_path)
{
	int ret;

	ret = media_content_connect();
	if (ret != MEDIA_CONTENT_ERROR_NONE) {
		ERR("Fail to connect the media content: %d", ret);
		return FALSE;
	}

	ret = media_content_scan_file(file_path);
	if (ret != MEDIA_CONTENT_ERROR_NONE)
		ERR("Fail to scan file: %d", ret);

	ret = media_content_disconnect();
	if (ret != MEDIA_CONTENT_ERROR_NONE)

	ERR("Fail to disconnect the media content: %d", ret);
	return TRUE;
}

void _bt_share_event_handler(int event, bluetooth_event_param_t *param,
			       void *user_data)
{
	int noti_id;
	int ret;
	static int send_index = 0;
	char *name = NULL;
	char title[NOTIFICATION_TEXT_LEN_MAX] = { 0 };
	char str[NOTIFICATION_TEXT_LEN_MAX] = { 0 };
	char opc_cnt[NOTIFICATION_TEXT_LEN_MAX] = { 0 };
	int percentage = 0;
	notification_h noti = NULL;
	bt_obex_server_authorize_into_t *auth_info;
	bt_obex_server_transfer_info_t *transfer_info;
	opc_transfer_info_t *node = NULL;
	struct bt_appdata *ad = app_state;
	bt_tr_data_t *info = NULL;
	bt_opc_transfer_info_t *client_info = NULL;
	bt_noti_data_t *data;
	int s_id = 0;
	pthread_t thread_id;

	if (bt_transfer_list)
		node = bt_transfer_list->data;
	DBG("OPC event : [0x%x] \n", event);

	switch (event) {
	case BLUETOOTH_EVENT_ENABLED:
		if (ad->obex_server_init == FALSE) {
			if (_bt_init_obex_server() == BT_SHARE_ERROR_NONE)
				ad->obex_server_init = TRUE;
		}
		break;

	case BLUETOOTH_EVENT_DISABLED:
		g_free(server_auth_info.filename);
		server_auth_info.filename = NULL;
		_bt_terminate_app();
		break;

	case BLUETOOTH_EVENT_OPC_CONNECTED:
		DBG("BLUETOOTH_EVENT_OPC_CONNECTED, result [%d] \n", param->result);
		if (param->result != BLUETOOTH_ERROR_NONE) {
			_bt_create_warning_popup(param->result);
			if (NULL != node && node->file_cnt > send_index) {
				info = (bt_tr_data_t *)(ad->tr_next_data)->data;
				if (info == NULL) {
					DBG("info is NULL");
					return;
				}

				s_id = info->sid;
				DBG("info->sid = %d info->id = %d\n", info->sid, info->id);
				while (NULL != ad->tr_next_data) {
					info = (bt_tr_data_t *)(ad->tr_next_data)->data;
					if (info == NULL)
						break;
					DBG("info->sid = %d info->id = %d\n", info->sid, info->id);
					if (info->sid != s_id) {
						DBG("SID did not match so break done.\n");
						break;
					}

					_bt_update_sent_data_status(info->id, BT_TR_FAIL);
					ad->send_data.tr_fail++;
					ad->tr_next_data = g_slist_next(ad->tr_next_data);
				}
				_bt_update_transfer_list_view("outbound");

				snprintf(title, sizeof(title), "%s: %s", BT_STR_SHARE, BT_STR_SENT);
				snprintf(str, sizeof(str), BT_TR_STATUS,
					ad->send_data.tr_success,
					ad->send_data.tr_fail);

				DBG("str = [%s] \n", str);

				if (ad->send_noti == NULL) {
					noti = _bt_create_notification(BT_NOTI_T);
					_bt_set_notification_app_launch(noti, CREATE_TR_LIST,
							NOTI_TR_TYPE_OUT, NULL, NULL, 0);

					_bt_set_notification_property(noti, QP_NO_DELETE,
						NOTIFICATION_DISPLAY_APP_NOTIFICATION_TRAY |
						NOTIFICATION_DISPLAY_APP_INDICATOR);

					ad->send_noti_id = _bt_insert_notification(noti,
								title, str,
								BT_ICON_NOTIFICATION_SENT,
								BT_ICON_NOTIFICATION_SENT_INDICATOR);
					ad->send_noti = noti;
				} else {
					_bt_update_notification(ad->send_noti,
								BT_STR_SENT, str,
								BT_ICON_NOTIFICATION_SENT);
				}
			}

			send_index = 0;
			_remove_transfer_info(node);

			if (!ad->tr_next_data) {
				bt_share_release_tr_data_list(ad->tr_send_list);
				ad->tr_send_list = NULL;
			}

		} else {
			ret = notification_status_message_post(BT_STR_SENDING);
			if (ret != NOTIFICATION_ERROR_NONE)
				ERR("notification_status_message_post() is failed : %d\n", ret);

			_bt_share_block_sleep(TRUE);
			_bt_set_transfer_indicator(TRUE);
		}
		break;

	case BLUETOOTH_EVENT_OPC_TRANSFER_STARTED:
		DBG("BLUETOOTH_EVENT_OPC_TRANSFER_STARTED \n");

		ret_if(node == NULL);

		name = __get_file_name(send_index++, node->file_path);
		snprintf(opc_cnt, sizeof(opc_cnt), "%d/%d",
					send_index, node->file_cnt);

		if (ad->opc_noti) {
			_bt_delete_notification(ad->opc_noti);
			ad->opc_noti = NULL;
			ad->opc_noti_id = 0;
		}

		noti = _bt_create_notification(BT_ONGOING_T);

		_bt_set_notification_property(noti, QP_NO_TICKER,
			NOTIFICATION_DISPLAY_APP_NOTIFICATION_TRAY |
			NOTIFICATION_DISPLAY_APP_INDICATOR);

		ad->opc_noti_id = _bt_insert_notification(noti,
					name, opc_cnt,
					BT_ICON_NOTIFICATION_SENDING,
					BT_ICON_NOTIFICATION_SENDING_INDICATOR);
		_bt_set_notification_app_launch(noti, CREATE_PROGRESS,
					NOTI_TR_TYPE_OUT, name, opc_cnt, 0);
		_bt_update_notification(noti, NULL, NULL, NULL);

		ad->opc_noti = noti;

		if (ad->tr_next_data == NULL) {
			ERR("ad->tr_next_data is NULL \n");
			break;
		}

		info = (bt_tr_data_t *)(ad->tr_next_data)->data;
		if (info == NULL)
			break;

		ad->current_tr_uid = info->id;
		DBG("ad->current_tr_uid = [%d] \n", ad->current_tr_uid);
		break;

	case BLUETOOTH_EVENT_OPC_TRANSFER_PROGRESS:
		client_info = (bt_opc_transfer_info_t *)param->param_data;
		ret_if(client_info == NULL);

		name =  strrchr(client_info->filename, '/');
		if (name)
			name++;
		else
			name = client_info->filename;

		percentage = client_info->percentage;
		_bt_update_notification_progress(NULL,
				ad->opc_noti_id, percentage);
		break;

	case BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE:
		DBG("BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE \n");
		client_info = (bt_opc_transfer_info_t *)param->param_data;
		ret_if(client_info == NULL);

		DBG("client_info->filename = [%s] \n", client_info->filename);
		DBG("ad->current_tr_uid = [%d] \n", ad->current_tr_uid);

		name =  strrchr(client_info->filename, '/');
		if (name)
			name++;
		else
			name = client_info->filename;

		DBG("name address = [%x] \n", name);

		_bt_delete_notification(ad->opc_noti);
		ad->opc_noti = NULL;
		ad->opc_noti_id = 0;

		DBG("ad->send_data.tr_fail = %d, ad->send_data.tr_success= %d \n",
			ad->send_data.tr_fail, ad->send_data.tr_success);

		if (param->result != BLUETOOTH_ERROR_NONE)
			ad->send_data.tr_fail++;
		else {
			ad->send_data.tr_success++;
			_bt_remove_vcf_file(client_info->filename);
		}

		snprintf(title, sizeof(title), "%s: %s", BT_STR_SHARE, BT_STR_SENT);
		snprintf(str, sizeof(str), BT_TR_STATUS,
			ad->send_data.tr_success,
			ad->send_data.tr_fail);

		DBG("str = [%s] \n", str);
		if (ad->send_noti == NULL) {
			noti = _bt_create_notification(BT_NOTI_T);
			_bt_set_notification_app_launch(noti, CREATE_TR_LIST,
					NOTI_TR_TYPE_OUT, NULL, NULL, 0);

			_bt_set_notification_property(noti, QP_NO_DELETE,
				NOTIFICATION_DISPLAY_APP_NOTIFICATION_TRAY |
				NOTIFICATION_DISPLAY_APP_INDICATOR);

			ad->send_noti_id = _bt_insert_notification(noti,
						title, str,
						BT_ICON_NOTIFICATION_SENT,
						BT_ICON_NOTIFICATION_SENT_INDICATOR);
			ad->send_noti = noti;
		} else {
			_bt_update_notification(ad->send_noti,
						BT_STR_SENT, str,
						BT_ICON_NOTIFICATION_SENT);
		}

		if (param->result != BLUETOOTH_ERROR_NONE) {
			_bt_update_sent_data_status(ad->current_tr_uid,
							BT_TR_FAIL);
			_bt_create_warning_popup(param->result);
		} else {
			_bt_update_sent_data_status(ad->current_tr_uid,
							BT_TR_SUCCESS);
		}

		_bt_remove_tmp_file(client_info->filename);
		_bt_update_transfer_list_view("outbound");

		ad->tr_next_data = g_slist_next(ad->tr_next_data);
		break;

	case BLUETOOTH_EVENT_OPC_DISCONNECTED:
		DBG("BLUETOOTH_EVENT_OPC_DISCONNECTED \n");

		ret_if(node == NULL);

		if (node->file_cnt > send_index) {
			info = (bt_tr_data_t *)(ad->tr_next_data)->data;
			if (info == NULL)
				break;
			s_id = info->sid;
			DBG("info->sid = %d info->id = %d\n", info->sid, info->id);

			while (NULL != ad->tr_next_data) {
				info = (bt_tr_data_t *)(ad->tr_next_data)->data;
				if (info == NULL)
					break;
				DBG("info->sid = %d info->id = %d\n", info->sid, info->id);

				if (s_id != info->sid) {
					DBG("SID did not match so break done.\n");
					break;
				}

				_bt_update_sent_data_status(info->id, BT_TR_FAIL);
				ad->send_data.tr_fail++;
				ad->tr_next_data = g_slist_next(ad->tr_next_data);
			}

			_bt_update_transfer_list_view("outbound");

			snprintf(title, sizeof(title), "%s: %s", BT_STR_SHARE, BT_STR_SENT);
			snprintf(str, sizeof(str), BT_TR_STATUS,
				ad->send_data.tr_success,
				ad->send_data.tr_fail);

			DBG("str = [%s] \n", str);

			if (ad->send_noti == NULL) {
				noti = _bt_create_notification(BT_NOTI_T);
				_bt_set_notification_app_launch(noti, CREATE_TR_LIST,
						NOTI_TR_TYPE_OUT, NULL, NULL, 0);
				_bt_set_notification_property(noti, QP_NO_DELETE,
					NOTIFICATION_DISPLAY_APP_NOTIFICATION_TRAY |
					NOTIFICATION_DISPLAY_APP_INDICATOR);

				ad->send_noti_id = _bt_insert_notification(noti,
							title, str,
							BT_ICON_NOTIFICATION_SENT,
							BT_ICON_NOTIFICATION_SENT_INDICATOR);
				ad->send_noti = noti;
			} else {
				_bt_update_notification(ad->send_noti,
							BT_STR_SENT, str,
							BT_ICON_NOTIFICATION_SENT);
			}
		}

		send_index = 0;
		_bt_share_block_sleep(FALSE);
		_bt_set_transfer_indicator(FALSE);
		_remove_transfer_info(node);
		if (!ad->tr_next_data) {
			bt_share_release_tr_data_list(ad->tr_send_list);
			ad->tr_send_list = NULL;
		}
		break;

	case BLUETOOTH_EVENT_OBEX_SERVER_CONNECTION_AUTHORIZE:
		DBG("BLUETOOTH_EVENT_OBEX_SERVER_CONNECTION_AUTHORIZE \n");
		break;
	case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_AUTHORIZE:
		DBG("BT_EVENT_OBEX_TRANSFER_AUTHORIZE \n");
		if (param->result == BLUETOOTH_ERROR_NONE) {
			g_free(server_auth_info.filename);
			server_auth_info.filename = NULL;

			auth_info = param->param_data;
			server_auth_info.filename = g_strdup(auth_info->filename);
			server_auth_info.length = auth_info->length;
			if (server_auth_info.filename)
				__bt_obex_file_push_auth(&server_auth_info);
		}
		break;

	case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_CONNECTED:
		DBG("BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_CONNECTED \n");
		if (param->result == BLUETOOTH_ERROR_NONE) {

			ret = notification_status_message_post(BT_STR_RECEIVING);
			if (ret != NOTIFICATION_ERROR_NONE)
				ERR("notification_status_message_post() is failed : %d\n", ret);
		}
		break;
	case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_STARTED:
		DBG("BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_STARTED \n");
		transfer_info = param->param_data;

		noti  = _bt_create_notification(BT_ONGOING_T);
		_bt_set_notification_property(noti, QP_NO_TICKER,
					NOTIFICATION_DISPLAY_APP_NOTIFICATION_TRAY |
					NOTIFICATION_DISPLAY_APP_INDICATOR);


		if (0 == g_strcmp0(transfer_info->type, TRANSFER_GET)) {
			DBG("TRANSFER_GET \n");
			noti_id = _bt_insert_notification(noti,
					transfer_info->filename, NULL,
					BT_ICON_NOTIFICATION_SENDING,
					BT_ICON_NOTIFICATION_SENDING_INDICATOR);
			_bt_set_notification_app_launch(noti, CREATE_PROGRESS,
					NOTI_TR_TYPE_IN, transfer_info->filename, NULL,
					transfer_info->transfer_id);
			_bt_update_notification_progress(NULL,
					noti_id, 0);
			_bt_update_notification(noti, NULL, NULL, NULL);
		} else {
			DBG("TRANSFER_PUT \n");
			noti_id = _bt_insert_notification(noti,
					transfer_info->filename, NULL,
					BT_ICON_NOTIFICATION_RECEIVING,
					BT_ICON_NOTIFICATION_RECEIVING_INDICATOR);
			_bt_set_notification_app_launch(noti, CREATE_PROGRESS,
					NOTI_TR_TYPE_IN, transfer_info->filename, NULL,
					transfer_info->transfer_id);
			_bt_update_notification(noti, NULL, NULL, NULL);
		}

		data = g_new0(bt_noti_data_t, 1);
		data->noti_handle = noti;
		data->noti_id = noti_id;
		data->transfer_id = transfer_info->transfer_id;

		bt_receive_noti_list = g_slist_append(bt_receive_noti_list, data);
		_bt_set_transfer_indicator(TRUE);
		_bt_share_block_sleep(TRUE);
		break;

	case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_PROGRESS:
		DBG("BT_EVENT_OBEX_TRANSFER_PROGRESS \n");
		if (param->result == BLUETOOTH_ERROR_NONE) {
			transfer_info = param->param_data;

			data = __bt_get_noti_data_by_transfer_id(transfer_info->transfer_id);

			_bt_update_notification_progress(NULL,
					data->noti_id,
					transfer_info->percentage);
		}
		break;

	case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_COMPLETED:
		DBG("BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_COMPLETED \n");
		char mime_type[BT_MIME_TYPE_MAX_LEN] = { 0 };
		unsigned int file_size = 0;
		struct stat file_attr;
		char *file_path = NULL;
		transfer_info = param->param_data;
		_bt_set_transfer_indicator(FALSE);
		_bt_share_block_sleep(FALSE);

		data = __bt_get_noti_data_by_transfer_id(transfer_info->transfer_id);

		if (data) {

			_bt_delete_notification(data->noti_handle);
			bt_receive_noti_list = g_slist_remove(bt_receive_noti_list, data);
			g_free(data);
		}

		if (0 == g_strcmp0(transfer_info->type, TRANSFER_PUT)) {
			DBG("TRANSFER_PUT \n");

			file_path = __get_dest_file_path(transfer_info->file_path);
			DBG(" Filename    =%s  \n", transfer_info->filename);
			DBG(" File Path    =%s  \n", transfer_info->file_path);

			if (aul_get_mime_from_file(file_path, mime_type,
				BT_MIME_TYPE_MAX_LEN) == AUL_R_OK)
				DBG(" mime type    =%s  \n", mime_type);

			if (_bt_filepath_validate(file_path)) {
				if (stat(file_path, &file_attr) == 0)
					file_size = file_attr.st_size;
				else
					file_size = 0;
			}

			if (param->result != BLUETOOTH_ERROR_NONE) {
				ad->recv_data.tr_fail++;
				_bt_add_recv_transfer_status_data(
							transfer_info->device_name,
							transfer_info->filename,
							mime_type, file_size,
							BT_TR_FAIL);
			} else {
				ad->recv_data.tr_success++;
				_bt_add_recv_transfer_status_data(
							transfer_info->device_name,
							transfer_info->filename,
							mime_type, file_size,
							BT_TR_SUCCESS);
			}

			_bt_update_transfer_list_view("inbound");

			snprintf(title, sizeof(title), "%s: %s", BT_STR_SHARE, BT_STR_RECEIVED);
			snprintf(str, sizeof(str), BT_TR_STATUS,
				ad->recv_data.tr_success, ad->recv_data.tr_fail);
			DBG("str = [%s] \n", str);

			if (ad->receive_noti == NULL) {
				noti  = _bt_create_notification(BT_NOTI_T);
				_bt_set_notification_app_launch(noti, CREATE_TR_LIST,
					NOTI_TR_TYPE_IN, NULL, NULL, 0);
				_bt_set_notification_property(noti, QP_NO_DELETE,
						NOTIFICATION_DISPLAY_APP_NOTIFICATION_TRAY |
						NOTIFICATION_DISPLAY_APP_INDICATOR);

				ad->receive_noti_id = _bt_insert_notification(noti,
						title, str,
						BT_ICON_NOTIFICATION_RECEIVED,
						BT_ICON_NOTIFICATION_RECEIVED_INDICATOR);
				ad->receive_noti = noti;
			} else {
				_bt_update_notification(ad->receive_noti,
						BT_STR_RECEIVED, str,
						BT_ICON_NOTIFICATION_RECEIVED);
			}
		}else if(0 == g_strcmp0(transfer_info->type, TRANSFER_GET)){
			DBG("TRANSFER_GET \n");
		}

		if (param->result == BLUETOOTH_ERROR_NONE) {
			bt_file_type_t file_type;
			char *extn;
			bt_file_info_t *info;

			transfer_info = param->param_data;

		if (transfer_info->file_path == NULL) {
				ERR("File path is NULL");
				break;
		}

		if (!g_strcmp0(transfer_info->type, TRANSFER_GET)) {
				DBG("Transfer is GET, so no need to handle");
				break;
		}

			name = __get_dest_file_path(transfer_info->file_path);

			extn = strrchr(name, '.');
			if (NULL != extn)
				extn++;
			file_type = __get_file_type(extn);

			DBG("file type %d", file_type);

			if (transfer_info->server_type == FTP_SERVER ||
					file_type != BT_FILE_VCARD) {
				if (file_type != BT_FILE_VCAL)
					__bt_scan_media_file(name);
				g_free(name);
				break;
			}

			info = g_new0(bt_file_info_t, 1);
			info->file_path = name;
			info->file_type = file_type;

			if (pthread_create(&thread_id, NULL, (void *)&_bt_obex_writeclose,
							info) < 0) {
				ERR("pthread_create() is failed\n");
				__free_file_info(info);
				break;
			}
			if (pthread_detach(thread_id) < 0) {
				ERR("pthread_detach() is failed\n");
			}
			} else {
				DBG("param->result = %d  \n", param->result);
				_bt_create_warning_popup(param->result);
		}


		break;

	default:
		DBG("Unhandled event %x", event);
		break;
	}

}

void _bt_get_default_storage(char *storage)
{
	int val;
	int err;
	char *path;

	if (vconf_get_int(VCONFKEY_SETAPPL_DEFAULT_MEM_BLUETOOTH_INT,
						(void *)&val)) {
		DBG("vconf error\n");
		val = BT_DEFAULT_MEM_PHONE;
	}

	if (val == BT_DEFAULT_MEM_MMC)
		path = BT_DOWNLOAD_MMC_FOLDER;
	else
		path = BT_DOWNLOAD_PHONE_FOLDER;

	if (access(path, W_OK) == 0) {
		g_strlcpy(storage, path, STORAGE_PATH_LEN_MAX);
		DBG("Storage path = [%s]\n", storage);
		return;
	}

	if ((mkdir(path, 0755)) < 0) {
		ERR("mkdir fail![%s]", path);
	}

	g_strlcpy(storage, path, STORAGE_PATH_LEN_MAX);

	DBG("Default storage : %s\n", storage);
}

static gboolean __bt_get_available_memory(long *available_mem_size)
{
	struct statfs fs = {0, };
	int val = -1;
	char *default_memory = NULL;

	if (-1 == vconf_get_int(VCONFKEY_SETAPPL_DEFAULT_MEM_BLUETOOTH_INT,
				(void *)&val)) {
		ERR("vconf error\n");
		return FALSE;
	}
	if (val == 0) { /* Phone memory is 0, MMC is 1 */
		default_memory = BT_DOWNLOAD_PHONE_ROOT;
	} else {
		default_memory = BT_DOWNLOAD_MMC_ROOT;
	}
	if (statfs(default_memory, &fs) != 0) {
		*available_mem_size = 0;
		return FALSE;
	}
	*available_mem_size = fs.f_bavail * (fs.f_bsize/1024);
	return TRUE;
}

static gchar *__bt_get_unique_file_name(char *storage_path, char *filename)
{
	char temp_filepath[BT_FILE_PATH_LEN_MAX] = { 0, };
	char temp_filename[BT_TEMP_FILE_PATH_LEN_MAX] = { 0, };
	char *ext = NULL;
	char *temp;
	unsigned int seq = 1;

	temp = strrchr(filename, '.');

	if (temp != NULL) {
		ext = temp + 1;
		*temp = '\0';
	}

	do {
		if (ext != NULL)
			snprintf(temp_filename, sizeof(temp_filename), "%s_%d.%s",
					filename, seq, ext);
		else
			snprintf(temp_filename, sizeof(temp_filename), "%s_%d",
					filename, seq);

		snprintf(temp_filepath, sizeof(temp_filepath), "%s/%s",
					storage_path, temp_filename);

		/* In below code check is done for unique file name or
		   Max value of integer reached, in this case overwrite
		   the last file */
		if ((access(temp_filepath, F_OK) == 0) && (seq < 65536))
			seq++;
		else
			break;

	} while (1);

	return g_strdup(temp_filename);
}


static void __bt_app_obex_openwrite_requested(bt_obex_server_authorize_into_t
							*server_auth_info)
{
	ret_if(server_auth_info == NULL);
	ret_if(server_auth_info->filename == NULL);

	char temp_filename[BT_FILE_PATH_LEN_MAX] = { 0, };
	char storage[STORAGE_PATH_LEN_MAX] = { 0, };
	char *name = NULL;
	GRegex *regex = NULL;

	/* Check if  file is already present */
	_bt_get_default_storage(storage);

	/* For vcf file type, some device send weird filename like "telecom/pb.vcf"
		This filename should be renamed.
	*/

	regex = g_regex_new("[*\"<>;?|\\^:/]", 0, 0, NULL);
	name = g_regex_replace(regex, server_auth_info->filename, -1, 0, "_", 0, NULL);
	g_regex_unref(regex );
	if (g_strcmp0(name, server_auth_info->filename) != 0) {
		g_free(server_auth_info->filename);
		server_auth_info->filename = name;
	} else {
		g_free(name);
	}

	snprintf(temp_filename, BT_FILE_PATH_LEN_MAX, "%s/%s",
		    storage, server_auth_info->filename);

	DBG("temp_filename : %s", temp_filename);
	if (access(temp_filename, F_OK) == 0) {
		name = server_auth_info->filename;

		server_auth_info->filename = __bt_get_unique_file_name(storage,
									name);

		g_free(name);
	}

	bluetooth_obex_server_accept_authorize(server_auth_info->filename);

	return;
}

static void __bt_obex_file_push_auth(bt_obex_server_authorize_into_t
							*server_auth_info)
{
	 long available_mem_size = 0;

	DBG("+\n");

	 if (__bt_get_available_memory(&available_mem_size) == FALSE) {
		ERR("Unable to get available memory size\n");
		goto reject;
	 }
	 if (available_mem_size < server_auth_info->length / 1024) {
		g_timeout_add(BT_APP_POPUP_LAUNCH_TIMEOUT,
			      (GSourceFunc)_bt_app_popup_memoryfull,
			      NULL);
		goto reject;
	 }

	__bt_app_obex_openwrite_requested(server_auth_info);

	return;

reject:
	bluetooth_obex_server_reject_authorize();

	DBG("-\n");
	return;
}

static bool __bt_vcalendar_handler(calendar_record_h record, void *user_data)
{
	int ret;

	ret = calendar_db_insert_record(record, NULL);
	if (ret != CALENDAR_ERROR_NONE) {
		ERR("calendar_db_insert_record error : %d\n", ret);
	}

	return true;
}

static gboolean __bt_save_v_object(char *file_path,
					    bt_file_type_t file_type)
{
	retv_if(NULL == file_path, FALSE);
	int ret;

	DBG("file_path = %s, file_type = %d\n", file_path, file_type);

	switch (file_type) {
	case BT_FILE_VCARD:
	{
		service_h service;
		service_create(&service);
		service_set_operation(service, "http://samsung.com/appcontrol/operation/vcard_import");
		service_set_mime(service, "text/vcard");
		service_add_extra_data(service, "account_popup", "false");
		service_add_extra_data(service, "file_delete", "true");
		service_set_uri(service, file_path);
		service_send_launch_request(service, NULL, NULL);
		service_destroy(service);
	}
		break;
	case BT_FILE_VCAL:
		ret = calendar_connect();
		if (ret != CALENDAR_ERROR_NONE) {
			ERR("calendar_connect error = %d \n", ret);
			return FALSE;
		}

		ret = calendar_vcalendar_parse_to_calendar_foreach(file_path,
					__bt_vcalendar_handler, NULL);
		if (ret != CALENDAR_ERROR_NONE) {
			ERR("[error] = %d \n", ret);
			ret = calendar_disconnect();
			if ( ret != CALENDAR_ERROR_NONE) {
				ERR("calendar_disconnect error = %d \n", ret);
			}
			return FALSE;
		}
		ret = calendar_disconnect();
		if (ret != CALENDAR_ERROR_NONE) {
			ERR("calendar_disconnect error = %d \n", ret);
			return FALSE;
		}
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

void _bt_obex_writeclose(bt_file_info_t *info)
{
	if (__bt_save_v_object(info->file_path, info->file_type) == FALSE) {
		ERR("Unable to save vObject");
		__bt_scan_media_file(info->file_path);
	} else if (info->file_type == BT_FILE_VCAL) {
		ecore_file_remove(info->file_path);
	}
	__free_file_info(info);

	return NULL;
}


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

#include <glib.h>
#include <vconf.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <locale.h>
#include "applog.h"
#include "bluetooth-api.h"
#include "bt-share-noti-handler.h"
#include "bt-share-notification.h"
#include "bt-share-main.h"


static void __bt_default_memory_changed_cb(keynode_t *node, void *data)
{
	int default_memory = 0;
	char *root_path = NULL;
	char *download_path = NULL;

	DBG("__bt_default_memory_changed_cb\n");

	ret_if (node == NULL);

	DBG("key=%s\n", vconf_keynode_get_name(node));

	if (vconf_keynode_get_type(node) == VCONF_TYPE_INT) {
		/* Phone memory is 0, MMC is 1 */
		default_memory = vconf_keynode_get_int(node);
		root_path = default_memory ? BT_DOWNLOAD_MMC_ROOT : BT_DOWNLOAD_PHONE_ROOT;
		download_path = default_memory ? BT_DOWNLOAD_MMC_FOLDER : BT_DOWNLOAD_PHONE_FOLDER;

		if (access(download_path, W_OK) != 0) {
			if (mkdir(download_path, 0755) < 0) {
				ERR("mkdir fail![%s]", download_path);
			}
		}

		bluetooth_obex_server_set_root(root_path);
		bluetooth_obex_server_set_destination_path(download_path);
	}
}

static void __bt_language_changed_cb(keynode_t *node, void *data)
{
	struct bt_appdata *ad = data;
	char *language = NULL;

	if (node == NULL || data == NULL) {
		ERR("Parameter is NULL\n");
		return;
	}

	if (vconf_keynode_get_type(node) != VCONF_TYPE_STRING) {
		ERR("Invalid vconf key type\n");
		return;
	}

	language = vconf_get_str(VCONFKEY_LANGSET);
	if (language) {
		setenv("LANG", language, 1);
		setenv("LC_MESSAGES",  language, 1);
		setlocale(LC_ALL, language);
		free(language);

	}
}

void _bt_init_vconf_notification(void *data)
{
	int ret;
	ret = vconf_notify_key_changed(VCONFKEY_SETAPPL_DEFAULT_MEM_BLUETOOTH_INT,
			__bt_default_memory_changed_cb, NULL);
	if (ret < 0) {
		DBG("vconf_notify_key_changed failed\n");
	}

	ret = vconf_notify_key_changed(VCONFKEY_LANGSET,
			__bt_language_changed_cb, data);
	if (ret < 0) {
		DBG("vconf_notify_key_changed failed\n");
	}
}

void _bt_deinit_vconf_notification(void)
{
	vconf_ignore_key_changed(VCONFKEY_SETAPPL_DEFAULT_MEM_BLUETOOTH_INT,
			(vconf_callback_fn) __bt_default_memory_changed_cb);

	vconf_ignore_key_changed(VCONFKEY_LANGSET,
			(vconf_callback_fn) __bt_language_changed_cb);
	return;
}


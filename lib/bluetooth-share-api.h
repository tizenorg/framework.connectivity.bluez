/*
 * bluetooth-share-api
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

#ifndef __DEF_BLUETOOTH_SHARE_API_H_
#define __DEF_BLUETOOTH_SHARE_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <db-util.h>

#ifndef EXPORT_API
#define EXPORT_API __attribute__((visibility("default")))
#endif

enum {
	BT_SHARE_ERR_NONE = 0,
	BT_SHARE_ERR_INTERNAL = -1,
	BT_SHARE_ERR_INVALID_PARAM = -2,
	BT_SHARE_ERR_UNKNOWN = -3
}bt_share_err_e;


typedef struct {
	unsigned int id;
	unsigned int sid;
	unsigned int tr_status;
	int timestamp;
	char *file_path;
	char *dev_name;
	char *addr;
	char *type;
	char *content;
	unsigned int size;
}bt_tr_data_t;


typedef enum {
	BT_DB_OUTBOUND,
	BT_DB_INBOUND
} bt_tr_db_table_e;


EXPORT_API sqlite3 *bt_share_open_db(void);

EXPORT_API int bt_share_close_db(sqlite3 *db);

EXPORT_API int bt_share_add_tr_data(sqlite3 *db, int db_table, bt_tr_data_t *data);

EXPORT_API int bt_share_update_tr_data(sqlite3 *db, int db_table, int id, bt_tr_data_t *data);

EXPORT_API bt_tr_data_t *bt_share_get_tr_data(sqlite3 *db, int db_table, int id);

EXPORT_API GSList *bt_share_get_all_tr_data_list(sqlite3 *db, int db_table);

EXPORT_API GSList *bt_share_get_completed_tr_data_list(sqlite3 *db, int db_table);

EXPORT_API GSList *bt_share_get_tr_data_list_by_status(sqlite3 *db, int db_table, int status);

EXPORT_API unsigned int bt_share_get_last_session_id(sqlite3 *db, int db_table);

EXPORT_API int bt_share_release_tr_data_list(GSList *list);

EXPORT_API int bt_share_get_tr_data_count(sqlite3 *db, int db_table);

EXPORT_API int bt_share_remove_tr_data_by_id(sqlite3 *db, int db_table, int id);

EXPORT_API int bt_share_remove_tr_data_by_status(sqlite3 *db, int db_table, int status);

EXPORT_API int bt_share_remove_tr_data_by_notification(sqlite3 *db, int db_table);

EXPORT_API int bt_share_remove_all_tr_data(sqlite3 *db, int db_table);


#ifdef __cplusplus
}
#endif
#endif				/* __DEF_BLUETOOTH_SHARE_API_H_ */

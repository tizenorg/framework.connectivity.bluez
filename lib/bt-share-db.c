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

#include <glib.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <db-util.h>

#include "applog.h"
#include "bt-share-db.h"
#include "bluetooth-share-api.h"


static int __bt_exec_query(sqlite3 *db, char *query)
{
	int ret;
	char *errmsg = NULL;

	retvm_if(db == NULL, BT_SHARE_ERR_INTERNAL, "DB handler is null");
	retvm_if(query == NULL, BT_SHARE_ERR_INVALID_PARAM, "Invalid param");

	ret = sqlite3_exec(db, query, NULL, 0, &errmsg);
	if (ret != SQLITE_OK) {
		DBG("Query: [%s]", query);
		ERR("SQL error: %s\n", errmsg);
		sqlite3_free(errmsg);
		return BT_SHARE_ERR_INTERNAL;
	}

	return BT_SHARE_ERR_NONE;
}


sqlite3 *__bt_db_open(void)
{
	int ret;
	sqlite3 *db = NULL;

	ret = db_util_open(BT_TRANSFER_DB, &db, DB_UTIL_REGISTER_HOOK_METHOD);
	if (ret) {
		ERR("Can't open database: %s", sqlite3_errmsg(db));
		db_util_close(db);
		return NULL;
	}

	return db;
}


static int __bt_db_close(sqlite3 *db)
{
	retv_if(db == NULL, BT_SHARE_ERR_INVALID_PARAM);

	db_util_close(db);
	db = NULL;

	return BT_SHARE_ERR_NONE;
}

static int __bt_db_begin_transaction(sqlite3 *db)
{
	return __bt_exec_query(db, "BEGIN  TRANSACTION");
}

static int __bt_db_end_transaction(sqlite3 *db)
{
	return __bt_exec_query(db, "COMMIT  TRANSACTION");
}

static int __bt_db_insert_record(sqlite3 *db, int db_table, bt_tr_data_t *data)
{
	DBG("+\n");
	int ret = 0;
	char *query = NULL;
	sqlite3_stmt *stmt = NULL;

	retvm_if(db == NULL, BT_SHARE_ERR_INTERNAL, "DB handler is null");
	retvm_if(data == NULL, BT_SHARE_ERR_INTERNAL, "Insert data is null");

	DBG("File: %s Type: %s, Size: %d",data->file_path, data->type, data->size);
	query = sqlite3_mprintf("INSERT INTO %s (id, sid, tr_status, file_path, dev_name, timestamp, addr, type, content, size) VALUES(?, '%d', '%d', '%q', '%q', '%d', '%q', '%q', '%q', '%d');",
	TABLE(db_table), data->sid, data->tr_status, data->file_path,
	data->dev_name, data->timestamp, data->addr, data->type, data->content, data->size);

	if (!query)
		return BT_SHARE_ERR_INTERNAL;

	ret = sqlite3_prepare_v2(db, query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK)
		goto error;

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE)
		goto error;

	sqlite3_finalize(stmt);
	sqlite3_free(query);

	DBG("-\n");
	return BT_SHARE_ERR_NONE;

error:
	ERR("[ERROR] szQuery : %s", query);
	ERR("[ERROR] SQL error msg : %s", (char *)sqlite3_errmsg(db));
	if (stmt) {
		sqlite3_finalize(stmt);
	}

	sqlite3_free(query);

	return BT_SHARE_ERR_INTERNAL;
}

static int __bt_db_update_record(sqlite3 *db, int db_table, int id, bt_tr_data_t *data)
{
	DBG("+\n");
	int ret = 0;
	char query[BT_DB_QUERY_LEN] = {0, };

	retvm_if(db == NULL, BT_SHARE_ERR_INTERNAL, "DB handler is null");
	retvm_if(data == NULL, BT_SHARE_ERR_INTERNAL, "Insert data is null");

	snprintf(query, BT_DB_QUERY_LEN,
		"UPDATE %s SET tr_status=%d, timestamp='%d' WHERE id=%d;",
		TABLE(db_table), data->tr_status, data->timestamp, id);

	__bt_db_begin_transaction(db);
	ret = __bt_exec_query(db, query);
	if (ret != BT_SHARE_ERR_NONE) {
		return BT_SHARE_ERR_INTERNAL;
	}
	__bt_db_end_transaction(db);

	return BT_SHARE_ERR_NONE;
}

static bt_tr_data_t *__bt_db_get_record_by_id(sqlite3 *db, int db_table, int id)
{
	DBG("+\n");
	int ret = 0;
	char query[BT_DB_QUERY_LEN] = {0, };
	sqlite3_stmt *stmt = NULL;
	int idx = 0;
	bt_tr_data_t *data = NULL;

	retvm_if(db == NULL, NULL, "DB handler is null");

	snprintf(query, BT_DB_QUERY_LEN,
		"SELECT * FROM %s WHERE id=%d;", TABLE(db_table), id);

	ret = sqlite3_prepare_v2(db, query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK)
		goto error;

	ret = sqlite3_step(stmt);
	if (ret == SQLITE_ROW) {
		data = g_new0(bt_tr_data_t, 1);
		data->id = INT(stmt, idx++);
		data->sid = INT(stmt, idx++);
		data->tr_status = INT(stmt, idx++);
		data->file_path = g_strdup(TEXT(stmt, idx++));
		data->dev_name = g_strdup(TEXT(stmt, idx++));
		data->timestamp = INT(stmt, idx++);
		data->addr = g_strdup(TEXT(stmt, idx++));
		data->type = g_strdup(TEXT(stmt, idx++));
		data->content = g_strdup(TEXT(stmt, idx++));
	} else {
		goto error;
	}

	DBG_SECURE("%d, %d, %s, %s, %d, %s, %d\n", data->sid, data->tr_status, data->file_path,
		data->dev_name, data->timestamp, data->addr, data->size);

	sqlite3_finalize(stmt);

	DBG("-\n");
	return data;

error:
	ERR("[ERROR] szQuery : %s", query);
	ERR("[ERROR] SQL error msg : %s", (char *)sqlite3_errmsg(db));
	if (stmt) {
		sqlite3_finalize(stmt);
	}
	return NULL;
}


static GSList *__bt_db_get_record_list(sqlite3 *db, const char*query)
{
	int ret;
	sqlite3_stmt *stmt = NULL;
	int idx = 0;
	GSList *slist = NULL;

	retvm_if(db == NULL, NULL, "DB handler is null");

	ret = sqlite3_prepare(db, query, -1, &stmt, NULL);
	if (ret != SQLITE_OK  || stmt == NULL) {
		ERR("SQL error\n");
		sqlite3_finalize(stmt);
		return NULL;
	}

	ret = sqlite3_step(stmt);
	while (ret == SQLITE_ROW) {
		idx = 0;
		bt_tr_data_t *data = g_new0(bt_tr_data_t, 1);
		data->id = INT(stmt, idx++);
		data->sid = INT(stmt, idx++);
		data->tr_status = INT(stmt, idx++);
		data->file_path = g_strdup(TEXT(stmt, idx++));
		data->dev_name = g_strdup(TEXT(stmt, idx++));
		data->timestamp = INT(stmt, idx++);
		data->addr = g_strdup(TEXT(stmt, idx++));
		data->type = g_strdup(TEXT(stmt, idx++));
		data->content = g_strdup(TEXT(stmt, idx++));
		data->size = INT(stmt, idx++);

		slist = g_slist_append(slist, data);

		ret = sqlite3_step(stmt);
	}

	sqlite3_finalize(stmt);

	return slist;
}

static unsigned int __bt_db_get_last_session_id(sqlite3 *db, int db_table)
{
	DBG("+\n");
	int ret = 0;
	unsigned int sid = 0;
	char query[BT_DB_QUERY_LEN] = {0, };
	sqlite3_stmt *stmt = NULL;

	retvm_if(db == NULL, BT_SHARE_ERR_INTERNAL, "DB handler is null");

	snprintf(query, BT_DB_QUERY_LEN,
		"SELECT * FROM %s WHERE id=(SELECT MAX(id) FROM %s);",
		TABLE(db_table), TABLE(db_table));

	ret = sqlite3_prepare_v2(db, query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK)
		goto error;

	ret = sqlite3_step(stmt);
	if (ret == SQLITE_ROW)
		sid = INT(stmt, 1);  /* to get the session id */
	else
		goto error;

	sqlite3_finalize(stmt);

	DBG("-\n");
	return sid;
error:
	ERR("[ERROR] szQuery : %s", query);
	ERR("[ERROR] SQL error msg : %s", (char *)sqlite3_errmsg(db));
	if (stmt) {
		sqlite3_finalize(stmt);
	}

	return 0;
}


static void __bt_release_memory(bt_tr_data_t *data)
{
	retm_if(data == NULL, "Invalid param");

	g_free(data->file_path);
	g_free(data->dev_name);
	g_free(data->addr);
	g_free(data->type);
	g_free(data->content);
	g_free(data);
}


static int __bt_db_release_record_list(GSList *list)
{
	retvm_if(list == NULL, BT_SHARE_ERR_INVALID_PARAM, "Invalid param");

	g_slist_foreach(list, (GFunc)__bt_release_memory, NULL);
	g_slist_free(list);

	return BT_SHARE_ERR_NONE;
}

static int __bt_db_get_record_count(sqlite3 *db, int db_table)
{
	char query[BT_DB_QUERY_LEN] = {0, };
	sqlite3_stmt *stmt;
	int idx;
	int count = 0;
	int ret = 0;

	snprintf(query, sizeof(query), "SELECT COUNT(id) FROM %s;", TABLE(db_table));

	ret = sqlite3_prepare(db, query, -1, &stmt, NULL);
	if (ret != SQLITE_OK  || stmt == NULL) {
		ERR("SQL error\n");
		sqlite3_finalize(stmt);
		return BT_SHARE_ERR_INTERNAL;
	}

	ret = sqlite3_step(stmt);
	if (ret == SQLITE_ROW) {
		idx = 0;
		count = INT(stmt, idx++);
	} else {
		sqlite3_finalize(stmt);
		return BT_SHARE_ERR_INTERNAL;
	}

	sqlite3_finalize(stmt);

	DBG("Record count : %d\n", count);

	return count;
}

static int __bt_db_delete_record(sqlite3 *db, const char *query )
{
	int ret = 0;
	sqlite3_stmt *stmt = NULL;

	retvm_if(db == NULL, BT_SHARE_ERR_INTERNAL, "DB handler is null");

	ret = sqlite3_prepare_v2(db, query, strlen(query), &stmt, NULL);
	if (ret != SQLITE_OK) {
		ERR("sqlite3_prepare_v2(%s) Failed(%s)", query, sqlite3_errmsg(db));
		return BT_SHARE_ERR_INTERNAL;
	}

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE)	{
		ERR("sqlite3_step() Failed(%d)", ret);
		sqlite3_finalize(stmt);
		return BT_SHARE_ERR_INTERNAL;
	}

	sqlite3_finalize(stmt);

	return BT_SHARE_ERR_NONE;
}

EXPORT_API sqlite3 *bt_share_open_db(void)
{
	sqlite3 *db = NULL;

	db = __bt_db_open();
	if (!db)
		return NULL;

	return db;
}

EXPORT_API int bt_share_close_db(sqlite3 *db)
{
	retvm_if(db == NULL, BT_SHARE_ERR_INTERNAL, "DB handle is NULL");

	return __bt_db_close(db);
}

EXPORT_API int bt_share_add_tr_data(sqlite3 *db, int db_table, bt_tr_data_t *data)
{
	retvm_if(data == NULL, BT_SHARE_ERR_INVALID_PARAM, "Invalid param");
	retvm_if(db == NULL, BT_SHARE_ERR_INTERNAL, "DB handle is NULL");

	return __bt_db_insert_record(db, db_table, data);
}

EXPORT_API int bt_share_update_tr_data(sqlite3 *db, int db_table, int id, bt_tr_data_t *data)
{
	retvm_if(data == NULL, BT_SHARE_ERR_INVALID_PARAM, "Invalid param");
	retvm_if(db == NULL, BT_SHARE_ERR_INTERNAL, "DB handle is NULL");

	return __bt_db_update_record(db, db_table, id, data);
}

EXPORT_API bt_tr_data_t *bt_share_get_tr_data(sqlite3 *db, int db_table, int id)
{
	retvm_if(db == NULL, NULL, "DB handle is NULL");

	return __bt_db_get_record_by_id(db, db_table, id);
}

EXPORT_API GSList *bt_share_get_all_tr_data_list(sqlite3 *db, int db_table)
{
	retvm_if(db == NULL, NULL, "DB handle is NULL");

	char query[BT_DB_QUERY_LEN] = {0, };
	snprintf(query, sizeof(query),
			"SELECT * FROM %s ORDER BY TIMESTAMP DESC;",
			TABLE(db_table));

	return __bt_db_get_record_list(db, query);
}

EXPORT_API GSList *bt_share_get_completed_tr_data_list(sqlite3 *db, int db_table)
{
	retvm_if(db == NULL, NULL, "DB handle is NULL");

	char query[BT_DB_QUERY_LEN] = {0, };
	snprintf(query, sizeof(query),
			"SELECT * FROM %s WHERE tr_status != -1 ORDER BY TIMESTAMP DESC;",
			TABLE(db_table));

	return __bt_db_get_record_list(db, query);
}

EXPORT_API GSList *bt_share_get_tr_data_list_by_status(sqlite3 *db, int db_table, int status)
{
	retvm_if(db == NULL, NULL, "DB handle is NULL");

	char query[BT_DB_QUERY_LEN] = {0, };
	snprintf(query, sizeof(query),
			"SELECT * FROM %s WHERE tr_status = %d;",
			TABLE(db_table), status);

	return __bt_db_get_record_list(db, query);
}

EXPORT_API unsigned int bt_share_get_last_session_id(sqlite3 *db, int db_table)
{
	retvm_if(db == NULL, BT_SHARE_ERR_INTERNAL, "DB handle is NULL");

	return __bt_db_get_last_session_id(db, db_table);
}

EXPORT_API int bt_share_release_tr_data_list(GSList *list)
{
	retvm_if(list == NULL, BT_SHARE_ERR_INVALID_PARAM, "Invalid param");

	return __bt_db_release_record_list(list);
}

EXPORT_API int bt_share_get_tr_data_count(sqlite3 *db, int db_table)
{
	retvm_if(db == NULL, BT_SHARE_ERR_INTERNAL, "DB handle is NULL");

	return __bt_db_get_record_count(db, db_table);
}

EXPORT_API int bt_share_remove_tr_data_by_id(sqlite3 *db, int db_table, int id)
{
	retvm_if(db == NULL, BT_SHARE_ERR_INTERNAL, "DB handle is NULL");

	char query[BT_DB_QUERY_LEN] = {0, };
	snprintf(query, sizeof(query),
			"DELETE FROM %s WHERE id=%d;",
			TABLE(db_table), id);

	return __bt_db_delete_record(db, query);
}

EXPORT_API int bt_share_remove_tr_data_by_status(sqlite3 *db, int db_table, int status)
{
	retvm_if(db == NULL, BT_SHARE_ERR_INTERNAL, "DB handle is NULL");

	char query[BT_DB_QUERY_LEN] = {0, };
	snprintf(query, sizeof(query),
			"DELETE FROM %s WHERE tr_status=%d;",
			TABLE(db_table), status);

	return __bt_db_delete_record(db, query);
}

EXPORT_API int bt_share_remove_tr_data_by_notification(sqlite3 *db, int db_table)
{
	retvm_if(db == NULL, BT_SHARE_ERR_INTERNAL, "DB handle is NULL");

	char query[BT_DB_QUERY_LEN] = {0, };
	snprintf(query, sizeof(query),
			"DELETE FROM %s WHERE tr_status > -1;", TABLE(db_table));

	return __bt_db_delete_record(db, query);
}

EXPORT_API int bt_share_remove_all_tr_data(sqlite3 *db, int db_table)
{
	retvm_if(db == NULL, BT_SHARE_ERR_INTERNAL, "DB handle is NULL");

	char query[BT_DB_QUERY_LEN] = {0, };
	snprintf(query, sizeof(query),
			"DELETE FROM %s;", TABLE(db_table));

	return __bt_db_delete_record(db, query);
}


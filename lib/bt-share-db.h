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

 #ifndef __DEF_BT_SHARE_DB_H_
#define __DEF_BT_SHARE_DB_H_

#ifdef __cplusplus
extern "C" {
#endif

#define BT_TRANSFER_DB		"/opt/usr/dbspace/.bluetooth_trasnfer.db"
#define BT_INBOUND_TABLE	"inbound"
#define BT_OUTBOUND_TABLE	"outbound"
#define BT_DB_QUERY_LEN		512

#define TABLE(type) ((type == BT_DB_INBOUND) ? BT_INBOUND_TABLE : BT_OUTBOUND_TABLE)
#define TEXT(s, n) (char *)sqlite3_column_text(s, n)
#define INT(s, n) sqlite3_column_int(s, n)


#ifdef __cplusplus
}
#endif
#endif				/* __DEF_BT_SHARE_DB_H_ */


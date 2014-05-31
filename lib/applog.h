/*
 *  bluetooth-share-api
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

#ifndef __APPLOG_H__
#define __APPLOG_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SYSLOG_INFO(), SYSLOG_ERR(), SYSLOG_DBG() are syslog() wrappers.
 * PRT_INFO(), PRT_ERR(), PRT_DBG() are fprintf() wrappers.
 *
 * If SLP_DEBUG is not defined, SYSLOG_DBG() and PRT_DBG() is ignored.
 *
 * IF SLP_SYSLOG_OUT or SLP_DAEMON is defined,
 *   INFO(), ERR(), DBG() are SYSLOG_XXX()
 * Otherwise,
 *   They are PRT_XXX()
 *
 *
 * warn_if(exrp, fmt, ...)
 *   If expr is true, The fmt string is printed using ERR().
 *
 * ret_if(), retv_if(), retm_if(), retvm_if()
 *   If expr is true, current function return.
 *   Postfix 'v' means that it has a return value and
 *   'm' means that it has output message.
 *
 */

#include <stdio.h>
#include <dlog.h>

#undef LOG_TAG
#define LOG_TAG "BLUETOOTH_SHARE_DB"

#define INFO(fmt, arg...) \
	SLOGI(fmt, ##arg)

#define ERR(fmt, arg...) \
	SLOGE(fmt, ##arg)

#define DBG(fmt, arg...) \
	SLOGD(fmt, ##arg)

#define DBG_SECURE(fmt, args...) SECURE_SLOGD(fmt, ##args)
#define ERR_SECURE(fmt, args...) SECURE_SLOGE(fmt, ##args)
#define INFO_SECURE(fmt, args...) SECURE_SLOGI(fmt, ##args)

#define warn_if(expr, fmt, arg...) do { \
		if (expr) { \
			ERR(fmt, ##arg); \
		} \
	} while (0)
#define ret_if(expr) do { \
		if (expr) { \
			return; \
		} \
	} while (0)
#define retv_if(expr, val) do { \
		if (expr) { \
			return (val); \
		} \
	} while (0)
#define retm_if(expr, fmt, arg...) do { \
		if (expr) { \
			ERR(fmt, ##arg); \
			return; \
		} \
	} while (0)
#define retvm_if(expr, val, fmt, arg...) do { \
		if (expr) { \
			ERR(fmt, ##arg); \
			return (val); \
		} \
	} while (0)

#ifdef __cplusplus
}
#endif
#endif				/* __APPLOG_H__ */


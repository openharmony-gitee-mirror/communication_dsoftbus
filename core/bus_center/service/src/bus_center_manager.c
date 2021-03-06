/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bus_center_manager.h"

#include <stdint.h>
#include <stdlib.h>

#include "bus_center_event.h"
#include "lnn_async_callback_utils.h"
#include "lnn_discovery_manager.h"
#include "lnn_distributed_net_ledger.h"
#include "lnn_event_monitor.h"
#include "lnn_lane_manager.h"
#include "lnn_local_net_ledger.h"
#include "lnn_network_manager.h"
#include "lnn_net_builder.h"
#include "lnn_sync_item_info.h"
#include "lnn_time_sync_manager.h"
#include "softbus_errcode.h"
#include "softbus_feature_config.h"
#include "softbus_log.h"
#include "softbus_utils.h"

#define DEFAULT_DELAY_LEN 0
#define RETRY_MAX 10

typedef int32_t (*LnnInitDelayImpl)(void);

typedef enum {
    INIT_LOCAL_LEDGER_DELAY_TYPE = 0,
    INIT_NETWORK_MANAGER_DELAY_TYPE,
    INIT_NETBUILDER_DELAY_TYPE,
    INIT_DELAY_MAX_TYPE,
} InitDelayType;

typedef struct {
    LnnInitDelayImpl implInit;
    bool isInit;
} InitDelayImpl;

typedef struct {
    InitDelayImpl initDelayImpl[INIT_DELAY_MAX_TYPE];
    int32_t delayLen;
} LNNLocalConfigInit;

static LNNLocalConfigInit g_lnnLocalConfigInit = {
    .initDelayImpl = {
        [INIT_LOCAL_LEDGER_DELAY_TYPE] = {
            .implInit = LnnInitLocalLedgerDelay,
            .isInit = false,
        },
        [INIT_NETWORK_MANAGER_DELAY_TYPE] = {
            .implInit = LnnInitNetworkManagerDelay,
            .isInit = false,
        },
        [INIT_NETBUILDER_DELAY_TYPE] = {
            .implInit = LnnInitNetBuilderDelay,
            .isInit = false,
        },
    },
};

static void LnnGetUdidDelayLenInit(void)
{
    if (SoftbusGetConfig(SOFTBUS_INT_LNN_UDID_INIT_DELAY_LEN,
        (unsigned char*)&g_lnnLocalConfigInit.delayLen, sizeof(g_lnnLocalConfigInit.delayLen)) != SOFTBUS_OK) {
        SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_ERROR, "get lnn udid delay len fail, use default value");
        g_lnnLocalConfigInit.delayLen = DEFAULT_DELAY_LEN;
    }
    SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_INFO, "lnn udid delay len is %u", g_lnnLocalConfigInit.delayLen);
}

static void LnnInitLocalConfigLooper(void *para)
{
    (void)para;
    static int32_t retry = 0;
    if (retry > RETRY_MAX) {
        SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_ERROR, "try LnnInitLocalConfigLooper max times");
        return;
    }
    int32_t ret = SOFTBUS_OK;
    uint32_t i;
    for (i = 0; i < INIT_DELAY_MAX_TYPE; ++i) {
        if (g_lnnLocalConfigInit.initDelayImpl[i].implInit == NULL) {
            continue;
        }
        if (!g_lnnLocalConfigInit.initDelayImpl[i].isInit
            && g_lnnLocalConfigInit.initDelayImpl[i].implInit() != SOFTBUS_OK) {
            SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_ERROR, "init delay impl(%u) failed", i);
            ret = SOFTBUS_ERR;
        } else {
            g_lnnLocalConfigInit.initDelayImpl[i].isInit = true;
        }
    }
    if (ret != SOFTBUS_OK) {
        retry++;
        SoftBusLooper *looper = GetLooper(LOOP_TYPE_DEFAULT);
        ret = LnnAsyncCallbackDelayHelper(looper, LnnInitLocalConfigLooper, NULL, g_lnnLocalConfigInit.delayLen);
        if (ret != SOFTBUS_OK) {
            SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_ERROR, "LnnInitLocalConfigLooper LnnAsyncCallbackDelayHelper fail");
        }
    }
}

static int32_t LnnInitLocalConfigDelay(void)
{
    SoftBusLooper *looper = GetLooper(LOOP_TYPE_DEFAULT);
    int32_t ret = LnnAsyncCallbackDelayHelper(looper, LnnInitLocalConfigLooper, NULL, g_lnnLocalConfigInit.delayLen);
    if (ret != SOFTBUS_OK) {
        SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_ERROR, "LnnInitLocalConfigDelay LnnAsyncCallbackDelayHelper fail");
    }
    return ret;
}

int32_t BusCenterServerInit(void)
{
    if (LnnInitLocalLedger() != SOFTBUS_OK) {
        SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_ERROR, "init local net ledger fail!");
        return SOFTBUS_ERR;
    }
    if (LnnInitDistributedLedger() != SOFTBUS_OK) {
        SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_ERROR, "init distributed net ledger fail!");
        return SOFTBUS_ERR;
    }
    if (LnnInitSyncLedgerItem() != SOFTBUS_OK) {
        SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_ERROR, "init sync ledger item fail!");
        return SOFTBUS_ERR;
    }
    if (LnnInitEventMonitor() != SOFTBUS_OK) {
        SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_ERROR, "init event monitor failed");
        return SOFTBUS_ERR;
    }
    if (LnnInitDiscoveryManager() != SOFTBUS_OK) {
        SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_ERROR, "init lnn discovery manager fail!");
        return SOFTBUS_ERR;
    }
    if (LnnInitNetworkManager() != SOFTBUS_OK) {
        SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_ERROR, "init lnn network manager fail!");
        return SOFTBUS_ERR;
    }
    if (LnnInitNetBuilder() != SOFTBUS_OK) {
        SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_ERROR, "init net builder fail!");
        return SOFTBUS_ERR;
    }
    if (InitLaneManager() != SOFTBUS_OK) {
        SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_ERROR, "init InitLaneManager fail!");
        return SOFTBUS_ERR;
    }
    LnnTimeSyncInit(LnnNotifyTimeSyncResult);
    LnnGetUdidDelayLenInit();
    if (LnnInitLocalConfigDelay() != SOFTBUS_OK) {
        SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_ERROR, "init local config delay fail!");
        return SOFTBUS_ERR;
    }
    SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_INFO, "bus center server init ok");

    return SOFTBUS_OK;
}

void BusCenterServerDeinit(void)
{
    LnnDeinitLocalLedger();
    LnnDeinitDistributedLedger();
    LnnDeinitNetBuilder();
    LnnDeinitSyncLedgerItem();
    LnnDeinitEventMonitor();
    LnnTimeSyncDeinit();
    LnnDeinitNetworkManager();
    SoftBusLog(SOFTBUS_LOG_LNN, SOFTBUS_LOG_INFO, "bus center server deinit");
}

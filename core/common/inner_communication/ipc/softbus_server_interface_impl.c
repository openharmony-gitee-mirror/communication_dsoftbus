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

#include <ohos_init.h>
#include <stdlib.h>

#include "iproxy_server.h"
#include "samgr_lite.h"
#include "securec.h"
#include "softbus_client_info_manager.h"
#include "softbus_errcode.h"
#include "softbus_interface.h"
#include "softbus_log.h"
#include "softbus_server_weak.h"

#define STACK_SIZE 0x800
#define QUEUE_SIZE 20

typedef struct {
    INHERIT_SERVER_IPROXY;
} DefaultFeatureApi;

typedef struct {
    INHERIT_SERVICE;
    INHERIT_IUNKNOWNENTRY(DefaultFeatureApi);
    Identity identity;
} SoftbusSamgrService;

static const char *GetName(Service *service)
{
    (void)service;
    return SOFTBUS_SERVICE;
}

static BOOL Initialize(Service *service, Identity identity)
{
    if (service == NULL) {
        LOG_ERR("invalid param");
        return TRUE;
    }

    SoftbusSamgrService *samgrService = (SoftbusSamgrService *)service;
    samgrService->identity = identity;
    return TRUE;
}

static BOOL MessageHandle(Service *service, Request *msg)
{
    if (service == NULL || msg == NULL) {
        LOG_ERR("invalid param");
        return TRUE;
    }
    return FALSE;
}

static TaskConfig GetTaskConfig(Service *service)
{
    (void)service;
    TaskConfig config = { LEVEL_HIGH, PRI_BELOW_NORMAL, STACK_SIZE, QUEUE_SIZE, SHARED_TASK };
    return config;
}

static void ClientDeathCallback(const IpcContext *context, void *ipcMsg, IpcIo *data, void *arg)
{
    if (arg == NULL) {
        LOG_ERR("package name is NULL.");
        return;
    }
    struct CommonScvId svcId = {0};
    if (SERVER_GetIdentityByPkgName((const char *)arg, &svcId) != SOFTBUS_OK) {
        LOG_ERR("not found client by package name.");
        SoftBusFree(arg);
        arg = NULL;
        return;
    }
    SERVER_UnregisterService((const char *)arg);
    SoftBusFree(arg);
    arg = NULL;
#ifdef __LINUX__
    BinderRelease(svcId.ipcCtx, svcId.handle);
#endif
    SvcIdentity sid = {0};
    sid.handle = svcId.handle;
    sid.token = svcId.token;
    sid.cookie = svcId.cookie;
    UnregisterDeathCallback(sid, svcId.cbId);
}

static int ServerRegisterService(void *origin, IpcIo *req, IpcIo *reply)
{
    size_t len = 0;
    int ret = SOFTBUS_ERR;
    struct CommonScvId svcId = {0};

    uint8_t *name = IpcIoPopString(req, &len);
    SvcIdentity *svc = IpcIoPopSvc(req);
    if (name == NULL || svc == NULL || len == 0) {
        LOG_ERR("get data fail");
        goto EXIT;
    }

    svcId.handle = svc->handle;
    svcId.token = svc->token;
    svcId.cookie = svc->cookie;
    SvcIdentity sid = *svc;
#ifdef __LINUX__
    svcId.ipcCtx = svc->ipcContext;
    BinderAcquire(svcId.ipcCtx, svcId.handle);
    SoftBusFree(svc);
    svc = NULL;
#endif
    char *pkgName = (char *)SoftBusMalloc(len + 1);
    if (pkgName == NULL) {
        LOG_ERR("softbus malloc failed!");
        goto EXIT;
    }
    if (strcpy_s(pkgName, len + 1, (const char *)name) != EOK) {
        LOG_ERR("softbus strcpy_s failed!");
        SoftBusFree(pkgName);
        goto EXIT;
    }
    uint32_t cbId = 0;
    RegisterDeathCallback(NULL, sid, ClientDeathCallback, pkgName, &cbId);
    svcId.cbId = cbId;
    ret = SERVER_RegisterService((const char *)name, &svcId);
EXIT:
    IpcIoPushInt32(reply, ret);
    return SOFTBUS_OK;
}

typedef struct {
    enum SoftBusFuncId id;
    int (*func)(void *origin, IpcIo *req, IpcIo *reply);
} ServerInvokeCmd;

ServerInvokeCmd g_serverInvokeCmdTbl[] = {
    { MANAGE_REGISTER_SERVICE, ServerRegisterService },
    { SERVER_PUBLISH_SERVICE, ServerPublishService },
    { SERVER_UNPUBLISH_SERVICE,  ServerUnPublishService },
    { SERVER_CREATE_SESSION_SERVER, ServerCreateSessionServer },
    { SERVER_REMOVE_SESSION_SERVER, ServerRemoveSessionServer },
    { SERVER_OPEN_SESSION, ServerOpenSession },
    { SERVER_CLOSE_CHANNEL, ServerCloseSession },
    { SERVER_SESSION_SENDMSG, ServerSendSessionMsg },
    { SERVER_START_DISCOVERY, ServerStartDiscovery},
    { SERVER_STOP_DISCOVERY, ServerStopDiscovery},
};

static int32_t Invoke(const IServerProxy *iProxy, int funcId, const void *origin,
    const IpcIo *req, const IpcIo *reply)
{
    LOG_INFO("RECEIVE FUNCID:%d", funcId);
    int tblSize = sizeof(g_serverInvokeCmdTbl) / sizeof(ServerInvokeCmd);
    for (int i = 0; i < tblSize; i++) {
        if (funcId == g_serverInvokeCmdTbl[i].id) {
            return g_serverInvokeCmdTbl[i].func(origin, req, reply);
        }
    }
    LOG_ERR("not support func[%d]", funcId);
    return SOFTBUS_ERR;
}

static SoftbusSamgrService g_samgrService = {
    .GetName = GetName,
    .Initialize = Initialize,
    .MessageHandle = MessageHandle,
    .GetTaskConfig = GetTaskConfig,
    SERVER_IPROXY_IMPL_BEGIN,
    .Invoke = Invoke,
    IPROXY_END,
};

void __attribute__((weak)) HOS_SystemInit(void)
{
    SAMGR_Bootstrap();
    return;
}

int ServerProvideInterfaceImplInit(void)
{
    HOS_SystemInit();
    return SOFTBUS_OK;
}

static void Init(void)
{
    SAMGR_GetInstance()->RegisterService((Service *)&g_samgrService);
    SAMGR_GetInstance()->RegisterDefaultFeatureApi(SOFTBUS_SERVICE, GET_IUNKNOWN(g_samgrService));
    LOG_INFO("Init success %s", SOFTBUS_SERVICE);
}
SYSEX_SERVICE_INIT(Init);
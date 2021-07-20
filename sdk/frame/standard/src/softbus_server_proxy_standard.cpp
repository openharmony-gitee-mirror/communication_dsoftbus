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

#include "softbus_server_proxy_standard.h"

#include "message_parcel.h"
#include "softbus_client_stub.h"
#include "softbus_errcode.h"
#include "softbus_ipc_def.h"
#include "softbus_log.h"

namespace OHOS {
sptr<IRemoteObject> SoftBusServerProxyFrame::clientCallbackStub_;
std::mutex SoftBusServerProxyFrame::instanceLock;

sptr<IRemoteObject> SoftBusServerProxyFrame::GetRemoteInstance()
{
    if (clientCallbackStub_ == nullptr) {
        std::lock_guard<std::mutex> autoLock(instanceLock);
        if (clientCallbackStub_ == nullptr) {
            clientCallbackStub_ = sptr<IRemoteObject>(new (std::nothrow) SoftBusClientStub());
        }
    }
    return clientCallbackStub_;
}

int32_t SoftBusServerProxyFrame::StartDiscovery(const char *pkgName, const SubscribeInfo *info)
{
    return SOFTBUS_OK;
}

int32_t SoftBusServerProxyFrame::StopDiscovery(const char *pkgName, int subscribeId)
{
    return SOFTBUS_OK;
}

int32_t SoftBusServerProxyFrame::PublishService(const char *pkgName, const PublishInfo *info)
{
    return SOFTBUS_OK;
}

int32_t SoftBusServerProxyFrame::UnPublishService(const char *pkgName, int publishId)
{
    return SOFTBUS_OK;
}

int32_t SoftBusServerProxyFrame::SoftbusRegisterService(const char *clientPkgName, const sptr<IRemoteObject>& object)
{
    sptr<IRemoteObject> remote = Remote();
    if (remote == nullptr) {
        LOG_ERR("remote is nullptr!");
        return SOFTBUS_ERR;
    }

    sptr<IRemoteObject> clientStub = SoftBusServerProxyFrame::GetRemoteInstance();
    if (clientStub == nullptr) {
        LOG_ERR("client stub is nullptr!");
        return SOFTBUS_ERR;
    }
    MessageParcel data;
    int ret = data.WriteRemoteObject(clientStub);
    if (!ret) {
        LOG_ERR("SoftbusRegisterService write remote object failed!");
        return SOFTBUS_ERR;
    }
    ret = data.WriteCString(clientPkgName);
    if (!ret) {
        LOG_ERR("SoftbusRegisterService write clientPkgName failed!");
        return SOFTBUS_ERR;
    }

    MessageParcel reply;
    MessageOption option;
    int32_t err = remote->SendRequest(MANAGE_REGISTER_SERVICE, data, reply, option);
    if (err != 0) {
        LOG_ERR("SoftbusRegisterService send request failed!");
        return SOFTBUS_ERR;
    }
    int32_t serverRet = 0;
    ret = reply.ReadInt32(serverRet);
    if (!ret) {
        LOG_ERR("SoftbusRegisterService read serverRet failed!");
        return SOFTBUS_ERR;
    }
    return SOFTBUS_OK;
}

int32_t SoftBusServerProxyFrame::CreateSessionServer(const char *pkgName, const char *sessionName)
{
    return SOFTBUS_OK;
}

int32_t SoftBusServerProxyFrame::RemoveSessionServer(const char *pkgName, const char *sessionName)
{
    return SOFTBUS_OK;
}

int32_t SoftBusServerProxyFrame::OpenSession(const char *mySessionName, const char *peerSessionName,
    const char *peerDeviceId, const char *groupId, int32_t flags)
{
    return SOFTBUS_OK;
}

int32_t SoftBusServerProxyFrame::CloseChannel(int32_t channelId, int32_t channelType)
{
    return SOFTBUS_OK;
}

int32_t SoftBusServerProxyFrame::SendMessage(int32_t channelId, const void *data, uint32_t len, int32_t msgType)
{
    return SOFTBUS_OK;
}

int32_t SoftBusServerProxyFrame::JoinLNN(const char *pkgName, void *addr, uint32_t addrTypeLen)
{
    return SOFTBUS_OK;
}

int32_t SoftBusServerProxyFrame::LeaveLNN(const char *pkgName, const char *networkId)
{
    return SOFTBUS_OK;
}

int32_t SoftBusServerProxyFrame::GetAllOnlineNodeInfo(const char *pkgName, void **info, uint32_t infoTypeLen,
    int *infoNum)
{
    return SOFTBUS_OK;
}

int32_t SoftBusServerProxyFrame::GetLocalDeviceInfo(const char *pkgName, void *info, uint32_t infoTypeLen)
{
    return SOFTBUS_OK;
}

int32_t SoftBusServerProxyFrame::GetNodeKeyInfo(const char *pkgName, const char *networkId, int key,
    unsigned char *buf, uint32_t len)
{
    return SOFTBUS_OK;
}
}
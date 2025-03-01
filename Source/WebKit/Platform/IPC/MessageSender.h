/**
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Forward.h>

namespace IPC {

class Connection;
class Encoder;
class Timeout;
enum class SendOption : uint8_t;
enum class SendSyncOption : uint8_t;
struct AsyncReplyIDType;
struct ConnectionAsyncReplyHandler;
template<typename> struct ConnectionSendSyncResult;
using AsyncReplyID = ObjectIdentifier<AsyncReplyIDType>;

class MessageSender {
public:
    virtual ~MessageSender();

    template<typename T> inline bool send(T&& message);
    template<typename T> inline bool send(T&& message, OptionSet<SendOption>);
    template<typename T> inline bool send(T&& message, uint64_t destinationID);
    template<typename T> inline bool send(T&& message, uint64_t destinationID, OptionSet<SendOption>);
    template<typename T, typename U> inline bool send(T&& message, ObjectIdentifier<U> destinationID);
    template<typename T, typename U> inline bool send(T&& message, ObjectIdentifier<U> destinationID, OptionSet<SendOption>);

    template<typename T> using SendSyncResult = ConnectionSendSyncResult<T>;
    template<typename T> inline SendSyncResult<T> sendSync(T&& message);
    template<typename T> inline SendSyncResult<T> sendSync(T&& message, Timeout);
    template<typename T> inline SendSyncResult<T> sendSync(T&& message, Timeout, OptionSet<SendSyncOption>);
    template<typename T> inline SendSyncResult<T> sendSync(T&& message, uint64_t destinationID);
    template<typename T> inline SendSyncResult<T> sendSync(T&& message, uint64_t destinationID, Timeout);
    template<typename T> inline SendSyncResult<T> sendSync(T&& message, uint64_t destinationID, Timeout, OptionSet<SendSyncOption>);
    template<typename T, typename U> inline SendSyncResult<T> sendSync(T&& message, ObjectIdentifier<U> destinationID);
    template<typename T, typename U> inline SendSyncResult<T> sendSync(T&& message, ObjectIdentifier<U> destinationID, Timeout);
    template<typename T, typename U> inline SendSyncResult<T> sendSync(T&& message, ObjectIdentifier<U> destinationID, Timeout, OptionSet<SendSyncOption>);

    using AsyncReplyID = IPC::AsyncReplyID;
    template<typename T, typename C> inline AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler);
    template<typename T, typename C> inline AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler, OptionSet<SendOption>);
    template<typename T, typename C> inline AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler, uint64_t destinationID);
    template<typename T, typename C> inline AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler, uint64_t destinationID, OptionSet<SendOption>);
    template<typename T, typename C, typename U> inline AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler, ObjectIdentifier<U> destinationID);
    template<typename T, typename C, typename U> inline AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler, ObjectIdentifier<U> destinationID, OptionSet<SendOption>);

    virtual bool sendMessage(UniqueRef<Encoder>&&, OptionSet<SendOption>);

    using AsyncReplyHandler = ConnectionAsyncReplyHandler;
    virtual bool sendMessageWithAsyncReply(UniqueRef<Encoder>&&, AsyncReplyHandler, OptionSet<SendOption>);

private:
    virtual Connection* messageSenderConnection() const = 0;
    virtual uint64_t messageSenderDestinationID() const = 0;
};

} // namespace IPC

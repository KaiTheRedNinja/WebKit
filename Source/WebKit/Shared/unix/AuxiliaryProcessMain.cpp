/*
 * Copyright (C) 2014 Igalia S.L.
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

#include "config.h"
#include "AuxiliaryProcessMain.h"

#include <JavaScriptCore/Options.h>
#include <WebCore/ProcessIdentifier.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#if ENABLE(BREAKPAD)
#include "unix/BreakpadExceptionHandler.h"
#endif

namespace WebKit {

AuxiliaryProcessMainCommon::AuxiliaryProcessMainCommon()
{
#if ENABLE(BREAKPAD)
    installBreakpadExceptionHandler();
#endif
}

bool AuxiliaryProcessMainCommon::parseCommandLine(int argc, char** argv)
{
    ASSERT(argc >= 3);
    if (argc < 3)
        return false;

    m_parameters.processIdentifier = makeObjectIdentifier<WebCore::ProcessIdentifierType>(atoll(argv[1]));
    m_parameters.connectionIdentifier = IPC::Connection::Identifier { atoi(argv[2]) };
#if ENABLE(DEVELOPER_MODE)
    if (argc > 3 && argv[3] && !strcmp(argv[3], "--configure-jsc-for-testing"))
        JSC::Config::configureForTesting();
#endif
    return true;
}

void AuxiliaryProcess::platformInitialize(const AuxiliaryProcessInitializationParameters&)
{
    struct sigaction signalAction;
    memset(&signalAction, 0, sizeof(signalAction));
    RELEASE_ASSERT(!sigemptyset(&signalAction.sa_mask));
    signalAction.sa_handler = SIG_IGN;
    RELEASE_ASSERT(!sigaction(SIGPIPE, &signalAction, nullptr));
}

} // namespace WebKit

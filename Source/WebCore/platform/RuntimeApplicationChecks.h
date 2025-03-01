/*
 * Copyright (C) 2009-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <optional>
#include <wtf/Forward.h>

namespace WebCore {

WEBCORE_EXPORT void setPresentingApplicationPID(int);
WEBCORE_EXPORT int presentingApplicationPID();

enum class AuxiliaryProcessType : uint8_t {
    WebContent,
    Network,
    Plugin,
#if ENABLE(GPU_PROCESS)
    GPU,
#endif
};

WEBCORE_EXPORT void setAuxiliaryProcessType(AuxiliaryProcessType);
WEBCORE_EXPORT void setAuxiliaryProcessTypeForTesting(std::optional<AuxiliaryProcessType>);
WEBCORE_EXPORT bool checkAuxiliaryProcessType(AuxiliaryProcessType);
WEBCORE_EXPORT std::optional<AuxiliaryProcessType> processType();
WEBCORE_EXPORT const char* processTypeDescription(std::optional<AuxiliaryProcessType>);

bool isInAuxiliaryProcess();
inline bool isInWebProcess() { return checkAuxiliaryProcessType(AuxiliaryProcessType::WebContent); }
inline bool isInNetworkProcess() { return checkAuxiliaryProcessType(AuxiliaryProcessType::Network); }
inline bool isInGPUProcess()
{
#if ENABLE(GPU_PROCESS)
    return checkAuxiliaryProcessType(AuxiliaryProcessType::GPU);
#else
    return false;
#endif
}

#if PLATFORM(COCOA)

WEBCORE_EXPORT void setApplicationBundleIdentifier(const String&);
WEBCORE_EXPORT void setApplicationBundleIdentifierOverride(const String&);
WEBCORE_EXPORT String applicationBundleIdentifier();
WEBCORE_EXPORT void clearApplicationBundleIdentifierTestingOverride();

WEBCORE_EXPORT void setPresentingApplicationBundleIdentifier(const String&);
WEBCORE_EXPORT const String& presentingApplicationBundleIdentifier();

namespace CocoaApplication {

WEBCORE_EXPORT bool isIBooks();
WEBCORE_EXPORT bool isWebkitTestRunner();

}

#if PLATFORM(MAC)

namespace MacApplication {

WEBCORE_EXPORT bool isAdobeInstaller();
WEBCORE_EXPORT bool isAppleMail();
WEBCORE_EXPORT bool isMiniBrowser();
bool isQuickenEssentials();
WEBCORE_EXPORT bool isSafari();
bool isSolidStateNetworksDownloader();
WEBCORE_EXPORT bool isVersions();
WEBCORE_EXPORT bool isHRBlock();
WEBCORE_EXPORT bool isIAdProducer();
WEBCORE_EXPORT bool isEpsonSoftwareUpdater();

} // MacApplication

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

namespace IOSApplication {

WEBCORE_EXPORT bool isAppleApplication();
WEBCORE_EXPORT bool isCardiogram();
WEBCORE_EXPORT bool isCrunchyroll();
WEBCORE_EXPORT bool isDataActivation();
WEBCORE_EXPORT bool isDoubleDown();
WEBCORE_EXPORT bool isDumpRenderTree();
WEBCORE_EXPORT bool isESPNFantasySports();
WEBCORE_EXPORT bool isEssentialSkeleton();
WEBCORE_EXPORT bool isEventbrite();
WEBCORE_EXPORT bool isEvernote();
WEBCORE_EXPORT bool isFIFACompanion();
WEBCORE_EXPORT bool isFeedly();
WEBCORE_EXPORT bool isFirefox();
WEBCORE_EXPORT bool isIMDb();
WEBCORE_EXPORT bool isJWLibrary();
WEBCORE_EXPORT bool isLaBanquePostale();
WEBCORE_EXPORT bool isLutron();
WEBCORE_EXPORT bool isMailCompositionService();
WEBCORE_EXPORT bool isMiniBrowser();
WEBCORE_EXPORT bool isMobileMail();
WEBCORE_EXPORT bool isMobileSafari();
WEBCORE_EXPORT bool isNews();
WEBCORE_EXPORT bool isNike();
WEBCORE_EXPORT bool isNoggin();
WEBCORE_EXPORT bool isOKCupid();
WEBCORE_EXPORT bool isPaperIO();
WEBCORE_EXPORT bool isPocketCity();
WEBCORE_EXPORT bool isSafariViewService();
WEBCORE_EXPORT bool isStocks();
WEBCORE_EXPORT bool isTheSecretSocietyHiddenMystery();
WEBCORE_EXPORT bool isWebBookmarksD();
WEBCORE_EXPORT bool isWebProcess();
bool isBackboneApp();
bool isIBooksStorytime();
bool isMobileStore();
bool isMoviStarPlus();
bool isSpringBoard();
bool isUNIQLOApp();
bool isWechat();

} // IOSApplication

#endif // PLATFORM(IOS_FAMILY)

#endif // PLATFORM(COCOA)

} // namespace WebCore

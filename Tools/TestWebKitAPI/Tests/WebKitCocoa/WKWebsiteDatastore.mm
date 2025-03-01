/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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

#import "config.h"

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKWebsiteDataSize.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/WTFString.h>

static RetainPtr<NSURLCredential> persistentCredential;
static bool usePersistentCredentialStorage = false;

@interface NavigationTestDelegate : NSObject <WKNavigationDelegate>
@end

@implementation NavigationTestDelegate {
    bool _hasFinishedNavigation;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    
    _hasFinishedNavigation = false;
    
    return self;
}

- (void)waitForDidFinishNavigation
{
    TestWebKitAPI::Util::run(&_hasFinishedNavigation);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    _hasFinishedNavigation = true;
}

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler
{
    static bool firstChallenge = true;
    if (firstChallenge) {
        firstChallenge = false;
        persistentCredential = adoptNS([[NSURLCredential alloc] initWithUser:@"username" password:@"password" persistence:(usePersistentCredentialStorage ? NSURLCredentialPersistencePermanent: NSURLCredentialPersistenceForSession)]);
        return completionHandler(NSURLSessionAuthChallengeUseCredential, persistentCredential.get());
    }
    return completionHandler(NSURLSessionAuthChallengeUseCredential, nil);
}
@end

@interface WKWebsiteDataStoreMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation WKWebsiteDataStoreMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    lastScriptMessage = message;
}

@end

namespace TestWebKitAPI {


// FIXME: Re-enable this test once webkit.org/b/208451 is resolved.
#if !PLATFORM(IOS)
TEST(WKWebsiteDataStore, RemoveAndFetchData)
{
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ(0u, dataRecords.count);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}
#endif // !PLATFORM(IOS)

TEST(WKWebsiteDataStore, RemoveEphemeralData)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    __block bool done = false;
    [[configuration websiteDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler: ^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKWebsiteDataStore, FetchNonPersistentCredentials)
{
    HTTPServer server(HTTPServer::respondWithChallengeThenOK);
    
    usePersistentCredentialStorage = false;
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    [configuration setWebsiteDataStore:websiteDataStore];
    auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        int credentialCount = dataRecords.count;
        ASSERT_EQ(credentialCount, 1);
        for (WKWebsiteDataRecord *record in dataRecords)
            ASSERT_TRUE([[record displayName] isEqualToString:@"127.0.0.1"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKWebsiteDataStore, FetchPersistentCredentials)
{
    HTTPServer server(HTTPServer::respondWithChallengeThenOK);

    usePersistentCredentialStorage = true;
    auto websiteDataStore = [WKWebsiteDataStore defaultDataStore];
    auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    // Make sure no credential left by previous tests.
    auto protectionSpace = adoptNS([[NSURLProtectionSpace alloc] initWithHost:@"127.0.0.1" port:server.port() protocol:NSURLProtectionSpaceHTTP realm:@"testrealm" authenticationMethod:NSURLAuthenticationMethodHTTPBasic]);
    [[webView configuration].processPool _clearPermanentCredentialsForProtectionSpace:protectionSpace.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        int credentialCount = dataRecords.count;
        EXPECT_EQ(credentialCount, 0);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // Clear persistent credentials created by this test.
    [[webView configuration].processPool _clearPermanentCredentialsForProtectionSpace:protectionSpace.get()];
}

TEST(WKWebsiteDataStore, RemoveNonPersistentCredentials)
{
    HTTPServer server(HTTPServer::respondWithChallengeThenOK);

    usePersistentCredentialStorage = false;
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    [configuration setWebsiteDataStore:websiteDataStore];
    auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    __block RetainPtr<WKWebsiteDataRecord> expectedRecord;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        int credentialCount = dataRecords.count;
        ASSERT_GT(credentialCount, 0);
        for (WKWebsiteDataRecord *record in dataRecords) {
            auto name = [record displayName];
            if ([name isEqualToString:@"127.0.0.1"]) {
                expectedRecord = record;
                break;
            }
        }
        EXPECT_TRUE(expectedRecord);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [websiteDataStore removeDataOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] forDataRecords:@[expectedRecord.get()] completionHandler:^(void) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        bool foundLocalHostRecord = false;
        for (WKWebsiteDataRecord *record in dataRecords) {
            auto name = [record displayName];
            if ([name isEqualToString:@"127.0.0.1"]) {
                foundLocalHostRecord = true;
                break;
            }
        }
        EXPECT_FALSE(foundLocalHostRecord);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, SettingNonPersistentDataStorePathsThrowsException)
{
    auto configuration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);

    auto shouldThrowExceptionWhenUsed = [](Function<void(void)>&& modifier) {
        @try {
            modifier();
            EXPECT_TRUE(false);
        } @catch (NSException *exception) {
            EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        }
    };

    NSURL *fileURL = [NSURL fileURLWithPath:@"/tmp"];

    shouldThrowExceptionWhenUsed([&] {
        [configuration _setWebStorageDirectory:fileURL];
    });
    shouldThrowExceptionWhenUsed([&] {
        [configuration _setIndexedDBDatabaseDirectory:fileURL];
    });
    shouldThrowExceptionWhenUsed([&] {
        [configuration _setWebSQLDatabaseDirectory:fileURL];
    });
    shouldThrowExceptionWhenUsed([&] {
        [configuration _setCookieStorageFile:fileURL];
    });
    shouldThrowExceptionWhenUsed([&] {
        [configuration _setResourceLoadStatisticsDirectory:fileURL];
    });
    shouldThrowExceptionWhenUsed([&] {
        [configuration _setCacheStorageDirectory:fileURL];
    });
    shouldThrowExceptionWhenUsed([&] {
        [configuration _setServiceWorkerRegistrationDirectory:fileURL];
    });

    // These properties shouldn't throw exceptions when set on a non-persistent data store.
    [configuration setDeviceManagementRestrictionsEnabled:YES];
    [configuration setHTTPProxy:[NSURL URLWithString:@"http://www.apple.com/"]];
    [configuration setHTTPSProxy:[NSURL URLWithString:@"https://www.apple.com/"]];
    [configuration setSourceApplicationBundleIdentifier:@"com.apple.Safari"];
    [configuration setSourceApplicationSecondaryIdentifier:@"com.apple.Safari"];
}

TEST(WKWebsiteDataStore, FetchPersistentWebStorage)
{
    auto dataTypes = [NSSet setWithObjects:WKWebsiteDataTypeLocalStorage, WKWebsiteDataTypeSessionStorage, nil];
    auto localStorageType = [NSSet setWithObjects:WKWebsiteDataTypeLocalStorage, nil];

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^{
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    @autoreleasepool {
        auto webView = adoptNS([[WKWebView alloc] init]);
        auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView loadHTMLString:@"<script>sessionStorage.setItem('session', 'storage'); localStorage.setItem('local', 'storage');</script>" baseURL:[NSURL URLWithString:@"http://localhost"]];
        [navigationDelegate waitForDidFinishNavigation];
    }

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:dataTypes completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
        EXPECT_EQ([records count], 1u);
        EXPECT_TRUE([[[records firstObject] dataTypes] isEqualToSet:localStorageType]);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(WKWebsiteDataStore, FetchNonPersistentWebStorage)
{
    auto nonPersistentDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:nonPersistentDataStore];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadHTMLString:@"<script>sessionStorage.setItem('session', 'storage');localStorage.setItem('local', 'storage');</script>" baseURL:[NSURL URLWithString:@"http://localhost"]];
    [navigationDelegate waitForDidFinishNavigation];

    readyToContinue = false;
    [webView evaluateJavaScript:@"window.sessionStorage.getItem('session')" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE([@"storage" isEqualToString:result]);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    [webView evaluateJavaScript:@"window.localStorage.getItem('local')" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE([@"storage" isEqualToString:result]);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    [nonPersistentDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeSessionStorage] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ((int)dataRecords.count, 1);
        EXPECT_TRUE([[[dataRecords objectAtIndex:0] displayName] isEqualToString:@"localhost"]);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    readyToContinue = false;
    [nonPersistentDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeLocalStorage] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        EXPECT_EQ((int)dataRecords.count, 1);
        EXPECT_TRUE([[[dataRecords objectAtIndex:0] displayName] isEqualToString:@"localhost"]);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(WKWebsiteDataStore, SessionSetCount)
{
    auto countSessionSets = [] {
        __block bool done = false;
        __block size_t result = 0;
        [[WKWebsiteDataStore defaultDataStore] _countNonDefaultSessionSets:^(size_t count) {
            result = count;
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        return result;
    };
    @autoreleasepool {
        auto webView0 = adoptNS([WKWebView new]);
        EXPECT_EQ(countSessionSets(), 0u);
        auto configuration = adoptNS([WKWebViewConfiguration new]);
        EXPECT_NULL(configuration.get()._attributedBundleIdentifier);
        configuration.get()._attributedBundleIdentifier = @"test.bundle.id.1";
        auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
        [webView1 loadHTMLString:@"hi" baseURL:nil];
        EXPECT_EQ(countSessionSets(), 1u);
        auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
        [webView2 loadHTMLString:@"hi" baseURL:nil];
        EXPECT_EQ(countSessionSets(), 1u);
        configuration.get()._attributedBundleIdentifier = @"test.bundle.id.2";
        auto webView3 = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
        [webView3 loadHTMLString:@"hi" baseURL:nil];
        EXPECT_EQ(countSessionSets(), 2u);
    }
    while (countSessionSets()) { }
}

TEST(WKWebsiteDataStore, ReferenceCycle)
{
    WeakObjCPtr<WKWebsiteDataStore> dataStore;
    WeakObjCPtr<WKHTTPCookieStore> cookieStore;
    @autoreleasepool {
        dataStore = [WKWebsiteDataStore nonPersistentDataStore];
        cookieStore = [dataStore httpCookieStore];
    }
    while (dataStore.get() || cookieStore.get())
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(WKWebsiteDataStore, ClearCustomDataStoreNoWebViews)
{
    HTTPServer server([connectionCount = 0] (Connection connection) mutable {
        ++connectionCount;
        connection.receiveHTTPRequest([connection, connectionCount] (Vector<char>&& request) {
            switch (connectionCount) {
            case 1:
                connection.send(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 5\r\n"
                    "Set-Cookie: a=b\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "Hello"_s);
                break;
            case 2:
                EXPECT_FALSE(strnstr(request.data(), "Cookie: a=b\r\n", request.size()));
                connection.send(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 5\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "Hello"_s);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        });
    });


    NSURL *fileURL = [NSURL fileURLWithPath:@"/tmp/testcookiefile.cookie"];
    auto configuration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    [configuration _setCookieStorageFile:fileURL];

    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:configuration.get()]);
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:dataStore.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get() addToWindow:YES]);

    auto *url = [NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/index.html", server.port()]];

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
    [webView _close];
    webView = nil;

    // Now that the WebView is closed, remove all website data.
    // Then recreate a WebView with the same configuration to confirm the website data was removed.
    static bool done;
    [dataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^{
        done = true;
    }];
    Util::run(&done);
    done = false;

    webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get() addToWindow:YES]);
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:url]];
}

TEST(WKWebsiteDataStore, DoNotCreateDefaultDataStore)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration.get() copy];
    EXPECT_FALSE([WKWebsiteDataStore _defaultDataStoreExists]);
}

TEST(WKWebsiteDataStore, DefaultHSTSStorageDirectory)
{
    auto configuration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    EXPECT_NOT_NULL(configuration.get().hstsStorageDirectory);
}

static RetainPtr<WKWebsiteDataStore> createWebsiteDataStoreAndPrepare(NSUUID *uuid, NSString *pushPartition)
{
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:uuid]);
    websiteDataStoreConfiguration.get().webPushPartitionString = pushPartition;
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    EXPECT_TRUE([websiteDataStoreConfiguration.get().identifier isEqual:uuid]);
    EXPECT_TRUE([websiteDataStore.get()._identifier isEqual:uuid]);
    EXPECT_TRUE([websiteDataStoreConfiguration.get().webPushPartitionString isEqual:pushPartition]);
    EXPECT_TRUE([websiteDataStore.get()._webPushPartition isEqual:pushPartition]);

    pid_t webprocessIdentifier;
    @autoreleasepool {
        auto handler = adoptNS([[TestMessageHandler alloc] init]);
        [handler addMessage:@"continue" withHandler:^{
            receivedScriptMessage = true;
        }];
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
        [configuration setWebsiteDataStore:websiteDataStore.get()];
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        NSString *htmlString = @"<script> \
            indexedDB.open('testDB').onsuccess = function(event) { \
                window.webkit.messageHandlers.testHandler.postMessage('continue'); \
            } \
        </script>";
        receivedScriptMessage = false;
        [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
        TestWebKitAPI::Util::run(&receivedScriptMessage);
        webprocessIdentifier = [webView _webProcessIdentifier];
        EXPECT_NE(webprocessIdentifier, 0);
    }

    // Running web process may hold WebsiteDataStore alive, so make ensure it exits before return.
    while (!kill(webprocessIdentifier, 0))
        TestWebKitAPI::Util::spinRunLoop();

    return websiteDataStore;
}

TEST(WKWebsiteDataStore, DataStoreWithIdentifierAndPushPartition)
{
    __block auto uuid = [NSUUID UUID];
    @autoreleasepool {
        // Make sure WKWebsiteDataStore with identifier does not exist so it can be deleted.
        createWebsiteDataStoreAndPrepare(uuid, @"partition");
    }
}

TEST(WKWebsiteDataStore, RemoveDataStoreWithIdentifier)
{
    NSString *uuidString = @"68753a44-4d6f-1226-9c60-0050e4c00067";
    auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:uuidString]);
    RetainPtr<NSURL> generalStorageDirectory;
    @autoreleasepool {
        // Make sure WKWebsiteDataStore with identifier does not exist.
        auto websiteDataStore = createWebsiteDataStoreAndPrepare(uuid.get(), @"");
        generalStorageDirectory = websiteDataStore.get()._configuration.generalStorageDirectory;
    }

    EXPECT_NOT_NULL(generalStorageDirectory.get());
    NSFileManager *fileManager = [NSFileManager defaultManager];
    EXPECT_TRUE([fileManager fileExistsAtPath:generalStorageDirectory.get().path]);

    __block bool done = false;
    [WKWebsiteDataStore _removeDataStoreWithIdentifier:uuid.get() completionHandler:^(NSError *error) {
        done = true;
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE([fileManager fileExistsAtPath:generalStorageDirectory.get().path]);
}

TEST(WKWebsiteDataStore, RemoveDataStoreWithIdentifierRemoveCredentials)
{
    // FIXME: we should use persistent credential for test after rdar://100722784 is in build.
    usePersistentCredentialStorage = false;
    done = false;
    auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:@"68753a44-4d6f-1226-9c60-0050e4c00067"]);
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:uuid.get()]);
    pid_t networkProcessIdentifier;
    @autoreleasepool {
        auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
        [websiteDataStore removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] modifiedSince:[NSDate distantPast] completionHandler:^{
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        done = false;

        HTTPServer server(HTTPServer::respondWithChallengeThenOK);
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration setWebsiteDataStore:websiteDataStore.get()];
        auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
        [navigationDelegate waitForDidFinishNavigation];

        [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
            EXPECT_EQ((int)dataRecords.count, 1);
            for (WKWebsiteDataRecord *record in dataRecords)
                EXPECT_WK_STREQ([record displayName], @"127.0.0.1");
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        done = false;
        networkProcessIdentifier = [websiteDataStore.get() _networkProcessIdentifier];
        EXPECT_NE(networkProcessIdentifier, 0);
    }

    // Wait until network process exits so we are sure website data files are not in use during removal.
    while (!kill(networkProcessIdentifier, 0))
        TestWebKitAPI::Util::spinRunLoop();

    [WKWebsiteDataStore _removeDataStoreWithIdentifier:uuid.get() completionHandler:^(NSError *error) {
        done = true;
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        int credentialCount = dataRecords.count;
        EXPECT_EQ(credentialCount, 0);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKWebsiteDataStore, RemoveDataStoreWithIdentifierErrorWhenInUse)
{
    auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:@"68753a44-4d6f-1226-9c60-0050e4c00067"]);
    auto websiteDataStore = createWebsiteDataStoreAndPrepare(uuid.get(), @"");
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:websiteDataStore.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:@"" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];

    __block bool done = false;
    [WKWebsiteDataStore _removeDataStoreWithIdentifier:uuid.get() completionHandler:^(NSError *error) {
        done = true;
        EXPECT_TRUE(!!error);
        EXPECT_TRUE([[error description] containsString:@"in use"]);
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKWebsiteDataStore, ListIdentifiers)
{
    __block auto uuid = [NSUUID UUID];
    @autoreleasepool {
        // Make sure WKWebsiteDataStore with identifier does not exist so it can be deleted.
        createWebsiteDataStoreAndPrepare(uuid, @"");
    }

    __block bool done = false;
    [WKWebsiteDataStore _fetchAllIdentifiers:^(NSArray<NSUUID *> * identifiers) {
        done = true;
        EXPECT_TRUE([identifiers containsObject:uuid]);
    }];
    TestWebKitAPI::Util::run(&done);

    // Clean up to not leave data on disk.
    done = false;
    [WKWebsiteDataStore _removeDataStoreWithIdentifier:uuid completionHandler:^(NSError *error) {
        done = true;
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKWebsiteDataStorePrivate, FetchWithSize)
{
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^{
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    auto handler = adoptNS([[TestMessageHandler alloc] init]);
    [handler addMessage:@"continue" withHandler:^{
        receivedScriptMessage = true;
    }];
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSString *htmlString = @"<script> \
        localStorage.setItem('key', 'value'); \
        indexedDB.open('testDB').onsuccess = function(event) { \
            window.webkit.messageHandlers.testHandler.postMessage('continue'); \
        } \
    </script>";
    receivedScriptMessage = false;
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);

    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] _fetchDataRecordsOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] withOptions:_WKWebsiteDataStoreFetchOptionComputeSizes completionHandler:^(NSArray<WKWebsiteDataRecord *> * records) {
        EXPECT_EQ([records count], 1u);
        WKWebsiteDataRecord *record = [records firstObject];
        EXPECT_TRUE([[record displayName] isEqualToString:@"webkit.org"]);
        _WKWebsiteDataSize *dataSize = [record _dataSize];
        EXPECT_GT([dataSize totalSize], 0u);
        NSSet *localStorageType = [NSSet setWithObjects:WKWebsiteDataTypeLocalStorage, nil];
        EXPECT_GT([dataSize sizeOfDataTypes:localStorageType], 0u);
        NSSet *indexedDBType = [NSSet setWithObjects:WKWebsiteDataTypeIndexedDBDatabases, nil];
        EXPECT_GT([dataSize sizeOfDataTypes:indexedDBType], 0u);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(WKWebsiteDataStore, DataStoreForNilIdentifier)
{
    bool hasException = false;
    @try {
        auto websiteDataStore = [WKWebsiteDataStore dataStoreForIdentifier:[[NSUUID alloc] initWithUUIDString:@"1234"]];
        EXPECT_NOT_NULL(websiteDataStore);
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        EXPECT_WK_STREQ(@"Identifier is nil", exception.reason);
        hasException = true;
    }
    EXPECT_TRUE(hasException);
}

TEST(WKWebsiteDataStore, DataStoreForEmptyIdentifier)
{
    bool hasException = false;
    @try {
        auto data = [NSMutableData dataWithLength:16];
        unsigned char* dataBytes = (unsigned char*) [data mutableBytes];
        auto emptyUUID = [[NSUUID alloc] initWithUUIDBytes:dataBytes];
        auto websiteDataStore = [WKWebsiteDataStore dataStoreForIdentifier:emptyUUID];
        EXPECT_NOT_NULL(websiteDataStore);
    } @catch (NSException *exception) {
        EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        EXPECT_WK_STREQ(@"Identifier (00000000-0000-0000-0000-000000000000) is invalid for data store", exception.reason);
        hasException = true;
    }
    EXPECT_TRUE(hasException);
}

TEST(WKWebsiteDataStoreConfiguration, OriginQuotaRatio)
{
    auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:@"68753a44-4d6f-1226-9c60-0050e4c00067"]);
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:uuid.get()]);
    EXPECT_NULL([websiteDataStoreConfiguration.get() originQuotaRatio]);
    // 1 GB disk has 1 byte quota.
    auto ratioNumber = [NSNumber numberWithFloat:0.000000001];
    [websiteDataStoreConfiguration.get() setOriginQuotaRatio:ratioNumber];
    EXPECT_TRUE([[websiteDataStoreConfiguration.get() originQuotaRatio] isEqualToNumber:ratioNumber]);
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    auto handler = adoptNS([[WKWebsiteDataStoreMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    [configuration setWebsiteDataStore:websiteDataStore.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSString *htmlString = @"<script> \
        var messageSent = false; \
        function sendMessage(message) { \
            if (messageSent) return; \
            messageSent = true; \
            window.webkit.messageHandlers.testHandler.postMessage(message); \
        }; \
        indexedDB.deleteDatabase('testRatio'); \
        var request = indexedDB.open('testRatio'); \
        request.onupgradeneeded = function(event) { \
            db = event.target.result; \
            os = db.createObjectStore('os'); \
            const item = new Array(100000).join('x');\
            os.put(item, 'key').onerror = function(event) { sendMessage(event.target.error.name); }; \
        }; \
        request.onsuccess = function() { sendMessage('Unexpected success'); }; \
        request.onerror = function(event) { sendMessage(event.target.error.name); }; \
    </script>";
    receivedScriptMessage = false;
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"http://webkit.org/"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"QuotaExceededError", [lastScriptMessage body]);
}

TEST(WKWebsiteDataStoreConfiguration, TotalQuotaRatio)
{
    done = false;
    receivedScriptMessage = false;
    auto uuid = adoptNS([[NSUUID alloc] initWithUUIDString:@"68753a44-4d6f-1226-9c60-0050e4c00067"]);
    // Clear existing data.
    [WKWebsiteDataStore _removeDataStoreWithIdentifier:uuid.get() completionHandler:^(NSError *error) {
        done = true;
        EXPECT_NULL(error);
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    NSString *htmlString = @"<script> \
        window.caches.open('test').then((cache) => { \
            return cache.put('https://webkit.org/test', new Response(new ArrayBuffer(20000))); \
        }).then(() => { \
            window.webkit.messageHandlers.testHandler.postMessage('success'); \
        }).catch(() => { \
            window.webkit.messageHandlers.testHandler.postMessage('error'); \
        }); \
    </script>";
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithIdentifier:uuid.get()]);
    EXPECT_NULL([websiteDataStoreConfiguration.get() totalQuotaRatio]);
    [websiteDataStoreConfiguration.get() setTotalQuotaRatio:[NSNumber numberWithFloat:0.5]];
    [websiteDataStoreConfiguration.get() setVolumeCapacityOverride:[NSNumber numberWithFloat:100000]];
    auto handler = adoptNS([[WKWebsiteDataStoreMessageHandler alloc] init]);
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    [configuration setWebsiteDataStore:websiteDataStore.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://first.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success", [lastScriptMessage body]);

    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://second.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success", [lastScriptMessage body]);

    auto sortFunction = ^(WKWebsiteDataRecord *record1, WKWebsiteDataRecord *record2) {
        return [record1.displayName compare:record2.displayName];
    };
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeFetchCache] completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 2u);
        auto sortedRecords = [records sortedArrayUsingComparator:sortFunction];
        EXPECT_WK_STREQ(@"first.com", [[sortedRecords objectAtIndex:0] displayName]);
        EXPECT_WK_STREQ(@"second.com", [[sortedRecords objectAtIndex:1] displayName]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Update recently used origin list.
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://first.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success", [lastScriptMessage body]);

    // Trigger eviction on example.com.
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://third.com"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"success", [lastScriptMessage body]);

    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeFetchCache] completionHandler:^(NSArray<WKWebsiteDataRecord *> *records) {
        EXPECT_EQ(records.count, 2u);
        auto sortedRecords = [records sortedArrayUsingComparator:sortFunction];
        EXPECT_WK_STREQ(@"first.com", [[sortedRecords objectAtIndex:0] displayName]);
        EXPECT_WK_STREQ(@"third.com", [[sortedRecords objectAtIndex:1] displayName]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

} // namespace TestWebKitAPI

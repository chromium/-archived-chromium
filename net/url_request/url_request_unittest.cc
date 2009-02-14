// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_unittest.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shlobj.h>
#elif defined(OS_LINUX)
#include "base/nss_init.h"
#endif

#include <algorithm>
#include <string>

#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_module.h"
#include "net/base/net_util.h"
#include "net/base/ssl_test_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Time;

namespace {

class URLRequestHttpCacheContext : public URLRequestContext {
 public:
  URLRequestHttpCacheContext() {
    proxy_service_ = net::ProxyService::CreateNull();
    http_transaction_factory_ =
        new net::HttpCache(net::HttpNetworkLayer::CreateFactory(proxy_service_),
                           disk_cache::CreateInMemoryCacheBackend(0));
  }

  virtual ~URLRequestHttpCacheContext() {
    delete http_transaction_factory_;
    delete proxy_service_;
  }
};

class TestURLRequest : public URLRequest {
 public:
  TestURLRequest(const GURL& url, Delegate* delegate)
      : URLRequest(url, delegate) {
    set_context(new URLRequestHttpCacheContext());
  }
};

StringPiece TestNetResourceProvider(int key) {
  return "header";
}

// Do a case-insensitive search through |haystack| for |needle|.
bool ContainsString(const std::string& haystack, const char* needle) {
  std::string::const_iterator it =
      std::search(haystack.begin(),
                  haystack.end(),
                  needle,
                  needle + strlen(needle),
                  CaseInsensitiveCompare<char>());
  return it != haystack.end();
}

}  // namespace

// Inherit PlatformTest since we require the autorelease pool on Mac OS X.f
class URLRequestTest : public PlatformTest {
};

TEST_F(URLRequestTest, ProxyTunnelRedirectTest) {
  // In this unit test, we're using the HTTPTestServer as a proxy server and
  // issuing a CONNECT request with the magic host name "www.redirect.com".
  // The HTTPTestServer will return a 302 response, which we should not
  // follow.
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"", NULL);
  ASSERT_TRUE(NULL != server.get());
  TestDelegate d;
  {
    URLRequest r(GURL("https://www.redirect.com/"), &d);
    std::string proxy("localhost:");
    proxy.append(IntToString(kHTTPDefaultPort));
    r.set_context(new TestURLRequestContext(proxy));

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
    // We should have rewritten the 302 response code as 500.
    EXPECT_EQ(500, r.GetResponseCode());
    EXPECT_EQ(1, d.response_started_count());
    // We should not have followed the redirect.
    EXPECT_EQ(0, d.received_redirect_count());
  }
}

TEST_F(URLRequestTest, UnexpectedServerAuthTest) {
  // In this unit test, we're using the HTTPTestServer as a proxy server and
  // issuing a CONNECT request with the magic host name "www.server-auth.com".
  // The HTTPTestServer will return a 401 response, which we should balk at.
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"", NULL);
  ASSERT_TRUE(NULL != server.get());
  TestDelegate d;
  {
    URLRequest r(GURL("https://www.server-auth.com/"), &d);
    std::string proxy("localhost:");
    proxy.append(IntToString(kHTTPDefaultPort));
    r.set_context(new TestURLRequestContext(proxy));

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::FAILED, r.status().status());
    EXPECT_EQ(net::ERR_UNEXPECTED_SERVER_AUTH, r.status().os_error());
  }
}

TEST_F(URLRequestTest, GetTest_NoCache) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"", NULL);
  ASSERT_TRUE(NULL != server.get());
  TestDelegate d;
  {
    TestURLRequest r(server->TestServerPage(""), &d);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
  }
#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}

TEST_F(URLRequestTest, GetTest) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"", NULL);
  ASSERT_TRUE(NULL != server.get());
  TestDelegate d;
  {
    TestURLRequest r(server->TestServerPage(""), &d);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
  }
#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}

class HTTPSRequestTest : public testing::Test {
 protected:
  HTTPSRequestTest() : util_() {}

  SSLTestUtil util_;
};

#if defined(OS_MACOSX)
// TODO(port): support temporary root cert on mac
#define MAYBE_HTTPSGetTest DISABLED_HTTPSGetTest
#else
#define MAYBE_HTTPSGetTest HTTPSGetTest
#endif

TEST_F(HTTPSRequestTest, MAYBE_HTTPSGetTest) {
  // Note: tools/testserver/testserver.py does not need
  // a working document root to server the pages / and /hello.html,
  // so this test doesn't really need to specify a document root.
  // But if it did, a good one would be net/data/ssl.
  scoped_refptr<HTTPSTestServer> server =
      HTTPSTestServer::CreateServer(util_.kHostName, util_.kOKHTTPSPort,
      L"net/data/ssl", util_.GetOKCertPath().ToWStringHack());
  ASSERT_TRUE(NULL != server.get());

  EXPECT_TRUE(util_.CheckCATrusted());
  TestDelegate d;
  {
    TestURLRequest r(server->TestServerPage(""), &d);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_NE(0, d.bytes_received());
  }
#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}

TEST_F(URLRequestTest, CancelTest) {
  TestDelegate d;
  {
    TestURLRequest r(GURL("http://www.google.com/"), &d);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    r.Cancel();

    MessageLoop::current()->Run();

    // We expect to receive OnResponseStarted even though the request has been
    // cancelled.
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
  }
#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}

TEST_F(URLRequestTest, CancelTest2) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"", NULL);
  ASSERT_TRUE(NULL != server.get());

  // error C2446: '!=' : no conversion from 'HTTPTestServer *const '
  // to 'const int'

  TestDelegate d;
  {
    TestURLRequest r(server->TestServerPage(""), &d);

    d.set_cancel_in_response_started(true);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(URLRequestStatus::CANCELED, r.status().status());
  }
#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}

TEST_F(URLRequestTest, CancelTest3) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"", NULL);
  ASSERT_TRUE(NULL != server.get());
  TestDelegate d;
  {
    TestURLRequest r(server->TestServerPage(""), &d);

    d.set_cancel_in_received_data(true);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    // There is no guarantee about how much data was received
    // before the cancel was issued.  It could have been 0 bytes,
    // or it could have been all the bytes.
    // EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(URLRequestStatus::CANCELED, r.status().status());
  }
#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}

TEST_F(URLRequestTest, CancelTest4) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"", NULL);
  ASSERT_TRUE(NULL != server.get());
  TestDelegate d;
  {
    TestURLRequest r(server->TestServerPage(""), &d);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    // The request will be implicitly canceled when it is destroyed. The
    // test delegate must not post a quit message when this happens because
    // this test doesn't actually have a message loop. The quit message would
    // get put on this thread's message queue and the next test would exit
    // early, causing problems.
    d.set_quit_on_complete(false);
  }
  // expect things to just cleanup properly.

  // we won't actually get a received reponse here because we've never run the
  // message loop
  EXPECT_FALSE(d.received_data_before_response());
  EXPECT_EQ(0, d.bytes_received());
}

TEST_F(URLRequestTest, CancelTest5) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"", NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<URLRequestContext> context = new URLRequestHttpCacheContext();

  // populate cache
  {
    TestDelegate d;
    URLRequest r(server->TestServerPage("cachetime"), &d);
    r.set_context(context);
    r.Start();
    MessageLoop::current()->Run();
    EXPECT_EQ(URLRequestStatus::SUCCESS, r.status().status());
  }

  // cancel read from cache (see bug 990242)
  {
    TestDelegate d;
    URLRequest r(server->TestServerPage("cachetime"), &d);
    r.set_context(context);
    r.Start();
    r.Cancel();
    MessageLoop::current()->Run();

    EXPECT_EQ(URLRequestStatus::CANCELED, r.status().status());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
  }

#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}

TEST_F(URLRequestTest, PostTest) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"net/data", NULL);
  ASSERT_TRUE(NULL != server.get());
  const int kMsgSize = 20000;  // multiple of 10
  const int kIterations = 50;
  char *uploadBytes = new char[kMsgSize+1];
  char *ptr = uploadBytes;
  char marker = 'a';
  for (int idx = 0; idx < kMsgSize/10; idx++) {
    memcpy(ptr, "----------", 10);
    ptr += 10;
    if (idx % 100 == 0) {
      ptr--;
      *ptr++ = marker;
      if (++marker > 'z')
        marker = 'a';
    }
  }
  uploadBytes[kMsgSize] = '\0';

  scoped_refptr<URLRequestContext> context =
      new URLRequestHttpCacheContext();

  for (int i = 0; i < kIterations; ++i) {
    TestDelegate d;
    URLRequest r(server->TestServerPage("echo"), &d);
    r.set_context(context);
    r.set_method("POST");

    r.AppendBytesToUpload(uploadBytes, kMsgSize);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    ASSERT_EQ(1, d.response_started_count()) << "request failed: " <<
        (int) r.status().status() << ", os error: " << r.status().os_error();

    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(uploadBytes, d.data_received());
    EXPECT_EQ(memcmp(uploadBytes, d.data_received().c_str(), kMsgSize), 0);
    EXPECT_EQ(d.data_received().compare(uploadBytes), 0);
  }
  delete[] uploadBytes;
#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}

TEST_F(URLRequestTest, PostEmptyTest) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"net/data", NULL);
  ASSERT_TRUE(NULL != server.get());
  TestDelegate d;
  {
    TestURLRequest r(server->TestServerPage("echo"), &d);
    r.set_method("POST");

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    ASSERT_EQ(1, d.response_started_count()) << "request failed: " <<
        (int) r.status().status() << ", os error: " << r.status().os_error();

    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_TRUE(d.data_received().empty());
  }
#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}

TEST_F(URLRequestTest, PostFileTest) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"net/data", NULL);
  ASSERT_TRUE(NULL != server.get());
  TestDelegate d;
  {
    TestURLRequest r(server->TestServerPage("echo"), &d);
    r.set_method("POST");

    std::wstring dir;
    PathService::Get(base::DIR_EXE, &dir);
    file_util::SetCurrentDirectory(dir);

    std::wstring path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    file_util::AppendToPath(&path, L"net");
    file_util::AppendToPath(&path, L"data");
    file_util::AppendToPath(&path, L"url_request_unittest");
    file_util::AppendToPath(&path, L"with-headers.html");
    r.AppendFileToUpload(path);

    // This file should just be ignored in the upload stream.
    r.AppendFileToUpload(L"c:\\path\\to\\non\\existant\\file.randomness.12345");

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 longsize;
    ASSERT_EQ(true, file_util::GetFileSize(path, &longsize));
    int size = static_cast<int>(longsize);
    scoped_array<char> buf(new char[size]);

    int size_read = static_cast<int>(file_util::ReadFile(path,
        buf.get(), size));
    ASSERT_EQ(size, size_read);

    ASSERT_EQ(1, d.response_started_count()) << "request failed: " <<
        (int) r.status().status() << ", os error: " << r.status().os_error();

    EXPECT_FALSE(d.received_data_before_response());

    ASSERT_EQ(size, d.bytes_received());
    EXPECT_EQ(0, memcmp(d.data_received().c_str(), buf.get(), size));
  }
#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}

TEST_F(URLRequestTest, AboutBlankTest) {
  TestDelegate d;
  {
    TestURLRequest r(GURL("about:blank"), &d);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_TRUE(!r.is_pending());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), 0);
  }
#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}

TEST_F(URLRequestTest, FileTest) {
  FilePath app_path;
  PathService::Get(base::FILE_EXE, &app_path);
  GURL app_url = net::FilePathToFileURL(app_path.ToWStringHack());

  TestDelegate d;
  {
    TestURLRequest r(app_url, &d);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_TRUE(!r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), static_cast<int>(file_size));
  }
#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}

TEST_F(URLRequestTest, InvalidUrlTest) {
  TestDelegate d;
  {
    TestURLRequest r(GURL("invalid url"), &d);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();
    EXPECT_TRUE(d.request_failed());
  }
#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}

// This test is disabled because it fails on some computers due to proxies
// returning a page in response to this request rather than reporting failure.
TEST_F(URLRequestTest, DISABLED_DnsFailureTest) {
  TestDelegate d;
  {
    URLRequest r(GURL("http://thisisnotavalidurl0123456789foo.com/"), &d);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();
    EXPECT_TRUE(d.request_failed());
  }
#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}

TEST_F(URLRequestTest, ResponseHeadersTest) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"net/data/url_request_unittest", NULL);
  ASSERT_TRUE(NULL != server.get());
  TestDelegate d;
  TestURLRequest req(server->TestServerPage("files/with-headers.html"), &d);
  req.Start();
  MessageLoop::current()->Run();

  const net::HttpResponseHeaders* headers = req.response_headers();
  std::string header;
  EXPECT_TRUE(headers->GetNormalizedHeader("cache-control", &header));
  EXPECT_EQ("private", header);

  header.clear();
  EXPECT_TRUE(headers->GetNormalizedHeader("content-type", &header));
  EXPECT_EQ("text/html; charset=ISO-8859-1", header);

  // The response has two "X-Multiple-Entries" headers.
  // This verfies our output has them concatenated together.
  header.clear();
  EXPECT_TRUE(headers->GetNormalizedHeader("x-multiple-entries", &header));
  EXPECT_EQ("a, b", header);
}

TEST_F(URLRequestTest, BZip2ContentTest) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"net/data/filter_unittests", NULL);
  ASSERT_TRUE(NULL != server.get());

  // for localhost domain, we also should support bzip2 encoding
  // first, get the original file
  TestDelegate d1;
  TestURLRequest req1(server->TestServerPage("realfiles/google.txt"), &d1);
  req1.Start();
  MessageLoop::current()->Run();

  const std::string& got_content = d1.data_received();

  // second, get bzip2 content
  TestDelegate d2;
  TestURLRequest req2(server->TestServerPage("realbz2files/google.txt"), &d2);
  req2.Start();
  MessageLoop::current()->Run();

  const std::string& got_bz2_content = d2.data_received();

  // compare those two results
  EXPECT_EQ(got_content, got_bz2_content);
}

TEST_F(URLRequestTest, BZip2ContentTest_IncrementalHeader) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"net/data/filter_unittests", NULL);
  ASSERT_TRUE(NULL != server.get());

  // for localhost domain, we also should support bzip2 encoding
  // first, get the original file
  TestDelegate d1;
  TestURLRequest req1(server->TestServerPage("realfiles/google.txt"), &d1);
  req1.Start();
  MessageLoop::current()->Run();

  const std::string& got_content = d1.data_received();

  // second, get bzip2 content.  ask the testserver to send the BZ2 header in
  // two chunks with a delay between them.  this tests our fix for bug 867161.
  TestDelegate d2;
  TestURLRequest req2(server->TestServerPage(
      "realbz2files/google.txt?incremental-header"), &d2);
  req2.Start();
  MessageLoop::current()->Run();

  const std::string& got_bz2_content = d2.data_received();

  // compare those two results
  EXPECT_EQ(got_content, got_bz2_content);
}

#if defined(OS_WIN)
TEST_F(URLRequestTest, ResolveShortcutTest) {
  std::wstring app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  file_util::AppendToPath(&app_path, L"net");
  file_util::AppendToPath(&app_path, L"data");
  file_util::AppendToPath(&app_path, L"url_request_unittest");
  file_util::AppendToPath(&app_path, L"with-headers.html");

  std::wstring lnk_path = app_path + L".lnk";

  HRESULT result;
  IShellLink *shell = NULL;
  IPersistFile *persist = NULL;

  CoInitialize(NULL);
  // Temporarily create a shortcut for test
  result = CoCreateInstance(CLSID_ShellLink, NULL,
                          CLSCTX_INPROC_SERVER, IID_IShellLink,
                          reinterpret_cast<LPVOID*>(&shell));
  EXPECT_TRUE(SUCCEEDED(result));
  result = shell->QueryInterface(IID_IPersistFile,
                             reinterpret_cast<LPVOID*>(&persist));
  EXPECT_TRUE(SUCCEEDED(result));
  result = shell->SetPath(app_path.c_str());
  EXPECT_TRUE(SUCCEEDED(result));
  result = shell->SetDescription(L"ResolveShortcutTest");
  EXPECT_TRUE(SUCCEEDED(result));
  result = persist->Save(lnk_path.c_str(), TRUE);
  EXPECT_TRUE(SUCCEEDED(result));
  if (persist)
    persist->Release();
  if (shell)
    shell->Release();

  TestDelegate d;
  {
    TestURLRequest r(net::FilePathToFileURL(lnk_path), &d);

    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    WIN32_FILE_ATTRIBUTE_DATA data;
    GetFileAttributesEx(app_path.c_str(), GetFileExInfoStandard, &data);
    HANDLE file = CreateFile(app_path.c_str(), GENERIC_READ,
                             FILE_SHARE_READ, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT_NE(INVALID_HANDLE_VALUE, file);
    scoped_array<char> buffer(new char[data.nFileSizeLow]);
    DWORD read_size;
    BOOL result;
    result = ReadFile(file, buffer.get(), data.nFileSizeLow,
                      &read_size, NULL);
    std::string content(buffer.get(), read_size);
    CloseHandle(file);

    EXPECT_TRUE(!r.is_pending());
    EXPECT_EQ(1, d.received_redirect_count());
    EXPECT_EQ(content, d.data_received());
  }

  // Clean the shortcut
  DeleteFile(lnk_path.c_str());
  CoUninitialize();

#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif
}
#endif  // defined(OS_WIN)

TEST_F(URLRequestTest, ContentTypeNormalizationTest) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"net/data/url_request_unittest", NULL);
  ASSERT_TRUE(NULL != server.get());

  TestDelegate d;
  TestURLRequest req(server->TestServerPage(
      "files/content-type-normalization.html"), &d);
  req.Start();
  MessageLoop::current()->Run();

  std::string mime_type;
  req.GetMimeType(&mime_type);
  EXPECT_EQ("text/html", mime_type);

  std::string charset;
  req.GetCharset(&charset);
  EXPECT_EQ("utf-8", charset);
  req.Cancel();
}

TEST_F(URLRequestTest, FileDirCancelTest) {
  // Put in mock resource provider.
  net::NetModule::SetResourceProvider(TestNetResourceProvider);

  TestDelegate d;
  {
    std::wstring file_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
    file_util::AppendToPath(&file_path, L"net");
    file_util::AppendToPath(&file_path, L"data");
    file_util::AppendToPath(&file_path, L"");

    TestURLRequest req(net::FilePathToFileURL(file_path), &d);
    req.Start();
    EXPECT_TRUE(req.is_pending());

    d.set_cancel_in_received_data_pending(true);

    MessageLoop::current()->Run();
  }
#ifndef NDEBUG
  DCHECK_EQ(url_request_metrics.object_count, 0);
#endif

  // Take out mock resource provider.
  net::NetModule::SetResourceProvider(NULL);
}

TEST_F(URLRequestTest, RestrictRedirects) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"net/data/url_request_unittest", NULL);
  ASSERT_TRUE(NULL != server.get());

  TestDelegate d;
  TestURLRequest req(server->TestServerPage(
      "files/redirect-to-file.html"), &d);
  req.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(URLRequestStatus::FAILED, req.status().status());
  EXPECT_EQ(net::ERR_UNSAFE_REDIRECT, req.status().os_error());
}

TEST_F(URLRequestTest, NoUserPassInReferrer) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"net/data/url_request_unittest", NULL);
  ASSERT_TRUE(NULL != server.get());
  TestDelegate d;
  TestURLRequest req(server->TestServerPage(
      "echoheader?Referer"), &d);
  req.set_referrer("http://user:pass@foo.com/");
  req.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(std::string("http://foo.com/"), d.data_received());
}

TEST_F(URLRequestTest, CancelRedirect) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"net/data/url_request_unittest", NULL);
  ASSERT_TRUE(NULL != server.get());
  TestDelegate d;
  {
    d.set_cancel_in_received_redirect(true);
    TestURLRequest req(server->TestServerPage(
        "files/redirect-test.html"), &d);
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_EQ(1, d.response_started_count());
    EXPECT_EQ(0, d.bytes_received());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(URLRequestStatus::CANCELED, req.status().status());
  }
}

TEST_F(URLRequestTest, VaryHeader) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"net/data/url_request_unittest", NULL);
  ASSERT_TRUE(NULL != server.get());

  scoped_refptr<URLRequestContext> context = new URLRequestHttpCacheContext();

  Time response_time;

  // populate the cache
  {
    TestDelegate d;
    URLRequest req(server->TestServerPage("echoheader?foo"), &d);
    req.set_context(context);
    req.SetExtraRequestHeaders("foo:1");
    req.Start();
    MessageLoop::current()->Run();

    response_time = req.response_time();
  }

  // Make sure that the response time of a future response will be in the
  // future!
  PlatformThread::Sleep(10);

  // expect a cache hit
  {
    TestDelegate d;
    URLRequest req(server->TestServerPage("echoheader?foo"), &d);
    req.set_context(context);
    req.SetExtraRequestHeaders("foo:1");
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_TRUE(req.response_time() == response_time);
  }

  // expect a cache miss
  {
    TestDelegate d;
    URLRequest req(server->TestServerPage("echoheader?foo"), &d);
    req.set_context(context);
    req.SetExtraRequestHeaders("foo:2");
    req.Start();
    MessageLoop::current()->Run();

    EXPECT_FALSE(req.response_time() == response_time);
  }
}

TEST_F(URLRequestTest, BasicAuth) {
  scoped_refptr<URLRequestContext> context = new URLRequestHttpCacheContext();
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"", NULL);
  ASSERT_TRUE(NULL != server.get());

  Time response_time;

  // populate the cache
  {
    TestDelegate d;
    d.set_username(L"user");
    d.set_password(L"secret");

    URLRequest r(server->TestServerPage("auth-basic"), &d);
    r.set_context(context);
    r.Start();

    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);

    response_time = r.response_time();
  }

  // Let some time pass so we can ensure that a future response will have a
  // response time value in the future.
  PlatformThread::Sleep(10 /* milliseconds */);

  // repeat request with end-to-end validation.  since auth-basic results in a
  // cachable page, we expect this test to result in a 304.  in which case, the
  // response should be fetched from the cache.
  {
    TestDelegate d;
    d.set_username(L"user");
    d.set_password(L"secret");

    URLRequest r(server->TestServerPage("auth-basic"), &d);
    r.set_context(context);
    r.set_load_flags(net::LOAD_VALIDATE_CACHE);
    r.Start();

    MessageLoop::current()->Run();

    EXPECT_TRUE(d.data_received().find("user/secret") != std::string::npos);

    // Should be the same cached document, which means that the response time
    // should not have changed.
    EXPECT_TRUE(response_time == r.response_time());
  }
}

// In this test, we do a POST which the server will 302 redirect.
// The subsequent transaction should use GET, and should not send the
// Content-Type header.
// http://code.google.com/p/chromium/issues/detail?id=843
TEST_F(URLRequestTest, Post302RedirectGet) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"net/data/url_request_unittest", NULL);
  ASSERT_TRUE(NULL != server.get());
  TestDelegate d;
  TestURLRequest req(server->TestServerPage("files/redirect-to-echoall"), &d);
  req.set_method("POST");

  // Set headers (some of which are specific to the POST).
  // ("Content-Length: 10" is just a junk value to make sure it gets stripped).
  req.SetExtraRequestHeaders(
    "Content-Type: multipart/form-data; "
    "boundary=----WebKitFormBoundaryAADeAA+NAAWMAAwZ\r\n"
    "Accept: text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,"
    "text/plain;q=0.8,image/png,*/*;q=0.5\r\n"
    "Accept-Language: en-US,en\r\n"
    "Accept-Charset: ISO-8859-1,*,utf-8\r\n"
    "Content-Length: 10\r\n"
    "Origin: http://localhost:1337/"
  );
  req.Start();
  MessageLoop::current()->Run();

  std::string mime_type;
  req.GetMimeType(&mime_type);
  EXPECT_EQ("text/html", mime_type);

  const std::string& data = d.data_received();

  // Check that the post-specific headers were stripped:
  EXPECT_FALSE(ContainsString(data, "Content-Length:"));
  EXPECT_FALSE(ContainsString(data, "Content-Type:"));
  EXPECT_FALSE(ContainsString(data, "Origin:"));

  // These extra request headers should not have been stripped.
  EXPECT_TRUE(ContainsString(data, "Accept:"));
  EXPECT_TRUE(ContainsString(data, "Accept-Language:"));
  EXPECT_TRUE(ContainsString(data, "Accept-Charset:"));
}

TEST_F(URLRequestTest, Post307RedirectPost) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(L"net/data/url_request_unittest", NULL);
  ASSERT_TRUE(NULL != server.get());
  TestDelegate d;
  TestURLRequest req(server->TestServerPage("files/redirect307-to-echoall"),
      &d);
  req.set_method("POST");
  req.Start();
  MessageLoop::current()->Run();
  EXPECT_EQ(req.method(), "POST");
}

// FTP tests appear to be hanging some of the time
#if 1  // !defined(OS_WIN)
  #define MAYBE_FTPGetTestAnonymous   DISABLED_FTPGetTestAnonymous
  #define MAYBE_FTPGetTest            DISABLED_FTPGetTest
  #define MAYBE_FTPCheckWrongUser     DISABLED_FTPCheckWrongUser
  #define MAYBE_FTPCheckWrongPassword DISABLED_FTPCheckWrongPassword
#else
  #define MAYBE_FTPGetTestAnonymous   FTPGetTestAnonymous
  #define MAYBE_FTPGetTest            FTPGetTest
  #define MAYBE_FTPCheckWrongUser     FTPCheckWrongUser
  #define MAYBE_FTPCheckWrongPassword FTPCheckWrongPassword
#endif

TEST_F(URLRequestTest, MAYBE_FTPGetTestAnonymous) {
  scoped_refptr<FTPTestServer> server = FTPTestServer::CreateServer(L"");
  ASSERT_TRUE(NULL != server.get());
  std::wstring app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path.append(L"\\LICENSE");
  TestDelegate d;
  {
    TestURLRequest r(server->TestServerPage("/LICENSE"), &d);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = 0;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_TRUE(!r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), static_cast<int>(file_size));
  }
}

TEST_F(URLRequestTest, MAYBE_FTPGetTest) {
  scoped_refptr<FTPTestServer> server =
      FTPTestServer::CreateServer(L"", "chrome", "chrome");
  ASSERT_TRUE(NULL != server.get());
  std::wstring app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path.append(L"\\LICENSE");
  TestDelegate d;
  {
    TestURLRequest r(server->TestServerPage("/LICENSE"), &d);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = 0;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_TRUE(!r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), static_cast<int>(file_size));
  }
}

TEST_F(URLRequestTest, MAYBE_FTPCheckWrongPassword) {
  scoped_refptr<FTPTestServer> server =
      FTPTestServer::CreateServer(L"", "chrome", "wrong_password");
  ASSERT_TRUE(NULL != server.get());
  std::wstring app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path.append(L"\\LICENSE");
  TestDelegate d;
  {
    TestURLRequest r(server->TestServerPage("/LICENSE"), &d);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = 0;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_TRUE(!r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), 0);
  }
}

TEST_F(URLRequestTest, MAYBE_FTPCheckWrongUser) {
  scoped_refptr<FTPTestServer> server =
      FTPTestServer::CreateServer(L"", "wrong_user", "chrome");
  ASSERT_TRUE(NULL != server.get());
  std::wstring app_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &app_path);
  app_path.append(L"\\LICENSE");
  TestDelegate d;
  {
    TestURLRequest r(server->TestServerPage("/LICENSE"), &d);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    int64 file_size = 0;
    file_util::GetFileSize(app_path, &file_size);

    EXPECT_TRUE(!r.is_pending());
    EXPECT_EQ(1, d.response_started_count());
    EXPECT_FALSE(d.received_data_before_response());
    EXPECT_EQ(d.bytes_received(), 0);
  }
}


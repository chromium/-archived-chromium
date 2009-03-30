// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_UNITTEST_H_
#define NET_URL_REQUEST_URL_REQUEST_UNITTEST_H_

#include <stdlib.h>

#include <sstream>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_test_util.h"
#include "net/http/http_network_layer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/proxy/proxy_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "googleurl/src/url_util.h"

const int kHTTPDefaultPort = 1337;
const int kFTPDefaultPort = 1338;

const std::string kDefaultHostName("localhost");

using base::TimeDelta;

// This URLRequestContext does not use a local cache.
class TestURLRequestContext : public URLRequestContext {
 public:
  TestURLRequestContext() {
    proxy_service_ = net::ProxyService::CreateNull();
    http_transaction_factory_ =
        net::HttpNetworkLayer::CreateFactory(proxy_service_);
  }

  explicit TestURLRequestContext(const std::string& proxy) {
    net::ProxyInfo proxy_info;
    proxy_info.UseNamedProxy(proxy);
    proxy_service_ = net::ProxyService::Create(&proxy_info);
    http_transaction_factory_ =
        net::HttpNetworkLayer::CreateFactory(proxy_service_);
  }

  virtual ~TestURLRequestContext() {
    delete http_transaction_factory_;
    delete proxy_service_;
  }
};

class TestDelegate : public URLRequest::Delegate {
 public:
  TestDelegate()
      : cancel_in_rr_(false),
        cancel_in_rs_(false),
        cancel_in_rd_(false),
        cancel_in_rd_pending_(false),
        quit_on_complete_(true),
        response_started_count_(0),
        received_bytes_count_(0),
        received_redirect_count_(0),
        received_data_before_response_(false),
        request_failed_(false),
        buf_(new net::IOBuffer(kBufferSize)) {
  }

  virtual void OnReceivedRedirect(URLRequest* request, const GURL& new_url) {
    received_redirect_count_++;
    if (cancel_in_rr_)
      request->Cancel();
  }

  virtual void OnResponseStarted(URLRequest* request) {
    // It doesn't make sense for the request to have IO pending at this point.
    DCHECK(!request->status().is_io_pending());

    response_started_count_++;
    if (cancel_in_rs_) {
      request->Cancel();
      OnResponseCompleted(request);
    } else if (!request->status().is_success()) {
      DCHECK(request->status().status() == URLRequestStatus::FAILED ||
             request->status().status() == URLRequestStatus::CANCELED);
      request_failed_ = true;
      OnResponseCompleted(request);
    } else {
      // Initiate the first read.
      int bytes_read = 0;
      if (request->Read(buf_, kBufferSize, &bytes_read))
        OnReadCompleted(request, bytes_read);
      else if (!request->status().is_io_pending())
        OnResponseCompleted(request);
    }
  }

  virtual void OnReadCompleted(URLRequest* request, int bytes_read) {
    // It doesn't make sense for the request to have IO pending at this point.
    DCHECK(!request->status().is_io_pending());

    if (response_started_count_ == 0)
      received_data_before_response_ = true;

    if (cancel_in_rd_)
      request->Cancel();

    if (bytes_read >= 0) {
      // There is data to read.
      received_bytes_count_ += bytes_read;

      // consume the data
      data_received_.append(buf_->data(), bytes_read);
    }

    // If it was not end of stream, request to read more.
    if (request->status().is_success() && bytes_read > 0) {
      bytes_read = 0;
      while (request->Read(buf_, kBufferSize, &bytes_read)) {
        if (bytes_read > 0) {
          data_received_.append(buf_->data(), bytes_read);
          received_bytes_count_ += bytes_read;
        } else {
          break;
        }
      }
    }
    if (!request->status().is_io_pending())
      OnResponseCompleted(request);
    else if (cancel_in_rd_pending_)
      request->Cancel();
  }

  virtual void OnResponseCompleted(URLRequest* request) {
    if (quit_on_complete_)
      MessageLoop::current()->Quit();
  }

  void OnAuthRequired(URLRequest* request, net::AuthChallengeInfo* auth_info) {
    if (!username_.empty() || !password_.empty()) {
      request->SetAuth(username_, password_);
    } else {
      request->CancelAuth();
    }
  }

  virtual void OnSSLCertificateError(URLRequest* request,
                                     int cert_error,
                                     net::X509Certificate* cert) {
    // Ignore SSL errors, we test the server is started and shut it down by
    // performing GETs, no security restrictions should apply as we always want
    // these GETs to go through.
    request->ContinueDespiteLastError();
  }

  void set_cancel_in_received_redirect(bool val) { cancel_in_rr_ = val; }
  void set_cancel_in_response_started(bool val) { cancel_in_rs_ = val; }
  void set_cancel_in_received_data(bool val) { cancel_in_rd_ = val; }
  void set_cancel_in_received_data_pending(bool val) {
    cancel_in_rd_pending_ = val;
  }
  void set_quit_on_complete(bool val) { quit_on_complete_ = val; }
  void set_username(const std::wstring& u) { username_ = u; }
  void set_password(const std::wstring& p) { password_ = p; }

  // query state
  const std::string& data_received() const { return data_received_; }
  int bytes_received() const { return static_cast<int>(data_received_.size()); }
  int response_started_count() const { return response_started_count_; }
  int received_redirect_count() const { return received_redirect_count_; }
  bool received_data_before_response() const {
    return received_data_before_response_;
  }
  bool request_failed() const { return request_failed_; }

 private:
  static const int kBufferSize = 4096;
  // options for controlling behavior
  bool cancel_in_rr_;
  bool cancel_in_rs_;
  bool cancel_in_rd_;
  bool cancel_in_rd_pending_;
  bool quit_on_complete_;

  std::wstring username_;
  std::wstring password_;

  // tracks status of callbacks
  int response_started_count_;
  int received_bytes_count_;
  int received_redirect_count_;
  bool received_data_before_response_;
  bool request_failed_;
  std::string data_received_;

  // our read buffer
  scoped_refptr<net::IOBuffer> buf_;
};

// This object bounds the lifetime of an external python-based HTTP/FTP server
// that can provide various responses useful for testing.
class BaseTestServer : public base::RefCounted<BaseTestServer> {
 protected:
  BaseTestServer() { }

 public:

  // Used with e.g. HTTPTestServer::SendQuit()
  bool WaitToFinish(int milliseconds) {
    return launcher_.WaitToFinish(milliseconds);
  }

  bool Stop() {
    return launcher_.Stop();
  }

  GURL TestServerPage(const std::string& base_address,
      const std::string& path) {
    return GURL(base_address + path);
  }

  GURL TestServerPage(const std::string& path) {
    return GURL(base_address_ + path);
  }

  GURL TestServerPageW(const std::wstring& path) {
    return GURL(base_address_ + WideToUTF8(path));
  }

  virtual bool MakeGETRequest(const std::string& page_name) = 0;

  std::wstring GetDataDirectory() {
    return launcher_.GetDocumentRootPath().ToWStringHack();
  }

 protected:
  bool Start(net::TestServerLauncher::Protocol protocol,
             const std::string& host_name, int port,
             const FilePath& document_root,
             const FilePath& cert_path) {
    std::string blank;
    return Start(protocol, host_name, port, document_root, cert_path,
                 blank, blank);
  }

  bool Start(net::TestServerLauncher::Protocol protocol,
             const std::string& host_name, int port,
             const FilePath& document_root,
             const FilePath& cert_path,
             const std::string& url_user,
             const std::string& url_password) {
    if (!launcher_.Start(protocol,
        host_name, port, document_root, cert_path))
      return false;

    std::string scheme;
    if (protocol == net::TestServerLauncher::ProtoFTP)
      scheme = "ftp";
    else
      scheme = "http";
    if (!cert_path.empty())
      scheme.push_back('s');

    std::string port_str = IntToString(port);
    if (url_user.empty()) {
      base_address_ = scheme + "://" + host_name + ":" + port_str + "/";
    } else {
      if (url_password.empty())
        base_address_ = scheme + "://" + url_user + "@" +
            host_name + ":" + port_str + "/";
      else
        base_address_ = scheme + "://" + url_user + ":" + url_password +
            "@" + host_name + ":" + port_str + "/";
    }
    return true;
  }

  // Used by MakeGETRequest to implement sync load behavior.
  class SyncTestDelegate : public TestDelegate {
   public:
    SyncTestDelegate() : event_(false, false), success_(false) {
    }
    virtual void OnResponseCompleted(URLRequest* request) {
      MessageLoop::current()->DeleteSoon(FROM_HERE, request);
      success_ = request->status().is_success();
      event_.Signal();
    }
    bool Wait(int64 secs) {
      TimeDelta td = TimeDelta::FromSeconds(secs);
      if (event_.TimedWait(td))
        return true;
      return false;
    }
    bool did_succeed() const { return success_; }
   private:
    base::WaitableEvent event_;
    bool success_;
    DISALLOW_COPY_AND_ASSIGN(SyncTestDelegate);
  };

  net::TestServerLauncher launcher_;
  std::string base_address_;
};


// HTTP
class HTTPTestServer : public BaseTestServer {
 protected:
  explicit HTTPTestServer() : loop_(NULL) {
  }

 public:
  // Creates and returns a new HTTPTestServer. If |loop| is non-null, requests
  // are serviced on it, otherwise a new thread and message loop are created.
  static scoped_refptr<HTTPTestServer> CreateServer(
      const std::wstring& document_root,
      MessageLoop* loop) {
    scoped_refptr<HTTPTestServer> test_server = new HTTPTestServer();
    test_server->loop_ = loop;
    FilePath no_cert;
    FilePath docroot = FilePath::FromWStringHack(document_root);
    if (!test_server->Start(net::TestServerLauncher::ProtoHTTP,
                            kDefaultHostName, kHTTPDefaultPort,
                            docroot, no_cert)) {
      return NULL;
    }
    return test_server;
  }

  // A subclass may wish to send the request in a different manner
  virtual bool MakeGETRequest(const std::string& page_name) {
    const GURL& url = TestServerPage(page_name);

    // Spin up a background thread for this request so that we have access to
    // an IO message loop, and in cases where this thread already has an IO
    // message loop, we also want to avoid spinning a nested message loop.
    SyncTestDelegate d;
    {
      MessageLoop* loop = loop_;
      scoped_ptr<base::Thread> io_thread;

      if (!loop) {
        io_thread.reset(new base::Thread("MakeGETRequest"));
        base::Thread::Options options;
        options.message_loop_type = MessageLoop::TYPE_IO;
        io_thread->StartWithOptions(options);
        loop = io_thread->message_loop();
      }
      loop->PostTask(FROM_HERE, NewRunnableFunction(
            &HTTPTestServer::StartGETRequest, url, &d));

      // Build bot wait for only 300 seconds we should ensure wait do not take
      // more than 300 seconds
      if (!d.Wait(250))
        return false;
    }
    return d.did_succeed();
  }

  static void StartGETRequest(const GURL& url, URLRequest::Delegate* delegate) {
    URLRequest* request = new URLRequest(url, delegate);
    request->set_context(new TestURLRequestContext());
    request->set_method("GET");
    request->Start();
    EXPECT_TRUE(request->is_pending());
  }

  // Some tests use browser javascript to fetch a 'kill' url that causes
  // the server to exit by itself (rather than letting TestServerLauncher's
  // destructor kill it).
  // This method does the same thing so we can unit test that mechanism.
  // You can then use WaitToFinish() to sleep until the server terminates.
  void SendQuit() {
    // Append the time to avoid problems where the kill page
    // is being cached rather than being executed on the server
    std::string page_name = StringPrintf("kill?%u",
        static_cast<int>(base::Time::Now().ToInternalValue()));
    int retry_count = 5;
    while (retry_count > 0) {
      bool r = MakeGETRequest(page_name);
      // BUG #1048625 causes the kill GET to fail.  For now we just retry.
      // Once the bug is fixed, we should remove the while loop and put back
      // the following DCHECK.
      // DCHECK(r);
      if (r)
        break;
      retry_count--;
    }
    // Make sure we were successful in stopping the testserver.
    DCHECK(retry_count > 0);
  }

  virtual std::string scheme() { return "http"; }

 private:
  // If non-null a background thread isn't created and instead this message loop
  // is used.
  MessageLoop* loop_;
};

class HTTPSTestServer : public HTTPTestServer {
 protected:
  explicit HTTPSTestServer() {
  }

 public:
  // Create a server with a valid certificate
  // TODO(dkegel): HTTPSTestServer should not require an instance to specify
  // stock test certificates
  static scoped_refptr<HTTPSTestServer> CreateGoodServer(
      const std::wstring& document_root) {
    scoped_refptr<HTTPSTestServer> test_server = new HTTPSTestServer();
    FilePath docroot = FilePath::FromWStringHack(document_root);
    FilePath certpath = test_server->launcher_.GetOKCertPath();
    if (!test_server->Start(net::TestServerLauncher::ProtoHTTP,
        net::TestServerLauncher::kHostName,
        net::TestServerLauncher::kOKHTTPSPort,
        docroot, certpath)) {
      return NULL;
    }
    return test_server;
  }

  // Create a server with an up to date certificate for the wrong hostname
  // for this host
  static scoped_refptr<HTTPSTestServer> CreateMismatchedServer(
      const std::wstring& document_root) {
    scoped_refptr<HTTPSTestServer> test_server = new HTTPSTestServer();
    FilePath docroot = FilePath::FromWStringHack(document_root);
    FilePath certpath = test_server->launcher_.GetOKCertPath();
    if (!test_server->Start(net::TestServerLauncher::ProtoHTTP,
        net::TestServerLauncher::kMismatchedHostName,
        net::TestServerLauncher::kOKHTTPSPort,
        docroot, certpath)) {
      return NULL;
    }
    return test_server;
  }

  // Create a server with an expired certificate
  static scoped_refptr<HTTPSTestServer> CreateExpiredServer(
      const std::wstring& document_root) {
    HTTPSTestServer* test_server = new HTTPSTestServer();
    FilePath docroot = FilePath::FromWStringHack(document_root);
    FilePath certpath = test_server->launcher_.GetExpiredCertPath();
    if (!test_server->Start(net::TestServerLauncher::ProtoHTTP,
        net::TestServerLauncher::kHostName,
        net::TestServerLauncher::kBadHTTPSPort,
        docroot, certpath)) {
      return NULL;
    }
    return test_server;
  }

  // Create a server with an arbitrary certificate
  static scoped_refptr<HTTPSTestServer> CreateServer(
      const std::string& host_name, int port,
      const std::wstring& document_root,
      const std::wstring& cert_path) {
    scoped_refptr<HTTPSTestServer> test_server = new HTTPSTestServer();
    FilePath docroot = FilePath::FromWStringHack(document_root);
    FilePath certpath = FilePath::FromWStringHack(cert_path);
    if (!test_server->Start(net::TestServerLauncher::ProtoHTTP,
        host_name, port, docroot, certpath)) {
      return NULL;
    }
    return test_server;
  }

  virtual ~HTTPSTestServer() {
  }

 protected:
  std::wstring cert_path_;
};


class FTPTestServer : public BaseTestServer {
 public:
  FTPTestServer() {
  }

  static scoped_refptr<FTPTestServer> CreateServer(
      const std::wstring& document_root) {
    std::string blank;
    return CreateServer(document_root, blank, blank);
  }

  static scoped_refptr<FTPTestServer> CreateServer(
      const std::wstring& document_root,
      const std::string& url_user,
      const std::string& url_password) {
    scoped_refptr<FTPTestServer> test_server = new FTPTestServer();
    FilePath docroot = FilePath::FromWStringHack(document_root);
    FilePath no_cert;
    if (!test_server->Start(net::TestServerLauncher::ProtoFTP,
        kDefaultHostName, kFTPDefaultPort, docroot, no_cert,
        url_user, url_password)) {
      return NULL;
    }
    return test_server;
  }

  virtual bool MakeGETRequest(const std::string& page_name) {
    const GURL& url = TestServerPage(base_address_, page_name);
    TestDelegate d;
    URLRequest request(url, &d);
    request.set_context(new TestURLRequestContext());
    request.set_method("GET");
    request.Start();
    EXPECT_TRUE(request.is_pending());

    MessageLoop::current()->Run();
    if (request.is_pending())
      return false;

    return true;
  }

};

#endif  // NET_URL_REQUEST_URL_REQUEST_UNITTEST_H_

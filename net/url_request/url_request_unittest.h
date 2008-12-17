// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_UNITTEST_H_
#define NET_URL_REQUEST_URL_REQUEST_UNITTEST_H_

#include <stdlib.h>

#include <sstream>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_layer.h"
#include "net/url_request/url_request.h"
#include "net/proxy/proxy_resolver_null.h"
#include "net/proxy/proxy_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "googleurl/src/url_util.h"

const int kDefaultPort = 1337;
const std::string kDefaultHostName("localhost");

// This URLRequestContext does not use a local cache.
class TestURLRequestContext : public URLRequestContext {
 public:
  TestURLRequestContext() {
    proxy_service_ = new net::ProxyService(new net::ProxyResolverNull);
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
        request_failed_(false) {
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
      if (request->Read(buf_, sizeof(buf_), &bytes_read))
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
      data_received_.append(buf_, bytes_read);
    }

    // If it was not end of stream, request to read more.
    if (request->status().is_success() && bytes_read > 0) {
      bytes_read = 0;
      while (request->Read(buf_, sizeof(buf_), &bytes_read)) {
        if (bytes_read > 0) {
          data_received_.append(buf_, bytes_read);
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
  char buf_[4096];
};

// This object bounds the lifetime of an external python-based HTTP server
// that can provide various responses useful for testing.
class TestServer : public base::ProcessFilter {
 public:
  TestServer(const std::wstring& document_root)
      : process_handle_(NULL),
        is_shutdown_(true) {
    Init(kDefaultHostName, kDefaultPort, document_root, std::wstring());
  }

  virtual ~TestServer() {
    Shutdown();
  }

  // Implementation of ProcessFilter
  virtual bool Includes(uint32 pid, uint32 parent_pid) const {
    // This function may be called after Shutdown(), in which process_handle_ is
    // set to NULL. Since no process handle is set, it can't be included in the
    // filter.
    if (!process_handle_)
      return false;
    // TODO(port): rationalize return value of GetProcId
    return pid == (uint32)base::GetProcId(process_handle_);
  }

  GURL TestServerPage(const std::string& path) {
    return GURL(base_address_ + path);
  }

  GURL TestServerPageW(const std::wstring& path) {
    return GURL(base_address_ + WideToUTF8(path));
  }

  // A subclass may wish to send the request in a different manner
  virtual bool MakeGETRequest(const std::string& page_name) {
    const GURL& url = TestServerPage(page_name);

    // Spin up a background thread for this request so that we have access to
    // an IO message loop, and in cases where this thread already has an IO
    // message loop, we also want to avoid spinning a nested message loop.
    
    SyncTestDelegate d;
    {
      base::Thread io_thread("MakeGETRequest");
      base::Thread::Options options;
      options.message_loop_type = MessageLoop::TYPE_IO;
      io_thread.StartWithOptions(options);
      io_thread.message_loop()->PostTask(FROM_HERE, NewRunnableFunction(
          &TestServer::StartGETRequest, url, &d));
      d.Wait();
    }
    return d.did_succeed();
  }

  bool init_successful() const { return init_successful_; }

 protected:
  struct ManualInit {};

  // Used by subclasses that need to defer initialization until they are fully
  // constructed.  The subclass should call Init once it is ready (usually in
  // its constructor).
  TestServer(ManualInit)
      : process_handle_(NULL),
        init_successful_(false),
        is_shutdown_(true) {
  }

  virtual std::string scheme() { return std::string("http"); }

  // This is in a separate function so that we can have assertions and so that
  // subclasses can call this later.
  void Init(const std::string& host_name, int port,
            const std::wstring& document_root,
            const std::wstring& cert_path) {
    std::stringstream ss;
    std::string port_str;
    ss << (port ? port : kDefaultPort);
    ss >> port_str;
    base_address_ = scheme() + "://" + host_name + ":" + port_str + "/";

    std::wstring testserver_path;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &testserver_path));
    file_util::AppendToPath(&testserver_path, L"net");
    file_util::AppendToPath(&testserver_path, L"tools");
    file_util::AppendToPath(&testserver_path, L"testserver");
    file_util::AppendToPath(&testserver_path, L"testserver.py");

    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &python_runtime_));
    file_util::AppendToPath(&python_runtime_, L"third_party");
    file_util::AppendToPath(&python_runtime_, L"python_24");
    file_util::AppendToPath(&python_runtime_, L"python.exe");

    std::wstring test_data_directory;
    PathService::Get(base::DIR_SOURCE_ROOT, &test_data_directory);
    std::wstring normalized_document_root = document_root;
#if defined(OS_WIN)
    std::replace(normalized_document_root.begin(),
                 normalized_document_root.end(),
                 L'/', FilePath::kSeparators[0]);
#endif
    file_util::AppendToPath(&test_data_directory, normalized_document_root);

#if defined(OS_WIN)
    std::wstring command_line =
        L"\"" + python_runtime_ + L"\" " + L"\"" + testserver_path +
        L"\" --port=" + UTF8ToWide(port_str) + L" --data-dir=\"" +
        test_data_directory + L"\"";
    if (!cert_path.empty()) {
      command_line.append(L" --https=\"");
      command_line.append(cert_path);
      command_line.append(L"\"");
    }

    ASSERT_TRUE(
        base::LaunchApp(command_line, false, true, &process_handle_)) <<
        "Failed to launch " << command_line;
#elif defined(OS_POSIX)
    // Set up PYTHONPATH so that Python is able to find the in-tree copy of
    // tlslite.

    static bool set_python_path = false;
    if (!set_python_path) {
      FilePath tlslite_path;
      ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &tlslite_path));
      tlslite_path = tlslite_path.Append("third_party");
      tlslite_path = tlslite_path.Append("tlslite");

      const char kPythonPath[] = "PYTHONPATH";
      char* python_path_c = getenv(kPythonPath);
      if (python_path_c) {
        // PYTHONPATH is already set, append to it.
        std::string python_path(python_path_c);
        python_path.append(":");
        python_path.append(tlslite_path.value());
        setenv(kPythonPath, python_path.c_str(), 1);
      } else {
        setenv(kPythonPath, tlslite_path.value().c_str(), 1);
      }

      set_python_path = true;
    }

    std::vector<std::string> command_line;
    command_line.push_back("python");
    command_line.push_back(WideToUTF8(testserver_path));
    command_line.push_back("--port=" + port_str);
    command_line.push_back("--data-dir=" + WideToUTF8(test_data_directory));
    if (!cert_path.empty()) 
      command_line.push_back("--https=" + WideToUTF8(cert_path));

    base::file_handle_mapping_vector no_mappings;
    ASSERT_TRUE(
        base::LaunchApp(command_line, no_mappings, false, &process_handle_)) <<
        "Failed to launch " << command_line[0] << " ...";
#endif

    // Verify that the webserver is actually started.
    // Otherwise tests can fail if they run faster than Python can start.
    int retries = 10;
    bool success;
    while ((success = MakeGETRequest("hello.html")) == false && retries > 0) {
      retries--;
      PlatformThread::Sleep(500);
    }
    ASSERT_TRUE(success) << "Webserver not starting properly.";

    init_successful_ = true;
    is_shutdown_ = false;
  }

  void Shutdown() {
    if (is_shutdown_)
      return;

    // here we append the time to avoid problems where the kill page
    // is being cached rather than being executed on the server
    std::ostringstream page_name;
    page_name << "kill?" << (unsigned int)(base::Time::Now().ToInternalValue());
    int retry_count = 5;
    while (retry_count > 0) {
      bool r = MakeGETRequest(page_name.str());
      // BUG #1048625 causes the kill GET to fail.  For now we just retry.
      // Once the bug is fixed, we should remove the while loop and put back
      // the following DCHECK.
      // DCHECK(r);
      if (r)
        break;
      retry_count--;
    }
    // Make sure we were successfull in stopping the testserver.
    DCHECK(retry_count > 0);

    if (process_handle_) {
#if defined(OS_WIN)
      CloseHandle(process_handle_);
#endif
      process_handle_ = NULL;
    }

    // Make sure we don't leave any stray testserver processes laying around.
    std::wstring testserver_name =
        file_util::GetFilenameFromPath(python_runtime_);
    base::CleanupProcesses(testserver_name, 10000, 1, this);
    EXPECT_EQ(0, base::GetProcessCount(testserver_name, this));

    is_shutdown_ = true;
  }

 private:
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
    void Wait() { event_.Wait(); }
    bool did_succeed() const { return success_; }
   private:
    base::WaitableEvent event_;
    bool success_;
    DISALLOW_COPY_AND_ASSIGN(SyncTestDelegate);
  };
  static void StartGETRequest(const GURL& url, URLRequest::Delegate* delegate) {
    URLRequest* request = new URLRequest(url, delegate);
    request->set_context(new TestURLRequestContext());
    request->set_method("GET");
    request->Start();
    EXPECT_TRUE(request->is_pending());
  }

  std::string base_address_;
  std::wstring python_runtime_;
  base::ProcessHandle process_handle_;
  bool init_successful_;
  bool is_shutdown_;
};

class HTTPSTestServer : public TestServer {
 public:
   HTTPSTestServer(const std::string& host_name, int port,
                   const std::wstring& document_root,
                   const std::wstring& cert_path) : TestServer(ManualInit()) {
    Init(host_name, port, document_root, cert_path);
  }

  virtual std::string scheme() { return std::string("https"); }
};

#endif  // NET_URL_REQUEST_URL_REQUEST_UNITTEST_H_


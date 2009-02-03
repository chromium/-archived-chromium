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
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_layer.h"
#include "net/url_request/url_request.h"
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
class BaseTestServer : public base::ProcessFilter,
                       public base::RefCounted<BaseTestServer> {
 protected:
  BaseTestServer()
      : process_handle_(NULL) {
  }

 public:
  virtual ~BaseTestServer() {
    if (process_handle_) {
#if defined(OS_WIN)
      CloseHandle(process_handle_);
#elif defined(OS_POSIX)
      // Make sure the process has exited and clean up the process to avoid
      // a zombie.
      kill(process_handle_, SIGINT);
      waitpid(process_handle_, 0, 0);
#endif
      process_handle_ = NULL;
    }
    // Make sure we don't leave any stray testserver processes laying around.
    std::wstring testserver_name =
    file_util::GetFilenameFromPath(python_runtime_);
    base::CleanupProcesses(testserver_name, 10000, 1, this);
    EXPECT_EQ(0, base::GetProcessCount(testserver_name, this));
  }

  // Implementation of ProcessFilter
  virtual bool Includes(uint32 pid, uint32 parent_pid) const {
    // Since no process handle is set, it can't be included in the filter.
    if (!process_handle_)
      return false;
    // TODO(port): rationalize return value of GetProcId
    return pid == static_cast<uint32>(base::GetProcId(process_handle_));
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

  void SetPythonPaths() {
#if defined(OS_WIN)
     // Set up PYTHONPATH so that Python is able to find the in-tree copy of
    // pyftpdlib.
    static bool set_python_path = false;
    if (!set_python_path) {
      FilePath pyftpdlib_path;
      ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &pyftpdlib_path));
      pyftpdlib_path = pyftpdlib_path.Append(L"third_party");
      pyftpdlib_path = pyftpdlib_path.Append(L"pyftpdlib");

      const wchar_t kPythonPath[] = L"PYTHONPATH";
      wchar_t python_path_c[1024];
      if (GetEnvironmentVariable(kPythonPath, python_path_c, 1023) > 0) {
        // PYTHONPATH is already set, append to it.
        std::wstring python_path(python_path_c);
        python_path.append(L":");
        python_path.append(pyftpdlib_path.value());
        SetEnvironmentVariableW(kPythonPath, python_path.c_str());
      } else {
        SetEnvironmentVariableW(kPythonPath, pyftpdlib_path.value().c_str());
      }

      set_python_path = true;
    }
#elif defined(OS_POSIX)
    // Set up PYTHONPATH so that Python is able to find the in-tree copy of
    // tlslite and pyftpdlib.
    static bool set_python_path = false;
    if (!set_python_path) {
      FilePath tlslite_path;
      FilePath pyftpdlib_path;
      ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &tlslite_path));
      tlslite_path = tlslite_path.Append("third_party");
      tlslite_path = tlslite_path.Append("tlslite");

      ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &pyftpdlib_path));
      pyftpdlib_path = pyftpdlib_path.Append("third_party");
      pyftpdlib_path = pyftpdlib_path.Append("pyftpdlib");

      const char kPythonPath[] = "PYTHONPATH";
      char* python_path_c = getenv(kPythonPath);
      if (python_path_c) {
        // PYTHONPATH is already set, append to it.
        std::string python_path(python_path_c);
        python_path.append(":");
        python_path.append(tlslite_path.value());
        python_path.append(":");
        python_path.append(pyftpdlib_path.value());
        setenv(kPythonPath, python_path.c_str(), 1);
      } else {
        std::string python_path = tlslite_path.value().c_str();
        python_path.append(":");
        python_path.append(pyftpdlib_path.value());
        setenv(kPythonPath, python_path.c_str(), 1);
      }
      set_python_path = true;
    }
#endif
  }

  void SetAppPath(const std::string& host_name, int port,
      const std::wstring& document_root, const std::string& scheme,
      std::wstring* testserver_path, std::wstring* test_data_directory) {
    port_str_ = IntToString(port);
    if (url_user_.empty()) {
      base_address_ = scheme + "://" + host_name + ":" + port_str_ + "/";
    } else {
      if (url_password_.empty())
        base_address_ = scheme + "://" + url_user_ + "@" +
            host_name + ":" + port_str_ + "/";
      else
        base_address_ = scheme + "://" + url_user_ + ":" + url_password_ +
            "@" + host_name + ":" + port_str_ + "/";
    }

    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, testserver_path));
    file_util::AppendToPath(testserver_path, L"net");
    file_util::AppendToPath(testserver_path, L"tools");
    file_util::AppendToPath(testserver_path, L"testserver");
    file_util::AppendToPath(testserver_path, L"testserver.py");

    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &python_runtime_));
    file_util::AppendToPath(&python_runtime_, L"third_party");
    file_util::AppendToPath(&python_runtime_, L"python_24");
    file_util::AppendToPath(&python_runtime_, L"python.exe");

    PathService::Get(base::DIR_SOURCE_ROOT, test_data_directory);
    std::wstring normalized_document_root = document_root;

#if defined(OS_WIN)
    // It is just for windows only and have no effect on other OS
    std::replace(normalized_document_root.begin(),
        normalized_document_root.end(),
        L'/', FilePath::kSeparators[0]);
#endif
    if (!normalized_document_root.empty())
      file_util::AppendToPath(test_data_directory, normalized_document_root);

  }

#if defined(OS_WIN)
  void LaunchApp(const std::wstring& command_line) {
    ASSERT_TRUE(base::LaunchApp(command_line, false, true, &process_handle_)) <<
                "Failed to launch " << command_line;
  }
#elif defined(OS_POSIX)
  void LaunchApp(const std::vector<std::string>& command_line) {
    base::file_handle_mapping_vector fds_empty;
    ASSERT_TRUE(base::LaunchApp(command_line, fds_empty, false,
                                &process_handle_)) <<
                "Failed to launch " << command_line[0] << " ...";
  }
#endif

  virtual bool MakeGETRequest(const std::string& page_name) = 0;

  // Verify that the Server is actually started.
  // Otherwise tests can fail if they run faster than Python can start.
  bool VerifyLaunchApp(const std::string& page_name) {
    int retries = 10;
    bool success;
    while ((success = MakeGETRequest(page_name)) == false && retries > 0) {
      retries--;
      PlatformThread::Sleep(500);
    }
    if (!success)
      return false;
    return true;
  }

 protected:
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

  std::string host_name_;
  std::string base_address_;
  std::string url_user_;
  std::string url_password_;
  std::wstring python_runtime_;
  base::ProcessHandle process_handle_;
  std::string port_str_;
};

class HTTPTestServer : public BaseTestServer {
 protected:
  explicit HTTPTestServer() : loop_(NULL) {
  }

 public:
  // Creates and returns a new HTTPTestServer. If |loop| is non-null, requests
  // are serviced on it, otherwise a new thread and message loop are created.
  static HTTPTestServer* CreateServer(const std::wstring& document_root,
                                      MessageLoop* loop) {
    HTTPTestServer* test_server = new HTTPTestServer();
    test_server->loop_ = loop;
    if (!test_server->Init(kDefaultHostName, kHTTPDefaultPort, document_root)) {
      delete test_server;
      return NULL;
    }
    return test_server;
  }

  bool Init(const std::string& host_name, int port,
            const std::wstring& document_root) {
    std::wstring testserver_path;
    std::wstring test_data_directory;
    host_name_ = host_name;
#if defined(OS_WIN)
    std::wstring command_line;
#elif defined(OS_POSIX)
    std::vector<std::string> command_line;
#endif

    // Set PYTHONPATH for tlslite and pyftpdlib
    SetPythonPaths();
    SetAppPath(host_name, port, document_root, scheme(),
        &testserver_path, &test_data_directory);
    SetCommandLineOption(testserver_path, test_data_directory, &command_line);
    LaunchApp(command_line);
    if (!VerifyLaunchApp("hello.html")) {
      LOG(ERROR) << "Webserver not starting properly";
      return false;
    }
    return true;
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

  virtual ~HTTPTestServer() {
    // here we append the time to avoid problems where the kill page
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
    // Make sure we were successfull in stopping the testserver.
    DCHECK(retry_count > 0);
  }

  virtual std::string scheme() { return "http"; }

#if defined(OS_WIN)
  virtual void SetCommandLineOption(const std::wstring& testserver_path,
                            const std::wstring& test_data_directory,
                            std::wstring* command_line ) {
    command_line->append(L"\"" + python_runtime_ + L"\" " + L"\"" +
    testserver_path + L"\" --port=" + UTF8ToWide(port_str_) +
    L" --data-dir=\"" + test_data_directory + L"\"");
  }
#elif defined(OS_POSIX)
  virtual void SetCommandLineOption(const std::wstring& testserver_path,
                            const std::wstring& test_data_directory,
                            std::vector<std::string>* command_line) {
    command_line->push_back("python");
    command_line->push_back(WideToUTF8(testserver_path));
    command_line->push_back("--port=" + port_str_);
    command_line->push_back("--data-dir=" + WideToUTF8(test_data_directory));
  }
#endif

 private:
  // If non-null a background thread isn't created and instead this message loop
  // is used.
  MessageLoop* loop_;
};

class HTTPSTestServer : public HTTPTestServer {
 protected:
  explicit HTTPSTestServer(const std::wstring& cert_path)
      : cert_path_(cert_path) {
  }

 public:
  static HTTPSTestServer* CreateServer(const std::string& host_name, int port,
                                       const std::wstring& document_root,
                                       const std::wstring& cert_path) {
    HTTPSTestServer* test_server = new HTTPSTestServer(cert_path);
    if (!test_server->Init(host_name, port, document_root)) {
      delete test_server;
      return NULL;
    }
    return test_server;
  }

#if defined(OS_WIN)
  virtual void SetCommandLineOption(const std::wstring& testserver_path,
                            const std::wstring& test_data_directory,
                            std::wstring* command_line ) {
    command_line->append(L"\"" + python_runtime_ + L"\" " + L"\"" +
    testserver_path + L"\"" + L" --port=" +
    UTF8ToWide(port_str_) + L" --data-dir=\"" +
    test_data_directory + L"\"");
    if (!cert_path_.empty()) {
      command_line->append(L" --https=\"");
      command_line->append(cert_path_);
      command_line->append(L"\"");
    }
  }
#elif defined(OS_POSIX)
  virtual void SetCommandLineOption(const std::wstring& testserver_path,
                            const std::wstring& test_data_directory,
                            std::vector<std::string>* command_line) {
    command_line->push_back("python");
    command_line->push_back(WideToUTF8(testserver_path));
    command_line->push_back("--port=" + port_str_);
    command_line->push_back("--data-dir=" + WideToUTF8(test_data_directory));
    if (!cert_path_.empty())
      command_line->push_back("--https=" + WideToUTF8(cert_path_));
}
#endif

  virtual std::string scheme() { return "https"; }

  virtual ~HTTPSTestServer() {
  }

 protected:
  std::wstring cert_path_;
};


class FTPTestServer : public BaseTestServer {
 protected:
  FTPTestServer() {
  }

 public:
  FTPTestServer(const std::string& url_user, const std::string& url_password) {
    url_user_ = url_user;
    url_password_ = url_password;
  }

  static FTPTestServer* CreateServer(const std::wstring& document_root) {
    FTPTestServer* test_server = new FTPTestServer();
    if (!test_server->Init(kDefaultHostName, kFTPDefaultPort, document_root)) {
      delete test_server;
      return NULL;
    }
    return test_server;
  }

  static FTPTestServer* CreateServer(const std::wstring& document_root,
                                     const std::string& url_user,
                                     const std::string& url_password) {
    FTPTestServer* test_server = new FTPTestServer(url_user, url_password);
    if (!test_server->Init(kDefaultHostName, kFTPDefaultPort, document_root)) {
      delete test_server;
      return NULL;
    }
    return test_server;
  }

  bool Init(const std::string& host_name, int port,
            const std::wstring& document_root) {
    std::wstring testserver_path;
    std::wstring test_data_directory;
    host_name_ = host_name;

#if defined(OS_WIN)
    std::wstring command_line;
#elif defined(OS_POSIX)
    std::vector<std::string> command_line;
#endif

    // Set PYTHONPATH for tlslite and pyftpdlib
    SetPythonPaths();
    SetAppPath(kDefaultHostName, port, document_root, scheme(),
        &testserver_path, &test_data_directory);
    SetCommandLineOption(testserver_path, test_data_directory, &command_line);
    LaunchApp(command_line);
    if (!VerifyLaunchApp("/LICENSE")) {
      LOG(ERROR) << "FTPServer not starting properly.";
      return false;
    }
    return true;
  }

  virtual ~FTPTestServer() {
    const std::string base_address = scheme() + "://" + host_name_ + ":" +
        port_str_ + "/";
    const GURL& url = TestServerPage(base_address, "kill");
    TestDelegate d;
    URLRequest request(url, &d);
    request.set_context(new TestURLRequestContext());
    request.set_method("GET");
    request.Start();
    EXPECT_TRUE(request.is_pending());

    MessageLoop::current()->Run();
  }

  virtual std::string scheme() { return "ftp"; }

  virtual bool MakeGETRequest(const std::string& page_name) {
    const std::string base_address = scheme() + "://" + host_name_ + ":" +
        port_str_ + "/";
    const GURL& url = TestServerPage(base_address, page_name);
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

#if defined(OS_WIN)
  virtual void SetCommandLineOption(const std::wstring& testserver_path,
                            const std::wstring& test_data_directory,
                            std::wstring* command_line ) {
    command_line->append(L"\"" + python_runtime_ + L"\" " + L"\"" +
    testserver_path + L"\"" + L" -f " + L" --port=" +
    UTF8ToWide(port_str_) + L" --data-dir=\"" +
    test_data_directory + L"\"");
  }
#elif defined(OS_POSIX)
  virtual void SetCommandLineOption(const std::wstring& testserver_path,
                            const std::wstring& test_data_directory,
                            std::vector<std::string>* command_line) {
    command_line->push_back("python");
    command_line->push_back(WideToUTF8(testserver_path));
    command_line->push_back(" -f ");
    command_line->push_back("--data-dir=" + WideToUTF8(test_data_directory));
    command_line->push_back("--port=" + port_str_);
  }
#endif
};

#endif  // NET_URL_REQUEST_URL_REQUEST_UNITTEST_H_

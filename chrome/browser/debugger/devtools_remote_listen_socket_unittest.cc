// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_remote_listen_socket_unittest.h"

#include <fcntl.h>

#include "base/eintr_wrapper.h"
#include "net/base/net_util.h"
#include "testing/platform_test.h"

const int DevToolsRemoteListenSocketTester::kTestPort = 9999;

static const int kReadBufSize = 1024;
static const char* kChromeDevToolsHandshake = "ChromeDevToolsHandshake\r\n";
static const char* kSimpleMessagePart1 =
    "Tool:V8Debugger\r\n"
    "Destination:2\r";
static const char* kSimpleMessagePart2 =
    "\n"
    "Content-Length:0\r\n"
    "\r\n";
static const char* kTwoMessages =
    "Tool:DevToolsService\r\n"
    "Content-Length:300\r\n"
    "\r\n"
    "00000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000"
    "Tool:V8Debugger\r\n"
    "Destination:1\r\n"
    "Content-Length:0\r\n"
    "\r\n";

static const int kMaxQueueSize = 20;
static const char* kLoopback = "127.0.0.1";
static const int kDefaultTimeoutMs = 5000;
#if defined(OS_POSIX)
static const char* kSemaphoreName = "chromium.listen_socket";
#endif


ListenSocket* DevToolsRemoteListenSocketTester::DoListen() {
  return DevToolsRemoteListenSocket::Listen(kLoopback, kTestPort, this, this);
}

void DevToolsRemoteListenSocketTester::SetUp() {
#if defined(OS_WIN)
  InitializeCriticalSection(&lock_);
  semaphore_ = CreateSemaphore(NULL, 0, kMaxQueueSize, NULL);
  server_ = NULL;
  net::EnsureWinsockInit();
#elif defined(OS_POSIX)
  ASSERT_EQ(0, pthread_mutex_init(&lock_, NULL));
  sem_unlink(kSemaphoreName);
  semaphore_ = sem_open(kSemaphoreName, O_CREAT, 0, 0);
  ASSERT_NE(SEM_FAILED, semaphore_);
#endif
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  thread_.reset(new base::Thread("socketio_test"));
  thread_->StartWithOptions(options);
  loop_ = static_cast<MessageLoopForIO*>(thread_->message_loop());

  loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &DevToolsRemoteListenSocketTester::Listen));

  // verify Listen succeeded
  ASSERT_TRUE(NextAction(kDefaultTimeoutMs));
  ASSERT_FALSE(server_ == NULL);
  ASSERT_EQ(ACTION_LISTEN, last_action_.type());

  // verify the connect/accept and setup test_socket_
  test_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  struct sockaddr_in client;
  client.sin_family = AF_INET;
  client.sin_addr.s_addr = inet_addr(kLoopback);
  client.sin_port = htons(kTestPort);
  int ret = HANDLE_EINTR(connect(test_socket_,
                                 reinterpret_cast<sockaddr*>(&client),
                                 sizeof(client)));
  ASSERT_NE(ret, SOCKET_ERROR);

  net::SetNonBlocking(test_socket_);
  ASSERT_TRUE(NextAction(kDefaultTimeoutMs));
  ASSERT_EQ(ACTION_ACCEPT, last_action_.type());
}

void DevToolsRemoteListenSocketTester::TearDown() {
  // verify close
#if defined(OS_WIN)
  closesocket(test_socket_);
#elif defined(OS_POSIX)
  HANDLE_EINTR(close(test_socket_));
#endif
  ASSERT_TRUE(NextAction(kDefaultTimeoutMs));
  ASSERT_EQ(ACTION_CLOSE, last_action_.type());

  loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &DevToolsRemoteListenSocketTester::Shutdown));
  ASSERT_TRUE(NextAction(kDefaultTimeoutMs));
  ASSERT_EQ(ACTION_SHUTDOWN, last_action_.type());

#if defined(OS_WIN)
  CloseHandle(semaphore_);
  semaphore_ = 0;
  DeleteCriticalSection(&lock_);
#elif defined(OS_POSIX)
  ASSERT_EQ(0, pthread_mutex_lock(&lock_));
  semaphore_ = NULL;
  ASSERT_EQ(0, pthread_mutex_unlock(&lock_));
  ASSERT_EQ(0, sem_unlink(kSemaphoreName));
  ASSERT_EQ(0, pthread_mutex_destroy(&lock_));
#endif

  thread_.reset();
  loop_ = NULL;
}

void DevToolsRemoteListenSocketTester::ReportAction(
    const ListenSocketTestAction& action) {
#if defined(OS_WIN)
  EnterCriticalSection(&lock_);
  queue_.push_back(action);
  LeaveCriticalSection(&lock_);
  ReleaseSemaphore(semaphore_, 1, NULL);
#elif defined(OS_POSIX)
  ASSERT_EQ(0, pthread_mutex_lock(&lock_));
  queue_.push_back(action);
  ASSERT_EQ(0, pthread_mutex_unlock(&lock_));
  ASSERT_EQ(0, sem_post(semaphore_));
#endif
}

bool DevToolsRemoteListenSocketTester::NextAction(int timeout) {
#if defined(OS_WIN)
  DWORD ret = ::WaitForSingleObject(semaphore_, timeout);
  if (ret != WAIT_OBJECT_0)
    return false;
  EnterCriticalSection(&lock_);
  if (queue_.size() == 0) {
    LeaveCriticalSection(&lock_);
    return false;
  }
  last_action_ = queue_.front();
  queue_.pop_front();
  LeaveCriticalSection(&lock_);
  return true;
#elif defined(OS_POSIX)
  if (semaphore_ == SEM_FAILED)
    return false;
  while (true) {
    int result = sem_trywait(semaphore_);
    PlatformThread::Sleep(1);  // 1MS sleep
    timeout--;
    if (timeout <= 0)
      return false;
    if (result == 0)
      break;
  }
  pthread_mutex_lock(&lock_);
  if (queue_.size() == 0) {
    pthread_mutex_unlock(&lock_);
    return false;
  }
  last_action_ = queue_.front();
  queue_.pop_front();
  pthread_mutex_unlock(&lock_);
  return true;
#endif
}

int DevToolsRemoteListenSocketTester::ClearTestSocket() {
  char buf[kReadBufSize];
  int len_ret = 0;
  int time_out = 0;
  do {
    int len = HANDLE_EINTR(recv(test_socket_, buf, kReadBufSize, 0));
#if defined(OS_WIN)
    if (len == SOCKET_ERROR) {
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK) {
#elif defined(OS_POSIX)
    if (len == SOCKET_ERROR) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
#endif
        PlatformThread::Sleep(1);
        time_out++;
        if (time_out > 10)
          break;
        continue;  // still trying
      }
    } else if (len == 0) {
      // socket closed
      break;
    } else {
      time_out = 0;
      len_ret += len;
    }
  } while (true);
  return len_ret;
}

void DevToolsRemoteListenSocketTester::Shutdown() {
  connection_->Release();
  connection_ = NULL;
  server_->Release();
  server_ = NULL;
  ReportAction(ListenSocketTestAction(ACTION_SHUTDOWN));
}

void DevToolsRemoteListenSocketTester::Listen() {
  server_ = DoListen();
  if (server_) {
    server_->AddRef();
    ReportAction(ListenSocketTestAction(ACTION_LISTEN));
  }
}

void DevToolsRemoteListenSocketTester::SendFromTester() {
  connection_->Send(kChromeDevToolsHandshake);
  ReportAction(ListenSocketTestAction(ACTION_SEND));
}

void DevToolsRemoteListenSocketTester::DidAccept(ListenSocket *server,
                                                 ListenSocket *connection) {
  connection_ = connection;
  connection_->AddRef();
  ReportAction(ListenSocketTestAction(ACTION_ACCEPT));
}

void DevToolsRemoteListenSocketTester::DidRead(ListenSocket *connection,
                                 const std::string& data) {
  ReportAction(ListenSocketTestAction(ACTION_READ, data));
}

void DevToolsRemoteListenSocketTester::DidClose(ListenSocket *sock) {
  ReportAction(ListenSocketTestAction(ACTION_CLOSE));
}

void DevToolsRemoteListenSocketTester::HandleMessage(
    const DevToolsRemoteMessage& message) {
  ReportAction(ListenSocketTestAction(ACTION_READ_MESSAGE, message));
}

bool DevToolsRemoteListenSocketTester::Send(SOCKET sock,
                                            const std::string& str) {
  int len = static_cast<int>(str.length());
  int send_len = HANDLE_EINTR(send(sock, str.data(), len, 0));
  if (send_len == SOCKET_ERROR) {
    LOG(ERROR) << "send failed: " << errno;
    return false;
  } else if (send_len != len) {
    return false;
  }
  return true;
}

void DevToolsRemoteListenSocketTester::TestClientSend() {
  ASSERT_TRUE(Send(test_socket_, kChromeDevToolsHandshake));
  {
    ASSERT_TRUE(Send(test_socket_, kSimpleMessagePart1));
    // sleep for 10ms to test message split between \r and \n
    PlatformThread::Sleep(10);
    ASSERT_TRUE(Send(test_socket_, kSimpleMessagePart2));
    ASSERT_TRUE(NextAction(kDefaultTimeoutMs));
    ASSERT_EQ(ACTION_READ_MESSAGE, last_action_.type());
    const DevToolsRemoteMessage& message = last_action_.message();
    ASSERT_STREQ("V8Debugger", message.GetHeaderWithEmptyDefault(
        DevToolsRemoteMessageHeaders::kTool).c_str());
    ASSERT_STREQ("2", message.GetHeaderWithEmptyDefault(
        DevToolsRemoteMessageHeaders::kDestination).c_str());
    ASSERT_STREQ("0", message.GetHeaderWithEmptyDefault(
        DevToolsRemoteMessageHeaders::kContentLength).c_str());
    ASSERT_EQ(0, static_cast<int>(message.content().size()));
  }
  ASSERT_TRUE(Send(test_socket_, kTwoMessages));
  {
    ASSERT_TRUE(NextAction(kDefaultTimeoutMs));
    ASSERT_EQ(ACTION_READ_MESSAGE, last_action_.type());
    const DevToolsRemoteMessage& message = last_action_.message();
    ASSERT_STREQ("DevToolsService", message.tool().c_str());
    ASSERT_STREQ("", message.destination().c_str());
    ASSERT_EQ(300, message.content_length());
    const std::string& content = message.content();
    ASSERT_EQ(300, static_cast<int>(content.size()));
    for (int i = 0; i < 300; ++i) {
      ASSERT_EQ('0', content[i]);
    }
  }
  {
    ASSERT_TRUE(NextAction(kDefaultTimeoutMs));
    ASSERT_EQ(ACTION_READ_MESSAGE, last_action_.type());
    const DevToolsRemoteMessage& message = last_action_.message();
    ASSERT_STREQ("V8Debugger", message.GetHeaderWithEmptyDefault(
        DevToolsRemoteMessageHeaders::kTool).c_str());
    ASSERT_STREQ("1", message.GetHeaderWithEmptyDefault(
        DevToolsRemoteMessageHeaders::kDestination).c_str());
    ASSERT_STREQ("0", message.GetHeaderWithEmptyDefault(
        DevToolsRemoteMessageHeaders::kContentLength).c_str());
    const std::string& content = message.content();
    ASSERT_EQ(0, static_cast<int>(content.size()));
  }
}

void DevToolsRemoteListenSocketTester::TestServerSend() {
  loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &DevToolsRemoteListenSocketTester::SendFromTester));
  ASSERT_TRUE(NextAction(kDefaultTimeoutMs));
  ASSERT_EQ(ACTION_SEND, last_action_.type());
  // TODO(erikkay): Without this sleep, the recv seems to fail a small amount
  // of the time.  I could fix this by making the socket blocking, but then
  // this test might hang in the case of errors.  It would be nice to do
  // something that felt more reliable here.
  PlatformThread::Sleep(10);  // sleep for 10ms
  const int buf_len = 200;
  char buf[buf_len+1];
  int recv_len = HANDLE_EINTR(recv(test_socket_, buf, buf_len, 0));
  ASSERT_NE(recv_len, SOCKET_ERROR);
  buf[recv_len] = 0;
  ASSERT_STREQ(buf, kChromeDevToolsHandshake);
}


class DevToolsRemoteListenSocketTest: public PlatformTest {
 public:
  DevToolsRemoteListenSocketTest() {
    tester_ = NULL;
  }

  virtual void SetUp() {
    PlatformTest::SetUp();
    tester_ = new DevToolsRemoteListenSocketTester();
    tester_->SetUp();
  }

  virtual void TearDown() {
    PlatformTest::TearDown();
    tester_->TearDown();
    tester_ = NULL;
  }

  scoped_refptr<DevToolsRemoteListenSocketTester> tester_;
};

TEST_F(DevToolsRemoteListenSocketTest, ServerSend) {
  tester_->TestServerSend();
}

TEST_F(DevToolsRemoteListenSocketTest, ClientSend) {
  tester_->TestClientSend();
}

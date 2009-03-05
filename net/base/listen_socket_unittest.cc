// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/listen_socket_unittest.h"

#include "net/base/net_util.h"
#include "testing/platform_test.h"

const int ListenSocketTester::kTestPort = 9999;

static const int kReadBufSize = 1024;
static const char* kHelloWorld = "HELLO, WORLD";
static const int kMaxQueueSize = 20;
static const char* kLoopback = "127.0.0.1";
static const int kDefaultTimeoutMs = 5000;
#if defined(OS_POSIX)
static const char* kSemaphoreName = "chromium.listen_socket";
#endif


ListenSocket* ListenSocketTester::DoListen() {
  return ListenSocket::Listen(kLoopback, kTestPort, this);
}

void ListenSocketTester::SetUp() {
#if defined(OS_WIN)
  InitializeCriticalSection(&lock_);
  semaphore_ = CreateSemaphore(NULL, 0, kMaxQueueSize, NULL);
  server_ = NULL;
  net::EnsureWinsockInit();
#elif defined(OS_POSIX)
  ASSERT_EQ(0, pthread_mutex_init(&lock_, NULL ));
  sem_unlink(kSemaphoreName);
  semaphore_ = sem_open(kSemaphoreName, O_CREAT, 0, 0);
  ASSERT_NE(SEM_FAILED, semaphore_);
#endif
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  thread_.reset(new base::Thread("socketio_test"));
  thread_->StartWithOptions(options);
  loop_ = (MessageLoopForIO*)thread_->message_loop();

  loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &ListenSocketTester::Listen));

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
  int ret = connect(test_socket_,
                    reinterpret_cast<sockaddr*>(&client), sizeof(client));
  ASSERT_NE(ret, SOCKET_ERROR);

  net::SetNonBlocking(test_socket_);
  ASSERT_TRUE(NextAction(kDefaultTimeoutMs));
  ASSERT_EQ(ACTION_ACCEPT, last_action_.type());
}

void ListenSocketTester::TearDown() {
  // verify close
#if defined(OS_WIN)
  closesocket(test_socket_);
#elif defined(OS_POSIX)
  close(test_socket_);
#endif
  ASSERT_TRUE(NextAction(kDefaultTimeoutMs));
  ASSERT_EQ(ACTION_CLOSE, last_action_.type());

  loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &ListenSocketTester::Shutdown));
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

void ListenSocketTester::ReportAction(const ListenSocketTestAction& action) {
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

bool ListenSocketTester::NextAction(int timeout) {
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
    PlatformThread::Sleep(1); // 1MS sleep
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

int ListenSocketTester::ClearTestSocket() {
  char buf[kReadBufSize];
  int len_ret = 0;
  int time_out = 0;
  do {
    int len = recv(test_socket_, buf, kReadBufSize, 0);
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

void ListenSocketTester::Shutdown() {
  connection_->Release();
  connection_ = NULL;
  server_->Release();
  server_ = NULL;
  ReportAction(ListenSocketTestAction(ACTION_SHUTDOWN));
}

void ListenSocketTester::Listen() {
  server_ = DoListen();
  if (server_) {
    server_->AddRef();
    ReportAction(ListenSocketTestAction(ACTION_LISTEN));
  }
}

void ListenSocketTester::SendFromTester() {
  connection_->Send(kHelloWorld);
  ReportAction(ListenSocketTestAction(ACTION_SEND));
}

void ListenSocketTester::DidAccept(ListenSocket *server,
                                   ListenSocket *connection) {
  connection_ = connection;
  connection_->AddRef();
  ReportAction(ListenSocketTestAction(ACTION_ACCEPT));
}

void ListenSocketTester::DidRead(ListenSocket *connection,
                                 const std::string& data) {
  ReportAction(ListenSocketTestAction(ACTION_READ, data));
}

void ListenSocketTester::DidClose(ListenSocket *sock) {
  ReportAction(ListenSocketTestAction(ACTION_CLOSE));
}

bool ListenSocketTester::Send(SOCKET sock, const std::string& str) {
  int len = static_cast<int>(str.length());
  int send_len = send(sock, str.data(), len, 0);
  if (send_len == SOCKET_ERROR) {
    LOG(ERROR) << "send failed: " << errno;
    return false;
  } else if (send_len != len) {
    return false;
  }
  return true;
}

void ListenSocketTester::TestClientSend() {
  ASSERT_TRUE(Send(test_socket_, kHelloWorld));
  ASSERT_TRUE(NextAction(kDefaultTimeoutMs));
  ASSERT_EQ(ACTION_READ, last_action_.type());
  ASSERT_EQ(last_action_.data(), kHelloWorld);
}

void ListenSocketTester::TestClientSendLong() {
  int hello_len = strlen(kHelloWorld);
  std::string long_string;
  int long_len = 0;
  for (int i = 0; i < 200; i++) {
    long_string += kHelloWorld;
    long_len += hello_len;
  }
  ASSERT_TRUE(Send(test_socket_, long_string));
  int read_len = 0;
  while (read_len < long_len) {
    ASSERT_TRUE(NextAction(kDefaultTimeoutMs));
    ASSERT_EQ(ACTION_READ, last_action_.type());
    std::string last_data = last_action_.data();
    size_t len = last_data.length();
    if (long_string.compare(read_len, len, last_data)) {
      ASSERT_EQ(long_string.compare(read_len, len, last_data), 0);
    }
    read_len += static_cast<int>(last_data.length());
  }
  ASSERT_EQ(read_len, long_len);
}

void ListenSocketTester::TestServerSend() {
  loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &ListenSocketTester::SendFromTester));
  ASSERT_TRUE(NextAction(kDefaultTimeoutMs));
  ASSERT_EQ(ACTION_SEND, last_action_.type());
  // TODO(erikkay): Without this sleep, the recv seems to fail a small amount
  // of the time.  I could fix this by making the socket blocking, but then
  // this test might hang in the case of errors.  It would be nice to do
  // something that felt more reliable here.
  PlatformThread::Sleep(10);  // sleep for 10ms
  const int buf_len = 200;
  char buf[buf_len+1];
  int recv_len = recv(test_socket_, buf, buf_len, 0);
  buf[recv_len] = 0;
  ASSERT_STREQ(buf, kHelloWorld);
}


class ListenSocketTest: public PlatformTest {
 public:
  ListenSocketTest() {
    tester_ = NULL;
  }

  virtual void SetUp() {
    PlatformTest::SetUp();
    tester_ = new ListenSocketTester();
    tester_->SetUp();
  }

  virtual void TearDown() {
    PlatformTest::TearDown();
    tester_->TearDown();
    tester_ = NULL;
  }

  scoped_refptr<ListenSocketTester> tester_;
};

TEST_F(ListenSocketTest, ClientSend) {
  tester_->TestClientSend();
}

TEST_F(ListenSocketTest, ClientSendLong) {
  tester_->TestClientSendLong();
}

TEST_F(ListenSocketTest, ServerSend) {
  tester_->TestServerSend();
}


// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef LISTEN_SOCKET_UNITTEST_H__
#define LISTEN_SOCKET_UNITTEST_H__

#include <winsock2.h>

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "net/base/listen_socket.h"
#include "net/base/winsock_init.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int TEST_PORT = 9999;
const std::string HELLO_WORLD("HELLO, WORLD");
const int MAX_QUEUE_SIZE = 20;

enum ActionType {
  ACTION_NONE = 0,
  ACTION_LISTEN = 1,
  ACTION_ACCEPT = 2,
  ACTION_READ = 3,
  ACTION_SEND = 4,
  ACTION_CLOSE = 5,
};

class ListenSocketTestAction {
 public:
  ListenSocketTestAction() : action_(ACTION_NONE) {}
  explicit ListenSocketTestAction(ActionType action) : action_(action) {}
  ListenSocketTestAction(ActionType action, std::string data)
      : action_(action),
        data_(data) {}

  const std::string data() const { return data_; }
  const ActionType type() const { return action_; }

 private:
  std::string data_;
  ActionType action_;
};

// This had to be split out into a separate class because I couldn't
// make a the testing::Test class refcounted.
class ListenSocketTester :
    public ListenSocket::ListenSocketDelegate,
    public base::RefCountedThreadSafe<ListenSocketTester> {
 protected:
  virtual ListenSocket* DoListen() {
    return ListenSocket::Listen("127.0.0.1", TEST_PORT, this, loop_);
  }

 public:
  ListenSocketTester()
      : server_(NULL),
        connection_(NULL),
        thread_(NULL),
        loop_(NULL) {
  }

  virtual ~ListenSocketTester() {
  }

  virtual void SetUp() {
    InitializeCriticalSection(&lock_);
    semaphore_ = CreateSemaphore(NULL, 0, MAX_QUEUE_SIZE, NULL);
    server_ = NULL;
    WinsockInit::Init();

    thread_.reset(new Thread("socketio_test"));
    thread_->Start();
    loop_ = thread_->message_loop();

    loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &ListenSocketTester::Listen));

    // verify Listen succeeded
    ASSERT_TRUE(NextAction());
    ASSERT_FALSE(server_ == NULL);
    ASSERT_EQ(ACTION_LISTEN, last_action_.type());

    // verify the connect/accept and setup test_socket_
    test_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in client;
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = inet_addr("127.0.0.1");
    client.sin_port = htons(TEST_PORT);
    int ret = connect(test_socket_,
                      reinterpret_cast<SOCKADDR*>(&client), sizeof(client));
    ASSERT_NE(ret, SOCKET_ERROR);
    // non-blocking socket
    unsigned long no_block = 1;
    ioctlsocket(test_socket_, FIONBIO, &no_block);
    ASSERT_TRUE(NextAction());
    ASSERT_EQ(ACTION_ACCEPT, last_action_.type());
  }

  virtual void TearDown() {
    // verify close
    closesocket(test_socket_);
    ASSERT_TRUE(NextAction(5000));
    ASSERT_EQ(ACTION_CLOSE, last_action_.type());

    CloseHandle(semaphore_);
    semaphore_ = 0;
    DeleteCriticalSection(&lock_);
    if (connection_) {
      loop_->ReleaseSoon(FROM_HERE, connection_);
      connection_ = NULL;
    }
    if (server_) {
      loop_->ReleaseSoon(FROM_HERE, server_);
      server_ = NULL;
    }
    thread_.reset();
    loop_ = NULL;
    WinsockInit::Cleanup();
  }

  void ReportAction(const ListenSocketTestAction& action) {
    EnterCriticalSection(&lock_);
    queue_.push_back(action);
    LeaveCriticalSection(&lock_);
    ReleaseSemaphore(semaphore_, 1, NULL);
  }

  bool NextAction(int timeout = 5000) {
    DWORD ret = ::WaitForSingleObject(semaphore_, timeout);
    if (ret != WAIT_OBJECT_0)
      return false;
    EnterCriticalSection(&lock_);
    if (queue_.size() == 0)
      return false;
    last_action_ = queue_.front();
    queue_.pop_front();
    LeaveCriticalSection(&lock_);
    return true;
  }

  // read all pending data from the test socket
  int ClearTestSocket() {
    char buf[1024];
    int len = 0;
    do {
      int ret = recv(test_socket_, buf, 1024, 0);
      if (ret < 0) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
          break;
        }
      } else {
        len += ret;
      }
    } while (true);
    return len;
  }

  void Listen() {
    server_ = DoListen();
    if (server_) {
      server_->AddRef();
      ReportAction(ListenSocketTestAction(ACTION_LISTEN));
    }
  }

  void SendFromTester() {
    connection_->Send(HELLO_WORLD);
    ReportAction(ListenSocketTestAction(ACTION_SEND));
  }

  virtual void DidAccept(ListenSocket *server, ListenSocket *connection) {
    connection_ = connection;
    connection_->AddRef();
    ReportAction(ListenSocketTestAction(ACTION_ACCEPT));
  }

  virtual void DidRead(ListenSocket *connection, const std::string& data) {
    ReportAction(ListenSocketTestAction(ACTION_READ, data));
  }

  virtual void DidClose(ListenSocket *sock) {
    ReportAction(ListenSocketTestAction(ACTION_CLOSE));
  }

  virtual bool Send(SOCKET sock, const std::string& str) {
    int len = static_cast<int>(str.length());
    int send_len = send(sock, str.data(), len, 0);
    if (send_len != len) {
      return false;
    }
    return true;
  }

  // verify the send/read from client to server
  void TestClientSend() {
    ASSERT_TRUE(Send(test_socket_, HELLO_WORLD));
    ASSERT_TRUE(NextAction());
    ASSERT_EQ(ACTION_READ, last_action_.type());
    ASSERT_EQ(last_action_.data(), HELLO_WORLD);
  }

  // verify send/read of a longer string
  void TestClientSendLong() {
    int hello_len = static_cast<int>(HELLO_WORLD.length());
    std::string long_string;
    int long_len = 0;
    for (int i = 0; i < 200; i++) {
      long_string += HELLO_WORLD;
      long_len += hello_len;
    }
    ASSERT_TRUE(Send(test_socket_, long_string));
    int read_len = 0;
    while (read_len < long_len) {
      ASSERT_TRUE(NextAction());
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

  // verify a send/read from server to client
  void TestServerSend() {
    loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &ListenSocketTester::SendFromTester));
    ASSERT_TRUE(NextAction());
    ASSERT_EQ(ACTION_SEND, last_action_.type());
    // TODO(erikkay): Without this sleep, the recv seems to fail a small amount
    // of the time.  I could fix this by making the socket blocking, but then
    // this test might hang in the case of errors.  It would be nice to do
    // something that felt more reliable here.
    Sleep(10);
    const int buf_len = 200;
    char buf[buf_len+1];
    int recv_len = recv(test_socket_, buf, buf_len, 0);
    buf[recv_len] = 0;
    ASSERT_EQ(buf, HELLO_WORLD);
  }

  scoped_ptr<Thread> thread_;
  MessageLoop* loop_;
  ListenSocket* server_;
  ListenSocket* connection_;
  CRITICAL_SECTION lock_;
  HANDLE semaphore_;
  ListenSocketTestAction last_action_;
  std::deque<ListenSocketTestAction> queue_;
  SOCKET test_socket_;
};

}  // namespace

#endif  // LISTEN_SOCKET_UNITTEST_H__

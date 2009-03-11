// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LISTEN_SOCKET_UNITTEST_H_
#define NET_BASE_LISTEN_SOCKET_UNITTEST_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <sys/socket.h>
#include <errno.h>
#include <semaphore.h>
#include <arpa/inet.h>
#endif

#include "base/thread.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "net/base/net_util.h"
#include "net/base/listen_socket.h"
#include "net/base/winsock_init.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
// Used same name as in Windows to avoid #ifdef where refrenced
#define SOCKET int
const int INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;
#endif

enum ActionType {
  ACTION_NONE = 0,
  ACTION_LISTEN = 1,
  ACTION_ACCEPT = 2,
  ACTION_READ = 3,
  ACTION_SEND = 4,
  ACTION_CLOSE = 5,
  ACTION_SHUTDOWN = 6
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
  ActionType action_;
  std::string data_;
};


// This had to be split out into a separate class because I couldn't
// make a the testing::Test class refcounted.
class ListenSocketTester :
    public ListenSocket::ListenSocketDelegate,
    public base::RefCountedThreadSafe<ListenSocketTester> {

 protected:
  virtual ListenSocket* DoListen();

 public:
  ListenSocketTester()
      : thread_(NULL),
        loop_(NULL),
        server_(NULL),
        connection_(NULL){
  }

  virtual ~ListenSocketTester() {
  }

  virtual void SetUp();
  virtual void TearDown();

  void ReportAction(const ListenSocketTestAction& action);
  bool NextAction(int timeout);

  // read all pending data from the test socket
  int ClearTestSocket();
  // Release the connection and server sockets
  void Shutdown();
  void Listen();
  void SendFromTester();
  virtual void DidAccept(ListenSocket *server, ListenSocket *connection);
  virtual void DidRead(ListenSocket *connection, const std::string& data);
  virtual void DidClose(ListenSocket *sock);
  virtual bool Send(SOCKET sock, const std::string& str);
  // verify the send/read from client to server
  void TestClientSend();
  // verify send/read of a longer string
  void TestClientSendLong();
  // verify a send/read from server to client
  void TestServerSend();

#if defined(OS_WIN)
  CRITICAL_SECTION lock_;
  HANDLE semaphore_;
#elif defined(OS_POSIX)
  pthread_mutex_t lock_;
  sem_t* semaphore_;
#endif

  scoped_ptr<base::Thread> thread_;
  MessageLoopForIO* loop_;
  ListenSocket* server_;
  ListenSocket* connection_;
  ListenSocketTestAction last_action_;
  std::deque<ListenSocketTestAction> queue_;
  SOCKET test_socket_;
  static const int kTestPort;
};

#endif  // NET_BASE_LISTEN_SOCKET_UNITTEST_H_

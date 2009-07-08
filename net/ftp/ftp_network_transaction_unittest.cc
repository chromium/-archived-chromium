// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_network_transaction.h"

#include "base/ref_counted.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_unittest.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/ftp/ftp_network_session.h"
#include "net/ftp/ftp_request_info.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Size we use for IOBuffers used to receive data from the test data socket.
const int kBufferSize = 128;

}  // namespace

namespace net {

class FtpMockControlSocket : public DynamicMockSocket {
 public:
  enum State {
    NONE,
    PRE_USER,
    PRE_PASSWD,
    PRE_SYST,
    PRE_PWD,
    PRE_TYPE,
    PRE_PASV,
    PRE_SIZE,
    PRE_MDTM,
    PRE_LIST,
    PRE_RETR,
    PRE_PASV2,
    PRE_CWD,
    PRE_QUIT,
    QUIT
  };

  FtpMockControlSocket()
      : failure_injection_state_(NONE),
        multiline_welcome_(false) {
    Init();
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, OK);
    switch (state()) {
      case PRE_USER:
        return Verify("USER anonymous\r\n", data, PRE_PASSWD,
                      "331 Password needed\r\n");
      case PRE_PASSWD:
        {
          const char* response_one = "230 Welcome\r\n";
          const char* response_multi = "230 One\r\n230 Two\r\n230 Three\r\n";
          return Verify("PASS chrome@example.com\r\n", data, PRE_SYST,
                        multiline_welcome_ ? response_multi : response_one);
        }
      case PRE_SYST:
        return Verify("SYST\r\n", data, PRE_PWD, "215 UNIX\r\n");
      case PRE_PWD:
        return Verify("PWD\r\n", data, PRE_TYPE,
                      "257 \"/\" is your current location\r\n");
      case PRE_TYPE:
        return Verify("TYPE I\r\n", data, PRE_PASV,
                      "200 TYPE is now 8-bit binary\r\n");
      case PRE_PASV:
        return Verify("PASV\r\n", data, PRE_SIZE,
                      "227 Entering Passive Mode (127,0,0,1,123,456)\r\n");
      case PRE_QUIT:
        return Verify("QUIT\r\n", data, QUIT, "221 Goodbye.\r\n");
      default:
        return MockWriteResult(true, ERR_UNEXPECTED);
    }
  }

  void InjectFailure(State state, State next_state, const char* response) {
    DCHECK_EQ(NONE, failure_injection_state_);
    DCHECK_NE(NONE, state);
    DCHECK_NE(NONE, next_state);
    DCHECK_NE(state, next_state);
    failure_injection_state_ = state;
    failure_injection_next_state_ = next_state;
    fault_response_ = response;
  }

  State state() const {
    return state_;
  }

  virtual void Reset() {
    DynamicMockSocket::Reset();
    Init();
  }

  void set_multiline_welcome(bool multiline) {
    multiline_welcome_ = multiline;
  }

 protected:
  void Init() {
    state_ = PRE_USER;
    SimulateRead("220 host TestFTPd\r\n");
  }

  // If protocol fault injection has been requested, adjusts state and mocked
  // read and returns true.
  bool InjectFault() {
    if (state_ != failure_injection_state_)
      return false;
    SimulateRead(fault_response_);
    state_ = failure_injection_next_state_;
    return true;
  }

  MockWriteResult Verify(const std::string& expected,
                         const std::string& data,
                         State next_state,
                         const char* next_read) {
    EXPECT_EQ(expected, data);
    if (expected == data) {
      state_ = next_state;
      SimulateRead(next_read);
      return MockWriteResult(true, OK);
    }
    return MockWriteResult(true, ERR_UNEXPECTED);
  }

 private:
  State state_;
  State failure_injection_state_;
  State failure_injection_next_state_;
  const char* fault_response_;

  // If true, we will send multiple 230 lines as response after PASS.
  bool multiline_welcome_;

  DISALLOW_COPY_AND_ASSIGN(FtpMockControlSocket);
};

class FtpMockControlSocketDirectoryListing : public FtpMockControlSocket {
 public:
  FtpMockControlSocketDirectoryListing() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, OK);
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /\r\n", data, PRE_MDTM,
                      "550 I can only retrieve regular files\r\n");
      case PRE_MDTM:
        return Verify("MDTM /\r\n", data, PRE_RETR,
                      "213 20070221112533\r\n");
      case PRE_RETR:
        return Verify("RETR /\r\n", data, PRE_PASV2,
                      "550 Can't download directory\r\n");
      case PRE_PASV2:
        // Parser should also accept format without parentheses.
        return Verify("PASV\r\n", data, PRE_CWD,
                      "227 Entering Passive Mode 127,0,0,1,123,456\r\n");
      case PRE_CWD:
        return Verify("CWD /\r\n", data, PRE_LIST, "200 OK\r\n");
      case PRE_LIST:
        // TODO(phajdan.jr): Also test with "150 Accepted Data Connection".
        return Verify("LIST\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpMockControlSocket::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpMockControlSocketDirectoryListing);
};

class FtpMockControlSocketFileDownload : public FtpMockControlSocket {
 public:
  FtpMockControlSocketFileDownload() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, OK);
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /file\r\n", data, PRE_MDTM,
                      "213 18\r\n");
      case PRE_MDTM:
        return Verify("MDTM /file\r\n", data, PRE_RETR,
                      "213 20070221112533\r\n");
      case PRE_RETR:
        // TODO(phajdan.jr): Also test with "150 Accepted Data Connection".
        return Verify("RETR /file\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpMockControlSocket::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpMockControlSocketFileDownload);
};

class FtpMockControlSocketFileDownloadRetrFail
    : public FtpMockControlSocketFileDownload {
 public:
  FtpMockControlSocketFileDownloadRetrFail() {
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    if (InjectFault())
      return MockWriteResult(true, OK);
    switch (state()) {
      case PRE_PASV2:
        return Verify("PASV\r\n", data, PRE_CWD,
                      "227 Entering Passive Mode (127,0,0,1,123,456)\r\n");
      case PRE_CWD:
        return Verify("CWD /file\r\n", data, PRE_QUIT,
                      "500 file is a directory\r\n");
      default:
        return FtpMockControlSocketFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpMockControlSocketFileDownloadRetrFail);
};

class FtpNetworkTransactionTest : public PlatformTest {
 public:
  FtpNetworkTransactionTest()
      : session_(new FtpNetworkSession(new HostResolver)),
        transaction_(session_.get(), &mock_socket_factory_) {
  }

 protected:
  FtpRequestInfo GetRequestInfo(const std::string& url) {
    FtpRequestInfo info;
    info.url = GURL(url);
    return info;
  }

  void ExecuteTransaction(FtpMockControlSocket* ctrl_socket,
                          const char* request,
                          int expected_result) {
    std::string mock_data("mock-data");
    MockRead data_reads[] = {
      MockRead(mock_data.c_str()),
    };
    // TODO(phajdan.jr): FTP transaction should not open two data sockets.
    StaticMockSocket data_socket1(data_reads, NULL);
    StaticMockSocket data_socket2(data_reads, NULL);
    mock_socket_factory_.AddMockSocket(ctrl_socket);
    mock_socket_factory_.AddMockSocket(&data_socket1);
    mock_socket_factory_.AddMockSocket(&data_socket2);
    FtpRequestInfo request_info = GetRequestInfo(request);
    ASSERT_EQ(ERR_IO_PENDING, transaction_.Start(&request_info, &callback_));
    EXPECT_EQ(expected_result, callback_.WaitForResult());
    EXPECT_EQ(FtpMockControlSocket::QUIT, ctrl_socket->state());
    if (expected_result == OK) {
      scoped_refptr<IOBuffer> io_buffer(new IOBuffer(kBufferSize));
      memset(io_buffer->data(), 0, kBufferSize);
      ASSERT_EQ(ERR_IO_PENDING,
                transaction_.Read(io_buffer.get(), kBufferSize, &callback_));
      EXPECT_EQ(static_cast<int>(mock_data.length()),
                callback_.WaitForResult());
      EXPECT_EQ(mock_data, std::string(io_buffer->data(), mock_data.length()));
    }
  }

  void TransactionFailHelper(FtpMockControlSocket* ctrl_socket,
                             const char* request,
                             FtpMockControlSocket::State state,
                             FtpMockControlSocket::State next_state,
                             const char* response,
                             int expected_result) {
    ctrl_socket->InjectFailure(state, next_state, response);
    ExecuteTransaction(ctrl_socket, request, expected_result);
  }

  scoped_refptr<FtpNetworkSession> session_;
  MockClientSocketFactory mock_socket_factory_;
  FtpNetworkTransaction transaction_;
  TestCompletionCallback callback_;
};

TEST_F(FtpNetworkTransactionTest, FailedLookup) {
  FtpRequestInfo request_info = GetRequestInfo("ftp://badhost");
  scoped_refptr<RuleBasedHostMapper> mapper(new RuleBasedHostMapper());
  mapper->AddSimulatedFailure("badhost");
  ScopedHostMapper scoped_mapper(mapper.get());
  ASSERT_EQ(ERR_IO_PENDING, transaction_.Start(&request_info, &callback_));
  EXPECT_EQ(ERR_FAILED, callback_.WaitForResult());
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransaction) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionMultilineWelcome) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  ctrl_socket.set_multiline_welcome(true);
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionShortReads2) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  ctrl_socket.set_short_read_limit(2);
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionShortReads5) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  ctrl_socket.set_short_read_limit(5);
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionMultilineWelcomeShort) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  // The client will not consume all three 230 lines. That's good, we want to
  // test that scenario.
  ctrl_socket.allow_unconsumed_reads(true);
  ctrl_socket.set_multiline_welcome(true);
  ctrl_socket.set_short_read_limit(5);
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransaction) {
  FtpMockControlSocketFileDownload ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionMultilineWelcome) {
  FtpMockControlSocketFileDownload ctrl_socket;
  ctrl_socket.set_multiline_welcome(true);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionShortReads2) {
  FtpMockControlSocketFileDownload ctrl_socket;
  ctrl_socket.set_short_read_limit(2);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionShortReads5) {
  FtpMockControlSocketFileDownload ctrl_socket;
  ctrl_socket.set_short_read_limit(5);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailUser) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpMockControlSocket::PRE_USER,
                        FtpMockControlSocket::PRE_QUIT,
                        "500 no such user\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailPass) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpMockControlSocket::PRE_PASSWD,
                        FtpMockControlSocket::PRE_QUIT,
                        "530 Login authentication failed\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailSyst) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpMockControlSocket::PRE_SYST,
                        FtpMockControlSocket::PRE_PWD,
                        "500 failed syst\r\n",
                        OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailPwd) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpMockControlSocket::PRE_PWD,
                        FtpMockControlSocket::PRE_QUIT,
                        "500 failed pwd\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailType) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpMockControlSocket::PRE_TYPE,
                        FtpMockControlSocket::PRE_QUIT,
                        "500 failed type\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailPasv) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpMockControlSocket::PRE_PASV,
                        FtpMockControlSocket::PRE_QUIT,
                        "500 failed pasv\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionMalformedMdtm) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpMockControlSocket::PRE_MDTM,
                        FtpMockControlSocket::PRE_RETR,
                        "213 foobar\r\n",
                        OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailMdtm) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpMockControlSocket::PRE_MDTM,
                        FtpMockControlSocket::PRE_RETR,
                        "500 failed mdtm\r\n",
                        OK);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailPasv2) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpMockControlSocket::PRE_PASV2,
                        FtpMockControlSocket::PRE_QUIT,
                        "500 failed pasv\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailCwd) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpMockControlSocket::PRE_CWD,
                        FtpMockControlSocket::PRE_QUIT,
                        "500 failed cwd\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DirectoryTransactionFailList) {
  FtpMockControlSocketDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpMockControlSocket::PRE_LIST,
                        FtpMockControlSocket::PRE_QUIT,
                        "500 failed list\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailUser) {
  FtpMockControlSocketFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpMockControlSocket::PRE_USER,
                        FtpMockControlSocket::PRE_QUIT,
                        "500 no such user\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailPass) {
  FtpMockControlSocketFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpMockControlSocket::PRE_PASSWD,
                        FtpMockControlSocket::PRE_QUIT,
                        "530 Login authentication failed\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailSyst) {
  FtpMockControlSocketFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpMockControlSocket::PRE_SYST,
                        FtpMockControlSocket::PRE_PWD,
                        "500 failed syst\r\n",
                        OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailPwd) {
  FtpMockControlSocketFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpMockControlSocket::PRE_PWD,
                        FtpMockControlSocket::PRE_QUIT,
                        "500 failed pwd\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailType) {
  FtpMockControlSocketFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpMockControlSocket::PRE_TYPE,
                        FtpMockControlSocket::PRE_QUIT,
                        "500 failed type\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailPasv) {
  FtpMockControlSocketFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpMockControlSocket::PRE_PASV,
                        FtpMockControlSocket::PRE_QUIT,
                        "500 failed pasv\r\n",
                        ERR_FAILED);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailMdtm) {
  FtpMockControlSocketFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpMockControlSocket::PRE_MDTM,
                        FtpMockControlSocket::PRE_RETR,
                        "500 failed mdtm\r\n",
                        OK);
}

TEST_F(FtpNetworkTransactionTest, DownloadTransactionFailRetr) {
  FtpMockControlSocketFileDownloadRetrFail ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpMockControlSocket::PRE_RETR,
                        FtpMockControlSocket::PRE_PASV2,
                        "500 failed retr\r\n",
                        ERR_FAILED);
}

}  // namespace net

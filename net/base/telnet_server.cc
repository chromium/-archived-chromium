// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
// winsock2.h must be included first in order to ensure it is included before
// windows.h.
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <errno.h>
#include <sys/socket.h>
#include "base/message_loop.h"
#include "net/base/net_errors.h"
#include "third_party/libevent/event.h"
#include "base/message_pump_libevent.h"
#endif

#include "base/eintr_wrapper.h"
#include "net/base/telnet_server.h"

#if defined(OS_POSIX)
// Used same name as in Windows to avoid #ifdef where refrenced
#define SOCKET int
const int INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;
struct event;  // From libevent
#endif

const int kReadBufSize = 200;

// Telnet protocol constants.
class TelnetProtocol {
 public:
  // Telnet command definitions (from arpa/telnet.h).
  enum Commands {
    IAC   = 255,  // Interpret as command.
    DONT  = 254,  // You are not to use option.
    DO    = 253,  // Please, you use option.
    WONT  = 252,  // I won't use option.
    WILL  = 251,  // I will use option.
    SB    = 250,  // Interpret as subnegotiation.
    GA    = 249,  // You may reverse the line.
    EL    = 248,  // Erase the current line.
    EC    = 247,  // Erase the current character.
    AYT   = 246,  // Are you there.
    AO    = 245,  // Abort output--but let prog finish.
    IP    = 244,  // Interrupt process--permanently.
    BREAK = 243,  // Break.
    DM    = 242,  // Data mark--for connect. cleaning.
    NOP   = 241,  // Nop.
    SE    = 240,  // End sub negotiation.
    EOR   = 239,  // End of record (transparent mode).
    ABORT = 238,  // Abort process.
    SUSP  = 237,  // Suspend process.
    XEOF  = 236   // End of file: EOF is already used...
  };

  // Telnet options (from arpa/telnet.h).
  enum Options {
    BINARY =  0,  // 8-bit data path.
    ECHO   =  1,  // Echo.
    SGA    =  3,  // Suppress go ahead.
    NAWS   = 31,  // Window size.
    LFLOW  = 33  // Remote flow control.
  };

  // Fixed character definitions mentioned in RFC 854.
  enum Characters {
    NUL  = 0x00,
    LF   = 0x0A,
    CR   = 0x0D,
    BELL = 0x07,
    BS   = 0x08,
    HT   = 0x09,
    VT   = 0x0B,
    FF   = 0x0C,
    DEL  = 0x7F,
    ESC  = 0x1B
  };
};


///////////////////////

// must run in the IO thread
TelnetServer::TelnetServer(SOCKET s, ListenSocketDelegate* del)
    : ListenSocket(s, del) {
  input_state_ = NOT_IN_IAC_OR_ESC_SEQUENCE;
}

// must run in the IO thread
TelnetServer::~TelnetServer() {
}

void TelnetServer::SendIAC(int command, int option) {
  char data[3];
  data[0] = static_cast<unsigned char>(TelnetProtocol::IAC);
  data[1] = static_cast<unsigned char>(command);
  data[2] = option;
  Send(data, 3);
}

// always fixup \n to \r\n
void TelnetServer::SendInternal(const char* data, int len) {
  int begin_index = 0;
  for (int i = 0; i < len; i++) {
    if (data[i] == TelnetProtocol::LF) {
      // Send CR before LF if missing before.
      if (i == 0 || data[i - 1] != TelnetProtocol::CR) {
        // Send til before LF.
        ListenSocket::SendInternal(data + begin_index, i - begin_index);
        // Send CRLF.
        ListenSocket::SendInternal("\r\n", 2);
        // Continue after LF.
        begin_index = i + 1;
      }
    }
  }
  // Send what is left (the whole string is sent here if CRLF was ok)
  ListenSocket::SendInternal(data + begin_index, len - begin_index);
}

void TelnetServer::Accept() {
  SOCKET conn = ListenSocket::Accept(socket_);
  if (conn == INVALID_SOCKET) {
    // TODO
  } else {
    scoped_refptr<TelnetServer> sock =
        new TelnetServer(conn, socket_delegate_);
#if defined(OS_POSIX)
    sock->WatchSocket(WAITING_READ);
#endif
    // Setup the way we want to communicate
    sock->SendIAC(TelnetProtocol::DO, TelnetProtocol::ECHO);
    sock->SendIAC(TelnetProtocol::DO, TelnetProtocol::NAWS);
    sock->SendIAC(TelnetProtocol::DO, TelnetProtocol::LFLOW);
    sock->SendIAC(TelnetProtocol::WILL, TelnetProtocol::ECHO);
    sock->SendIAC(TelnetProtocol::WILL, TelnetProtocol::SGA);

    // it's up to the delegate to AddRef if it wants to keep it around
    socket_delegate_->DidAccept(this, sock);
  }
}

TelnetServer* TelnetServer::Listen(std::string ip, int port,
                                   ListenSocketDelegate *del) {
  SOCKET s = ListenSocket::Listen(ip, port);
  if (s == INVALID_SOCKET) {
    // TODO (ibrar): error handling
  } else {
    TelnetServer *serv = new TelnetServer(s, del);
    serv->Listen();
    return serv;
  }
  return NULL;
}

void TelnetServer::StateMachineStep(unsigned char c) {
  switch (input_state_) {
    case NOT_IN_IAC_OR_ESC_SEQUENCE:
      if (c == TelnetProtocol::IAC) {
        // Expect IAC command
        input_state_ = EXPECTING_COMMAND;
      } else if (c == TelnetProtocol::ESC) {
        // Expect left suare bracket
        input_state_ = EXPECTING_FIRST_ESC_CHARACTER;
      } else {
        char data[1];
        data[0] = c;
        // handle backspace specially
        if (c == TelnetProtocol::DEL) {
          if (!command_line_.empty()) {
            command_line_.erase(--command_line_.end());
            Send(data, 1);
          }
        } else {
          // Collect command
          if (c >= ' ')
            command_line_ += c;
          // Echo character to client (for now ignore control characters).
          if (c >= ' ' || c == TelnetProtocol::CR) {
            Send(data, 1);
          }
          // Check for line termination
          if (c == TelnetProtocol::CR)
            input_state_ = EXPECTING_NEW_LINE;
        }
      }
      break;
    case EXPECTING_NEW_LINE:
      if (c == TelnetProtocol::LF) {
        Send("\n", 1);
        socket_delegate_->DidRead(this, command_line_);
        command_line_ = "";
      }
      input_state_ = NOT_IN_IAC_OR_ESC_SEQUENCE;
      break;
    case EXPECTING_COMMAND:
      // Read command, expect option.
      iac_command_ = c;
      input_state_ = EXPECTING_OPTION;
      break;
    case EXPECTING_OPTION:
      // Read option
      iac_option_ = c;
      // check for subnegoating if not done reading IAC.
      if (iac_command_ != TelnetProtocol::SB) {
        input_state_ = NOT_IN_IAC_OR_ESC_SEQUENCE;
      } else {
        input_state_ = SUBNEGOTIATION_EXPECTING_IAC;
      }
      break;
    case SUBNEGOTIATION_EXPECTING_IAC:
      // Currently ignore content of subnegotiation.
      if (c == TelnetProtocol::IAC)
        input_state_ = SUBNEGOTIATION_EXPECTING_SE;
      break;
    case SUBNEGOTIATION_EXPECTING_SE:
      // Character must be SE and subnegotiation is finished.
      input_state_ = NOT_IN_IAC_OR_ESC_SEQUENCE;
      break;
    case EXPECTING_FIRST_ESC_CHARACTER:
      if (c == '[') {
        // Expect ESC sequence content.
        input_state_ = EXPECTING_NUMBER_SEMICOLON_OR_END;
      } else if (c == 'O') {
        // VT100 "ESC O" sequence.
        input_state_ = EXPECTING_SECOND_ESC_CHARACTER;
      } else {
        // Unknown ESC sequence - ignore.
      }
      break;
    case EXPECTING_SECOND_ESC_CHARACTER:
      // Ignore ESC sequence content for now.
      input_state_ = NOT_IN_IAC_OR_ESC_SEQUENCE;
      break;
    case EXPECTING_NUMBER_SEMICOLON_OR_END:
      if (isdigit(c) || c ==';') {
        // Ignore ESC sequence content for now.
      } else {
        // Final character in ESC sequence.
        input_state_ = NOT_IN_IAC_OR_ESC_SEQUENCE;
      }
      break;
  }
}

void TelnetServer::Read() {
  char buf[kReadBufSize + 1];
  int len;
  do {
    len = HANDLE_EINTR(recv(socket_, buf, kReadBufSize, 0));

#if defined(OS_WIN)
    if (len == SOCKET_ERROR) {
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK)
        break;
#else
    if (len == SOCKET_ERROR) {
      if (errno == EWOULDBLOCK || errno == EAGAIN)
        break;
#endif
    } else if (len == 0) {
      Close();
    } else {
      const char *data = buf;
      for (int i = 0; i < len; ++i) {
        unsigned char c = static_cast<unsigned char>(*data);
        StateMachineStep(c);
        data++;
      }
    }
  } while (len == kReadBufSize);
}

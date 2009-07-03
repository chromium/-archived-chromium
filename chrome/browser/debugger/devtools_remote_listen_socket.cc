// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_remote_listen_socket.h"

#include "build/build_config.h"

#include <stdlib.h>

#if defined(OS_WIN)
// winsock2.h must be included first in order to ensure it is included before
// windows.h.
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <errno.h>
#include <sys/socket.h>
#include "base/message_loop.h"
#include "base/message_pump_libevent.h"
#include "net/base/net_errors.h"
#include "third_party/libevent/event.h"
#endif

#include "base/eintr_wrapper.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "chrome/browser/debugger/devtools_remote.h"
#include "chrome/browser/debugger/devtools_remote_message.h"

#define CONSUME_BUFFER_CHAR \
  pBuf++;\
  len--

#if defined(OS_POSIX)
// Used same name as in Windows to avoid #ifdef where refrenced
#define SOCKET int
const int INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;
struct event;  // From libevent
#endif

const int kReadBufSize = 200;

DevToolsRemoteListenSocket::DevToolsRemoteListenSocket(
    SOCKET s,
    ListenSocketDelegate* del,
    DevToolsRemoteListener* message_listener)
        : ListenSocket(s, del),
          state_(HANDSHAKE),
          message_listener_(message_listener),
          cr_received_(false) {}

void DevToolsRemoteListenSocket::StartNextField() {
  switch (state_) {
    case INVALID:
      state_ = HANDSHAKE;
      break;
    case HANDSHAKE:
      state_ = HEADERS;
      break;
    case HEADERS:
      if (protocol_field_.size() == 0) {  // empty line - end of headers
        const std::string& payload_length_string = GetHeader(
            DevToolsRemoteMessageHeaders::kContentLength, "0");
        remaining_payload_length_ = StringToInt(payload_length_string);
        state_ = PAYLOAD;
        if (remaining_payload_length_ == 0) {  // no payload
          DispatchField();
          return;
        }
      }
      break;
    case PAYLOAD:
      header_map_.clear();
      payload_.clear();
      state_ = HEADERS;
      break;
    default:
      NOTREACHED();
      break;
  }
  protocol_field_.clear();
}

DevToolsRemoteListenSocket::~DevToolsRemoteListenSocket() {}

DevToolsRemoteListenSocket*
    DevToolsRemoteListenSocket::Listen(const std::string& ip,
                                       int port,
                                       ListenSocketDelegate* del,
                                       DevToolsRemoteListener* listener) {
  SOCKET s = ListenSocket::Listen(ip, port);
  if (s == INVALID_SOCKET) {
    // TODO(apavlov): error handling
  } else {
    DevToolsRemoteListenSocket* sock =
        new DevToolsRemoteListenSocket(s, del, listener);
    sock->Listen();
    return sock;
  }
  return NULL;
}

void DevToolsRemoteListenSocket::Read() {
  char buf[kReadBufSize];
  int len;
  do {
    len = HANDLE_EINTR(recv(socket_, buf, kReadBufSize, 0));
    if (len == SOCKET_ERROR) {
#if defined(OS_WIN)
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK) {
#elif defined(OS_POSIX)
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
#endif
        break;
      } else {
        // TODO(apavlov): some error handling required here
        break;
      }
    } else if (len == 0) {
      // In Windows, Close() is called by OnObjectSignaled.  In POSIX, we need
      // to call it here.
#if defined(OS_POSIX)
      Close();
#endif
    } else {
      // TODO(apavlov): maybe change DidRead to take a length instead
      DCHECK(len > 0 && len <= kReadBufSize);
      this->DispatchRead(buf, len);
    }
  } while (len == kReadBufSize);
}

// Dispatches data from socket to socket_delegate_, extracting messages
// delimited by newlines.
void DevToolsRemoteListenSocket::DispatchRead(char* buf, int len) {
  char* pBuf = buf;
  while (len > 0) {
    if (state_ != PAYLOAD) {
      if (cr_received_ && *pBuf == '\n') {
        cr_received_ = false;
        CONSUME_BUFFER_CHAR;
      } else {
        while (*pBuf != '\r' && len > 0) {
          protocol_field_.push_back(*pBuf);
          CONSUME_BUFFER_CHAR;
        }
        if (*pBuf == '\r') {
          cr_received_ = true;
          CONSUME_BUFFER_CHAR;
        }
        continue;
      }
      switch (state_) {
        case HANDSHAKE:
        case HEADERS:
          DispatchField();
          break;
        default:
          NOTREACHED();
          break;
      }
    } else {  // PAYLOAD
      while (remaining_payload_length_ > 0 && len > 0) {
        protocol_field_.push_back(*pBuf);
        CONSUME_BUFFER_CHAR;
        remaining_payload_length_--;
      }
      if (remaining_payload_length_ == 0) {
        DispatchField();
      }
    }
  }
}

void DevToolsRemoteListenSocket::DispatchField() {
  static const std::string kHandshakeString = "ChromeDevToolsHandshake";
  switch (state_) {
    case HANDSHAKE:
      if (protocol_field_.compare(kHandshakeString)) {
        state_ = INVALID;
      } else {
        Send(kHandshakeString, true);
      }
      break;
    case HEADERS: {
      if (protocol_field_.size() > 0) {  // not end-of-headers
        std::string::size_type colon_pos = protocol_field_.find_first_of(":");
        if (colon_pos == std::string::npos) {
          // TODO(apavlov): handle the error (malformed header)
        } else {
          const std::string header_name = protocol_field_.substr(0, colon_pos);
          std::string header_val = protocol_field_.substr(colon_pos + 1);
          header_map_[header_name] = header_val;
        }
      }
      break;
    }
    case PAYLOAD:
      payload_ = protocol_field_;
      HandleMessage();
      break;
    default:
      NOTREACHED();
      break;
  }
  StartNextField();
}

const std::string& DevToolsRemoteListenSocket::GetHeader(
    const std::string& header_name,
    const std::string& default_value) const {
  DevToolsRemoteMessage::HeaderMap::const_iterator it =
      header_map_.find(header_name);
  if (it == header_map_.end()) {
    return default_value;
  }
  return it->second;
}

// Handle header_map_ and payload_
void DevToolsRemoteListenSocket::HandleMessage() {
  if (message_listener_ != NULL) {
    DevToolsRemoteMessage message(header_map_, payload_);
    message_listener_->HandleMessage(message);
  }
}

void DevToolsRemoteListenSocket::Accept() {
  SOCKET conn = ListenSocket::Accept(socket_);
  if (conn != INVALID_SOCKET) {
    scoped_refptr<DevToolsRemoteListenSocket> sock =
        new DevToolsRemoteListenSocket(conn,
                                       socket_delegate_,
                                       message_listener_);
    // it's up to the delegate to AddRef if it wants to keep it around
#if defined(OS_POSIX)
    sock->WatchSocket(WAITING_READ);
#endif
    socket_delegate_->DidAccept(this, sock);
  } else {
    // TODO(apavlov): some error handling required here
  }
}

void DevToolsRemoteListenSocket::SendInternal(const char* bytes, int len) {
  char* send_buf = const_cast<char *>(bytes);
  int len_left = len;
  while (true) {
    int sent = HANDLE_EINTR(send(socket_, send_buf, len_left, 0));
    if (sent == len_left) {  // A shortcut to avoid extraneous checks.
      break;
    }
    if (sent == kSocketError) {
#if defined(OS_WIN)
      if (WSAGetLastError() != WSAEWOULDBLOCK) {
        LOG(ERROR) << "send failed: WSAGetLastError()==" << WSAGetLastError();
#elif defined(OS_POSIX)
      if (errno != EWOULDBLOCK && errno != EAGAIN) {
        LOG(ERROR) << "send failed: errno==" << errno;
#endif
        break;
      }
      // Otherwise we would block, and now we have to wait for a retry.
      // Fall through to PlatformThread::YieldCurrentThread()
    } else {
      // sent != len_left according to the shortcut above.
      // Shift the buffer start and send the remainder after a short while.
      send_buf += sent;
      len_left -= sent;
    }
    PlatformThread::YieldCurrentThread();
  }
}

void DevToolsRemoteListenSocket::Close() {
  ListenSocket::Close();
}

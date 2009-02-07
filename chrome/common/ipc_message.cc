// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_message.h"

#include "base/logging.h"
#include "build/build_config.h"

namespace IPC {

//------------------------------------------------------------------------------

Message::~Message() {
}

Message::Message()
    : Pickle(sizeof(Header)) {
  header()->routing = header()->type = header()->flags = 0;
#if defined(OS_POSIX)
  header()->num_fds = 0;
#endif
  InitLoggingVariables();
}

Message::Message(int32 routing_id, uint16 type, PriorityValue priority)
    : Pickle(sizeof(Header)) {
  header()->routing = routing_id;
  header()->type = type;
  header()->flags = priority;
#if defined(OS_POSIX)
  header()->num_fds = 0;
#endif
  InitLoggingVariables();
}

Message::Message(const char* data, int data_len) : Pickle(data, data_len) {
  InitLoggingVariables();
}

Message::Message(const Message& other) : Pickle(other) {
  InitLoggingVariables();
}

void Message::InitLoggingVariables() {
#ifdef IPC_MESSAGE_LOG_ENABLED
  received_time_ = 0;
  dont_log_ = false;
  log_data_ = NULL;
#endif
}

Message& Message::operator=(const Message& other) {
  *static_cast<Pickle*>(this) = other;
  return *this;
}

#ifdef IPC_MESSAGE_LOG_ENABLED
void Message::set_sent_time(int64 time) {
  DCHECK((header()->flags & HAS_SENT_TIME_BIT) == 0);
  header()->flags |= HAS_SENT_TIME_BIT;
  WriteInt64(time);
}

int64 Message::sent_time() const {
  if ((header()->flags & HAS_SENT_TIME_BIT) == 0)
    return 0;

  const char* data = end_of_payload();
  data -= sizeof(int64);
  return *(reinterpret_cast<const int64*>(data));
}

void Message::set_received_time(int64 time) const {
  received_time_ = time;
}
#endif

}  // namespace IPC


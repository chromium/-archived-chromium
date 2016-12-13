// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_MESSAGE_H__
#define CHROME_COMMON_IPC_MESSAGE_H__

#include <string>

#include "base/basictypes.h"
#include "base/pickle.h"

#ifndef NDEBUG
#define IPC_MESSAGE_LOG_ENABLED
#endif

#if defined(OS_POSIX)
#include "base/ref_counted.h"
#endif

namespace base {
class FileDescriptor;
}

class FileDescriptorSet;

namespace IPC {

//------------------------------------------------------------------------------

class Channel;
class Message;
struct LogData;

class Message : public Pickle {
 public:
  // Implemented by objects that can send IPC messages across a channel.
  class Sender {
   public:
    virtual ~Sender() {}

    // Sends the given IPC message.  The implementor takes ownership of the
    // given Message regardless of whether or not this method succeeds.  This
    // is done to make this method easier to use.  Returns true on success and
    // false otherwise.
    virtual bool Send(Message* msg) = 0;
  };

  enum PriorityValue {
    PRIORITY_LOW = 1,
    PRIORITY_NORMAL,
    PRIORITY_HIGH
  };

  virtual ~Message();

  Message();

  // Initialize a message with a user-defined type, priority value, and
  // destination WebView ID.
  Message(int32 routing_id, uint16 type, PriorityValue priority);

  // Initializes a message from a const block of data.  The data is not copied;
  // instead the data is merely referenced by this message.  Only const methods
  // should be used on the message when initialized this way.
  Message(const char* data, int data_len);

  Message(const Message& other);
  Message& operator=(const Message& other);

  PriorityValue priority() const {
    return static_cast<PriorityValue>(header()->flags & PRIORITY_MASK);
  }

  // True if this is a synchronous message.
  bool is_sync() const {
    return (header()->flags & SYNC_BIT) != 0;
  }

  // Set this on a reply to a synchronous message.
  void set_reply() {
    header()->flags |= REPLY_BIT;
  }

  bool is_reply() const {
    return (header()->flags & REPLY_BIT) != 0;
  }

  // Set this on a reply to a synchronous message to indicate that no receiver
  // was found.
  void set_reply_error() {
    header()->flags |= REPLY_ERROR_BIT;
  }

  bool is_reply_error() const {
    return (header()->flags & REPLY_ERROR_BIT) != 0;
  }

  // Normally when a receiver gets a message and they're blocked on a
  // synchronous message Send, they buffer a message.  Setting this flag causes
  // the receiver to be unblocked and the message to be dispatched immediately.
  void set_unblock(bool unblock) {
    if (unblock) {
      header()->flags |= UNBLOCK_BIT;
    } else {
      header()->flags &= ~UNBLOCK_BIT;
    }
  }

  bool should_unblock() const {
    return (header()->flags & UNBLOCK_BIT) != 0;
  }

  // Tells the receiver that the caller is pumping messages while waiting
  // for the result.
  bool is_caller_pumping_messages() const {
    return (header()->flags & PUMPING_MSGS_BIT) != 0;
  }

  uint16 type() const {
    return header()->type;
  }

  int32 routing_id() const {
    return header()->routing;
  }

  void set_routing_id(int32 new_id) {
    header()->routing = new_id;
  }

  template<class T>
  static bool Dispatch(const Message* msg, T* obj, void (T::*func)()) {
    (obj->*func)();
    return true;
  }

  template<class T>
  static bool Dispatch(const Message* msg, T* obj, void (T::*func)() const) {
    (obj->*func)();
    return true;
  }

  template<class T>
  static bool Dispatch(const Message* msg, T* obj,
                       void (T::*func)(const Message&)) {
    (obj->*func)(*msg);
    return true;
  }

  template<class T>
  static bool Dispatch(const Message* msg, T* obj,
                       void (T::*func)(const Message&) const) {
    (obj->*func)(*msg);
    return true;
  }

  // Used for async messages with no parameters.
  static void Log(const Message* msg, std::wstring* l) {
  }

  // Find the end of the message data that starts at range_start.  Returns NULL
  // if the entire message is not found in the given data range.
  static const char* FindNext(const char* range_start, const char* range_end) {
    return Pickle::FindNext(sizeof(Header), range_start, range_end);
  }

#if defined(OS_POSIX)
  // On POSIX, a message supports reading / writing FileDescriptor objects.
  // This is used to pass a file descriptor to the peer of an IPC channel.

  // Add a descriptor to the end of the set. Returns false iff the set is full.
  bool WriteFileDescriptor(const base::FileDescriptor& descriptor);
  // Get a file descriptor from the message. Returns false on error.
  //   iter: a Pickle iterator to the current location in the message.
  bool ReadFileDescriptor(void** iter, base::FileDescriptor* descriptor) const;
#endif

#ifdef IPC_MESSAGE_LOG_ENABLED
  // Adds the outgoing time from Time::Now() at the end of the message and sets
  // a bit to indicate that it's been added.
  void set_sent_time(int64 time);
  int64 sent_time() const;

  void set_received_time(int64 time) const;
  int64 received_time() const { return received_time_; }
  void set_output_params(const std::wstring& op) const { output_params_ = op; }
  const std::wstring& output_params() const { return output_params_; }
  // The following four functions are needed so we can log sync messages with
  // delayed replies.  We stick the log data from the sent message into the
  // reply message, so that when it's sent and we have the output parameters
  // we can log it.  As such, we set a flag on the sent message to not log it.
  void set_sync_log_data(LogData* data) const { log_data_ = data; }
  LogData* sync_log_data() const { return log_data_; }
  void set_dont_log() const { dont_log_ = true; }
  bool dont_log() const { return dont_log_; }
#endif

 protected:
  friend class Channel;
  friend class MessageReplyDeserializer;
  friend class SyncMessage;

  void set_sync() {
    header()->flags |= SYNC_BIT;
  }

  // flags
  enum {
    PRIORITY_MASK   = 0x0003,
    SYNC_BIT        = 0x0004,
    REPLY_BIT       = 0x0008,
    REPLY_ERROR_BIT = 0x0010,
    UNBLOCK_BIT     = 0x0020,
    PUMPING_MSGS_BIT= 0x0040,
    HAS_SENT_TIME_BIT = 0x0080,
  };

#pragma pack(push, 2)
  struct Header : Pickle::Header {
    int32 routing;  // ID of the view that this message is destined for
    uint16 type;    // specifies the user-defined message type
    uint16 flags;   // specifies control flags for the message
#if defined(OS_POSIX)
    uint32 num_fds; // the number of descriptors included with this message
#endif
  };
#pragma pack(pop)

  Header* header() {
    return headerT<Header>();
  }
  const Header* header() const {
    return headerT<Header>();
  }

  void InitLoggingVariables();

#if defined(OS_POSIX)
  // The set of file descriptors associated with this message.
  scoped_refptr<FileDescriptorSet> file_descriptor_set_;

  // Ensure that a FileDescriptorSet is allocated
  void EnsureFileDescriptorSet();

  FileDescriptorSet* file_descriptor_set() {
    EnsureFileDescriptorSet();
    return file_descriptor_set_.get();
  }
  const FileDescriptorSet* file_descriptor_set() const {
    return file_descriptor_set_.get();
  }
#endif

#ifdef IPC_MESSAGE_LOG_ENABLED
  // Used for logging.
  mutable int64 received_time_;
  mutable std::wstring output_params_;
  mutable LogData* log_data_;
  mutable bool dont_log_;
#endif
};

//------------------------------------------------------------------------------

}  // namespace IPC

enum SpecialRoutingIDs {
  // indicates that we don't have a routing ID yet.
  MSG_ROUTING_NONE = -2,

  // indicates a general message not sent to a particular tab.
  MSG_ROUTING_CONTROL = kint32max,
};

#define IPC_REPLY_ID 0xFFF0  // Special message id for replies
#define IPC_LOGGING_ID 0xFFF1  // Special message id for logging

#endif  // CHROME_COMMON_IPC_MESSAGE_H__

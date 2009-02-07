// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_FILE_DESCRIPTOR_POSIX_H_
#define CHROME_COMMON_FILE_DESCRIPTOR_POSIX_H_

#include <vector>

// -----------------------------------------------------------------------------
// A FileDescriptor is a structure for use in IPC messages. It allows one to
// send descriptors over an IPC channel.
//
// In the Windows world, processes can peek and poke the HANDLE table of other
// processes. On POSIX, in order to transmit descriptors we need to include
// them in a control-message (a side-channel on the UNIX domain socket).
// Serialising this type adds descriptors to a vector in the IPC Message, from
// which the IPC channel can package them up for the kernel.
// -----------------------------------------------------------------------------
struct FileDescriptor {
  FileDescriptor()
      : fd(-1),
        auto_close(false) { }

  int fd;
  // If true, close this descriptor after it has been sent.
  bool auto_close;
};

// -----------------------------------------------------------------------------
// A DescriptorSet is an ordered set of POSIX file descriptors. These are
// associated with IPC messages so that descriptors can be transmitted over a
// UNIX domain socket.
// -----------------------------------------------------------------------------
class DescriptorSet {
 public:
  DescriptorSet();
  ~DescriptorSet();

  // This is the maximum number of descriptors per message. We need to know this
  // because the control message kernel interface has to be given a buffer which
  // is large enough to store all the descriptor numbers. Otherwise the kernel
  // tells us that it truncated the control data and the extra descriptors are
  // lost.
  //
  // In debugging mode, it's a fatal error to try and add more than this number
  // of descriptors to a DescriptorSet.
  enum {
    MAX_DESCRIPTORS_PER_MESSAGE = 4,
  };

  // ---------------------------------------------------------------------------
  // Interfaces for building during message serialisation...

  // Add a descriptor to the end of the set
  void Add(int fd);
  // Add a descriptor to the end of the set and automatically close it after
  // transmission.
  void AddAndAutoClose(int fd);

  // ---------------------------------------------------------------------------


  // ---------------------------------------------------------------------------
  // Interfaces for accessing during message deserialisation...

  // Return the number of descriptors remaining
  unsigned size() const { return descriptors_.size() - next_descriptor_; }
  // Return true if no unconsumed descriptors remain
  bool empty() const { return descriptors_.size() == next_descriptor_; }
  // Fetch the next descriptor from the beginning of the set. This interface is
  // designed for the deserialising code as it doesn't support close flags.
  //   returns: file descriptor, or -1 on error
  int NextDescriptor();

  // ---------------------------------------------------------------------------


  // ---------------------------------------------------------------------------
  // Interfaces for transmission...

  // Fill an array with file descriptors without 'consuming' them. CommitAll
  // must be called after these descriptors have been transmitted.
  //   buffer: (output) a buffer of, at least, size() integers.
  void GetDescriptors(int* buffer) const;
  // This must be called after transmitting the descriptors returned by
  // GetDescriptors. It marks all the descriptors as consumed and closes those
  // which are auto-close.
  void CommitAll();

  // ---------------------------------------------------------------------------


  // ---------------------------------------------------------------------------
  // Interfaces for receiving...

  // Set the contents of the set from the given buffer. This set must be empty
  // before calling. The auto-close flag is set on all the descriptors so that
  // unconsumed descriptors are closed on destruction.
  void SetDescriptors(const int* buffer, unsigned count);

  // ---------------------------------------------------------------------------

 private:
  // A vector of descriptors and close flags. If this message is sent, then
  // these descriptors are sent as control data. After sending, any descriptors
  // with a true flag are closed. If this message has been received, then these
  // are the descriptors which were received and all close flags are true.
  std::vector<FileDescriptor> descriptors_;
  // When deserialising the message, the descriptors will be extracted
  // one-by-one. This contains the index of the next unused descriptor.
  unsigned next_descriptor_;
};

#endif  // CHROME_COMMON_FILE_DESCRIPTOR_POSIX_H_

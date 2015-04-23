// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_TRANSPORT_DIB_H_
#define CHROME_COMMON_TRANSPORT_DIB_H_

#include "base/basictypes.h"

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "base/shared_memory.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_LINUX)
#include "chrome/common/x11_util.h"
#endif

namespace gfx {
class Size;
}
namespace skia {
class PlatformCanvas;
}

// -----------------------------------------------------------------------------
// A TransportDIB is a block of memory that is used to transport pixels
// between processes: from the renderer process to the browser, and
// between renderer and plugin processes.
// -----------------------------------------------------------------------------
class TransportDIB {
 public:
  ~TransportDIB();

  // Two typedefs are defined. A Handle is the type which can be sent over
  // the wire so that the remote side can map the transport DIB. The Id typedef
  // is sufficient to identify the transport DIB when you know that the remote
  // side already may have it mapped.
#if defined(OS_WIN)
  typedef HANDLE Handle;
  // On Windows, the Id type includes a sequence number (epoch) to solve an ABA
  // issue:
  //   1) Process A creates a transport DIB with HANDLE=1 and sends to B.
  //   2) Process B maps the transport DIB and caches 1 -> DIB.
  //   3) Process A closes the transport DIB and creates a new one. The new DIB
  //      is also assigned HANDLE=1.
  //   4) Process A sends the Handle to B, but B incorrectly believes that it
  //      already has it cached.
  struct HandleAndSequenceNum {
    HandleAndSequenceNum()
        : handle(NULL),
          sequence_num(0) {
    }

    HandleAndSequenceNum(HANDLE h, uint32 seq_num)
        : handle(h),
          sequence_num(seq_num) {
    }

    bool operator< (const HandleAndSequenceNum& other) const {
      // Use the lexicographic order on the tuple <handle, sequence_num>.
      if (other.handle != handle)
        return other.handle < handle;
      return other.sequence_num < sequence_num;
    }

    HANDLE handle;
    uint32 sequence_num;
  };
  typedef HandleAndSequenceNum Id;
#elif defined(OS_MACOSX)
  typedef base::SharedMemoryHandle Handle;
  // On Mac, the inode number of the backing file is used as an id.
  typedef base::SharedMemoryId Id;
#elif defined(OS_LINUX)
  typedef int Handle;  // These two ints are SysV IPC shared memory keys
  typedef int Id;
#endif

  // Create a new TransportDIB
  //   size: the minimum size, in bytes
  //   epoch: Windows only: a global counter. See comment above.
  //   returns: NULL on failure
  static TransportDIB* Create(size_t size, uint32 sequence_num);

  // Map the referenced transport DIB. Returns NULL on failure.
  static TransportDIB* Map(Handle transport_dib);

  // Returns a canvas using the memory of this TransportDIB. The returned
  // pointer will be owned by the caller. The bitmap will be of the given size,
  // which should fit inside this memory.
  skia::PlatformCanvas* GetPlatformCanvas(int w, int h);

  // Return a pointer to the shared memory
  void* memory() const;

  // Return the maximum size of the shared memory. This is not the amount of
  // data which is valid, you have to know that via other means, this is simply
  // the maximum amount that /could/ be valid.
  size_t size() const { return size_; }

  // Return the identifier which can be used to refer to this shared memory
  // on the wire.
  Id id() const;

  // Return a handle to the underlying shared memory. This can be sent over the
  // wire to give this transport DIB to another process.
  Handle handle() const;

#if defined(OS_LINUX)
  // Map the shared memory into the X server and return an id for the shared
  // segment.
  XID MapToX(Display* connection);
#endif

 private:
  TransportDIB();
#if defined(OS_WIN) || defined(OS_MACOSX)
  explicit TransportDIB(base::SharedMemoryHandle dib);
  base::SharedMemory shared_memory_;
  uint32 sequence_num_;
#elif defined(OS_LINUX)
  int key_;  // SysV shared memory id
  void* address_;  // mapped address
  XID x_shm_;  // X id for the shared segment
  Display* display_;  // connection to the X server
#endif
  size_t size_;  // length, in bytes
};

class MessageLoop;

#endif  // CHROME_COMMON_TRANSPORT_DIB_H_

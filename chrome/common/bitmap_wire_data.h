// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_BITMAP_WIRE_DATA_H_
#define CHROME_COMMON_BITMAP_WIRE_DATA_H_

#if defined(OS_POSIX)
class SkBitmap;
#endif

// BitmapWireData is the type of the bitmap data which is carried from renderer
// to browser over the wire.

#if defined(OS_WIN)

// On Windows, the bitmap data is carried out-of-band in a shared memory
// segment. This is the handle to the shared memory. These handles are valid
// only in the context of the renderer process.
// TODO(agl): get a clarification on that last sentence. It doesn't make any
// sense to me
typedef HANDLE BitmapWireData;

#elif defined(OS_POSIX)

// On POSIX, we currently serialise the bitmap data over the wire. This will
// change at some point when we too start using shared memory, but we wish to
// use shared memory in a different way so this is a temporary work-around.
// TODO(port): implement drawing with shared backing stores and replace this
//   with an IPC no-op type.
typedef SkBitmap BitmapWireData;

#endif  // defined(OS_WIN)

#endif  // CHROME_COMMON_BITMAP_WIRE_DATA_H_

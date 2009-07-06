// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_NATIVE_METAFILE_H__
#define PRINTING_NATIVE_METAFILE_H__

// Define a metafile format for the current platform.  We use this platform
// independent define so we can define interfaces in platform agnostic manner.
// It is still an outstanding design issue whether we create classes on all
// platforms that have the same interface as Emf or if we change Emf to support
// multiple platforms (and rename to NativeMetafile).


#if defined(OS_WIN)

#include "printing/emf_win.h"

namespace printing {

typedef Emf NativeMetafile;

}  // namespace printing

#elif defined(OS_MACOSX)

// TODO(port): Printing using PDF?

#elif defined(OS_LINUX)

// TODO(port): Printing using PostScript?

#endif


#endif  // PRINTING_NATIVE_METAFILE_H__

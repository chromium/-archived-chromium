// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// In most .h files, we would rather include a declaration of an stl
// rather than including the appropriate stl h file (which brings in
// lots of crap).  For many STL classes this is ok (eg pair), but for
// some it's really annoying.  We define those here, so you can
// just include this file instead of having to deal with the annoyance.
//
// Most of the annoyance, btw, has to do with the default allocator.
//
// DEPRECATED: this file is deprecated.  Do not use in new code.
// Be careful about removing from old code, though, because your
// header file might be included by higher-level code that is
// accidentally depending on this.
// -- mec 2007-01-17

#ifndef BASE_STL_DECL_H_
#define BASE_STL_DECL_H_

#include "third_party/cld/base/stl_decl_msvc.h"

#endif   // BASE_STL_DECL_H_

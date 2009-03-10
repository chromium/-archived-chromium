// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// This header file is pre-compiled and added to the cl.exe command line for
// each source file.  You should not include it explicitly.  Instead, you
// should ensure that your source files specify all of their include files
// explicitly so they may be used without this pre-compiled header.
//
// To make use of this header file in a vcproj, just add precompiled.cc as a
// build target, and modify its build properties to enable /Yc for that file
// only.  Then add the precompiled.vsprops property sheet to your vcproj.
//

// windows headers
// TODO: when used, winsock2.h must be included before windows.h or
// the build blows up. The best fix is to define WIN32_LEAN_AND_MEAN.
// The best workaround is to define _WINSOCKAPI_. That's what winsock2.h does.
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tchar.h>

// runtime headers
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory.h>

// Usual STL
#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <strstream>
#include <vector>

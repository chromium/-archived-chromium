// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Header used to generate the precompiled file.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#include <winsock2.h>

#include <algorithm>
#include <hash_map>
#include <hash_set>
#include <list>
#include <set>
#include <string>
#include <vector>

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

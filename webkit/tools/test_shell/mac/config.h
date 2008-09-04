// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// defines to make some webkit headers happy outside of their normal environment
#define MAC_OS_X
#define PLATFORM_NAME "Mac OS X"
#define PLATFORM(WTF_FEATURE) (defined( WTF_PLATFORM_##WTF_FEATURE ) && WTF_PLATFORM_##WTF_FEATURE)
#define WTF_PLATFORM_MAC 1
#define WTF_PLATFORM_CHROMIUM 1

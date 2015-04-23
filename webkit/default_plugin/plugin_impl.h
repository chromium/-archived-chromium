// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include "webkit/default_plugin/plugin_impl_win.h"
#elif defined (OS_MACOSX)
#include "webkit/default_plugin/plugin_impl_mac.h"
#elif defined (OS_LINUX)
#include "webkit/default_plugin/plugin_impl_gtk.h"
#endif

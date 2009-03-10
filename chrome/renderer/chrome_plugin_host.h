// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_PLUGIN_HOST_H__
#define CHROME_RENDERER_CHROME_PLUGIN_HOST_H__

#include "chrome/common/chrome_plugin_api.h"

// Returns the table of browser functions for use from the renderer process.
CPBrowserFuncs* GetCPBrowserFuncsForRenderer();

#endif  // CHROME_RENDERER_CHROME_PLUGIN_HOST_H__

// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_CHROME_PLUGIN_HOST_H_
#define CHROME_PLUGIN_CHROME_PLUGIN_HOST_H_

#include "chrome/common/chrome_plugin_api.h"

// Returns the table of browser functions for use from the plugin process.
CPBrowserFuncs* GetCPBrowserFuncsForPlugin();

#endif  // CHROME_PLUGIN_CHROME_PLUGIN_HOST_H_

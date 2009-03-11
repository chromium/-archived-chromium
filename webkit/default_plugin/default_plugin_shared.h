// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Thes file contains stuff that should be shared among projects that do some
// special handling with default plugin

#ifndef WEBKIT_DEFAULT_PLUGIN_DEFAULT_PLUGIN_SHARED_H
#define WEBKIT_DEFAULT_PLUGIN_DEFAULT_PLUGIN_SHARED_H

namespace default_plugin {

// We use the NPNGetValue host function to send notification message to host.
// This corresponds to NPNVariable defined in npapi.h. However, we don't care
// too much about value conflicts because on the host side, it will only respond
// to such request coming from default plugin.
const int kMissingPluginStatusStart = 3000;

enum MissingPluginStatus {
  MISSING_PLUGIN_AVAILABLE,
  MISSING_PLUGIN_USER_STARTED_DOWNLOAD
};

}  // namespace activex_shim

#endif // #ifndef WEBKIT_DEFAULT_PLUGIN_DEFAULT_PLUGIN_SHARED_H

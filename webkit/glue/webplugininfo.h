// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBPLUGININFO_H_
#define WEBKIT_GLUE_WEBPLUGININFO_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"

// Describes a mime type entry for a plugin.
struct WebPluginMimeType {
  // The name of the mime type (e.g., "application/x-shockwave-flash").
  std::string mime_type;

  // A list of all the file extensions for this mime type.
  std::vector<std::string> file_extensions;

  // Description of the mime type.
  std::wstring description;
};

// Describes an available NPAPI plugin.
struct WebPluginInfo {
  // The name of the plugin (i.e. Flash).
  std::wstring name;

  // The path to the plugin file (DLL/bundle/library).
  FilePath path;

  // The version number of the plugin file (may be OS-specific)
  std::wstring version;

  // A description of the plugin that we get from its version info.
  std::wstring desc;

  // A list of all the mime types that this plugin supports.
  std::vector<WebPluginMimeType> mime_types;
};

#endif  // WEBKIT_GLUE_WEBPLUGININFO_H_

// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "PluginData.h"

// TODO(port):Temporarily stub out Plugin code for Mac port of Chromium so we
// can get things to compile.

namespace WebCore {

// Copied from PluginInfo.h, since including it clashes with PluginData.h.
// The collision isn't worth fixing since the workaround here is only temporary.
struct PluginInfo;
class PluginInfoStore {
 public:
  PluginInfo *createPluginInfoForPluginAtIndex(unsigned);
  unsigned pluginCount() const;
  static String pluginNameForMIMEType(const String& mimeType);
  static bool supportsMIMEType(const String& mimeType);
};

PluginInfo * PluginInfoStore::createPluginInfoForPluginAtIndex(unsigned) {
  ASSERT_NOT_REACHED();
  return NULL;
}

unsigned PluginInfoStore::pluginCount() const {
  return 0;
}

bool PluginInfoStore::supportsMIMEType(const WebCore::String& mimeType) {
  return false;
}

PluginData::PluginData(const Page* page) 
   : m_page(page) {
  ASSERT_NOT_REACHED();
}

PluginData::~PluginData() {
}

bool PluginData::supportsMimeType(const String& mimeType) const {
  return false;
}

String PluginData::pluginNameForMimeType(const String& mimeType) const {
  ASSERT_NOT_REACHED();
  return String();
}

// static
void PluginData::refresh() {
}

void refreshPlugins(bool reloadOpenPages) {
}
  
}  // namespace WebCore

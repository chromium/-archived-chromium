// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file stubs out some functions needed to make the linker happy
// without linking in all the plugin code.  It should be removed once
// we have plugins working on all platforms.

// TODO(port): remove this file.

#include "base/logging.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_stream.h"

namespace NPAPI {

PluginStream::~PluginStream() {
  NOTIMPLEMENTED();
}

bool PluginStream::Close(NPReason reason) {
  NOTIMPLEMENTED();
  return false;
}

void PluginInstance::NPP_StreamAsFile(NPStream*, const char*) {
  NOTIMPLEMENTED();
}

}  // namespace NPAPI

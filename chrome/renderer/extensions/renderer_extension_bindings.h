// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_

#include "v8/include/v8.h"

#include <string>

class RenderThreadBase;

// This class adds extension-related javascript bindings to a renderer.  It is
// used by both web renderers and extension processes.
class RendererExtensionBindings {
 public:
  // Name of extension, for dependencies.
  static const char* kName;

  // Creates an instance of the extension.
  static v8::Extension* Get();

  // Notify any listeners that a message channel has been opened to this
  // process.  |tab_json| is the info for the tab that initiated this
  // connection, or "null" if the initiator was not a tab.
  static void HandleConnect(int port_id, const std::string& tab_json);

  // Dispatch the given message sent on this channel.
  static void HandleMessage(const std::string& message, int port_id);

  // Send this event to all extensions in this process. |args| is a JSON-
  // serialized array that will be deserialized and provided to the callback
  // function in event_bindings.js
  static void HandleEvent(const std::string& event_name,
                          const std::string& args);
};

#endif  // CHROME_RENDERER_EXTENSIONS_RENDERER_EXTENSION_BINDINGS_H_

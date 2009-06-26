// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EVENT_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_EVENT_BINDINGS_H_

#include "v8/include/v8.h"

#include <string>

class RenderThreadBase;
class WebFrame;

// This class deals with the javascript bindings related to Event objects.
class EventBindings {
 public:
  static const char* kName;  // The v8::Extension name, for dependencies.
  static v8::Extension* Get();

  // Allow RenderThread to be mocked out.
  static void SetRenderThread(RenderThreadBase* thread);

  // Handle a script context coming / going away.
  static void HandleContextCreated(WebFrame* frame);
  static void HandleContextDestroyed(WebFrame* frame);

  // Calls the given function in each registered context which is listening
  // for events.  The function can be an object property, ie:
  // "chromium.Event.dispatch_".
  static void CallFunction(const std::string& function_name, int argc,
                           v8::Handle<v8::Value>* argv);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EVENT_BINDINGS_H_

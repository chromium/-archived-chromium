// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_DISPATCHER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_DISPATCHER_H_

#include <string>
#include <vector>

#include "base/values.h"

class ExtensionFunction;
class RenderViewHost;

// ExtensionFunctionDispatcher receives requests to execute functions from
// Chromium extensions running in a RenderViewHost and dispatches them to the
// appropriate handler. It lives entirely on the UI thread.
class ExtensionFunctionDispatcher {
 public:
  // Gets a list of all known extension function names.
  static void GetAllFunctionNames(std::vector<std::string>* names);

  ExtensionFunctionDispatcher(RenderViewHost* render_view_host);

  // Handle a request to execute an extension function.
  void HandleRequest(const std::string& name, const std::string& args,
                     int callback_id);

  // Send a response to a function.
  void SendResponse(ExtensionFunction* api);

 private:
  RenderViewHost* render_view_host_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_DISPATCHER_H_

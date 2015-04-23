// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines AutomationExtensionFunction.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_EXTENSION_FUNCTION_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_EXTENSION_FUNCTION_H_

#include <string>

#include "chrome/browser/extensions/extension_function.h"

class RenderViewHost;

// An extension function that pipes the extension API call through the
// automation interface, so that extensions can be tested using UITests.
class AutomationExtensionFunction : public ExtensionFunction {
 public:
  AutomationExtensionFunction() { }

  // ExtensionFunction implementation.
  virtual void SetName(const std::string& name);
  virtual void SetArgs(const std::string& args);
  virtual const std::string GetResult();
  virtual const std::string GetError();
  virtual void Run();

  static ExtensionFunction* Factory();

  // If enabled, we set an instance of this function as the functor
  // for all function names in ExtensionFunctionFactoryRegistry.
  // If disabled, we restore the initial functions.
  static void SetEnabled(bool enabled);

  // Intercepts messages sent from the external host to check if they
  // are actually responses to extension API calls.  If they are, redirects
  // the message to view_host->SendExtensionResponse and returns true,
  // otherwise returns false to indicate the message was not intercepted.
  static bool InterceptMessageFromExternalHost(RenderViewHost* view_host,
                                               const std::string& message,
                                               const std::string& origin,
                                               const std::string& target);

 private:
  static bool enabled_;
  std::string name_;
  std::string args_;
  DISALLOW_COPY_AND_ASSIGN(AutomationExtensionFunction);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_EXTENSION_FUNCTION_H_

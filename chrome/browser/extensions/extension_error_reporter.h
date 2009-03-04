// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ERROR_REPORTER_H_
#define CHROME_BROWSER_EXTENSIONS_ERROR_REPORTER_H_

#include <string>
#include <vector>

#include "base/message_loop.h"
#include "base/ref_counted.h"

// Exposes an easy way for the various components of the extension system to
// report errors. This is a singleton that lives on the UI thread, with the
// exception of ReportError() which may be called from any thread.
// TODO(aa): Hook this up to about:extensions, when we have about:extensions.
// TODO(aa): Consider exposing directly, or via a helper, to the renderer
// process and plumbing the errors out to the browser.
// TODO(aa): Add ReportError(extension_id, message, be_noisy), so that we can
// report errors that are specific to a particular extension.
class ExtensionErrorReporter {
 public:
  // Initializes the error reporter. Must be called before any other methods
  // and on the UI thread.
  static void Init(bool enable_noisy_errors);

  // Get the singleton instance.
  static ExtensionErrorReporter* GetInstance();

  // Report an error. Errors always go to LOG(INFO). Optionally, they can also
  // cause a noisy alert box. This method can be called from any thread.
  void ReportError(const std::string& message, bool be_noisy);

  // Get the errors that have been reported so far.
  const std::vector<std::string>* GetErrors();

  // Clear the list of errors reported so far.
  void ClearErrors();

 private:
  static ExtensionErrorReporter* instance_;

  ExtensionErrorReporter(bool enable_noisy_errors);

  MessageLoop* ui_loop_;
  std::vector<std::string> errors_;
  bool enable_noisy_errors_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_ERROR_REPORTER_H_

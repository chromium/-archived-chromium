// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSIONS_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSIONS_UI_H_

#include <string>

#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/dom_ui/dom_ui_contents.h"
#include "chrome/browser/extensions/extensions_service.h"

class GURL;

class ExtensionsUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  ExtensionsUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionsUIHTMLSource);
};

// The handler for Javascript messages related to the "extensions" view.
class ExtensionsDOMHandler : public DOMMessageHandler {
 public:
  ExtensionsDOMHandler(DOMUI* dom_ui,
      ExtensionsService* extension_service);

  virtual ~ExtensionsDOMHandler();
  void Init();

  // Extension Detail JSON Struct for page. (static for ease of testing).
  static DictionaryValue*
      CreateExtensionDetailValue(const Extension *extension);

  // ContentScript JSON Struct for page. (static for ease of testing).
  static DictionaryValue*
      CreateContentScriptDetailValue(const UserScript& script,
                                     const FilePath& extension_path);

 private:
  // Callback for "requestExtensionsData" message.
  void HandleRequestExtensionsData(const Value* value);

  // Our model.
  scoped_refptr<ExtensionsService> extensions_service_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsDOMHandler);
};

class ExtensionsUI : public DOMUI {
 public:
  explicit ExtensionsUI(DOMUIContents* contents);

  // Return the URL for the front page of this UI.
  static GURL GetBaseURL();

  // DOMUI Implementation
  virtual void Init();

 private:
  DOMUIContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsUI);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_UI_H_


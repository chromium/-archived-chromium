// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSIONS_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSIONS_UI_H_

#include <string>
#include <vector>

#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "googleurl/src/gurl.h"

class DictionaryValue;
class Extension;
class ExtensionsService;
class FilePath;
class UserScript;
class Value;

// Information about a page running in an extension, for example a toolstrip,
// a background page, or a tab contents.
struct ExtensionPage {
  ExtensionPage(const GURL& url, int render_process_id, int render_view_id)
    : url(url), render_process_id(render_process_id),
      render_view_id(render_view_id) {}
  GURL url;
  int render_process_id;
  int render_view_id;
};

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
  ExtensionsDOMHandler(ExtensionsService* extension_service);
  virtual ~ExtensionsDOMHandler();
  
  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // Extension Detail JSON Struct for page. (static for ease of testing).
  static DictionaryValue* CreateExtensionDetailValue(
      const Extension *extension,
      const std::vector<ExtensionPage>&);

  // ContentScript JSON Struct for page. (static for ease of testing).
  static DictionaryValue* CreateContentScriptDetailValue(
      const UserScript& script,
      const FilePath& extension_path);

 private:
  // Callback for "requestExtensionsData" message.
  void HandleRequestExtensionsData(const Value* value);

  // Callback for "inspect" message.
  void HandleInspectMessage(const Value* value);

  // Callback for "uninstall" message.
  void HandleUninstallMessage(const Value* value);

  // Helper that lists the current active html pages for an extension.
  std::vector<ExtensionPage> GetActivePagesForExtension(
      const std::string& extension_id);

  // Our model.
  scoped_refptr<ExtensionsService> extensions_service_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsDOMHandler);
};

class ExtensionsUI : public DOMUI {
 public:
  explicit ExtensionsUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionsUI);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_UI_H_

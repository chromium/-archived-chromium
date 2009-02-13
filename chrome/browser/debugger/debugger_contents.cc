// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for working with strings.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/debugger_contents.h"
#include "chrome/browser/debugger/debugger_shell.h"
#include "chrome/browser/debugger/debugger_wrapper.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/resource_bundle.h"
#include "net/base/mime_util.h"

#include "grit/debugger_resources.h"

class DebuggerHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  // Creates our datasource and sets our user message to a specific message
  // from our string bundle.
  DebuggerHTMLSource()
      : DataSource("debugger", MessageLoop::current()) { }

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id) {
    int resource_id = 0;

    if (!path.length()) {
      resource_id = IDR_DEBUGGER_HTML;
    } else if (path == "debugger.js") {
      resource_id = IDR_DEBUGGER_JS;
    } else if (path == "debugger.css") {
      resource_id = IDR_DEBUGGER_CSS;
    } else {
      SendResponse(request_id, NULL);
      return;
    }

    std::wstring debugger_path =
        CommandLine::ForCurrentProcess()->GetSwitchValue(
            switches::kJavaScriptDebuggerPath);
    std::string data_str;
    if (!debugger_path.empty() && file_util::PathExists(debugger_path)) {
      if (path.empty())
        file_util::AppendToPath(&debugger_path, L"debugger.html");
      else
        file_util::AppendToPath(&debugger_path, UTF8ToWide(path));
      if (!file_util::ReadFileToString(debugger_path, &data_str)) {
        SendResponse(request_id, NULL);
        return;
      }
    } else {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      data_str = rb.GetDataResource(resource_id);
    }
    scoped_refptr<RefCountedBytes> data_bytes(new RefCountedBytes);
    data_bytes->data.resize(data_str.size());
    std::copy(data_str.begin(), data_str.end(), data_bytes->data.begin());

    SendResponse(request_id, data_bytes);
  }

  virtual std::string GetMimeType(const std::string& path) const {
    // Currently but three choices {"", "debugger.js", "debugger.css"}.
    // Map the extension to mime-type, defaulting to "text/html".
    std::string mime_type("text/html");
    net::GetMimeTypeFromFile(ASCIIToWide(path), &mime_type);
    return mime_type;
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(DebuggerHTMLSource);
};


class DebuggerHandler : public DOMMessageHandler {
 public:
  explicit DebuggerHandler(DOMUIHost* host) {
    host->RegisterMessageCallback("DebuggerHostMessage",
        NewCallback(this, &DebuggerHandler::HandleDebuggerHostMessage));
  }

  void HandleDebuggerHostMessage(const Value* content) {
    if (!content || !content->IsType(Value::TYPE_LIST)) {
      NOTREACHED();
      return;
    }
    const ListValue* args = static_cast<const ListValue*>(content);
    if (args->GetSize() < 1) {
      NOTREACHED();
      return;
    }

#ifndef CHROME_DEBUGGER_DISABLED
    DebuggerWrapper* wrapper = g_browser_process->debugger_wrapper();
    DebuggerHost* debugger_host = wrapper->GetDebugger();
    if (!debugger_host) {
      NOTREACHED();
      return;
    }
    debugger_host->OnDebuggerHostMsg(args);
#endif
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(DebuggerHandler);
};


DebuggerContents::DebuggerContents(Profile* profile, SiteInstance* instance)
    : DOMUIHost(profile, instance, NULL) {
  set_type(TAB_CONTENTS_DEBUGGER);
}

void DebuggerContents::AttachMessageHandlers() {
  AddMessageHandler(new DebuggerHandler(this));

  DebuggerHTMLSource* html_source = new DebuggerHTMLSource();
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(&chrome_url_data_manager,
      &ChromeURLDataManager::AddDataSource,
      html_source));
}

// static
bool DebuggerContents::IsDebuggerUrl(const GURL& url) {
  return (url.SchemeIs("chrome-ui") && url.host() == "inspector");
}


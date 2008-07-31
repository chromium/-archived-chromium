// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// This file defines utility functions for working with strings.

#include "base/command_line.h"
#include "base/file_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/debugger_contents.h"
#include "chrome/browser/debugger/debugger_shell.h"
#include "chrome/browser/debugger/debugger_wrapper.h"
#include "chrome/browser/debugger/resources/debugger_resources.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/resource_bundle.h"

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
        CommandLine().GetSwitchValue(switches::kJavaScriptDebuggerPath);
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

 private:
  DISALLOW_EVIL_CONSTRUCTORS(DebuggerHTMLSource);
};


class DebuggerHandler : public DOMMessageHandler {
 public:
  explicit DebuggerHandler(DOMUIHost* host) {
    host->RegisterMessageCallback("command",
        NewCallback(this, &DebuggerHandler::HandleCommand));
  }

  void HandleCommand(const Value* content) {
    // Extract the parameters out of the input list.
    if (!content || !content->IsType(Value::TYPE_LIST)) {
      NOTREACHED();
      return;
    }
    const ListValue* args = static_cast<const ListValue*>(content);
    if (args->GetSize() != 1) {
      NOTREACHED();
      return;
    }
    std::wstring command;
    Value* value = NULL;
    if (!args->Get(0, &value) || !value->GetAsString(&command)) {
      NOTREACHED();
      return;
    }
#ifndef CHROME_DEBUGGER_DISABLED
    DebuggerWrapper* wrapper = g_browser_process->debugger_wrapper();
    DebuggerShell* shell = wrapper->GetDebugger();
    shell->ProcessCommand(command);
#endif
  }
 private:
  DISALLOW_EVIL_CONSTRUCTORS(DebuggerHandler);
};


DebuggerContents::DebuggerContents(Profile* profile, SiteInstance* instance)
    : DOMUIHost(profile, instance, NULL) {
  type_ = TAB_CONTENTS_DEBUGGER;
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
  if (url.SchemeIs("chrome-resource") && url.host() == "debugger")
    return true;
  return false;
}

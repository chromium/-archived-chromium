// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_REMOTE_SERVICE_H_
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_REMOTE_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/debugger/devtools_protocol_handler.h"
#include "chrome/browser/debugger/devtools_remote.h"

class DevToolsProtocolHandler;
class DevToolsRemoteMessage;
class DictionaryValue;
class Value;
class TabContents;

// Contains constants for DebuggerRemoteService tool protocol commands
// (only V8-related).
struct DebuggerRemoteServiceCommand {
  static const std::string kAttach;
  static const std::string kDetach;
  static const std::string kDebuggerCommand;
  static const std::string kEvaluateJavascript;
};

// Handles V8 debugger-related messages from the remote debugger (like
// attach to V8 debugger, detach from V8 debugger, send command to V8 debugger)
// and proxies JSON messages from V8 debugger to the remote debugger.
class DebuggerRemoteService : public DevToolsRemoteListener {
 public:
  explicit DebuggerRemoteService(DevToolsProtocolHandler* delegate);
  virtual ~DebuggerRemoteService();

  // Handles a JSON message from the tab_id-associated V8 debugger.
  void DebuggerOutput(int32 tab_id, const std::string& message);

  // Expose to public so that we can detach from tab
  // on remote debugger connection loss. If |response| is not NULL,
  // the operation result will be written as the "result" field in |response|,
  // otherwise it will not be propagated back to the caller.
  void DetachTab(const std::string& destination,
                 DictionaryValue* response);

  // DevToolsRemoteListener interface
  virtual void HandleMessage(const DevToolsRemoteMessage& message);
  virtual void OnConnectionLost();

  static const std::string kToolName;

 private:
  // Operation result returned in the "result" field.
  struct Result {
    static const int kOk = 0;
    static const int kIllegalTabState = 1;
    static const int kUnknownTab = 2;
    static const int kDebuggerError = 3;
    static const int kUnknownCommand = 4;
  };

  void AttachTab(const std::string& destination,
                 DictionaryValue* response);
  TabContents* ToTabContents(int32 tab_uid);
  void SendResponse(const Value& response,
                    const std::string& tool,
                    const std::string& destination);
  static const std::wstring kDataWide;
  static const std::wstring kResultWide;
  DevToolsProtocolHandler* delegate_;
  DISALLOW_COPY_AND_ASSIGN(DebuggerRemoteService);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEBUGGER_REMOTE_SERVICE_H_

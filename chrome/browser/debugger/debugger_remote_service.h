// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares the DebuggerRemoteServiceCommand struct and the
// DebuggerRemoteService class which handles commands directed to the
// "V8Debugger" tool.
#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_REMOTE_SERVICE_H_
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_REMOTE_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/debugger/devtools_remote.h"

class DevToolsProtocolHandler;
class DevToolsRemoteMessage;
class DictionaryValue;
class Value;
class TabContents;

// Contains constants for DebuggerRemoteService tool protocol commands
// (V8-related only).
struct DebuggerRemoteServiceCommand {
  static const std::string kAttach;
  static const std::string kDetach;
  static const std::string kDebuggerCommand;
  static const std::string kEvaluateJavascript;
  static const std::string kFrameNavigate;  // navigation event
  static const std::string kTabClosed;  // tab closing event
};

// Handles V8 debugger-related messages from the remote debugger (like
// attach to V8 debugger, detach from V8 debugger, send command to V8 debugger)
// and proxies JSON messages from V8 debugger to the remote debugger.
class DebuggerRemoteService : public DevToolsRemoteListener {
 public:
  // |delegate| (never NULL) is the protocol handler instance
  // which dispatches messages to this service. The responses from the
  // V8 VM debugger are routed back to |delegate|.
  // The ownership of |delegate| is NOT transferred to this class.
  explicit DebuggerRemoteService(DevToolsProtocolHandler* delegate);
  virtual ~DebuggerRemoteService();

  // Handles a JSON message from the tab_uid-associated V8 debugger.
  void DebuggerOutput(int32 tab_uid, const std::string& message);

  // Handles a frame navigation event.
  void FrameNavigate(int32 tab_uid, const std::string& url);

  // Handles a tab closing event.
  void TabClosed(int32 tab_uid);

  // Detaches the remote debugger from the tab specified by |destination|.
  // It is public so that we can detach from the tab on the remote debugger
  // connection loss.
  // If |response| is not NULL, the operation result will be written
  // as the "result" field in |response|, otherwise the result
  // will not be propagated back to the caller.
  void DetachFromTab(const std::string& destination,
                     DictionaryValue* response);

  // DevToolsRemoteListener interface.

  // Processes |message| from the remote debugger, where the tool is
  // "V8Debugger". Either sends the reply immediately or waits for an
  // asynchronous response from the V8 debugger.
  virtual void HandleMessage(const DevToolsRemoteMessage& message);

  // Gets invoked on the remote debugger [socket] connection loss.
  // Notifies the InspectableTabProxy of the remote debugger detachment.
  virtual void OnConnectionLost();

  // Specifies a tool name ("V8Debugger") handled by this class.
  static const std::string kToolName;

 private:
  // Operation result returned in the "result" field.
  typedef enum {
    RESULT_OK = 0,
    RESULT_ILLEGAL_TAB_STATE,
    RESULT_UNKNOWN_TAB,
    RESULT_DEBUGGER_ERROR,
    RESULT_UNKNOWN_COMMAND
  } Result;

  // Attaches a remote debugger to the tab specified by |destination|.
  // Writes the attachment result (one of Result enum values) into |response|.
  void AttachToTab(const std::string& destination,
                   DictionaryValue* response);

  // Retrieves a WebContents instance for the specified |tab_uid|
  // or NULL if no such tab is found or no WebContents instance
  // corresponds to that tab.
  TabContents* ToTabContents(int32 tab_uid);

  // Sends a JSON message with the |response| to the remote debugger.
  // |tool| and |destination| are used as the respective header values.
  void SendResponse(const Value& response,
                    const std::string& tool,
                    const std::string& destination);

  // Redirects a V8 debugger command from |content| to a V8 debugger associated
  // with the |tab_uid| and writes the result into |response| if it becomes
  // known immediately.
  bool DispatchDebuggerCommand(int tab_uid,
                               DictionaryValue* content,
                               DictionaryValue* response);

  // Redirects a Javascript evaluation command from |content| to
  // a V8 debugger associated with the |tab_uid| and writes the result
  // into |response| if it becomes known immediately.
  bool DispatchEvaluateJavascript(int tab_uid,
                                  DictionaryValue* content,
                                  DictionaryValue* response);

  // The delegate is used to get an InspectableTabProxy instance.
  DevToolsProtocolHandler* delegate_;
  DISALLOW_COPY_AND_ASSIGN(DebuggerRemoteService);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEBUGGER_REMOTE_SERVICE_H_

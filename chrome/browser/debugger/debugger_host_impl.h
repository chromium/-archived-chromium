// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEBUGGER_HOST_IMPL_H__
#define CHROME_BROWSER_DEBUGGER_DEBUGGER_HOST_IMPL_H__

#include "base/scoped_ptr.h"
#include "chrome/browser/debugger/debugger_host.h"

class DebuggerInputOutput;
class TabContents;
class TabContentsReference;

class DebuggerHostImpl : public DebuggerHost {
 public:
  DebuggerHostImpl(DebuggerInputOutput *io);
  virtual ~DebuggerHostImpl();

  virtual void Start();

  // Start debugging the specified tab
  virtual void Debug(TabContents* tab);
  // A message from the V8 debugger in the renderer being debugged via
  // RenderViewHost
  virtual void DebugMessage(const std::wstring& msg);
  // We've been successfully attached to a renderer.
  virtual void OnDebugAttach();
  // The renderer we're attached to is gone.
  virtual void OnDebugDisconnect();

  virtual void DidDisconnect();

  // Handles messages from debugger UI.
  virtual void OnDebuggerHostMsg(const ListValue* args);

 private:

  TabContents* GetTabContentsBeingDebugged() const;

  scoped_refptr<DebuggerInputOutput> io_;
  // reference to the tab being debugged by this instance
  scoped_ptr<TabContentsReference> tab_reference_;
  // If the debugger is ready to process another command or is busy.
  bool debugger_ready_;

  DISALLOW_COPY_AND_ASSIGN(DebuggerHostImpl);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEBUGGER_HOST_IMPL_H__

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_MOCK_RENDER_PROCESS_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_MOCK_RENDER_PROCESS_HOST_H_

#include "base/basictypes.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "ipc/ipc_test_sink.h"

class TransportDIB;

// A mock render process host that has no corresponding renderer process. The
// process() refers to the current process, and all IPC messages are sent into
// the message sink for inspection by tests.
class MockRenderProcessHost : public RenderProcessHost {
 public:
  MockRenderProcessHost(Profile* profile);
  virtual ~MockRenderProcessHost();

  // Provides access to all IPC messages that would have been sent to the
  // renderer via this RenderProcessHost.
  IPC::TestSink& sink() { return sink_; }

  // RenderProcessHost implementation (public portion).
  virtual bool Init();
  virtual int GetNextRoutingID();
  virtual void CancelResourceRequests(int render_widget_id);
  virtual void CrossSiteClosePageACK(int new_render_process_host_id,
                                     int new_request_id);
  virtual bool WaitForPaintMsg(int render_widget_id,
                               const base::TimeDelta& max_delay,
                               IPC::Message* msg);
  virtual void ReceivedBadMessage(uint16 msg_type);
  virtual void WidgetRestored();
  virtual void WidgetHidden();
  virtual void AddWord(const std::wstring& word);
  virtual bool FastShutdownIfPossible();
  virtual TransportDIB* GetTransportDIB(TransportDIB::Id dib_id);

  // IPC::Channel::Sender via RenderProcessHost.
  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener via RenderProcessHost.
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelConnected(int32 peer_pid);

 private:
  // Stores IPC messages that would have been sent to the renderer.
  IPC::TestSink sink_;
  TransportDIB* transport_dib_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderProcessHost);
};

class MockRenderProcessHostFactory : public RenderProcessHostFactory {
 public:
  MockRenderProcessHostFactory() {}
  virtual ~MockRenderProcessHostFactory() {}

  virtual RenderProcessHost* CreateRenderProcessHost(
      Profile* profile) const {
    return new MockRenderProcessHost(profile);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRenderProcessHostFactory);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_MOCK_RENDER_PROCESS_HOST_H_

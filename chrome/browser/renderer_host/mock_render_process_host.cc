// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/mock_render_process_host.h"

MockRenderProcessHost::MockRenderProcessHost(Profile* profile)
    : RenderProcessHost(profile) {
}

MockRenderProcessHost::~MockRenderProcessHost() {
}

bool MockRenderProcessHost::Init() {
  return true;
}

int MockRenderProcessHost::GetNextRoutingID() {
  static int prev_routing_id = 0;
  return ++prev_routing_id;
}

void MockRenderProcessHost::CancelResourceRequests(int render_widget_id) {
}

void MockRenderProcessHost::CrossSiteClosePageACK(
    int new_render_process_host_id,
    int new_request_id) {
}

bool MockRenderProcessHost::WaitForPaintMsg(int render_widget_id,
                                            const base::TimeDelta& max_delay,
                                            IPC::Message* msg) {
  return false;
}

void MockRenderProcessHost::ReceivedBadMessage(uint16 msg_type) {
}

void MockRenderProcessHost::WidgetRestored() {
}

void MockRenderProcessHost::WidgetHidden() {
}

void MockRenderProcessHost::AddWord(const std::wstring& word) {
}

bool MockRenderProcessHost::FastShutdownIfPossible() {
  return false;
}

bool MockRenderProcessHost::Send(IPC::Message* msg) {
  // Save the message in the sink.
  sink_.OnMessageReceived(*msg);
  delete msg;
  return true;
}

TransportDIB* MockRenderProcessHost::GetTransportDIB(TransportDIB::Id dib_id) {
#if defined(OS_WIN)
  return TransportDIB::Map(dib_id.handle);
#elif defined(OS_MACOSX)
  // On Mac, TransportDIBs are always created in the browser, so we cannot map
  // one from a dib_id.
  return TransportDIB::Create(100 * 100 * 4, 0);
#elif defined(OS_LINUX)
  return TransportDIB::Map(dib_id);
#endif
}

void MockRenderProcessHost::OnMessageReceived(const IPC::Message& msg) {
}

void MockRenderProcessHost::OnChannelConnected(int32 peer_pid) {
}

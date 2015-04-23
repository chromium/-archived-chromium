// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MOCK_RENDER_THREAD_H_
#define CHROME_RENDERER_MOCK_RENDER_THREAD_H_

#include <string>
#include <vector>

#include "chrome/common/ipc_test_sink.h"
#include "chrome/renderer/mock_printer.h"
#include "chrome/renderer/render_thread.h"

struct ViewMsg_Print_Params;
struct ViewMsg_PrintPages_Params;
struct ViewHostMsg_ScriptedPrint_Params;

// This class is very simple mock of RenderThread. It simulates an IPC channel
// which supports only two messages:
// ViewHostMsg_CreateWidget : sync message sent by the Widget.
// ViewMsg_Close : async, send to the Widget.
class MockRenderThread : public RenderThreadBase {
 public:
  MockRenderThread();
  virtual ~MockRenderThread();

  // Provides access to the messages that have been received by this thread.
  IPC::TestSink& sink() { return sink_; }

  // Called by the Widget. The routing_id must match the routing id assigned
  // to the Widget in reply to ViewHostMsg_CreateWidget message.
  virtual void AddRoute(int32 routing_id, IPC::Channel::Listener* listener);

  // Called by the Widget. The routing id must match the routing id of AddRoute.
  virtual void RemoveRoute(int32 routing_id);

  // Called by the Widget. Used to send messages to the browser.
  // We short-circuit the mechanim and handle the messages right here on this
  // class.
  virtual bool Send(IPC::Message* msg);

  // Our mock thread doesn't do filtering.
  virtual void AddFilter(IPC::ChannelProxy::MessageFilter* filter) {
  }
  virtual void RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) {
  }

  //////////////////////////////////////////////////////////////////////////
  // The following functions are called by the test itself.

  void set_routing_id(int32 id) {
    routing_id_ = id;
  }

  int32 opener_id() const {
    return opener_id_;
  }

  bool has_widget() const {
    return widget_ ? true : false;
  }

  // Simulates the Widget receiving a close message. This should result
  // on releasing the internal reference counts and destroying the internal
  // state.
  void SendCloseMessage();

  // Returns the pseudo-printer instance.
  const MockPrinter* printer() const { return printer_.get(); }

 private:
  // This function operates as a regular IPC listener.
  void OnMessageReceived(const IPC::Message& msg);

  // The Widget expects to be returned valid route_id.
  void OnMsgCreateWidget(int opener_id,
                         bool activatable,
                         int* route_id);

  // The callee expects to be returned a valid channel_id.
  void OnMsgOpenChannelToExtension(int routing_id,
                                   const std::string& extension_id,
                                   int* channel_id);

  void OnDuplicateSection(base::SharedMemoryHandle renderer_handle,
                          base::SharedMemoryHandle* browser_handle);

  // The RenderView expects default print settings.
  void OnGetDefaultPrintSettings(ViewMsg_Print_Params* setting);

  // The RenderView expects final print settings from the user.
  void OnScriptedPrint(const ViewHostMsg_ScriptedPrint_Params& params,
                       ViewMsg_PrintPages_Params* settings);

  void OnDidGetPrintedPagesCount(int cookie, int number_pages);
  void OnDidPrintPage(const ViewHostMsg_DidPrintPage_Params& params);

  IPC::TestSink sink_;

  // Routing id what will be assigned to the Widget.
  int32 routing_id_;

  // Opener id reported by the Widget.
  int32 opener_id_;

  // We only keep track of one Widget, we learn its pointer when it
  // adds a new route.
  IPC::Channel::Listener* widget_;

  // The last known good deserializer for sync messages.
  scoped_ptr<IPC::MessageReplyDeserializer> reply_deserializer_;

  // A mock printer device used for printing tests.
  scoped_ptr<MockPrinter> printer_;
};

#endif  // CHROME_RENDERER_MOCK_RENDER_THREAD_H_

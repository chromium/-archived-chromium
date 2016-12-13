// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "Frame.h"
#include "PageGroupLoadDeferrer.h"
#include "V8Proxy.h"
#include <wtf/HashSet.h>
#undef LOG

#include "base/string_util.h"
#include "webkit/glue/devtools/debugger_agent_impl.h"
#include "webkit/glue/devtools/debugger_agent_manager.h"
#include "webkit/glue/webdevtoolsagent_impl.h"
#include "webkit/glue/webview_impl.h"

#if USE(V8)
#include "v8/include/v8-debug.h"
#endif

WebDevToolsAgent::MessageLoopDispatchHandler
    DebuggerAgentManager::message_loop_dispatch_handler_ = NULL;

// static
bool DebuggerAgentManager::in_host_dispatch_handler_ = false;

// static
DebuggerAgentManager::DeferrersMap DebuggerAgentManager::page_deferrers_;

namespace {

class CallerIdWrapper : public v8::Debug::ClientData {
 public:
  CallerIdWrapper() : caller_is_mananager_(true), caller_id_(0) {}
  explicit CallerIdWrapper(int caller_id)
    : caller_is_mananager_(false),
      caller_id_(caller_id) {}
  ~CallerIdWrapper() {}
  bool caller_is_mananager() const { return caller_is_mananager_; }
  int caller_id() const { return caller_id_; }
 private:
  bool caller_is_mananager_;
  int caller_id_;
  DISALLOW_COPY_AND_ASSIGN(CallerIdWrapper);
};

}  // namespace


void DebuggerAgentManager::V8DebugHostDispatchHandler() {
  if (!DebuggerAgentManager::message_loop_dispatch_handler_ ||
      !attached_agents_map_) {
    return;
  }
  if (in_host_dispatch_handler_) {
    return;
  }
  in_host_dispatch_handler_ = true;

  Vector<WebViewImpl*> views;
  // 1. Disable active objects and input events.
  for (AttachedAgentsMap::iterator it = attached_agents_map_->begin();
       it != attached_agents_map_->end();
       ++it) {
    DebuggerAgentImpl* agent = it->second;
    page_deferrers_.set(
        agent->web_view(),
        new WebCore::PageGroupLoadDeferrer(agent->GetPage(), true));
    views.append(agent->web_view());
    agent->web_view()->SetIgnoreInputEvents(true);
  }

  // 2. Process messages.
  DebuggerAgentManager::message_loop_dispatch_handler_();

  // 3. Bring things back.
  for (Vector<WebViewImpl*>::iterator it = views.begin();
       it != views.end();
       ++it) {
    if (page_deferrers_.contains(*it)) {
      // The view was not closed during the dispatch.
      (*it)->SetIgnoreInputEvents(false);
    }
  }
  deleteAllValues(page_deferrers_);
  page_deferrers_.clear();

  in_host_dispatch_handler_ = false;
  if (!attached_agents_map_) {
    // Remove handlers if all agents were detached within host dispatch.
    v8::Debug::SetMessageHandler(NULL);
    v8::Debug::SetHostDispatchHandler(NULL);
  }
}

// static
DebuggerAgentManager::AttachedAgentsMap*
    DebuggerAgentManager::attached_agents_map_ = NULL;

// static
void DebuggerAgentManager::DebugAttach(DebuggerAgentImpl* debugger_agent) {
  if (!attached_agents_map_) {
    attached_agents_map_ = new AttachedAgentsMap();
    v8::Debug::SetMessageHandler2(&DebuggerAgentManager::OnV8DebugMessage);
    v8::Debug::SetHostDispatchHandler(
        &DebuggerAgentManager::V8DebugHostDispatchHandler, 100 /* ms */);
  }
  int host_id = debugger_agent->webdevtools_agent()->host_id();
  DCHECK(host_id != 0);
  attached_agents_map_->set(host_id, debugger_agent);
}

// static
void DebuggerAgentManager::DebugDetach(DebuggerAgentImpl* debugger_agent) {
  if (!attached_agents_map_) {
    NOTREACHED();
    return;
  }
  int host_id = debugger_agent->webdevtools_agent()->host_id();
  DCHECK(attached_agents_map_->get(host_id) == debugger_agent);
  bool is_on_breakpoint = (FindAgentForCurrentV8Context() == debugger_agent);
  attached_agents_map_->remove(host_id);

  if (attached_agents_map_->isEmpty()) {
    delete attached_agents_map_;
    attached_agents_map_ = NULL;
    // Note that we do not empty handlers while in dispatch - we schedule
    // continue and do removal once we are out of the dispatch. Also there is
    // no need to send continue command in this case since removing message
    // handler will cause debugger unload and all breakpoints will be cleared.
    if (!in_host_dispatch_handler_) {
      v8::Debug::SetMessageHandler2(NULL);
      v8::Debug::SetHostDispatchHandler(NULL);
    }
  } else {
    // Remove all breakpoints set by the agent.
    std::wstring clear_breakpoint_group_cmd(StringPrintf(
        L"{\"seq\":1,\"type\":\"request\",\"command\":\"clearbreakpointgroup\","
            L"\"arguments\":{\"groupId\":%d}}",
        host_id));
    SendCommandToV8(WideToUTF16(clear_breakpoint_group_cmd),
                    new CallerIdWrapper());

    if (is_on_breakpoint) {
      // Force continue if detach happened in nessted message loop while
      // debugger was paused on a breakpoint(as long as there are other
      // attached agents v8 will wait for explicit'continue' message).
      SendContinueCommandToV8();
    }
  }
}

// static
void DebuggerAgentManager::DebugBreak(DebuggerAgentImpl* debugger_agent) {
#if USE(V8)
  DCHECK(DebuggerAgentForHostId(debugger_agent->webdevtools_agent()->host_id())
             == debugger_agent);
  v8::Debug::DebugBreak();
#endif
}

// static
void DebuggerAgentManager::OnV8DebugMessage(const v8::Debug::Message& message) {
  v8::HandleScope scope;
  v8::String::Utf8Value value(message.GetJSON());
  std::string out(*value, value.length());

  // If caller_data is not NULL the message is a response to a debugger command.
  if (v8::Debug::ClientData* caller_data = message.GetClientData()) {
    CallerIdWrapper* wrapper = static_cast<CallerIdWrapper*>(caller_data);
    if (wrapper->caller_is_mananager()) {
      // Just ignore messages sent by this manager.
      return;
    }
    DebuggerAgentImpl* debugger_agent =
        DebuggerAgentForHostId(wrapper->caller_id());
    if (debugger_agent) {
      debugger_agent->DebuggerOutput(out);
    } else if (!message.WillStartRunning()) {
      // Autocontinue execution if there is no handler.
      SendContinueCommandToV8();
    }
    return;
  } // Otherwise it's an event message.
  DCHECK(message.IsEvent());

  // Ignore unsupported event types.
  if (message.GetEvent() != v8::AfterCompile &&
      message.GetEvent() != v8::Break &&
      message.GetEvent() != v8::Exception) {
    return;
  }

  v8::Handle<v8::Context> context = message.GetEventContext();
  // If the context is from one of the inpected tabs it should have its context
  // data.
  if (context.IsEmpty()) {
    // Unknown context, skip the event.
    return;
  }

  // If the context is from one of the inpected tabs or injected extension
  // scripts it must have host_id in the data field.
  int host_id = WebCore::V8Proxy::contextDebugId(context);
  if (host_id != -1) {
    DebuggerAgentImpl* agent = DebuggerAgentForHostId(host_id);
    if (agent) {
      agent->DebuggerOutput(out);
      return;
    }
  }

  if (!message.WillStartRunning()) {
    // Autocontinue execution on break and exception  events if there is no
    // handler.
    SendContinueCommandToV8();
  }
}

// static
void DebuggerAgentManager::ExecuteDebuggerCommand(
    const std::string& command,
    int caller_id) {
  SendCommandToV8(UTF8ToUTF16(command), new CallerIdWrapper(caller_id));
}

// static
void DebuggerAgentManager::SetMessageLoopDispatchHandler(
    WebDevToolsAgent::MessageLoopDispatchHandler handler) {
  message_loop_dispatch_handler_ = handler;
}

// static
void DebuggerAgentManager::SetHostId(WebFrameImpl* webframe, int host_id) {
  DCHECK(host_id > 0);
  WebCore::V8Proxy* proxy = WebCore::V8Proxy::retrieve(webframe->frame());
  if (proxy) {
    proxy->setContextDebugId(host_id);
  }
}

// static
void DebuggerAgentManager::OnWebViewClosed(WebViewImpl* webview) {
  if (page_deferrers_.contains(webview)) {
    delete page_deferrers_.get(webview);
    page_deferrers_.remove(webview);
  }
}

// static
void DebuggerAgentManager::SendCommandToV8(const string16& cmd,
                                           v8::Debug::ClientData* data) {
#if USE(V8)
  v8::Debug::SendCommand(reinterpret_cast<const uint16_t*>(cmd.data()),
                         cmd.length(),
                         data);
#endif
}

void DebuggerAgentManager::SendContinueCommandToV8() {
  std::wstring continue_cmd(
      L"{\"seq\":1,\"type\":\"request\",\"command\":\"continue\"}");
  SendCommandToV8(WideToUTF16(continue_cmd), new CallerIdWrapper());
}

// static
DebuggerAgentImpl* DebuggerAgentManager::FindAgentForCurrentV8Context() {
  if (!attached_agents_map_) {
    return NULL;
  }
  DCHECK(!attached_agents_map_->isEmpty());

  WebCore::Frame* frame = WebCore::V8Proxy::retrieveFrameForEnteredContext();
  if (!frame) {
    return NULL;
  }
  WebCore::Page* page = frame->page();
  for (AttachedAgentsMap::iterator it = attached_agents_map_->begin();
       it != attached_agents_map_->end(); ++it) {
    if (it->second->GetPage() == page) {
      return it->second;
    }
  }
  return NULL;
}

// static
DebuggerAgentImpl* DebuggerAgentManager::DebuggerAgentForHostId(int host_id) {
  if (!attached_agents_map_) {
    return NULL;
  }
  return attached_agents_map_->get(host_id);
}

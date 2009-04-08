// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "Frame.h"
#include "v8_proxy.h"
#include <wtf/HashSet.h>
#undef LOG

#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "webkit/glue/devtools/debugger_agent_impl.h"
#include "webkit/glue/devtools/debugger_agent_manager.h"

#if USE(V8)
#include "v8/include/v8-debug.h"
#endif


// static
void DebuggerAgentManager::V8DebugMessageHandler(const uint16_t* message,
                                                 int length,
                                                 void* data) {
#if USE(V8)
  std::wstring out(reinterpret_cast<const wchar_t*>(message), length);
  std::string out_utf8 = WideToUTF8(out);
  DebuggerAgentManager::DebuggerOutput(out_utf8);
#endif
}

// static
DebuggerAgentManager::AttachedAgentsSet*
    DebuggerAgentManager::attached_agents_ = NULL;

// static
void DebuggerAgentManager::DebugAttach(DebuggerAgentImpl* debugger_agent) {
#if USE(V8)
  if (!attached_agents_) {
    attached_agents_ = new AttachedAgentsSet();
    v8::Debug::SetMessageHandler(
        &DebuggerAgentManager::V8DebugMessageHandler,
        NULL, /* no additional data */
        false /* don't create separate thread for sending debugger output */);
  }
  attached_agents_->add(debugger_agent);
#endif
}

// static
void DebuggerAgentManager::DebugDetach(DebuggerAgentImpl* debugger_agent) {
#if USE(V8)
  if (!attached_agents_) {
    NOTREACHED();
    return;
  }
  DCHECK(attached_agents_->contains(debugger_agent));
  attached_agents_->remove(debugger_agent);
  if (attached_agents_->isEmpty()) {
    v8::Debug::SetMessageHandler(NULL);
    delete attached_agents_;
    attached_agents_ = NULL;
  }
#endif
}

// static
void DebuggerAgentManager::DebugBreak(DebuggerAgentImpl* debugger_agent) {
#if USE(V8)
  DCHECK(attached_agents_->contains(debugger_agent));
  v8::Debug::DebugBreak();
#endif
}

// static
void DebuggerAgentManager::DebuggerOutput(const std::string& out) {
  OwnPtr<Value> response(JSONReader::Read(out,
                                          false /* allow_trailing_comma */));
  if (!response.get()) {
    NOTREACHED();
    return;
  }
  if (!response->IsType(Value::TYPE_DICTIONARY)) {
    NOTREACHED();
    return;
  }
  DictionaryValue* m = static_cast<DictionaryValue*>(response.get());

  std::string type;
  if (!m->GetString(L"type", &type)) {
    NOTREACHED();
    return;
  }

  // Agent that should be used for sending command response is determined based
  // on the 'request_seq' field in the response body.
  if (type == "response") {
    SendCommandResponse(m);
    return;
  }

  // Agent that should be used for sending events is determined based
  // on the active Frame.
  DebuggerAgentImpl* agent = FindAgentForCurrentV8Context();
  if (!agent) {
    // Autocontinue execution on break and exception  events if there is no
    // handler.
    std::wstring continue_cmd(
        L"{\"seq\":1,\"type\":\"request\",\"command\":\"continue\"}");
    SendCommandToV8(continue_cmd);
    return;
  }
  agent->DebuggerOutput(out);
}

// static
bool DebuggerAgentManager::SendCommandResponse(DictionaryValue* response) {
  // TODO(yurys): there is a bug in v8 which converts original string seq into
  // a json dictinary.
  DictionaryValue* request_seq;
  if (!response->GetDictionary(L"request_seq", &request_seq)) {
    NOTREACHED();
    return false;
  }

  int agent_ptr;
  if (!request_seq->GetInteger(L"webdevtools_agent", &agent_ptr)) {
    NOTREACHED();
    return false;
  }

  Value* original_request_seq;
  if (request_seq->Get(L"request_seq", &original_request_seq)) {
    response->Set(L"request_seq", original_request_seq->DeepCopy());
  } else {
    response->Remove(L"request_seq", NULL /* out_value */);
  }

  DebuggerAgentImpl* debugger_agent = FindDebuggerAgentForToolsAgent(
      reinterpret_cast<WebDevToolsAgent*>(agent_ptr));
  if (!debugger_agent) {
    return false;
  }

  std::string json;
  JSONWriter::Write(response, false /* pretty_print */, &json);
  debugger_agent->DebuggerOutput(json);
  return true;
}


// static
void DebuggerAgentManager::ExecuteDebuggerCommand(
    const std::string& command,
    WebDevToolsAgent* webdevtools_agent) {
  const std::string cmd = DebuggerAgentManager::ReplaceRequestSequenceId(
      command,
      webdevtools_agent);

  SendCommandToV8(UTF8ToWide(cmd));
}

// static
void DebuggerAgentManager::SendCommandToV8(const std::wstring& cmd) {
#if USE(V8)
  v8::Debug::SendCommand(reinterpret_cast<const uint16_t*>(cmd.data()),
                         cmd.length());
#endif
}


// static
DebuggerAgentImpl* DebuggerAgentManager::FindAgentForCurrentV8Context() {
  if (!attached_agents_) {
    return NULL;
  }
  DCHECK(!attached_agents_->isEmpty());

  WebCore::Frame* frame = WebCore::V8Proxy::retrieveActiveFrame();
  if (!frame) {
    return NULL;
  }
  WebCore::Page* page = frame->page();
  for (AttachedAgentsSet::iterator it = attached_agents_->begin();
       it != attached_agents_->end(); ++it) {
    if ((*it)->GetPage() == page) {
      return *it;
    }
  }
  return NULL;
}

const std::string DebuggerAgentManager::ReplaceRequestSequenceId(
    const std::string& request,
    WebDevToolsAgent* webdevtools_agent) {
  OwnPtr<Value> message(JSONReader::Read(request,
                                         false /* allow_trailing_comma */));
  if (!message.get()) {
    return request;
  }
  if (!message->IsType(Value::TYPE_DICTIONARY)) {
    return request;
  }
  DictionaryValue* m = static_cast<DictionaryValue*>(message.get());

  std::string type;
  if (!(m->GetString(L"type", &type) && type == "request")) {
    return request;
  }

  DictionaryValue new_seq;
  Value* request_seq;
  if (m->Get(L"seq", &request_seq)) {
    new_seq.Set(L"request_seq", request_seq->DeepCopy());
  }

  // TODO(yurys): get rid of this hack, handler pointer should be passed
  // into v8::Debug::SendCommand along with the command.
  int agent_ptr = reinterpret_cast<int>(webdevtools_agent);
  new_seq.Set(L"webdevtools_agent", Value::CreateIntegerValue(agent_ptr));

  // TODO(yurys): fix v8 parser so that it handle objects as ids correctly.
  std::string new_seq_str;
  JSONWriter::Write(&new_seq, false /* pretty_print */, &new_seq_str);
  m->SetString(L"seq", new_seq_str);

  std::string json;
  JSONWriter::Write(m, false /* pretty_print */, &json);
  return json;
}

// Note that we cannot safely dereference 'webdevtools_agent' bacause the
// referenced agent may already have been detached and destroyed.
DebuggerAgentImpl* DebuggerAgentManager::FindDebuggerAgentForToolsAgent(
    WebDevToolsAgent* webdevtools_agent) {
  for (AttachedAgentsSet::iterator it = attached_agents_->begin();
       it != attached_agents_->end(); ++it) {
    if ((*it)->webdevtools_agent() == webdevtools_agent) {
      return *it;
    }
  }
  return NULL;
}

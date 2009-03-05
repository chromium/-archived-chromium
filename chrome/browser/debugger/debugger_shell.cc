// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/debugger_shell.h"

#include "build/build_config.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/debugger_io.h"
#include "chrome/browser/debugger/debugger_node.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/resource_bundle.h"

#include "grit/debugger_resources.h"

#if defined(OS_WIN)
#include "chrome/browser/tab_contents/tab_contents.h"
#elif defined(OS_POSIX)
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

DebuggerShell::DebuggerShell(DebuggerInputOutput* io) : io_(io),
                                                        debugger_ready_(true) {
}

DebuggerShell::~DebuggerShell() {
  io_->Stop();
  io_ = NULL;

  v8::Locker locked;
  v8::HandleScope scope;
  SubshellFunction("exit", 0, NULL);
  v8::V8::RemoveMessageListeners(&DelegateMessageListener);
  v8_this_.Dispose();
  v8_context_.Dispose();
  shell_.Dispose();
}

void DebuggerShell::Start() {
  io_->Start(this);

  v8::Locker locked;
  v8::HandleScope scope;

  v8_this_ = v8::Persistent<v8::External>::New(v8::External::New(this));

  v8::V8::AddMessageListener(&DelegateMessageListener, v8_this_);

  v8::Local<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New();

  // shell function
  v8::Local<v8::FunctionTemplate> shell_template =
    v8::FunctionTemplate::New(&DelegateSubshell, v8_this_);
  global_template->Set(v8::String::New("shell"), shell_template);

  // print function
  v8::Local<v8::FunctionTemplate> print_template =
    v8::FunctionTemplate::New(&DelegatePrint, v8_this_);
  global_template->Set(v8::String::New("print"), print_template);

  // source function
  v8::Local<v8::FunctionTemplate> source_template =
    v8::FunctionTemplate::New(&DelegateSource, v8_this_);
  global_template->Set(v8::String::New("source"), source_template);

  v8_context_ = v8::Context::New(NULL, global_template);
  v8::Context::Scope ctx(v8_context_);

  // This doesn't really leak. It's wrapped by the NewInstance() return value.
  ChromeNode* chrome = new ChromeNode(this);
  v8_context_->Global()->Set(v8::String::New("chrome"),
                             chrome->NewInstance());

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const std::string& debugger_shell_js =
      rb.GetDataResource(IDR_DEBUGGER_SHELL_JS);
  CompileAndRun(debugger_shell_js, "chrome.dll/debugger_shell.js");
}

void DebuggerShell::HandleWeakReference(v8::Persistent<v8::Value> obj,
                                        void* data) {
  DebuggerNodeWrapper* node = static_cast<DebuggerNodeWrapper*>(data);
  node->Release();
}

v8::Handle<v8::Value> DebuggerShell::SetDebuggerReady(const v8::Arguments& args,
                                                      DebuggerShell* debugger) {
  if (args[0]->IsBoolean()) {
    bool flag = args[0]->BooleanValue();
    debugger->debugger_ready_ = flag;
    debugger->GetIo()->SetDebuggerReady(flag);
  }
  return v8::Undefined();
}

v8::Handle<v8::Value> DebuggerShell::SetDebuggerBreak(const v8::Arguments& args,
                                                      DebuggerShell* debugger) {
  if (args[0]->IsBoolean()) {
    bool flag = args[0]->BooleanValue();
    debugger->GetIo()->SetDebuggerBreak(flag);
  }
  return v8::Undefined();
}

DebuggerInputOutput* DebuggerShell::GetIo() {
  return io_.get();
}

v8::Handle<v8::Value> DebuggerShell::DelegateSubshell(
    const v8::Arguments& args) {
  DebuggerShell* debugger =
    static_cast<DebuggerShell*>(v8::External::Cast(*args.Data())->Value());
  return debugger->Subshell(args);
}

v8::Handle<v8::Value> DebuggerShell::Subshell(const v8::Arguments& args) {
  if (args.Length() != 1) {
    return v8::Undefined();
  }
  if (!shell_.IsEmpty()) {
    shell_.Dispose();
    shell_.Clear();
    v8_context_->Global()->Delete(v8::String::New("shell_"));
  }
  if (args[0]->IsFunction()) {
    v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(args[0]);
    v8::Local<v8::Object> obj = func->NewInstance();
    if (!obj->IsUndefined()) {
      shell_ = v8::Persistent<v8::Object>::New(obj);
      v8_context_->Global()->Set(v8::String::New("shell_"), shell_);
    }
  } else if (args[0]->IsObject()) {
    shell_ =
        v8::Persistent<v8::Object>::New(v8::Local<v8::Object>::Cast(args[0]));
    v8_context_->Global()->Set(v8::String::New("shell_"), shell_);
  }
  return v8::Undefined();
}

v8::Handle<v8::Value> DebuggerShell::SubshellFunction(const char* func,
                                              int argc,
                                              v8::Handle<v8::Value>* argv) {
  if (!shell_.IsEmpty()) {
    v8::Context::Scope scope(v8_context_);
    v8::Local<v8::Value> function = shell_->Get(v8::String::New(func));
    if (function->IsFunction()) {
      v8::Local<v8::Value> ret =
        v8::Function::Cast(*function)->Call(shell_, argc, argv);
      return ret;
    }
  }
  return v8::Undefined();
}

v8::Handle<v8::Value> DebuggerShell::DelegatePrint(const v8::Arguments& args) {
  DebuggerShell* debugger =
    static_cast<DebuggerShell*>(v8::External::Cast(*args.Data())->Value());
  return debugger->Print(args);
}

v8::Handle<v8::Value> DebuggerShell::Print(const v8::Arguments& args) {
  int len = args.Length();
  for (int i = 0; i < len; i++) {
    PrintObject(args[i]);
  }
  return v8::Undefined();
}

v8::Handle<v8::Value> DebuggerShell::DelegateSource(const v8::Arguments& args) {
  DebuggerShell* debugger =
    static_cast<DebuggerShell*>(v8::External::Cast(*args.Data())->Value());
  std::wstring path;
  if (args.Length() == 0) {
    debugger->LoadUserConfig();
  } else {
    ObjectToString(args[0], &path);
    bool ret = debugger->LoadFile(path);
    if (!ret) {
      return v8::String::New("failed to load");
    }
  }
  return v8::Undefined();
}

void DebuggerShell::DelegateMessageListener(v8::Handle<v8::Message> message,
                                    v8::Handle<v8::Value> data) {
  DCHECK(!data.IsEmpty());
  DebuggerShell* debugger =
    static_cast<DebuggerShell*>(v8::External::Cast(*data)->Value());
  debugger->MessageListener(message);
}

void DebuggerShell::MessageListener(v8::Handle<v8::Message> message) {
  v8::HandleScope scope;
  v8::Local<v8::String> msg_str = message->Get();
  PrintObject(msg_str);

  v8::Handle<v8::Value> data = message->GetScriptResourceName();
  if (!data.IsEmpty() && !data->IsUndefined()) {
    std::wstring out;
    ObjectToString(data, &out);
    int line_number = message->GetLineNumber();
    if (line_number >= 0)
      out += StringPrintf(L":%d", line_number);
    PrintLine(out);
    data = message->GetSourceLine();
    if (!data->IsUndefined()) {
      ObjectToString(data, &out);
      PrintLine(out);
    }
  }
}

void DebuggerShell::Debug(TabContents* tab) {
  v8::Locker locked;
  v8::HandleScope outer;
  v8::Context::Scope scope(v8_context_);

  v8::Local<v8::Object> global = v8_context_->Global();
  v8::Local<v8::Value> function = global->Get(v8::String::New("debug"));
  if (function->IsFunction()) {
    TabNode* node = new TabNode(tab);
    v8::Handle<v8::Value> argv[] = {node->NewInstance()};
    PrintObject(v8::Function::Cast(*function)->Call(global, 1, argv));
  }
}

void DebuggerShell::DebugMessage(const std::wstring& msg) {
  v8::Locker locked;
  v8::HandleScope scope;

  if (msg.length()) {
    if ((msg[0] == L'{' || msg[0] == L'[' || msg[0] == L'(') &&
        (!shell_.IsEmpty())) {
      // v8's wide String constructor requires uint16 rather than wchar
      const uint16* data = reinterpret_cast<const uint16* >(msg.c_str());
      v8::Handle<v8::Value> argv[] = {v8::String::New(data)};
      PrintObject(SubshellFunction("response", 1, argv));
      PrintPrompt();
    } else {
      if (msg[msg.length() - 1] == L'\n')
        PrintString(msg);
      else
        PrintLine(msg);
    }
  }
}

void DebuggerShell::OnDebugAttach() {
  v8::Locker locked;
  v8::HandleScope scope;
  SubshellFunction("on_attach", 0, NULL);
}

void DebuggerShell::OnDebugDisconnect() {
  v8::Locker locked;
  v8::HandleScope scope;
  SubshellFunction("on_disconnect", 0, NULL);
}

void DebuggerShell::ObjectToString(v8::Handle<v8::Value> result,
                                   std::wstring* str) {
  v8::HandleScope scope;
  if (!result.IsEmpty() && !result->IsUndefined()) {
    v8::Local<v8::String> str_obj = result->ToString();
    if (!str_obj.IsEmpty()) {
      int length = str_obj->Length();
      wchar_t* buf = new wchar_t[length + 1];
      int size = str_obj->Write(reinterpret_cast<uint16_t*>(buf));
      str->clear();
      str->append(buf, size);
      delete[] buf;
    }
  }
}

void DebuggerShell::ObjectToString(v8::Handle<v8::Value> result,
                                   std::string* str) {
  v8::HandleScope scope;
  if (!result.IsEmpty() && !result->IsUndefined()) {
    v8::Local<v8::String> str_obj = result->ToString();
    if (!str_obj.IsEmpty()) {
      int length = str_obj->Length();
      char* buf = new char[length + 1];
      str_obj->WriteAscii(buf);
      str->clear();
      str->append(buf);
      delete[] buf;
    }
  }
}

void DebuggerShell::PrintObject(v8::Handle<v8::Value> result, bool crlf) {
  if (!result.IsEmpty() && !result->IsUndefined()) {
    std::wstring out;
    ObjectToString(result, &out);
    if (crlf) {
      PrintLine(out);
    } else if (out.length()) {
      PrintString(out);
    }
  }
}

void DebuggerShell::PrintString(const std::wstring& out) {
  if (io_)
    io_->Output(out);
}

void DebuggerShell::PrintLine(const std::wstring& out) {
  if (io_)
    io_->OutputLine(out);
}

void DebuggerShell::PrintString(const std::string& out) {
  if (io_)
    io_->Output(out);
}

void DebuggerShell::PrintLine(const std::string& out) {
  if (io_)
    io_->OutputLine(out);
}

void DebuggerShell::PrintPrompt() {
  std::wstring out = L"Chrome> ";
  if (!shell_.IsEmpty()) {
    if (!debugger_ready_)
      return;
    v8::Locker locked;
    v8::HandleScope outer;
    v8::Handle<v8::Value> result = CompileAndRun("shell_.prompt()");
    if (!result.IsEmpty() && !result->IsUndefined()) {
      ObjectToString(result, &out);
    }
  }
  if (io_)
    io_->OutputPrompt(out);
}

void DebuggerShell::ProcessCommand(const std::wstring& data) {
  v8::Locker locked;
  v8::HandleScope outer;
  v8::Context::Scope scope(v8_context_);
  if (!shell_.IsEmpty() && data.substr(0, 7) != L"source(") {
    if (data == L"exit") {
      PrintObject(SubshellFunction("exit", 0, NULL));
      v8_context_->Global()->Delete(v8::String::New("shell_"));
      shell_.Dispose();
      shell_.Clear();
    } else {
      const uint16* utf16 = reinterpret_cast<const uint16*>(data.c_str());
      v8::Handle<v8::Value> argv[] = {v8::String::New(utf16)};
      PrintObject(SubshellFunction("command", 1, argv));
    }
  } else if (data.length()) {
    //TODO(erikkay): change everything to wstring
    v8::Handle<v8::Value> result = CompileAndRun(data);
    PrintObject(result);
  }
  PrintPrompt();
}

bool DebuggerShell::LoadFile(const std::wstring& file) {
  if (file_util::PathExists(file)) {
    std::string contents;
    if (file_util::ReadFileToString(file, &contents)) {
      std::string afile = WideToUTF8(file);
      CompileAndRun(contents, afile);
      return true;
    }
  }
  return false;
}

void DebuggerShell::LoadUserConfig() {
  std::wstring path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  file_util::AppendToPath(&path, L"debugger_custom.js");
  LoadFile(path);
}

void DebuggerShell::DidConnect() {
  v8::Locker locked;
  v8::HandleScope outer;
  v8::Context::Scope scope(v8_context_);

  LoadUserConfig();

  PrintPrompt();
}

void DebuggerShell::DidDisconnect() {
  v8::Locker locked;
  v8::HandleScope outer;
  SubshellFunction("exit", 0, NULL);
}

v8::Handle<v8::Value> DebuggerShell::CompileAndRun(
    const std::string& str,
    const std::string& filename) {
  const std::wstring wstr = UTF8ToWide(str);
  return CompileAndRun(wstr, filename);
}

v8::Handle<v8::Value> DebuggerShell::CompileAndRun(
    const std::wstring& wstr,
    const std::string& filename) {
  v8::Locker locked;
  v8::Context::Scope scope(v8_context_);
  v8::Handle<v8::String> scriptname;
  if (filename.length() > 0) {
    scriptname = v8::String::New(filename.c_str());
  } else {
    scriptname = v8::String::New("");
  }
  const uint16* utf16 = reinterpret_cast<const uint16*>(wstr.c_str());
  v8::ScriptOrigin origin = v8::ScriptOrigin(scriptname);
  v8::Local<v8::Script> code =
      v8::Script::Compile(v8::String::New(utf16), &origin);
  if (!code.IsEmpty()) {
    v8::Local<v8::Value> result = code->Run();
    if (!result.IsEmpty()) {
      return result;
    }
  }
  return v8::Undefined();
}

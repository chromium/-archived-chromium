// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/render_view_test.h"

#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/renderer_preferences.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/js_only_v8_extensions.h"
#include "chrome/renderer/renderer_main_platform_delegate.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebScriptSource.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/glue/webview.h"

using WebKit::WebScriptSource;
using WebKit::WebString;
using WebKit::WebURLRequest;

namespace {

const int32 kRouteId = 5;
const int32 kOpenerId = 7;

};

void RenderViewTest::ProcessPendingMessages() {
  msg_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask());
  msg_loop_.Run();
}

WebFrame* RenderViewTest::GetMainFrame() {
  return view_->webview()->GetMainFrame();
}

void RenderViewTest::ExecuteJavaScript(const char* js) {
  GetMainFrame()->ExecuteScript(WebScriptSource(WebString::fromUTF8(js)));
}

void RenderViewTest::LoadHTML(const char* html) {
  std::string url_str = "data:text/html;charset=utf-8,";
  url_str.append(html);
  GURL url(url_str);

  GetMainFrame()->LoadRequest(WebURLRequest(url));

  // The load actually happens asynchronously, so we pump messages to process
  // the pending continuation.
  ProcessPendingMessages();
}

void RenderViewTest::SetUp() {
  sandbox_init_wrapper_.reset(new SandboxInitWrapper());
#if defined(OS_WIN)
  command_line_.reset(new CommandLine(std::wstring()));
#elif defined(OS_POSIX)
  command_line_.reset(new CommandLine(std::vector<std::string>()));
#endif
  params_.reset(new MainFunctionParams(*command_line_, *sandbox_init_wrapper_,
                                       NULL));
  platform_.reset(new RendererMainPlatformDelegate(*params_));
  platform_->PlatformInitialize();

  WebKit::initialize(&webkitclient_);
  WebKit::registerExtension(BaseJsV8Extension::Get());
  WebKit::registerExtension(JsonSchemaJsV8Extension::Get());
  WebKit::registerExtension(EventBindings::Get());
  WebKit::registerExtension(ExtensionProcessBindings::Get());
  WebKit::registerExtension(RendererExtensionBindings::Get());
  EventBindings::SetRenderThread(&render_thread_);

  // TODO(aa): Should some of this go to some other inheriting class?
  std::vector<std::string> names;
  ExtensionFunctionDispatcher::GetAllFunctionNames(&names);
  ExtensionProcessBindings::SetFunctionNames(names);

  mock_process_.reset(new MockProcess());

  render_thread_.set_routing_id(kRouteId);

  // This needs to pass the mock render thread to the view.
  view_ = RenderView::Create(&render_thread_, NULL, NULL, kOpenerId,
                             RendererPreferences(), WebPreferences(),
                             new SharedRenderViewCounter(0), kRouteId);

  // Attach a pseudo keyboard device to this object.
  mock_keyboard_.reset(new MockKeyboard());
}
void RenderViewTest::TearDown() {
  render_thread_.SendCloseMessage();

  // Run the loop so the release task from the renderwidget executes.
  ProcessPendingMessages();

  EventBindings::SetRenderThread(NULL);

  view_ = NULL;

  mock_process_.reset();
  WebKit::shutdown();

  msg_loop_.RunAllPending();

  mock_keyboard_.reset();

  platform_->PlatformUninitialize();
  platform_.reset();
  params_.reset();
  command_line_.reset();
  sandbox_init_wrapper_.reset();
}

int RenderViewTest::SendKeyEvent(MockKeyboard::Layout layout,
                                 int key_code,
                                 MockKeyboard::Modifiers modifiers,
                                 std::wstring* output) {
#if defined(OS_WIN)
  // Retrieve the Unicode character for the given tuple (keyboard-layout,
  // key-code, and modifiers).
  // Exit when a keyboard-layout driver cannot assign a Unicode character to
  // the tuple to prevent sending an invalid key code to the RenderView object.
  CHECK(mock_keyboard_.get());
  CHECK(output);
  int length = mock_keyboard_->GetCharacters(layout, key_code, modifiers,
                                             output);
  if (length != 1)
    return -1;

  // Create IPC messages from Windows messages and send them to our
  // back-end.
  // A keyboard event of Windows consists of three Windows messages:
  // WM_KEYDOWN, WM_CHAR, and WM_KEYUP.
  // WM_KEYDOWN and WM_KEYUP sends virtual-key codes. On the other hand,
  // WM_CHAR sends a composed Unicode character.
  NativeWebKeyboardEvent keydown_event(NULL, WM_KEYDOWN, key_code, 0);
  scoped_ptr<IPC::Message> keydown_message(new ViewMsg_HandleInputEvent(0));
  keydown_message->WriteData(reinterpret_cast<const char*>(&keydown_event),
                             sizeof(WebKit::WebKeyboardEvent));
  view_->OnHandleInputEvent(*keydown_message);

  NativeWebKeyboardEvent char_event(NULL, WM_CHAR, (*output)[0], 0);
  scoped_ptr<IPC::Message> char_message(new ViewMsg_HandleInputEvent(0));
  char_message->WriteData(reinterpret_cast<const char*>(&char_event),
                          sizeof(WebKit::WebKeyboardEvent));
  view_->OnHandleInputEvent(*char_message);

  NativeWebKeyboardEvent keyup_event(NULL, WM_KEYUP, key_code, 0);
  scoped_ptr<IPC::Message> keyup_message(new ViewMsg_HandleInputEvent(0));
  keyup_message->WriteData(reinterpret_cast<const char*>(&keyup_event),
                           sizeof(WebKit::WebKeyboardEvent));
  view_->OnHandleInputEvent(*keyup_message);

  return length;
#else
  NOTIMPLEMENTED();
  return L'\0';
#endif
}

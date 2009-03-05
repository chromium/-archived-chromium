// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui_host.h"

#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents_type.h"

DOMUIHost::DOMUIHost(Profile* profile,
                     SiteInstance* instance,
                     RenderViewHostFactory* render_view_factory)
    : WebContents(profile,
                  instance,
                  render_view_factory,
                  MSG_ROUTING_NONE,
                  NULL) {
  // Implementors of this class will have a specific tab contents type.
  set_type(TAB_CONTENTS_UNKNOWN_TYPE);
}

DOMUIHost::~DOMUIHost() {
  STLDeleteContainerPairSecondPointers(message_callbacks_.begin(),
                                       message_callbacks_.end());
  STLDeleteContainerPointers(handlers_.begin(), handlers_.end());
}

bool DOMUIHost::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host) {
  // Be sure to enable DOM UI bindings on the RenderViewHost before
  // CreateRenderView is called.  Since a cross-site transition may be
  // involved, this may or may not be the same RenderViewHost that we had when
  // we were created.
  render_view_host->AllowDOMUIBindings();
  return WebContents::CreateRenderViewForRenderManager(render_view_host);
}

void DOMUIHost::AddMessageHandler(DOMMessageHandler* handler) {
  handlers_.push_back(handler);
}

void DOMUIHost::RegisterMessageCallback(const std::string& name,
                                        MessageCallback* callback) {
  message_callbacks_.insert(std::make_pair(name, callback));
}


void DOMUIHost::CallJavascriptFunction(const std::wstring& function_name,
                                       const Value& arg) {
  std::string json;
  JSONWriter::Write(&arg, false, &json);
  std::wstring javascript = function_name + L"(" + UTF8ToWide(json) + L");";

  ExecuteJavascript(javascript);
}

void DOMUIHost::CallJavascriptFunction(
    const std::wstring& function_name,
    const Value& arg1, const Value& arg2) {
  std::string json;
  JSONWriter::Write(&arg1, false, &json);
  std::wstring javascript = function_name + L"(" + UTF8ToWide(json);
  JSONWriter::Write(&arg2, false, &json);
  javascript += L"," + UTF8ToWide(json) + L");";

  ExecuteJavascript(javascript);
}

void DOMUIHost::ProcessDOMUIMessage(const std::string& message,
                                    const std::string& content) {
  // Look up the callback for this message.
  MessageCallbackMap::const_iterator callback(message_callbacks_.find(message));
  if (callback == message_callbacks_.end())
    return;

  // Convert the content JSON into a Value.
  scoped_ptr<Value> value;
  if (!content.empty()) {
    value.reset(JSONReader::Read(content, false));
    if (!value.get()) {
      // The page sent us something that we didn't understand.
      // This probably indicates a programming error.
      NOTREACHED();
      return;
    }
  }

  // Forward this message and content on.
  callback->second->Run(value.get());
}

WebPreferences DOMUIHost::GetWebkitPrefs() {
  // Get the users preferences then force image loading to always be on.
  WebPreferences web_prefs = WebContents::GetWebkitPrefs();
  web_prefs.loads_images_automatically = true;

  return web_prefs;
}

void DOMUIHost::ExecuteJavascript(const std::wstring& javascript) {
  render_view_host()->ExecuteJavascriptInWebFrame(std::wstring(), javascript);
}


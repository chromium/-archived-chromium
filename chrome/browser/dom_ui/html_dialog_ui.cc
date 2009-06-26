// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/html_dialog_ui.h"

#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/renderer_host/render_view_host.h"

HtmlDialogUI::HtmlDialogUI(TabContents* tab_contents) : DOMUI(tab_contents) {
}

HtmlDialogUI::~HtmlDialogUI() {
  // Don't unregister our property. During the teardown of the TabContents,
  // this will be deleted, but the TabContents will already be destroyed.
  //
  // This object is owned indirectly by the TabContents. DOMUIs can change, so
  // it's scary if this DOMUI is changed out and replaced with something else,
  // since the property will still point to the old delegate. But the delegate
  // is itself the owner of the TabContents for a dialog so will be in scope,
  // and the HTML dialogs won't swap DOMUIs anyway since they don't navigate.
}

// static
PropertyAccessor<HtmlDialogUIDelegate*>& HtmlDialogUI::GetPropertyAccessor() {
  return *Singleton< PropertyAccessor<HtmlDialogUIDelegate*> >::get();
}

////////////////////////////////////////////////////////////////////////////////
// Private:

// Helper function to read the JSON string from the Value parameter.
static std::string GetJsonResponse(const Value* content) {
  if (!content || !content->IsType(Value::TYPE_LIST))  {
    NOTREACHED();
    return std::string();
  }
  const ListValue* args = static_cast<const ListValue*>(content);
  if (args->GetSize() != 1) {
    NOTREACHED();
    return std::string();
  }

  std::string result;
  Value* value = NULL;
  if (!args->Get(0, &value) || !value->GetAsString(&result)) {
    NOTREACHED();
    return std::string();
  }

  return result;
}

void HtmlDialogUI::RenderViewCreated(RenderViewHost* render_view_host) {
  // Hook up the javascript function calls, also known as chrome.send("foo")
  // calls in the HTML, to the actual C++ functions.
  RegisterMessageCallback("DialogClose",
                          NewCallback(this, &HtmlDialogUI::OnDialogClosed));

  // Pass the arguments to the renderer supplied by the delegate.
  std::string dialog_args;
  std::vector<DOMMessageHandler*> handlers;
  HtmlDialogUIDelegate** delegate = GetPropertyAccessor().GetProperty(
      tab_contents()->property_bag());
  if (delegate) {
    dialog_args = (*delegate)->GetDialogArgs();
    (*delegate)->GetDOMMessageHandlers(&handlers);
  }

  render_view_host->SetDOMUIProperty("dialogArguments", dialog_args);
  for (std::vector<DOMMessageHandler*>::iterator it = handlers.begin();
       it != handlers.end(); ++it) {
    (*it)->Attach(this);
    AddMessageHandler(*it);
  }
}

void HtmlDialogUI::OnDialogClosed(const Value* content) {
  HtmlDialogUIDelegate** delegate = GetPropertyAccessor().GetProperty(
      tab_contents()->property_bag());
  if (delegate)
    (*delegate)->OnDialogClosed(GetJsonResponse(content));
}

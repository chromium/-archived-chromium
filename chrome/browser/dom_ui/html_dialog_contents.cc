// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/html_dialog_contents.h"

#include "chrome/browser/render_view_host.h"

const char kGearsScheme[] = "gears";

HtmlDialogContents::HtmlDialogContents(Profile* profile,
                                       SiteInstance* instance,
                                       RenderViewHostFactory* rvf)
    : DOMUIHost(profile, instance, rvf),
      delegate_(NULL) {
  set_type(TAB_CONTENTS_HTML_DIALOG);
}

HtmlDialogContents::~HtmlDialogContents() {
}

void HtmlDialogContents::Init(HtmlDialogContentsDelegate* delegate) {
  delegate_ = delegate;

  std::string dialog_args;
  if (delegate_)
    dialog_args = delegate_->GetDialogArgs();

  DCHECK(render_view_host());
  render_view_host()->SetDOMUIProperty("dialogArguments", dialog_args);
}

////////////////////////////////////////////////////////////////////////////////
// DOMUIHost implementation:

void HtmlDialogContents::AttachMessageHandlers() {
  // Hook up the javascript function calls, also known as chrome.send("foo")
  // calls in the HTML, to the actual C++ functions.
  RegisterMessageCallback("DialogClose",
      NewCallback(this, &HtmlDialogContents::OnDialogClosed));
}

// static
bool HtmlDialogContents::IsHtmlDialogUrl(const GURL& url) {
  return url.SchemeIs(kGearsScheme);
}

////////////////////////////////////////////////////////////////////////////////
// Private:

// Helper function to read the JSON string from the Value parameter.
std::string GetJsonResponse(const Value* content) {
  if (!content || !content->IsType(Value::TYPE_LIST))  {
    NOTREACHED();
    return "";
  }
  const ListValue* args = static_cast<const ListValue*>(content);
  if (args->GetSize() != 1) {
    NOTREACHED();
    return "";
  }

  std::string result;
  Value* value = NULL;
  if (!args->Get(0, &value) || !value->GetAsString(&result)) {
    NOTREACHED();
    return "";
  }

  return result;
}

void HtmlDialogContents::OnDialogClosed(const Value* content) {
  if (delegate_)
    delegate_->OnDialogClosed(GetJsonResponse(content));
}


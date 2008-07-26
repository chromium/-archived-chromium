// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/dom_ui/html_dialog_contents.h"

#include "chrome/browser/render_view_host.h"

const char kGearsScheme[] = "gears";

HtmlDialogContents::HtmlDialogContents(Profile* profile,
                                       SiteInstance* instance,
                                       RenderViewHostFactory* rvf)
    : DOMUIHost(profile, instance, rvf),
      delegate_(NULL) {
  type_ = TAB_CONTENTS_HTML_DIALOG;
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

  std::wstring result;
  Value* value = NULL;
  if (!args->Get(0, &value) || !value->GetAsString(&result)) {
    NOTREACHED();
    return "";
  }

  return WideToASCII(result);
}

void HtmlDialogContents::OnDialogClosed(const Value* content) {
  if (delegate_)
    delegate_->OnDialogClosed(GetJsonResponse(content));
}

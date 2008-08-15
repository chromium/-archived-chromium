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

#include "chrome/browser/views/dom_view.h"

#include "chrome/browser/dom_ui/dom_ui_host.h"

DOMView::DOMView(const GURL& contents)
    : contents_(contents), initialized_(false), host_(NULL) {
}

DOMView::~DOMView() {
  if (host_) {
    Detach();
    host_->Destroy();
    host_ = NULL;
  }
}

bool DOMView::Init(Profile* profile, SiteInstance* instance) {
  if (initialized_)
    return true;
  initialized_ = true;

  // TODO(timsteele): This should use a separate factory method; e.g
  // a DOMUIHostFactory rather than TabContentsFactory, because DOMView's
  // should only be associated with instances of DOMUIHost.
  TabContentsType type = TabContents::TypeForURL(&contents_);
  TabContents* tab_contents =  TabContents::CreateWithType(type,
      GetViewContainer()->GetHWND(), profile, instance);
  host_ = tab_contents->AsDOMUIHost();
  DCHECK(host_);

  ChromeViews::HWNDView::Attach(host_->GetContainerHWND());
  host_->SetupController(profile);
  host_->controller()->LoadURL(contents_, PageTransition::START_PAGE);
  return true;
}

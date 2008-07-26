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

// DOMView is a ChromeView that displays the content of a web DOM.
// It should be used with data: URLs.

#ifndef CHROME_BROWSER_VIEWS_DOM_VIEW_H__
#define CHROME_BROWSER_VIEWS_DOM_VIEW_H__

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/dom_ui/dom_ui_host.h"
#include "chrome/views/hwnd_view.h"

class DOMView : public ChromeViews::HWNDView {
 public:
  // Construct a DOMView to display the given data: URL.
  explicit DOMView(const GURL& contents)
      : contents_(contents), initialized_(false), host_(NULL) {}

  virtual ~DOMView() {
    if (host_) {
      Detach();
      host_->Destroy();
      host_ = NULL;
    }
  }

  // Initialize the view, causing it to load its contents.  This should be
  // called once the view has been added to a container.
  // If |instance| is not null, then the view will be loaded in the same
  // process as the given instance.
  bool Init(Profile* profile, SiteInstance* instance) {
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

 protected:
  DOMUIHost* host_;

 private:
  GURL contents_;
  bool initialized_;

  DISALLOW_EVIL_CONSTRUCTORS(DOMView);
};

#endif  // CHROME_BROWSER_VIEWS_DOM_VIEW_H__


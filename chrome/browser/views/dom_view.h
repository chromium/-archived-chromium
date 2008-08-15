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

#ifndef CHROME_BROWSER_VIEWS_DOM_VIEW_H_
#define CHROME_BROWSER_VIEWS_DOM_VIEW_H_

#include "chrome/views/hwnd_view.h"
#include "googleurl/src/gurl.h"

class DOMUIHost;
class Profile;
class SiteInstance;

class DOMView : public ChromeViews::HWNDView {
 public:
  // Construct a DOMView to display the given data: URL.
  explicit DOMView(const GURL& contents);
  virtual ~DOMView();

  // Initialize the view, causing it to load its contents.  This should be
  // called once the view has been added to a container.
  // If |instance| is not null, then the view will be loaded in the same
  // process as the given instance.
  bool Init(Profile* profile, SiteInstance* instance);

 protected:
  DOMUIHost* host_;

 private:
  GURL contents_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(DOMView);
};

#endif  // CHROME_BROWSER_VIEWS_DOM_VIEW_H_

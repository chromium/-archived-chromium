// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_H_

#include "chrome/browser/tab_contents/tab_contents.h"

// TODO(brettw) this class is a placeholder until I can remove all references
// to WebContents.
class WebContents : public TabContents {
 public:
  WebContents(Profile* profile,
              SiteInstance* site_instance,
              int routing_id,
              base::WaitableEvent* modal_dialog_event)
      : TabContents(profile,
                    site_instance,
                    routing_id,
                    modal_dialog_event) {
  }

  virtual ~WebContents() {}

 private:
   DISALLOW_COPY_AND_ASSIGN(WebContents);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_H_

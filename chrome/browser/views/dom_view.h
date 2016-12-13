// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DOMView is a ChromeView that displays the content of a web DOM.
// It should be used with data: URLs.

#ifndef CHROME_BROWSER_VIEWS_DOM_VIEW_H_
#define CHROME_BROWSER_VIEWS_DOM_VIEW_H_

#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "views/controls/native/native_view_host.h"
#include "views/event.h"

class Profile;
class SiteInstance;
class TabContents;

class DOMView : public views::NativeViewHost {
 public:
  // Construct a DOMView to display the given data: URL.
  explicit DOMView();
  virtual ~DOMView();

  // Initialize the view, creating the contents. This should be
  // called once the view has been added to a container.
  //
  // If |instance| is not null, then the view will be loaded in the same
  // process as the given instance.
  bool Init(Profile* profile, SiteInstance* instance);

  // Loads the given URL into the page. You must have previously called Init().
  void LoadURL(const GURL& url);

 protected:
  // Overridden from View.
  virtual bool SkipDefaultKeyEventProcessing(const views::KeyEvent& e);
  virtual void Focus();

  scoped_ptr<TabContents> tab_contents_;

 private:
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(DOMView);
};

#endif  // CHROME_BROWSER_VIEWS_DOM_VIEW_H_

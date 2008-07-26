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

#ifndef CHROME_BROWSER_VIEWS_INFO_BAR_VIEW_H__
#define CHROME_BROWSER_VIEWS_INFO_BAR_VIEW_H__

#include <map>

#include "chrome/views/view.h"

class NavigationEntry;
class WebContents;

// This view is used to store and display views in the info bar.
//
// It will paint all of its children vertically, with the most recently
// added child displayed at the top of the info bar.
class InfoBarView : public ChromeViews::View {
 public:
  explicit InfoBarView(WebContents* web_contents);

  virtual ~InfoBarView();

  // Adds view as a child.  Views adding using AddChildView() are automatically
  // removed after 1 navigation, which is also the behavior if you set
  // |auto_expire| to true here.  You mainly need this function if you want to
  // add an infobar that should not expire.
  void AppendInfoBarItem(ChromeViews::View* view, bool auto_expire);

  virtual void GetPreferredSize(CSize *out);

  virtual void Layout();

  // API to allow infobar children to notify us of size changes.
  virtual void ChildAnimationProgressed();
  virtual void ChildAnimationEnded();

  // Invokes the following methods to do painting:
  // PaintBackground, PaintBorder and PaintSeparators.
  virtual void Paint(ChromeCanvas* canvas);

  virtual void DidChangeBounds(const CRect& previous, const CRect& current);

  // Notification that the user has done a navigation. Removes child views that
  // are set to be removed after a certain number of navigations and
  // potentially hides the info bar.  |entry| is the new entry that we are
  // navigating to.
  void DidNavigate(NavigationEntry* entry);

  WebContents* web_contents() { return web_contents_; }

 protected:
  // Overridden to force the frame to re-layout the info bar whenever a view
  // is added or removed.
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

 private:
  void Init();

  // Returns the unique ID of the active entry on the WebContents'
  // NavigationController.
  int GetActiveID() const;

  // Paints the border.
  void PaintBorder(ChromeCanvas* canvas);

  // Paints the separators. This invokes PaintSeparator to paint a particular
  // separator.
  void PaintSeparators(ChromeCanvas* canvas);

  // Paints the separator between views.
  void PaintSeparator(ChromeCanvas* canvas,
                      ChromeViews::View* v1,
                      ChromeViews::View* v2);

  WebContents* web_contents_;

  // Map from view to number of navigations before it is removed. If a child
  // doesn't have an entry in here, it is NOT removed on navigations.
  std::map<View*,int> expire_map_;

  DISALLOW_EVIL_CONSTRUCTORS(InfoBarView);
};

#endif // CHROME_BROWSER_VIEWS_INFO_BAR_VIEW_H__

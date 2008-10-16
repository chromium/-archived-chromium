// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFO_BAR_VIEW_H_
#define CHROME_BROWSER_VIEWS_INFO_BAR_VIEW_H_

#include <map>

#include "chrome/common/notification_service.h"
#include "chrome/views/view.h"

class NavigationEntry;
class WebContents;

// This view is used to store and display views in the info bar.
//
// It will paint all of its children vertically, with the most recently
// added child displayed at the top of the info bar.
class InfoBarView : public views::View,
                    public NotificationObserver {
 public:
  explicit InfoBarView(WebContents* web_contents);

  virtual ~InfoBarView();

  // Adds view as a child.  Views adding using AddChildView() are automatically
  // removed after 1 navigation, which is also the behavior if you set
  // |auto_expire| to true here.  You mainly need this function if you want to
  // add an infobar that should not expire.
  void AppendInfoBarItem(views::View* view, bool auto_expire);

  virtual gfx::Size GetPreferredSize();

  virtual void Layout();

  // API to allow infobar children to notify us of size changes.
  virtual void ChildAnimationProgressed();
  virtual void ChildAnimationEnded();

  // Invokes the following methods to do painting:
  // PaintBackground, PaintBorder and PaintSeparators.
  virtual void Paint(ChromeCanvas* canvas);

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
                      views::View* v1,
                      views::View* v2);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  WebContents* web_contents_;

  // Map from view to number of navigations before it is removed. If a child
  // doesn't have an entry in here, it is NOT removed on navigations.
  std::map<View*,int> expire_map_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarView);
};

#endif // CHROME_BROWSER_VIEWS_INFO_BAR_VIEW_H_


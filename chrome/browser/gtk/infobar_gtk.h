// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_INFOBAR_GTK_H_
#define CHROME_BROWSER_GTK_INFOBAR_GTK_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/common/owned_widget_gtk.h"

class CustomDrawButton;
class InfoBarContainerGtk;

class InfoBar {
 public:
  explicit InfoBar(InfoBarDelegate* delegate);
  virtual ~InfoBar();

  InfoBarDelegate* delegate() const { return delegate_; }

  // Get the top level native GTK widget for this infobar.
  GtkWidget* widget() { return widget_.get(); }

  // Set a link to the parent InfoBarContainer. This must be set before the
  // InfoBar is added to the view hierarchy.
  void set_container(InfoBarContainerGtk* container) { container_ = container; }

  // Starts animating the InfoBar open.
  void AnimateOpen();

  // Opens the InfoBar immediately.
  void Open();

  // Starts animating the InfoBar closed. It will not be closed until the
  // animation has completed, when |Close| will be called.
  void AnimateClose();

  // Closes the InfoBar immediately and removes it from its container. Notifies
  // the delegate that it has closed. The InfoBar is deleted after this function
  // is called.
  void Close();

 protected:
  // Removes our associated InfoBarDelegate from the associated TabContents.
  // (Will lead to this InfoBar being closed).
  void RemoveInfoBar() const;

 private:
  // The top level GTK widget.
  OwnedWidgetGtk widget_;

  // The x that closes the bar.
  scoped_ptr<CustomDrawButton> close_button_;

  // The InfoBar's container
  InfoBarContainerGtk* container_;

  // The InfoBar's delegate.
  InfoBarDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(InfoBar);
};

#endif  // CHROME_BROWSER_GTK_INFOBAR_GTK_H_

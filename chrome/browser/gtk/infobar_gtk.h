// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_INFOBAR_GTK_H_
#define CHROME_BROWSER_GTK_INFOBAR_GTK_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/gtk/slide_animator_gtk.h"
#include "chrome/common/owned_widget_gtk.h"

class CustomDrawButton;
class InfoBarContainerGtk;
class InfoBarDelegate;

class InfoBar : public SlideAnimatorGtk::Delegate {
 public:
  explicit InfoBar(InfoBarDelegate* delegate);
  virtual ~InfoBar();

  InfoBarDelegate* delegate() const { return delegate_; }

  // Get the top level native GTK widget for this infobar.
  GtkWidget* widget();

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

  // Returns true if the infobar is showing the close animation.
  bool IsClosing();

  // SlideAnimatorGtk::Delegate implementation.
  virtual void Closed();

 protected:
  // Removes our associated InfoBarDelegate from the associated TabContents.
  // (Will lead to this InfoBar being closed).
  void RemoveInfoBar() const;

  // The top level widget of the infobar.
  scoped_ptr<SlideAnimatorGtk> slide_widget_;

  // The second highest level widget of the infobar.
  OwnedWidgetGtk border_bin_;

  // The hbox that holds infobar elements (button, text, icon, etc.).
  GtkWidget* hbox_;

  // The x that closes the bar.
  scoped_ptr<CustomDrawButton> close_button_;

  // The InfoBar's container
  InfoBarContainerGtk* container_;

  // The InfoBar's delegate.
  InfoBarDelegate* delegate_;

 private:
  static void OnCloseButton(GtkWidget* button, InfoBar* info_bar);

  DISALLOW_COPY_AND_ASSIGN(InfoBar);
};

#endif  // CHROME_BROWSER_GTK_INFOBAR_GTK_H_

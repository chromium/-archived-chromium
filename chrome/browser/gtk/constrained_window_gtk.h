// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CONSTRAINED_WINDOW_GTK_H_
#define CHROME_BROWSER_GTK_CONSTRAINED_WINDOW_GTK_H_

#include "chrome/browser/tab_contents/constrained_window.h"

#include "base/basictypes.h"
#include "chrome/common/owned_widget_gtk.h"

class TabContents;
class TabContentsViewGtk;
typedef struct _GtkWidget GtkWidget;

class ConstrainedWindowGtkDelegate {
 public:
  // Returns the widget that will be put in the constrained window's container.
  virtual GtkWidget* GetWidgetRoot() = 0;

  // Tells the delegate to either delete itself or set up a task to delete
  // itself later.
  virtual void DeleteDelegate() = 0;
};

// Constrained window implementation for the GTK port. Unlike the Win32 system,
// ConstrainedWindowGtk doesn't draw draggable fake windows and instead just
// centers the dialog. It is thus an order of magnitude simpler.
class ConstrainedWindowGtk : public ConstrainedWindow {
 public:
  virtual ~ConstrainedWindowGtk();

  // Overridden from ConstrainedWindow:
  virtual void CloseConstrainedWindow();

  // Returns the TabContents that constrains this Constrained Window.
  TabContents* owner() const { return owner_; }

  // Returns the toplevel widget that displays this "window".
  GtkWidget* widget() { return border_.get(); }

  // Returns the View that we collaborate with to position ourselves.
  TabContentsViewGtk* ContainingView();

 private:
  friend class ConstrainedWindow;

  ConstrainedWindowGtk(TabContents* owner,
                       ConstrainedWindowGtkDelegate* delegate);

  // The TabContents that owns and constrains this ConstrainedWindow.
  TabContents* owner_;

  // The top level widget container that exports to our TabContentsViewGtk.
  OwnedWidgetGtk border_;

  // Delegate that provides the contents of this constrained window.
  ConstrainedWindowGtkDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowGtk);
};

#endif  // CHROME_BROWSER_GTK_CONSTRAINED_WINDOW_GTK_H_

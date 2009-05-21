// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_STATUS_BUBBLE_GTK_H_
#define CHROME_BROWSER_GTK_STATUS_BUBBLE_GTK_H_

#include <gtk/gtk.h>

#include <string>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/status_bubble.h"
#include "chrome/common/owned_widget_gtk.h"

class GURL;

// GTK implementation of StatusBubble. Unlike Windows, our status bubble
// doesn't have the nice leave-the-window effect since we can't rely on the
// window manager to not try to be "helpful" and center our popups, etc.
// We therefore position it absolutely in a GtkFixed, that we don't own.
class StatusBubbleGtk : public StatusBubble {
 public:
  StatusBubbleGtk();
  virtual ~StatusBubbleGtk();

  // StatusBubble implementation.
  virtual void SetStatus(const std::wstring& status);
  virtual void SetURL(const GURL& url, const std::wstring& languages);
  virtual void Hide();
  virtual void MouseMoved();

  void SetStatus(const std::string& status_utf8);

  // Notification from our parent GtkFixed about its size. |allocation| is the
  // size of our |parent| GtkFixed, and we use it to position our status bubble
  // directly on top of the current render view.
  void SetParentAllocation(GtkWidget* parent, GtkAllocation* allocation);

  // Top of the widget hierarchy for a StatusBubble. This top level widget is
  // guarenteed to have its gtk_widget_name set to "status-bubble" for
  // identification.
  GtkWidget* widget() { return container_.get(); }

 private:
  // Sets the status bubble's location in the parent GtkFixed, shows the widget
  // and makes sure that the status bubble has the highest z-order.
  void Show();

  // Sets an internal timer to hide the status bubble after a delay.
  void HideInASecond();

  // Builds the widgets, containers, etc.
  void InitWidgets();

  // An ad hoc, informally-specified, bug-ridden, slow implementation of half
  // of GTK's requisition/allocation system. We use this to position the status
  // bubble on top of our parent GtkFixed.
  void SetStatusBubbleSize();

  // A GtkAlignment that is the child of |slide_widget_|.
  OwnedWidgetGtk container_;

  // The GtkLabel holding the text.
  GtkWidget* label_;

  // Our parent GtkFixed. (We don't own this; we only keep a reference as we
  // set our own size by notifying |parent_| of our desired size.)
  GtkWidget* parent_;

  // |parent_|'s GtkAllocation. We make sure that |container_| lives along the
  // bottom of this and doesn't protrude.
  GtkAllocation parent_allocation_;

  // A timer that hides our window after a delay.
  ScopedRunnableMethodFactory<StatusBubbleGtk> timer_factory_;
};

#endif  // CHROME_BROWSER_GTK_STATUS_BUBBLE_GTK_H_

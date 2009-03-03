// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_STATUS_BUBBLE_GTK_H_
#define CHROME_BROWSER_GTK_STATUS_BUBBLE_GTK_H_

#include <string>

#include <gtk/gtk.h>

#include "chrome/browser/status_bubble.h"

class GURL;

class StatusBubbleGtk : public StatusBubble {
 public:
  StatusBubbleGtk(GtkWindow* parent);
  virtual ~StatusBubbleGtk();

  // StatusBubble implementation.
  virtual void SetStatus(const std::wstring& status);
  virtual void SetURL(const GURL& url, const std::wstring& languages);
  virtual void Hide();
  virtual void MouseMoved();

  void SetStatus(const std::string& status_utf8);

 private:
  // Construct the window/widget.
  void Create();

  // Reposition ourselves atop our parent window.
  void Reposition();

  // The window we display on top of.
  GtkWindow* parent_;

  // The top-level (popup) window we own.
  // NULL when we're not showing.
  GtkWidget* window_;

  // The GtkLabel holding the text./
  GtkWidget* label_;
};

#endif  // #ifndef CHROME_BROWSER_GTK_STATUS_BUBBLE_GTK_H_

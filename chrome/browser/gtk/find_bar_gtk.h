// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_FIND_BAR_GTK_H_
#define CHROME_BROWSER_GTK_FIND_BAR_GTK_H_

#include <string>

#include <gtk/gtk.h>

class TabContentsContainerGtk;
class WebContents;

// Currently this class contains both a model and a view.  We may want to
// eventually pull out the model specific bits and share with Windows.
class FindBarGtk {
 public:
  FindBarGtk();
  ~FindBarGtk() { }

  // Show the find dialog if it's not already showing.  The Find dialog is
  // positioned above the web contents area (TabContentsContainerGtk).
  void Show();
  void Hide();

  // Callback when the text in the find box changes.
  void ContentsChanged();

  // Callback from BrowserWindowGtk when the web contents changes.
  void ChangeWebContents(WebContents* contents);

  GtkWidget* gtk_widget() const { return container_; }

 private:
  // GtkHBox containing the find bar widgets.
  GtkWidget* container_;

  // The widget where text is entered.
  GtkWidget* find_text_;

  // A non-owning pointer to the web contents associated with the find bar.
  // This can be NULL.
  WebContents* web_contents_;
};

#endif  // CHROME_BROWSER_GTK_FIND_BAR_GTK_H_

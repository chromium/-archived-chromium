// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/find_bar_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "base/string_util.h"
#include "chrome/browser/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/tab_contents/web_contents.h"

namespace {

gboolean EntryContentsChanged(GtkWindow* window, FindBarGtk* find_bar) {
  find_bar->ContentsChanged();
  return FALSE;
}

gboolean KeyPressEvent(GtkWindow* window, GdkEventKey* event,
                       FindBarGtk* find_bar) {
  if (GDK_Escape == event->keyval)
    find_bar->Hide();
  return FALSE;
}

}

FindBarGtk::FindBarGtk() : web_contents_(NULL) {
  find_text_ = gtk_entry_new();
  gtk_widget_show(find_text_);

  container_ = gtk_hbox_new(false, 2);
  gtk_box_pack_end(GTK_BOX(container_), find_text_, FALSE, FALSE, 0);

  g_signal_connect(G_OBJECT(find_text_), "changed", 
                   G_CALLBACK(EntryContentsChanged), this);
  g_signal_connect(G_OBJECT(find_text_), "key-press-event", 
                   G_CALLBACK(KeyPressEvent), this);
}

void FindBarGtk::Show() {
  // TODO(tc): This should be an animated slide in.
  gtk_widget_show(container_);
  gtk_widget_grab_focus(find_text_);
}

void FindBarGtk::Hide() {
  gtk_widget_hide(container_);
  if (web_contents_)
    web_contents_->StopFinding(true);
}

void FindBarGtk::ContentsChanged() {
  if (!web_contents_)
    return;

  std::string new_contents(gtk_entry_get_text(GTK_ENTRY(find_text_)));

  if (new_contents.length() > 0) {
    web_contents_->StartFinding(UTF8ToWide(new_contents), true);
  } else {
    // The textbox is empty so we reset.
    web_contents_->StopFinding(true);  // true = clear selection on page.
  }
}

void FindBarGtk::ChangeWebContents(WebContents* contents) {
  web_contents_ = contents;
}

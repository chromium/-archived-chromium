// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/find_bar_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "base/string_util.h"
#include "chrome/browser/find_bar_controller.h"
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
    find_bar->EscapePressed();
  return FALSE;
}

}

FindBarGtk::FindBarGtk() {
  // TODO(tc): Pull out widget creation into an Init() method.
  find_text_ = gtk_entry_new();
  gtk_widget_show(find_text_);

  container_.Own(gtk_hbox_new(false, 2));
  gtk_box_pack_end(GTK_BOX(container_.get()), find_text_, FALSE, FALSE, 0);

  g_signal_connect(G_OBJECT(find_text_), "changed", 
                   G_CALLBACK(EntryContentsChanged), this);
  g_signal_connect(G_OBJECT(find_text_), "key-press-event", 
                   G_CALLBACK(KeyPressEvent), this);
}

FindBarGtk::~FindBarGtk() {
  container_.Destroy();
}

void FindBarGtk::Show() {
  // TODO(tc): This should be an animated slide in.
  gtk_widget_show(container_.get());
  gtk_widget_grab_focus(find_text_);
}

void FindBarGtk::Hide(bool animate) {
  // TODO(tc): Animated slide away.
  gtk_widget_hide(container_.get());
}

void FindBarGtk::SetFocusAndSelection() {
}

void FindBarGtk::ClearResults(const FindNotificationDetails& results) {
}

void FindBarGtk::StopAnimation() {
  // No animation yet, so do nothing.
}

void FindBarGtk::SetFindText(const string16& find_text) {
}

void FindBarGtk::UpdateUIForFindResult(const FindNotificationDetails& result,
                                       const string16& find_text) {
}

gfx::Rect FindBarGtk::GetDialogPosition(gfx::Rect avoid_overlapping_rect) {
  return gfx::Rect();
}

void FindBarGtk::SetDialogPosition(const gfx::Rect& new_pos, bool no_redraw) {
}

bool FindBarGtk::IsFindBarVisible() {
  return GTK_WIDGET_VISIBLE(container_.get());
}

void FindBarGtk::RestoreSavedFocus() {
}

void FindBarGtk::ContentsChanged() {
  WebContents* web_contents = find_bar_controller_->web_contents();
  if (!web_contents)
    return;

  std::string new_contents(gtk_entry_get_text(GTK_ENTRY(find_text_)));

  if (new_contents.length() > 0) {
    web_contents->StartFinding(UTF8ToUTF16(new_contents), true);
  } else {
    // The textbox is empty so we reset.
    web_contents->StopFinding(true);  // true = clear selection on page.
  }
}

void FindBarGtk::EscapePressed() {
  find_bar_controller_->EndFindSession();
}

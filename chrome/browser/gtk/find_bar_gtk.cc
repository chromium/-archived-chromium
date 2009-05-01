// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/find_bar_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "base/gfx/gtk_util.h"
#include "base/string_util.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/gtk/slide_animator_gtk.h"
#include "chrome/browser/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/l10n_util.h"
#include "grit/generated_resources.h"

namespace {

const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xe6, 0xed, 0xf4);
const GdkColor kBorderColor = GDK_COLOR_RGB(0xbe, 0xc8, 0xd4);

// Padding around the container.
const int kBarPadding = 4;

// The height of the findbar dialog, as dictated by the size of the background
// images.
const int kFindBarHeight = 32;

gboolean EntryContentsChanged(GtkWindow* window, FindBarGtk* find_bar) {
  find_bar->ContentsChanged();
  return FALSE;
}

gboolean KeyPressEvent(GtkWindow* window, GdkEventKey* event,
                       FindBarGtk* find_bar) {
  if (GDK_Escape == event->keyval) {
    find_bar->EscapePressed();
    return TRUE;
  }
  return FALSE;
}

// Get the ninebox that draws the background of |container_|. It is also used
// to change the shape of |container_|. The pointer is shared by all instances
// of FindBarGtk.
const NineBox* GetDialogBackground() {
  static NineBox* dialog_background = NULL;
  if (!dialog_background) {
    dialog_background = new NineBox(
      IDR_FIND_DLG_LEFT_BACKGROUND,
      IDR_FIND_DLG_MIDDLE_BACKGROUND,
      IDR_FIND_DLG_RIGHT_BACKGROUND,
      NULL, NULL, NULL, NULL, NULL, NULL);
    dialog_background->ChangeWhiteToTransparent();
  }

  return dialog_background;
}

// Used to handle custom painting of |container_|.
gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e, gpointer userdata) {
  GetDialogBackground()->RenderToWidget(widget);
  GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
  if (child)
    gtk_container_propagate_expose(GTK_CONTAINER(widget), child, e);
  return TRUE;
}

}  // namespace

FindBarGtk::FindBarGtk(BrowserWindowGtk* browser)
    : container_shaped_(false) {
  InitWidgets();

  // Insert the widget into the browser gtk hierarchy.
  browser->AddFindBar(this);

  // Hook up signals after the widget has been added to the hierarchy so the
  // widget will be realized.
  g_signal_connect(find_text_, "changed",
                   G_CALLBACK(EntryContentsChanged), this);
  g_signal_connect(find_text_, "key-press-event",
                   G_CALLBACK(KeyPressEvent), this);
  g_signal_connect(widget(), "size-allocate",
                   G_CALLBACK(OnFixedSizeAllocate), this);
  // We can't call ContourWidget() until after |container_| has been
  // allocated, hence we connect to this signal.
  g_signal_connect(container_, "size-allocate",
                   G_CALLBACK(OnContainerSizeAllocate), this);
  g_signal_connect(container_, "expose-event",
                   G_CALLBACK(OnExpose), NULL);
}

FindBarGtk::~FindBarGtk() {
  fixed_.Destroy();
}

void FindBarGtk::InitWidgets() {
  // The find bar is basically an hbox with a gtkentry (text box) followed by 3
  // buttons (previous result, next result, close).  We wrap the hbox in a gtk
  // alignment and a gtk event box to get the padding and light blue
  // background. We put that event box in a fixed in order to control its
  // lateral position. We put that fixed in a SlideAnimatorGtk in order to get
  // the slide effect.
  GtkWidget* hbox = gtk_hbox_new(false, 0);
  container_ = gfx::CreateGtkBorderBin(hbox, &kBackgroundColor,
      kBarPadding, kBarPadding, kBarPadding, kBarPadding);
  gtk_widget_set_app_paintable(container_, TRUE);

  slide_widget_.reset(new SlideAnimatorGtk(container_,
                                           SlideAnimatorGtk::DOWN,
                                           0, false, NULL));

  // |fixed_| has to be at least one pixel tall. We color this pixel the same
  // color as the border that separates the toolbar from the web contents.
  fixed_.Own(gtk_fixed_new());
  border_ = gtk_event_box_new();
  gtk_widget_set_size_request(border_, 1, 1);
  gtk_widget_modify_bg(border_, GTK_STATE_NORMAL, &kBorderColor);

  gtk_fixed_put(GTK_FIXED(widget()), border_, 0, 0);
  gtk_fixed_put(GTK_FIXED(widget()), slide_widget(), 0, 0);
  gtk_widget_set_size_request(widget(), -1, 0);

  close_button_.reset(CustomDrawButton::AddBarCloseButton(hbox));
  g_signal_connect(G_OBJECT(close_button_->widget()), "clicked",
                   G_CALLBACK(OnButtonPressed), this);
  gtk_widget_set_tooltip_text(close_button_->widget(),
      l10n_util::GetStringUTF8(IDS_FIND_IN_PAGE_CLOSE_TOOLTIP).c_str());

  find_next_button_.reset(new CustomDrawButton(IDR_FINDINPAGE_NEXT,
      IDR_FINDINPAGE_NEXT_H, IDR_FINDINPAGE_NEXT_H, IDR_FINDINPAGE_NEXT_P));
  g_signal_connect(G_OBJECT(find_next_button_->widget()), "clicked",
                   G_CALLBACK(OnButtonPressed), this);
  gtk_widget_set_tooltip_text(find_next_button_->widget(),
      l10n_util::GetStringUTF8(IDS_FIND_IN_PAGE_NEXT_TOOLTIP).c_str());
  gtk_box_pack_end(GTK_BOX(hbox), find_next_button_->widget(),
                   FALSE, FALSE, 0);

  find_previous_button_.reset(new CustomDrawButton(IDR_FINDINPAGE_PREV,
      IDR_FINDINPAGE_PREV_H, IDR_FINDINPAGE_PREV_H, IDR_FINDINPAGE_PREV_P));
  g_signal_connect(G_OBJECT(find_previous_button_->widget()), "clicked",
                   G_CALLBACK(OnButtonPressed), this);
  gtk_widget_set_tooltip_text(find_previous_button_->widget(),
      l10n_util::GetStringUTF8(IDS_FIND_IN_PAGE_PREVIOUS_TOOLTIP).c_str());
  gtk_box_pack_end(GTK_BOX(hbox), find_previous_button_->widget(),
                   FALSE, FALSE, 0);

  find_text_ = gtk_entry_new();
  // Force the text widget height so it lines up with the buttons regardless of
  // font size.
  gtk_widget_set_size_request(find_text_, -1, 20);
  gtk_entry_set_has_frame(GTK_ENTRY(find_text_), FALSE);
  GtkWidget* border_bin = gfx::CreateGtkBorderBin(find_text_, &kBorderColor,
                                                  1, 1, 1, 0);
  GtkWidget* centering_vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(centering_vbox), border_bin, TRUE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(hbox), centering_vbox, FALSE, FALSE, 0);

  // We show just the GtkFixed and |border_| (but not the dialog).
  gtk_widget_show(widget());
  gtk_widget_show(border_);
}

GtkWidget* FindBarGtk::slide_widget() {
  return slide_widget_->widget();
}

void FindBarGtk::Show() {
  AssureOnTop();
  gtk_widget_show_all(slide_widget());
  gtk_widget_grab_focus(find_text_);
  slide_widget_->Open();
}

void FindBarGtk::Hide(bool animate) {
  if (animate)
    slide_widget_->Close();
  else
    gtk_widget_hide(slide_widget());
}

void FindBarGtk::SetFocusAndSelection() {
  gtk_widget_grab_focus(find_text_);
  // Select all the text.
  gtk_entry_select_region(GTK_ENTRY(find_text_), 0, -1);
}

void FindBarGtk::ClearResults(const FindNotificationDetails& results) {
}

void FindBarGtk::StopAnimation() {
  NOTIMPLEMENTED();
}

void FindBarGtk::MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                       bool no_redraw) {
  // Not moving the window on demand, so do nothing.
}

void FindBarGtk::SetFindText(const string16& find_text) {
  std::string find_text_utf8 = UTF16ToUTF8(find_text);
  gtk_entry_set_text(GTK_ENTRY(find_text_), find_text_utf8.c_str());
}

void FindBarGtk::UpdateUIForFindResult(const FindNotificationDetails& result,
                                       const string16& find_text) {
}

gfx::Rect FindBarGtk::GetDialogPosition(gfx::Rect avoid_overlapping_rect) {
  // TODO(estade): Logic for the positioning of the find bar should be factored
  // out of here and browser/views/* and into FindBarController.
  int xposition = widget()->allocation.width -
                  slide_widget()->allocation.width - 50;

  return gfx::Rect(xposition, 0, 1, 1);
}

void FindBarGtk::SetDialogPosition(const gfx::Rect& new_pos, bool no_redraw) {
  gtk_fixed_move(GTK_FIXED(widget()), slide_widget(), new_pos.x(), 0);
  gtk_widget_show_all(slide_widget());
}

bool FindBarGtk::IsFindBarVisible() {
  return GTK_WIDGET_VISIBLE(widget());
}

void FindBarGtk::RestoreSavedFocus() {
}

FindBarTesting* FindBarGtk::GetFindBarTesting() {
  return this;
}

bool FindBarGtk::GetFindBarWindowInfo(gfx::Point* position,
                                      bool* fully_visible) {
  NOTIMPLEMENTED();
  return false;
}

void FindBarGtk::AssureOnTop() {
  if (container_->window)
    gdk_window_raise(container_->window);
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

// static
void FindBarGtk::OnButtonPressed(GtkWidget* button, FindBarGtk* find_bar) {
  if (button == find_bar->close_button_->widget()) {
    find_bar->find_bar_controller_->EndFindSession();
  } else if (button == find_bar->find_previous_button_->widget() ||
             button == find_bar->find_next_button_->widget()) {
    std::string find_text_utf8(
        gtk_entry_get_text(GTK_ENTRY(find_bar->find_text_)));
    find_bar->find_bar_controller_->web_contents()->StartFinding(
        UTF8ToUTF16(find_text_utf8),
        button == find_bar->find_next_button_->widget());
  } else {
    NOTREACHED();
  }
}

// static
void FindBarGtk::OnFixedSizeAllocate(GtkWidget* fixed,
                                     GtkAllocation* allocation,
                                     FindBarGtk* findbar) {
  // Set the background widget to the size of |fixed|.
  if (findbar->border_->allocation.width != allocation->width) {
    // Typically it's not a good idea to use this function outside of container
    // implementations, but GtkFixed doesn't do any sizing on its children so
    // in this case it's safe.
    gtk_widget_size_allocate(findbar->border_, allocation);
  }

  // Reposition the dialog.
  GtkWidget* dialog = findbar->slide_widget();
  if (!GTK_WIDGET_VISIBLE(dialog))
    return;

  int xposition = findbar->GetDialogPosition(gfx::Rect()).x();
  if (xposition == dialog->allocation.x) {
    return;
  } else {
    gtk_fixed_move(GTK_FIXED(fixed), findbar->slide_widget(), xposition, 0);
  }
}

// static
void FindBarGtk::OnContainerSizeAllocate(GtkWidget* container,
                                         GtkAllocation* allocation,
                                         FindBarGtk* findbar) {
  if (!findbar->container_shaped_) {
    GetDialogBackground()->ContourWidget(container);
    findbar->container_shaped_ = true;
  }
}

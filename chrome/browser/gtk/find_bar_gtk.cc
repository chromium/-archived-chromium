// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/find_bar_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "app/l10n_util.h"
#include "base/gfx/gtk_util.h"
#include "base/string_util.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/gtk/slide_animator_gtk.h"
#include "chrome/browser/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "grit/generated_resources.h"
#include "webkit/api/public/gtk/WebInputEventFactory.h"

namespace {

const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xe6, 0xed, 0xf4);
const GdkColor kFrameBorderColor = GDK_COLOR_RGB(0xbe, 0xc8, 0xd4);
const GdkColor kTextBorderColor = GDK_COLOR_RGB(0xa6, 0xaf, 0xba);
const GdkColor kTextBorderColorAA = GDK_COLOR_RGB(0xee, 0xf4, 0xfb);

// Padding around the container.
const int kBarPaddingTopBottom = 4;
const int kEntryPaddingLeft = 6;
const int kCloseButtonPaddingLeft = 3;
const int kBarPaddingRight = 4;

// The height of the findbar dialog, as dictated by the size of the background
// images.
const int kFindBarHeight = 32;

// The width of the text entry field.
const int kTextEntryWidth = 220;

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
  changed_handler_id_ = g_signal_connect(text_entry_, "changed",
                                         G_CALLBACK(OnChanged), this);
  g_signal_connect(text_entry_, "key-press-event",
                   G_CALLBACK(OnKeyPressEvent), this);
  g_signal_connect(text_entry_, "key-release-event",
                   G_CALLBACK(OnKeyReleaseEvent), this);
  // When the user tabs to us or clicks on us, save where the focus used to
  // be.
  g_signal_connect(text_entry_, "focus",
                   G_CALLBACK(OnFocus), this);
  gtk_widget_add_events(text_entry_, GDK_BUTTON_PRESS_MASK);
  g_signal_connect(text_entry_, "button-press-event",
                   G_CALLBACK(OnButtonPress), this);
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
  container_ = gtk_util::CreateGtkBorderBin(hbox, NULL,
      kBarPaddingTopBottom, kBarPaddingTopBottom,
      kEntryPaddingLeft, kBarPaddingRight);
  gtk_widget_set_app_paintable(container_, TRUE);

  slide_widget_.reset(new SlideAnimatorGtk(container_,
                                           SlideAnimatorGtk::DOWN,
                                           0, false, NULL));

  // |fixed_| has to be at least one pixel tall. We color this pixel the same
  // color as the border that separates the toolbar from the tab contents.
  fixed_.Own(gtk_fixed_new());
  border_ = gtk_event_box_new();
  gtk_widget_set_size_request(border_, 1, 1);
  gtk_widget_modify_bg(border_, GTK_STATE_NORMAL, &kFrameBorderColor);

  gtk_fixed_put(GTK_FIXED(widget()), border_, 0, 0);
  gtk_fixed_put(GTK_FIXED(widget()), slide_widget(), 0, 0);
  gtk_widget_set_size_request(widget(), -1, 0);

  close_button_.reset(CustomDrawButton::CloseButton());
  gtk_util::CenterWidgetInHBox(hbox, close_button_->widget(), true,
                               kCloseButtonPaddingLeft);
  g_signal_connect(G_OBJECT(close_button_->widget()), "clicked",
                   G_CALLBACK(OnClicked), this);
  gtk_widget_set_tooltip_text(close_button_->widget(),
      l10n_util::GetStringUTF8(IDS_FIND_IN_PAGE_CLOSE_TOOLTIP).c_str());

  find_next_button_.reset(new CustomDrawButton(IDR_FINDINPAGE_NEXT,
      IDR_FINDINPAGE_NEXT_H, IDR_FINDINPAGE_NEXT_H, IDR_FINDINPAGE_NEXT_P));
  g_signal_connect(G_OBJECT(find_next_button_->widget()), "clicked",
                   G_CALLBACK(OnClicked), this);
  gtk_widget_set_tooltip_text(find_next_button_->widget(),
      l10n_util::GetStringUTF8(IDS_FIND_IN_PAGE_NEXT_TOOLTIP).c_str());
  gtk_box_pack_end(GTK_BOX(hbox), find_next_button_->widget(),
                   FALSE, FALSE, 0);

  find_previous_button_.reset(new CustomDrawButton(IDR_FINDINPAGE_PREV,
      IDR_FINDINPAGE_PREV_H, IDR_FINDINPAGE_PREV_H, IDR_FINDINPAGE_PREV_P));
  g_signal_connect(G_OBJECT(find_previous_button_->widget()), "clicked",
                   G_CALLBACK(OnClicked), this);
  gtk_widget_set_tooltip_text(find_previous_button_->widget(),
      l10n_util::GetStringUTF8(IDS_FIND_IN_PAGE_PREVIOUS_TOOLTIP).c_str());
  gtk_box_pack_end(GTK_BOX(hbox), find_previous_button_->widget(),
                   FALSE, FALSE, 0);

  // Make a box for the edit and match count widgets. This is fixed size since
  // we want the widgets inside to resize themselves rather than making the
  // dialog bigger.
  GtkWidget* content_hbox = gtk_hbox_new(false, 0);
  gtk_widget_set_size_request(content_hbox, kTextEntryWidth, -1);

  text_entry_ = gtk_entry_new();
  match_count_label_ = gtk_label_new(NULL);

  // Force the text widget height so it lines up with the buttons regardless of
  // font size.
  gtk_widget_set_size_request(text_entry_, -1, 20);
  gtk_entry_set_has_frame(GTK_ENTRY(text_entry_), FALSE);
  // Until we switch to vector graphics, force the font size.
  gtk_util::ForceFontSizePixels(text_entry_, 13.4);  // 13.4px == 10pt @ 96dpi
  gtk_util::ForceFontSizePixels(match_count_label_, 13.4);

  gtk_box_pack_end(GTK_BOX(content_hbox), match_count_label_, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(content_hbox), text_entry_, TRUE, TRUE, 0);

  // We fake anti-aliasing by having two borders.
  GtkWidget* border_bin = gtk_util::CreateGtkBorderBin(content_hbox,
                                                       &kTextBorderColor,
                                                      1, 1, 1, 0);
  GtkWidget* border_bin_aa = gtk_util::CreateGtkBorderBin(border_bin,
                                                          &kTextBorderColorAA,
                                                          1, 1, 1, 0);
  gtk_util::CenterWidgetInHBox(hbox, border_bin_aa, true, 0);

  // We take care to avoid showing the slide animator widget.
  gtk_widget_show_all(container_);
  gtk_widget_show(widget());
  gtk_widget_show(border_);
}

GtkWidget* FindBarGtk::slide_widget() {
  return slide_widget_->widget();
}

void FindBarGtk::Show() {
  slide_widget_->Open();
  if (container_->window)
    gdk_window_raise(container_->window);
}

void FindBarGtk::Hide(bool animate) {
  if (animate)
    slide_widget_->Close();
  else
    slide_widget_->CloseWithoutAnimation();
}

void FindBarGtk::SetFocusAndSelection() {
  StoreOutsideFocus();
  gtk_widget_grab_focus(text_entry_);
  // Select all the text.
  gtk_entry_select_region(GTK_ENTRY(text_entry_), 0, -1);
}

void FindBarGtk::ClearResults(const FindNotificationDetails& results) {
  UpdateUIForFindResult(results, string16());
}

void FindBarGtk::StopAnimation() {
  // http://code.google.com/p/chromium/issues/detail?id=11979
}

void FindBarGtk::MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                       bool no_redraw) {
  // Not moving the window on demand, so do nothing.
}

void FindBarGtk::SetFindText(const string16& find_text) {
  std::string text_entry_utf8 = UTF16ToUTF8(find_text);

  // Unhook the "changed" signal handler because programatically setting the
  // text should not fire a "changed" event.
  g_signal_handler_disconnect(text_entry_, changed_handler_id_);
  gtk_entry_set_text(GTK_ENTRY(text_entry_), text_entry_utf8.c_str());
  changed_handler_id_ = g_signal_connect(text_entry_, "changed",
                                         G_CALLBACK(OnChanged), this);
}

void FindBarGtk::UpdateUIForFindResult(const FindNotificationDetails& result,
                                       const string16& find_text) {
  // Once we find a match we no longer want to keep track of what had
  // focus. EndFindSession will then set the focus to the page content.
  if (result.number_of_matches() > 0)
    focus_store_.Store(NULL);

  std::string text_entry_utf8 = UTF16ToUTF8(find_text);
  bool have_valid_range =
      result.number_of_matches() != -1 && result.active_match_ordinal() != -1;

  // If we don't have any results and something was passed in, then that means
  // someone pressed F3 while the Find box was closed. In that case we need to
  // repopulate the Find box with what was passed in.
  std::string search_string(gtk_entry_get_text(GTK_ENTRY(text_entry_)));
  if (search_string.empty() && !text_entry_utf8.empty()) {
    gtk_entry_set_text(GTK_ENTRY(text_entry_), text_entry_utf8.c_str());
    gtk_entry_select_region(GTK_ENTRY(text_entry_), 0, -1);
  }

  if (!search_string.empty() && have_valid_range) {
    gtk_label_set_text(GTK_LABEL(match_count_label_),
        l10n_util::GetStringFUTF8(IDS_FIND_IN_PAGE_COUNT,
            IntToString16(result.active_match_ordinal()),
            IntToString16(result.number_of_matches())).c_str());
  } else {
    // If there was no text entered, we don't show anything in the result count
    // area.
    gtk_label_set_text(GTK_LABEL(match_count_label_), "");
  }

  // TODO(brettw) We should update the background color of the text field to
  // reflect whether any text was found. See the Windows version for how.

  // TODO(brettw) enable or disable the find next/previous buttons depending
  // on whether any matches were found.
}

void FindBarGtk::AudibleAlert() {
  gtk_widget_error_bell(widget());
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
  slide_widget_->OpenWithoutAnimation();
}

bool FindBarGtk::IsFindBarVisible() {
  return GTK_WIDGET_VISIBLE(widget());
}

void FindBarGtk::RestoreSavedFocus() {
  // This function sometimes gets called when we don't have focus. We should do
  // nothing in this case.
  if (!gtk_widget_is_focus(text_entry_))
    return;

  if (focus_store_.widget())
    gtk_widget_grab_focus(focus_store_.widget());
  else
    find_bar_controller_->tab_contents()->Focus();
}

FindBarTesting* FindBarGtk::GetFindBarTesting() {
  return this;
}

bool FindBarGtk::GetFindBarWindowInfo(gfx::Point* position,
                                      bool* fully_visible) {
  NOTIMPLEMENTED();
  return false;
}

void FindBarGtk::FindEntryTextInContents(bool forward_search) {
  TabContents* tab_contents = find_bar_controller_->tab_contents();
  if (!tab_contents)
    return;

  std::string new_contents(gtk_entry_get_text(GTK_ENTRY(text_entry_)));

  if (new_contents.length() > 0) {
    tab_contents->StartFinding(UTF8ToUTF16(new_contents), forward_search,
                               false);  // Not case sensitive.
  } else {
    // The textbox is empty so we reset.
    tab_contents->StopFinding(true);  // true = clear selection on page.
    UpdateUIForFindResult(find_bar_controller_->tab_contents()->find_result(),
                          string16());
  }
}

void FindBarGtk::StoreOutsideFocus() {
  // |text_entry_| is the only widget in the find bar that can be focused,
  // so it's the only one we have to check.
  // TODO(estade): when we make the find bar buttons focusable, we'll have
  // to change this (same above in RestoreSavedFocus).
  if (!gtk_widget_is_focus(text_entry_))
    focus_store_.Store(text_entry_);
}

bool FindBarGtk::MaybeForwardKeyEventToRenderer(GdkEventKey* event) {
  switch (event->keyval) {
    case GDK_Down:
    case GDK_Up:
    case GDK_Page_Up:
    case GDK_Page_Down:
      break;
    case GDK_Home:
    case GDK_End:
      if ((event->state & gtk_accelerator_get_default_mod_mask()) ==
          GDK_CONTROL_MASK) {
        break;
      }
    // Fall through.
    default:
      return false;
  }

  TabContents* contents = find_bar_controller_->tab_contents();
  if (!contents)
    return false;

  RenderViewHost* render_view_host = contents->render_view_host();

  // Make sure we don't have a text field element interfering with keyboard
  // input. Otherwise Up and Down arrow key strokes get eaten. "Nom Nom Nom".
  render_view_host->ClearFocusedNode();

  NativeWebKeyboardEvent wke(event);
  render_view_host->ForwardKeyboardEvent(wke);
  return true;
}

// static
gboolean FindBarGtk::OnChanged(GtkWindow* window, FindBarGtk* find_bar) {
  find_bar->FindEntryTextInContents(true);
  return FALSE;
}

// static
gboolean FindBarGtk::OnKeyPressEvent(GtkWidget* widget, GdkEventKey* event,
                                     FindBarGtk* find_bar) {
  if (find_bar->MaybeForwardKeyEventToRenderer(event)) {
    return TRUE;
  } else if (GDK_Escape == event->keyval) {
    find_bar->find_bar_controller_->EndFindSession();
    return TRUE;
  } else if (GDK_Return == event->keyval) {
    find_bar->FindEntryTextInContents(true);
    return TRUE;
  }
  return FALSE;
}

// static
gboolean FindBarGtk::OnKeyReleaseEvent(GtkWidget* widget, GdkEventKey* event,
                                       FindBarGtk* find_bar) {
  return find_bar->MaybeForwardKeyEventToRenderer(event);
}

// static
void FindBarGtk::OnClicked(GtkWidget* button, FindBarGtk* find_bar) {
  if (button == find_bar->close_button_->widget()) {
    find_bar->find_bar_controller_->EndFindSession();
  } else if (button == find_bar->find_previous_button_->widget() ||
             button == find_bar->find_next_button_->widget()) {
    find_bar->FindEntryTextInContents(
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

// static
gboolean FindBarGtk::OnFocus(GtkWidget* text_entry, GtkDirectionType focus,
                             FindBarGtk* find_bar) {
  find_bar->StoreOutsideFocus();

  // Continue propagating the event.
  return FALSE;
}

// static
gboolean FindBarGtk::OnButtonPress(GtkWidget* text_entry, GdkEventButton* e,
                                   FindBarGtk* find_bar) {
  find_bar->StoreOutsideFocus();

  // Continue propagating the event.
  return FALSE;
}

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"

namespace {

const char kTextBaseColor[] = "#808080";
const char kSecureSchemeColor[] = "#009614";
const char kInsecureSchemeColor[] = "#009614";
const GdkColor kSecureBackgroundColor = GDK_COLOR_RGB(0xff, 0xf5, 0xc3);
const GdkColor kInsecureBackgroundColor = GDK_COLOR_RGB(0xff, 0xff, 0xff);

}  // namespace

AutocompleteEditViewGtk::AutocompleteEditViewGtk(
    AutocompleteEditController* controller,
    ToolbarModel* toolbar_model,
    Profile* profile,
    CommandUpdater* command_updater)
    : tag_table_(NULL),
      text_buffer_(NULL),
      base_tag_(NULL),
      secure_scheme_tag_(NULL),
      insecure_scheme_tag_(NULL),
      model_(new AutocompleteEditModel(this, controller, profile)),
      popup_view_(new AutocompletePopupViewGtk(this, model_.get(), profile)),
      controller_(controller),
      toolbar_model_(toolbar_model),
      command_updater_(command_updater),
      popup_window_mode_(false),  // TODO(deanm)
      scheme_security_level_(ToolbarModel::NORMAL) {
  model_->set_popup_model(popup_view_->model());
}

AutocompleteEditViewGtk::~AutocompleteEditViewGtk() {
  NotificationService::current()->Notify(
      NotificationType::AUTOCOMPLETE_EDIT_DESTROYED,
      Source<AutocompleteEditViewGtk>(this),
      NotificationService::NoDetails());

  // Explicitly teardown members which have a reference to us.  Just to be safe
  // we want them to be destroyed before destroying any other internal state.
  popup_view_.reset();
  model_.reset();

  // We own our widget and TextView related objects.
  if (text_view_.get()) {  // Init() has been called.
    text_view_.Destroy();
    g_object_unref(text_buffer_);
    g_object_unref(tag_table_);
    // The tags we created are owned by the tag_table, and should be destroyed
    // along with it.  We don't hold our own reference to them.
  }
}

void AutocompleteEditViewGtk::Init() {
  // The GtkTagTable and GtkTextBuffer are not initially unowned, so we have
  // our own reference when we create them, and we own them.  Adding them to
  // the other objects adds a reference; it doesn't adopt them.
  tag_table_ = gtk_text_tag_table_new();
  text_buffer_ = gtk_text_buffer_new(tag_table_);
  text_view_.Own(gtk_text_view_new_with_buffer(text_buffer_));

  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view_.get()), 4);
  gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view_.get()), 4);

  // TODO(deanm): This is a super lame attempt to vertically center our single
  // line of text in a multiline edit control.  Mannnn.
  gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(text_view_.get()), 4);

  // TODO(deanm): This will probably have to be handled differently with the
  // tab to search business.  Maybe we should just eat the tab characters.
  // We want the tab key to move focus, not insert a tab.
  gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(text_view_.get()), false);

  base_tag_ = gtk_text_buffer_create_tag(text_buffer_,
      NULL, "foreground", kTextBaseColor, NULL);
  secure_scheme_tag_ = gtk_text_buffer_create_tag(text_buffer_,
      NULL, "foreground", kSecureSchemeColor, NULL);
  insecure_scheme_tag_ = gtk_text_buffer_create_tag(text_buffer_,
      NULL, "foreground", kInsecureSchemeColor, NULL);

  // NOTE: This code used to connect to "changed", however this was fired too
  // often and during bad times (our own buffer changes?).  It works out much
  // better to listen to end-user-action, which should be fired whenever the
  // user makes some sort of change to the buffer.
  g_signal_connect(text_buffer_, "begin-user-action",
                   G_CALLBACK(&HandleBeginUserActionThunk), this);
  g_signal_connect(text_buffer_, "end-user-action",
                   G_CALLBACK(&HandleEndUserActionThunk), this);
  // We connect to key press and release for special handling of a few keys.
  g_signal_connect(text_view_.get(), "key-press-event",
                   G_CALLBACK(&HandleKeyPressThunk), this);
  g_signal_connect(text_view_.get(), "key-release-event",
                   G_CALLBACK(&HandleKeyReleaseThunk), this);
  g_signal_connect(text_view_.get(), "size-request",
                   G_CALLBACK(&HandleViewSizeRequest), this);
  g_signal_connect(text_view_.get(), "button-press-event",
                   G_CALLBACK(&HandleViewButtonPressThunk), this);
  g_signal_connect(text_view_.get(), "focus-in-event",
                   G_CALLBACK(&HandleViewFocusInThunk), this);
  g_signal_connect(text_view_.get(), "focus-out-event",
                   G_CALLBACK(&HandleViewFocusOutThunk), this);
  // NOTE: The GtkTextView documentation asks you not to connect to this
  // signal, but it is very convenient and clean for catching up/down.
  g_signal_connect(text_view_.get(), "move-cursor",
                   G_CALLBACK(&HandleViewMoveCursorThunk), this);
}

void AutocompleteEditViewGtk::SetFocus() {
  gtk_widget_grab_focus(text_view_.get());
}

void AutocompleteEditViewGtk::SaveStateToTab(TabContents* tab) {
  NOTIMPLEMENTED();
}

void AutocompleteEditViewGtk::Update(const TabContents* contents) {
  // NOTE: We're getting the URL text here from the ToolbarModel.
  bool visibly_changed_permanent_text =
      model_->UpdatePermanentText(toolbar_model_->GetText());

  ToolbarModel::SecurityLevel security_level =
        toolbar_model_->GetSchemeSecurityLevel();
  bool changed_security_level = (security_level != scheme_security_level_);
  scheme_security_level_ = security_level;

  if (contents) {
    RevertAll();
    // TODO(deanm): Tab switching.  The Windows code puts some state in a
    // PropertyBag on the tab contents, and restores state from there.
  } else if (visibly_changed_permanent_text) {
    RevertAll();
    // TODO(deanm): There should be code to restore select all here.
  } else if(changed_security_level) {
    EmphasizeURLComponents();
  }
}

void AutocompleteEditViewGtk::OpenURL(const GURL& url,
                                      WindowOpenDisposition disposition,
                                      PageTransition::Type transition,
                                      const GURL& alternate_nav_url,
                                      size_t selected_line,
                                      const std::wstring& keyword) {
  if (!url.is_valid())
    return;

  model_->SendOpenNotification(selected_line, keyword);

  if (disposition != NEW_BACKGROUND_TAB)
    RevertAll();  // Revert the box to its unedited state
  controller_->OnAutocompleteAccept(url, disposition, transition,
                                    alternate_nav_url);
}

std::wstring AutocompleteEditViewGtk::GetText() const {
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
  gchar* utf8 = gtk_text_buffer_get_text(text_buffer_, &start, &end, false);
  std::wstring out(UTF8ToWide(utf8));
  g_free(utf8);
  return out;
}

void AutocompleteEditViewGtk::SetUserText(const std::wstring& text,
                                          const std::wstring& display_text,
                                          bool update_popup) {
  model_->SetUserText(text);
  // TODO(deanm): something about selection / focus change here.
  SetWindowTextAndCaretPos(display_text, display_text.length());
  if (update_popup)
    UpdatePopup();
  TextChanged();
}

void AutocompleteEditViewGtk::SetWindowTextAndCaretPos(const std::wstring& text,
                                                       size_t caret_pos) {
  std::string utf8 = WideToUTF8(text);
  gtk_text_buffer_set_text(text_buffer_, utf8.data(), utf8.length());
  EmphasizeURLComponents();

  GtkTextIter cur_pos;
  gtk_text_buffer_get_iter_at_offset(text_buffer_, &cur_pos, caret_pos);
  gtk_text_buffer_place_cursor(text_buffer_, &cur_pos);
}

bool AutocompleteEditViewGtk::IsSelectAll() {
  NOTIMPLEMENTED();
  return false;
}

void AutocompleteEditViewGtk::SelectAll(bool reversed) {
  GtkTextIter start, end;
  if (reversed) {
    gtk_text_buffer_get_bounds(text_buffer_, &end, &start);
  } else {
    gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
  }
  gtk_text_buffer_place_cursor(text_buffer_, &start);
  gtk_text_buffer_select_range(text_buffer_, &start, &end);
}

void AutocompleteEditViewGtk::RevertAll() {
  ClosePopup();
  model_->Revert();
  TextChanged();
}

void AutocompleteEditViewGtk::UpdatePopup() {
  model_->SetInputInProgress(true);
  if (!model_->has_focus())
    return;

  // Don't inline autocomplete when the caret/selection isn't at the end of
  // the text.
  CharRange sel = GetSelection();
  model_->StartAutocomplete(sel.cp_max < GetTextLength());
}

void AutocompleteEditViewGtk::ClosePopup() {
  popup_view_->model()->StopAutocomplete();
}

void AutocompleteEditViewGtk::OnTemporaryTextMaybeChanged(
    const std::wstring& display_text,
    bool save_original_selection) {
  // TODO(deanm): Ignoring save_original_selection here, etc.
  SetWindowTextAndCaretPos(display_text, display_text.length());
  TextChanged();
}

bool AutocompleteEditViewGtk::OnInlineAutocompleteTextMaybeChanged(
    const std::wstring& display_text,
    size_t user_text_length) {
  if (display_text == GetText())
    return false;

  SetWindowTextAndCaretPos(display_text, 0);

  // Select the part of the text that was inline autocompleted.
  GtkTextIter bound, insert;
  gtk_text_buffer_get_bounds(text_buffer_, &insert, &bound);
  gtk_text_buffer_get_iter_at_offset(text_buffer_, &insert, user_text_length);
  gtk_text_buffer_select_range(text_buffer_, &insert, &bound);

  TextChanged();
  return true;
}

void AutocompleteEditViewGtk::OnRevertTemporaryText() {
  NOTIMPLEMENTED();
}

void AutocompleteEditViewGtk::OnBeforePossibleChange() {
  // Record our state.
  text_before_change_ = GetText();
  sel_before_change_ = GetSelection();
}

// TODO(deanm): This is mostly stolen from Windows, and will need some work.
bool AutocompleteEditViewGtk::OnAfterPossibleChange() {
  CharRange new_sel = GetSelection();
  int length = GetTextLength();
  bool selection_differs = (new_sel.cp_min != sel_before_change_.cp_min) ||
                           (new_sel.cp_max != sel_before_change_.cp_max);
  bool at_end_of_edit = (new_sel.cp_min == length && new_sel.cp_max == length);

  // See if the text or selection have changed since OnBeforePossibleChange().
  std::wstring new_text(GetText());
  bool text_differs = (new_text != text_before_change_);

  // When the user has deleted text, we don't allow inline autocomplete.  Make
  // sure to not flag cases like selecting part of the text and then pasting
  // (or typing) the prefix of that selection.  (We detect these by making
  // sure the caret, which should be after any insertion, hasn't moved
  // forward of the old selection start.)
  bool just_deleted_text =
      (text_before_change_.length() > new_text.length()) &&
      (new_sel.cp_min <= std::min(sel_before_change_.cp_min,
                                 sel_before_change_.cp_max));

  bool something_changed = model_->OnAfterPossibleChange(new_text,
      selection_differs, text_differs, just_deleted_text, at_end_of_edit);

  if (something_changed && text_differs)
    TextChanged();

  return something_changed;
}

void AutocompleteEditViewGtk::BottomLeftPosWidth(int* x, int* y, int* width) {
  gdk_window_get_origin(text_view_.get()->window, x, y);
  *y += text_view_.get()->allocation.height;
  *width = text_view_.get()->allocation.width;
}

void AutocompleteEditViewGtk::HandleBeginUserAction() {
  OnBeforePossibleChange();
}

void AutocompleteEditViewGtk::HandleEndUserAction() {
  // Eat any newline / paragraphs that might have come in, for example in a
  // copy and paste.  We want to make sure our widget stays single line.
  for (;;) {
    GtkTextIter cur;
    gtk_text_buffer_get_start_iter(text_buffer_, &cur);

    // If there is a line ending, this should put us right before the newline
    // or carriage return / newline (or Unicode) sequence.  If not, we're done.
    if (gtk_text_iter_forward_to_line_end(&cur) == FALSE)
      break;

    // Stepping to the next cursor position should put us on the other side of
    // the newline / paragraph / etc sequence, and then delete this range.
    GtkTextIter next_line = cur;
    gtk_text_iter_forward_cursor_position(&next_line);
    gtk_text_buffer_delete(text_buffer_, &cur, &next_line);

    // We've invalidated our iterators, gotta start again.
  }

  OnAfterPossibleChange();
}

gboolean AutocompleteEditViewGtk::HandleKeyPress(GtkWidget* widget,
                                                 GdkEventKey* event) {
  // This is very similar to the special casing of the return key in the
  // GtkTextView key_press default handler.  TODO(deanm): We do however omit
  // some IME related code, this might become a problem if an IME wants to
  // handle enter.  We can get at the im_context and do it ourselves if needed.
  if (event->keyval == GDK_Return ||
      event->keyval == GDK_ISO_Enter ||
      event->keyval == GDK_KP_Enter ||
     (event->keyval == GDK_Escape && event->state == 0)) {
    // Handle IME. This is basically taken from GtkTextView and reworked a bit.
    GtkTextIter iter;
    GtkTextView* text_view = GTK_TEXT_VIEW(text_view_.get());
    GtkTextMark* insert = gtk_text_buffer_get_insert(text_buffer_);
    gtk_text_buffer_get_iter_at_mark(text_buffer_, &iter, insert);
    gboolean can_insert = gtk_text_iter_can_insert(&iter, text_view->editable);
    if (gtk_im_context_filter_keypress(text_view->im_context, event)) {
      // The IME handled it, do the follow up IME handling.
      if (!can_insert) {
        gtk_im_context_reset(text_view->im_context);
      } else {
        text_view->need_im_reset = TRUE;
      }
    } else {
      // Ok, not handled by the IME, we can handle it.
      if (event->keyval == GDK_Escape) {
        model_->OnEscapeKeyPressed();
      } else {
        bool alt_held = (event->state & GDK_MOD1_MASK);
        model_->AcceptInput(alt_held ? NEW_FOREGROUND_TAB : CURRENT_TAB, false);
      }
    }
    return TRUE;  // Don't propagate into GtkTextView.
  }

  return FALSE;  // Propagate into GtkTextView.
}

gboolean AutocompleteEditViewGtk::HandleKeyRelease(GtkWidget* widget,
                                                   GdkEventKey* event) {
  // Even though we handled the press ourselves, let GtkTextView handle the
  // release.  It shouldn't do anything particularly interesting, but it will
  // handle the IME work for us.
  return FALSE;  // Propagate into GtkTextView.
}

// static
void AutocompleteEditViewGtk::HandleViewSizeRequest(GtkWidget* view,
                                                    GtkRequisition* req,
                                                    gpointer unused) {
  // Don't force a minimum size, allow our embedder to size us better.
  req->height = req->width = 1;
}

gboolean AutocompleteEditViewGtk::HandleViewButtonPress(GdkEventButton* event) {
  // When the GtkTextView is clicked, it will call gtk_widget_grab_focus.
  // I believe this causes the focus-in event to be fired before the main
  // clicked handling code.  If we were to try to set the selection from
  // the focus-in event, it's just going to be undone by the click handler.
  // This is a bit ugly.  We shim in to get the click before the GtkTextView,
  // then if we don't have focus, we (hopefully safely) assume that the click
  // will cause us to become focused.  We call GtkTextView's default handler
  // and then stop propagation.  This allows us to run our code after the
  // default handler, even if that handler stopped propagation.
  if (GTK_WIDGET_HAS_FOCUS(text_view_.get()))
    return FALSE;  // Continue to propagate into the GtkTextView handler.

  // Call the GtkTextView default handler, ignoring the fact that it will
  // likely have told us to stop propagating.  We want to handle selection.
  GtkWidgetClass* klass = GTK_WIDGET_GET_CLASS(text_view_.get());
  klass->button_press_event(text_view_.get(), event);

  // Select the full input when we get focus.
  SelectAll(false);
  // So we told the buffer where the cursor should be, but make sure to tell
  // the view so it can scroll it to be visible if needed.
  // NOTE: This function doesn't seem to like a count of 0, looking at the
  // code it will skip an important loop.  Use -1 to achieve the same.
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
  gtk_text_view_move_visually(GTK_TEXT_VIEW(text_view_.get()), &start, -1);

  return TRUE;  // Don't continue, we called the default handler already.
}

gboolean AutocompleteEditViewGtk::HandleViewFocusIn() {
  model_->OnSetFocus(false);
  // TODO(deanm): Some keyword hit business, etc here.
  
  return FALSE;  // Continue propagation.
}

gboolean AutocompleteEditViewGtk::HandleViewFocusOut() {
  // Close the popup.
  ClosePopup();
  // Tell the model to reset itself.
  model_->OnKillFocus();
  return FALSE;  // Pass the event on to the GtkTextView.
}

void AutocompleteEditViewGtk::HandleViewMoveCursor(
    GtkMovementStep step,
    gint count,
    gboolean extendion_selection) {
  // Handle up / down cursor movement on our own.
  if (step == GTK_MOVEMENT_DISPLAY_LINES) {
    model_->OnUpOrDownKeyPressed(count);
    // move-cursor doesn't use a signal accumulator on the return value (it
    // just ignores them), so we have to stop the propagation.
    g_signal_stop_emission_by_name(text_view_.get(), "move-cursor");
    return;
  }
  // Propagate into GtkTextView.
}

AutocompleteEditViewGtk::CharRange AutocompleteEditViewGtk::GetSelection() {
  // You can not just use get_selection_bounds here, since the order will be
  // ascending, and you don't know where the user's start and end of the
  // selection was (if the selection was forwards or backwards).  Get the
  // actual marks so that we can preserve the selection direction.
  GtkTextIter start, insert;
  GtkTextMark* mark;

  mark = gtk_text_buffer_get_selection_bound(text_buffer_);
  gtk_text_buffer_get_iter_at_mark(text_buffer_, &start, mark);

  mark = gtk_text_buffer_get_insert(text_buffer_);
  gtk_text_buffer_get_iter_at_mark(text_buffer_, &insert, mark);

  return CharRange(gtk_text_iter_get_offset(&start),
                   gtk_text_iter_get_offset(&insert));
}

void AutocompleteEditViewGtk::ItersFromCharRange(const CharRange& range,
                                                 GtkTextIter* iter_min,
                                                 GtkTextIter* iter_max) {
  gtk_text_buffer_get_iter_at_offset(text_buffer_, iter_min, range.cp_min);
  gtk_text_buffer_get_iter_at_offset(text_buffer_, iter_max, range.cp_max);
}

int AutocompleteEditViewGtk::GetTextLength() {
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
  return gtk_text_iter_get_offset(&end);
}

void AutocompleteEditViewGtk::EmphasizeURLComponents() {
  // See whether the contents are a URL with a non-empty host portion, which we
  // should emphasize.  To check for a URL, rather than using the type returned
  // by Parse(), ask the model, which will check the desired page transition for
  // this input.  This can tell us whether an UNKNOWN input string is going to
  // be treated as a search or a navigation, and is the same method the Paste
  // And Go system uses.
  url_parse::Parsed parts;
  AutocompleteInput::Parse(GetText(), model_->GetDesiredTLD(), &parts, NULL);
  bool emphasize = model_->CurrentTextIsURL() && (parts.host.len > 0);

  // Set the baseline emphasis.
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(text_buffer_, &start, &end);
  gtk_text_buffer_remove_all_tags(text_buffer_, &start, &end);
  if (emphasize) {
    gtk_text_buffer_apply_tag(text_buffer_, base_tag_, &start, &end);

    // We've found a host name, give it more emphasis.
    gtk_text_buffer_get_iter_at_line_index(text_buffer_, &start, 0,
                                           parts.host.begin);
    gtk_text_buffer_get_iter_at_line_index(text_buffer_, &end, 0,
                                           parts.host.end());
    gtk_text_buffer_remove_all_tags(text_buffer_, &start, &end);
  }

  // Emphasize the scheme for security UI display purposes (if necessary).
  if (!model_->user_input_in_progress() && parts.scheme.is_nonempty() &&
      (scheme_security_level_ != ToolbarModel::NORMAL)) {
    gtk_text_buffer_get_iter_at_line_index(text_buffer_, &start, 0,
                                           parts.scheme.begin);
    gtk_text_buffer_get_iter_at_line_index(text_buffer_, &end, 0,
                                           parts.scheme.end());
    const GdkColor* background;
    if (scheme_security_level_ == ToolbarModel::SECURE) {
      background = &kSecureBackgroundColor;
      gtk_text_buffer_apply_tag(text_buffer_, secure_scheme_tag_,
                                &start, &end);
    } else {
      background = &kInsecureBackgroundColor;
      gtk_text_buffer_apply_tag(text_buffer_, insecure_scheme_tag_,
                                &start, &end);
    }
    gtk_widget_modify_base(text_view_.get(), GTK_STATE_NORMAL, background);
  }
}

void AutocompleteEditViewGtk::TextChanged() {
  EmphasizeURLComponents();
  controller_->OnChanged();
}

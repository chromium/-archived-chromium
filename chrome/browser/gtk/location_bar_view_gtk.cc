// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/location_bar_view_gtk.h"

#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "base/gfx/gtk_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/alternate_nav_url_fetcher.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/page_transition_types.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/window_open_disposition.h"

namespace {

// Top and bottom padding/margin
// We are positioned with a little bit of extra space that we don't use now.
const int kTopMargin = 1;
const int kBottomMargin = 1;
// We draw a border on the top and bottom (but not on left or right).
const int kBorderThickness = 1;

// Left and right padding/margin.
// no icon/text  : 4px url_text 4px
//                 [4px|url text|4px] <hide ssl icon> <hide ev text>
// with icon     : 4px url_text 6px ssl_icon 8px
//                 [4px|url text|4px] [2px|ssl icon|8px] <hide ev text>
// with icon/text: 4px url_text 6px ssl_icon 8px ev_text 4px]
//                 [4px|url text|4px] [2px|ssl icon|8px] [ev text|4px]

// We don't want to edit control's text to be right against the edge.
const int kEditLeftRightPadding = 4;

// Padding around the security icon.
const int kSecurityIconPaddingLeft = 0;
const int kSecurityIconPaddingRight = 6;

const int kEvTextPaddingRight = 4;

const int kKeywordTopBottomPadding = 4;
const int kKeywordLeftRightPadding = 4;

// TODO(deanm): Eventually this should be painted with the background png
// image, but for now we get pretty close by just drawing a solid border.
const GdkColor kBorderColor = GDK_COLOR_RGB(0xbe, 0xc8, 0xd4);
const GdkColor kEvTextColor = GDK_COLOR_RGB(0x00, 0x96, 0x14);  // Green.
const GdkColor kKeywordBackgroundColor = GDK_COLOR_RGB(0xf0, 0xf4, 0xfa);
const GdkColor kKeywordBorderColor = GDK_COLOR_RGB(0xcb, 0xde, 0xf7);

// Returns the short name for a keyword.
std::wstring GetKeywordName(Profile* profile,
                            const std::wstring& keyword) {
  // Make sure the TemplateURL still exists.
  // TODO(sky): Once LocationBarView adds a listener to the TemplateURLModel
  // to track changes to the model, this should become a DCHECK.
  const TemplateURL* template_url =
      profile->GetTemplateURLModel()->GetTemplateURLForKeyword(keyword);
  if (template_url)
    return template_url->AdjustedShortNameForLocaleDirection();
  return std::wstring();
}

}  // namespace

// static
const GdkColor LocationBarViewGtk::kBackgroundColorByLevel[3] = {
  GDK_COLOR_RGB(255, 245, 195),  // SecurityLevel SECURE: Yellow.
  GDK_COLOR_RGB(255, 255, 255),  // SecurityLevel NORMAL: White.
  GDK_COLOR_RGB(255, 255, 255),  // SecurityLevel INSECURE: White.
};

LocationBarViewGtk::LocationBarViewGtk(CommandUpdater* command_updater,
    ToolbarModel* toolbar_model, AutocompletePopupPositioner* popup_positioner)
    : security_icon_align_(NULL),
      security_lock_icon_image_(NULL),
      security_warning_icon_image_(NULL),
      info_label_align_(NULL),
      info_label_(NULL),
      tab_to_search_(NULL),
      tab_to_search_label_(NULL),
      tab_to_search_hint_(NULL),
      tab_to_search_hint_leading_label_(NULL),
      tab_to_search_hint_icon_(NULL),
      tab_to_search_hint_trailing_label_(NULL),
      profile_(NULL),
      command_updater_(command_updater),
      toolbar_model_(toolbar_model),
      popup_positioner_(popup_positioner),
      disposition_(CURRENT_TAB),
      transition_(PageTransition::TYPED) {
}

LocationBarViewGtk::~LocationBarViewGtk() {
  // All of our widgets should have be children of / owned by the alignment.
  hbox_.Destroy();
}

void LocationBarViewGtk::Init() {
  location_entry_.reset(new AutocompleteEditViewGtk(this,
                                                    toolbar_model_,
                                                    profile_,
                                                    command_updater_,
                                                    popup_positioner_));
  location_entry_->Init();

  hbox_.Own(gtk_hbox_new(FALSE, 0));
  // We will paint for the alignment, to paint the background and border.
  gtk_widget_set_app_paintable(hbox_.get(), TRUE);
  // Have GTK double buffer around the expose signal.
  gtk_widget_set_double_buffered(hbox_.get(), TRUE);
  // Redraw the whole location bar when it changes size (e.g., when toggling
  // the home button on/off.
  gtk_widget_set_redraw_on_allocate(hbox_.get(), TRUE);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  security_lock_icon_image_ = gtk_image_new_from_pixbuf(
      rb.GetPixbufNamed(IDR_LOCK));
  gtk_widget_hide(GTK_WIDGET(security_lock_icon_image_));
  security_warning_icon_image_ = gtk_image_new_from_pixbuf(
      rb.GetPixbufNamed(IDR_WARNING));
  gtk_widget_hide(GTK_WIDGET(security_warning_icon_image_));

  info_label_ = gtk_label_new(NULL);
  gtk_widget_modify_base(info_label_, GTK_STATE_NORMAL,
      &LocationBarViewGtk::kBackgroundColorByLevel[0]);
  gtk_widget_hide(GTK_WIDGET(info_label_));

  g_signal_connect(hbox_.get(), "expose-event",
                   G_CALLBACK(&HandleExposeThunk), this);

  // Tab to search (the keyword box on the left hand side).
  tab_to_search_label_ = gtk_label_new(NULL);
  // We need an alignment to pad our box inside the edit area.
  tab_to_search_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(tab_to_search_),
      kKeywordTopBottomPadding, kKeywordTopBottomPadding,
      kKeywordLeftRightPadding, kKeywordLeftRightPadding);

  // This crazy stack of alignments and event boxes creates a box around the
  // keyword text with a border, background color, and padding around the text.
  gtk_container_add(GTK_CONTAINER(tab_to_search_),
      gtk_util::CreateGtkBorderBin(
        gtk_util::CreateGtkBorderBin(
            tab_to_search_label_, &kKeywordBackgroundColor, 1, 1, 2, 2),
        &kKeywordBorderColor, 1, 1, 1, 1));

  gtk_box_pack_start(GTK_BOX(hbox_.get()), tab_to_search_, FALSE, FALSE, 0);

  GtkWidget* align = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  // TODO(erg): Redo this so that it adjusts during theme changes.
  if (GtkThemeProvider::UseSystemThemeGraphics(profile_)) {
    gtk_alignment_set_padding(GTK_ALIGNMENT(align),
                              0, 0,
                              kEditLeftRightPadding, kEditLeftRightPadding);
  } else {
    gtk_alignment_set_padding(GTK_ALIGNMENT(align),
                              kTopMargin + kBorderThickness,
                              kBottomMargin + kBorderThickness,
                              kEditLeftRightPadding, kEditLeftRightPadding);
  }
  gtk_container_add(GTK_CONTAINER(align), location_entry_->widget());
  gtk_box_pack_start(GTK_BOX(hbox_.get()), align, TRUE, TRUE, 0);

  // Tab to search notification (the hint on the right hand side).
  tab_to_search_hint_ = gtk_hbox_new(FALSE, 0);
  tab_to_search_hint_leading_label_ = gtk_label_new(NULL);
  tab_to_search_hint_icon_ = gtk_image_new_from_pixbuf(
      rb.GetPixbufNamed(IDR_LOCATION_BAR_KEYWORD_HINT_TAB));
  tab_to_search_hint_trailing_label_ = gtk_label_new(NULL);
  gtk_box_pack_start(GTK_BOX(tab_to_search_hint_),
                     tab_to_search_hint_leading_label_,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(tab_to_search_hint_),
                     tab_to_search_hint_icon_,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(tab_to_search_hint_),
                     tab_to_search_hint_trailing_label_,
                     FALSE, FALSE, 0);
  // tab_to_search_hint_ gets hidden initially in OnChanged.  Hiding it here
  // doesn't work, someone is probably calling show_all on our parent box.
  gtk_box_pack_end(GTK_BOX(hbox_.get()), tab_to_search_hint_, FALSE, FALSE, 4);

  // Pack info_label_ and security icons in hbox.  We hide/show them
  // by SetSecurityIcon() and SetInfoText().
  info_label_align_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(info_label_align_),
                            kTopMargin + kBorderThickness,
                            kBottomMargin + kBorderThickness,
                            0, kEvTextPaddingRight);
  gtk_container_add(GTK_CONTAINER(info_label_align_), info_label_);
  gtk_box_pack_end(GTK_BOX(hbox_.get()), info_label_align_, FALSE, FALSE, 0);

  GtkWidget* security_icon_box = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(security_icon_box),
                     security_lock_icon_image_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(security_icon_box),
                     security_warning_icon_image_, FALSE, FALSE, 0);

  security_icon_align_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(security_icon_align_),
                            kTopMargin + kBorderThickness,
                            kBottomMargin + kBorderThickness,
                            kSecurityIconPaddingLeft,
                            kSecurityIconPaddingRight);
  gtk_container_add(GTK_CONTAINER(security_icon_align_), security_icon_box);
  gtk_box_pack_end(GTK_BOX(hbox_.get()), security_icon_align_, FALSE, FALSE, 0);
}

void LocationBarViewGtk::SetProfile(Profile* profile) {
  profile_ = profile;
}

void LocationBarViewGtk::Update(const TabContents* contents) {
  SetSecurityIcon(toolbar_model_->GetIcon());
  SetInfoText();
  location_entry_->Update(contents);
  // The security level (background color) could have changed, etc.
  gtk_widget_queue_draw(hbox_.get());
}

void LocationBarViewGtk::OnAutocompleteAccept(const GURL& url,
      WindowOpenDisposition disposition,
      PageTransition::Type transition,
      const GURL& alternate_nav_url) {
  if (!url.is_valid())
    return;

  location_input_ = UTF8ToWide(url.spec());
  disposition_ = disposition;
  transition_ = transition;

  if (!command_updater_)
    return;

  if (!alternate_nav_url.is_valid()) {
    command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);
    return;
  }

  scoped_ptr<AlternateNavURLFetcher> fetcher(
      new AlternateNavURLFetcher(alternate_nav_url));
  // The AlternateNavURLFetcher will listen for the pending navigation
  // notification that will be issued as a result of the "open URL." It
  // will automatically install itself into that navigation controller.
  command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);
  if (fetcher->state() == AlternateNavURLFetcher::NOT_STARTED) {
    // I'm not sure this should be reachable, but I'm not also sure enough
    // that it shouldn't to stick in a NOTREACHED().  In any case, this is
    // harmless; we can simply let the fetcher get deleted here and it will
    // clean itself up properly.
  } else {
    fetcher.release();  // The navigation controller will delete the fetcher.
  }
}

void LocationBarViewGtk::OnChanged() {
  const std::wstring keyword(location_entry_->model()->keyword());
  const bool is_keyword_hint = location_entry_->model()->is_keyword_hint();
  const bool show_selected_keyword = !keyword.empty() && !is_keyword_hint;
  const bool show_keyword_hint = !keyword.empty() && is_keyword_hint;

  if (show_selected_keyword) {
    SetKeywordLabel(keyword);
    gtk_widget_show_all(tab_to_search_);
  } else {
    gtk_widget_hide_all(tab_to_search_);
  }

  if (show_keyword_hint) {
    SetKeywordHintLabel(keyword);
    gtk_widget_show_all(tab_to_search_hint_);
  } else {
    gtk_widget_hide_all(tab_to_search_hint_);
  }
}

void LocationBarViewGtk::OnInputInProgress(bool in_progress) {
  // This is identical to the Windows code, except that we don't proxy the call
  // back through the Toolbar, and just access the model here.
  // The edit should make sure we're only notified when something changes.
  DCHECK(toolbar_model_->input_in_progress() != in_progress);

  toolbar_model_->set_input_in_progress(in_progress);
  Update(NULL);
}

SkBitmap LocationBarViewGtk::GetFavIcon() const {
  NOTIMPLEMENTED();
  return SkBitmap();
}

std::wstring LocationBarViewGtk::GetTitle() const {
  NOTIMPLEMENTED();
  return std::wstring();
}

void LocationBarViewGtk::ShowFirstRunBubble(bool use_OEM_bubble) {
  NOTIMPLEMENTED();
}

std::wstring LocationBarViewGtk::GetInputString() const {
  return location_input_;
}

WindowOpenDisposition LocationBarViewGtk::GetWindowOpenDisposition() const {
  return disposition_;
}

PageTransition::Type LocationBarViewGtk::GetPageTransition() const {
  return transition_;
}

void LocationBarViewGtk::AcceptInput() {
  AcceptInputWithDisposition(CURRENT_TAB);
}

void LocationBarViewGtk::AcceptInputWithDisposition(
    WindowOpenDisposition disposition) {
  location_entry_->model()->AcceptInput(disposition, false);
}

void LocationBarViewGtk::FocusLocation() {
  location_entry_->SetFocus();
  location_entry_->SelectAll(true);
}

void LocationBarViewGtk::FocusSearch() {
  location_entry_->SetFocus();
  location_entry_->SetForcedQuery();
}

void LocationBarViewGtk::UpdatePageActions() {
  // http://code.google.com/p/chromium/issues/detail?id=11973
}

void LocationBarViewGtk::SaveStateToContents(TabContents* contents) {
  location_entry_->SaveStateToTab(contents);
}

void LocationBarViewGtk::Revert() {
  location_entry_->RevertAll();
}

gboolean LocationBarViewGtk::HandleExpose(GtkWidget* widget,
                                          GdkEventExpose* event) {
  GdkDrawable* drawable = GDK_DRAWABLE(event->window);
  GdkGC* gc = gdk_gc_new(drawable);

  GdkRectangle* alloc_rect = &hbox_->allocation;

  // The area outside of our margin, which includes the border.
  GdkRectangle inner_rect = {
      alloc_rect->x,
      alloc_rect->y + kTopMargin,
      alloc_rect->width,
      alloc_rect->height - kTopMargin - kBottomMargin};

  // Some of our calculations are a bit sloppy.  Since we draw on our parent
  // window, set a clip to make sure that we don't draw outside.
  gdk_gc_set_clip_rectangle(gc, &inner_rect);

  // Draw our 1px border.  TODO(deanm): Maybe this would be cleaner as an
  // overdrawn stroked rect with a clip to the allocation?
  gdk_gc_set_rgb_fg_color(gc, &kBorderColor);
  gdk_draw_rectangle(drawable, gc, TRUE,
                     inner_rect.x,
                     inner_rect.y,
                     inner_rect.width,
                     kBorderThickness);
  gdk_draw_rectangle(drawable, gc, TRUE,
                     inner_rect.x,
                     inner_rect.y + inner_rect.height - kBorderThickness,
                     inner_rect.width,
                     kBorderThickness);

  // Draw the background within the border.
  gdk_gc_set_rgb_fg_color(gc,
      &kBackgroundColorByLevel[toolbar_model_->GetSchemeSecurityLevel()]);
  gdk_draw_rectangle(drawable, gc, TRUE,
                     inner_rect.x,
                     inner_rect.y + kBorderThickness,
                     inner_rect.width,
                     inner_rect.height - (kBorderThickness * 2));

  g_object_unref(gc);

  return FALSE;  // Continue propagating the expose.
}

void LocationBarViewGtk::SetSecurityIcon(ToolbarModel::Icon icon) {
  gtk_widget_hide(GTK_WIDGET(security_lock_icon_image_));
  gtk_widget_hide(GTK_WIDGET(security_warning_icon_image_));
  if (icon != ToolbarModel::NO_ICON)
    gtk_widget_show(GTK_WIDGET(security_icon_align_));
  else
    gtk_widget_hide(GTK_WIDGET(security_icon_align_));
  switch (icon) {
    case ToolbarModel::LOCK_ICON:
      gtk_widget_show(GTK_WIDGET(security_lock_icon_image_));
      break;
    case ToolbarModel::WARNING_ICON:
      gtk_widget_show(GTK_WIDGET(security_warning_icon_image_));
      break;
    case ToolbarModel::NO_ICON:
      break;
    default:
      NOTREACHED();
      break;
  }
}

void LocationBarViewGtk::SetInfoText() {
  std::wstring info_text, info_tooltip;
  ToolbarModel::InfoTextType info_text_type =
      toolbar_model_->GetInfoText(&info_text, &info_tooltip);
  if (info_text_type == ToolbarModel::INFO_EV_TEXT) {
    gtk_widget_modify_fg(GTK_WIDGET(info_label_), GTK_STATE_NORMAL,
                         &kEvTextColor);
    gtk_widget_show(GTK_WIDGET(info_label_align_));
  } else {
    DCHECK_EQ(info_text_type, ToolbarModel::INFO_NO_INFO);
    DCHECK(info_text.empty());
    // Clear info_text.  Should we reset the fg here?
    gtk_widget_hide(GTK_WIDGET(info_label_align_));
  }
  gtk_label_set_text(GTK_LABEL(info_label_), WideToUTF8(info_text).c_str());
  gtk_widget_set_tooltip_text(GTK_WIDGET(info_label_),
                              WideToUTF8(info_tooltip).c_str());
}

void LocationBarViewGtk::SetKeywordLabel(const std::wstring& keyword) {
  if (keyword.empty())
    return;

  DCHECK(profile_);
  if (!profile_->GetTemplateURLModel())
    return;

  const std::wstring short_name = GetKeywordName(profile_, keyword);
  // TODO(deanm): Windows does some measuring of the text here and truncates
  // it if it's too long?
  std::wstring full_name(l10n_util::GetStringF(IDS_OMNIBOX_KEYWORD_TEXT,
                                               short_name));
  gtk_label_set_text(GTK_LABEL(tab_to_search_label_),
                     WideToUTF8(full_name).c_str());
}

void LocationBarViewGtk::SetKeywordHintLabel(const std::wstring& keyword) {
  if (keyword.empty())
    return;

  DCHECK(profile_);
  if (!profile_->GetTemplateURLModel())
    return;

  std::vector<size_t> content_param_offsets;
  const std::wstring keyword_hint(l10n_util::GetStringF(
      IDS_OMNIBOX_KEYWORD_HINT, std::wstring(),
      GetKeywordName(profile_, keyword), &content_param_offsets));

  if (content_param_offsets.size() != 2) {
    // See comments on an identical NOTREACHED() in search_provider.cc.
    NOTREACHED();
    return;
  }

  std::string leading(WideToUTF8(
      keyword_hint.substr(0, content_param_offsets.front())));
  std::string trailing(WideToUTF8(
      keyword_hint.substr(content_param_offsets.front())));
  gtk_label_set_text(GTK_LABEL(tab_to_search_hint_leading_label_),
                     leading.c_str());
  gtk_label_set_text(GTK_LABEL(tab_to_search_hint_trailing_label_),
                     trailing.c_str());
}

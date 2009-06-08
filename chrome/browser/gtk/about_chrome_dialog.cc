// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/about_chrome_dialog.h"

#include <gtk/gtk.h>
#include <wchar.h>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_version_info.h"
#include "base/gfx/gtk_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/browser/profile.h"
#include "chrome/common/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "webkit/glue/webkit_glue.h"

namespace {
// The pixel width of the version text field. Ideally, we'd like to have the
// bounds set to the edge of the icon. However, the icon is not a view but a
// part of the background, so we have to hard code the width to make sure
// the version field doesn't overlap it.
const int kVersionFieldWidth = 195;

// The URLs that you navigate to when clicking the links in the About dialog.
const wchar_t* const kChromiumUrl = L"http://www.chromium.org/";
const wchar_t* const kAcknowledgements = L"about:credits";
const wchar_t* const kTOS = L"about:terms";

// Left or right margin.
const int kPanelHorizMargin = 13;

// Top or bottom margin.
const int kPanelVertMargin = 13;

// These are used as placeholder text around the links in the text in the about
// dialog.
const wchar_t* kBeginLinkChr = L"BEGIN_LINK_CHR";
const wchar_t* kBeginLinkOss = L"BEGIN_LINK_OSS";
const wchar_t* kEndLinkChr = L"END_LINK_CHR";
const wchar_t* kEndLinkOss = L"END_LINK_OSS";
const wchar_t* kBeginLink = L"BEGIN_LINK";
const wchar_t* kEndLink = L"END_LINK";

void RemoveText(std::wstring* text, const wchar_t* to_remove) {
  size_t start = text->find(to_remove, 0);
  if (start != std::string::npos) {
    size_t length =  wcslen(to_remove);
    *text = text->substr(0, start) + text->substr(start + length);
  }
}

void OnDialogResponse(GtkDialog* dialog, int response_id) {
  // We're done.
  gtk_widget_destroy(GTK_WIDGET(dialog));
}

void FixLabelWrappingCallback(GtkWidget *label,
                              GtkAllocation *allocation,
                              gpointer data) {
  gtk_widget_set_size_request(label, allocation->width, -1);
}

GtkWidget* MakeMarkupLabel(const char* format, const std::wstring& str) {
  GtkWidget* label = gtk_label_new(NULL);
  char* markup = g_markup_printf_escaped(
      format, WideToUTF8(str).c_str());
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);

  return label;
}

}  // namespace

void ShowAboutDialogForProfile(GtkWindow* parent, Profile* profile) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  static GdkPixbuf* background = rb.GetPixbufNamed(IDR_ABOUT_BACKGROUND);
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  std::wstring current_version = version_info->file_version();
#if !defined(GOOGLE_CHROME_BUILD)
  current_version += L" (";
  current_version += version_info->last_change();
  current_version += L")";
#endif

  // Build the dialog.
  GtkWidget* dialog = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_ABOUT_CHROME_TITLE).c_str(),
      parent,
      GTK_DIALOG_MODAL,
      GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
      NULL);
  // Pick up the style set in gtk_util.cc:InitRCStyles().
  // The layout of this dialog is special because the logo should be flush
  // with the edges of the window.
  gtk_widget_set_name(dialog, "about-dialog");
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

  GtkWidget* content_area = GTK_DIALOG(dialog)->vbox;

  // Use an event box to get the background painting correctly
  GtkWidget* ebox = gtk_event_box_new();
  gtk_widget_modify_bg(ebox, GTK_STATE_NORMAL, &gfx::kGdkWhite);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);

  GtkWidget* text_alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(text_alignment),
                            kPanelVertMargin, kPanelVertMargin,
                            kPanelHorizMargin, kPanelHorizMargin);

  GtkWidget* text_vbox = gtk_vbox_new(FALSE, 0);

  GtkWidget* product_label = MakeMarkupLabel(
      "<span font_desc=\"18\" weight=\"bold\" style=\"normal\">%s</span>",
      l10n_util::GetString(IDS_PRODUCT_NAME));
  gtk_box_pack_start(GTK_BOX(text_vbox), product_label, FALSE, FALSE, 0);

  GtkWidget* version_label = MakeMarkupLabel(
      "<span style=\"italic\">%s</span>",
      current_version);
  gtk_label_set_selectable(GTK_LABEL(version_label), TRUE);
  gtk_box_pack_start(GTK_BOX(text_vbox), version_label, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(text_alignment), text_vbox);
  gtk_box_pack_start(GTK_BOX(hbox), text_alignment, TRUE, FALSE, 0);

  GtkWidget* image_vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_end(GTK_BOX(image_vbox),
                   gtk_image_new_from_pixbuf(background),
                   FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), image_vbox, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(ebox), hbox);
  gtk_box_pack_start(GTK_BOX(content_area), ebox, TRUE, TRUE, 0);

  // We use a separate box for the licensing etc. text.  See the comment near
  // the top of this function about using a special layout for this dialog.
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(vbox),
                                 gtk_util::kContentAreaBorder);

  GtkWidget* copyright_label = MakeMarkupLabel(
      "<span size=\"smaller\">%s</span>",
      l10n_util::GetString(IDS_ABOUT_VERSION_COPYRIGHT));
  gtk_box_pack_start(GTK_BOX(vbox), copyright_label, TRUE, TRUE, 5);

  // TODO(erg): Figure out how to really insert links. We could just depend on
  // (or include the source of) libsexy's SexyUrlLabel gtk widget...
  std::wstring license = l10n_util::GetString(IDS_ABOUT_VERSION_LICENSE);
  RemoveText(&license, kBeginLinkChr);
  RemoveText(&license, kBeginLinkOss);
  RemoveText(&license, kBeginLink);
  RemoveText(&license, kEndLinkChr);
  RemoveText(&license, kEndLinkOss);
  RemoveText(&license, kEndLink);

  GtkWidget* license_label = MakeMarkupLabel(
      "<span size=\"smaller\">%s</span>", license);

  gtk_label_set_line_wrap(GTK_LABEL(license_label), TRUE);
  gtk_misc_set_alignment(GTK_MISC(license_label), 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox), license_label, TRUE, TRUE, 0);

  // Hack around Gtk's not-so-good label wrapping, as described here:
  // http://blog.16software.com/dynamic-label-wrapping-in-gtk
  g_signal_connect(G_OBJECT(license_label), "size-allocate",
                   G_CALLBACK(FixLabelWrappingCallback), NULL);

  gtk_box_pack_start(GTK_BOX(content_area), vbox, TRUE, TRUE, 0);

  g_signal_connect(dialog, "response", G_CALLBACK(OnDialogResponse), NULL);
  gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
  gtk_widget_show_all(dialog);
}

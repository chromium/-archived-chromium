// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "chrome/browser/options_window.h"

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gtk/options/advanced_page_gtk.h"
#include "chrome/browser/gtk/options/content_page_gtk.h"
#include "chrome/browser/gtk/options/general_page_gtk.h"
#include "chrome/browser/profile.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#ifdef CHROME_PERSONALIZATION
#include "chrome/personalization/personalization.h"
#endif
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowGtk
//
// The contents of the Options dialog window.

class OptionsWindowGtk {
 public:
  explicit OptionsWindowGtk(Profile* profile);
  ~OptionsWindowGtk();

  // Shows the Tab corresponding to the specified OptionsPage.
  void ShowOptionsPage(OptionsPage page, OptionsGroup highlight_group);

 private:
  static void OnSwitchPage(GtkNotebook* notebook, GtkNotebookPage* page,
                           guint page_num, OptionsWindowGtk* options_window);

  static void OnWindowDestroy(GtkWidget* widget, OptionsWindowGtk* window);

  // The options dialog.
  GtkWidget *dialog_;

  // The container of the option pages.
  GtkWidget *notebook_;

  // The Profile associated with these options.
  Profile* profile_;

  // The general page.
  GeneralPageGtk general_page_;

  // The content page.
  ContentPageGtk content_page_;

  // The advanced (user data) page.
  AdvancedPageGtk advanced_page_;

  // The last page the user was on when they opened the Options window.
  IntegerPrefMember last_selected_page_;

  DISALLOW_COPY_AND_ASSIGN(OptionsWindowGtk);
};

static OptionsWindowGtk* instance_ = NULL;

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowGtk, public:

OptionsWindowGtk::OptionsWindowGtk(Profile* profile)
      // Always show preferences for the original profile. Most state when off
      // the record comes from the original profile, but we explicitly use
      // the original profile to avoid potential problems.
    : profile_(profile->GetOriginalProfile()),
      general_page_(profile_),
      content_page_(profile_),
      advanced_page_(profile_) {
  // The download manager needs to be initialized before the contents of the
  // Options Window are created.
  profile_->GetDownloadManager();
  // We don't need to observe changes in this value.
  last_selected_page_.Init(prefs::kOptionsWindowLastTabIndex,
                           g_browser_process->local_state(), NULL);

  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringFUTF8(IDS_OPTIONS_DIALOG_TITLE,
          WideToUTF16(l10n_util::GetString(IDS_PRODUCT_NAME))).c_str(),
      // Prefs window is shared between all browser windows.
      NULL,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE,
      NULL);
  gtk_window_set_default_size(GTK_WINDOW(dialog_), 500, -1);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  notebook_ = gtk_notebook_new();

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      general_page_.get_page_widget(),
      gtk_label_new(
          l10n_util::GetStringUTF8(IDS_OPTIONS_GENERAL_TAB_LABEL).c_str()));

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      content_page_.get_page_widget(),
      gtk_label_new(
          l10n_util::GetStringUTF8(IDS_OPTIONS_CONTENT_TAB_LABEL).c_str()));

#ifdef CHROME_PERSONALIZATION
  if (!Personalization::IsP13NDisabled(profile)) {
    gtk_notebook_append_page(
        GTK_NOTEBOOK(notebook_),
        gtk_label_new("TODO personalization"),
        gtk_label_new(
            l10n_util::GetStringUTF8(IDS_OPTIONS_USER_DATA_TAB_LABEL).c_str()));
  }
#endif

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      advanced_page_.get_page_widget(),
      gtk_label_new(
          l10n_util::GetStringUTF8(IDS_OPTIONS_ADVANCED_TAB_LABEL).c_str()));

  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), notebook_);

  DCHECK(
      gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_)) == OPTIONS_PAGE_COUNT);

  // Need to show the notebook before connecting switch-page signal, otherwise
  // we'll immediately get a signal switching to page 0 and overwrite our
  // last_selected_page_ value.
  gtk_widget_show_all(dialog_);

  g_signal_connect(notebook_, "switch-page", G_CALLBACK(OnSwitchPage), this);

  // We only have one button and don't do any special handling, so just hook it
  // directly to gtk_widget_destroy.
  g_signal_connect_swapped(dialog_, "response", G_CALLBACK(gtk_widget_destroy),
                           dialog_);

  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroy), this);
}

OptionsWindowGtk::~OptionsWindowGtk() {
}

void OptionsWindowGtk::ShowOptionsPage(OptionsPage page,
                                       OptionsGroup highlight_group) {
  // Bring options window to front if it already existed and isn't already
  // in front
  // TODO(mattm): docs say it's preferable to use gtk_window_present_with_time
  gtk_window_present(GTK_WINDOW(dialog_));

  if (page == OPTIONS_PAGE_DEFAULT) {
    // Remember the last visited page from local state.
    page = static_cast<OptionsPage>(last_selected_page_.GetValue());
    if (page == OPTIONS_PAGE_DEFAULT)
      page = OPTIONS_PAGE_GENERAL;
  }
  // If the page number is out of bounds, reset to the first tab.
  if (page < 0 || page >= gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_)))
    page = OPTIONS_PAGE_GENERAL;

  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), page);

  // TODO(mattm): set highlight_group
}

///////////////////////////////////////////////////////////////////////////////
// OptionsWindowGtk, private:

// static
void OptionsWindowGtk::OnSwitchPage(GtkNotebook* notebook,
                                    GtkNotebookPage* page,
                                    guint page_num,
                                    OptionsWindowGtk* options_window) {
  int index = page_num;
  DCHECK(index > OPTIONS_PAGE_DEFAULT && index < OPTIONS_PAGE_COUNT);
  options_window->last_selected_page_.SetValue(index);
}

// static
void OptionsWindowGtk::OnWindowDestroy(GtkWidget* widget,
                                       OptionsWindowGtk* options_window) {
  instance_ = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, options_window);
}

///////////////////////////////////////////////////////////////////////////////
// Factory/finder method:

void ShowOptionsWindow(OptionsPage page,
                       OptionsGroup highlight_group,
                       Profile* profile) {
  DCHECK(profile);
  // If there's already an existing options window, activate it and switch to
  // the specified page.
  if (!instance_) {
    instance_ = new OptionsWindowGtk(profile);
  }
  instance_->ShowOptionsPage(page, highlight_group);
}

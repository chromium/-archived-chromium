// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_bar_gtk.h"

#include "base/gfx/gtk_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"

namespace {

const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xe6, 0xed, 0xf4);

// Padding around the container.
const int kBarPadding = 2;

// Maximum number of characters on a bookmark button.
const int kMaxCharsOnAButton = 15;

}  // namespace

BookmarkBarGtk::BookmarkBarGtk(Profile* profile, Browser* browser)
    : profile_(NULL),
      page_navigator_(NULL),
      browser_(browser),
      model_(NULL),
      instructions_(NULL),
      show_instructions_(true) {
  Init(profile);
  SetProfile(profile);
}

BookmarkBarGtk::~BookmarkBarGtk() {
  if (model_)
    model_->RemoveObserver(this);

  RemoveAllBookmarkButtons();
  container_.Destroy();
}

void BookmarkBarGtk::SetProfile(Profile* profile) {
  DCHECK(profile);
  if (profile_ == profile)
    return;

  RemoveAllBookmarkButtons();

  profile_ = profile;

  if (model_)
    model_->RemoveObserver(this);

  // TODO(erg): Add the other bookmarked button, disabled.

  // TODO(erg): Handle extensions

  model_ = profile_->GetBookmarkModel();
  model_->AddObserver(this);
  if (model_->IsLoaded())
    Loaded(model_);

  // else case: we'll receive notification back from the BookmarkModel when done
  // loading, then we'll populate the bar.
}

void BookmarkBarGtk::SetPageNavigator(PageNavigator* navigator) {
  page_navigator_ = navigator;
}

void BookmarkBarGtk::Init(Profile* profile) {
  bookmark_hbox_ = gtk_hbox_new(FALSE, 0);
  container_.Own(gfx::CreateGtkBorderBin(bookmark_hbox_, &kBackgroundColor,
      kBarPadding, kBarPadding, kBarPadding, kBarPadding));

  instructions_ =
      gtk_label_new(
          WideToUTF8(l10n_util::GetString(IDS_BOOKMARKS_NO_ITEMS)).c_str());
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), instructions_,
                     FALSE, FALSE, 0);
}

void BookmarkBarGtk::AddBookmarkbarToBox(GtkWidget* box) {
  gtk_box_pack_start(GTK_BOX(box), container_.get(), FALSE, FALSE, 0);
}

void BookmarkBarGtk::Show() {
  gtk_widget_show_all(container_.get());

  // Maybe show the instructions
  if (show_instructions_) {
    gtk_widget_show(instructions_);
  } else {
    gtk_widget_hide(instructions_);
  }
}

void BookmarkBarGtk::Hide() {
  gtk_widget_hide_all(container_.get());
}

bool BookmarkBarGtk::OnNewTabPage() {
  return (browser_ && browser_->GetSelectedTabContents() &&
          browser_->GetSelectedTabContents()->IsBookmarkBarAlwaysVisible());
}

void BookmarkBarGtk::Loaded(BookmarkModel* model) {
  RemoveAllBookmarkButtons();

  BookmarkNode* node = model_->GetBookmarkBarNode();
  DCHECK(node && model_->other_node());

  // Create a button for each of the children on the bookmark bar.
  for (int i = 0; i < node->GetChildCount(); ++i) {
    CustomContainerButton* button = CreateBookmarkButton(node->GetChild(i));
    gtk_box_pack_start(GTK_BOX(bookmark_hbox_), button->widget(),
                       FALSE, FALSE, 0);
    current_bookmark_buttons_.push_back(button);
  }

  show_instructions_ = (node->GetChildCount() == 0);
  if (show_instructions_) {
    gtk_widget_show(instructions_);
  } else {
    gtk_widget_hide(instructions_);
  }

  // TODO(erg): Reenable the other bookmarks button here once it exists.
}

void BookmarkBarGtk::RemoveAllBookmarkButtons() {
  for (std::vector<CustomContainerButton*>::const_iterator it =
           current_bookmark_buttons_.begin();
       it != current_bookmark_buttons_.end(); ++it) {
    delete *it;
  }

  current_bookmark_buttons_.clear();
}

bool BookmarkBarGtk::IsAlwaysShown() {
  return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

CustomContainerButton* BookmarkBarGtk::CreateBookmarkButton(
    BookmarkNode* node) {
  if (node->is_url()) {
    CustomContainerButton* button = new CustomContainerButton;

    gtk_widget_set_tooltip_text(button->widget(), BuildTooltip(node).c_str());
    gtk_button_set_label(GTK_BUTTON(button->widget()),
                         WideToUTF8(node->GetTitle()).c_str());
    // TODO(erg): Consider a soft maximum instead of this hard 15.
    gtk_label_set_max_width_chars(
        GTK_LABEL(gtk_bin_get_child(GTK_BIN(button->widget()))),
        kMaxCharsOnAButton);

    // Connect to 'button-release-event' instead of 'clicked' because we need
    // to access to the modifier keys and we do different things on each
    // button.
    g_signal_connect(G_OBJECT(button->widget()), "button-release-event",
                     G_CALLBACK(OnButtonReleased), this);
    GTK_WIDGET_UNSET_FLAGS(button->widget(), GTK_CAN_FOCUS);

    g_object_set_data(G_OBJECT(button->widget()), "bookmark-node", node);

    gtk_widget_show_all(button->widget());

    return button;
  } else {
    NOTIMPLEMENTED();
    return NULL;
  }
}

std::string BookmarkBarGtk::BuildTooltip(BookmarkNode* node) {
  // TODO(erg): Actually build the tooltip. For now, we punt and just return
  // the URL.
  return node->GetURL().possibly_invalid_spec();
}

gboolean BookmarkBarGtk::OnButtonReleased(GtkWidget* sender,
                                          GdkEventButton* event,
                                          BookmarkBarGtk* bar) {
  gpointer user_data = g_object_get_data(G_OBJECT(sender), "bookmark-node");
  BookmarkNode* node = static_cast<BookmarkNode*>(user_data);
  DCHECK(node);
  DCHECK(bar->page_navigator_);

  if (node->is_url()) {
    bar->page_navigator_->OpenURL(
        node->GetURL(), GURL(),
        // TODO(erg): Detect the disposition based on the click type.
        CURRENT_TAB,
        PageTransition::AUTO_BOOKMARK);
  } else {
    // TODO(erg): Handle folders and extensions.
  }

  UserMetrics::RecordAction(L"ClickedBookmarkBarURLButton", bar->profile_);

  // Allow other handlers to run so the button state is updated correctly.
  return FALSE;
}

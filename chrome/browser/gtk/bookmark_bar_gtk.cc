// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_bar_gtk.h"

#include "base/gfx/gtk_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/common/gfx/text_elider.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/resource_bundle.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xe6, 0xed, 0xf4);

// Padding around the container.
const int kBarPadding = 2;

// Maximum number of characters on a bookmark button.
const int kMaxCharsOnAButton = 15;

// Our custom draging type for bookmarks.
static GtkTargetEntry target_table[] = {
  { const_cast<char*>("application/x-bookmark-toolbar-item"),
    GTK_TARGET_SAME_APP, 0 }
};

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
  bookmark_toolbar_.Destroy();
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

  bookmark_toolbar_.Own(gtk_toolbar_new());
  g_signal_connect(G_OBJECT(bookmark_toolbar_.get()), "expose-event",
                   G_CALLBACK(&OnToolbarExpose), this);
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), bookmark_toolbar_.get(),
                     TRUE, TRUE, 0);

  gtk_drag_dest_set(bookmark_toolbar_.get(), GTK_DEST_DEFAULT_DROP,
                    target_table, G_N_ELEMENTS(target_table),
                    GDK_ACTION_MOVE);
  g_signal_connect(bookmark_toolbar_.get(), "drag-motion",
                   G_CALLBACK(&OnToolbarDragMotion), this);
  g_signal_connect(bookmark_toolbar_.get(), "drag-leave",
                   G_CALLBACK(&OnToolbarDragLeave), this);

  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), gtk_vseparator_new(),
                     FALSE, FALSE, 0);

  other_bookmarks_button_ = gtk_chrome_button_new();
  gtk_button_set_label(GTK_BUTTON(other_bookmarks_button_),
                       "Other bookmarks");
  // TODO(erg): Hook up a popup menu to |other_bookmarks_button_|.
  gtk_box_pack_start(GTK_BOX(bookmark_hbox_), other_bookmarks_button_,
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
    GtkToolItem* item = CreateBookmarkToolItem(node->GetChild(i));
    gtk_toolbar_insert(GTK_TOOLBAR(bookmark_toolbar_.get()), item, -1);
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
  gfx::RemoveAllChildren(bookmark_toolbar_.get());
}

bool BookmarkBarGtk::IsAlwaysShown() {
  return profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

GtkWidget* BookmarkBarGtk::CreateBookmarkButton(
    BookmarkNode* node) {
  GtkWidget* button = gtk_chrome_button_new();

  if (node->is_url()) {
    gtk_widget_set_tooltip_text(button, BuildTooltip(node).c_str());
    gtk_button_set_label(GTK_BUTTON(button),
                         WideToUTF8(node->GetTitle()).c_str());
    // TODO(erg): Consider a soft maximum instead of this hard 15.
    gtk_label_set_max_width_chars(
        GTK_LABEL(gtk_bin_get_child(GTK_BIN(button))),
        kMaxCharsOnAButton);

    // The tool item is also a source for dragging
    gtk_drag_source_set(button, GDK_BUTTON1_MASK,
                        target_table, G_N_ELEMENTS(target_table),
                        GDK_ACTION_MOVE);
    g_signal_connect(G_OBJECT(button), "drag-begin",
                     G_CALLBACK(&OnButtonDragBegin), this);
    g_signal_connect(G_OBJECT(button), "drag-end",
                     G_CALLBACK(&OnButtonDragEnd), this);

    // Connect to 'button-release-event' instead of 'clicked' because we need
    // to access to the modifier keys and we do different things on each
    // button.
    g_signal_connect(G_OBJECT(button), "button-press-event",
                     G_CALLBACK(OnButtonPressed), this);
    g_signal_connect(G_OBJECT(button), "button-release-event",
                     G_CALLBACK(OnButtonReleased), this);
    GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);

    g_object_set_data(G_OBJECT(button), "bookmark-node", node);
  } else {
    NOTIMPLEMENTED();
  }

  return button;
}

GtkToolItem* BookmarkBarGtk::CreateBookmarkToolItem(BookmarkNode* node) {
  GtkWidget* button = CreateBookmarkButton(node);

  GtkToolItem* item = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(item), button);
  gtk_widget_show_all(GTK_WIDGET(item));

  return item;
}

std::string BookmarkBarGtk::BuildTooltip(BookmarkNode* node) {
  // TODO(erg): Actually build the tooltip. For now, we punt and just return
  // the URL.
  return node->GetURL().possibly_invalid_spec();
}

gboolean BookmarkBarGtk::OnButtonPressed(GtkWidget* sender,
                                         GdkEventButton* event,
                                         BookmarkBarGtk* bar) {
  bar->ignore_button_release_ = false;
  return FALSE;
}

gboolean BookmarkBarGtk::OnButtonReleased(GtkWidget* sender,
                                          GdkEventButton* event,
                                          BookmarkBarGtk* bar) {
  if (bar->ignore_button_release_) {
    // Don't handle this message; it was a drag.
    bar->ignore_button_release_ = false;
    return FALSE;
  }

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

// static
void BookmarkBarGtk::OnButtonDragBegin(GtkWidget* button,
                                       GdkDragContext* drag_context,
                                       BookmarkBarGtk* bar) {
  // Signal to any future OnButtonReleased calls that we're dragging instead of
  // pressing.
  bar->ignore_button_release_ = true;

  gpointer user_data = g_object_get_data(G_OBJECT(button), "bookmark-node");
  BookmarkNode* node = static_cast<BookmarkNode*>(user_data);
  DCHECK(node);

  bar->dragged_node_ = node;
  bar->toolbar_drop_item_ = NULL;

  // Build a windowed representation for our button.
  GtkWidget* window = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_widget_realize(window);

  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_container_add(GTK_CONTAINER(window), frame);
  gtk_widget_show(frame);

  GtkWidget* floating_button = bar->CreateBookmarkButton(node);
  gtk_container_add(GTK_CONTAINER(frame), floating_button);
  gtk_widget_show(floating_button);

  gint x, y;
  gtk_widget_get_pointer(button, &x, &y);
  gtk_drag_set_icon_widget(drag_context, window, x, y);

  // Hide our node
  gtk_widget_hide(button);
}

// static
void BookmarkBarGtk::OnButtonDragEnd(GtkWidget* button,
                                     GdkDragContext* drag_context,
                                     BookmarkBarGtk* bar) {
  // Cleanup everything from this drag
  if (bar->toolbar_drop_item_) {
    g_object_unref(bar->toolbar_drop_item_);
    bar->toolbar_drop_item_ = NULL;
  }

  bar->dragged_node_ = NULL;

  gtk_widget_show(button);
}

// static
gboolean BookmarkBarGtk::OnToolbarExpose(GtkWidget* widget,
                                         GdkEventExpose* event,
                                         BookmarkBarGtk* bar) {
  // A GtkToolbar's expose handler first draws a box. We don't want that so we
  // need to propagate the expose event to all the container's children.
  GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
  while (children) {
    GList *next = children->next;
    gtk_container_propagate_expose(GTK_CONTAINER(widget),
                                   GTK_WIDGET(children->data),
                                   event);
    children = next;
  }
  g_list_free(children);

  return TRUE;
}

// static
gboolean BookmarkBarGtk::OnToolbarDragMotion(GtkToolbar* toolbar,
                                             GdkDragContext* context,
                                             gint x,
                                             gint y,
                                             guint time,
                                             BookmarkBarGtk* bar) {
  if (!bar->toolbar_drop_item_) {
    bar->toolbar_drop_item_ = bar->CreateBookmarkToolItem(bar->dragged_node_);
    g_object_ref_sink(GTK_OBJECT(bar->toolbar_drop_item_));
  }

  gdk_drag_status(context, GDK_ACTION_MOVE, time);
  gint index = gtk_toolbar_get_drop_index(toolbar, x, y);
  gtk_toolbar_set_drop_highlight_item(toolbar,
                                      GTK_TOOL_ITEM(bar->toolbar_drop_item_),
                                      index);
  return TRUE;
}

// static
void BookmarkBarGtk::OnToolbarDragLeave(GtkToolbar* toolbar,
                                        GdkDragContext* context,
                                        guint time,
                                        BookmarkBarGtk* bar) {
  if (bar->toolbar_drop_item_) {
    g_object_unref(bar->toolbar_drop_item_);
    bar->toolbar_drop_item_ = NULL;
  }

  gtk_toolbar_set_drop_highlight_item(toolbar, NULL, 0);
}

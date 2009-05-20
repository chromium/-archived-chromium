// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tabs/tab_gtk.h"

#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/gtk/dnd_registry.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// The targets available for drag n' drop.
GtkTargetEntry target_table[] = {
  { const_cast<char*>("application/x-chrome-tab"), GTK_TARGET_SAME_APP,
    dnd::X_CHROME_TAB }
};

}  // namespace

class TabGtk::ContextMenuController : public MenuGtk::Delegate {
 public:
  explicit ContextMenuController(TabGtk* tab)
      : tab_(tab) {
    menu_.reset(new MenuGtk(this, false));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandNewTab,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_NEWTAB));
    menu_->AppendSeparator();
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandReload,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_RELOAD));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandDuplicate,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_DUPLICATE));
    menu_->AppendSeparator();
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandCloseTab,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_CLOSETAB));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandCloseOtherTabs,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_CLOSEOTHERTABS));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandCloseTabsToRight,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_CLOSETABSTORIGHT));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandCloseTabsOpenedBy,
        l10n_util::GetStringUTF8(IDS_TAB_CXMENU_CLOSETABSOPENEDBY));
    menu_->AppendMenuItemWithLabel(
        TabStripModel::CommandRestoreTab,
        l10n_util::GetStringUTF8(IDS_RESTORE_TAB));
  }

  virtual ~ContextMenuController() {}

  void RunMenu() {
    menu_->PopupAsContext(gtk_get_current_event_time());
  }

  void Cancel() {
    tab_ = NULL;
    menu_->Cancel();
  }

 private:

  // MenuGtk::Delegate implementation:
  virtual bool IsCommandEnabled(int command_id) const {
    if (!tab_)
      return false;

    return tab_->delegate()->IsCommandEnabledForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id), tab_);
  }

  virtual void ExecuteCommand(int command_id) {
    if (!tab_)
      return;

    tab_->delegate()->ExecuteCommandForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id), tab_);
  }

  // The context menu.
  scoped_ptr<MenuGtk> menu_;

  // The Tab the context menu was brought up for. Set to NULL when the menu
  // is canceled.
  TabGtk* tab_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuController);
};

///////////////////////////////////////////////////////////////////////////////
// TabGtk, public:

TabGtk::TabGtk(TabDelegate* delegate)
    : TabRendererGtk(),
      delegate_(delegate),
      closing_(false),
      dragging_(false) {
  event_box_.Own(gtk_event_box_new());
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box_.get()), FALSE);
  gtk_drag_source_set(event_box_.get(), GDK_BUTTON1_MASK,
                      target_table, G_N_ELEMENTS(target_table),
                      GDK_ACTION_MOVE);
  g_signal_connect(G_OBJECT(event_box_.get()), "button-press-event",
                   G_CALLBACK(OnMousePress), this);
  g_signal_connect(G_OBJECT(event_box_.get()), "button-release-event",
                   G_CALLBACK(OnMouseRelease), this);
  g_signal_connect(G_OBJECT(event_box_.get()), "enter-notify-event",
                   G_CALLBACK(OnEnterNotify), this);
  g_signal_connect(G_OBJECT(event_box_.get()), "leave-notify-event",
                   G_CALLBACK(OnLeaveNotify), this);
  g_signal_connect_after(G_OBJECT(event_box_.get()), "drag-begin",
                           G_CALLBACK(OnDragBegin), this);
  g_signal_connect_after(G_OBJECT(event_box_.get()), "drag-end",
                         G_CALLBACK(OnDragEnd), this);
  g_signal_connect_after(G_OBJECT(event_box_.get()), "drag-failed",
                           G_CALLBACK(OnDragFailed), this);
  gtk_widget_add_events(event_box_.get(),
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_LEAVE_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
  gtk_container_add(GTK_CONTAINER(event_box_.get()), TabRendererGtk::widget());
  gtk_widget_show_all(event_box_.get());
}

TabGtk::~TabGtk() {
  if (menu_controller_.get()) {
    // The menu is showing. Close the menu.
    menu_controller_->Cancel();

    // Invoke this so that we hide the highlight.
    ContextMenuClosed();
  }

  event_box_.Destroy();
}

// static
void TabGtk::GdkEventHandler(GdkEvent* event, void* data) {
  TabGtk* tab = static_cast<TabGtk*>(data);

  if (event->type == GDK_MOTION_NOTIFY && tab && tab->dragging_) {
    tab->delegate_->ContinueDrag(NULL);
  }

  gtk_main_do_event(event);
}

// static
gboolean TabGtk::OnMousePress(GtkWidget* widget, GdkEventButton* event,
                              TabGtk* tab) {
  if (event->button == 1) {
    // Store whether or not we were selected just now... we only want to be
    // able to drag foreground tabs, so we don't start dragging the tab if
    // it was in the background.
    bool just_selected = !tab->IsSelected();
    if (just_selected) {
      tab->delegate_->SelectTab(tab);
    }
  }

  return TRUE;
}

// static
gboolean TabGtk::OnMouseRelease(GtkWidget* widget, GdkEventButton* event,
                                TabGtk* tab) {
  if (event->button == 2) {
    tab->delegate_->CloseTab(tab);
  } else if (event->button == 3) {
    tab->ShowContextMenu();
  }

  return TRUE;
}

// static
gboolean TabGtk::OnEnterNotify(GtkWidget* widget, GdkEventCrossing* event,
                               TabGtk* tab) {
  tab->OnMouseEntered();
  return TRUE;
}

// static
gboolean TabGtk::OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event,
                               TabGtk* tab) {
  tab->OnMouseExited();
  return TRUE;
}

// static
void TabGtk::OnDragBegin(GtkWidget* widget, GdkDragContext* context,
                         TabGtk* tab) {
  gdk_event_handler_set(TabGtk::GdkEventHandler, tab, NULL);

  int x, y;
  gdk_window_get_pointer(tab->event_box_.get()->window, &x, &y, NULL);

  // Make the mouse coordinate relative to the tab.
  x -= tab->bounds().x();
  y -= tab->bounds().y();

  tab->dragging_ = true;
  tab->delegate_->MaybeStartDrag(tab, gfx::Point(x, y));
}

// static
void TabGtk::OnDragEnd(GtkWidget* widget, GdkDragContext* context,
                       TabGtk* tab) {
  tab->dragging_ = false;
  // Notify the drag helper that we're done with any potential drag operations.
  // Clean up the drag helper, which is re-created on the next mouse press.
  tab->delegate_->EndDrag(false);

  // Reset the user data pointer for our event handler.
  gdk_event_handler_set(TabGtk::GdkEventHandler, NULL, NULL);
}

// static
gboolean TabGtk::OnDragMotion(GtkWidget* widget,
                              GdkDragContext* context,
                              guint x, guint y,
                              guint time,
                              TabGtk* tab) {
  tab->delegate_->ContinueDrag(context);
  return TRUE;
}

// static
gboolean TabGtk::OnDragFailed(GtkWidget* widget, GdkDragContext* context,
                              GtkDragResult result,
                              TabGtk* tab) {
  tab->delegate_->EndDrag(false);
  return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// TabGtk, TabRendererGtk overrides:

bool TabGtk::IsSelected() const {
  return delegate_->IsTabSelected(this);
}

bool TabGtk::IsVisible() const {
  return GTK_WIDGET_FLAGS(event_box_.get()) & GTK_VISIBLE;
}

void TabGtk::SetVisible(bool visible) const {
  if (visible) {
    gtk_widget_show(event_box_.get());
  } else {
    gtk_widget_hide(event_box_.get());
  }
}

void TabGtk::CloseButtonClicked() {
  delegate_->CloseTab(this);
}

///////////////////////////////////////////////////////////////////////////////
// TabGtk, private:

void TabGtk::ShowContextMenu() {
  if (!menu_controller_.get())
    menu_controller_.reset(new ContextMenuController(this));

  menu_controller_->RunMenu();
}

void TabGtk::ContextMenuClosed() {
  delegate()->StopAllHighlighting();
  menu_controller_.reset();
}

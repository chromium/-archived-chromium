// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tab_contents_container_gtk.h"

#include "base/gfx/native_widget_types.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/common/notification_service.h"


TabContentsContainerGtk::TabContentsContainerGtk()
    : tab_contents_(NULL),
      vbox_(gtk_vbox_new(FALSE, 0)) {
}

TabContentsContainerGtk::~TabContentsContainerGtk() {
  if (tab_contents_)
    RemoveObservers();
}

void TabContentsContainerGtk::AddContainerToBox(GtkWidget* box) {
  gtk_box_pack_start(GTK_BOX(box), vbox_, TRUE, TRUE, 0);
}

void TabContentsContainerGtk::SetTabContents(TabContents* tab_contents) {
  if (tab_contents_) {
    gfx::NativeView widget = tab_contents_->GetNativeView();
    if (widget)
      gtk_container_remove(GTK_CONTAINER(vbox_), widget);

    tab_contents_->WasHidden();

    RemoveObservers();
  }

  tab_contents_ = tab_contents;

  // When detaching the last tab of the browser SetTabContents is invoked
  // with NULL. Don't attempt to do anything in that case.
  if (tab_contents_) {
    AddObservers();

    gfx::NativeView widget = tab_contents_->GetNativeView();
    if (widget) {
      gtk_box_pack_start(GTK_BOX(vbox_), widget, TRUE, TRUE, 0);
      gtk_widget_show_all(widget);
    }
  }
}

void TabContentsContainerGtk::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  if (type == NotificationType::RENDER_VIEW_HOST_CHANGED) {
    RenderViewHostSwitchedDetails* switched_details =
        Details<RenderViewHostSwitchedDetails>(details).ptr();
    RenderViewHostChanged(switched_details->old_host,
                          switched_details->new_host);
  } else if (type == NotificationType::TAB_CONTENTS_DESTROYED) {
    TabContentsDestroyed(Source<TabContents>(source).ptr());
  } else {
    NOTREACHED();
  }
}

void TabContentsContainerGtk::AddObservers() {
  DCHECK(tab_contents_);
  if (tab_contents_->AsWebContents()) {
    // WebContents can change their RenderViewHost and hence the GtkWidget that
    // is shown. I'm not entirely sure that we need to observe this event under
    // GTK, but am putting a stub implementation and a comment saying that if
    // we crash after that NOTIMPLEMENTED(), we'll need it.
    NotificationService::current()->AddObserver(
        this, NotificationType::RENDER_VIEW_HOST_CHANGED,
        Source<NavigationController>(tab_contents_->controller()));
  }
  NotificationService::current()->AddObserver(
      this,
      NotificationType::TAB_CONTENTS_DESTROYED,
      Source<TabContents>(tab_contents_));
}

void TabContentsContainerGtk::RemoveObservers() {
  DCHECK(tab_contents_);
  if (tab_contents_->AsWebContents()) {
    NotificationService::current()->RemoveObserver(
        this,
        NotificationType::RENDER_VIEW_HOST_CHANGED,
        Source<NavigationController>(tab_contents_->controller()));
  }
  NotificationService::current()->RemoveObserver(
      this,
      NotificationType::TAB_CONTENTS_DESTROYED,
      Source<TabContents>(tab_contents_));
}

void TabContentsContainerGtk::RenderViewHostChanged(RenderViewHost* old_host,
                                                    RenderViewHost* new_host) {
  // TODO(port): Remove this method and the logic where we subscribe to the
  // RENDER_VIEW_HOST_CHANGED notification. This was used on Windows for focus
  // issues, and I'm not entirely convinced that this isn't necessary.
}

void TabContentsContainerGtk::TabContentsDestroyed(TabContents* contents) {
  // Sometimes, a TabContents is destroyed before we know about it. This allows
  // us to clean up our state in case this happens.
  DCHECK(contents == tab_contents_);
  SetTabContents(NULL);
}

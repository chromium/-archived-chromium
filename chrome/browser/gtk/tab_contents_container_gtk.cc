// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tab_contents_container_gtk.h"

#include "base/gfx/native_widget_types.h"
#include "chrome/browser/gtk/gtk_floating_container.h"
#include "chrome/browser/gtk/status_bubble_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/common/notification_service.h"

namespace {

// Allocates all normal tab contents views to the size of the passed in
// |allocation|.
void ResizeChildren(GtkWidget* widget, void* param) {
  GtkAllocation* allocation = reinterpret_cast<GtkAllocation*>(param);

  if (widget->allocation.width != allocation->width ||
      widget->allocation.height != allocation->height) {
    gtk_widget_set_size_request(widget, allocation->width,
                                allocation->height);
  }
}

}  // namespace

TabContentsContainerGtk::TabContentsContainerGtk(StatusBubbleGtk* status_bubble)
    : tab_contents_(NULL),
      status_bubble_(status_bubble) {
  Init();
}

TabContentsContainerGtk::~TabContentsContainerGtk() {
  floating_.Destroy();
}

void TabContentsContainerGtk::Init() {
  // A high level overview of the TabContentsContainer:
  //
  // +- GtkFloatingContainer |floating_| -------------------------------+
  // |+- GtkFixedContainer |fixed_| -----------------------------------+|
  // ||                                                                ||
  // ||                                                                ||
  // ||                                                                ||
  // ||                                                                ||
  // |+- (StatusBubble) ------+                                        ||
  // |+                       +                                        ||
  // |+-----------------------+----------------------------------------+|
  // +------------------------------------------------------------------+

  floating_.Own(gtk_floating_container_new());

  fixed_ = gtk_fixed_new();
  g_signal_connect(fixed_, "size-allocate",
                   G_CALLBACK(OnFixedSizeAllocate), this);
  gtk_container_add(GTK_CONTAINER(floating_.get()), fixed_);

  gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(floating_.get()),
                                      status_bubble_->widget());
  g_signal_connect(floating_.get(), "set-floating-position",
                   G_CALLBACK(OnSetFloatingPosition), this);

  gtk_widget_show(fixed_);
  gtk_widget_show(floating_.get());
}

void TabContentsContainerGtk::AddContainerToBox(GtkWidget* box) {
  gtk_box_pack_start(GTK_BOX(box), floating_.get(), TRUE, TRUE, 0);
}

void TabContentsContainerGtk::SetTabContents(TabContents* tab_contents) {
  if (tab_contents_) {
    gfx::NativeView widget = tab_contents_->GetNativeView();
    if (widget)
      gtk_widget_hide(widget);

    tab_contents_->WasHidden();

    registrar_.Remove(this, NotificationType::RENDER_VIEW_HOST_CHANGED,
        Source<NavigationController>(&tab_contents_->controller()));
    registrar_.Remove(this, NotificationType::TAB_CONTENTS_DESTROYED,
                      Source<TabContents>(tab_contents_));
  }

  tab_contents_ = tab_contents;

  // When detaching the last tab of the browser SetTabContents is invoked
  // with NULL. Don't attempt to do anything in that case.
  if (tab_contents_) {
    // TabContents can change their RenderViewHost and hence the GtkWidget that
    // is shown. I'm not entirely sure that we need to observe this event under
    // GTK, but am putting a stub implementation and a comment saying that if
    // we crash after that NOTIMPLEMENTED(), we'll need it.
    registrar_.Add(this, NotificationType::RENDER_VIEW_HOST_CHANGED,
                   Source<NavigationController>(&tab_contents_->controller()));
    registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(tab_contents_));

    gfx::NativeView widget = tab_contents_->GetNativeView();
    if (widget) {
      if (widget->parent != fixed_)
        gtk_fixed_put(GTK_FIXED(fixed_), widget, 0, 0);
      gtk_widget_show(widget);
    }

    // We need to make sure that we are below the findbar.
    // Sometimes the content native view will be null.
    // TODO(estade): will this case ever cause findbar occlusion problems?
    if (tab_contents_->GetContentNativeView()) {
      GdkWindow* content_gdk_window =
          tab_contents_->GetContentNativeView()->window;
      if (content_gdk_window)
        gdk_window_lower(content_gdk_window);
    }
  }
}

void TabContentsContainerGtk::DetachTabContents(TabContents* tab_contents) {
  gfx::NativeView widget = tab_contents->GetNativeView();
  // It is possible to detach an unrealized, unparented TabContents if you
  // slow things down enough in valgrind. Might happen in the real world, too.
  if (widget && widget->parent) {
    DCHECK_EQ(widget->parent, fixed_);
    gtk_container_remove(GTK_CONTAINER(fixed_), widget);
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

// static
void TabContentsContainerGtk::OnFixedSizeAllocate(
    GtkWidget* fixed,
    GtkAllocation* allocation,
    TabContentsContainerGtk* container) {
  // Set all the tab contents GtkWidgets to the size of the allocation.
  gtk_container_foreach(GTK_CONTAINER(fixed), ResizeChildren, allocation);
}

// static
void TabContentsContainerGtk::OnSetFloatingPosition(
    GtkFloatingContainer* floating_container, GtkAllocation* allocation,
    TabContentsContainerGtk* tab_contents_container) {
  GtkWidget* widget = tab_contents_container->status_bubble_->widget();

  // Look at the size request of the status bubble and tell the
  // GtkFloatingContainer where we want it positioned.
  GtkRequisition requisition;
  gtk_widget_size_request(widget, &requisition);

  GValue value = { 0, };
  g_value_init(&value, G_TYPE_INT);
  g_value_set_int(&value, 0);
  // TODO(erg): Since we're absolutely positioning stuff, we probably have to
  // do manual RTL right here.
  gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                   widget, "x", &value);

  int child_y = std::max(
      allocation->y + allocation->height - requisition.height, 0);
  g_value_set_int(&value, child_y);
  gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                   widget, "y", &value);
  g_value_unset(&value);
}

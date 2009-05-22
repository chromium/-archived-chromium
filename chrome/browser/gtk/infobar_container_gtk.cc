// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/infobar_container_gtk.h"

#include <gtk/gtk.h>

#include "chrome/browser/browser_window.h"
#include "chrome/browser/gtk/infobar_gtk.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"

namespace {

// If |infobar_widget| matches |info_bar_delegate|, then close the infobar.
void AnimateClosingForDelegate(GtkWidget* infobar_widget,
                               gpointer info_bar_delegate) {
  InfoBarDelegate* delegate =
      static_cast<InfoBarDelegate*>(info_bar_delegate);
  InfoBar* infobar = reinterpret_cast<InfoBar*>(
      g_object_get_data(G_OBJECT(infobar_widget), "info-bar"));

  if (!infobar) {
    NOTREACHED();
    return;
  }

  if (delegate == infobar->delegate())
    infobar->AnimateClose();
}

// Get the height of the widget and add it to |userdata|, but only if it is in
// the process of closing.
void SumClosingBarHeight(GtkWidget* widget, gpointer userdata) {
  int* height_sum = static_cast<int*>(userdata);
  InfoBar* infobar = reinterpret_cast<InfoBar*>(
      g_object_get_data(G_OBJECT(widget), "info-bar"));
  if (infobar->IsClosing())
    *height_sum += widget->allocation.height;
}

}  // namespace

// InfoBarContainerGtk, public: ------------------------------------------------

InfoBarContainerGtk::InfoBarContainerGtk(BrowserWindow* browser_window)
    : browser_window_(browser_window),
      tab_contents_(NULL),
      container_(gtk_vbox_new(FALSE, 0)) {
  gtk_widget_show(widget());
}

InfoBarContainerGtk::~InfoBarContainerGtk() {
  browser_window_ = NULL;
  ChangeTabContents(NULL);

  container_.Destroy();
}

void InfoBarContainerGtk::ChangeTabContents(TabContents* contents) {
  if (tab_contents_)
    registrar_.RemoveAll();

  gtk_util::RemoveAllChildren(widget());

  tab_contents_ = contents;
  if (tab_contents_) {
    UpdateInfoBars();
    Source<TabContents> source(tab_contents_);
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_ADDED, source);
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
                   source);
  }
}

void InfoBarContainerGtk::RemoveDelegate(InfoBarDelegate* delegate) {
  tab_contents_->RemoveInfoBar(delegate);
}

int InfoBarContainerGtk::TotalHeightOfClosingBars() const {
  int sum = 0;
  gtk_container_foreach(GTK_CONTAINER(widget()), SumClosingBarHeight, &sum);
  return sum;
}

// InfoBarContainerGtk, NotificationObserver implementation: -------------------

void InfoBarContainerGtk::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  if (type == NotificationType::TAB_CONTENTS_INFOBAR_ADDED) {
    AddInfoBar(Details<InfoBarDelegate>(details).ptr(), true);
  } else if (type == NotificationType::TAB_CONTENTS_INFOBAR_REMOVED) {
    RemoveInfoBar(Details<InfoBarDelegate>(details).ptr());
  } else {
    NOTREACHED();
  }
}

// InfoBarContainerGtk, private: -----------------------------------------------

void InfoBarContainerGtk::UpdateInfoBars() {
  for (int i = 0; i < tab_contents_->infobar_delegate_count(); ++i) {
    InfoBarDelegate* delegate = tab_contents_->GetInfoBarDelegateAt(i);
    AddInfoBar(delegate, false);
  }
}

void InfoBarContainerGtk::AddInfoBar(InfoBarDelegate* delegate, bool animate) {
  InfoBar* infobar = delegate->CreateInfoBar();
  infobar->set_container(this);
  gtk_box_pack_end(GTK_BOX(widget()), infobar->widget(),
                   FALSE, FALSE, 0);
  if (animate)
    infobar->AnimateOpen();
  else
    infobar->Open();
}

void InfoBarContainerGtk::RemoveInfoBar(InfoBarDelegate* delegate) {
  gtk_container_foreach(GTK_CONTAINER(widget()),
                        AnimateClosingForDelegate, delegate);
}

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

// Used by RemoveInfoBar to pass data to AnimateClosingForDelegate.
struct RemoveInfoBarData {
  GtkWidget* container;
  InfoBarDelegate* delegate;
};

void AnimateClosingForDelegate(GtkWidget* infobar_widget,
                               gpointer remove_info_bar_data) {
  InfoBar* infobar = reinterpret_cast<InfoBar*>(
      g_object_get_data(G_OBJECT(infobar_widget), "info-bar"));
  RemoveInfoBarData* data =
      reinterpret_cast<RemoveInfoBarData*>(remove_info_bar_data);

  if (!infobar) {
    NOTREACHED();
    return;
  }

  if (data->delegate == infobar->delegate())
    infobar->AnimateClose();
}

}  // namespace

// InfoBarContainerGtk, public: ------------------------------------------------

InfoBarContainerGtk::InfoBarContainerGtk(BrowserWindow* browser_window)
    : browser_window_(browser_window),
      tab_contents_(NULL),
      container_(gtk_vbox_new(FALSE, 0)) {
  gtk_widget_show(container_.get());
}

InfoBarContainerGtk::~InfoBarContainerGtk() {
  browser_window_ = NULL;
  ChangeTabContents(NULL);

  container_.Destroy();
}

void InfoBarContainerGtk::ChangeTabContents(TabContents* contents) {
  if (tab_contents_) {
    NotificationService::current()->RemoveObserver(
        this, NotificationType::TAB_CONTENTS_INFOBAR_ADDED,
        Source<TabContents>(tab_contents_));
    NotificationService::current()->RemoveObserver(
        this, NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
        Source<TabContents>(tab_contents_));
  }

  gtk_util::RemoveAllChildren(container_.get());

  tab_contents_ = contents;
  if (tab_contents_) {
    UpdateInfoBars();
    NotificationService::current()->AddObserver(
        this, NotificationType::TAB_CONTENTS_INFOBAR_ADDED,
        Source<TabContents>(tab_contents_));
    NotificationService::current()->AddObserver(
        this, NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
        Source<TabContents>(tab_contents_));
  }
}

void InfoBarContainerGtk::RemoveDelegate(InfoBarDelegate* delegate) {
  tab_contents_->RemoveInfoBar(delegate);
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
  gtk_box_pack_end(GTK_BOX(container_.get()), infobar->widget(),
                   FALSE, FALSE, 0);
  if (animate)
    infobar->AnimateOpen();
  else
    infobar->Open();
}

void InfoBarContainerGtk::RemoveInfoBar(InfoBarDelegate* delegate) {
  RemoveInfoBarData remove_info_bar_data = { container_.get(), delegate };

  gtk_container_foreach(GTK_CONTAINER(container_.get()),
                        AnimateClosingForDelegate, &remove_info_bar_data);
}

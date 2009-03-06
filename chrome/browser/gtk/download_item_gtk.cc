// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/download_item_gtk.h"

#include "base/basictypes.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/common/resource_bundle.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// The width of the |menu_button_| widget. It has to be at least as wide as the
// bitmap that we use to draw it, i.e. 16, but can be more.
const int kMenuButtonWidth = 16;

}  // namespace

// DownloadShelfContextMenuGtk -------------------------------------------------

class DownloadShelfContextMenuGtk : public DownloadShelfContextMenu,
                                    public MenuGtk::Delegate {
 public:
  // The constructor creates the menu and immediately pops it up.
  // |model| is the download item model associated with this context menu,
  // |widget| is the button that popped up this context menu, and |e| is
  // the button press event that caused this menu to be created.
  DownloadShelfContextMenuGtk(BaseDownloadItemModel* model)
      : DownloadShelfContextMenu(model),
        menu_is_for_complete_download_(false) {
  }

  ~DownloadShelfContextMenuGtk() {
  }

  void Popup(GtkWidget* widget, GdkEvent* event) {
    // Create the menu if we have not created it yet or we created it for
    // an in-progress download that has since completed.
    bool download_is_complete = download_->state() == DownloadItem::COMPLETE;
    if (menu_.get() == NULL ||
        (download_is_complete && !menu_is_for_complete_download_)) {
      menu_.reset(new MenuGtk(this, download_is_complete ?
                  finished_download_menu : in_progress_download_menu));
      menu_is_for_complete_download_ = download_is_complete;
    }
    menu_->Popup(widget, event);
  }

  // MenuGtk::Delegate implementation ------------------------------------------
  virtual bool IsCommandEnabled(int id) const {
    return IsItemCommandEnabled(id);
  }

  virtual bool IsItemChecked(int id) const {
    return ItemIsChecked(id);
  }

  virtual void ExecuteCommand(int id) {
    return ExecuteItemCommand(id);
  }

 private:
  // The menu we show on Popup(). We keep a pointer to it for a couple reasons:
  //  * we don't want to have to recreate the menu every time it's popped up.
  //  * we have to keep it in scope for longer than the duration of Popup(), or
  //    completing the user-selected action races against the menu's
  //    destruction.
  scoped_ptr<MenuGtk> menu_;

  // If true, the MenuGtk in |menu_| refers to a finished download menu.
  bool menu_is_for_complete_download_;

  // We show slightly different menus if the download is in progress vs. if the
  // download has finished.
  static MenuCreateMaterial in_progress_download_menu[];

  static MenuCreateMaterial finished_download_menu[];
};

MenuCreateMaterial DownloadShelfContextMenuGtk::finished_download_menu[] = {
  { MENU_NORMAL, OPEN_WHEN_COMPLETE, IDS_DOWNLOAD_MENU_OPEN, 0, NULL },
  { MENU_CHECKBOX, ALWAYS_OPEN_TYPE, IDS_DOWNLOAD_MENU_ALWAYS_OPEN_TYPE,
    0, NULL},
  { MENU_SEPARATOR, 0, 0, 0, NULL },
  { MENU_NORMAL, SHOW_IN_FOLDER, IDS_DOWNLOAD_LINK_SHOW, 0, NULL},
  { MENU_SEPARATOR, 0, 0, 0, NULL },
  { MENU_NORMAL, CANCEL, IDS_DOWNLOAD_MENU_CANCEL, 0, NULL},
  { MENU_END, 0, 0, 0, NULL },
};

MenuCreateMaterial DownloadShelfContextMenuGtk::in_progress_download_menu[] = {
  { MENU_CHECKBOX, OPEN_WHEN_COMPLETE, IDS_DOWNLOAD_MENU_OPEN_WHEN_COMPLETE,
    0, NULL },
  { MENU_CHECKBOX, ALWAYS_OPEN_TYPE, IDS_DOWNLOAD_MENU_ALWAYS_OPEN_TYPE,
    0, NULL},
  { MENU_SEPARATOR, 0, 0, 0, NULL },
  { MENU_NORMAL, SHOW_IN_FOLDER, IDS_DOWNLOAD_LINK_SHOW, 0, NULL},
  { MENU_SEPARATOR, 0, 0, 0, NULL },
  { MENU_NORMAL, CANCEL, IDS_DOWNLOAD_MENU_CANCEL, 0, NULL},
  { MENU_END, 0, 0, 0, NULL },
};

// DownloadItemGtk -------------------------------------------------------------

NineBox* DownloadItemGtk::body_nine_box_normal_ = NULL;
NineBox* DownloadItemGtk::body_nine_box_prelight_ = NULL;
NineBox* DownloadItemGtk::body_nine_box_active_ = NULL;

NineBox* DownloadItemGtk::menu_nine_box_normal_ = NULL;
NineBox* DownloadItemGtk::menu_nine_box_prelight_ = NULL;
NineBox* DownloadItemGtk::menu_nine_box_active_ = NULL;

DownloadItemGtk::DownloadItemGtk(BaseDownloadItemModel* download_model,
                                 GtkWidget* parent_shelf)
    : download_model_(download_model),
      parent_shelf_(parent_shelf) {
  InitNineBoxes();

  body_ = gtk_button_new();
  gtk_widget_set_app_paintable(body_, TRUE);
  g_signal_connect(G_OBJECT(body_), "expose-event",
                   G_CALLBACK(OnExpose), this);
  GTK_WIDGET_UNSET_FLAGS(body_, GTK_CAN_FOCUS);
  // TODO(estade): gtk_label_new() expects UTF8, but FilePath may have a
  // different encoding on linux.
  GtkWidget* label = gtk_label_new(
      download_model->download()->GetFileName().value().c_str());
  gtk_container_add(GTK_CONTAINER(body_), label);

  menu_button_ = gtk_button_new();
  gtk_widget_set_app_paintable(menu_button_, TRUE);
  GTK_WIDGET_UNSET_FLAGS(menu_button_, GTK_CAN_FOCUS);
  g_signal_connect(G_OBJECT(menu_button_), "expose-event",
                   G_CALLBACK(OnExpose), this);
  g_signal_connect(G_OBJECT(menu_button_), "button-press-event",
                   G_CALLBACK(OnMenuButtonPressEvent), this);
  g_object_set_data(G_OBJECT(menu_button_), "left-align-popup",
                    reinterpret_cast<void*>(true));
  gtk_widget_set_size_request(menu_button_, kMenuButtonWidth, 0);

  hbox_ = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_), body_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_), menu_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(parent_shelf), hbox_, FALSE, FALSE, 0);
  gtk_widget_show_all(hbox_);
}

DownloadItemGtk::~DownloadItemGtk() {
}

// static
void DownloadItemGtk::InitNineBoxes() {
  if (body_nine_box_normal_)
    return;

  GdkPixbuf* images[9];
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();

  int i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_TOP);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_TOP);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_TOP);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM);
  body_nine_box_normal_ = new NineBox(images);

  i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_TOP_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_TOP_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_H);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_H);
  body_nine_box_prelight_ = new NineBox(images);

  i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_TOP_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_TOP_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_P);
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_P);
  body_nine_box_active_ = new NineBox(images);

  i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_TOP);
  images[i++] = NULL;
  images[i++] = NULL;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE);
  images[i++] = NULL;
  images[i++] = NULL;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM);
  images[i++] = NULL;
  images[i++] = NULL;
  menu_nine_box_normal_ = new NineBox(images);

  i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_TOP_H);
  images[i++] = NULL;
  images[i++] = NULL;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_H);
  images[i++] = NULL;
  images[i++] = NULL;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_H);
  images[i++] = NULL;
  images[i++] = NULL;
  menu_nine_box_prelight_ = new NineBox(images);

  i = 0;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_TOP_P);
  images[i++] = NULL;
  images[i++] = NULL;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_P);
  images[i++] = NULL;
  images[i++] = NULL;
  images[i++] = rb.LoadPixbuf(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_P);
  images[i++] = NULL;
  images[i++] = NULL;
  menu_nine_box_active_ = new NineBox(images);
}

// static
gboolean DownloadItemGtk::OnExpose(GtkWidget* widget, GdkEventExpose* e,
                                   DownloadItemGtk* download_item) {
  NineBox* nine_box = NULL;
  // If true, this widget is |body_|, otherwise it is |menu_button_|.
  bool is_body = widget == download_item->body_;
  if (GTK_WIDGET_STATE(widget) == GTK_STATE_PRELIGHT)
    nine_box = is_body ? body_nine_box_prelight_ : menu_nine_box_prelight_;
  else if (GTK_WIDGET_STATE(widget) == GTK_STATE_ACTIVE)
    nine_box = is_body ? body_nine_box_active_ : menu_nine_box_active_;
  else
    nine_box = is_body ? body_nine_box_normal_ : menu_nine_box_normal_;

  GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                     true,  // alpha
                                     8,  // bits per channel
                                     widget->allocation.width,
                                     widget->allocation.height);

  nine_box->RenderToPixbuf(pixbuf);

  gdk_draw_pixbuf(widget->window,
                  widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                  pixbuf,
                  0, 0,
                  widget->allocation.x, widget->allocation.y, -1, -1,
                  GDK_RGB_DITHER_NONE, 0, 0);

  gdk_pixbuf_unref(pixbuf);

  GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
  if (child)
    gtk_container_propagate_expose(GTK_CONTAINER(widget), child, e);

  return TRUE;
}

gboolean DownloadItemGtk::OnMenuButtonPressEvent(GtkWidget* button,
                                                 GdkEvent* event,
                                                 DownloadItemGtk* item) {
  // TODO(port): this never puts the button into the "active" state,
  // so this may need to be changed. See note in BrowserToolbarGtk.
  if (event->type == GDK_BUTTON_PRESS) {
    GdkEventButton* event_button = reinterpret_cast<GdkEventButton*>(event);
    if (event_button->button == 1) {
      if (item->menu_.get() == NULL) {
        item->menu_.reset(new DownloadShelfContextMenuGtk(
            item->download_model_.get()));
      }
      item->menu_->Popup(button, event);
    }
  }

  return FALSE;
}


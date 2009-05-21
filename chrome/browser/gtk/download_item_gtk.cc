// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/download_item_gtk.h"

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/gfx/text_elider.h"
#include "app/slide_animation.h"
#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/gtk/download_shelf_gtk.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/common/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

// The width of the |menu_button_| widget. It has to be at least as wide as the
// bitmap that we use to draw it, i.e. 16, but can be more.
const int kMenuButtonWidth = 16;

// Amount of space we allot to showing the filename. If the filename is too wide
// it will be elided.
const int kTextWidth = 140;

// The minimum width we will ever draw the download item. Used as a lower bound
// during animation. This number comes from the width of the images used to
// make the download item.
const int kMinDownloadItemWidth = 13 + download_util::kSmallProgressIconSize;

const char* kLabelColorMarkup = "<span color='#%s'>%s</span>";
const char* kFilenameColor = "576C95";  // 87, 108, 149
const char* kStatusColor = "7B8DAE";  // 123, 141, 174

// New download item animation speed in milliseconds.
const int kNewItemAnimationDurationMs = 800;

// How long the 'download complete' animation should last for.
const int kCompleteAnimationDurationMs = 2500;

}  // namespace

// DownloadShelfContextMenuGtk -------------------------------------------------

class DownloadShelfContextMenuGtk : public DownloadShelfContextMenu,
                                    public MenuGtk::Delegate {
 public:
  // The constructor creates the menu and immediately pops it up.
  // |model| is the download item model associated with this context menu,
  // |widget| is the button that popped up this context menu, and |e| is
  // the button press event that caused this menu to be created.
  DownloadShelfContextMenuGtk(BaseDownloadItemModel* model,
                              DownloadItemGtk* download_item)
      : DownloadShelfContextMenu(model),
        download_item_(download_item),
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
          finished_download_menu : in_progress_download_menu, NULL));
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

  virtual void StoppedShowing() {
    download_item_->menu_showing_ = false;
    gtk_widget_queue_draw(download_item_->menu_button_);
  }

 private:
  // The menu we show on Popup(). We keep a pointer to it for a couple reasons:
  //  * we don't want to have to recreate the menu every time it's popped up.
  //  * we have to keep it in scope for longer than the duration of Popup(), or
  //    completing the user-selected action races against the menu's
  //    destruction.
  scoped_ptr<MenuGtk> menu_;

  // The download item that created us.
  DownloadItemGtk* download_item_;

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

DownloadItemGtk::DownloadItemGtk(DownloadShelfGtk* parent_shelf,
                                 BaseDownloadItemModel* download_model)
    : parent_shelf_(parent_shelf),
      menu_showing_(false),
      progress_angle_(download_util::kStartAngleDegrees),
      download_model_(download_model),
      bounding_widget_(parent_shelf->GetRightBoundingWidget()),
      icon_(NULL) {
  InitNineBoxes();
  LoadIcon();

  body_ = gtk_button_new();
  gtk_widget_set_app_paintable(body_, TRUE);
  g_signal_connect(body_, "expose-event",
                   G_CALLBACK(OnExpose), this);
  GTK_WIDGET_UNSET_FLAGS(body_, GTK_CAN_FOCUS);
  // Remove internal padding on the button.
  GtkRcStyle* no_padding_style = gtk_rc_style_new();
  no_padding_style->xthickness = 0;
  no_padding_style->ythickness = 0;
  gtk_widget_modify_style(body_, no_padding_style);
  g_object_unref(no_padding_style);

  name_label_ = gtk_label_new(NULL);

  // TODO(estade): This is at best an educated guess, since we don't actually
  // use gfx::Font() to draw the text. This is why we need to add so
  // much padding when we set the size request. We need to either use gfx::Font
  // or somehow extend TextElider.
  std::wstring elided_filename = gfx::ElideFilename(
      download_model_->download()->GetFileName(),
      gfx::Font(), kTextWidth);
  gchar* label_markup =
      g_markup_printf_escaped(kLabelColorMarkup, kFilenameColor,
                              WideToUTF8(elided_filename).c_str());
  gtk_label_set_markup(GTK_LABEL(name_label_), label_markup);
  g_free(label_markup);

  status_label_ = gtk_label_new(NULL);
  // Left align and vertically center the labels.
  gtk_misc_set_alignment(GTK_MISC(name_label_), 0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(status_label_), 0, 0.5);
  // Until we switch to vector graphics, force the font size.
  gtk_util::ForceFontSizePixels(name_label_, 13.4); // 13.4px == 10pt @ 96dpi
  gtk_util::ForceFontSizePixels(status_label_, 13.4); // 13.4px == 10pt @ 96dpi

  // Stack the labels on top of one another.
  GtkWidget* text_stack = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(text_stack), name_label_, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(text_stack), status_label_, FALSE, FALSE, 0);

  // We use a GtkFixed because we don't want it to have its own window.
  // This choice of widget is not critically important though.
  progress_area_ = gtk_fixed_new();
  gtk_widget_set_size_request(progress_area_,
      download_util::kSmallProgressIconSize,
      download_util::kSmallProgressIconSize);
  gtk_widget_set_app_paintable(progress_area_, TRUE);
  g_signal_connect(progress_area_, "expose-event",
                   G_CALLBACK(OnProgressAreaExpose), this);

  // Put the download progress icon on the left of the labels.
  GtkWidget* body_hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(body_), body_hbox);
  gtk_box_pack_start(GTK_BOX(body_hbox), progress_area_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(body_hbox), text_stack, TRUE, TRUE, 0);

  menu_button_ = gtk_button_new();
  gtk_widget_set_app_paintable(menu_button_, TRUE);
  GTK_WIDGET_UNSET_FLAGS(menu_button_, GTK_CAN_FOCUS);
  g_signal_connect(menu_button_, "expose-event",
                   G_CALLBACK(OnExpose), this);
  g_signal_connect(menu_button_, "button-press-event",
                   G_CALLBACK(OnMenuButtonPressEvent), this);
  g_object_set_data(G_OBJECT(menu_button_), "left-align-popup",
                    reinterpret_cast<void*>(true));
  gtk_widget_set_size_request(menu_button_, kMenuButtonWidth, 0);

  GtkWidget* shelf_hbox = parent_shelf->GetHBox();
  hbox_ = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_), body_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_), menu_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(shelf_hbox), hbox_, FALSE, FALSE, 0);
  // Insert as the leftmost item.
  gtk_box_reorder_child(GTK_BOX(shelf_hbox), hbox_, 1);

  resize_handler_id_ = g_signal_connect(G_OBJECT(shelf_hbox), "size-allocate",
                                        G_CALLBACK(OnShelfResized), this);

  download_model_->download()->AddObserver(this);

  new_item_animation_.reset(new SlideAnimation(this));
  new_item_animation_->SetSlideDuration(kNewItemAnimationDurationMs);
  gtk_widget_show_all(hbox_);
  new_item_animation_->Show();
}

DownloadItemGtk::~DownloadItemGtk() {
  StopDownloadProgress();
  g_signal_handler_disconnect(parent_shelf_->GetHBox(), resize_handler_id_);
  gtk_widget_destroy(hbox_);
  download_model_->download()->RemoveObserver(this);
}

void DownloadItemGtk::OnDownloadUpdated(DownloadItem* download) {
  DCHECK_EQ(download, download_model_->download());

  switch (download->state()) {
    case DownloadItem::REMOVING:
      parent_shelf_->RemoveDownloadItem(this);  // This will delete us!
      return;
    case DownloadItem::CANCELLED:
      StopDownloadProgress();
      break;
    case DownloadItem::COMPLETE:
      StopDownloadProgress();
      complete_animation_.reset(new SlideAnimation(this));
      complete_animation_->SetSlideDuration(kCompleteAnimationDurationMs);
      complete_animation_->SetTweenType(SlideAnimation::NONE);
      complete_animation_->Show();
      break;
    case DownloadItem::IN_PROGRESS:
      download_model_->download()->is_paused() ?
          StopDownloadProgress() : StartDownloadProgress();
      break;
    default:
      NOTREACHED();
  }

  // Now update the status label. We may have already removed it; if so, we
  // do nothing.
  if (!status_label_) {
    return;
  }

  std::wstring status_text = download_model_->GetStatusText();
  // Remove the status text label.
  if (status_text.empty()) {
    gtk_widget_destroy(status_label_);
    status_label_ = NULL;
    return;
  }

  gchar* label_markup =
      g_markup_printf_escaped(kLabelColorMarkup, kStatusColor,
                              WideToUTF8(status_text).c_str());
  gtk_label_set_markup(GTK_LABEL(status_label_), label_markup);
  g_free(label_markup);
}

void DownloadItemGtk::AnimationProgressed(const Animation* animation) {
  if (animation == complete_animation_.get()) {
    gtk_widget_queue_draw(progress_area_);
  } else {
    DCHECK(animation == new_item_animation_.get());
    // See above TODO for explanation of the extra 50.
    int showing_width = std::max(kMinDownloadItemWidth,
        static_cast<int>((kTextWidth + 50 +
                         download_util::kSmallProgressIconSize) *
                         new_item_animation_->GetCurrentValue()));
    showing_width = std::max(showing_width, kMinDownloadItemWidth);
    gtk_widget_set_size_request(body_, showing_width, -1);
  }
}

// Download progress animation functions.

void DownloadItemGtk::UpdateDownloadProgress() {
  progress_angle_ = (progress_angle_ +
                     download_util::kUnknownIncrementDegrees) %
                    download_util::kMaxDegrees;
  gtk_widget_queue_draw(progress_area_);
}

void DownloadItemGtk::StartDownloadProgress() {
  if (progress_timer_.IsRunning())
    return;
  progress_timer_.Start(
      base::TimeDelta::FromMilliseconds(download_util::kProgressRateMs), this,
      &DownloadItemGtk::UpdateDownloadProgress);
}

void DownloadItemGtk::StopDownloadProgress() {
  progress_timer_.Stop();
}

// Icon loading functions.

void DownloadItemGtk::OnLoadIconComplete(IconManager::Handle handle,
                                         SkBitmap* icon_bitmap) {
  icon_ = icon_bitmap;
  gtk_widget_queue_draw(progress_area_);
}

void DownloadItemGtk::LoadIcon() {
  IconManager* im = g_browser_process->icon_manager();
  im->LoadIcon(download_model_->download()->full_path(),
               IconLoader::SMALL, &icon_consumer_,
               NewCallback(this, &DownloadItemGtk::OnLoadIconComplete));
}

// static
void DownloadItemGtk::InitNineBoxes() {
  if (body_nine_box_normal_)
    return;

  body_nine_box_normal_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_LEFT_TOP,
      IDR_DOWNLOAD_BUTTON_CENTER_TOP,
      IDR_DOWNLOAD_BUTTON_RIGHT_TOP,
      IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE,
      IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE,
      IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE,
      IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM,
      IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM,
      IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM);

  body_nine_box_prelight_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_LEFT_TOP_H,
      IDR_DOWNLOAD_BUTTON_CENTER_TOP_H,
      IDR_DOWNLOAD_BUTTON_RIGHT_TOP_H,
      IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_H,
      IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_H,
      IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_H,
      IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_H,
      IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_H,
      IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_H);

  body_nine_box_active_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_LEFT_TOP_P,
      IDR_DOWNLOAD_BUTTON_CENTER_TOP_P,
      IDR_DOWNLOAD_BUTTON_RIGHT_TOP_P,
      IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_P,
      IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_P,
      IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_P,
      IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_P,
      IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_P,
      IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_P);

  menu_nine_box_normal_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_MENU_TOP, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_MIDDLE, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_BOTTOM, 0, 0);

  menu_nine_box_prelight_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_MENU_TOP_H, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_H, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_H, 0, 0);

  menu_nine_box_active_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_MENU_TOP_P, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_P, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_P, 0, 0);
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

  // When the button is showing, we want to draw it as active. We have to do
  // this explicitly because the button's state will be NORMAL while the menu
  // has focus.
  if (!is_body && download_item->menu_showing_)
    nine_box = menu_nine_box_active_;

  nine_box->RenderToWidget(widget);

  GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
  if (child)
    gtk_container_propagate_expose(GTK_CONTAINER(widget), child, e);

  return TRUE;
}

// static
gboolean DownloadItemGtk::OnProgressAreaExpose(GtkWidget* widget,
    GdkEventExpose* event, DownloadItemGtk* download_item) {
  // Create a transparent canvas.
  gfx::CanvasPaint canvas(event, false);
  if (download_item->complete_animation_.get()) {
    if (download_item->complete_animation_->IsAnimating()) {
      download_util::PaintDownloadComplete(&canvas,
          widget->allocation.x, widget->allocation.y,
          download_item->complete_animation_->GetCurrentValue(),
          download_util::SMALL);
    }
  } else {
    download_util::PaintDownloadProgress(&canvas,
        widget->allocation.x, widget->allocation.y,
        download_item->progress_angle_,
        download_item->download_model_->download()->PercentComplete(),
        download_util::SMALL);
  }

  // TODO(estade): draw a default icon if |icon_| is null.
  if (download_item->icon_) {
    canvas.DrawBitmapInt(*download_item->icon_,
        widget->allocation.x + download_util::kSmallProgressIconOffset,
        widget->allocation.y + download_util::kSmallProgressIconOffset);
  }

  return TRUE;
}

// static
gboolean DownloadItemGtk::OnMenuButtonPressEvent(GtkWidget* button,
                                                 GdkEvent* event,
                                                 DownloadItemGtk* item) {
  // Stop any completion animation.
  if (item->complete_animation_.get() &&
      item->complete_animation_->IsAnimating()) {
    item->complete_animation_->End();
  }

  item->complete_animation_->End();

  // TODO(port): this never puts the button into the "active" state,
  // so this may need to be changed. See note in BrowserToolbarGtk.
  if (event->type == GDK_BUTTON_PRESS) {
    GdkEventButton* event_button = reinterpret_cast<GdkEventButton*>(event);
    if (event_button->button == 1) {
      if (item->menu_.get() == NULL) {
        item->menu_.reset(new DownloadShelfContextMenuGtk(
                          item->download_model_.get(), item));
      }
      item->menu_->Popup(button, event);
      item->menu_showing_ = true;
      gtk_widget_queue_draw(button);
    }
  }

  return FALSE;
}

// static
void DownloadItemGtk::OnShelfResized(GtkWidget *widget,
                                     GtkAllocation *allocation,
                                     DownloadItemGtk* item) {
  if (item->hbox_->allocation.x + item->hbox_->allocation.width >
      item->bounding_widget_->allocation.x)
    gtk_widget_hide(item->hbox_);
  else
    gtk_widget_show(item->hbox_);
}

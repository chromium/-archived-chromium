// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/gtk_chrome_button.h"

#include "base/basictypes.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/browser/gtk/nine_box.h"

#include "grit/theme_resources.h"

namespace {

// The theme graphics for when the mouse is over the button.
scoped_ptr<NineBox> nine_box_prelight;
// The theme graphics for when the button is clicked.
scoped_ptr<NineBox> nine_box_active;

}

G_BEGIN_DECLS

G_DEFINE_TYPE (GtkChromeButton, gtk_chrome_button, GTK_TYPE_BUTTON)
static gboolean gtk_chrome_button_expose (GtkWidget      *widget,
                                          GdkEventExpose *event);

static void gtk_chrome_button_class_init(GtkChromeButtonClass *button_class) {
  GtkWidgetClass* widget_class = (GtkWidgetClass*)button_class;
  widget_class->expose_event = gtk_chrome_button_expose;

  GdkPixbuf* images[9];
  int i = 0;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_TOP_LEFT_H);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_TOP_H);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_TOP_RIGHT_H);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_LEFT_H);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_CENTER_H);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_RIGHT_H);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_BOTTOM_LEFT_H);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_BOTTOM_H);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_BOTTOM_RIGHT_H);
  nine_box_prelight.reset(new NineBox(images));

  i = 0;
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_TOP_LEFT_P);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_TOP_P);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_TOP_RIGHT_P);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_LEFT_P);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_CENTER_P);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_RIGHT_P);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_BOTTOM_LEFT_P);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_BOTTOM_P);
  images[i++] = rb.GetPixbufNamed(IDR_TEXTBUTTON_BOTTOM_RIGHT_P);
  nine_box_active.reset(new NineBox(images));
}

static void gtk_chrome_button_init(GtkChromeButton* button) {
  gtk_widget_set_app_paintable(GTK_WIDGET(button), TRUE);
}

static gboolean gtk_chrome_button_expose (GtkWidget      *widget,
                                          GdkEventExpose *event) {
  NineBox* nine_box = NULL;
  if (GTK_WIDGET_STATE(widget) == GTK_STATE_PRELIGHT)
    nine_box = nine_box_prelight.get();
  else if (GTK_WIDGET_STATE(widget) == GTK_STATE_ACTIVE)
    nine_box = nine_box_active.get();

  // Only draw theme graphics if we have some.
  if (nine_box)
    nine_box->RenderToWidget(widget);

  gtk_container_propagate_expose(GTK_CONTAINER(widget),
                                 gtk_bin_get_child(GTK_BIN(widget)),
                                 event);

  return FALSE;
}

GtkWidget* gtk_chrome_button_new(void) {
  return GTK_WIDGET(g_object_new(GTK_TYPE_CHROME_BUTTON, NULL));
}

G_END_DECLS


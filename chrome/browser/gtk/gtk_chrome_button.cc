// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/gtk_chrome_button.h"

#include "base/basictypes.h"
#include "base/gfx/gtk_util.h"
#include "chrome/browser/gtk/nine_box.h"

#include "grit/app_resources.h"

namespace {

// The theme graphics for when the mouse is over the button.
NineBox* g_nine_box_prelight;
// The theme graphics for when the button is clicked.
NineBox* g_nine_box_active;

}  // namespace

G_BEGIN_DECLS

#define GTK_CHROME_BUTTON_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o),\
                                          GTK_TYPE_CHROME_BUTTON,\
                                          GtkChromeButtonPrivate))
typedef struct _GtkChromeButtonPrivate GtkChromeButtonPrivate;

struct _GtkChromeButtonPrivate {
  int paint_state;

  // If true, we use images provided by the theme instead of GTK's default
  // button rendering.
  gboolean use_gtk_rendering;
};

G_DEFINE_TYPE(GtkChromeButton, gtk_chrome_button, GTK_TYPE_BUTTON)
static gboolean gtk_chrome_button_expose(GtkWidget* widget,
                                         GdkEventExpose* event);

static void gtk_chrome_button_class_init(GtkChromeButtonClass* button_class) {
  GObjectClass* gobject_class = G_OBJECT_CLASS(button_class);
  GtkWidgetClass* widget_class = reinterpret_cast<GtkWidgetClass*>(button_class);
  widget_class->expose_event = gtk_chrome_button_expose;

  g_nine_box_prelight = new NineBox(
      IDR_TEXTBUTTON_TOP_LEFT_H,
      IDR_TEXTBUTTON_TOP_H,
      IDR_TEXTBUTTON_TOP_RIGHT_H,
      IDR_TEXTBUTTON_LEFT_H,
      IDR_TEXTBUTTON_CENTER_H,
      IDR_TEXTBUTTON_RIGHT_H,
      IDR_TEXTBUTTON_BOTTOM_LEFT_H,
      IDR_TEXTBUTTON_BOTTOM_H,
      IDR_TEXTBUTTON_BOTTOM_RIGHT_H);

  g_nine_box_active = new NineBox(
      IDR_TEXTBUTTON_TOP_LEFT_P,
      IDR_TEXTBUTTON_TOP_P,
      IDR_TEXTBUTTON_TOP_RIGHT_P,
      IDR_TEXTBUTTON_LEFT_P,
      IDR_TEXTBUTTON_CENTER_P,
      IDR_TEXTBUTTON_RIGHT_P,
      IDR_TEXTBUTTON_BOTTOM_LEFT_P,
      IDR_TEXTBUTTON_BOTTOM_P,
      IDR_TEXTBUTTON_BOTTOM_RIGHT_P);

  g_type_class_add_private(gobject_class, sizeof(GtkChromeButtonPrivate));
}

static void gtk_chrome_button_init(GtkChromeButton* button) {
  GtkChromeButtonPrivate* priv = GTK_CHROME_BUTTON_GET_PRIVATE(button);
  priv->paint_state = -1;
  priv->use_gtk_rendering = FALSE;

  gtk_widget_set_app_paintable(GTK_WIDGET(button), TRUE);

  GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
}

static gboolean gtk_chrome_button_expose(GtkWidget* widget,
                                         GdkEventExpose* event) {
  GtkChromeButtonPrivate *priv = GTK_CHROME_BUTTON_GET_PRIVATE(widget);
  int paint_state = priv->paint_state < 0 ?
                    GTK_WIDGET_STATE(widget) : priv->paint_state;

  if (priv->use_gtk_rendering) {
    // We have the superclass handle this expose when we aren't using custom
    // rendering AND we're in either the prelight or active state so that we
    // get the button border for the current GTK theme drawn.
    if (paint_state == GTK_STATE_PRELIGHT || paint_state == GTK_STATE_ACTIVE) {
      (*GTK_WIDGET_CLASS(gtk_chrome_button_parent_class)->expose_event)
          (widget, event);
    } else {
      // Otherwise, we're still responsible for rendering our children.
      gtk_container_propagate_expose(GTK_CONTAINER(widget),
                                     gtk_bin_get_child(GTK_BIN(widget)),
                                     event);
    }
  } else {
    NineBox* nine_box = NULL;
    if (paint_state == GTK_STATE_PRELIGHT)
      nine_box = g_nine_box_prelight;
    else if (paint_state == GTK_STATE_ACTIVE)
      nine_box = g_nine_box_active;

    // Only draw theme graphics if we have some.
    if (nine_box)
      nine_box->RenderToWidget(widget);

    gtk_container_propagate_expose(GTK_CONTAINER(widget),
                                   gtk_bin_get_child(GTK_BIN(widget)),
                                   event);
  }

  return TRUE;  // Don't propagate, we are the default handler.
}

GtkWidget* gtk_chrome_button_new(void) {
  return GTK_WIDGET(g_object_new(GTK_TYPE_CHROME_BUTTON, NULL));
}

void gtk_chrome_button_set_paint_state(GtkChromeButton* button,
                                       GtkStateType state) {
  g_return_if_fail(GTK_IS_CHROME_BUTTON(button));

  GtkChromeButtonPrivate *priv = GTK_CHROME_BUTTON_GET_PRIVATE(button);
  priv->paint_state = state;

  gtk_widget_queue_draw(GTK_WIDGET(button));
}

void gtk_chrome_button_unset_paint_state(GtkChromeButton* button) {
  g_return_if_fail(GTK_IS_CHROME_BUTTON(button));

  GtkChromeButtonPrivate *priv = GTK_CHROME_BUTTON_GET_PRIVATE(button);
  priv->paint_state = -1;

  gtk_widget_queue_draw(GTK_WIDGET(button));
}

void gtk_chrome_button_set_use_gtk_rendering(GtkChromeButton* button,
                                             gboolean value) {
  g_return_if_fail(GTK_IS_CHROME_BUTTON(button));
  GtkChromeButtonPrivate *priv = GTK_CHROME_BUTTON_GET_PRIVATE(button);
  priv->use_gtk_rendering = value;
}

G_END_DECLS

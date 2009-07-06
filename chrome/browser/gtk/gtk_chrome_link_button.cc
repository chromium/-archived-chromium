// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/gtk_chrome_link_button.h"

#include <stdlib.h>

#include "base/logging.h"

static const gchar* kLinkMarkup = "<u><span color=\"%s\">%s</span></u>";

namespace {

// Set the GTK style on our custom link button. We don't want any border around
// the link text.
void SetLinkButtonStyle() {
  static bool style_was_set = false;

  if (style_was_set)
    return;
  style_was_set = true;

  gtk_rc_parse_string(
      "style \"chrome-link-button\" {"
      "  GtkButton::inner-border = {0, 0, 0, 0}"
      "  GtkButton::child-displacement-x = 0"
      "  GtkButton::child-displacement-y = 0"
      "  xthickness = 0"
      "  ythickness = 0"
      "}"
      "widget \"*chrome-link-button\" style \"chrome-link-button\"");
}

}  // namespace

G_BEGIN_DECLS
G_DEFINE_TYPE(GtkChromeLinkButton, gtk_chrome_link_button, GTK_TYPE_BUTTON)

static gboolean gtk_chrome_link_button_expose(GtkWidget* widget,
                                              GdkEventExpose* event) {
  GtkChromeLinkButton* button = GTK_CHROME_LINK_BUTTON(widget);
  GtkWidget* label = button->label;

  if (GTK_WIDGET_STATE(widget) == GTK_STATE_ACTIVE && button->is_blue) {
    gtk_label_set_markup(GTK_LABEL(label), button->red_markup);
    button->is_blue = FALSE;
  } else if (GTK_WIDGET_STATE(widget) != GTK_STATE_ACTIVE && !button->is_blue) {
    gtk_label_set_markup(GTK_LABEL(label), button->blue_markup);
    button->is_blue = TRUE;
  }

  // Draw the link inside the button.
  gtk_container_propagate_expose(GTK_CONTAINER(widget), label, event);

  // Draw the focus rectangle.
  if (GTK_WIDGET_HAS_FOCUS(widget)) {
    gtk_paint_focus(widget->style, widget->window,
                    static_cast<GtkStateType>(GTK_WIDGET_STATE(widget)),
                    &event->area, widget, NULL,
                    widget->allocation.x, widget->allocation.y,
                    widget->allocation.width, widget->allocation.height);
  }

  return TRUE;
}

static gboolean gtk_chrome_link_button_button_press(GtkWidget* widget,
                                                    GdkEventButton* event) {
  GtkButton* button;

  if (event->type == GDK_BUTTON_PRESS) {
    button = GTK_BUTTON(widget);

    if (button->focus_on_click && !GTK_WIDGET_HAS_FOCUS (widget))
      gtk_widget_grab_focus(widget);

    if (event->button == 1 || event->button == 2)
      gtk_button_pressed(button);
  }

  return TRUE;
}

static gboolean gtk_chrome_link_button_button_release(GtkWidget* widget,
                                                      GdkEventButton* event) {
  GtkButton* button = GTK_BUTTON(widget);
  GtkChromeLinkButton* link_button = GTK_CHROME_LINK_BUTTON(widget);

  free(link_button->click_button_event);
  link_button->click_button_event = static_cast<GdkEventButton*>(
      malloc(sizeof(GdkEventButton)));
  *link_button->click_button_event = *event;

  if (event->button == 1 || event->button == 2)
    gtk_button_released(button);

  return TRUE;
}

static void gtk_chrome_link_button_enter(GtkButton* button) {
  GtkWidget* widget = GTK_WIDGET(button);
  GtkChromeLinkButton* link_button = GTK_CHROME_LINK_BUTTON(button);
  gdk_window_set_cursor(widget->window, link_button->hand_cursor);
}

static void gtk_chrome_link_button_leave(GtkButton* button) {
  GtkWidget* widget = GTK_WIDGET(button);
  GtkChromeLinkButton* link_button = GTK_CHROME_LINK_BUTTON(button);
  gdk_window_set_cursor(widget->window, NULL);
  free(link_button->click_button_event);
  link_button->click_button_event = NULL;
}

static void gtk_chrome_link_button_destroy(GtkObject* object) {
  GtkChromeLinkButton* button = GTK_CHROME_LINK_BUTTON(object);
  if (button->blue_markup) {
    g_free(button->blue_markup);
    button->blue_markup = NULL;
  }
  if (button->red_markup) {
    g_free(button->red_markup);
    button->red_markup = NULL;
  }
  if (button->hand_cursor) {
    gdk_cursor_unref(button->hand_cursor);
    button->hand_cursor = NULL;
  }
  free(button->click_button_event);
  button->click_button_event = NULL;

  GTK_OBJECT_CLASS(gtk_chrome_link_button_parent_class)->destroy(object);
}

static void gtk_chrome_link_button_class_init(
    GtkChromeLinkButtonClass* link_button_class) {
  GtkWidgetClass* widget_class =
      reinterpret_cast<GtkWidgetClass*>(link_button_class);
  GtkButtonClass* button_class =
      reinterpret_cast<GtkButtonClass*>(link_button_class);
  GtkObjectClass* object_class =
      reinterpret_cast<GtkObjectClass*>(link_button_class);
  widget_class->expose_event = &gtk_chrome_link_button_expose;
  widget_class->button_press_event = &gtk_chrome_link_button_button_press;
  widget_class->button_release_event = &gtk_chrome_link_button_button_release;
  button_class->enter = &gtk_chrome_link_button_enter;
  button_class->leave = &gtk_chrome_link_button_leave;
  object_class->destroy = &gtk_chrome_link_button_destroy;
}

static void gtk_chrome_link_button_init(GtkChromeLinkButton* button) {
  SetLinkButtonStyle();

  // We put a label in a button so we can connect to the click event. We don't
  // let the button draw itself; catch all expose events to the button and pass
  // them through to the label.
  button->label = gtk_label_new(NULL);
  button->blue_markup = NULL;
  button->red_markup = NULL;
  button->is_blue = TRUE;
  button->hand_cursor = gdk_cursor_new(GDK_HAND2);
  button->click_button_event = NULL;

  gtk_container_add(GTK_CONTAINER(button), button->label);
  gtk_widget_set_name(GTK_WIDGET(button), "chrome-link-button");
  gtk_widget_set_app_paintable(GTK_WIDGET(button), TRUE);
}

static void gtk_chrome_link_button_set_text(GtkChromeLinkButton* button,
                                            const char* text,
                                            bool contains_markup) {
  // We should have only been called once or we'd leak the markups.
  DCHECK(!button->blue_markup && !button->red_markup);

  if (!contains_markup) {
    button->blue_markup = g_markup_printf_escaped(kLinkMarkup, "blue", text);
    button->red_markup = g_markup_printf_escaped(kLinkMarkup, "red", text);
  } else {
    button->blue_markup = static_cast<gchar*>(
        g_malloc(strlen(kLinkMarkup) + strlen("blue") + strlen(text) + 1));
    sprintf(button->blue_markup, kLinkMarkup, "blue", text);

    button->red_markup = static_cast<gchar*>(
        g_malloc(strlen(kLinkMarkup) + strlen("red") + strlen(text) + 1));
    sprintf(button->red_markup, kLinkMarkup, "red", text);
  }

  gtk_label_set_markup(GTK_LABEL(button->label), button->blue_markup);
  button->is_blue = TRUE;
}

GtkWidget* gtk_chrome_link_button_new(const char* text) {
  GtkWidget* lb = GTK_WIDGET(g_object_new(GTK_TYPE_CHROME_LINK_BUTTON, NULL));
  gtk_chrome_link_button_set_text(GTK_CHROME_LINK_BUTTON(lb), text, false);
  return lb;
}

GtkWidget* gtk_chrome_link_button_new_with_markup(const char* markup) {
  GtkWidget* lb = GTK_WIDGET(g_object_new(GTK_TYPE_CHROME_LINK_BUTTON, NULL));
  gtk_chrome_link_button_set_text(GTK_CHROME_LINK_BUTTON(lb), markup, true);
  return lb;
}

const GdkEventButton* gtk_chrome_link_button_get_event_for_click(
    GtkChromeLinkButton* button) {
  return button->click_button_event;
}

G_END_DECLS

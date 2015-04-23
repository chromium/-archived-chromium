#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "webkit/tools/test_shell/test_shell_x11.h"

namespace test_shell_x11 {

Display* GtkWidgetGetDisplay(GtkWidget* widget) {
  GdkDisplay* gdk_display = gtk_widget_get_display(widget);
  return gdk_x11_display_get_xdisplay(gdk_display);
}

int GtkWidgetGetScreenNum(GtkWidget* widget) {
  GdkDisplay* gdk_display = gtk_widget_get_display(widget);
  GdkScreen* gdk_screen = gdk_display_get_default_screen(gdk_display);
  return gdk_x11_screen_get_screen_number(gdk_screen);
}

}  // namespace test_shell_x11

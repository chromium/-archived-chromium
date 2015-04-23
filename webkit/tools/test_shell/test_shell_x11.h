#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_X11_H
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_X11_H

// This header is required because one cannot include both Xlib headers and
// WebKit headers in the same source file.

typedef struct _XDisplay Display;

namespace test_shell_x11 {

// Return an Xlib Display pointer for the given widget
Display* GtkWidgetGetDisplay(GtkWidget* widget);
// Return the screen number for the given widget
int GtkWidgetGetScreenNum(GtkWidget* widget);

}  // namespace test_shell_x11

#endif  // !WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_X11_H

// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was forked off the Mac port.

#include "webkit/tools/test_shell/test_webview_delegate.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "base/gfx/gtk_util.h"
#include "base/gfx/point.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "chrome/common/page_transition_types.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview.h"
#include "webkit/glue/plugins/gtk_plugin_container.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell.h"

using WebKit::WebRect;

namespace {

enum SelectionClipboardType {
  TEXT_HTML,
  PLAIN_TEXT,
};

GdkAtom GetTextHtmlAtom() {
  GdkAtom kTextHtmlGdkAtom = gdk_atom_intern_static_string("text/html");
  return kTextHtmlGdkAtom;
}

void SelectionClipboardGetContents(GtkClipboard* clipboard,
    GtkSelectionData* selection_data, guint info, gpointer data) {
  // Ignore formats that we don't know about.
  if (info != TEXT_HTML && info != PLAIN_TEXT)
    return;

  WebView* webview = static_cast<WebView*>(data);
  WebFrame* frame = webview->GetFocusedFrame();
  if (!frame)
    frame = webview->GetMainFrame();
  DCHECK(frame);

  std::string selection = frame->GetSelection(TEXT_HTML == info);
  if (TEXT_HTML == info) {
    gtk_selection_data_set(selection_data,
                           GetTextHtmlAtom(),
                           8 /* bits per data unit, ie, char */,
                           reinterpret_cast<const guchar*>(selection.data()),
                           selection.length());
  } else {
    gtk_selection_data_set_text(selection_data, selection.data(),
        selection.length());
  }
}

}  // namespace

// WebViewDelegate -----------------------------------------------------------

TestWebViewDelegate::~TestWebViewDelegate() {
}

WebPluginDelegate* TestWebViewDelegate::CreatePluginDelegate(
    WebView* webview,
    const GURL& url,
    const std::string& mime_type,
    const std::string& clsid,
    std::string* actual_mime_type) {
  bool allow_wildcard = true;
  WebPluginInfo info;
  if (!NPAPI::PluginList::Singleton()->GetPluginInfo(url, mime_type, clsid,
                                                     allow_wildcard, &info,
                                                     actual_mime_type))
    return NULL;

  const std::string& mtype =
      (actual_mime_type && !actual_mime_type->empty()) ? *actual_mime_type
                                                       : mime_type;
  // TODO(evanm): we probably shouldn't be doing this mapping to X ids at
  // this level.
  GdkNativeWindow plugin_parent =
      GDK_WINDOW_XWINDOW(shell_->webViewHost()->view_handle()->window);

  return WebPluginDelegateImpl::Create(info.path, mtype, plugin_parent);
}

gfx::PluginWindowHandle TestWebViewDelegate::CreatePluginContainer() {
  return shell_->webViewHost()->CreatePluginContainer();
}

void TestWebViewDelegate::WillDestroyPluginWindow(unsigned long id) {
  shell_->webViewHost()->OnPluginWindowDestroyed(id);
}

void TestWebViewDelegate::ShowJavaScriptAlert(const std::wstring& message) {
  GtkWidget* dialog = gtk_message_dialog_new(
      shell_->mainWnd(), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
      GTK_BUTTONS_OK, "%s", WideToUTF8(message).c_str());
  gtk_window_set_title(GTK_WINDOW(dialog), "JavaScript Alert");
  gtk_dialog_run(GTK_DIALOG(dialog));  // Runs a nested message loop.
  gtk_widget_destroy(dialog);
}

void TestWebViewDelegate::Show(WebWidget* webwidget,
                               WindowOpenDisposition disposition) {
  WebWidgetHost* host = GetHostForWidget(webwidget);
  GtkWidget* drawing_area = host->view_handle();
  GtkWidget* window =
      gtk_widget_get_parent(gtk_widget_get_parent(drawing_area));
  gtk_widget_show_all(window);
}

void TestWebViewDelegate::ShowAsPopupWithItems(
    WebWidget* webwidget,
    const WebRect& bounds,
    int item_height,
    int selected_index,
    const std::vector<WebMenuItem>& items) {
  NOTREACHED();
}

void TestWebViewDelegate::CloseWidgetSoon(WebWidget* webwidget) {
  if (webwidget == shell_->webView()) {
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableFunction(
        &gtk_widget_destroy, GTK_WIDGET(shell_->mainWnd())));
  } else if (webwidget == shell_->popup()) {
    shell_->ClosePopup();
  }
}

void TestWebViewDelegate::SetCursor(WebWidget* webwidget,
                                    const WebCursor& cursor) {
  current_cursor_ = cursor;
  GdkCursorType cursor_type = current_cursor_.GetCursorType();
  GdkCursor* gdk_cursor;
  if (cursor_type == GDK_CURSOR_IS_PIXMAP) {
    // TODO(port): WebKit bug https://bugs.webkit.org/show_bug.cgi?id=16388 is
    // that calling gdk_window_set_cursor repeatedly is expensive.  We should
    // avoid it here where possible.
    gdk_cursor = current_cursor_.GetCustomCursor();
  } else {
    // Optimize the common case, where the cursor hasn't changed.
    // However, we can switch between different pixmaps, so only on the
    // non-pixmap branch.
    if (cursor_type_ == cursor_type)
      return;
    if (cursor_type_ == GDK_LAST_CURSOR)
      gdk_cursor = NULL;
    else
      gdk_cursor = gdk_cursor_new(cursor_type);
  }
  cursor_type_ = cursor_type;
  gdk_window_set_cursor(shell_->webViewWnd()->window, gdk_cursor);
  // The window now owns the cursor.
  if (gdk_cursor)
    gdk_cursor_unref(gdk_cursor);
}

void TestWebViewDelegate::GetWindowRect(WebWidget* webwidget,
                                        WebRect* out_rect) {
  DCHECK(out_rect);
  WebWidgetHost* host = GetHostForWidget(webwidget);
  GtkWidget* drawing_area = host->view_handle();
  GtkWidget* vbox = gtk_widget_get_parent(drawing_area);
  GtkWidget* window = gtk_widget_get_parent(vbox);

  gint x, y;
  gtk_window_get_position(GTK_WINDOW(window), &x, &y);
  x += vbox->allocation.x + drawing_area->allocation.x;
  y += vbox->allocation.y + drawing_area->allocation.y;

  *out_rect = WebRect(x, y,
                      drawing_area->allocation.width,
                      drawing_area->allocation.height);
}

void TestWebViewDelegate::SetWindowRect(WebWidget* webwidget,
                                        const WebRect& rect) {
  if (webwidget == shell_->webView()) {
    // ignored
  } else if (webwidget == shell_->popup()) {
    WebWidgetHost* host = GetHostForWidget(webwidget);
    GtkWidget* drawing_area = host->view_handle();
    GtkWidget* window =
        gtk_widget_get_parent(gtk_widget_get_parent(drawing_area));
    gtk_window_resize(GTK_WINDOW(window), rect.width, rect.height);
    gtk_window_move(GTK_WINDOW(window), rect.x, rect.y);
  }
}

void TestWebViewDelegate::GetRootWindowRect(WebWidget* webwidget,
                                            WebRect* out_rect) {
  if (WebWidgetHost* host = GetHostForWidget(webwidget)) {
    // We are being asked for the x/y and width/height of the entire browser
    // window.  This means the x/y is the distance from the corner of the
    // screen, and the width/height is the size of the entire browser window.
    // For example, this is used to implement window.screenX and window.screenY.
    GtkWidget* drawing_area = host->view_handle();
    GtkWidget* window =
        gtk_widget_get_parent(gtk_widget_get_parent(drawing_area));
    gint x, y, width, height;
    gtk_window_get_position(GTK_WINDOW(window), &x, &y);
    gtk_window_get_size(GTK_WINDOW(window), &width, &height);
    *out_rect = WebRect(x, y, width, height);
  }
}

void TestWebViewDelegate::GetRootWindowResizerRect(WebWidget* webwidget,
                                                   WebRect* out_rect) {
  // Not necessary on Linux.
  *out_rect = WebRect();
}

void TestWebViewDelegate::DidMove(WebWidget* webwidget,
                                  const WebPluginGeometry& move) {
  WebWidgetHost* host = GetHostForWidget(webwidget);

  // The "window" on WebPluginGeometry is actually the XEmbed parent
  // X window id.
  GtkWidget* widget = ((WebViewHost*)host)->MapIDToWidget(move.window);
  // If we don't know about this plugin (maybe we're shutting down the
  // window?), ignore the message.
  if (!widget)
    return;
  DCHECK(!GTK_WIDGET_NO_WINDOW(widget));
  DCHECK(GTK_WIDGET_REALIZED(widget));

  if (!move.visible) {
    gtk_widget_hide(widget);
    return;
  } else {
    gtk_widget_show(widget);
  }

  // Update the clipping region on the GdkWindow.
  GdkRectangle clip_rect = move.clip_rect.ToGdkRectangle();
  GdkRegion* clip_region = gdk_region_rectangle(&clip_rect);
  gfx::SubtractRectanglesFromRegion(clip_region, move.cutout_rects);
  gdk_window_shape_combine_region(widget->window, clip_region, 0, 0);
  gdk_region_destroy(clip_region);

  // Update the window position.  Resizing is handled by WebPluginDelegate.
  // TODO(deanm): Verify that we only need to move and not resize.
  // TODO(evanm): we should cache the last shape and position and skip all
  // of this business in the common case where nothing has changed.
  int current_x, current_y;

  // Until the above TODO is resolved, we can grab the last position
  // off of the GtkFixed with a bit of hackery.
  GValue value = {0};
  g_value_init(&value, G_TYPE_INT);
  gtk_container_child_get_property(GTK_CONTAINER(host->view_handle()), widget,
                                   "x", &value);
  current_x = g_value_get_int(&value);
  gtk_container_child_get_property(GTK_CONTAINER(host->view_handle()), widget,
                                   "y", &value);
  current_y = g_value_get_int(&value);
  g_value_unset(&value);

  if (move.window_rect.x() != current_x || move.window_rect.y() != current_y) {
    // Calling gtk_fixed_move unnecessarily is a no-no, as it causes the parent
    // window to repaint!
    gtk_fixed_move(GTK_FIXED(host->view_handle()),
                   widget,
                   move.window_rect.x(),
                   move.window_rect.y());
  }

  gtk_plugin_container_set_size(widget,
                                move.window_rect.width(),
                                move.window_rect.height());
}

void TestWebViewDelegate::RunModal(WebWidget* webwidget) {
  NOTIMPLEMENTED();
}

void TestWebViewDelegate::UpdateSelectionClipboard(bool is_empty_selection) {
  if (is_empty_selection)
    return;

  GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  // Put data on the X clipboard.  This doesn't actually grab the text from
  // the HTML, it just registers a callback for when someone tries to paste.
  GtkTargetList* target_list = gtk_target_list_new(NULL, 0);
  gtk_target_list_add(target_list, GetTextHtmlAtom(), 0, TEXT_HTML);
  gtk_target_list_add_text_targets(target_list, PLAIN_TEXT);

  gint num_targets = 0;
  GtkTargetEntry* targets = gtk_target_table_new_from_list(target_list,
                                                           &num_targets);
  gtk_clipboard_set_with_data(clipboard, targets, num_targets,
                              SelectionClipboardGetContents, NULL,
                              shell_->webView());
  gtk_target_list_unref(target_list);
  gtk_target_table_free(targets, num_targets);
}

// Private methods -----------------------------------------------------------

void TestWebViewDelegate::SetPageTitle(const std::wstring& title) {
  gtk_window_set_title(GTK_WINDOW(shell_->mainWnd()),
                       ("Test Shell - " + WideToUTF8(title)).c_str());
}

void TestWebViewDelegate::SetAddressBarURL(const GURL& url) {
  gtk_entry_set_text(GTK_ENTRY(shell_->editWnd()), url.spec().c_str());
}

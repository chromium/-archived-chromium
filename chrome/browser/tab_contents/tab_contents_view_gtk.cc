// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents_view_gtk.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "base/mime_util.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/gtk/blocked_popup_container_view_gtk.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/constrained_window_gtk.h"
#include "chrome/browser/gtk/gtk_dnd_util.h"
#include "chrome/browser/gtk/gtk_floating_container.h"
#include "chrome/browser/gtk/sad_tab_gtk.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_factory.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/render_view_context_menu_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "webkit/glue/webdropdata.h"

namespace {

// TODO(erg): I have no idea how to progromatically figure out how wide the
// vertical scrollbar is. Hack it with a hardcoded value for now.
const int kScrollbarWidthHack = 25;

// Called when the content view gtk widget is tabbed to, or after the call to
// gtk_widget_child_focus() in TakeFocus(). We return true
// and grab focus if we don't have it. The call to
// FocusThroughTabTraversal(bool) forwards the "move focus forward" effect to
// webkit.
gboolean OnFocus(GtkWidget* widget, GtkDirectionType focus,
                 TabContents* tab_contents) {
  // If we already have focus, let the next widget have a shot at it. We will
  // reach this situation after the call to gtk_widget_child_focus() in
  // TakeFocus().
  if (gtk_widget_is_focus(widget))
    return FALSE;

  gtk_widget_grab_focus(widget);
  bool reverse = focus == GTK_DIR_TAB_BACKWARD;
  tab_contents->FocusThroughTabTraversal(reverse);
  return TRUE;
}

// Called when the mouse leaves the widget. We notify our delegate.
gboolean OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event,
                       TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(tab_contents, false);
  return FALSE;
}

// Called when the mouse moves within the widget. We notify our delegate.
gboolean OnMouseMove(GtkWidget* widget, GdkEventMotion* event,
                     TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(tab_contents, true);
  return FALSE;
}

// See tab_contents_view_win.cc for discussion of mouse scroll zooming.
gboolean OnMouseScroll(GtkWidget* widget, GdkEventScroll* event,
                       TabContents* tab_contents) {
  if ((event->state & gtk_accelerator_get_default_mod_mask()) ==
      GDK_CONTROL_MASK) {
    if (event->direction == GDK_SCROLL_DOWN) {
      tab_contents->delegate()->ContentsZoomChange(false);
      return TRUE;
    } else if (event->direction == GDK_SCROLL_UP) {
      tab_contents->delegate()->ContentsZoomChange(true);
      return TRUE;
    }
  }

  return FALSE;
}

// Used with gtk_container_foreach to change the sizes of the children of
// |fixed_|.
void SetSizeRequest(GtkWidget* widget, gpointer userdata) {
  gfx::Size* size = static_cast<gfx::Size*>(userdata);
  if (widget->allocation.width != size->width() ||
      widget->allocation.height != size->height()) {
    gtk_widget_set_size_request(widget, size->width(), size->height());
  }
}

}  // namespace

// static
TabContentsView* TabContentsView::Create(TabContents* tab_contents) {
  return new TabContentsViewGtk(tab_contents);
}

TabContentsViewGtk::TabContentsViewGtk(TabContents* tab_contents)
    : TabContentsView(tab_contents),
      floating_(gtk_floating_container_new()),
      fixed_(gtk_fixed_new()),
      popup_view_(NULL) {
  g_signal_connect(fixed_, "size-allocate",
                   G_CALLBACK(OnSizeAllocate), this);
  g_signal_connect(floating_.get(), "set-floating-position",
                   G_CALLBACK(OnSetFloatingPosition), this);

  gtk_container_add(GTK_CONTAINER(floating_.get()), fixed_);
  gtk_widget_show(fixed_);
  gtk_widget_show(floating_.get());
  registrar_.Add(this, NotificationType::TAB_CONTENTS_CONNECTED,
                 Source<TabContents>(tab_contents));
}

TabContentsViewGtk::~TabContentsViewGtk() {
  floating_.Destroy();
}

void TabContentsViewGtk::AttachBlockedPopupView(
    BlockedPopupContainerViewGtk* popup_view) {
  DCHECK(popup_view_ == NULL);
  popup_view_ = popup_view;
  gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(floating_.get()),
                                      popup_view->widget());
}

void TabContentsViewGtk::RemoveBlockedPopupView(
    BlockedPopupContainerViewGtk* popup_view) {
  DCHECK(popup_view_ == popup_view);
  gtk_container_remove(GTK_CONTAINER(floating_.get()), popup_view->widget());
  popup_view_ = NULL;
}

void TabContentsViewGtk::AttachConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  DCHECK(find(constrained_windows_.begin(), constrained_windows_.end(),
              constrained_window) == constrained_windows_.end());

  constrained_windows_.push_back(constrained_window);
  gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(floating_.get()),
                                      constrained_window->widget());
}

void TabContentsViewGtk::RemoveConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  std::vector<ConstrainedWindowGtk*>::iterator item =
      find(constrained_windows_.begin(), constrained_windows_.end(),
           constrained_window);
  DCHECK(item != constrained_windows_.end());

  gtk_container_remove(GTK_CONTAINER(floating_.get()),
                       constrained_window->widget());
  constrained_windows_.erase(item);
}

void TabContentsViewGtk::CreateView() {
  // Windows uses this to do initialization, but we do all our initialization
  // in the constructor.
}

RenderWidgetHostView* TabContentsViewGtk::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->view()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->view();
  }

  RenderWidgetHostViewGtk* view =
      new RenderWidgetHostViewGtk(render_widget_host);
  view->InitAsChild();
  gfx::NativeView content_view = view->native_view();
  g_signal_connect(content_view, "focus",
                   G_CALLBACK(OnFocus), tab_contents());
  g_signal_connect(content_view, "leave-notify-event",
                   G_CALLBACK(OnLeaveNotify), tab_contents());
  g_signal_connect(content_view, "motion-notify-event",
                   G_CALLBACK(OnMouseMove), tab_contents());
  g_signal_connect(content_view, "scroll-event",
                   G_CALLBACK(OnMouseScroll), tab_contents());
  gtk_widget_add_events(content_view, GDK_LEAVE_NOTIFY_MASK |
                        GDK_POINTER_MOTION_MASK);
  g_signal_connect(content_view, "button-press-event",
                   G_CALLBACK(OnMouseDown), this);

  // DnD signals.
  g_signal_connect(content_view, "drag-end", G_CALLBACK(OnDragEnd), this);
  g_signal_connect(content_view, "drag-data-get", G_CALLBACK(OnDragDataGet),
                   this);

  InsertIntoContentArea(content_view);
  return view;
}

gfx::NativeView TabContentsViewGtk::GetNativeView() const {
  return floating_.get();
}

gfx::NativeView TabContentsViewGtk::GetContentNativeView() const {
  if (!tab_contents()->render_widget_host_view())
    return NULL;
  return tab_contents()->render_widget_host_view()->GetNativeView();
}


gfx::NativeWindow TabContentsViewGtk::GetTopLevelNativeWindow() const {
  GtkWidget* window = gtk_widget_get_ancestor(GetNativeView(), GTK_TYPE_WINDOW);
  return window ? GTK_WINDOW(window) : NULL;
}

void TabContentsViewGtk::GetContainerBounds(gfx::Rect* out) const {
  // This is used for positioning the download shelf arrow animation,
  // as well as sizing some other widgets in Windows.  In GTK the size is
  // managed for us, so it appears to be only used for the download shelf
  // animation.
  int x = 0;
  int y = 0;
  if (fixed_->window)
    gdk_window_get_origin(fixed_->window, &x, &y);
  out->SetRect(x + fixed_->allocation.x, y + fixed_->allocation.y,
               fixed_->allocation.width, fixed_->allocation.height);
}

void TabContentsViewGtk::OnContentsDestroy() {
  // TODO(estade): Windows uses this function cancel pending drag-n-drop drags.
  // We don't have drags yet, so do nothing for now.
}

void TabContentsViewGtk::SetPageTitle(const std::wstring& title) {
  // Set the window name to include the page title so it's easier to spot
  // when debugging (e.g. via xwininfo -tree).
  gfx::NativeView content_view = GetContentNativeView();
  if (content_view && content_view->window)
    gdk_window_set_title(content_view->window, WideToUTF8(title).c_str());
}

void TabContentsViewGtk::OnTabCrashed() {
  if (!sad_tab_.get()) {
    sad_tab_.reset(new SadTabGtk);
    InsertIntoContentArea(sad_tab_->widget());
    gtk_widget_show(sad_tab_->widget());
  }
}

void TabContentsViewGtk::SizeContents(const gfx::Size& size) {
  // This function is a hack and should go away. In any case we don't manually
  // control the size of the contents on linux, so do nothing.
}

void TabContentsViewGtk::Focus() {
  if (tab_contents()->showing_interstitial_page()) {
    tab_contents()->interstitial_page()->Focus();
  } else {
    GtkWidget* widget = GetContentNativeView();
    if (widget)
      gtk_widget_grab_focus(widget);
  }
}

void TabContentsViewGtk::SetInitialFocus() {
  if (tab_contents()->FocusLocationBarByDefault())
    tab_contents()->delegate()->SetFocusToLocationBar();
  else
    Focus();
}

void TabContentsViewGtk::StoreFocus() {
  focus_store_.Store(GetNativeView());
}

void TabContentsViewGtk::RestoreFocus() {
  if (focus_store_.widget())
    gtk_widget_grab_focus(focus_store_.widget());
  else
    SetInitialFocus();
}

void TabContentsViewGtk::UpdateDragCursor(bool is_drop_target) {
  NOTIMPLEMENTED();
}

void TabContentsViewGtk::GotFocus() {
  NOTIMPLEMENTED();
}

// This is called when we the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void TabContentsViewGtk::TakeFocus(bool reverse) {
  gtk_widget_child_focus(GTK_WIDGET(GetTopLevelNativeWindow()),
      reverse ? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD);
}

void TabContentsViewGtk::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  // This may be an accelerator. Try to pass it on to our browser window
  // to handle.
  GtkWindow* window = GetTopLevelNativeWindow();
  if (!window) {
    NOTREACHED();
    return;
  }

  // Filter out pseudo key events created by GtkIMContext signal handlers.
  // Since GtkIMContext signal handlers don't use GdkEventKey objects, its
  // |event.os_event| values are dummy values (or NULL.)
  // We should filter out these pseudo key events to prevent unexpected
  // behaviors caused by them.
  if (event.type == WebKit::WebInputEvent::Char)
    return;

  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(window);
  DCHECK(browser_window);
  browser_window->HandleAccelerator(event.os_event->keyval,
      static_cast<GdkModifierType>(event.os_event->state));
}

void TabContentsViewGtk::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::TAB_CONTENTS_CONNECTED: {
      // No need to remove the SadTabGtk's widget from the container since
      // the new RenderWidgetHostViewGtk instance already removed all the
      // vbox's children.
      sad_tab_.reset();
      break;
    }
    default:
      NOTREACHED() << "Got a notification we didn't register for.";
      break;
  }
}

void TabContentsViewGtk::ShowContextMenu(const ContextMenuParams& params) {
  context_menu_.reset(new RenderViewContextMenuGtk(tab_contents(), params,
                                                   last_mouse_down_.time));
  context_menu_->Init();
  context_menu_->Popup();
}

// Render view DnD -------------------------------------------------------------

void TabContentsViewGtk::DragEnded() {
  if (tab_contents()->render_view_host())
    tab_contents()->render_view_host()->DragSourceSystemDragEnded();
}

void TabContentsViewGtk::StartDragging(const WebDropData& drop_data) {
  DCHECK(GetContentNativeView());

  int targets_mask = 0;

  if (!drop_data.plain_text.empty())
    targets_mask |= GtkDndUtil::X_CHROME_TEXT_PLAIN;
  if (drop_data.url.is_valid())
    targets_mask |= GtkDndUtil::X_CHROME_TEXT_URI_LIST;
  if (!drop_data.text_html.empty())
    targets_mask |= GtkDndUtil::X_CHROME_TEXT_HTML;
  if (!drop_data.file_contents.empty())
    targets_mask |= GtkDndUtil::X_CHROME_WEBDROP_FILE_CONTENTS;

  if (targets_mask == 0) {
    NOTIMPLEMENTED();
    DragEnded();
    return;
  }

  drop_data_.reset(new WebDropData(drop_data));

  GtkTargetList* list = GtkDndUtil::GetTargetListFromCodeMask(targets_mask);
  if (targets_mask & GtkDndUtil::X_CHROME_WEBDROP_FILE_CONTENTS) {
    drag_file_mime_type_ = gdk_atom_intern(
        mime_util::GetDataMimeType(drop_data.file_contents).c_str(), FALSE);
    gtk_target_list_add(list, drag_file_mime_type_,
                        0, GtkDndUtil::X_CHROME_WEBDROP_FILE_CONTENTS);
  }

  // If we don't pass an event, GDK won't know what event time to start grabbing
  // mouse events. Technically it's the mouse motion event and not the mouse
  // down event that causes the drag, but there's no reliable way to know
  // *which* motion event initiated the drag, so this will have to do.
  gtk_drag_begin(GetContentNativeView(), list, GDK_ACTION_COPY,
                 last_mouse_down_.button,
                 reinterpret_cast<GdkEvent*>(&last_mouse_down_));
  // The drag adds a ref; let it own the list.
  gtk_target_list_unref(list);
}

// static
void TabContentsViewGtk::OnDragDataGet(
    GtkWidget* drag_widget,
    GdkDragContext* context, GtkSelectionData* selection_data,
    guint target_type, guint time, TabContentsViewGtk* view) {
  const int bits_per_byte = 8;
  // We must make this initialization here or gcc complains about jumping past
  // it in the switch statement.
  std::string utf8_text;

  switch (target_type) {
    case GtkDndUtil::X_CHROME_TEXT_PLAIN:
      utf8_text = UTF16ToUTF8(view->drop_data_->plain_text);
      gtk_selection_data_set_text(selection_data, utf8_text.c_str(),
                                  utf8_text.length());
      break;
    case GtkDndUtil::X_CHROME_TEXT_URI_LIST:
      gchar* uri_array[2];
      uri_array[0] = strdup(view->drop_data_->url.spec().c_str());
      uri_array[1] = NULL;
      gtk_selection_data_set_uris(selection_data, uri_array);
      free(uri_array[0]);
      break;
    case GtkDndUtil::X_CHROME_TEXT_HTML:
      // TODO(estade): change relative links to be absolute using
      // |html_base_url|.
      utf8_text = UTF16ToUTF8(view->drop_data_->text_html);
      gtk_selection_data_set(selection_data,
          GtkDndUtil::GetAtomForTarget(GtkDndUtil::X_CHROME_TEXT_HTML),
          bits_per_byte,
          reinterpret_cast<const guchar*>(utf8_text.c_str()),
          utf8_text.length());
      break;

    case GtkDndUtil::X_CHROME_WEBDROP_FILE_CONTENTS:
      gtk_selection_data_set(selection_data,
          view->drag_file_mime_type_, bits_per_byte,
          reinterpret_cast<const guchar*>(
              view->drop_data_->file_contents.data()),
          view->drop_data_->file_contents.length());
      break;

    default:
      NOTREACHED();
  }
}

// static
void TabContentsViewGtk::OnDragEnd(GtkWidget* widget,
     GdkDragContext* drag_context, TabContentsViewGtk* view) {
  view->DragEnded();
  view->drop_data_.reset();
}

// -----------------------------------------------------------------------------

void TabContentsViewGtk::InsertIntoContentArea(GtkWidget* widget) {
  gtk_fixed_put(GTK_FIXED(fixed_), widget, 0, 0);
}

gboolean TabContentsViewGtk::OnMouseDown(GtkWidget* widget,
    GdkEventButton* event, TabContentsViewGtk* view) {
  view->last_mouse_down_ = *event;
  return FALSE;
}

gboolean TabContentsViewGtk::OnSizeAllocate(GtkWidget* widget,
                                            GtkAllocation* allocation,
                                            TabContentsViewGtk* view) {
  int width = allocation->width;
  int height = allocation->height;
  // |delegate()| can be NULL here during browser teardown.
  if (view->tab_contents()->delegate())
    height += view->tab_contents()->delegate()->GetExtraRenderViewHeight();
  gfx::Size size(width, height);
  gtk_container_foreach(GTK_CONTAINER(widget), SetSizeRequest, &size);

  return FALSE;
}

// static
void TabContentsViewGtk::OnSetFloatingPosition(
    GtkFloatingContainer* floating_container, GtkAllocation* allocation,
    TabContentsViewGtk* tab_contents_view) {
  if (tab_contents_view->popup_view_) {
    GtkWidget* widget = tab_contents_view->popup_view_->widget();

    // Look at the size request of the status bubble and tell the
    // GtkFloatingContainer where we want it positioned.
    GtkRequisition requisition;
    gtk_widget_size_request(widget, &requisition);

    GValue value = { 0, };
    g_value_init(&value, G_TYPE_INT);

    int child_x = std::max(
        allocation->x + allocation->width - requisition.width -
        kScrollbarWidthHack, 0);
    g_value_set_int(&value, child_x);
    gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                     widget, "x", &value);

    int child_y = std::max(
        allocation->y + allocation->height - requisition.height, 0);
    g_value_set_int(&value, child_y);
    gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                     widget, "y", &value);
    g_value_unset(&value);
  }

  // Place each ConstrainedWindow in the center of the view.
  int half_view_width = std::max((allocation->x + allocation->width) / 2, 0);
  int half_view_height = std::max((allocation->y + allocation->height) / 2, 0);
  std::vector<ConstrainedWindowGtk*>::iterator it =
      tab_contents_view->constrained_windows_.begin();
  std::vector<ConstrainedWindowGtk*>::iterator end =
      tab_contents_view->constrained_windows_.end();
  for (; it != end; ++it) {
    GtkWidget* widget = (*it)->widget();
    DCHECK(widget->parent == tab_contents_view->floating_.get());

    GtkRequisition requisition;
    gtk_widget_size_request(widget, &requisition);

    GValue value = { 0, };
    g_value_init(&value, G_TYPE_INT);

    int child_x = std::max(half_view_width - (requisition.width / 2), 0);
    g_value_set_int(&value, child_x);
    gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                     widget, "x", &value);

    int child_y = std::max(half_view_height - (requisition.height / 2), 0);
    g_value_set_int(&value, child_y);
    gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                     widget, "y", &value);
    g_value_unset(&value);
  }
}

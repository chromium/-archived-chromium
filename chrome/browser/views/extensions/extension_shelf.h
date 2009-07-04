// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_SHELF_H_
#define CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_SHELF_H_

#include "app/gfx/canvas.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_shelf_model.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/views/browser_bubble.h"
#include "views/view.h"

class Browser;
class ExtensionShelfHandle;
namespace views {
  class Label;
  class MouseEvent;
}

// A shelf that contains Extension toolstrips.
class ExtensionShelf : public views::View,
                       public ExtensionContainer,
                       public BrowserBubble::Delegate,
                       public ExtensionShelfModelObserver {
 public:
  explicit ExtensionShelf(Browser* browser);
  virtual ~ExtensionShelf();

  // Get the current model.
  ExtensionShelfModel* model() { return model_.get(); }

  // Return the current active ExtensionShelfHandle (if any).
  BrowserBubble* GetHandle();

  // View
  virtual void Paint(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual void OnMouseEntered(const views::MouseEvent& event);

  // ExtensionContainer
  virtual void OnExtensionMouseEvent(ExtensionView* view);
  virtual void OnExtensionMouseLeave(ExtensionView* view);

  // BrowserBubble::Delegate
  virtual void BubbleBrowserWindowMoved(BrowserBubble* bubble);
  virtual void BubbleBrowserWindowClosed(BrowserBubble* bubble);

  // ExtensionShelfModelObserver
  virtual void ToolstripInsertedAt(ExtensionHost* toolstrip, int index);
  virtual void ToolstripRemovingAt(ExtensionHost* toolstrip, int index);
  virtual void ToolstripDraggingFrom(ExtensionHost* toolstrip, int index);
  virtual void ToolstripMoved(ExtensionHost* toolstrip,
                              int from_index,
                              int to_index);
  virtual void ToolstripChangedAt(ExtensionHost* toolstrip, int index);
  virtual void ExtensionShelfEmpty();
  virtual void ShelfModelReloaded();

  // Dragging toolstrips
  void DragExtension();
  void DropExtension(const gfx::Point& pt, bool cancel);
  void DragHandleTo(const gfx::Point& pt);

 protected:
  // View
  virtual void ChildPreferredSizeChanged(View* child);

 private:
  // Inits the background bitmap.
  void InitBackground(gfx::Canvas* canvas, const SkRect& subset);

  // Returns the toolstrip at |x| coordinate.  If |x| is < 0, returns
  // the first toolstrip.  If |x| > the last toolstrip position, the
  // last toolstrip is returned.
  ExtensionHost* ToolstripAtX(int x);

  // Returns the toolstrip associated with |view|.
  ExtensionHost* ToolstripForView(ExtensionView* view);

  // Show / Hide the shelf handle.
  void ShowShelfHandle();
  void DoShowShelfHandle();
  void HideShelfHandle(int delay_ms);
  void DoHideShelfHandle();

  // Adjust shelf handle size and position.
  void LayoutShelfHandle();

  // Loads initial state from |model_|.
  void LoadFromModel();

  // Background bitmap to draw under extension views.
  SkBitmap background_;

  // The current shelf handle.
  scoped_ptr<BrowserBubble> handle_;

  // Whether to handle is visible;
  bool handle_visible_;

  // Which child view the handle is currently over.
  ExtensionHost* current_toolstrip_;

  // Timers for tracking mouse hovering.
  ScopedRunnableMethodFactory<ExtensionShelf> timer_factory_;

  // A placeholder for a pending drag
  View* drag_placeholder_view_;

  // The model representing the toolstrips on the shelf.
  scoped_ptr<ExtensionShelfModel> model_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionShelf);
};

#endif  // CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_SHELF_H_

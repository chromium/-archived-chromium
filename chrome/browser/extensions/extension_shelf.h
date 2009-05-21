// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SHELF_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SHELF_H_

#include "app/gfx/canvas.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/views/browser_bubble.h"
#include "chrome/common/notification_registrar.h"
#include "views/view.h"

class Browser;
class ExtensionShelfHandle;
namespace views {
  class Label;
  class MouseEvent;
}

// A shelf that contains Extension toolstrips.
class ExtensionShelf : public views::View,
                       public NotificationObserver,
                       public ExtensionContainer,
                       public BrowserBubble::Delegate {
 public:
  explicit ExtensionShelf(Browser* browser);
  virtual ~ExtensionShelf();

  // Return the current active ExtensionShelfHandle (if any).
  BrowserBubble* GetHandle();

  // View
  virtual void Paint(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual void OnMouseEntered(const views::MouseEvent& event);

  // NotificationService
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  bool AddExtensionViews(const ExtensionList* extensions);
  bool HasExtensionViews();

  // ExtensionContainer
  virtual void OnExtensionMouseEvent(ExtensionView* view);
  virtual void OnExtensionMouseLeave(ExtensionView* view);

  // BrowserBubble::Delegate
  virtual void BubbleBrowserWindowMoved(BrowserBubble* bubble);
  virtual void BubbleBrowserWindowClosed(BrowserBubble* bubble);

 protected:
  // View
  virtual void ChildPreferredSizeChanged(View* child);

 private:
  // Inits the background bitmap.
  void InitBackground(gfx::Canvas* canvas, const SkRect& subset);

  // Show / Hide the shelf handle.
  void ShowShelfHandle();
  void DoShowShelfHandle();
  void HideShelfHandle(int delay_ms);
  void DoHideShelfHandle();

  // Adjust shelf handle size and position.
  void LayoutShelfHandle();

  NotificationRegistrar registrar_;

  // Which browser window this shelf is in.
  Browser* browser_;

  // Background bitmap to draw under extension views.
  SkBitmap background_;

  // The current shelf handle.
  scoped_ptr<BrowserBubble> handle_;

  // Whether to handle is visible;
  bool handle_visible_;

  // Which child view the handle is currently over.
  ExtensionView* current_handle_view_;

  // Timers for tracking mouse hovering.
  ScopedRunnableMethodFactory<ExtensionShelf> timer_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionShelf);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SHELF_H_

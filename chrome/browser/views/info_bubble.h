// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFO_BUBBLE_H_
#define CHROME_BROWSER_VIEWS_INFO_BUBBLE_H_

#include "chrome/common/slide_animation.h"
#include "chrome/views/view.h"
#include "chrome/views/widget_win.h"

// InfoBubble is used to display an arbitrary view above all other windows.
// Think of InfoBubble as a tooltip that allows you to embed an arbitrary view
// in the tooltip. Additionally the InfoBubble renders an arrow pointing at
// the region the info bubble is providing the information about.
//
// To use an InfoBubble invoke Show and it'll take care of the rest. InfoBubble
// (or rather ContentView) insets the content view for you, so that the
// content typically shouldn't have any additional margins around the view.

class InfoBubble;
namespace views {
class Window;
}

class InfoBubbleDelegate {
 public:
  // Called when the InfoBubble is closing and is about to be deleted.
  // |closed_by_escape| is true if the close is the result of the user pressing
  // escape.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape) = 0;

  // Whether the InfoBubble should be closed when the Esc key is pressed.
  virtual bool CloseOnEscape() = 0;
};

class InfoBubble : public views::WidgetWin,
                   public AnimationDelegate {
 public:
  // Shows the InfoBubble. The InfoBubble is parented to parent_hwnd, contains
  // the View content and positioned relative to the screen position
  // position_relative_to. Show takes ownership of content and deletes the
  // created InfoBubble when another window is activated. You can explicitly
  // close the bubble by invoking Close.  A delegate may optionally be provided
  // to be notified when the InfoBubble is closed and to prevent the InfoBubble
  // from being closed when the Escape key is pressed (which is the default
   // behavior if there is no delegate).
  static InfoBubble* Show(HWND parent_hwnd,
                          const gfx::Rect& position_relative_to,
                          views::View* content,
                          InfoBubbleDelegate* delegate);

  InfoBubble();
  virtual ~InfoBubble();

  // Creates the InfoBubble.
  void Init(HWND parent_hwnd,
            const gfx::Rect& position_relative_to,
            views::View* content);

  // Sets the delegate for that InfoBubble.
  void SetDelegate(InfoBubbleDelegate* delegate) { delegate_ = delegate; }

  // The InfoBubble is automatically closed when it loses activation status.
  virtual void OnActivate(UINT action, BOOL minimized, HWND window);

  // Return our rounded window shape.
  virtual void OnSize(UINT param, const CSize& size);

  // Overridden to notify the owning ChromeFrame the bubble is closing.
  virtual void Close();

  // AcceleratorTarget method:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // AnimationDelegate Implementation
  virtual void AnimationProgressed(const Animation* animation);

 protected:

  // InfoBubble::CreateContentView() creates one of these. ContentView houses
  // the supplied content as its only child view, renders the arrow/border of
  // the bubble and sizes the content.
  class ContentView : public views::View {
   public:
    // Possible edges the arrow is aligned along.
    enum ArrowEdge {
      TOP_LEFT     = 0,
      TOP_RIGHT    = 1,
      BOTTOM_LEFT  = 2,
      BOTTOM_RIGHT = 3
    };

    // Creates the ContentView. The supplied view is added as the only child of
    // the ContentView.
    ContentView(views::View* content, InfoBubble* host);

    virtual ~ContentView() {}

    // Returns the bounds for the window to contain this view.
    //
    // This invokes the method of the same name that doesn't take an HWND, if
    // the returned bounds don't fit on the monitor containing parent_hwnd,
    // the arrow edge is adjusted.
    virtual gfx::Rect CalculateWindowBounds(
        HWND parent_hwnd,
        const gfx::Rect& position_relative_to);

    // Sets the edge the arrow is rendered at.
    void SetArrowEdge(ArrowEdge arrow_edge) { arrow_edge_ = arrow_edge; }

    // Returns the preferred size, which is the sum of the preferred size of
    // the content and the border/arrow.
    virtual gfx::Size GetPreferredSize();

    // Positions the content relative to the border.
    virtual void Layout();

    // Return the mask for the content view.
    HRGN GetMask(const CSize& size);

    // Paints the background and arrow appropriately.
    virtual void Paint(ChromeCanvas* canvas);

    // Returns true if the arrow is positioned along the top edge of the
    // view. If this returns false the arrow is positioned along the bottom
    // edge.
    bool IsTop() { return (arrow_edge_ & 2) == 0; }

    // Returns true if the arrow is positioned along the left edge of the
    // view. If this returns false the arrow is positioned along the right edge.
    bool IsLeft() { return (arrow_edge_ & 1) == 0; }

   private:

    // Returns the bounds for the window containing us based on the current
    // arrow edge.
    gfx::Rect CalculateWindowBounds(const gfx::Rect& position_relative_to);

    // Edge to draw the arrow at.
    ArrowEdge arrow_edge_;

    // The bubble we're in.
    InfoBubble* host_;

    DISALLOW_COPY_AND_ASSIGN(ContentView);
  };

  // Creates and return a new ContentView containing content.
  virtual ContentView* CreateContentView(views::View* content);

 private:
  // Closes the window notifying the delegate. |closed_by_escape| is true if
  // the close is the result of pressing escape.
  void Close(bool closed_by_escape);

  // The delegate notified when the InfoBubble is closed.
  InfoBubbleDelegate* delegate_;

  // The window that this InfoBubble is parented to.
  views::Window* parent_;

  // The content view contained by the infobubble.
  ContentView* content_view_;

  // The fade-in animation.
  scoped_ptr<SlideAnimation> fade_animation_;

  // Have we been closed?
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubble);
};

#endif  // CHROME_BROWSER_VIEWS_INFO_BUBBLE_H_

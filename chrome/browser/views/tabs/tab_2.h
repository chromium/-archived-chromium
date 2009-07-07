// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_2_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_2_H_

#include "base/string16.h"
#include "views/view.h"

class Tab2;

namespace gfx {
class Canvas;
class Path;
};

namespace views {
class AnimationContext;
class Animator;
class AnimatorDelegate;
}

// An interface implemented by an object that provides data to the Tab2.
// The Tab2 sometimes owns the Tab2Model. See |removing_model_| in Tab2.
class Tab2Model {
 public:
  virtual ~Tab2Model() {}

  // Tab2 presentation state.
  virtual string16 GetTitle(Tab2* tab) const = 0;
  virtual bool IsSelected(Tab2* tab) const = 0;

  // The Tab2 has been clicked and should become selected.
  virtual void SelectTab(Tab2* tab) = 0;

  // The mouse has been pressed down on the Tab2, pertinent information for any
  // drag that might occur should be captured at this time.
  virtual void CaptureDragInfo(Tab2* tab,
                               const views::MouseEvent& drag_event) = 0;

  // The mouse has been dragged after a press on the Tab2.
  virtual bool DragTab(Tab2* tab, const views::MouseEvent& drag_event) = 0;

  // The current drag operation has ended.
  virtual void DragEnded(Tab2* tab) = 0;

  // TODO(beng): get rid of this once animator is on View.
  virtual views::AnimatorDelegate* AsAnimatorDelegate() = 0;
};

// A view that represents a Tab in a TabStrip2.
class Tab2 : public views::View {
 public:
  explicit Tab2(Tab2Model* model);
  virtual ~Tab2();

  bool dragging() const { return dragging_; }

  bool removing() const { return removing_; }
  void set_removing(bool removing) { removing_ = removing; }

  // Assigns and takes ownership of a model object to be used when painting this
  // Tab2 after the underlying data object has been removed from TabStrip2's
  // model.
  void SetRemovingModel(Tab2Model* model);

  // Returns true if the Tab2 is being animated.
  bool IsAnimating() const;

  // Returns the Tab2's animator, creating one if necessary.
  // TODO(beng): consider moving to views::View.
  views::Animator* GetAnimator();

  // Returns the ideal size of the Tab2.
  static gfx::Size GetStandardSize();

  // Adds the shape of the tab to the specified path. Used to create a clipped
  // window during detached window dragging operations.
  void AddTabShapeToPath(gfx::Path* path) const;

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event,
                               bool canceled);
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);

 private:
  Tab2Model* model_;

  // True if the Tab2 is being dragged currently.
  bool dragging_;

  // True if the Tab2 represents an object removed from its containing
  // TabStrip2's model, and is currently being animated closed.
  bool removing_;

  // Our animator.
  scoped_ptr<views::Animator> animator_;

  // A dummy model to use for painting the tab after it's been removed from the
  // TabStrip2's model but while it's still visible in the presentation (being
  // animated out of existence).
  scoped_ptr<Tab2Model> removing_model_;

  DISALLOW_COPY_AND_ASSIGN(Tab2);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_TABS_TAB_2_H_

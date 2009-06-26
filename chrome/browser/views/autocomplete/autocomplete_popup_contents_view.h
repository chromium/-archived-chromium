// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_
#define CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_

#include "app/gfx/font.h"
#include "app/slide_animation.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view.h"
#if defined(OS_WIN)
#include "chrome/browser/views/autocomplete/autocomplete_popup_win.h"
#endif
#include "views/view.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompleteEditModel;
class AutocompleteEditViewWin;
class AutocompletePopupWin;
class Profile;

// An interface implemented by an object that provides data to populate
// individual result views.
class AutocompleteResultViewModel {
 public:
  // Returns true if the index is selected.
  virtual bool IsSelectedIndex(size_t index) const = 0;

  // Called when the line at the specified index should be opened with the
  // provided disposition.
  virtual void OpenIndex(size_t index, WindowOpenDisposition disposition) = 0;

  // Called when the line at the specified index should be shown as hovered.
  virtual void SetHoveredLine(size_t index) = 0;

  // Called when the line at the specified index should be shown as selected.
  virtual void SetSelectedLine(size_t index, bool revert_to_default) = 0;
};

// A view representing the contents of the autocomplete popup.
class AutocompletePopupContentsView : public views::View,
                                      public AutocompleteResultViewModel,
                                      public AutocompletePopupView,
                                      public AnimationDelegate {
 public:
  AutocompletePopupContentsView(const gfx::Font& font,
                                AutocompleteEditViewWin* edit_view,
                                AutocompleteEditModel* edit_model,
                                Profile* profile,
                                AutocompletePopupPositioner* popup_positioner);
  virtual ~AutocompletePopupContentsView() {}

  // Returns the bounds the popup should be shown at. This is the display bounds
  // and includes offsets for the dropshadow which this view's border renders.
  gfx::Rect GetPopupBounds() const;

  // Overridden from AutocompletePopupView:
  virtual bool IsOpen() const;
  virtual void InvalidateLine(size_t line);
  virtual void UpdatePopupAppearance();
  virtual void OnHoverEnabledOrDisabled(bool disabled);
  virtual void PaintUpdatesNow();
  virtual AutocompletePopupModel* GetModel();

  // Overridden from AutocompleteResultViewModel:
  virtual bool IsSelectedIndex(size_t index) const;
  virtual void OpenIndex(size_t index, WindowOpenDisposition disposition);
  virtual void SetHoveredLine(size_t index);
  virtual void SetSelectedLine(size_t index, bool revert_to_default);

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation);

  // Overridden from views::View:
  virtual void PaintChildren(gfx::Canvas* canvas);
  virtual void Layout();

 private:
  // Returns true if the model has a match at the specified index.
  bool HasMatchAt(size_t index) const;

  // Returns the match at the specified index within the popup model.
  const AutocompleteMatch& GetMatchAtIndex(size_t index) const;

  // Fill a path for the contents' roundrect. |bounding_rect| is the rect that
  // bounds the path.
  void MakeContentsPath(gfx::Path* path, const gfx::Rect& bounding_rect);

  // Updates the window's blur region for the current size.
  void UpdateBlurRegion();

  // Makes the contents of the canvas slightly transparent.
  void MakeCanvasTransparent(gfx::Canvas* canvas);

  // Starts sizing the popup to its new target size.
  void StartSizing();

  // Calculate the start and target bounds of the popup for an animation.
  void CalculateAnimationFrameBounds();

  // Gets the ideal bounds to display the number of result rows.
  gfx::Rect GetTargetBounds() const;

  // Returns true if the popup needs to grow larger to show |result_rows_|.
  bool IsGrowingLarger() const;

#if defined(OS_WIN)
  // The popup that contains this view.
  scoped_ptr<AutocompletePopupWin> popup_;
#endif

  // The provider of our result set.
  scoped_ptr<AutocompletePopupModel> model_;

  // The edit view that invokes us.
  AutocompleteEditViewWin* edit_view_;

  // An object that tells the popup how to position itself.
  AutocompletePopupPositioner* popup_positioner_;

  // The font that we should use for result rows. This is based on the font used
  // by the edit that created us.
  gfx::Font result_font_;

  // See discussion in UpdatePopupAppearance.
  ScopedRunnableMethodFactory<AutocompletePopupContentsView>
      size_initiator_factory_;

  // The popup sizes vertically using an animation when the popup is getting
  // shorter (not larger, that makes it look "slow").
  SlideAnimation size_animation_;
  gfx::Rect start_bounds_;
  gfx::Rect target_bounds_;

  // The number of rows required by the result set. This can differ from the
  // number of child views, as we retain rows after they are removed from the
  // model so we can animate the popup and still have the removed rows painted.
  size_t result_rows_;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupContentsView);
};


#endif  // #ifndef CHROME_BROWSER_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_CONTENTS_VIEW_H_

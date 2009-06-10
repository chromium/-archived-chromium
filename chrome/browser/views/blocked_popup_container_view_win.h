// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_VIEW_WIN_H_
#define CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_VIEW_WIN_H_

#include <set>
#include <utility>
#include <vector>

#include "app/slide_animation.h"
#include "base/gfx/native_widget_types.h"
#include "base/gfx/rect.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "views/controls/button/button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu.h"
#include "views/view.h"
#include "views/widget/widget_win.h"

class BlockedPopupContainerInternalView;
class PrefService;
class Profile;
class TabContents;
class TextButton;

namespace views {
class ImageButton;
}

// Takes ownership of TabContents that are unrequested popup windows and
// presents an interface to the user for launching them. (Or never showing them
// again).
class BlockedPopupContainerViewWin : public BlockedPopupContainerView,
                                     public AnimationDelegate,
                                     public views::WidgetWin {
 public:
  virtual ~BlockedPopupContainerViewWin();

  // Returns the URL and title for popup |index|, used to construct a string for
  // display.
  void GetURLAndTitleForPopup(size_t index,
                              std::wstring* url,
                              std::wstring* title) const;

  // Returns the names of hosts showing popups.
  std::vector<std::wstring> GetHosts() const;

  // Returns the number of blocked popups from the model
  size_t GetBlockedPopupCount() const;

  // Returns the model that owns us.
  BlockedPopupContainer* GetModel() const { return container_model_; }

  // Overridden from AnimationDelegate:
  virtual void AnimationStarted(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);
  virtual void AnimationProgressed(const Animation* animation);

  // Overridden from BlockedPopupContainerView:
  virtual void SetPosition();
  virtual void ShowView();
  virtual void UpdateLabel();
  virtual void HideView();
  virtual void Destroy();

 private:
  // For the static constructor BlockedPopupContainerView::Create().
  friend class BlockedPopupContainerView;

  // Creates a container for a certain TabContents.
  explicit BlockedPopupContainerViewWin(BlockedPopupContainer* container);

  // Overridden from views::WidgetWin:

  // Makes the top corners of the window rounded during resizing events.
  virtual void OnSize(UINT param, const CSize& size);

  // Our model; calling the shots.
  BlockedPopupContainer* container_model_;

  // Our associated view object.
  BlockedPopupContainerInternalView* container_view_;

  // The animation that slides us up and down.
  scoped_ptr<SlideAnimation> slide_animation_;

  DISALLOW_COPY_AND_ASSIGN(BlockedPopupContainerViewWin);
};

#endif  // CHROME_BROWSER_VIEWS_BLOCKED_POPUP_CONTAINER_VIEW_WIN_H_

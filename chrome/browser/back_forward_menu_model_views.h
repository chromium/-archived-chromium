// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACK_FORWARD_MENU_MODEL_VIEWS_H_
#define CHROME_BROWSER_BACK_FORWARD_MENU_MODEL_VIEWS_H_

#include "base/basictypes.h"

#include "chrome/browser/back_forward_menu_model.h"
#include "views/controls/menu/menu_2.h"

class SkBitmap;

namespace views {
class Widget;
}

class BackForwardMenuModelViews : public BackForwardMenuModel,
                                  public views::Menu2Model {
 public:
  // Construct a BackForwardMenuModel. |frame| is used to locate the accelerator
  // for the history item.
  BackForwardMenuModelViews(Browser* browser,
                            ModelType model_type,
                            views::Widget* frame);

  // Overridden from views::Menu2Model:
  virtual bool HasIcons() const;
  virtual int GetItemCount() const;
  virtual ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const;
  virtual string16 GetLabelAt(int index) const;
  virtual bool IsLabelDynamicAt(int index) const;
  virtual bool GetAcceleratorAt(int index,
                                views::Accelerator* accelerator) const;
  virtual bool IsItemCheckedAt(int index) const;
  virtual int GetGroupIdAt(int index) const;
  virtual bool GetIconAt(int index, SkBitmap* icon) const;
  virtual bool IsEnabledAt(int index) const;
  virtual Menu2Model* GetSubmenuModelAt(int index) const;
  virtual void HighlightChangedTo(int index);
  virtual void ActivatedAt(int index);
  virtual void MenuWillShow();

 private:
  // The frame we ask about accelerator info.
  views::Widget* frame_;

  DISALLOW_COPY_AND_ASSIGN(BackForwardMenuModelViews);
};

#endif  // CHROME_BROWSER_BACK_FORWARD_MENU_MODEL_VIEWS_H_

// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/layout_manager.h"

#include "views/view.h"

namespace views {

int LayoutManager::GetPreferredHeightForWidth(View* host, int width) {
  return GetPreferredSize(host).height();
}

}  // namespace views

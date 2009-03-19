// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/views/fill_layout.h"

#include "base/logging.h"

namespace views {

///////////////////////////////////////////////////////////////////////////////
// FillLayout

FillLayout::FillLayout() {
}

FillLayout::~FillLayout() {
}

void FillLayout::Layout(View* host) {
  if (host->GetChildViewCount() == 0)
    return;

  View* frame_view = host->GetChildViewAt(0);
  frame_view->SetBounds(0, 0, host->width(), host->height());
}

gfx::Size FillLayout::GetPreferredSize(View* host) {
  DCHECK(host->GetChildViewCount() == 1);
  return host->GetChildViewAt(0)->GetPreferredSize();
}

}  // namespace views

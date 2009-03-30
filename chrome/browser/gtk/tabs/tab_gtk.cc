// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/tabs/tab_gtk.h"

#include "chrome/common/gfx/path.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/resource_bundle.h"
#include "grit/generated_resources.h"

static const SkScalar kTabCapWidth = 15;
static const SkScalar kTabTopCurveWidth = 4;
static const SkScalar kTabBottomCurveWidth = 3;


///////////////////////////////////////////////////////////////////////////////
// TabGtk, public:

TabGtk::TabGtk(TabDelegate* delegate)
    : TabRendererGtk(),
      delegate_(delegate),
      closing_(false) {
}

TabGtk::~TabGtk() {
}

///////////////////////////////////////////////////////////////////////////////
// TabGtk, TabRendererGtk overrides:

bool TabGtk::IsSelected() const {
  return delegate_->IsTabSelected(this);
}

///////////////////////////////////////////////////////////////////////////////
// TabGtk, private:

void TabGtk::MakePathForTab(gfx::Path* path) const {
  DCHECK(path);

  SkScalar h = SkIntToScalar(height());
  SkScalar w = SkIntToScalar(width());

  path->moveTo(0, h);

  // Left end cap.
  path->lineTo(kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(kTabCapWidth - kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(kTabCapWidth, 0);

  // Connect to the right cap.
  path->lineTo(w - kTabCapWidth, 0);

  // Right end cap.
  path->lineTo(w - kTabCapWidth - kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(w - kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(w, h);

  // Close out the path.
  path->lineTo(0, h);
  path->close();
}

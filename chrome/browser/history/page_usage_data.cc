// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/history/page_usage_data.h"

//static
bool PageUsageData::Predicate(const PageUsageData* lhs,
                              const PageUsageData* rhs) {
  return lhs->GetScore() > rhs->GetScore();
}

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A stub implementation of HistoryPublisher used to provide needed symbols.
// For now there is no equivalent of this functionality on systems other than
// Windows.

#include "chrome/browser/history/history_publisher.h"

#include "base/time.h"

namespace history {

HistoryPublisher::HistoryPublisher() {
}

HistoryPublisher::~HistoryPublisher() {
}

bool HistoryPublisher::Init() {
  return false;
}

void HistoryPublisher::PublishDataToIndexers(const PageData& page_data)
    const {
}

void HistoryPublisher::DeleteUserHistoryBetween(const base::Time& begin_time,
                                                const base::Time& end_time)
    const {
}

}  // namespace history

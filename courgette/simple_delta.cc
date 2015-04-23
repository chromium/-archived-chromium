// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of the byte-level differential compression used internally by
// Courgette.

#include "courgette/simple_delta.h"

#include "base/basictypes.h"
#include "base/logging.h"

#include "courgette/third_party/bsdiff.h"

namespace courgette {

namespace {

Status BSDiffStatusToStatus(BSDiffStatus status) {
  switch (status) {
    case OK: return C_OK;
    case CRC_ERROR: return C_BINARY_DIFF_CRC_ERROR;
    default: return C_GENERAL_ERROR;
  }
}

}

Status ApplySimpleDelta(SourceStream* old, SourceStream* delta,
                        SinkStream* target) {
  return BSDiffStatusToStatus(ApplyBinaryPatch(old, delta, target));
}

Status GenerateSimpleDelta(SourceStream* old, SourceStream* target,
                           SinkStream* delta) {
  LOG(INFO) << "GenerateSimpleDelta "
            << old->Remaining() << " " << target->Remaining();
  return BSDiffStatusToStatus(CreateBinaryPatch(old, target, delta));
}

}  // namespace courgette

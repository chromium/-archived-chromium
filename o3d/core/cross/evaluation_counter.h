/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef O3D_CORE_CROSS_EVALUATION_COUNTER_H_
#define O3D_CORE_CROSS_EVALUATION_COUNTER_H_

#include "core/cross/service_implementation.h"

namespace o3d {

// Keeps track of the current evaluation count, used to determine whether a
// parameter's state is valid or needs to be computed.
class EvaluationCounter {
 public:
  static const InterfaceId kInterfaceId;

  explicit EvaluationCounter(ServiceLocator* service_locator)
      : service_(service_locator, this),
        evaluation_count_(0) {}

  // Marks all parameters as so they will get re-evaluated
  void InvalidateAllParameters() {
    ++evaluation_count_;
  }

  // Gets the current global evaluation count.
  int evaluation_count() {
    return evaluation_count_;
  }

 private:
  ServiceImplementation<EvaluationCounter> service_;

  // The global evaluation count;
  int evaluation_count_;
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_EVALUATION_COUNTER_H_

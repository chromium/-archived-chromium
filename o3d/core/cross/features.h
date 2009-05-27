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


// This file contains the Features declaration.

#ifndef O3D_CORE_CROSS_FEATURES_H_
#define O3D_CORE_CROSS_FEATURES_H_

#include "core/cross/types.h"
#include "core/cross/service_locator.h"
#include "core/cross/service_implementation.h"
#include "core/cross/renderer.h"

namespace o3d {

// Features is a class that represents features requested by the user
// for the O3D.
class Features {
 public:
  static const InterfaceId kInterfaceId;

  explicit Features(ServiceLocator* service_locator);

  // Initalizes the Features with the user requested features.
  // Parameters:
  //   requested_features: A comma separate string of features.
  void Init(const String& requested_features);

  bool floating_point_textures() const {
    return floating_point_textures_;
  }

  bool large_geometry() const {
    return large_geometry_;
  }

  bool windowless() const {
    return windowless_;
  }

  bool not_anti_aliased() const {
    return  not_anti_aliased_;
  }

  // This can be used to force the renderer to fail for testing.
  Renderer::InitStatus init_status() const {
    return init_status_;
  }

 private:
  ServiceImplementation<Features> service_;

  bool floating_point_textures_;
  bool large_geometry_;
  bool windowless_;
  bool not_anti_aliased_;
  Renderer::InitStatus init_status_;

  DISALLOW_COPY_AND_ASSIGN(Features);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_FEATURES_H_





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


// This file contains the Features implementation

#include "core/cross/precompile.h"
#include "core/cross/features.h"
#include <vector>
#include "base/string_util.h"
#include "core/cross/types.h"

namespace o3d {

const InterfaceId Features::kInterfaceId =
    InterfaceTraits<Features>::kInterfaceId;

Features::Features(ServiceLocator* service_locator)
    : service_(service_locator, this),
      floating_point_textures_(false),
      large_geometry_(false),
      windowless_(false),
      not_anti_aliased_(false),
      init_status_(Renderer::SUCCESS) {
}

void Features::Init(const String& requested_features) {
  std::vector<std::string> features;
  SplitString(requested_features, ',', &features);
  for (size_t jj = 0; jj < features.size(); ++jj) {
    const std::string& feature_string = features[jj];
    std::vector<std::string> arguments;
    SplitString(feature_string, '=', &arguments);
    const std::string feature(arguments.front());
    arguments.erase(arguments.begin());
    if (feature.compare("FloatingPointTextures") == 0) {
      floating_point_textures_ = true;
    } else if (feature.compare("LargeGeometry") == 0) {
      large_geometry_ = true;
    } else if (feature.compare("Windowless") == 0) {
      windowless_ = true;
    } else if (feature.compare("NotAntiAliased") == 0) {
      not_anti_aliased_ = true;
    } else if (feature.compare("MaxCapabilities") == 0) {
      large_geometry_ = true;
      floating_point_textures_ = true;
    } else if (feature.compare("InitStatus") == 0 &&
               !arguments.empty()) {
      int value;
      StringToInt(arguments[0], &value);
      init_status_ = static_cast<Renderer::InitStatus>(value);
    }
  }
}

}  // namespace o3d

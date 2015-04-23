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


// This file implements unit tests for class Features.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/features.h"

namespace o3d {

class FeaturesTest : public testing::Test {
 public:
  ServiceLocator* service_locator() {
    return service_locator_;
  }

 protected:
  FeaturesTest() { }

  virtual void SetUp() {
    // We need to create a new SerivceLocator because the global one
    // already has a global Features object registered on it.
    service_locator_ = new ServiceLocator;
  }

  virtual void TearDown() {
    delete service_locator_;
  }

  ServiceLocator* service_locator_;
};

TEST_F(FeaturesTest, Basic) {
  Features* features = new Features(service_locator());

  // Check that the features start off correctly.
  //
  // NOTE: For backward compatibility floating_point_textures and
  //     large_geometry default to true.  o3djs.util.makeClients before 0.1.35.0
  //     does not set the o3d_features plugin parameters and therefore
  //     Features::Init is not called.  o3djs,util.makeClients after and
  //     including 0.1.35.0 do set o3d_features and therefore Init is called
  //     which sets those to false to start.
  EXPECT_TRUE(features->floating_point_textures());
  EXPECT_TRUE(features->large_geometry());
  EXPECT_FALSE(features->windowless());
  EXPECT_FALSE(features->not_anti_aliased());
  EXPECT_EQ(features->init_status(), Renderer::SUCCESS);

  delete features;
}

TEST_F(FeaturesTest, Empty) {
  Features* features = new Features(service_locator());

  features->Init("");

  // Check that the features start off as false.
  EXPECT_FALSE(features->floating_point_textures());
  EXPECT_FALSE(features->large_geometry());
  EXPECT_FALSE(features->windowless());
  EXPECT_FALSE(features->not_anti_aliased());
  EXPECT_EQ(features->init_status(), Renderer::SUCCESS);

  delete features;
}

TEST_F(FeaturesTest, FloatingPointTextures) {
  Features* features = new Features(service_locator());

  features->Init("FloatingPointTextures");

  EXPECT_TRUE(features->floating_point_textures());
  EXPECT_FALSE(features->large_geometry());
  EXPECT_FALSE(features->windowless());
  EXPECT_FALSE(features->not_anti_aliased());
  EXPECT_EQ(features->init_status(), Renderer::SUCCESS);

  delete features;
}

TEST_F(FeaturesTest, LargeGeometry) {
  Features* features = new Features(service_locator());

  features->Init("LargeGeometry");

  EXPECT_FALSE(features->floating_point_textures());
  EXPECT_TRUE(features->large_geometry());
  EXPECT_FALSE(features->windowless());
  EXPECT_FALSE(features->not_anti_aliased());
  EXPECT_EQ(features->init_status(), Renderer::SUCCESS);

  delete features;
}

TEST_F(FeaturesTest, Windowless) {
  Features* features = new Features(service_locator());

  features->Init("Windowless");

  EXPECT_FALSE(features->floating_point_textures());
  EXPECT_FALSE(features->large_geometry());
  EXPECT_TRUE(features->windowless());
  EXPECT_FALSE(features->not_anti_aliased());
  EXPECT_EQ(features->init_status(), Renderer::SUCCESS);

  delete features;
}

TEST_F(FeaturesTest, NotAntiAliased) {
  Features* features = new Features(service_locator());

  features->Init("NotAntiAliased");

  EXPECT_FALSE(features->floating_point_textures());
  EXPECT_FALSE(features->large_geometry());
  EXPECT_FALSE(features->windowless());
  EXPECT_TRUE(features->not_anti_aliased());
  EXPECT_EQ(features->init_status(), Renderer::SUCCESS);

  delete features;
}

TEST_F(FeaturesTest, InitStatus) {
  static Renderer::InitStatus statuses[] = {
    Renderer::OUT_OF_RESOURCES,
    Renderer::GPU_NOT_UP_TO_SPEC,
    Renderer::INITIALIZATION_ERROR,
  };
  for (unsigned ii = 0; ii < arraysize(statuses); ++ii) {
    Features* features = new Features(service_locator());
    std::string s(StringPrintf("InitStatus=%d", statuses[ii]));
    features->Init(s);

    EXPECT_FALSE(features->floating_point_textures());
    EXPECT_FALSE(features->large_geometry());
    EXPECT_FALSE(features->windowless());
    EXPECT_FALSE(features->not_anti_aliased());
    EXPECT_EQ(features->init_status(), statuses[ii]);

    delete features;
  }
}

TEST_F(FeaturesTest, BadInput) {
  Features* features = new Features(service_locator());

  features->Init(",abcd,,efe,FloatingPointT,");

  EXPECT_FALSE(features->floating_point_textures());
  EXPECT_FALSE(features->large_geometry());
  EXPECT_FALSE(features->windowless());
  EXPECT_FALSE(features->not_anti_aliased());
  EXPECT_EQ(features->init_status(), Renderer::SUCCESS);

  delete features;
}

TEST_F(FeaturesTest, MultipleFeatures) {
  Features* features = new Features(service_locator());

  features->Init("FloatingPointTextures,Windowless");

  EXPECT_TRUE(features->floating_point_textures());
  EXPECT_FALSE(features->large_geometry());
  EXPECT_TRUE(features->windowless());
  EXPECT_FALSE(features->not_anti_aliased());
  EXPECT_EQ(features->init_status(), Renderer::SUCCESS);

  delete features;
}

TEST_F(FeaturesTest, MaxCapabilities) {
  Features* features = new Features(service_locator());

  features->Init("MaxCapabilities");

  EXPECT_TRUE(features->floating_point_textures());
  EXPECT_TRUE(features->large_geometry());
  EXPECT_FALSE(features->windowless());
  EXPECT_FALSE(features->not_anti_aliased());
  EXPECT_EQ(features->init_status(), Renderer::SUCCESS);

  delete features;
}

}  // namespace o3d

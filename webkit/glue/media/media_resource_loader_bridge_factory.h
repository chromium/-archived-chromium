// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_MEDIA_MEDIA_RESOURCE_LOADER_BRIDGE_FACTORY_H_
#define WEBKIT_GLUE_MEDIA_MEDIA_RESOURCE_LOADER_BRIDGE_FACTORY_H_

#include "testing/gtest/include/gtest/gtest_prod.h"
#include "webkit/glue/resource_loader_bridge.h"

namespace webkit_glue {

// A factory used to create a ResourceLoaderBridge for the media player.
// This factory is used also for testing. Testing code can use this class and
// override CreateBridge() to inject a mock ResourceLoaderBridge for code that
// interacts with it, e.g. BufferedDataSource.
class MediaResourceLoaderBridgeFactory {
 public:
  MediaResourceLoaderBridgeFactory(
      const GURL& referrer,
      const std::string& frame_origin,
      const std::string& main_frame_origin,
      int origin_pid,
      int app_cache_context_id,
      int32 routing_id);

  virtual ~MediaResourceLoaderBridgeFactory() {}

  // Factory method to create a ResourceLoaderBridge with the following
  // parameters:
  // |url| - URL of the resource to be loaded.
  // |load_flags| - Load flags for this loading.
  // |first_byte_position| - First byte position for a range request, -1 if not.
  // |last_byte_position| - Last byte position for a range request, -1 if not.
  virtual ResourceLoaderBridge* CreateBridge(
      const GURL& url,
      int load_flags,
      int64 first_byte_position,
      int64 last_byte_position);

 private:
  FRIEND_TEST(MediaResourceLoaderBridgeFactoryTest, GenerateHeaders);

  // Returns a range request header using parameters |first_byte_position| and
  // |last_byte_position|.
  // Negative numbers other than -1 are not allowed for |first_byte_position|
  // and |last_byte_position|. |first_byte_position| should always be less than
  // or equal to |last_byte_position| if they are both not -1.
  // Here's a list of valid parameters:
  // |first_byte_position|     |last_byte_position|
  //          0                        1000
  //       4096                        4096
  //          0                          -1
  //         -1                          -1
  // Empty string is returned on invalid parameters.
  static const std::string GenerateHeaders(int64 first_byte_position,
                                           int64 last_byte_position);

  GURL first_party_for_cookies_;
  GURL referrer_;
  std::string frame_origin_;
  std::string main_frame_origin_;
  std::string headers_;
  int origin_pid_;
  int app_cache_context_id_;
  int32 routing_id_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_MEDIA_MEDIA_RESOURCE_LOADER_BRIDGE_FACTORY_H_

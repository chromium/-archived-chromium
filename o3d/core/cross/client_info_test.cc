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


// This file implements unit tests for class ClientInfoManager.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/client_info.h"

namespace o3d {

class ClientInfoManagerTest : public testing::Test {
 public:
  ServiceLocator* service_locator() {
    return service_locator_;
  }

  ObjectManager* object_manager() {
    return object_manager_;
  }

 protected:
  ClientInfoManagerTest() { }

  virtual void SetUp() {
    // We need to create a new SerivceLocator because the global one
    // already has a global ClientInfoManager object registered on it.
    service_locator_ = new ServiceLocator;
    object_manager_ = new ObjectManager(service_locator_);
  }

  virtual void TearDown() {
    delete object_manager_;
    delete service_locator_;
  }

  ServiceLocator* service_locator_;
  ObjectManager* object_manager_;
};

TEST_F(ClientInfoManagerTest, Basic) {
  ClientInfoManager* client_info_manager =
      new ClientInfoManager(service_locator());
  ASSERT_TRUE(client_info_manager != NULL);

  // Check that the client_info_manager start off correctly.
  EXPECT_EQ(0, client_info_manager->client_info().texture_memory_used());
  EXPECT_EQ(0, client_info_manager->client_info().buffer_memory_used());
  EXPECT_EQ(0, client_info_manager->client_info().num_objects());
  EXPECT_FALSE(client_info_manager->client_info().software_renderer());

  delete client_info_manager;
}

TEST_F(ClientInfoManagerTest, AdjustTextureMemoryUsed) {
  ClientInfoManager* client_info_manager =
      new ClientInfoManager(service_locator());
  ASSERT_TRUE(client_info_manager != NULL);

  client_info_manager->AdjustTextureMemoryUsed(10);
  EXPECT_EQ(10, client_info_manager->client_info().texture_memory_used());
  client_info_manager->AdjustTextureMemoryUsed(10);
  EXPECT_EQ(20, client_info_manager->client_info().texture_memory_used());
  client_info_manager->AdjustTextureMemoryUsed(-10);
  EXPECT_EQ(10, client_info_manager->client_info().texture_memory_used());
  client_info_manager->AdjustTextureMemoryUsed(-10);
  EXPECT_EQ(0, client_info_manager->client_info().texture_memory_used());

  delete client_info_manager;
}

TEST_F(ClientInfoManagerTest, AdjustBufferMemoryUsed) {
  ClientInfoManager* client_info_manager =
      new ClientInfoManager(service_locator());
  ASSERT_TRUE(client_info_manager != NULL);

  client_info_manager->AdjustBufferMemoryUsed(10);
  EXPECT_EQ(10, client_info_manager->client_info().buffer_memory_used());
  client_info_manager->AdjustBufferMemoryUsed(10);
  EXPECT_EQ(20, client_info_manager->client_info().buffer_memory_used());
  client_info_manager->AdjustBufferMemoryUsed(-10);
  EXPECT_EQ(10, client_info_manager->client_info().buffer_memory_used());
  client_info_manager->AdjustBufferMemoryUsed(-10);
  EXPECT_EQ(0, client_info_manager->client_info().buffer_memory_used());

  delete client_info_manager;
}

TEST_F(ClientInfoManagerTest, SetNumObjects) {
  ClientInfoManager* client_info_manager =
      new ClientInfoManager(service_locator());
  ASSERT_TRUE(client_info_manager != NULL);

  ObjectBase* object_1 = new ObjectBase(service_locator());
  ASSERT_TRUE(object_1 != NULL);
  EXPECT_EQ(1, client_info_manager->client_info().num_objects());

  ObjectBase* object_2 = new ObjectBase(service_locator());
  ASSERT_TRUE(object_2 != NULL);
  EXPECT_EQ(2, client_info_manager->client_info().num_objects());

  delete object_1;
  EXPECT_EQ(1, client_info_manager->client_info().num_objects());
  delete object_2;
  EXPECT_EQ(0, client_info_manager->client_info().num_objects());

  delete client_info_manager;
}

TEST_F(ClientInfoManagerTest, SetSoftwareRenderer) {
  ClientInfoManager* client_info_manager =
      new ClientInfoManager(service_locator());
  ASSERT_TRUE(client_info_manager != NULL);

  client_info_manager->SetSoftwareRenderer(true);
  EXPECT_TRUE(client_info_manager->client_info().software_renderer());
  client_info_manager->SetSoftwareRenderer(false);
  EXPECT_FALSE(client_info_manager->client_info().software_renderer());

  delete client_info_manager;
}

}  // namespace o3d

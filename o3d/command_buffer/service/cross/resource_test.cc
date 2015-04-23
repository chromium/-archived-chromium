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


// Tests for the ResourceMap.

#include "tests/common/win/testing_common.h"
#include "command_buffer/service/cross/resource.h"

namespace o3d {
namespace command_buffer {

// Mock resource implementation that checks for leaks.
class ResourceMock : public Resource {
 public:
  ResourceMock() : Resource() {
    ++instance_count_;
  }
  virtual ~ResourceMock() {
    --instance_count_;
  }

  // Returns the instance count. The instance count is increased in the
  // constructor and decreased in the destructor, to track leaks. The reason is
  // that we can't mock the destructor, though we want to make sure the mock is
  // destroyed.
  static int instance_count() { return instance_count_; }
 private:
  static int instance_count_;
  DISALLOW_COPY_AND_ASSIGN(ResourceMock);
};
int ResourceMock::instance_count_ = 0;

// Test fixture for ResourceMap test. Creates a ResourceMap using a mock
// Resource, and checks for ResourceMock leaks.
class ResourceMapTest : public testing::Test {
 protected:
  typedef ResourceMap<ResourceMock> Map;
  virtual void SetUp() {
    instance_count_ = ResourceMock::instance_count();
    map_.reset(new Map());
  }
  virtual void TearDown() {
    CheckLeaks();
  }

  // Makes sure we didn't leak any ResourceMock object.
  void CheckLeaks() {
    EXPECT_EQ(instance_count_, ResourceMock::instance_count());
  }

  Map *map() const { return map_.get(); }
 private:
  int instance_count_;
  scoped_ptr<Map> map_;
};

TEST_F(ResourceMapTest, TestMap) {
  // check that initial mapping is empty.
  EXPECT_EQ(NULL, map()->Get(0));
  EXPECT_EQ(NULL, map()->Get(1));
  EXPECT_EQ(NULL, map()->Get(392));

  // create a new resource, assign it to an ID.
  ResourceMock *resource = new ResourceMock();
  map()->Assign(123, resource);
  EXPECT_EQ(resource, map()->Get(123));

  // Destroy the resource, making sure the object is deleted.
  EXPECT_EQ(true, map()->Destroy(123));
  EXPECT_EQ(false, map()->Destroy(123));  // destroying again should fail.
  resource = NULL;
  CheckLeaks();

  // create a new resource, add it to the map, and make sure it gets deleted
  // when we assign a new resource to that ID.
  resource = new ResourceMock();
  map()->Assign(1, resource);
  resource = new ResourceMock();
  map()->Assign(1, resource);
  EXPECT_EQ(resource, map()->Get(1));  // check that we have the new resource.
  EXPECT_EQ(true, map()->Destroy(1));
  CheckLeaks();

  // Adds 3 resources, then call DestroyAllResources().
  resource = new ResourceMock();
  map()->Assign(1, resource);
  resource = new ResourceMock();
  map()->Assign(2, resource);
  resource = new ResourceMock();
  map()->Assign(3, resource);
  map()->DestroyAllResources();
  EXPECT_EQ(NULL, map()->Get(1));
  EXPECT_EQ(NULL, map()->Get(2));
  EXPECT_EQ(NULL, map()->Get(3));
  CheckLeaks();
}

}  // namespace command_buffer
}  // namespace o3d

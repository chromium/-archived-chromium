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


// Tests the functionality of the Stream class.

#include "core/cross/stream.h"
#include "core/cross/service_dependency.h"
#include "core/cross/buffer.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/object_manager.h"

namespace o3d {

namespace {

// A class derived from Buffer that trivially defines the virtual methods.
class FieldTest : public Field {
 public:
  FieldTest() : Field(g_service_locator, NULL, 3, 4) {}

  virtual size_t GetFieldComponentSize() const { return 1; }

  virtual void SetFromFloats(const float* source,
                             unsigned source_stride,
                             unsigned destination_start_index,
                             unsigned num_elements) { }

  virtual void SetFromUInt32s(const uint32* source,
                              unsigned source_stride,
                              unsigned destination_start_index,
                              unsigned num_elements) { }

  virtual void SetFromUByteNs(const uint8* source,
                              unsigned source_stride,
                              unsigned destination_start_index,
                              unsigned num_elements) { }

  virtual void GetAsFloats(unsigned source_start_index,
                           float* destination,
                           unsigned destination_stride,
                           unsigned num_elements) const { }

  virtual bool SetFromMemoryStream(MemoryReadStream* stream) { return true; }

  virtual void ConcreteCopy(const Field& source) { }
};

}  // end unnamed namespace

// Basic test fixture for the Stream class.  Nothing gets created during SetUp.
class StreamBasic : public testing::Test {
 protected:
  StreamBasic()
      : object_manager_(g_service_locator) { }

  ServiceDependency<ObjectManager> object_manager_;
};

// Tests the Stream constructor to make sure that all the arguments are set
// properly.
TEST_F(StreamBasic, Constructor) {
  FieldTest::Ref field(new FieldTest());

  unsigned start_index = 1;
  unsigned semantic_index = 2;
  Stream::Semantic semantic = Stream::BINORMAL;

  Stream stream(g_service_locator,
                field, start_index, semantic, semantic_index);

  EXPECT_EQ(field, &stream.field());
  EXPECT_EQ(start_index, stream.start_index());
  EXPECT_EQ(semantic, stream.semantic());
  EXPECT_EQ(semantic_index, stream.semantic_index());
}

}  // namespace o3d

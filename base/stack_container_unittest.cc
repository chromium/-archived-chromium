// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/stack_container.h"

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"
#include "base/ref_counted.h"

namespace {

class Dummy : public base::RefCounted<Dummy> {
 public:
  Dummy(int* alive) : alive_(alive) {
    ++*alive_;
  }
  ~Dummy() {
    --*alive_;
  }
 private:
  int* const alive_;
};

}  // namespace

TEST(StackContainer, Vector) {
  const int stack_size = 3;
  StackVector<int, stack_size> vect;
  const int* stack_buffer = &vect.stack_data().stack_buffer()[0];

  // The initial |stack_size| elements should appear in the stack buffer.
  EXPECT_EQ(stack_size, vect.container().capacity());
  for (int i = 0; i < stack_size; i++) {
    vect.container().push_back(i);
    EXPECT_EQ(stack_buffer, &vect.container()[0]);
    EXPECT_TRUE(vect.stack_data().used_stack_buffer_);
  }

  // Adding more elements should push the array onto the heap.
  for (int i = 0; i < stack_size; i++) {
    vect.container().push_back(i + stack_size);
    EXPECT_NE(stack_buffer, &vect.container()[0]);
    EXPECT_FALSE(vect.stack_data().used_stack_buffer_);
  }

  // The array should still be in order.
  for (int i = 0; i < stack_size * 2; i++)
    EXPECT_EQ(i, vect.container()[i]);

  // Resize to smaller. Our STL implementation won't reallocate in this case,
  // otherwise it might use our stack buffer. We reserve right after the resize
  // to guarantee it isn't using the stack buffer, even though it doesn't have
  // much data.
  vect.container().resize(stack_size);
  vect.container().reserve(stack_size * 2);
  EXPECT_FALSE(vect.stack_data().used_stack_buffer_);

  // Copying the small vector to another should use the same allocator and use
  // the now-unused stack buffer. GENERALLY CALLERS SHOULD NOT DO THIS since
  // they have to get the template types just right and it can cause errors.
  std::vector<int, StackAllocator<int, stack_size> > other(vect.container());
  EXPECT_EQ(stack_buffer, &other.front());
  EXPECT_TRUE(vect.stack_data().used_stack_buffer_);
  for (int i = 0; i < stack_size; i++)
    EXPECT_EQ(i, other[i]);
}

TEST(StackContainer, VectorDoubleDelete) {
  // Regression testing for double-delete.
  typedef StackVector<scoped_refptr<Dummy>, 2> Vector;
  typedef Vector::ContainerType Container;
  Vector vect;

  int alive = 0;
  scoped_refptr<Dummy> dummy(new Dummy(&alive));
  EXPECT_EQ(alive, 1);

  vect->push_back(dummy);
  EXPECT_EQ(alive, 1);

  Dummy* dummy_unref = dummy.get();
  dummy = NULL;
  EXPECT_EQ(alive, 1);

  Container::iterator itr = std::find(vect->begin(), vect->end(), dummy_unref);
  EXPECT_EQ(itr->get(), dummy_unref);
  vect->erase(itr);
  EXPECT_EQ(alive, 0);

  // Shouldn't crash at exit.
}

TEST(StackContainer, BufferAlignment) {
  StackVector<wchar_t, 16> text;
  text->push_back(L'A');
  text->push_back(L'B');
  text->push_back(L'C');
  text->push_back(L'D');
  text->push_back(L'E');
  text->push_back(L'F');
  text->push_back(0);

  const wchar_t* buffer = &text[1];
  bool even_aligned = (0 == (((size_t)buffer) & 0x1));
  EXPECT_EQ(even_aligned, true);
}

// Make sure all the class compiles correctly.
template StackVector<int, 2>;
template StackVector<scoped_refptr<Dummy>, 2>;

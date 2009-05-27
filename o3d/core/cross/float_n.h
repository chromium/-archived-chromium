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


// This file contains the definition and (inline) implementation of Float2,
// Float3 and Float4.
// Note: Although these classes look like a good candidate for templating,
// we cannot template them as they each require a constructor taking a different
// number of arguments.

#ifndef O3D_CORE_CROSS_FLOAT_N_H_
#define O3D_CORE_CROSS_FLOAT_N_H_

#include "core/cross/types.h"

namespace o3d {

// Class for storing and accessing two consecutive floats.
class Float2 {
 public:
  // Default constructor does not initialize the data in any way.
  Float2() {}

  // Creates a Float2 out of two floats
  Float2(float v0, float v1) {
    data_[0] = v0;
    data_[1] = v1;
  }

  Float2(const Float2& v) { CopyFrom(v); }

  float operator[](int index) const { return data_[index]; }
  float& operator[](int index) { return data_[index]; }

  // Returns a pointer to the start of the internal data storage.
  const float* GetFloatArray() const { return &data_[0]; }

  Float2& operator= (const Float2& v) {
    CopyFrom(v);
    return *this;
  }

  float getX() const { return data_[0]; }
  void setX(float v) { data_[0] = v; }
  float getY() const { return data_[1]; }
  void setY(float v) { data_[1] = v; }

 private:
  // Copies the values from another Float2.
  void CopyFrom(const Float2& v) {
    data_[0] = v[0];
    data_[1] = v[1];
  }
  // Data storage.
  float data_[2];
};

// Class for storing and accessing three consecutive floats.
class Float3 {
 public:
  // Default constructor does not initialize the data in any way
  Float3() {}

  // Creates a Float3 out of three floats
  Float3(float v0, float v1, float v2) {
    data_[0] = v0;
    data_[1] = v1;
    data_[2] = v2;
  }

  Float3(const Float3& v) { CopyFrom(v); }
  explicit Float3(const Vector3& v) {
    data_[0] = v.getX();
    data_[1] = v.getY();
    data_[2] = v.getZ();
  }
  explicit Float3(const Point3& v) {
    data_[0] = v.getX();
    data_[1] = v.getY();
    data_[2] = v.getZ();
  }

  float operator[](int index) const { return data_[index]; }
  float& operator[](int index) { return data_[index]; }

  // Returns a pointer to the start of the internal data storage.
  const float* GetFloatArray() const { return &data_[0]; }

  Float3& operator= (const Float3& v) {
    CopyFrom(v);
    return *this;
  }

  float getX() const { return data_[0]; }
  void setX(float v) { data_[0] = v; }
  float getY() const { return data_[1]; }
  void setY(float v) { data_[1] = v; }
  float getZ() const { return data_[2]; }
  void setZ(float v) { data_[2] = v; }

 private:
  // Copies the values from another Float2.
  void CopyFrom(const Float3& v) {
    data_[0] = v[0];
    data_[1] = v[1];
    data_[2] = v[2];
  }

  // Data storage.
  float data_[3];
};

inline Vector3 Float3ToVector3(const Float3& v) {
  return Vector3(v.getX(), v.getY(), v.getZ());
}

inline Point3 Float3ToPoint3(const Float3& v) {
  return Point3(v.getX(), v.getY(), v.getZ());
}

// Class for storing and accessing four consecutive floats.
class Float4 {
 public:
  // Default constructor does not initialize the data in any way
  Float4() {}

  // Creates a Float4 out of four floats
  Float4(float v0, float v1, float v2, float v3) {
    data_[0] = v0;
    data_[1] = v1;
    data_[2] = v2;
    data_[3] = v3;
  }

  Float4(const Float4& v) { CopyFrom(v); }
  explicit Float4(const Vector4& v) {
    data_[0] = v.getX();
    data_[1] = v.getY();
    data_[2] = v.getZ();
    data_[3] = v.getW();
  }
  explicit Float4(const Quaternion& v) {
    data_[0] = v.getX();
    data_[1] = v.getY();
    data_[2] = v.getZ();
    data_[3] = v.getW();
  }

  float operator[](int index) const { return data_[index]; }
  float& operator[](int index) { return data_[index]; }

  // Returns a pointer to the start of the internal data storage.
  const float* GetFloatArray() const { return &data_[0]; }

  Float4& operator= (const Float4& v) {
    CopyFrom(v);
    return *this;
  }

  float getX() const { return data_[0]; }
  void setX(float v) { data_[0] = v; }
  float getY() const { return data_[1]; }
  void setY(float v) { data_[1] = v; }
  float getZ() const { return data_[2]; }
  void setZ(float v) { data_[2] = v; }
  float getW() const { return data_[3]; }
  void setW(float v) { data_[3] = v; }

 private:
  // Copies the values from another Float2.
  void CopyFrom(const Float4& v) {
    data_[0] = v[0];
    data_[1] = v[1];
    data_[2] = v[2];
    data_[3] = v[3];
  }

  // Data storage.
  float data_[4];
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_FLOAT_N_H_

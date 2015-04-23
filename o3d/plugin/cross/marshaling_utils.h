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


// This file contains utility functions for marshaling between
// C++ types and dynamic types.

#ifndef O3D_PLUGIN_CROSS_MARSHALING_UTILS_H_
#define O3D_PLUGIN_CROSS_MARSHALING_UTILS_H_

#include <vector>

namespace o3d {

// Converts a std::vector<float>, representing a JavaScript array
// of numbers, to a FloatN, VectorN, or PointN.  This template function
// supports conversion to any type which accesses float elements using
// operator[].
template <typename VectorType, int dimension>
VectorType VectorToType(void* plugin_data,
                        const std::vector<float>& dynamic_value) {
  if (dynamic_value.size() != dimension) {
    o3d::ServiceLocator* service_locator =
        static_cast<glue::_o3d::PluginObject*>(
            plugin_data)->service_locator();
    O3D_ERROR(service_locator)
        << "Vector type expected array of " << dimension
        << " number values, got " << dynamic_value.size();
    return VectorType();
  }
  VectorType vector_value;
  for (int i = 0; i < dimension; ++i) {
    vector_value[i] = dynamic_value[i];
  }
  return vector_value;
}

// Converts a FloatN, VectorN or PointN to an std::vector<float>.
// This template function supports conversion from any type which
// accesses float elements using operator[].
template <typename VectorType, int dimension>
std::vector<float> VectorFromType(const VectorType& vector_value) {
  std::vector<float> dynamic_value(dimension);
  for (int i = 0; i < dimension; ++i) {
    dynamic_value[i] = vector_value[i];
  }
  return dynamic_value;
}

// Converts an std::vector<std::vector<float> > to a MatrixN.
template <typename MatrixType, int rows, int columns>
MatrixType VectorOfVectorToType(
    void* plugin_data,
    const std::vector<std::vector<float> >& dynamic_value) {
  if (dynamic_value.size() != rows) {
    o3d::ServiceLocator* service_locator =
        static_cast<glue::_o3d::PluginObject*>(
            plugin_data)->service_locator();
    O3D_ERROR(service_locator)
        << "Matrix type expected array of " << rows
        << " arrays of " << columns << " number values, got "
        << dynamic_value.size() << " rows";
    return MatrixType();
  }
  MatrixType matrix_value;
  for (int i = 0; i != rows; ++i) {
    if (dynamic_value[i].size() != columns) {
      o3d::ServiceLocator* service_locator =
          static_cast<glue::_o3d::PluginObject*>(
              plugin_data)->service_locator();
      O3D_ERROR(service_locator)
          << "Matrix type expected array of " << rows
          << " arrays of " << columns << " number values, got "
          << dynamic_value[i].size() << " columns in row "
          << i;
      return MatrixType();
    }
    for (int j = 0; j < columns; ++j) {
      matrix_value.setElem(i, j, dynamic_value[i][j]);
    }
  }
  return matrix_value;
}

// Converts a MatrixN to a std::vector<std::vector<float> >.
template <typename MatrixType, int rows, int columns>
std::vector<std::vector<float> > VectorOfVectorFromType(
    const MatrixType& matrix_value) {
  std::vector<std::vector<float> > dynamic_value(rows);
  for (int i = 0; i < rows; ++i) {
    dynamic_value[i].resize(columns);
    for (int j = 0; j < columns; ++j) {
      dynamic_value[i][j] = matrix_value.getElem(i, j);
    }
  }
  return dynamic_value;
}

}  // namespace o3d

#endif  // O3D_PLUGIN_CROSS_MARSHALING_UTILS_H_

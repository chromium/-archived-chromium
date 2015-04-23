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


#ifndef O3D_CORE_CROSS_SERVICE_INTERFACE_TRAITS_H_
#define O3D_CORE_CROSS_SERVICE_INTERFACE_TRAITS_H_

namespace o3d {

typedef const void* InterfaceId;

// Provides information (currently only the interface ID) about a service
// interface.
template <typename Interface>
class InterfaceTraits {
 public:
  // The interface ID for the service interface.
  static const InterfaceId kInterfaceId;

 private:
  static const char unique_;

  InterfaceTraits();
  DISALLOW_COPY_AND_ASSIGN(InterfaceTraits);
};

template <typename Interface>
const void* const InterfaceTraits<Interface>::kInterfaceId =
    &InterfaceTraits<Interface>::unique_;

template <typename Interface>
const char InterfaceTraits<Interface>::unique_ = 0;

}  // namespace o3d

#endif  // O3D_CORE_CROSS_SERVICE_INTERFACE_TRAITS_H_

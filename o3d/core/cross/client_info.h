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


// This file defines the ClientInfo class.

#ifndef O3D_CORE_CROSS_CLIENT_INFO_H_
#define O3D_CORE_CROSS_CLIENT_INFO_H_

#include "core/cross/types.h"
#include "core/cross/service_locator.h"
#include "core/cross/service_implementation.h"

namespace o3d {

// This class is used to report infomation about the client.
class ClientInfo {
 public:
  ClientInfo()
      : num_objects_(0),
        texture_memory_used_(0),
        buffer_memory_used_(0),
        software_renderer_(false) {
  }

  // The number of objects the client is currently tracking.
  int num_objects() const {
    return num_objects_;
  }

  // The amount of texture memory used.
  int texture_memory_used() const {
    return texture_memory_used_;
  };

  // The amount of texture memory used.
  int buffer_memory_used() const {
    return buffer_memory_used_;
  }

  // Whether or not we are using the software renderer.
  bool software_renderer() const {
    return software_renderer_;
  }

 private:
  friend class ClientInfoManager;

  int num_objects_;
  int texture_memory_used_;
  int buffer_memory_used_;
  bool software_renderer_;
};

// A class to manage the client info so other classes can easily look it up.
class ClientInfoManager {
 public:
  static const InterfaceId kInterfaceId;

  explicit ClientInfoManager(ServiceLocator* service_locator);

  const ClientInfo& client_info();

  // Adds or subtracts from the amount of texture memory used.
  void AdjustTextureMemoryUsed(int amount) {
    client_info_.texture_memory_used_ += amount;
    DCHECK(client_info_.texture_memory_used_ >= 0);
  }

  // Adds or subtracts from the amount of texture memory used.
  void AdjustBufferMemoryUsed(int amount) {
    client_info_.buffer_memory_used_ += amount;
    DCHECK(client_info_.buffer_memory_used_ >= 0);
  }

  void SetSoftwareRenderer(bool used) {
    client_info_.software_renderer_ = used;
  }

private:
  ServiceImplementation<ClientInfoManager> service_;

  ClientInfo client_info_;

  DISALLOW_COPY_AND_ASSIGN(ClientInfoManager);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_CLIENT_INFO_H_

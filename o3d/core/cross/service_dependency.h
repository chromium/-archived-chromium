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


#ifndef O3D_CORE_CROSS_SERVICE_DEPENDENCY_H_
#define O3D_CORE_CROSS_SERVICE_DEPENDENCY_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "core/cross/service_locator.h"

namespace o3d {

// Locates and provides access to a service through an interface class.
// The dependency is located from a ServiceLocator, either immediately or as
// soon as it is added to the ServiceLocator.
template <typename Interface>
class ServiceDependency : public IServiceDependency {
 public:
  explicit ServiceDependency(ServiceLocator* service_locator)
      : service_locator_(service_locator),
        service_(NULL) {
    service_locator_->AddDependency(Interface::kInterfaceId, this);
  }

  ~ServiceDependency() {
    service_locator_->RemoveDependency(Interface::kInterfaceId, this);
  }

  Interface* Get() const {
    DCHECK(NULL != service_);
    return service_;
  }

  Interface* operator->() const {
    DCHECK(NULL != service_);
    return service_;
  }

  bool IsAvailable() const {
    return service_ != NULL;
  }

 private:
  virtual void Update(void* newService) {
    service_ = static_cast<Interface*>(newService);
  }

  ServiceLocator* service_locator_;
  Interface* service_;
  DISALLOW_COPY_AND_ASSIGN(ServiceDependency);
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_SERVICE_DEPENDENCY_H_

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


#include "core/cross/precompile.h"
#include "base/logging.h"
#include "core/cross/service_locator.h"

namespace o3d {

void ServiceLocator::AddService(InterfaceId interfaceId, void* service) {
  // Check service with this interface is not already installed first.
  DCHECK(services_.end() == services_.find(interfaceId));
  services_.insert(ServiceMap::value_type(interfaceId, service));

  DependencyList& existingDependencies = dependencies_[interfaceId];
  for (DependencyList::iterator it = existingDependencies.begin();
       it != existingDependencies.end(); ++it) {
    (*it)->Update(service);
  }
}

void ServiceLocator::RemoveService(InterfaceId interfaceId, void* service) {
  // Remove the existing service, verifying that the caller identified
  // one that is currently active.
  ServiceMap::iterator serviceIt = services_.find(interfaceId);
  DCHECK_EQ(service, serviceIt->second);
  services_.erase(serviceIt);

  DependencyList& existingDependencies = dependencies_[interfaceId];
  for (DependencyList::iterator it = existingDependencies.begin();
       it != existingDependencies.end(); ++it) {
    (*it)->Update(NULL);
  }
}

void ServiceLocator::AddDependency(InterfaceId interfaceId,
                                   IServiceDependency* dependency) {
  dependencies_[interfaceId].push_back(dependency);

  ServiceMap::const_iterator serviceIt = services_.find(interfaceId);
  if (serviceIt != services_.end())
    dependency->Update(serviceIt->second);
  else
    dependency->Update(NULL);
}

void ServiceLocator::RemoveDependency(InterfaceId interfaceId,
                                      IServiceDependency* dependency) {
  dependencies_[interfaceId].remove(dependency);
  dependency->Update(NULL);
}
}  // namespace o3d

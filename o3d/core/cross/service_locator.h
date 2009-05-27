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


#ifndef O3D_CORE_CROSS_SERVICE_LOCATOR_H_
#define O3D_CORE_CROSS_SERVICE_LOCATOR_H_

#include <list>
#include <map>

#include "base/logging.h"
#include "base/basictypes.h"
#include "core/cross/service_interface_traits.h"

namespace o3d {

template <typename Interface>
class ServiceImplementation;

template <typename Interface>
class ServiceDependency;

// No virtual destructor for IServiceDependency because it is never destroyed
// through this class and because if forces the destructor in derived template
// classes to be instantiated early, causing a compilation error in some cases.
class IServiceDependency {
  friend class ServiceLocator;
 private:
  virtual void Update(void* newService) = 0;
};

// A ServiceLocator tracks a number of services and connects them together
// through their ServiceDependencies. When a service is constucted, one or
// more ServiceImplementation member variables cause the service to be
// registered for each implemented service interface. Then zero or more
// ServiceDependency member variables register themselves as needing references
// to other services. When these services become available, the dependencies
// are updated to reference them. When these services are not available, the
// dependencies report their reference as NULL.
class ServiceLocator {
  template <typename Interface>
  friend class ServiceImplementation;

  template <typename Interface>
  friend class ServiceDependency;

  typedef std::list<IServiceDependency*> DependencyList;
  typedef std::map<InterfaceId, DependencyList> DependencyMap;
  typedef std::map<InterfaceId, void*> ServiceMap;

 public:
  ServiceLocator() {}

  // Returns whether a given service is available.
  template <typename Interface>
  bool IsAvailable() const {
    ServiceMap::const_iterator it = services_.find(Interface::kInterfaceId);
    return it != services_.end();
  }
  // Get a pointer to a service added to the service locator. Consider using a
  // ServiceDependency instead.
  template <typename Interface>
  Interface* GetService() const {
    ServiceMap::const_iterator it = services_.find(Interface::kInterfaceId);
    if (it != services_.end()) {
      return static_cast<Interface*>(it->second);
    } else {
      DCHECK(false);
      return NULL;
    }
  }

 private:
  // Add service to list of those available through the service locator
  // For any services previously added that are dependent on this one, update
  // their dependency pointer to point here. This cannot be invoked directly.
  // Use ServiceImplementation class.
  void AddService(InterfaceId interfaceId, void* service);

  // Remove an existing service from the service locator. Update any existing
  // dependencies referencing this service to NULL. This cannot be invoked
  // directly. Use ServiceImplementation class.
  void RemoveService(InterfaceId interfaceId, void* service);

  // Add a service dependency to the service locator. If the service has already
  // been added then just modify the dependency parameter to reference the
  // known service. Otherwise, set the dependency to NULL and add it to a list
  // of those that will be resolved as soon as the necessary service is added.
  // This cannot be invoked directly. Use ServiceDependency class.
  void AddDependency(InterfaceId interfaceId, IServiceDependency* dependency);

  // Remove a service dependency from the service locator.
  // This cannot be invoked directly. Use ServiceDependency class.
  void RemoveDependency(InterfaceId interfaceId,
                        IServiceDependency* dependency);

  DependencyMap dependencies_;
  ServiceMap services_;
  DISALLOW_COPY_AND_ASSIGN(ServiceLocator);
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_SERVICE_LOCATOR_H_

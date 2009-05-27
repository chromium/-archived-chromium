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


#include "core/cross/service_dependency.h"
#include "core/cross/service_implementation.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

namespace {

class IService1 {
 public:
  static const InterfaceId kInterfaceId;
};

const InterfaceId IService1::kInterfaceId =
    InterfaceTraits<IService1>::kInterfaceId;

// A service implementing one interface with no dependencies.
class CService1 : public IService1 {
 public:
  explicit CService1(ServiceLocator* service_locator)
      : service_(service_locator, this) {
  }

  ServiceImplementation<IService1> service_;
};

class IService2 {
 public:
  static const InterfaceId kInterfaceId;
};

const InterfaceId IService2::kInterfaceId =
    InterfaceTraits<IService2>::kInterfaceId;

// A service implementing one interface with one dependency.
class DependentService2 : public IService2 {
 public:
  explicit DependentService2(ServiceLocator* service_locator)
      : service_(service_locator, this),
        dependency_(service_locator) {
  }

  ServiceImplementation<IService2> service_;
  ServiceDependency<IService1> dependency_;
};
}

class ServiceLocatorTest : public testing::Test {
 protected:
  ServiceLocator locator_;
};

TEST_F(ServiceLocatorTest, ServiceNotAvailableIfNotAdded) {
  EXPECT_FALSE(locator_.IsAvailable<IService1>());
}

TEST_F(ServiceLocatorTest, CanRetreiveServiceImplementingSingleInterface) {
  CService1 service1(&locator_);
  EXPECT_TRUE(locator_.IsAvailable<IService1>());
  EXPECT_EQ(&service1, locator_.GetService<IService1>());
}

TEST_F(ServiceLocatorTest, CanRetreiveServiceImplementingExplicitInterface) {
  CService1 service1(&locator_);
  EXPECT_EQ(&service1, locator_.GetService<IService1>());
}

TEST_F(ServiceLocatorTest,
  CanAddAndRetreiveServiceWithDependenciesWhenDepencenciesAlreadyExist) {
  CService1 service1(&locator_);
  DependentService2 service2(&locator_);
  EXPECT_EQ(&service2, locator_.GetService<IService2>());
  EXPECT_EQ(&service1, service2.dependency_.Get());
}

TEST_F(ServiceLocatorTest,
  CanAddAndRetreiveServiceWithDependenciesWhenDepencenciesDontYetExist) {
  DependentService2 service2(&locator_);
  CService1 service1(&locator_);
  EXPECT_EQ(&service2, locator_.GetService<IService2>());
  EXPECT_EQ(&service1, service2.dependency_.Get());
}
}

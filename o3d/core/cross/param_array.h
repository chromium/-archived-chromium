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


// This file contains the declaration of class ParamArray and ParamParamArray.

#ifndef O3D_CORE_CROSS_PARAM_ARRAY_H_
#define O3D_CORE_CROSS_PARAM_ARRAY_H_

#include <vector>
#include "core/cross/named_object.h"
#include "core/cross/param.h"

namespace o3d {

class IClassManager;

// ParamArray is an array of a Params that can be accessed by index. Two common
// uses are all the matrices needed for skinning can be stored in a ParamArray.
// Another is for texture animation, the textures can be stored in a ParamArray.
class ParamArray : public NamedObject {
 public:
  typedef SmartPointer<ParamArray> Ref;
  typedef WeakPointer<ParamArray> WeakPointerType;
  typedef std::vector<Param::Ref> ParamRefVector;

  ~ParamArray();

  // Creates a Param of the given type at the index requested. If a Param
  // already exists at that index the new param will replace that it. If the
  // index is past the end of the current array params of the requested type
  // will be created to fill out the array to the requested index.
  //
  // From C++ use the template CreateParam<ParamType>.
  //
  // Parameters:
  //   index: index at which to create new param.
  //   class_type: type of param to create.
  // Returns:
  //   Param created at index or NULL if failure.
  Param* CreateParamByClass(unsigned index,
                            const ObjectBase::Class* class_type);

  // Same as CreateParamByClass except takes a class name for Javascript.
  // Parameters:
  //   index: index at which to create new param.
  //   class_name: class name of param to create.
  // Returns:
  //   index to Param created at index or NULL if failure.
  Param* CreateParamByClassName(unsigned index,
                                const String& class_name);

  // Resizes the array of params.
  // Parameters:
  //   num_param: number of params to put in array.
  //   class_type: type of param to create if params need to be created.
  void ResizeByClass(unsigned num_param, const ObjectBase::Class* class_type);

  // Same as ResizeByClass except takes a class name for Javascript.
  // Parameters:
  //   index: index at which to create new param.
  //   class_name: class name of param to create.
  void ResizeByClassName(unsigned num_param, const String& class_name);

  // Returns the number of params.  Note: In javascript this is called "length".
  unsigned size() const {
    return static_cast<unsigned>(params_.size());
  }

  // Returns a const reference to the actual array of params.
  const ParamRefVector& params() const {
    return params_;
  }

  // Removes a range of params
  void RemoveParams(unsigned start_index, unsigned num_to_remove);

  // A typesafe version of CreateParamByClass for C++. Example usage
  // ParamFloat4* param = param_vector->CreateParam<ParamFloat4>(index);
  template <typename T>
  T* CreateParam(unsigned index) {
    return down_cast<T*>(CreateParamByClass(index, T::GetApparentClass()));
  }

  // Gets a Param from the ParamArray by index. From C++ use GetParam.
  // Parameters:
  //   index: index of param to get.
  // Returns:
  //   The address of the Param or NULL if the index is out of range.
  Param* GetUntypedParam(unsigned index) const {
    return index < params_.size() ? params_[index].Get() : NULL;
  }

  // A typesafe version of GetUntypedParam for C++. Example usage
  // ParamFloat4* param = array->GetParam<ParamFloat4>(index);
  // Parameters:
  //   index: index of param to get.
  // Returns:
  //   The address of the Param or NULL if the index is out of range or the type
  //   is incompatible.
  template <typename T>
  T* GetParam(unsigned index) const {
    Param* param = GetUntypedParam(index);
    return (param && param->IsA(T::GetApparentClass())) ?
        down_cast<T*>(param) : NULL;
  }

  // Checks if a param is in this ParamArray
  // Parameters:
  //   param: param to check for.
  // Returns:
  //   True if the param is in the array.
  bool ParamInArray(const Param* param) const;

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

 private:
  explicit ParamArray(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  IClassManager* class_manager_;

  // The array of params.
  ParamRefVector params_;

  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  O3D_DECL_CLASS(ParamArray, NamedObject);
  DISALLOW_COPY_AND_ASSIGN(ParamArray);
};

// A Param that holds a weak pointer to a ParamArray.
class ParamParamArray : public TypedRefParam<ParamArray> {
 public:
  typedef SmartPointer<ParamParamArray> Ref;

  ParamParamArray(ServiceLocator* service_locator,
                  bool dynamic,
                  bool read_only)
      : TypedRefParam<ParamArray>(service_locator, dynamic, read_only) {
  }

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamParamArray, RefParamBase);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_PARAM_ARRAY_H_

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


// Contains the declaration for ParamObject, the base class for all objects that
// can have Params.

#ifndef O3D_CORE_CROSS_PARAM_OBJECT_H_
#define O3D_CORE_CROSS_PARAM_OBJECT_H_

#include <map>
#include <vector>
#include <utility>
#include "core/cross/named_object.h"
#include "core/cross/param.h"
#include "core/cross/types.h"

namespace o3d {

class DrawContext;
class IClassManager;
class Transform;

// Dictionary of Param objects indexed by name.
typedef std::map<String, Param::Ref> NamedParamRefMap;

// Base class of all objects that can contain Param elements.
// Defines methods to add and remove params, search for params, etc.
class ParamObject : public NamedObject {
 public:
  typedef SmartPointer<ParamObject> Ref;

  // Constructor takes a pointer to the Client object that owns the
  // the ParamObject.
  explicit ParamObject(ServiceLocator* service_locator);
  virtual ~ParamObject();

  // You can check this value, if it has changed since the last time you checked
  // it Params have been added or removed.
  // Returns:
  //   The parameter change count.
  int change_count() const {
    return change_count_;
  }

  // Creates a Param with the given name and Class type on the ParamObject.
  // Where possible it is recommended to use the CreateParam
  // Parameters:
  //  param_name: Name of the param which must be unique from other params on
  //              this object.
  //  class_type: Class of parameter
  // Returns:
  //  Newly created parameter.
  Param* CreateParamByClass(const String& param_name,
                            const ObjectBase::Class* class_type);

  // Creates a Param with the given name and class name.
  // Parameters:
  //  param_name: Name of the param which must be unique from other params on
  //              this object.
  //  class_type_name: class type name of the desired parameter type. Valid type
  //                   names are:
  //                   'o3d.ParamFloat',
  //                   'o3d.ParamFloat2',
  //                   'o3d.ParamFloat3',
  //                   'o3d.ParamFloat4',
  //                   'o3d.ParamMatrix4',
  //                   'o3d.ParamInteger',
  //                   'o3d.ParamBoolean',
  //                   'o3d.ParamString',
  //                   'o3d.ParamTexture'
  // Returns:
  //  Newly created parameter.
  Param* CreateParamByClassName(const String& param_name,
                                const String& class_type_name);

  // Creates a Param based on the type passed in. This is a type safe version of
  // CreateParam for C++. You give it the name and a pointer to a typed param
  // and it will create a param of that type.
  // Paramaters:
  //  param_name: Name of the param which must be unique from other params on
  //              this object.
  // Returns:
  //  pointer to new param if successful.
  template<typename T>
  T* CreateParam(const String& param_name) {
    return down_cast<T*>(CreateParamByClass(param_name,
                                            T::GetApparentClass()));
  }

  // Creates a Param based on the type passed in. This is a type safe version of
  // CreateParam for C++. You give it the name and a pointer to a typed param
  // and it will create a param of that type.
  // Paramaters:
  //  param_name: Name of the param which must be unique from other params on
  //              this object.
  //  typed_param_pointer_pointer: Pointer to a pointer to TypedParam
  //                               (eg,  ParamFloat, ParamFloat4) which will be
  //                               set by this function. Will be NULL on
  //                               failure.
  // Returns:
  //  True if successful.
  template<typename T>
  bool CreateParamPointer(const String& param_name,
                          T** typed_param_pointer_pointer) {
    *typed_param_pointer_pointer = CreateParam<T>(param_name);
    return *typed_param_pointer_pointer != NULL;
  }

  // Gets a Param in a typesafe way
  //  param_name: Name of the param.
  // Returns:
  //  Pointer to typed param if successful, NULL if failure.
  template<typename T>
  T* GetParam(const String& param_name) const {
    Param* param = GetUntypedParam(param_name);
    return (param && param->GetClass() == T::GetApparentClass()) ?
        down_cast<T*>(param) : NULL;
  }

  // Gets a param by name, or creates it if it doesn't exist.
  // Returns the param, or NULL if it doesn't match the requested type
  Param* GetOrCreateParamByClass(const String& param_name,
                                 const ObjectBase::Class* param_type);

  // Gets a param by name, or creates it if it doesn't exist.
  // Returns the param, or NULL if it doesn't match the requested type
  template <typename T>
  T* GetOrCreateParam(const String& param_name) {
    return down_cast<T*>(GetOrCreateParamByClass(param_name,
                                                 T::GetApparentClass()));
  }

  // Searches by name for a Param defined in the object. Prefer GetParam where
  // possible.
  // Parameter:
  //   param_name: name of param to get.
  Param* GetUntypedParam(const String& param_name) const;

  // Adds a newly created param to the local Param map. Fails if a param by the
  // same name already exists.
  // Parameters:
  //   param_name: Name to assign Param
  //   param: Param to add.
  // Returns:
  //   true if successful.
  bool AddParam(const String& param_name, Param* param);

  // Removes the given Param from the object.
  // Parameters:
  //   param: Param to remove.
  // Returns:
  //   true if successful.
  bool RemoveParam(Param* param);

  // Determines whether a param was added to an object after it was originally
  // constructed.
  bool IsAddedParam(Param* param);

  // Called before a Param is added. You can override this method and return
  // false if you want to prevent the add from succeeding. Be sure to call the
  // base and return it's result to give it a chance to refuse addition.
  // Parameter:
  //   param: Param about to be added.
  // Return:
  //   true if okay to add Param.
  virtual bool OnBeforeAddParam(Param* param) {
    return true;
  }

  // Called after a Param has been added. Override if you need to know when a
  // Param is added. Be sure to call the base.
  // Parameter:
  //   param: Param that was just added.
  virtual void OnAfterAddParam(Param* param) { }

  // Called before a Param is about to be removed. You can override this
  // function and return false if you want to prevent a Param from being
  // removed. Be sure to call the base and return it's result to give it a
  // chance to refuse removal.
  // Parameter:
  //   param: Param about to be removed.
  // Return:
  //   true if okay to remove Param.
  virtual bool OnBeforeRemoveParam(Param* param) {
    return true;
  }

  // Called after a Param has been removed. Override if you need to know when a
  // Param has been removed. Be sure to call the base.
  // Parameter:
  //   param: Param that was just added.
  virtual void OnAfterRemoveParam(Param* param) { }

  // Copies all the params from a the given source_param_object does not replace
  // any currently existing params with the same name.
  void CopyParams(ParamObject* source_param_object);

  // Return the parameter map for this object.
  const NamedParamRefMap& params() const { return params_; }

  // Gets all the params.
  // Paramaters:
  //  param_array: ParamVector to fill in with params. Will be cleared.
  void GetParamsFast(ParamVector* param_array) const;

  // Gets all params.
  //
  // This is a slower version for javascript. If you are using C++ prefer the
  // version above. This one creates the array on the stack and passes it back
  // by value which means the entire array will get copied 2 twice and in making
  // the copies memory is allocated and deallocated twice.
  ParamVector GetParams() const;

  // This function is for internal use only. It is only called by
  // Param::AddInputsRecursive and therefore the Param will always be
  // owned by this ParamObject.
  //
  // For the given Param, returns all the inputs that affect that param through
  // this ParamObject. For example, given a Transform's worldMatrix this would
  // return both this Transform's localMatrix and its parent's worldMatrix.
  //
  // Parameters:
  //   param: Param to get inputs for. Note that param MUST be a Param on this
  //       ParamObject.
  //   inputs: ParamVector to fill out with array of inputs. The array will be
  //       cleared.
  void GetInputsForParam(const Param* param,
                         ParamVector* inputs) const;

  // This function is for internal use only. It is only called by
  // Param::AddOutputsRecursive and therefore the Param will always be owned by
  // this ParamObject.
  //
  // For the given Param, returns all the outputs that the given param will
  // affect through this ParamObject. For example, given a Transform's
  // localMatrix it will only return this Transform's worldMatrix. For a
  // transform's worldMatrix it will return all its chilren's worldMatrices.
  //
  // Parameters:
  //   param: Param to get outputs for. Note that param MUST be a Param on this
  //       ParamObject.
  //   outputs: ParamVector to fill out with array of outputs. The array will be
  //       cleared.
  void GetOutputsForParam(const Param* param,
                          ParamVector* outputs) const;

  // Registers a pointer to a reference to a parameter. If the parameter
  // does not exist it will be created. The reference will be set to refer
  // to the parameter.
  // Parameters:
  //   param_name: name of parameter
  //   typed_param_ref_pointer: address of param ref
  template<typename T>
  void RegisterParamRef(const String& param_name,
                        T* typed_param_ref_pointer) {
    typename T::Pointer param =
        GetOrCreateParam<typename T::ClassType>(param_name);
    LOG_ASSERT(param);
    *typed_param_ref_pointer = T(param);
    param_ref_helper_map_.insert(
        std::make_pair(param_name,
                       new ParamRefHelper<T>(typed_param_ref_pointer)));
  }

  // Registers a pointer to a reference to a read only parameter. If the
  // parameter does not exist it will be created. The reference will be set to
  // refer to the parameter.
  // Parameters:
  //   param_name: name of parameter
  //   typed_param_ref_pointer: address of param ref
  template<typename T>
  void RegisterReadOnlyParamRef(const String& param_name,
                                T* typed_param_ref_pointer) {
    RegisterParamRef<T>(param_name, typed_param_ref_pointer);
    (*typed_param_ref_pointer)->MarkAsReadOnly();
  }

 protected:
  // This is the concrete version of GetInputsForParam for a specific class. See
  // GetInputsForParam for details. You should override this function in your
  // derived class. You do not need to clear the inputs nor do you need to check
  // that Param is valid. That is handled in GetInputsForParam.
  //
  // Parameters:
  //   param: Param to get inputs for.
  //   inputs: ParamVector to append inputs.
  virtual void ConcreteGetInputsForParam(const Param* param,
                                         ParamVector* inputs) const;

  // This is the concrete version of GetOutputsForParam for a specific class.
  // See GetOutputsForParam for details. You should override this function in
  // your derived class. You do not need to clear the outputs nor do you need to
  // check that Param is valid. That is handled in GetOutputsForParam.
  //
  // Parameters:
  //   param: Param to get inputs for.
  //   outputs: ParamVector to append outputs.
  virtual void ConcreteGetOutputsForParam(const Param* param,
                                          ParamVector* outputs) const;

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // This class enables us to put multiple types of Params refs in the same
  // map.
  class ParamRefHelperBase {
   public:
    virtual ~ParamRefHelperBase() { }  // Because we are a base class.

    // Gets overridden for each typedparam.
    virtual void UpdateParamRef(Param* param) = 0;

    // Returns whether this is a helper for the given Param.
    virtual bool IsParam(Param* param) const = 0;
  };

  // Template to generate concrete ParamRefHelper.
  // Parameters:
  //   typed_param_ref_pointer: Pointer to reference to a param.
  template<typename T>
  class ParamRefHelper : public ParamRefHelperBase {
   public:
    explicit ParamRefHelper(T* typed_param_ref_pointer)
        : typed_param_ref_pointer_(typed_param_ref_pointer) {
      LOG_ASSERT(typed_param_ref_pointer);
    }

    // Attempt to update the ref to typed param to point to a new param. If the
    // param is the correct type the ref will be updated. If it is not the
    // correct type the ref will remain pointing to the old param.
    // Parameters:
    //   param: Param to update references for.
    virtual void UpdateParamRef(Param* param) {
      if (param && param->IsA(T::ClassType::GetApparentClass())) {
        *typed_param_ref_pointer_ = T(down_cast<typename T::Pointer>(param));
      }
    }

    // Returns whether this is a helper for the given Param.
    virtual bool IsParam(Param* param) const {
      return param == *typed_param_ref_pointer_;
    }

   private:
    // Pointer to pointer to typed param.
    T* typed_param_ref_pointer_;
  };

  typedef std::multimap<String, ParamRefHelperBase*>
      ParamRefHelperMultiMap;
  typedef ParamRefHelperMultiMap::iterator ParamRefHelperMultiMapIterator;
  typedef std::pair<ParamRefHelperMultiMapIterator,
                    ParamRefHelperMultiMapIterator> ParamRefHelperMultiMapRange;

  IClassManager* class_manager_;

  // Map of all the typed paramater pointers we are managing.
  ParamRefHelperMultiMap  param_ref_helper_map_;

  NamedParamRefMap params_;  // Stores Params defined on the ParamObject.

  // This is incremented every time a param is added or removed so other classes
  // can know when they need to update any pointers to params on this object
  // they may be holding.
  int change_count_;

  O3D_DECL_CLASS(ParamObject, NamedObject);
  DISALLOW_COPY_AND_ASSIGN(ParamObject);
};

// This template helps create Params whose values are set by the class that owns
// them. For example Transform has a param, "worldMatrix", that is set by the
// transform. When the Param's value is asked for it will call its master's
// UpdateOutputs() function which it expects will update its value.
template <typename ParamType, typename MasterType>
class SlaveParam : public ParamType {
 public:
  // Overriden from Param.
  virtual void ComputeValue() {
    // TODO: Check here if we really need to call UpdateOutput.
    //     The issue here is in param_object_test.cc is an example of a
    //     ParamObject that has 2 outputs. Currently asking for the value
    //     of either output will end up triggering a call to UpdateOutputs.
    //     What we want to happen is if one or the other is called that
    //     keep some kind of record in the ParamObject so that the next
    //     call to UpdateOuputs is not called or is marked as invalid or
    //     something like that.  But, Currently, except for that example in
    //     param_object_test.cc there are no ParamObjects that have more than 1
    //     output.
    if (ParamType::input_connection()) {
      ParamType::ComputeValue();
    } else {
      master_->UpdateOutputs();
    }
  }

  static void RegisterParamRef(const String& param_name,
                               typename ParamType::Ref* typed_param_ref_pointer,
                               MasterType* master) {
    typename ParamType::Ref param = typename ParamType::Ref(new SlaveParam(
        master->service_locator(), master));
    master->AddParam(param_name, param);
    master->RegisterParamRef(param_name, typed_param_ref_pointer);
  }
 private:
  SlaveParam(ServiceLocator* service_locator, MasterType* master)
      : ParamType(service_locator, true, false),
        master_(master) {
  }

  MasterType* master_;
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_PARAM_OBJECT_H_

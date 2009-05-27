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


// Contains declarations for Param and TypedParam classes.

#ifndef O3D_CORE_CROSS_PARAM_H_
#define O3D_CORE_CROSS_PARAM_H_

#include <vector>
#include "base/scoped_ptr.h"
#include "core/cross/named_object.h"
#include "core/cross/types.h"
#include "core/cross/weak_ptr.h"
#include "core/cross/service_locator.h"
#include "core/cross/evaluation_counter.h"

namespace o3d {

class Param;
class ParamObject;
class ServiceLocator;

#define PARAM_VALUE_CONST

// Type used to store and return an array of Param pointers.
typedef std::vector<Param*> ParamVector;

// Iterators for ParamVector.
typedef ParamVector::const_iterator ParamVectorConstIterator;
typedef ParamVector::iterator ParamVectorIterator;

// Param elements store user defined name/value pairs on nodes
// and other O3D objects.  Each element has a name, a type and a value
// that can be set and queried.  One of their uses is to hold
// "uniform constants" used to parameterize shaders.  Param elements
// can be connected in a source/destination fashion such that the target Param
// gets its value from the source param.
class Param : public NamedObjectBase {
  friend class IClassManager;
 public:
  typedef SmartPointer<Param> Ref;

  virtual ~Param();

  // Copies data from another Param.
  virtual void CopyDataFromParam(Param* source_param) = 0;

  // Gets the name of the Param.
  virtual const String& name() const;

  // Gets the parameter handle (opaque).
  const void* handle() const {
    return handle_;
  }

  // Sets the parameter handle (opaque).
  void set_handle(const void* handle) {
    handle_ = handle;
  }

  // Gets whether or not this param is read only
  inline bool read_only() const {
    return read_only_;
  }

  // Gets whether or not this param is dyamically updated (not by bind but by
  // the Param itself. The SAS params are dynamic for example.)
  inline bool dynamic() const {
    return dynamic_;
  }

  // The only point of this is to allow the user to make cycles in param chains
  // predictable. If param_A gets its value from param_B and param_B gets its
  // value from param_A the last_evaluation_count_ will prevent an infinite
  // cycle BUT there is no way to specify who gets evaluated first.
  // If you go param_A->value(), param_B will evaluate then copy to param_A.
  // If you go param_B->value(), param_A will evaluate then copy to param_B.
  // If you set param_B->set_update_input(false), then:
  // If you go param_A->value(), param_B will evaluate then copy to param_A.
  // If you go param_B->value(), param_B just copy param_A. param_A will NOT
  // evaluate when param_B asks for its value.
  bool update_input() const {
    return update_input_;
  }

  void set_update_input(bool value) {
    update_input_ = value;
  }

  inline bool cachable() const {
    return not_cachable_count_ == 0;
  }

  // Returns an input Param connection to this element or NULL if there is none.
  Param* input_connection() const {
    return input_connection_;
  }

  // Returns a reference to the internal array of output references.
  const ParamVector& output_connections() {
    return output_connections_;
  }

  // Gets ALL the params that affect this Param.
  // Parameters:
  //   params: ParamVector to fill out with array of inputs. The array will
  //       be cleared.
  void GetInputs(ParamVector* params) const;

  // Returns ALL the params this Param affects.
  // Parameters:
  //   params: ParamVector to fill out with array of outputs. The array will
  //       be cleared.
  void GetOutputs(ParamVector* params) const;

  // Directly binds two Param elements such that the this parameter gets its
  // value from the source parameter.  The source parameters
  // must be a compatible type to this param or NULL to unbind. Note: The
  // routine will fail if the bind would introduce a cycle in the Param graph.
  // Parameters:
  //  source_param: The parameter that the value originates from or NULL
  // Returns:
  //  True if the bind succeeds
  bool Bind(Param *source_param);

  // Breaks any input connection coming into this Param
  void UnbindInput();

  // Breaks a specific param-bind output connection on this param.
  // Parameters:
  //   destination_param: param to unbind.
  // Returns:
  //   True if the param was a destination param and was unbound.
  bool UnbindOutput(Param* destination_param);

  // Breaks all param-bind output connections on this param.
  void UnbindOutputs();

  // If input is not cachable will increment the not cachable count for this
  // param and its outputs.
  //
  // Note: This function must be called by classes derived from ParamObject when
  // implicit connections between parameters are changed such that a new
  // relationship is established. For example when one Transform is parented to
  // another there is an implicit relationship between parent.worldMatrix and
  // child.worldMatrix.
  //
  // Parameters:
  //   input: Param that is about to be made an input to this param.
  void IncrementNotCachableCountOnParamChainForInput(Param* input);

  // If input is not cachable will decrement the not cachable count for this
  // param and its outputs.
  //
  // Note: This function must be called by classes derived from ParamObject when
  // implicit connections between parameters are changed such that an old
  // relationship is broken. For example when one Transform is un-parented from
  // another there is an implicit relationship between parent.worldMatrix and
  // child.worldMatrix that is being broken.
  //
  // Parameters:
  //   input: Param that was an input to this param but is no longer.
  void DecrementNotCachableCountOnParamChainForInput(Param* input);

  // Safely gets a TypedParam from a param.
  // Paramaters:
  //  param: Pointer to base param you want down_cast to your TypedParam.
  // Returns:
  //  Pointer to TypedParam if success, NULL if failure.
  template <typename T>
  T* GetParam() {
    return ObjectBase::rtti_dynamic_cast<T>(this);
  }

  // Safely gets a TypedParam from a param.
  // The safety comes for the fact that you don't give it a type it derives the
  // type so you can't accidentally give it the wrong type.
  // Paramaters:
  //  typed_param_pointer_pointer: Pointer to a pointer to TypedParam
  //                               (eg,  ParamFloat, ParamFloat4) which will be
  //                               set by this function. Will be NULL on
  //                               failure.
  // Returns:
  //  True if successful.
  template <typename T>
  bool GetParamPointer(T** typed_param_pointer_pointer) {
    *typed_param_pointer_pointer = GetParam<T>();
    return *typed_param_pointer_pointer != NULL;
  }

  // Sets this parameter to be read only. This is an internal function and is
  // currently only used by ParamObject::RegisterReadOnlyParamRef
  void MarkAsReadOnly();

  // Get the ParamObject owning this Param.
  ParamObject* owner() const { return owner_; }

  // Sets this Param's owner. This should really probably be passed in
  // the constructor but that's a HUGE change.
  void set_owner(ParamObject* owner);

  // Sets the name of the param. This is called by ParamObject::AddParam.
  // The name can only be set once.
  void SetName(const String& name);

 protected:
  Param(ServiceLocator* service_locator, bool dynamic, bool read_only);

  // Compute or update a value from an input connection.
  // This method may be is overridden in derived classes to compute a new value.
  // The default implementation gets its value from an input connection if one
  // exists.
  virtual void ComputeValue();

  // Sets the client's error to an error message about read only. The sole
  // purpose of this function is so that client does not have to be defined
  // in param.h.
  void ReportReadOnlyError();

  // Sets the client's error to an error message about trying to set a dynamic
  // param. The sole purpose of this function is so that client does not have to
  // be defined in param.h.
  void ReportDynamicSetError();

  // Makes sure the data_ value is up to date, evaluating the input connection
  // if necessary.
  inline void UpdateValue() {
    if ((dynamic_ || !input_connection_.IsNull()) && !IsValid()) {
      ComputeValue();
      Validate();
    }
  }

  // Makes sure this param re-evaluates its value
  inline void Invalidate() {
    last_evaluation_count_ = evaluation_counter_->evaluation_count() - 1;
  }

  // Marks the param as valid so that it will not update its value.
  inline void Validate() {
    last_evaluation_count_ = evaluation_counter_->evaluation_count();
  }

  // Returns true if the param is valid.
  inline bool IsValid() {
    return last_evaluation_count_ == evaluation_counter_->evaluation_count() &&
        not_cachable_count_ == 0;
  }

  // This is a glue function for Client::InvalidateAllParameters so we
  // don't have to define client in this file.
  void InvalidateAllParameters();

  // Invalidates all the params that depend on this param
  void InvalidateAllOutputs() const;

  // Invalidates all the params that we depend. (also Invalidates ourself)
  void InvalidateAllInputs();

  // Marks this param non-cachable. Used during initialization only to mark a
  // param as not cachable.
  void SetNotCachable();

  // Called after a Param is bound to another. You can override this in a
  // derived class.
  virtual void OnAfterBindInput() { }

  // Called after a Param is unbound from another. You can override this in a
  // derived class.
  // Parameters:
  //   old_source: The Param that used to be bound.
  virtual void OnAfterUnbindInput(Param* old_source) { }

 private:
  // Adds ALL the params that affect this Param to the ParamVector.
  // Parameters:
  //   original: the original param we started from. We'll stop if we see this
  //       again since that's a cycle.
  //   params: ParamVector to add inputs Params to
  void AddInputsRecursive(const Param* original, ParamVector* params) const;

  // Returns ALL the params this Param affects to the ParamVector.
  // Parameters:
  //   original: the original param we started from. We'll stop if we see this
  //       again since that's a cycle.
  //   params: ParamVector to add outputs Params to
  void AddOutputsRecursive(const Param* original, ParamVector* params) const;

  // Adds a Param to the list of Params using this Param as source.
  void AddOutputConnection(Param* param);

  // Removes any existing input connection from the Param.
  void ResetInputConnection();

  void IncrementNotCachableCountForThisParamAndAllItsOuputs();
  void DecrementNotCachableCountForThisParamAndAllItsOuputs();

  // Array of references to Params.
  typedef std::vector<Param::Ref> ParamRefArray;

  // Name of the Param.
  String name_;

  EvaluationCounter* evaluation_counter_;

  // Pointer to an input connection, if one exists.
  Param::Ref input_connection_;

  // List of output connections.
  ParamVector output_connections_;

  // Value is cachable. If this is not zero we must call UpdateValue. Cachable
  // is zero by default execpt for Standard Params. Anytime a Param is bound the
  // Param Chain is checked and this flag will be cleared if any input in that
  // chain is not cachable. When the param is unbound the flag will be set
  // again.
  int not_cachable_count_;

  // Flag that value is dynamic and therefore ComputeValue needs to be called
  bool dynamic_;

  // Flag that value is read only so set_value and bind should fail
  bool read_only_;

  // Handle to an implementation specific object corresponding to the Param.
  const void *handle_;

  // ParamObject we are owned by.
  ParamObject* owner_;

  // last evaluation count. If this value doesn't match the global count
  // then our value is out of date.
  int last_evaluation_count_;

  // if true we update our input connection before we evaluate. default = true.
  // See set_update_input() for details.
  bool update_input_;

  O3D_DECL_CLASS(Param, NamedObjectBase);
  DISALLOW_COPY_AND_ASSIGN(Param);
};

// A template for value based param types except it does not implement
// CopyDataFromParam.
template<class T>
class TypedParamBase : public Param {
 public:
  typedef T DataType;
  TypedParamBase(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : Param(service_locator, dynamic, read_only) {}
  virtual ~TypedParamBase() {}

  // Sets the value stored in the Param.
  void set_value(const DataType& value) {
    if (!dynamic() && input_connection() == NULL) {
      // This check is not good enough because for example, setting localMatrix
      // affects worldMatrix but that relationship is not expressed.
      //
      // TODO: add flag to mark params like local matrix as having output
      //     so we can check that flag here.
      //
      // if (!output_connections_.empty()) {
      //   InvalidateAllParameters();
      // }
      InvalidateAllParameters();
      set_dynamic_value(value);
    } else {
      ReportDynamicSetError();
    }
  }

  // Sets the value stored in the Param without checking if it's dynamic.
  // ParamBinds should use this function.
  void set_dynamic_value(const DataType& value) {
    if (!read_only()) {
      value_ = value;
      Validate();
    } else {
      ReportReadOnlyError();
    }
  }

  // Sets the value for a read only param because there needs to be some way
  // to set it. This is an internal only function.
  void set_read_only_value(const DataType& value) {
    value_ = value;
  }

  // Returns the current value stored in the Param.
  DataType value() PARAM_VALUE_CONST {
    UpdateValue();
    return value_;
  }

 protected:
  // Because gcc complains that I can't set value_ directly from TypedParam
  void set_value_private(DataType value) {
    value_ = value;
  }

  // The value stored in the Param.
  DataType value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TypedParamBase);
};

template<class T>
class TypedParam : public TypedParamBase<T> {
 public:
  typedef T DataType;
  TypedParam(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedParamBase<T>(service_locator, dynamic, read_only) {}
  virtual ~TypedParam() {}

  // Copies the data from another Param.
  virtual void CopyDataFromParam(Param* source_param) {
    set_value_private((down_cast<TypedParam*>(source_param))->value_);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TypedParam);
};

// Abstract base class of all reference params.
class RefParamBase : public Param {
 public:
  RefParamBase(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : Param(service_locator, dynamic, read_only) {}
  virtual ~RefParamBase() {}

  virtual ObjectBase* value() PARAM_VALUE_CONST = 0;

 private:
  O3D_DECL_CLASS(RefParamBase, Param);
  DISALLOW_COPY_AND_ASSIGN(RefParamBase);
};

// Abstract base class for Specialized TypedParam for SmartPtr objects that have
// a GetWeakPointer method. Implements all needed methods except
// CopyDataFromParam.
template<typename T>
class TypedRefParamBase : public RefParamBase {
 public:
  typedef WeakPointer<T> WeakPointerType;

  TypedRefParamBase(ServiceLocator* service_locator,
                    bool dynamic,
                    bool read_only)
      : RefParamBase(service_locator, dynamic, read_only) {}
  virtual ~TypedRefParamBase() {}

  // Set the value stored in the param if it is not dynmaically updated. If the
  // user attempts to set the value and the value happens to be driven by a bind
  // they'll get an error telling them what they just tried is not going to
  // work.
  void set_value(T* const value) {
    if (!dynamic() && input_connection() == NULL) {
      // This check is not good enough because for example, setting localMatrix
      // affects worldMatrix but that relationship is not expressed.
      //
      // TODO: add flag to mark params like local matrix as having output
      //     so we can check that flag here.
      //
      // if (!output_connections_.empty()) {
      //   InvalidateAllParameters();
      // }
      InvalidateAllParameters();
      set_dynamic_value(value);
    } else {
      ReportDynamicSetError();
    }
  }

  // Sets the value stored in the Param without checking if it's dynamic.
  // ParamBinds should use this function.
  void set_dynamic_value(T* const value) {
    if (!read_only()) {
      value_ = value ? value->GetWeakPointer() : WeakPointerType();
      Validate();
    } else {
      ReportReadOnlyError();
    }
  }

  // Sets the value for a read only param because there needs to be some way
  // to set it.
  void set_read_only_value(T* const value) {
    value_ = value ? value->GetWeakPointer() : WeakPointerType();
  }

  // Returns the current value stored in the Param.
  T* value() PARAM_VALUE_CONST {
    UpdateValue();
    return value_.Get();
  }

 protected:
  // Because gcc complains that I can't set value_ directly from TypedRefParam.
  void set_value_private(WeakPointerType value) {
    value_ = value;
  }

  // The value stored in the Param.
  WeakPointerType value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TypedRefParamBase);
};

// Fully specialize TypedParam for SmartPtr objects that have a GetWeakPointer
// method.
template<typename T>
class TypedRefParam : public TypedRefParamBase<T> {
 public:
  typedef WeakPointer<T> WeakPointerType;

  TypedRefParam(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedRefParamBase<T>(service_locator, dynamic, read_only) {
  }
  virtual ~TypedRefParam() {}

  // Copies the data from another Param.
  virtual void CopyDataFromParam(Param* source_param) {
    T* value = down_cast<TypedRefParam*>(source_param)->value_.Get();
    set_value_private(value ? value->GetWeakPointer() : WeakPointerType());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TypedRefParam);
};

// The following are type defines for Param types used by the Param system.
// We use an inheritance model so that these names may be forward declared.
class ParamFloat : public TypedParam<Float> {
 public:
  typedef SmartPointer<ParamFloat> Ref;

 protected:
  ParamFloat(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedParam<Float>(service_locator, dynamic, read_only) {
    set_read_only_value(0.0f);
  }

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamFloat, Param);
  DISALLOW_COPY_AND_ASSIGN(ParamFloat);
};

class ParamFloat2 : public TypedParam<Float2> {
 public:
  typedef SmartPointer<ParamFloat2> Ref;

 protected:
  ParamFloat2(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedParam<Float2>(service_locator, dynamic, read_only) {
    set_read_only_value(Float2(0.0f, 0.0f));
  }

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamFloat2, Param);
  DISALLOW_COPY_AND_ASSIGN(ParamFloat2);
};

class ParamFloat3 : public TypedParam<Float3> {
 public:
  typedef SmartPointer<ParamFloat3> Ref;

 protected:
  ParamFloat3(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedParam<Float3>(service_locator, dynamic, read_only) {
    set_read_only_value(Float3(0.0f, 0.0f, 0.0f));
  }

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamFloat3, Param);
  DISALLOW_COPY_AND_ASSIGN(ParamFloat3);
};

class ParamFloat4 : public TypedParam<Float4> {
 public:
  typedef SmartPointer<ParamFloat4> Ref;

 protected:
  ParamFloat4(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedParam<Float4>(service_locator, dynamic, read_only) {
    set_read_only_value(Float4(0.0f, 0.0f, 0.0f, 0.0f));
  }

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamFloat4, Param);
  DISALLOW_COPY_AND_ASSIGN(ParamFloat4);
};

class ParamInteger : public TypedParam<int> {
 public:
  typedef SmartPointer<ParamInteger> Ref;

 protected:
  ParamInteger(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedParam<int>(service_locator, dynamic, read_only) {
    set_read_only_value(0);
  }

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamInteger, Param);
  DISALLOW_COPY_AND_ASSIGN(ParamInteger);
};

class ParamBoolean : public TypedParam<bool> {
 public:
  typedef SmartPointer<ParamBoolean> Ref;

 protected:
  ParamBoolean(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedParam<bool>(service_locator, dynamic, read_only) {
    set_read_only_value(false);
  }

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamBoolean, Param);
  DISALLOW_COPY_AND_ASSIGN(ParamBoolean);
};

class ParamString : public TypedParam<String> {
 public:
  typedef SmartPointer<ParamString> Ref;

 protected:
  ParamString(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedParam<String>(service_locator, dynamic, read_only) {}

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamString, Param);
};

class ParamMatrix4 : public TypedParam<Matrix4> {
 public:
  typedef SmartPointer<ParamMatrix4> Ref;

 protected:
  ParamMatrix4(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedParam<Matrix4>(service_locator, dynamic, read_only) {
    set_read_only_value(Vectormath::Aos::Matrix4::identity());
  }

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamMatrix4, Param);
  DISALLOW_COPY_AND_ASSIGN(ParamMatrix4);
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_PARAM_H_

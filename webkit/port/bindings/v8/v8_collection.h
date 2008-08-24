// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copied from base/basictypes.h with some modifications

#ifndef V8_PROPERTY_H__
#define V8_PROPERTY_H__

#include <v8.h>
#include "v8_proxy.h"

namespace WebCore {


// Returns named property of a collection.
template <class C>
static v8::Handle<v8::Value> GetNamedPropertyOfCollection(
    v8::Local<v8::String> name,
    v8::Local<v8::Object> object,
    v8::Local<v8::Value> data) {
  // TODO: assert object is a collection type
  ASSERT(V8Proxy::MaybeDOMWrapper(object));

  V8ClassIndex::V8WrapperType t = V8Proxy::GetDOMWrapperType(object);
  C* collection = V8Proxy::FastToNativeObject<C>(t, object);
  String prop_name = ToWebCoreString(name);
  void* result = collection->namedItem(prop_name);
  if (!result) return v8::Handle<v8::Value>();
  V8ClassIndex::V8WrapperType type = V8ClassIndex::ToWrapperType(data);
  return V8Proxy::ToV8Object(type, result);
}

// A template of named property accessor of collections.
template <class C>
static v8::Handle<v8::Value> CollectionNamedPropertyGetter(
    v8::Local<v8::String> name, const v8::AccessorInfo& info) {
  return GetNamedPropertyOfCollection<C>(name, info.Holder(), info.Data());
}

// A template returns whether a collection has a named property.
// This function does not cause JS heap allocation.
template <class C>
static bool HasNamedPropertyOfCollection(v8::Local<v8::String> name,
                                         v8::Local<v8::Object> object,
                                         v8::Local<v8::Value> data) {
  // TODO: assert object is a collection type
  ASSERT(V8Proxy::MaybeDOMWrapper(object));

  V8ClassIndex::V8WrapperType t = V8Proxy::GetDOMWrapperType(object);
  C* collection = V8Proxy::FastToNativeObject<C>(t, object);
  String prop_name = ToWebCoreString(name);
  void* result = collection->namedItem(prop_name);
  return result != NULL;
}


// Returns the property at the index of a collection.
template <class C>
static v8::Handle<v8::Value> GetIndexedPropertyOfCollection(
    uint32_t index, v8::Local<v8::Object> object, v8::Local<v8::Value> data) {
  // TODO, assert that object must be a collection type
  ASSERT(V8Proxy::MaybeDOMWrapper(object));
  V8ClassIndex::V8WrapperType t = V8Proxy::GetDOMWrapperType(object);
  C* collection = V8Proxy::FastToNativeObject<C>(t, object);
  void* result = collection->item(index);
  if (!result) return v8::Handle<v8::Value>();
  V8ClassIndex::V8WrapperType type = V8ClassIndex::ToWrapperType(data);
  return V8Proxy::ToV8Object(type, result);
}


// A template of index interceptor of collections.
template <class C>
static v8::Handle<v8::Value> CollectionIndexedPropertyGetter(
    uint32_t index, const v8::AccessorInfo& info) {
  return GetIndexedPropertyOfCollection<C>(index, info.Holder(), info.Data());
}


// Get an array containing the names of indexed properties in a collection.
template <class C>
static v8::Handle<v8::Array> CollectionIndexedPropertyEnumerator(
    const v8::AccessorInfo& info) {
  ASSERT(V8Proxy::MaybeDOMWrapper(info.Holder()));
  V8ClassIndex::V8WrapperType t = V8Proxy::GetDOMWrapperType(info.Holder());
  C* collection = V8Proxy::FastToNativeObject<C>(t, info.Holder());
  int length = collection->length();
  v8::Handle<v8::Array> properties = v8::Array::New(length);
  for (int i = 0; i < length; i++) {
    // TODO(ager): Do we need to check that the item function returns
    // a non-null value for this index?
    v8::Handle<v8::Integer> integer = v8::Integer::New(i);
    properties->Set(integer, integer);
  }
  return properties;
}


// Returns whether a collection has a property at a given index.
// This function does not cause JS heap allocation.
template <class C>
static bool HasIndexedPropertyOfCollection(uint32_t index,
                                           v8::Local<v8::Object> object,
                                           v8::Local<v8::Value> data) {
  // TODO, assert that object must be a collection type
  ASSERT(V8Proxy::MaybeDOMWrapper(object));
  V8ClassIndex::V8WrapperType t = V8Proxy::GetDOMWrapperType(object);
  C* collection = V8Proxy::FastToNativeObject<C>(t, object);
  void* result = collection->item(index);
  return result != NULL;
}


// A template for indexed getters on collections of strings that should return
// null if the resulting string is a null string.
template <class C>
static v8::Handle<v8::Value> CollectionStringOrNullIndexedPropertyGetter(
    uint32_t index, const v8::AccessorInfo& info) {
  // TODO, assert that object must be a collection type
  ASSERT(V8Proxy::MaybeDOMWrapper(info.Holder()));
  V8ClassIndex::V8WrapperType t = V8Proxy::GetDOMWrapperType(info.Holder());
  C* collection = V8Proxy::FastToNativeObject<C>(t, info.Holder());
  String result = collection->item(index);
  return v8StringOrNull(result);
}


// Add indexed getter to the function template for a collection.
template <class T>
static void SetCollectionIndexedGetter(v8::Handle<v8::FunctionTemplate> desc,
                                       V8ClassIndex::V8WrapperType type) {
  desc->InstanceTemplate()->SetIndexedPropertyHandler(
      CollectionIndexedPropertyGetter<T>,
      0,
      0,
      0,
      CollectionIndexedPropertyEnumerator<T>,
      v8::External::New(reinterpret_cast<void*>(type)));
}


// Add named getter to the function template for a collection.
template <class T>
static void SetCollectionNamedGetter(v8::Handle<v8::FunctionTemplate> desc,
                                     V8ClassIndex::V8WrapperType type) {
  desc->InstanceTemplate()->SetNamedPropertyHandler(
      CollectionNamedPropertyGetter<T>,
      0,
      0,
      0,
      0,
      v8::External::New(reinterpret_cast<void*>(type)));
}


// Add named and indexed getters to the function template for a collection.
template <class T>
static void SetCollectionIndexedAndNamedGetters(
    v8::Handle<v8::FunctionTemplate> desc, V8ClassIndex::V8WrapperType type) {
  // If we interceptor before object, accessing 'length' can trigger
  // a webkit assertion error.
  // (see fast/dom/HTMLDocument/document-special-properties.html
  desc->InstanceTemplate()->SetNamedPropertyHandler(
      CollectionNamedPropertyGetter<T>,
      0,
      0,
      0,
      0,
      v8::External::New(reinterpret_cast<void*>(type)));
  desc->InstanceTemplate()->SetIndexedPropertyHandler(
      CollectionIndexedPropertyGetter<T>,
      0,
      0,
      0,
      CollectionIndexedPropertyEnumerator<T>,
      v8::External::New(reinterpret_cast<void*>(type)));
}


// Add indexed getter returning a string or null to a function template
// for a collection.
template <class T>
static void SetCollectionStringOrNullIndexedGetter(
    v8::Handle<v8::FunctionTemplate> desc) {
  desc->InstanceTemplate()->SetIndexedPropertyHandler(
      CollectionStringOrNullIndexedPropertyGetter<T>,
      0,
      0,
      0,
      CollectionIndexedPropertyEnumerator<T>);
}


}  // namespace WebCore

#endif  // V8_PROPERTY_H__


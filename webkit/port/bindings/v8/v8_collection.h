// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copied from base/basictypes.h with some modifications

#ifndef V8_PROPERTY_H__
#define V8_PROPERTY_H__

#include <v8.h>
#include "v8_proxy.h"

namespace WebCore {

static v8::Handle<v8::Value> GetV8Object(
    void * result,
    v8::Local<v8::Value> data) {
  if (!result) return v8::Handle<v8::Value>();
  V8ClassIndex::V8WrapperType type = V8ClassIndex::ToWrapperType(data);
  if (type == V8ClassIndex::NODE)
    return V8Proxy::NodeToV8Object(static_cast<Node*>(result));
  else
    return V8Proxy::ToV8Object(type, result);
}

template <class D>
static v8::Handle<v8::Value> GetV8Object(
    PassRefPtr<D> result,
    v8::Local<v8::Value> data) {
  return GetV8Object(result.get(), data);
}

// Returns named property of a collection.
template <class C, class D>
static v8::Handle<v8::Value> GetNamedPropertyOfCollection(
    v8::Local<v8::String> name,
    v8::Local<v8::Object> object,
    v8::Local<v8::Value> data) {
  // TODO: assert object is a collection type
  ASSERT(V8Proxy::MaybeDOMWrapper(object));
  V8ClassIndex::V8WrapperType t = V8Proxy::GetDOMWrapperType(object);
  ASSERT(t != V8ClassIndex::NODE);
  C* collection = V8Proxy::ToNativeObject<C>(t, object);
  String prop_name = ToWebCoreString(name);
  return GetV8Object<D>(collection->namedItem(prop_name), data);
}

// A template of named property accessor of collections.
template <class C, class D>
static v8::Handle<v8::Value> CollectionNamedPropertyGetter(
    v8::Local<v8::String> name, const v8::AccessorInfo& info) {
  return GetNamedPropertyOfCollection<C, D>(name, info.Holder(), info.Data());
}


// A template of named property accessor of HTMLSelectElement and
// HTMLFormElement.
template <class C>
static v8::Handle<v8::Value> NodeCollectionNamedPropertyGetter(
    v8::Local<v8::String> name, const v8::AccessorInfo& info) {
  ASSERT(V8Proxy::MaybeDOMWrapper(info.Holder()));
  ASSERT(V8Proxy::GetDOMWrapperType(info.Holder()) == V8ClassIndex::NODE);
  C* collection = V8Proxy::DOMWrapperToNode<C>(info.Holder());
  String prop_name = ToWebCoreString(name);
  void* result = collection->namedItem(prop_name);
  return GetV8Object(result, info.Data());
}


// Returns the property at the index of a collection.
template <class C, class D>
static v8::Handle<v8::Value> GetIndexedPropertyOfCollection(
    uint32_t index, v8::Local<v8::Object> object, v8::Local<v8::Value> data) {
  // TODO, assert that object must be a collection type
  ASSERT(V8Proxy::MaybeDOMWrapper(object));
  V8ClassIndex::V8WrapperType t = V8Proxy::GetDOMWrapperType(object);
  ASSERT(t != V8ClassIndex::NODE);
  C* collection = V8Proxy::ToNativeObject<C>(t, object);
  return GetV8Object<D>(collection->item(index), data);
}


// A template of index interceptor of collections.
template <class C, class D>
static v8::Handle<v8::Value> CollectionIndexedPropertyGetter(
    uint32_t index, const v8::AccessorInfo& info) {
  return GetIndexedPropertyOfCollection<C, D>(index, info.Holder(), info.Data());
}


// A template of index interceptor of HTMLSelectElement and HTMLFormElement.
template <class C>
static v8::Handle<v8::Value> NodeCollectionIndexedPropertyGetter(
    uint32_t index, const v8::AccessorInfo& info) {
  ASSERT(V8Proxy::MaybeDOMWrapper(info.Holder()));
  ASSERT(V8Proxy::GetDOMWrapperType(info.Holder()) == V8ClassIndex::NODE);
  C* collection = V8Proxy::DOMWrapperToNode<C>(info.Holder());
  void* result = collection->item(index);
  return GetV8Object(result, info.Data());
}


// Get an array containing the names of indexed properties of
// HTMLSelectElement and HTMLFormElement.
template <class C>
static v8::Handle<v8::Array> NodeCollectionIndexedPropertyEnumerator(
    const v8::AccessorInfo& info) {
  ASSERT(V8Proxy::MaybeDOMWrapper(info.Holder()));
  ASSERT(V8Proxy::GetDOMWrapperType(info.Holder()) == V8ClassIndex::NODE);
  C* collection = V8Proxy::DOMWrapperToNode<C>(info.Holder());
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

// Get an array containing the names of indexed properties in a collection.
template <class C>
static v8::Handle<v8::Array> CollectionIndexedPropertyEnumerator(
    const v8::AccessorInfo& info) {
  ASSERT(V8Proxy::MaybeDOMWrapper(info.Holder()));
  V8ClassIndex::V8WrapperType t = V8Proxy::GetDOMWrapperType(info.Holder());
  C* collection = V8Proxy::ToNativeObject<C>(t, info.Holder());
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


// A template for indexed getters on collections of strings that should return
// null if the resulting string is a null string.
template <class C>
static v8::Handle<v8::Value> CollectionStringOrNullIndexedPropertyGetter(
    uint32_t index, const v8::AccessorInfo& info) {
  // TODO, assert that object must be a collection type
  ASSERT(V8Proxy::MaybeDOMWrapper(info.Holder()));
  V8ClassIndex::V8WrapperType t = V8Proxy::GetDOMWrapperType(info.Holder());
  C* collection = V8Proxy::ToNativeObject<C>(t, info.Holder());
  String result = collection->item(index);
  return v8StringOrNull(result);
}


// Add indexed getter to the function template for a collection.
template <class T, class D>
static void SetCollectionIndexedGetter(v8::Handle<v8::FunctionTemplate> desc,
                                       V8ClassIndex::V8WrapperType type) {
  desc->InstanceTemplate()->SetIndexedPropertyHandler(
      CollectionIndexedPropertyGetter<T, D>,
      0,
      0,
      0,
      CollectionIndexedPropertyEnumerator<T>,
      v8::External::New(reinterpret_cast<void*>(type)));
}


// Add named getter to the function template for a collection.
template <class T, class D>
static void SetCollectionNamedGetter(v8::Handle<v8::FunctionTemplate> desc,
                                     V8ClassIndex::V8WrapperType type) {
  desc->InstanceTemplate()->SetNamedPropertyHandler(
      CollectionNamedPropertyGetter<T, D>,
      0,
      0,
      0,
      0,
      v8::External::New(reinterpret_cast<void*>(type)));
}


// Add named and indexed getters to the function template for a collection.
template <class T, class D>
static void SetCollectionIndexedAndNamedGetters(
    v8::Handle<v8::FunctionTemplate> desc, V8ClassIndex::V8WrapperType type) {
  // If we interceptor before object, accessing 'length' can trigger
  // a webkit assertion error.
  // (see fast/dom/HTMLDocument/document-special-properties.html
  desc->InstanceTemplate()->SetNamedPropertyHandler(
      CollectionNamedPropertyGetter<T, D>,
      0,
      0,
      0,
      0,
      v8::External::New(reinterpret_cast<void*>(type)));
  desc->InstanceTemplate()->SetIndexedPropertyHandler(
      CollectionIndexedPropertyGetter<T, D>,
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


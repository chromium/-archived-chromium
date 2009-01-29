// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A filter factory handles the creation of filters given a FilterType (i.e.,
// FILTER_AUDIO_DECODER) and a MediaFormat.  Generally a filter factory handles
// creating a single type of filter, with multiple factories combined into a
// FilterFactoryCollection.  We use some template tricks to enforce type-safety
// and eliminate casting for callers.
//
// The majority of applications will only use FilterFactoryCollection since
// filter factory classes can be created from templates:
//   FilterFactoryCollection filter_factory = new FilterFactoryCollection();
//   filter_factory->AddFactory(new TypeFilterFactory<YourAudioDecoder>());
//   filter_factory->AddFactory(new TypeFilterFactory<YourAudioRenderer>());
//   etc...
//   AudioDecoder* filter;
//   if (filter_factory->Create<AudioDecoder>(media_format, &filter) {
//     do stuff with the filter...
//   }
//
// The only requirement is that your filter implementation provide a static
// Create method with the following signature:
//
//   Returns true and assigns |filter_out| if the filter was created, false
//   and assigns NULL otherwise.
//   static bool Create(MediaFormat* media_format, YourFilterType** filter_out);
//

#ifndef MEDIA_BASE_FACTORY_H_
#define MEDIA_BASE_FACTORY_H_

#include <vector>

#include "base/ref_counted.h"
#include "media/base/filters.h"

namespace media {

class FilterFactoryCollection;

class FilterFactory : public base::RefCountedThreadSafe<FilterFactory> {
 public:
  // Creates a filter implementing the specified interface.  Hides the casting
  // and FilterType constants from the callers and produces cleaner code:
  //   socped_refptr<MyAudioDecoder> d = Create<MyAudioDecoder>(media_format);
  // If the factory does not support the specific filter type or does not
  // support the |media_format| then NULL is returned.
  template <class Filter>
  Filter* Create(const MediaFormat* media_format) {
    return reinterpret_cast<Filter*>(Create(Filter::filter_type(),
                                            media_format));
  }

 protected:
  // For accessing the protected version of Create.
  friend class FilterFactoryCollection;

  // Attempt to create a filter of the given type using the information stored
  // in |media_format|.  If successful, the filter is returned.  If the filter
  // cannot be created for any reason, NULL is returned.
  //
  // It is assumed that the MediaFilter interface can be safely cast to the
  // corresponding interface type (i.e., FILTER_AUDIO_DECODER -> AudioDecoder).
  virtual MediaFilter* Create(FilterType filter_type,
                              const MediaFormat* media_format) = 0;

  friend class base::RefCountedThreadSafe<FilterFactory>;
  virtual ~FilterFactory() {}
};


// Maintains a collection of FilterFactories.
class FilterFactoryCollection : public FilterFactory {
 public:
  FilterFactoryCollection() {}

  // Adds a factory to the end of the collection.
  void AddFactory(FilterFactory* factory) {
    factories_.push_back(factory);
  }

 protected:
  // Attempts to create a filter by walking down the list of filter factories.
  MediaFilter* Create(FilterType filter_type, const MediaFormat* media_format) {
    MediaFilter* filter = NULL;
    for (FactoryVector::iterator factory = factories_.begin();
         !filter && factory != factories_.end();
         ++factory) {
      filter = (*factory)->Create(filter_type, media_format);
    }
    return filter;
  }

 private:
  typedef std::vector< scoped_refptr<FilterFactory> > FactoryVector;
  FactoryVector factories_;

  DISALLOW_COPY_AND_ASSIGN(FilterFactoryCollection);
};

//-----------------------------------------------------------------------------

// This template is used by classes to implement a type-safe filter factory.
// If the derived class needs to examine the |media_format| passed to the
// Create method then they should implement the static method
// IsMediaFormatSupported.  Classes should implement their contructor as private
// and make FilterFactoryImpl<MyClass> a friend class.
template <class Filter>
class FilterFactoryImpl0 : public FilterFactory {
 public:
  FilterFactoryImpl0() {}

 protected:
  virtual MediaFilter* Create(FilterType filter_type,
                              const MediaFormat* media_format) {
    Filter* filter = NULL;
    if (Filter::filter_type() == filter_type &&
        Filter::IsMediaFormatSupported(media_format)) {
      filter = new Filter();
    }
    return filter;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FilterFactoryImpl0);
};

// This template can be used by classes that need to be constructed with a
// parameter that needs to be used in the construction of the actual filter.
// This would usually be a "parent" object which the instantiated filter needs
// to communicate with.  The class's CreateFactory method would look like:
//   static FilterFactory* CreateFactory(MyRequiredParentClass* parent) {
//      return new FilterFactoryImpl1<MyClass>(parent);
//   }
// The class would be constructed with the same pointer passed to the
// CreateFactory method.
template <class Filter, class A>
class FilterFactoryImpl1 : public FilterFactory {
 public:
  explicit FilterFactoryImpl1(A a) : a_(a) {}

 protected:
  virtual MediaFilter* Create(FilterType filter_type,
                              const MediaFormat* media_format) {
    Filter* filter = NULL;
    if (Filter::filter_type() == filter_type &&
        Filter::IsMediaFormatSupported(media_format)) {
      filter = new Filter(a_);
    }
    return filter;
  }

 private:
  A const a_;
  DISALLOW_COPY_AND_ASSIGN(FilterFactoryImpl1);
};

}  // namespace media

#endif  // MEDIA_BASE_FACTORY_H_

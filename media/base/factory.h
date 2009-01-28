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
  //   AudioDecoder* filter = NULL;
  //   bool success = Create<AudioDecoder>(media_format, &filter);
  template <class T>
  bool Create(const MediaFormat* media_format, T** filter_out) {
    return Create(T::filter_type(), media_format,
                  reinterpret_cast<MediaFilter**>(filter_out));
  }

 protected:
  // For accessing the protected version of Create.
  friend class FilterFactoryCollection;

  // Attempt to create a filter of the given type using the information stored
  // in |media_format|.  If successful, the filter is assigned to |filter_out|
  // and the method returns true.  If the filter cannot be created for any
  // reason, |filter_out| is assigned NULL and false it returned.
  //
  // It is assumed that |filter_out| can be safely casted to the corresponding
  // interface type (i.e., FILTER_AUDIO_DECODER -> AudioDecoder).
  virtual bool Create(FilterType filter_type, const MediaFormat* media_format,
                      MediaFilter** filter_out) = 0;

  friend class base::RefCountedThreadSafe<FilterFactory>;
  virtual ~FilterFactory() {}
};


// Helper template class for implementing trivial filter factories.  If your
// filter does not require any special handling during creation, you can create
// a factory for it using this class.  It requires the following static method:
//   bool Create(MediaFormat* media_format, YourFilterType** filter_out)
//
// You can create the filter factory like so:
//   new TypeFilterFactory<YourFilterType>()
template <class Filter>
class TypeFilterFactory : public FilterFactory {
 public:
  TypeFilterFactory() {}

 protected:
  // Attempts to create a filter of the template type.  Assumes a static method
  // Create is declared.
  virtual bool Create(FilterType filter_type, const MediaFormat* media_format,
                      MediaFilter** filter_out) {
    Filter* filter;
    if (Filter::filter_type() == filter_type &&
        Filter::Create(media_format, &filter)) {
      *filter_out = filter;
      return true;
    }
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TypeFilterFactory);
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
  bool Create(FilterType filter_type, const MediaFormat* media_format,
              MediaFilter** filter_out) {
    for (FactoryVector::iterator factory = factories_.begin();
         factory != factories_.end();
         ++factory) {
      if ((*factory)->Create(filter_type, media_format, filter_out)) {
        return true;
      }
    }
    return false;
  }

 private:
  typedef std::vector< scoped_refptr<FilterFactory> > FactoryVector;
  FactoryVector factories_;

  DISALLOW_COPY_AND_ASSIGN(FilterFactoryCollection);
};

}  // namespace media

#endif  // MEDIA_BASE_FACTORY_H_

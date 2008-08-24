// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OBSERVER_LIST_H__
#define BASE_OBSERVER_LIST_H__

#include <vector>
#include <algorithm>

#include "base/basictypes.h"
#include "base/logging.h"

///////////////////////////////////////////////////////////////////////////////
//
// OVERVIEW:
//
//   A container for a list of observers.  Unlike a normal STL vector or list,
//   this container can be modified during iteration without invalidating the
//   iterator.  So, it safely handles the case of an observer removing itself
//   or other observers from the list while observers are being notified.
//
// TYPICAL USAGE:
//
//   class MyWidget {
//    public:
//     ...
//
//     class Observer {
//      public:
//       virtual void OnFoo(MyWidget* w) = 0;
//       virtual void OnBar(MyWidget* w, int x, int y) = 0;
//     };
//
//     void AddObserver(Observer* obs) {
//       observer_list_.AddObserver(obs);
//     }
//
//     void RemoveObserver(Observer* obs) {
//       observer_list_.RemoveObserver(obs);
//     }
//
//     void NotifyFoo() {
//       FOR_EACH_OBSERVER(Observer, observer_list_, OnFoo(this));
//     }
//
//     void NotifyBar(int x, int y) {
//       FOR_EACH_OBSERVER(Observer, observer_list_, OnBar(this, x, y));
//     }
//
//    private:
//     ObserverList<Observer> observer_list_;
//   };
//
///////////////////////////////////////////////////////////////////////////////

template <class ObserverType, bool check_empty = false>
class ObserverList {
 public:
  ObserverList() : notify_depth_(0) {}
  ~ObserverList() {
    // When check_empty is true, assert that the list is empty on destruction.
    if (check_empty) {
      Compact();
      DCHECK_EQ(observers_.size(), 0U);
    }
  }

  // Add an observer to the list.
  void AddObserver(ObserverType* obs) {
    DCHECK(find(observers_.begin(), observers_.end(), obs) == observers_.end())
        << "Observers can only be added once!";
    observers_.push_back(obs);
  }

  // Remove an observer from the list.
  void RemoveObserver(ObserverType* obs) {
    typename ListType::iterator it = 
      std::find(observers_.begin(), observers_.end(), obs);
    if (it != observers_.end()) {
      if (notify_depth_) {
        *it = 0;
      } else {
        observers_.erase(it);
      }
    }
  }

  // An iterator class that can be used to access the list of observers.  See
  // also the FOREACH_OBSERVER macro defined below.
  class Iterator {
   public:
    Iterator(const ObserverList<ObserverType>& list) : list_(list), index_(0) {
      ++list_.notify_depth_;
    }

    ~Iterator() {
      if (--list_.notify_depth_ == 0)
        list_.Compact();
    }

    ObserverType* GetNext() {
      ListType& observers = list_.observers_;
      // Advance if the current element is null
      while (index_ < observers.size() && !observers[index_])
        ++index_;
      return index_ < observers.size() ? observers[index_++] : NULL;
    }

   private:
    const ObserverList<ObserverType>& list_;
    size_t index_;
  };

 private:
  typedef std::vector<ObserverType*> ListType;

  void Compact() const {
    typename ListType::iterator it = observers_.begin();
    while (it != observers_.end()) {
      if (*it) {
        ++it;
      } else {
        it = observers_.erase(it);
      }
    }
  }

  // These are marked mutable to facilitate having NotifyAll be const.
  mutable ListType observers_;
  mutable int notify_depth_;

  friend class ObserverList::Iterator;

  DISALLOW_EVIL_CONSTRUCTORS(ObserverList);
};

#define FOR_EACH_OBSERVER(ObserverType, observer_list, func)  \
  do {                                                        \
    ObserverList<ObserverType>::Iterator it(observer_list);   \
    ObserverType* obs;                                        \
    while ((obs = it.GetNext()) != NULL)                      \
      obs->func;                                              \
  } while (0)

#endif  // BASE_OBSERVER_LIST_H__


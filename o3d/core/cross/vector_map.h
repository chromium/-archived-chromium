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


// This file contains the definition of vector_map, a replacement of std::map
// that is faster for certain workloads.

#ifndef O3D_CORE_CROSS_VECTOR_MAP_H_
#define O3D_CORE_CROSS_VECTOR_MAP_H_

#include <algorithm>
#include <vector>
#include <utility>
#include "base/cross/std_functional.h"

namespace o3d {

// Implementation of a map, but with a sorted std::vector storage instead of a
// RB tree. It makes insertions O(n) but iterations over the data faster than a
// map (O(n) but smaller constant than std::map). Look-ups are O(log(n)),
// comparable to std::map. It's also a lot less memory intensive.

// NOTE: since this is meant to be a drop-in replacement for std::map, it uses
// the STL naming conventions.

// NOTE: The Key needs to be Assignable so that value_type can be a
// pair<Key, Data> instead of a pair<const Key, Data> so that it can be put in a
// vector. std::map doesn't have that restriction. This is somewhat important
// because that means iterators return a pair where the key is mutable,
// but assigning the key could mess up the sorting of the vector. So don't do
// that (the compiler won't protect you like it would in a std::map).
// TODO: see if that could be made safer without adding extra copies.
template <typename Key, typename Data, typename Compare = std::less<Key> >
class vector_map {
 public:
  typedef Key key_type;
  typedef Data data_type;
  typedef Compare key_compare;

  typedef std::pair<key_type, data_type> value_type;

  class value_compare {
   public:
    explicit value_compare(const key_compare &compare) : compare_(compare) {}
    bool operator()(const value_type &left, const value_type &right) {
      return compare_(left.first, right.first);
    }
   private:
    key_compare compare_;
  };

  typedef std::vector<value_type> VectorType;
  typedef typename VectorType::size_type size_type;

  class const_iterator;

  class iterator {
   public:
    typedef typename VectorType::iterator::iterator_category iterator_category;
    typedef typename VectorType::iterator::value_type value_type;
    typedef typename VectorType::iterator::difference_type difference_type;
    typedef typename VectorType::iterator::pointer pointer;
    typedef typename VectorType::iterator::reference reference;

    iterator() : vector_iterator_() {}
    iterator(const iterator &other)
        : vector_iterator_(other.vector_iterator_) {}

    iterator& operator++() {
      ++vector_iterator_;
      return *this;
    }

    iterator operator++(int unused) {
      iterator old = *this;
      ++*this;
      return old;
    }

    value_type* operator->() const {
      return vector_iterator_.operator->();
    }

    value_type& operator*() const {
      return *vector_iterator_;
    }

    bool operator==(const iterator& other) const {
      return vector_iterator_ == other.vector_iterator_;
    }

    bool operator!=(const iterator& other) const {
      return !(*this == other);
    }

    bool operator==(const const_iterator& other) const;
    bool operator!=(const const_iterator& other) const;

   private:
    friend class vector_map;
    friend class const_iterator;
    explicit iterator(const typename VectorType::iterator &it)
        : vector_iterator_(it) {}

    typename VectorType::iterator vector_iterator_;
  };

  class const_iterator {
   public:
    typedef typename VectorType::const_iterator::iterator_category
        iterator_category;
    typedef typename VectorType::const_iterator::value_type value_type;
    typedef typename VectorType::const_iterator::difference_type
        difference_type;
    typedef typename VectorType::const_iterator::pointer pointer;
    typedef typename VectorType::const_iterator::reference reference;

    const_iterator() : vector_iterator_() {}

    const_iterator(const const_iterator &other)
        : vector_iterator_(other.vector_iterator_) {}

    const_iterator(const iterator &other)  // NOLINT - we want the implicit cast
        : vector_iterator_(other.vector_iterator_) {}

    const_iterator& operator++() {
      ++vector_iterator_;
      return *this;
    }

    const_iterator operator++(int unused) {
      const_iterator old = *this;
      ++*this;
      return old;
    }

    const value_type* operator->() const {
      return vector_iterator_.operator->();
    }

    const value_type& operator*() const {
      return *vector_iterator_;
    }

    bool operator==(const const_iterator& other) const {
      return vector_iterator_ == other.vector_iterator_;
    }

    bool operator!=(const const_iterator& other) const {
      return !(*this == other);
    }

   private:
    friend class vector_map;
    explicit const_iterator(const typename VectorType::const_iterator &it)
        : vector_iterator_(it) {}

    typename VectorType::const_iterator vector_iterator_;
  };
  // TODO: add reverse iterators.

  vector_map() {}

  explicit vector_map(const key_compare &compare)
      : compare_(compare) {}

  vector_map(const vector_map &other)
      : vector_(other.vector_),
        compare_(other.compare_) {}

  template <class InputIterator>
  vector_map(InputIterator f, InputIterator l) {
    insert(f, l);
  }

  template <class InputIterator>
  vector_map(InputIterator f, InputIterator l, const key_compare &compare)
      : compare_(compare) {
    insert(f, l);
  }

  vector_map &operator=(const vector_map &other) {
    vector_ = other.vector_;
    compare_ = other.compare_;
    return *this;
  }

  void swap(vector_map &other) {
    vector_.swap(other.vector_);
    compare_.swap(other.compare_);
  }

  key_compare key_comp() const { return compare_; }
  value_compare value_comp() const { return value_compare(key_comp()); }

  iterator begin() { return iterator(vector_.begin()); }
  const_iterator begin() const { return const_iterator(vector_.begin()); }
  iterator end() { return iterator(vector_.end()); }
  const_iterator end() const { return const_iterator(vector_.end()); }

  size_type size() const { return vector_.size(); }
  size_type max_size() const { return vector_.max_size(); }
  bool empty() const { return vector_.empty(); }

  std::pair<iterator, bool> insert(const value_type& x) {
    typename VectorType::iterator it =
        std::lower_bound(vector_.begin(), vector_.end(), x, value_comp());
    if ((it == vector_.end()) || (x.first != it->first)) {
      // Didn't find the key. Insert an element.
      return std::make_pair(iterator(vector_.insert(it, x)), true);
    } else {
      // Found the key.
      return std::make_pair(iterator(it), false);
    }
  }

  iterator insert(iterator pos, const value_type& x) {
    // TODO: take pos into account.
    return insert(x).first;
  }

  template <class InputIterator>
  void insert(InputIterator f, InputIterator l) {
    if (empty()) {
      // If the vector is empty, we can add all elements and then sort in the
      // end. It should be faster.
      vector_.insert(vector_.begin(), f, l);
      std::sort(vector_.begin(), vector_.end(), value_comp());
    } else {
      for (; f != l; ++f) {
        insert(*f);
      }
    }
  }

  void erase(iterator pos) {
    vector_.erase(pos.vector_iterator_);
  }

  size_type erase(const key_type& k) {
    iterator it = find(k);
    if (it != end()) {
      erase(it);
      return 1;
    } else {
      return 0;
    }
  }

  void erase(iterator first, iterator last) {
    vector_.erase(first.vector_iterator_, last.vector_iterator_);
  }

  void clear() {
    vector_.clear();
  }

  iterator find(const key_type& k) {
    iterator it = lower_bound(k);
    if ((it == end()) || (k != it->first)) {
      return end();
    } else {
      return it;
    }
  }

  const_iterator find(const key_type& k) const {
    const_iterator it = lower_bound(k);
    if ((it == end()) || (k != it->first)) {
      return end();
    } else {
      return it;
    }
  }

  const size_type count(const key_type& k) const {
    return (find(k) == end()) ? 0 : 1;
  }

  iterator lower_bound(const key_type& k) {
    // TODO: Is it possible to use lower_bound without having to
    // construct a data_type ?
    value_type x(k, data_type());
    typename VectorType::iterator it =
        std::lower_bound(vector_.begin(), vector_.end(), x, value_comp());
    return iterator(it);
  }

  const_iterator lower_bound(const key_type& k) const {
    // TODO: Is it possible to use lower_bound without having to
    // construct a data_type ?
    value_type x(k, data_type());
    typename VectorType::const_iterator it =
        std::lower_bound(vector_.begin(), vector_.end(), x, value_comp());
    return const_iterator(it);
  }

  iterator upper_bound(const key_type& k) {
    // TODO: Is it possible to use upper_bound without having to
    // construct a data_type ?
    value_type x(k, data_type());
    typename VectorType::iterator it =
        std::upper_bound(vector_.begin(), vector_.end(), x, value_comp());
    return iterator(it);
  }

  const_iterator upper_bound(const key_type& k) const {
    // TODO: Is it possible to use upper_bound without having to
    // construct a data_type ?
    value_type x(k, data_type());
    typename VectorType::const_iterator it =
        std::upper_bound(vector_.begin(), vector_.end(), x, value_comp());
    return const_iterator(it);
  }

  std::pair<iterator, iterator> equal_range(const key_type& k) {
    iterator low = lower_bound(k);
    iterator up = low;
    // We have at most 1 element.
    if (up != end() && up->first == k)
      ++up;
    return std::make_pair(low, up);
  }

  std::pair<const_iterator, const_iterator> equal_range(
      const key_type& k) const {
    const_iterator low = lower_bound(k);
    const_iterator up = low;
    // We have at most 1 element.
    if (up != end() && up->first == k)
      ++up;
    return std::make_pair(low, up);
  }

  data_type &operator[](const key_type &k) {
    std::pair<iterator, bool> pos = insert(value_type(k, data_type()));
    return pos.first->second;
  }

  bool operator==(const vector_map &other) const {
    return vector_ == other.vector_;
  }

  bool operator<(const vector_map &other) const {
    return vector_ < other.vector_;
  }
 private:
  VectorType vector_;
  key_compare compare_;
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_VECTOR_MAP_H_

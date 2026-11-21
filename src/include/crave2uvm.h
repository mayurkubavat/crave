//----------------------------------------------------------------------
//  Licensed to Accellera Systems Initiative Inc. (Accellera) 
//  under one or more contributor license agreements.  
//  See the NOTICE file distributed with this work for 
//  additional information regarding copyright ownership.
//
//  Accellera licenses this file to you under the Apache 
//  License, Version 2.0 (the "License"); you may not use 
//  this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in
//  writing, software distributed under the License is
//  distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//  CONDITIONS OF ANY KIND, either express or implied.  See
//  the License for the specific language governing
//  permissions and limitations under the License.
//----------------------------------------------------------------------

/*! \file crave2uvm.h
    \brief Include this file to get all Crave2UVM functionality.

    This file contains all includes and macros you need to get started with Crave2UVM.
    It includes the adapter classes and redefines the UVM_DO macros in order to
    randomize sequence items and sub-sequences with CRAVE constraints.
 */

#pragma once
#include <type_traits>
#include <utility>

#include "uvm_randomized_sequence_item.h"
#include "uvm_randomized_sequence.h"
#include "uvm_randomized_reg.h"

namespace crave2uvm_detail {

template <typename T>
auto do_not_randomize_impl(const T* seq, int)
    -> decltype(static_cast<bool>(seq->do_not_randomize)) {
  return seq != nullptr && static_cast<bool>(seq->do_not_randomize);
}

template <typename T>
bool do_not_randomize_impl(const T*, long) {
  return false;
}

inline bool do_not_randomize(const ::uvm::uvm_sequence_base* seq) {
  return do_not_randomize_impl(seq, 0);
}

template <typename T>
auto call_unconstrained_randomize(T* obj, int)
    -> typename std::enable_if<
        std::is_convertible<decltype(obj->randomize()), bool>::value,
        bool>::type {
  return static_cast<bool>(obj->randomize());
}

template <typename T>
auto call_unconstrained_randomize(T* obj, long)
    -> typename std::enable_if<
        std::is_convertible<decltype(obj->m_uvm_randomize()), bool>::value,
        bool>::type {
  return static_cast<bool>(obj->m_uvm_randomize());
}

template <typename T>
bool call_unconstrained_randomize(T*, ...) {
  return true;
}

template <typename T>
struct randomizer {
  T* obj;
  explicit randomizer(T* o) : obj(o) {}

  bool operator()() const {
    return call_unconstrained_randomize(obj, 0);
  }

  template <typename E0, typename... Es>
  bool operator()(E0&& e0, Es&&... es) const {
    return obj->randomize_with(std::forward<E0>(e0), std::forward<Es>(es)...);
  }
};

template <typename T>
randomizer<T> make_randomizer(T* obj) {
  return randomizer<T>(obj);
}

} // namespace crave2uvm_detail

#define UNPACK(...) __VA_ARGS__

#undef UVM_DO
#define UVM_DO(SEQ_OR_ITEM) \
  UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, this->m_sequencer, -1,)

#undef UVM_DO_WITH
#define UVM_DO_WITH(SEQ_OR_ITEM, CONSTRAINTS) \
  UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, this->m_sequencer, -1, UNPACK CONSTRAINTS)

#undef UVM_DO_PRI
#define UVM_DO_PRI(SEQ_OR_ITEM, PRIORITY) \
  UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, this->m_sequencer, PRIORITY,)

#undef UVM_DO_PRI_WITH
#define UVM_DO_PRI_WITH(SEQ_OR_ITEM, PRIORITY, CONSTRAINTS) \
  UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, this->m_sequencer, PRIORITY, UNPACK CONSTRAINTS)

#undef UVM_DO_ON
#define UVM_DO_ON(SEQ_OR_ITEM, SEQR) \
  UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, SEQR, -1,)

#undef UVM_DO_ON_PRI
#define UVM_DO_ON_PRI(SEQ_OR_ITEM, SEQR, PRIORITY) \
  UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, SEQR, PRIORITY,)

#undef UVM_DO_ON_WITH
#define UVM_DO_ON_WITH(SEQ_OR_ITEM, SEQR, CONSTRAINTS) \
  UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, SEQR, -1, UNPACK CONSTRAINTS)

/*! \def UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, SEQR, PRIORITY, CONSTRAINTS)
    \brief Redefine UVM_DO_ON_PRI_WITH to randomize both items and sub-sequences with CRAVE.
 */
#undef UVM_DO_ON_PRI_WITH
#define UVM_DO_ON_PRI_WITH(SEQ_OR_ITEM, SEQR, PRIORITY, CONSTRAINTS) \
  { \
  ::uvm::uvm_sequence_base* seq__; \
  UVM_CREATE_ON(SEQ_OR_ITEM, SEQR) \
  seq__ = dynamic_cast< ::uvm::uvm_sequence_base* >(SEQ_OR_ITEM); \
  if (seq__ == nullptr) this->start_item(SEQ_OR_ITEM, PRIORITY); \
  if (!::crave2uvm_detail::do_not_randomize(seq__) && \
      !::crave2uvm_detail::make_randomizer(SEQ_OR_ITEM)(CONSTRAINTS)) \
    UVM_WARNING("RNDFLD", "Randomization failed in uvm_do_with action"); \
  if (seq__ == nullptr) this->finish_item(SEQ_OR_ITEM, PRIORITY); \
  else seq__->start(SEQR, this, PRIORITY, 0); \
  }

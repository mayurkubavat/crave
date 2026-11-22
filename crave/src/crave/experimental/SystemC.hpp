//****************************************************************************************
//	MIT License
//****************************************************************************************
//	Copyright (c) 2012-2020 University of Bremen, Germany. 
//  	Copyright (c) 2015-2020 DFKI GmbH Bremen, Germany.
//  	Copyright (c) 2020 Johannes Kepler University Linz, Austria.
//      Copyright (c) 2022 - 2023 Coseda Technologies GmbH.
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
//	of this software and associated documentation files (the "Software"), to deal
//	in the Software without restriction, including without limitation the rights
//	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//	copies of the Software, and to permit persons to whom the Software is
//	furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in all
//	copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//	SOFTWARE.
//****************************************************************************************

#pragma once

#include "Variable.hpp"

#include <algorithm>
#include <limits>

#include "../frontend/Distribution.hpp"
#include "../frontend/SystemC.hpp"
#include "../frontend/bitsize_traits_sysc.hpp"

namespace crave {

/**
 * crv_variable for SystemC data types
 */
template <typename T>
class crv_variable<T, typename std::enable_if<is_sysc_dt<T>::value>::type> : public crv_var, public crv_variable_base<T> {
  CRV_VARIABLE_COMMON_CONSTRUCTORS(T);
  CRV_VARIABLE_ASSIGNMENT_INTERFACE(T);
  CRV_VARIABLE_ARITHMETIC_INTERFACE(T);
  CRV_VARIABLE_BITWISE_INTERFACE(T);

  crv_constraint c_var_uniform;

 public:
  /**
   * randomize variable
   * @return true if success
   */
  bool randomize() override {
    if (!this->rand_mode()) return true;
    static distribution<uint64_t> dist;
    if (bitsize_traits<T>::value <= 64) {
      this->value = dist.nextValue();
    } else {
      T v(0);
      for (int pos = 0; pos < bitsize_traits<T>::value; pos += 64) {
        v <<= 64;
        v |= T(dist.nextValue());
      }
      this->value = v;
    }
    return true;
  }

  void create_default_cstr () {
	  create_distribution (T());
    }

  template <typename L>
   void create_distribution (const L &tvar, typename std::enable_if<is_signed<L>::value, void>::type* = nullptr) {
    	  c_var_uniform = {dist(this->var, implicit_distribution(make_distribution(weighted_range<int64_t>(get_sc_dt_min_numeric_limit(tvar), get_sc_dt_max_numeric_limit(tvar),1))))};
    }

  template <typename L>
   void create_distribution (const L &tvar, typename std::enable_if<is_unsigned<L>::value && (bitsize_traits<L>::value <= 64), void>::type* = nullptr) {
    	  c_var_uniform = {dist(this->var, implicit_distribution(make_distribution(weighted_range<uint64_t>(get_sc_dt_min_numeric_limit(tvar), get_sc_dt_max_numeric_limit(tvar),1))))};
    }

  template <typename L>
   void create_distribution (const L &, typename std::enable_if<is_unsigned<L>::value && (bitsize_traits<L>::value > 64), void>::type* = nullptr) {}


  template <typename L >
  int64_t get_sc_dt_min_numeric_limit (const L &var, typename std::enable_if<is_signed<L>::value, void>::type* = nullptr) {
	  return -get_sc_dt_max_numeric_limit(var) - 1;
  }

  template <typename L>
    int64_t get_sc_dt_max_numeric_limit (const L &var, typename std::enable_if<is_signed<L>::value, void>::type* = nullptr) {
  	  return var.length() >= 64 ? std::numeric_limits<int64_t>::max() : (int64_t(1) << (var.length() - 1)) - 1;
  	}

  template <typename L >
    uint64_t get_sc_dt_min_numeric_limit(const L &var, typename std::enable_if<is_unsigned<L>::value, void>::type* = nullptr) {
  	  return (uint64_t)(0);
  	}

  template <typename L >
   uint64_t get_sc_dt_max_numeric_limit (const L &var, typename std::enable_if<is_unsigned<L>::value, void>::type* = nullptr) {
	  return var.length() >= 64 ? std::numeric_limits<uint64_t>::max() : (uint64_t(1) << var.length()) - 1;
  }

  unsigned getVarID() override {
 	  return crv_variable_base<T>::id();
   }


};

}  // namespace crave

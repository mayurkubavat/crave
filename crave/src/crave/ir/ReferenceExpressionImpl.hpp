//****************************************************************************************
//	MIT License
//****************************************************************************************
//	Copyright (c) 2012-2020 University of Bremen, Germany. 
//  	Copyright (c) 2015-2020 DFKI GmbH Bremen, Germany.
//  	Copyright (c) 2020 Johannes Kepler University Linz, Austria.
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

#include "ReferenceExpressionType.hpp"
#include "../frontend/bitsize_traits.hpp"

namespace crave {

/**
 * Convert a referenced value to an instance of Constant, with to_constant_expr if it
 * supports the type of the value, as it does for SystemC data types.
 */
template <typename Integer>
auto reference_constant(Integer const& value, unsigned, bool, int) -> decltype(to_constant_expr<Integer>()(value)) {
  return to_constant_expr<Integer>()(value);
}

template <typename Integer>
Constant reference_constant(Integer const& value, unsigned width, bool sign, long) {
  return Constant(value, width, sign);
}

template <typename Integer>
struct ReferenceExpressionImpl : public ReferenceExpression {
 public:
  ReferenceExpressionImpl(Integer const& value, ReferenceExpression::result_type expr) : value_(value), expr_(expr) {}

  virtual ~ReferenceExpressionImpl() {}

 public:
  virtual ReferenceExpression::result_type expr() const {
    unsigned width = bitsize_traits<Integer>::value;
    bool sign = crave::is_signed<Integer>::value;
    return new EqualOpr(expr_, new Constant(reference_constant(value_, width, sign, 0)));
  }

 private:
  Integer const& value_;
  ReferenceExpression::result_type expr_;
};
}  // namespace crave

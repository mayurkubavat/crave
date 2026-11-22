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

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>

#include "../frontend/Distribution.hpp"

namespace crave {

template <typename Integer, typename DistValue>
inline std::string dist_signature_impl(distribution<DistValue> const& dist) {
  std::ostringstream os;
  os << "int:" << bitsize_traits<Integer>::value << ":" << crave::is_signed<Integer>::value << ":"
     << dist.isImplicit() << ":" << dist.hasItems() << ":" << dist.isAllZero();
  for (weighted_range<DistValue> const& r : dist.items()) {
    os << ":[";
    if (std::is_signed<typename distribution_value_type<DistValue>::type>::value) {
      os << static_cast<int64_t>(distribution_value_type<DistValue>::value(r.left_)) << ","
         << static_cast<int64_t>(distribution_value_type<DistValue>::value(r.right_));
    } else {
      os << static_cast<uint64_t>(distribution_value_type<DistValue>::value(r.left_)) << ","
         << static_cast<uint64_t>(distribution_value_type<DistValue>::value(r.right_));
    }
    os << "," << static_cast<int>(r.kind_) << "," << r.raw_weight_ << "]";
  }
  return os.str();
}

template <typename Integer>
inline std::string dist_signature_impl(distribution<bool> const& dist) {
  std::ostringstream os;
  os << std::setprecision(17) << "bool:" << dist.prob() << ":" << dist.isImplicit();
  return os.str();
}

/**
 * get a distribution of T as a distribution of the builtin integer type that holds the values of T
 */
template <typename T>
typename std::enable_if<std::is_same<T, typename distribution_value_type<T>::type>::value,
                        distribution<T> const&>::type
value_distribution(distribution<T> const& dist) {
  return dist;
}

template <typename T>
typename std::enable_if<!std::is_same<T, typename distribution_value_type<T>::type>::value,
                        distribution<typename distribution_value_type<T>::type> >::type
value_distribution(distribution<T> const& dist) {
  typedef distribution_value_type<T> value_type;
  distribution<typename value_type::type> result;
  for (weighted_range<T> const& r : dist.items())
    result(weighted_range<typename value_type::type>(value_type::value(r.left_), value_type::value(r.right_),
                                                     r.raw_weight_, r.kind_));
  if (dist.hasItems()) result.markHasItems();
  result.setImplicit(dist.isImplicit());
  return result;
}

template <typename Integer>
struct DistReferenceExpr : ReferenceExpression {
  typedef typename distribution_value_type<Integer>::type value_type;

  DistReferenceExpr(distribution<value_type> dist, ReferenceExpression::result_type expr) : dist_(dist), expr_(expr) {}

  ReferenceExpression::result_type expr() const override {
    if (dist_.isAllZero()) {
      return new Constant(true);
    }
    unsigned width = bitsize_traits<Integer>::value;
    bool sign = crave::is_signed<Integer>::value;
    return new EqualOpr(expr_, new Constant(dist_.nextValue(), width, sign));
  }

  std::string distSignature() const override { return dist_signature_impl<Integer>(dist_); }

  std::shared_ptr<ReferenceExpression> cloneWithVar(ReferenceExpression::result_type const& new_var) const override {
    return std::make_shared<DistReferenceExpr<Integer> >(dist_, new_var);
  }

 private:
  distribution<value_type> dist_;
  ReferenceExpression::result_type expr_;
};
}  // namespace crave

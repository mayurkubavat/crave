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

#include <type_traits>

#include "ConstraintType.hpp"
#include "Variable.hpp"
#include "bitsize_traits.hpp"
#include "../ir/Node.hpp"

namespace crave {
// Vector

/**
 * tag class for vector
 */
template <typename value_type>
struct vector_tag {
  /**
   * constructor
   * @param id id of this object
   */
  explicit vector_tag(int id) : id_(id) {}
  int id_;
};

/**
 * class for internal symbolic vector variables
 */
template <typename value_type_>
struct Vector : public Constraint<typename boost::proto::terminal<vector_tag<value_type_> >::type> {
  typedef Constraint<typename boost::proto::terminal<vector_tag<value_type_> >::type> base_type;

  /**
   * constructor
   */
  Vector() : base_type(boost::proto::make_expr<boost::proto::tag::terminal>(vector_tag<value_type_>(new_var_id()))) {}

  typedef value_type_ value_type;

  /**
   * get id of this symbolic vector
   * @return id
   */
  int id() const { return boost::proto::value(*this).id_; }

  /**
   * number of elements in vector as a symbolic variable to used in constraints
   * @return symbolic size
   */
  const Variable<unsigned int>& size() const { return size_; }

  /**
   * expression of a reduction of the elements of a vector
   */
  typedef typename boost::proto::result_of::make_expr<boost::proto::tag::terminal, Constraint_Domain, NodePtr>::type
      reduction_type;

  /**
   * \brief The sum of the elements of the vector, to be used in constraints.
   *
   * As the sum() reduction method of the arrays of SystemVerilog (IEEE 1800-2017 7.12.3), it has the type of the
   * elements, so the sum of unsigned char elements wraps around at 8 bits. The number of elements is the size of
   * the vector. The sum of no elements is 0.
   *
   * The vector solver solves a constraint with a reduction after the size of the vector, so the constraint may refer
   * to the elements of this vector only, also in crave::foreach. The sum of a bool vector is not supported.
   *
   * \return symbolic sum
   */
  reduction_type sum() const {
    static_assert(!std::is_same<value_type, bool>::value, "the sum of a bool vector is not supported");
    return reduce(VectorReduction::Sum);
  }

  /**
   * \brief The product of the elements of the vector, to be used in constraints, see sum().
   *
   * The product of no elements is 1. The product of a bool vector is true if all elements are true.
   *
   * \return symbolic product
   */
  reduction_type product() const { return reduce(VectorReduction::Product); }

  /**
   * \brief The bitwise AND of the elements of the vector, to be used in constraints, see sum().
   *
   * It corresponds to the and() reduction method of SystemVerilog. The AND of no elements has all bits set.
   *
   * \return symbolic bitwise AND
   */
  reduction_type and_() const { return reduce(VectorReduction::And); }

  /**
   * \brief The bitwise OR of the elements of the vector, to be used in constraints, see sum().
   *
   * It corresponds to the or() reduction method of SystemVerilog. The OR of no elements is 0.
   *
   * \return symbolic bitwise OR
   */
  reduction_type or_() const { return reduce(VectorReduction::Or); }

  /**
   * \brief The bitwise XOR of the elements of the vector, to be used in constraints, see sum().
   *
   * It corresponds to the xor() reduction method of SystemVerilog. The XOR of no elements is 0.
   *
   * \return symbolic bitwise XOR
   */
  reduction_type xor_() const { return reduce(VectorReduction::Xor); }

 private:
  reduction_type reduce(VectorReduction::Kind kind) const {
    NodePtr r(new VectorReduction(id(), bitsize_traits<value_type>::value, crave::is_signed<value_type>::value, kind));
    return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(r);
  }

  Variable<unsigned int> size_;
};
}  // namespace crave

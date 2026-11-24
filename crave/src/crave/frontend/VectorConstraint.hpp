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

  template <typename VecExpr, VectorReduction::Kind Kind>
  struct vector_reduction_expr : public reduction_type {
    static constexpr bool crave_is_bool_reduction =
        std::is_same<value_type_, bool>::value && (Kind != VectorReduction::Sum);
    static constexpr VectorReduction::Kind crave_reduction_kind = Kind;
    using crave_elem_type = value_type_;

    vector_reduction_expr(reduction_type const& expr, VecExpr const& vec)
        : reduction_type(expr), vec_(boost::proto::deep_copy(vec)) {}

    template <typename Pred>
    auto with(Pred&& pred) const
        -> decltype(make_vector_reduce_with_expr<Kind>(std::declval<VecExpr const&>(), std::forward<Pred>(pred))) {
      return make_vector_reduce_with_expr<Kind>(vec_, std::forward<Pred>(pred));
    }

   private:
    typename boost::proto::result_of::deep_copy<VecExpr const&>::type vec_;
  };

  template <typename VecExpr, typename CapturedPred>
  struct vector_with_proxy {
    vector_with_proxy(VecExpr const& vec, CapturedPred pred)
        : vec_(boost::proto::deep_copy(vec)), pred_(std::move(pred)) {}

    auto sum() const { return make_vector_reduce_with_expr<VectorReduction::Sum>(vec_, pred_); }
    auto product() const { return make_vector_reduce_with_expr<VectorReduction::Product>(vec_, pred_); }
    auto and_() const { return make_vector_reduce_with_expr<VectorReduction::And>(vec_, pred_); }
    auto or_() const { return make_vector_reduce_with_expr<VectorReduction::Or>(vec_, pred_); }
    auto xor_() const { return make_vector_reduce_with_expr<VectorReduction::Xor>(vec_, pred_); }

   private:
    typename boost::proto::result_of::deep_copy<VecExpr const&>::type vec_;
    CapturedPred pred_;
  };

  /**
   * \brief The sum of the elements of the vector, to be used in constraints.
   */
  vector_reduction_expr<Vector, VectorReduction::Sum> sum() const {
    return vector_reduction_expr<Vector, VectorReduction::Sum>(reduce(VectorReduction::Sum), *this);
  }

  /**
   * \brief The product of the elements of the vector, to be used in constraints, see sum().
   */
  vector_reduction_expr<Vector, VectorReduction::Product> product() const {
    return vector_reduction_expr<Vector, VectorReduction::Product>(reduce(VectorReduction::Product), *this);
  }

  /**
   * \brief The bitwise AND of the elements of the vector, to be used in constraints, see sum().
   */
  vector_reduction_expr<Vector, VectorReduction::And> and_() const {
    return vector_reduction_expr<Vector, VectorReduction::And>(reduce(VectorReduction::And), *this);
  }

  /**
   * \brief The bitwise OR of the elements of the vector, to be used in constraints, see sum().
   */
  vector_reduction_expr<Vector, VectorReduction::Or> or_() const {
    return vector_reduction_expr<Vector, VectorReduction::Or>(reduce(VectorReduction::Or), *this);
  }

  /**
   * \brief The bitwise XOR of the elements of the vector, to be used in constraints, see sum().
   */
  vector_reduction_expr<Vector, VectorReduction::Xor> xor_() const {
    return vector_reduction_expr<Vector, VectorReduction::Xor>(reduce(VectorReduction::Xor), *this);
  }

  template <typename Pred>
  auto sum_with(Pred&& pred) const
      -> decltype(make_vector_reduce_with_expr<VectorReduction::Sum>(*this, std::forward<Pred>(pred))) {
    return make_vector_reduce_with_expr<VectorReduction::Sum>(*this, std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto product_with(Pred&& pred) const
      -> decltype(make_vector_reduce_with_expr<VectorReduction::Product>(*this, std::forward<Pred>(pred))) {
    return make_vector_reduce_with_expr<VectorReduction::Product>(*this, std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto and_with(Pred&& pred) const
      -> decltype(make_vector_reduce_with_expr<VectorReduction::And>(*this, std::forward<Pred>(pred))) {
    return make_vector_reduce_with_expr<VectorReduction::And>(*this, std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto or_with(Pred&& pred) const
      -> decltype(make_vector_reduce_with_expr<VectorReduction::Or>(*this, std::forward<Pred>(pred))) {
    return make_vector_reduce_with_expr<VectorReduction::Or>(*this, std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto xor_with(Pred&& pred) const
      -> decltype(make_vector_reduce_with_expr<VectorReduction::Xor>(*this, std::forward<Pred>(pred))) {
    return make_vector_reduce_with_expr<VectorReduction::Xor>(*this, std::forward<Pred>(pred));
  }

  template <typename Pred>
  vector_with_proxy<Vector, detail::capture_pred_t<Pred> > with(Pred&& pred) const {
    return vector_with_proxy<Vector, detail::capture_pred_t<Pred> >(
        *this, detail::capture_predicate(std::forward<Pred>(pred)));
  }

 private:
  reduction_type reduce(VectorReduction::Kind kind) const {
    if (kind == VectorReduction::Sum && std::is_same<value_type, bool>::value) {
      NodePtr r(new VectorReduction(id(), 32u, false, kind, 1u, true));
      return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(r);
    }
    NodePtr r(new VectorReduction(id(), bitsize_traits<value_type>::value, crave::is_signed<value_type>::value, kind));
    return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(r);
  }

  Variable<unsigned int> size_;
};
}  // namespace crave

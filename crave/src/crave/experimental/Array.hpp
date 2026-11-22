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

#include <string>
#include <type_traits>
#include <vector>

#include "Object.hpp"
#include "Variable.hpp"

namespace crave {

/**
 * \ingroup newAPI
 * \brief Class for an N-element array with random values of type T.
 * 
 * This class can be used as an array of random variables and should be preferred to the
 * definition 'crv_variable<T> array[N]' as it comes with CRAVE functionality.
 */
template <typename T, unsigned N>
class crv_array : public crv_object {
 public:
  typedef T value_type;

  template <typename CapturedPred>
  struct array_with_proxy {
    array_with_proxy(crv_array const& arr, CapturedPred pred) : arr_(&arr), pred_(std::move(pred)) {}

    auto sum() const { return make_array_reduce_with_expr(*arr_, VectorReduction::Sum, pred_); }
    auto product() const { return make_array_reduce_with_expr(*arr_, VectorReduction::Product, pred_); }
    auto and_() const { return make_array_reduce_with_expr(*arr_, VectorReduction::And, pred_); }
    auto or_() const { return make_array_reduce_with_expr(*arr_, VectorReduction::Or, pred_); }
    auto xor_() const { return make_array_reduce_with_expr(*arr_, VectorReduction::Xor, pred_); }

   private:
    crv_array const* arr_;
    CapturedPred pred_;
  };

  struct array_reduction_expr : public expression {
    array_reduction_expr(expression const& expr, crv_array const& arr, VectorReduction::Kind kind)
        : expression(expr), arr_(&arr), kind_(kind) {}

    template <typename Pred>
    auto with(Pred&& pred) const {
      return make_array_reduce_with_expr(*arr_, kind_, std::forward<Pred>(pred));
    }

   private:
    crv_array const* arr_;
    VectorReduction::Kind kind_;
  };

  /**
   * \brief Constructor using name.
   */
  crv_array(crv_object_name = "arr") : arr_(), sym_id_(new_var_id()) {
    for (unsigned i = 0; i < N; i++) {
      std::string s = std::string("element[") + std::to_string(i) + "]";
      arr_.emplace_back(new crv_variable<T>{s.c_str()});
    }
  }
  /**
   * \brief Destructor.
   */
  ~crv_array() {
    for (crv_variable<T>* arr : arr_) delete arr;
  }

  /**
   * \brief Deleted copy constructor.
   */
  crv_array(crv_array const&) = delete;

  std::string obj_kind() const override { return "crv_array"; }

  crv_array const& operator()() const { return *this; }
  int sym_id() const { return sym_id_; }

  /**
   * \brief Gets the sum of the array elements as a CRAVE expression.
   */
  array_reduction_expr sum() const {
    expression result = value_to_expression(0);
    for (crv_variable<T>* v : arr_) result = crave::make_expression(result + (*v)());
    return array_reduction_expr(result, *this, VectorReduction::Sum);
  }

  /**
   * \brief Gets the product of the array elements as a CRAVE expression.
   */
  array_reduction_expr product() const {
    expression expr = empty_or_reduce(
        VectorReduction::Product,
        [](expression const& lhs, WriteReference<T> const& rhs) { return multiply(lhs, rhs, is_bool()); });
    return array_reduction_expr(expr, *this, VectorReduction::Product);
  }

  /**
   * \brief Gets the bitwise AND of the array elements as a CRAVE expression.
   */
  array_reduction_expr and_() const {
    expression expr = empty_or_reduce(
        VectorReduction::And,
        [](expression const& lhs, WriteReference<T> const& rhs) { return bit_and(lhs, rhs, is_bool()); });
    return array_reduction_expr(expr, *this, VectorReduction::And);
  }

  /**
   * \brief Gets the bitwise OR of the array elements as a CRAVE expression.
   */
  array_reduction_expr or_() const {
    expression expr = empty_or_reduce(
        VectorReduction::Or,
        [](expression const& lhs, WriteReference<T> const& rhs) { return bit_or(lhs, rhs, is_bool()); });
    return array_reduction_expr(expr, *this, VectorReduction::Or);
  }

  /**
   * \brief Gets the bitwise XOR of the array elements as a CRAVE expression.
   */
  array_reduction_expr xor_() const {
    expression expr = empty_or_reduce(
        VectorReduction::Xor,
        [](expression const& lhs, WriteReference<T> const& rhs) { return bit_xor(lhs, rhs, is_bool()); });
    return array_reduction_expr(expr, *this, VectorReduction::Xor);
  }

  template <typename Pred>
  auto sum_with(Pred&& pred) const
      -> decltype(make_array_reduce_with_expr(*this, VectorReduction::Sum, std::forward<Pred>(pred))) {
    return make_array_reduce_with_expr(*this, VectorReduction::Sum, std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto product_with(Pred&& pred) const
      -> decltype(make_array_reduce_with_expr(*this, VectorReduction::Product, std::forward<Pred>(pred))) {
    return make_array_reduce_with_expr(*this, VectorReduction::Product, std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto and_with(Pred&& pred) const
      -> decltype(make_array_reduce_with_expr(*this, VectorReduction::And, std::forward<Pred>(pred))) {
    return make_array_reduce_with_expr(*this, VectorReduction::And, std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto or_with(Pred&& pred) const
      -> decltype(make_array_reduce_with_expr(*this, VectorReduction::Or, std::forward<Pred>(pred))) {
    return make_array_reduce_with_expr(*this, VectorReduction::Or, std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto xor_with(Pred&& pred) const
      -> decltype(make_array_reduce_with_expr(*this, VectorReduction::Xor, std::forward<Pred>(pred))) {
    return make_array_reduce_with_expr(*this, VectorReduction::Xor, std::forward<Pred>(pred));
  }

  template <typename Pred>
  array_with_proxy<detail::capture_pred_t<Pred> > with(Pred&& pred) const {
    return array_with_proxy<detail::capture_pred_t<Pred> >(
        *this, detail::capture_predicate(std::forward<Pred>(pred)));
  }

  /**
   * \brief Access operator [] for array element.
   * \return The value of type T
   */
  crv_variable<T>& operator[](unsigned pos) { return *arr_[pos]; }
  crv_variable<T> const& operator[](unsigned pos) const { return *arr_[pos]; }

 private:
  template <typename IdxExpr>
  static typename boost::proto::result_of::make_expr<boost::proto::tag::terminal, Constraint_Domain,
                                                     typename std::decay<IdxExpr>::type>::type
  make_array_idx(IdxExpr const& idx, std::true_type) {
    return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(idx);
  }

  template <typename IdxExpr>
  static typename boost::proto::result_of::deep_copy<typename std::decay<IdxExpr>::type const&>::type
  make_array_idx(IdxExpr const& idx, std::false_type) {
    return boost::proto::deep_copy(idx);
  }

 public:
  template <typename IdxExpr,
            typename std::enable_if<
                boost::proto::is_expr<typename std::decay<IdxExpr>::type>::value ||
                    detail::is_lambda_node_proxy<typename std::decay<IdxExpr>::type>::value,
                int>::type = 0>
  auto operator[](IdxExpr const& idx) const
      -> array_access_expr<
          T, N,
          decltype(make_array_idx(
              idx, std::integral_constant<
                       bool, detail::is_lambda_node_proxy<typename std::decay<IdxExpr>::type>::value>{}))> {
    typedef decltype(make_array_idx(
        idx, std::integral_constant<
                 bool, detail::is_lambda_node_proxy<typename std::decay<IdxExpr>::type>::value>{})) idx_t;
    return array_access_expr<T, N, idx_t>(array_access_tag<T, N, idx_t>(
        this,
        make_array_idx(
            idx, std::integral_constant<
                     bool, detail::is_lambda_node_proxy<typename std::decay<IdxExpr>::type>::value>{})));
  }

  constexpr unsigned size() const { return N; }
  constexpr bool empty() const { return N == 0; }

 private:
  // The solver backends do not accept bitwise and arithmetic operators on booleans, so the
  // reductions of a bool array use the logical operators, which give the same results.
  typedef typename std::is_same<T, bool>::type is_bool;

  static expression multiply(expression const& lhs, WriteReference<T> const& rhs, std::false_type) {
    return make_expression(lhs * rhs);
  }
  static expression multiply(expression const& lhs, WriteReference<T> const& rhs, std::true_type) {
    return make_expression(lhs && rhs);
  }
  static expression bit_and(expression const& lhs, WriteReference<T> const& rhs, std::false_type) {
    return make_expression(lhs & rhs);
  }
  static expression bit_and(expression const& lhs, WriteReference<T> const& rhs, std::true_type) {
    return make_expression(lhs && rhs);
  }
  static expression bit_or(expression const& lhs, WriteReference<T> const& rhs, std::false_type) {
    return make_expression(lhs | rhs);
  }
  static expression bit_or(expression const& lhs, WriteReference<T> const& rhs, std::true_type) {
    return make_expression(lhs || rhs);
  }
  static expression bit_xor(expression const& lhs, WriteReference<T> const& rhs, std::false_type) {
    return make_expression(lhs ^ rhs);
  }
  static expression bit_xor(expression const& lhs, WriteReference<T> const& rhs, std::true_type) {
    return make_expression(lhs != rhs);
  }

  template <typename Op>
  expression empty_or_reduce(VectorReduction::Kind kind, Op op) const {
    if (N == 0) {
      if (std::is_same<T, bool>::value) {
        return value_to_expression(kind == VectorReduction::Product || kind == VectorReduction::And);
      }
      unsigned const w = bitsize_traits<T>::value;
      bool const s = crave::is_signed<T>::value;
      uint64_t val = 0;
      if (kind == VectorReduction::Product)
        val = 1;
      else if (kind == VectorReduction::And)
        val = (s || w >= 64) ? ~uint64_t(0) : ((uint64_t(1) << w) - 1);
      NodePtr c(new Constant(val, w, s));
      return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(c);
    }
    expression result = make_expression((*arr_[0])());
    for (unsigned i = 1; i < N; i++) result = op(result, (*arr_[i])());
    return result;
  }

  std::vector<crv_variable<T>*> arr_; /** < elements of array */
  int sym_id_;
};

}  // namespace crave

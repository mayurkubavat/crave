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

#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "Object.hpp"
#include "Variable.hpp"
#include "../../crave/RandomSeedManager.hpp"

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

    auto sum() const { return make_array_reduce_with_expr<VectorReduction::Sum>(*arr_, pred_); }
    auto product() const { return make_array_reduce_with_expr<VectorReduction::Product>(*arr_, pred_); }
    auto and_() const { return make_array_reduce_with_expr<VectorReduction::And>(*arr_, pred_); }
    auto or_() const { return make_array_reduce_with_expr<VectorReduction::Or>(*arr_, pred_); }
    auto xor_() const { return make_array_reduce_with_expr<VectorReduction::Xor>(*arr_, pred_); }

   private:
    crv_array const* arr_;
    CapturedPred pred_;
  };

  template <VectorReduction::Kind Kind>
  struct array_reduction_expr : public expression {
    static constexpr bool crave_is_bool_reduction =
        std::is_same<T, bool>::value && (Kind != VectorReduction::Sum);
    static constexpr VectorReduction::Kind crave_reduction_kind = Kind;
    using crave_elem_type = T;

    array_reduction_expr(expression const& expr, crv_array const& arr) : expression(expr), arr_(&arr) {}

    template <typename Pred>
    auto with(Pred&& pred) const {
      return make_array_reduce_with_expr<Kind>(*arr_, std::forward<Pred>(pred));
    }

   private:
    crv_array const* arr_;
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
  array_reduction_expr<VectorReduction::Sum> sum() const {
    expression result = value_to_expression(0);
    for (crv_variable<T>* v : arr_) result = crave::make_expression(result + (*v)());
    return array_reduction_expr<VectorReduction::Sum>(result, *this);
  }

  /**
   * \brief Gets the product of the array elements as a CRAVE expression.
   */
  array_reduction_expr<VectorReduction::Product> product() const {
    expression expr = empty_or_reduce(
        VectorReduction::Product,
        [](expression const& lhs, WriteReference<T> const& rhs) { return multiply(lhs, rhs, is_bool()); });
    return array_reduction_expr<VectorReduction::Product>(expr, *this);
  }

  /**
   * \brief Gets the bitwise AND of the array elements as a CRAVE expression.
   */
  array_reduction_expr<VectorReduction::And> and_() const {
    expression expr = empty_or_reduce(
        VectorReduction::And,
        [](expression const& lhs, WriteReference<T> const& rhs) { return bit_and(lhs, rhs, is_bool()); });
    return array_reduction_expr<VectorReduction::And>(expr, *this);
  }

  /**
   * \brief Gets the bitwise OR of the array elements as a CRAVE expression.
   */
  array_reduction_expr<VectorReduction::Or> or_() const {
    expression expr = empty_or_reduce(
        VectorReduction::Or,
        [](expression const& lhs, WriteReference<T> const& rhs) { return bit_or(lhs, rhs, is_bool()); });
    return array_reduction_expr<VectorReduction::Or>(expr, *this);
  }

  /**
   * \brief Gets the bitwise XOR of the array elements as a CRAVE expression.
   */
  array_reduction_expr<VectorReduction::Xor> xor_() const {
    expression expr = empty_or_reduce(
        VectorReduction::Xor,
        [](expression const& lhs, WriteReference<T> const& rhs) { return bit_xor(lhs, rhs, is_bool()); });
    return array_reduction_expr<VectorReduction::Xor>(expr, *this);
  }

  template <typename Pred>
  auto sum_with(Pred&& pred) const
      -> decltype(make_array_reduce_with_expr<VectorReduction::Sum>(*this, std::forward<Pred>(pred))) {
    return make_array_reduce_with_expr<VectorReduction::Sum>(*this, std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto product_with(Pred&& pred) const
      -> decltype(make_array_reduce_with_expr<VectorReduction::Product>(*this, std::forward<Pred>(pred))) {
    return make_array_reduce_with_expr<VectorReduction::Product>(*this, std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto and_with(Pred&& pred) const
      -> decltype(make_array_reduce_with_expr<VectorReduction::And>(*this, std::forward<Pred>(pred))) {
    return make_array_reduce_with_expr<VectorReduction::And>(*this, std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto or_with(Pred&& pred) const
      -> decltype(make_array_reduce_with_expr<VectorReduction::Or>(*this, std::forward<Pred>(pred))) {
    return make_array_reduce_with_expr<VectorReduction::Or>(*this, std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto xor_with(Pred&& pred) const
      -> decltype(make_array_reduce_with_expr<VectorReduction::Xor>(*this, std::forward<Pred>(pred))) {
    return make_array_reduce_with_expr<VectorReduction::Xor>(*this, std::forward<Pred>(pred));
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

template <typename Item, unsigned N, typename Pred>
struct obj_array_foreach_tag;

template <typename Item, unsigned N, typename Pred, VectorReduction::Kind Kind>
struct obj_array_reduce_tag;

namespace detail {

template <typename T>
struct is_obj_array_foreach_tag : std::false_type {};

template <typename Item, unsigned N, typename Pred>
struct is_obj_array_foreach_tag<obj_array_foreach_tag<Item, N, Pred> > : std::true_type {};

template <typename T>
struct is_inside_expr_tag : std::false_type {};
template <typename Lhs, typename... Items>
struct is_inside_expr_tag<inside_expr_tag<Lhs, Items...> > : std::true_type {};

template <typename T>
struct is_unique_expr_tag : std::false_type {};
template <typename... Args>
struct is_unique_expr_tag<unique_expr_tag<Args...> > : std::true_type {};

template <typename T>
struct is_array_foreach_tag : std::false_type {};
template <typename ElemT, unsigned N, typename Pred>
struct is_array_foreach_tag<array_foreach_tag<ElemT, N, Pred> > : std::true_type {};

template <typename T>
struct is_vector_foreach_tag : std::false_type {};
template <typename VecExpr, typename Pred>
struct is_vector_foreach_tag<vector_foreach_tag<VecExpr, Pred> > : std::true_type {};

template <typename T>
struct is_bv_bool_unary_expr_tag : std::false_type {};
template <typename Arg>
struct is_bv_bool_unary_expr_tag<bv_unary_expr_tag<operator_onehot, Arg> > : std::true_type {};
template <typename Arg>
struct is_bv_bool_unary_expr_tag<bv_unary_expr_tag<operator_onehot0, Arg> > : std::true_type {};

template <typename T>
struct is_bool_array_access_tag : std::false_type {};
template <unsigned N, typename Idx>
struct is_bool_array_access_tag<array_access_tag<bool, N, Idx> > : std::true_type {};

template <typename T, typename Enable = void>
struct has_crave_reduction_meta : std::false_type {};
template <typename T>
struct has_crave_reduction_meta<T, std::void_t<decltype(T::crave_is_bool_reduction), typename T::crave_elem_type> >
    : std::true_type {};

template <typename T, bool IsExpr = boost::proto::is_expr<typename std::decay<T>::type>::value>
struct is_vector_bool_terminal : std::false_type {};
template <typename T>
struct is_vector_bool_terminal<T, true> {
  using decayed_t = typename std::decay<T>::type;
  static constexpr bool check() {
    if constexpr (std::is_same<typename boost::proto::tag_of<decayed_t>::type, boost::proto::tag::terminal>::value) {
      using val_t = typename std::decay<typename boost::proto::result_of::value<decayed_t>::type>::type;
      return std::is_same<val_t, vector_tag<bool> >::value;
    }
    return false;
  }
  static constexpr bool value = check();
};

template <typename Item, unsigned N, typename Pred>
auto invoke_obj_arr_pred(Pred& pred, unsigned idx, Item const& item, int)
    -> decltype(pred(idx, item)) {
  return pred(idx, item);
}

template <typename Item, unsigned N, typename Pred>
auto invoke_obj_arr_pred(Pred& pred, unsigned, Item const& item, long)
    -> decltype(pred(item)) {
  return pred(item);
}

#if defined(__cplusplus) && (__cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))
template <typename ElemT>
using lambda_probe_item_t = Constraint<typename boost::proto::terminal<var_tag<ElemT> >::type>;

using lambda_probe_idx_t = Constraint<typename boost::proto::terminal<placeholder_tag>::type>;

template <typename ElemT, typename Lambda, typename Enable = void>
struct captured_lambda_result {
  using type = void;
};

template <typename ElemT, typename Lambda>
struct captured_lambda_result<
    ElemT, Lambda,
    std::enable_if_t<std::is_invocable_v<Lambda const&, lambda_probe_idx_t const&, lambda_probe_item_t<ElemT> const&>>> {
  using type = std::invoke_result_t<Lambda const&, lambda_probe_idx_t const&, lambda_probe_item_t<ElemT> const&>;
};

template <typename ElemT, typename Lambda>
struct captured_lambda_result<
    ElemT, Lambda,
    std::enable_if_t<
        !std::is_invocable_v<Lambda const&, lambda_probe_idx_t const&, lambda_probe_item_t<ElemT> const&> &&
        std::is_invocable_v<Lambda const&, lambda_probe_item_t<ElemT> const&>>> {
  using type = std::invoke_result_t<Lambda const&, lambda_probe_item_t<ElemT> const&>;
};

template <typename ElemT, typename Lambda>
struct captured_lambda_result<
    ElemT, Lambda,
    std::enable_if_t<
        !std::is_invocable_v<Lambda const&, lambda_probe_idx_t const&, lambda_probe_item_t<ElemT> const&> &&
        !std::is_invocable_v<Lambda const&, lambda_probe_item_t<ElemT> const&> &&
        std::is_invocable_v<
            Lambda const&, lambda_node_proxy const&,
            std::conditional_t<std::is_same_v<ElemT, bool>, bool_lambda_node_proxy, lambda_node_proxy> const&>>> {
  using item_proxy_t = std::conditional_t<std::is_same_v<ElemT, bool>, bool_lambda_node_proxy, lambda_node_proxy>;
  using type = std::invoke_result_t<Lambda const&, lambda_node_proxy const&, item_proxy_t const&>;
};

template <typename ElemT, typename Lambda>
struct captured_lambda_result<
    ElemT, Lambda,
    std::enable_if_t<
        !std::is_invocable_v<Lambda const&, lambda_probe_idx_t const&, lambda_probe_item_t<ElemT> const&> &&
        !std::is_invocable_v<Lambda const&, lambda_probe_item_t<ElemT> const&> &&
        !std::is_invocable_v<
            Lambda const&, lambda_node_proxy const&,
            std::conditional_t<std::is_same_v<ElemT, bool>, bool_lambda_node_proxy, lambda_node_proxy> const&> &&
        std::is_invocable_v<
            Lambda const&,
            std::conditional_t<std::is_same_v<ElemT, bool>, bool_lambda_node_proxy, lambda_node_proxy> const&>>> {
  using item_proxy_t = std::conditional_t<std::is_same_v<ElemT, bool>, bool_lambda_node_proxy, lambda_node_proxy>;
  using type = std::invoke_result_t<Lambda const&, item_proxy_t const&>;
};

template <typename ElemT, typename Pred, bool IsTerm = false>
struct seq_pred_result_impl {
  using type = Pred;
};

template <typename ElemT, typename Pred>
struct seq_pred_result_impl<ElemT, Pred, true> {
  template <typename V>
  struct extract {
    using type = Pred;
  };
  template <typename Lambda>
  struct extract<captured_lambda_pred<Lambda> > {
    using type = typename captured_lambda_result<ElemT, Lambda>::type;
  };
  using val_t = typename std::decay<typename boost::proto::result_of::value<Pred>::type>::type;
  using type = typename extract<val_t>::type;
};

template <typename ElemT, typename Pred>
struct seq_pred_result {
  using dec_t = typename std::decay<Pred>::type;
  static constexpr bool is_term =
      boost::proto::is_expr<dec_t>::value &&
      std::is_same<typename boost::proto::tag_of<dec_t>::type, boost::proto::tag::terminal>::value;
  using type = typename seq_pred_result_impl<ElemT, dec_t, is_term>::type;
};

template <typename Item, unsigned N, typename Pred>
struct obj_array_pred_result {
  using type = decltype(invoke_obj_arr_pred<Item, N>(std::declval<Pred&>(), 0u, std::declval<Item const&>(), 0));
};

template <typename Expr, typename ElemT = void>
struct is_boolean_proto_expr {
  using Decayed = typename std::decay<Expr>::type;

  template <typename E, typename = void>
  struct check : std::false_type {};

  template <typename E>
  struct check<E, typename std::enable_if<boost::proto::is_expr<E>::value>::type> {
    using tag_t = typename boost::proto::tag_of<E>::type;

    static constexpr bool is_bool_tag() {
      return std::is_same<tag_t, boost::proto::tag::logical_not>::value ||
             std::is_same<tag_t, boost::proto::tag::logical_and>::value ||
             std::is_same<tag_t, boost::proto::tag::logical_or>::value ||
             std::is_same<tag_t, boost::proto::tag::equal_to>::value ||
             std::is_same<tag_t, boost::proto::tag::not_equal_to>::value ||
             std::is_same<tag_t, boost::proto::tag::less>::value ||
             std::is_same<tag_t, boost::proto::tag::less_equal>::value ||
             std::is_same<tag_t, boost::proto::tag::greater>::value ||
             std::is_same<tag_t, boost::proto::tag::greater_equal>::value;
    }

    template <typename V>
    static constexpr bool check_terminal_val() {
      if constexpr (std::is_same<V, bool>::value ||
                    std::is_same<V, bool_lambda_node_proxy>::value ||
                    std::is_same<V, write_ref_tag<bool> >::value ||
                    std::is_same<V, read_ref_tag<bool> >::value ||
                    std::is_same<V, var_tag<bool> >::value ||
                    is_obj_array_foreach_tag<V>::value ||
                    is_inside_expr_tag<V>::value ||
                    is_unique_expr_tag<V>::value ||
                    is_array_foreach_tag<V>::value ||
                    is_vector_foreach_tag<V>::value ||
                    is_bv_bool_unary_expr_tag<V>::value ||
                    is_bool_array_access_tag<V>::value) {
        return true;
      } else if constexpr (std::is_same<V, item_placeholder_tag>::value) {
        return std::is_same<ElemT, bool>::value;
      } else {
        return check_nested_reduce<V>(0);
      }
    }

    static constexpr bool check_nested_reduce(void const*) { return false; }

    template <typename InnerItem, unsigned M, typename InnerPred, VectorReduction::Kind Kind>
    static constexpr bool check_nested_reduce(obj_array_reduce_tag<InnerItem, M, InnerPred, Kind> const*) {
      using inner_res_t = typename obj_array_pred_result<InnerItem, M, InnerPred>::type;
      return Kind != VectorReduction::Sum && is_boolean_proto_expr<inner_res_t>::value;
    }

    template <typename T, unsigned M, typename InnerPred, VectorReduction::Kind Kind>
    static constexpr bool check_nested_reduce(array_reduce_with_tag<T, M, InnerPred, Kind> const*) {
      using inner_res_t = typename seq_pred_result<T, InnerPred>::type;
      return Kind != VectorReduction::Sum && is_boolean_proto_expr<inner_res_t, T>::value;
    }

    template <typename VecExpr, typename InnerPred, VectorReduction::Kind Kind>
    static constexpr bool check_nested_reduce(vector_reduce_with_tag<VecExpr, InnerPred, Kind> const*) {
      using vec_elem_t = typename vector_proto_elem_type<VecExpr>::type;
      using inner_res_t = typename seq_pred_result<vec_elem_t, InnerPred>::type;
      return Kind != VectorReduction::Sum && is_boolean_proto_expr<inner_res_t, vec_elem_t>::value;
    }

    template <typename V>
    static constexpr bool check_nested_reduce(int) {
      if constexpr (std::is_class<V>::value) {
        return check_nested_reduce(static_cast<V const*>(nullptr));
      }
      return false;
    }

    static constexpr bool check_terminal_or_fn() {
      if constexpr (has_crave_reduction_meta<E>::value) {
        return E::crave_is_bool_reduction;
      } else if constexpr (std::is_same<tag_t, boost::proto::tag::terminal>::value) {
        using val_t = typename std::decay<typename boost::proto::result_of::value<E>::type>::type;
        return check_terminal_val<val_t>();
      } else if constexpr (std::is_same<tag_t, boost::proto::tag::subscript>::value) {
        using child0_t = typename std::decay<typename boost::proto::result_of::child_c<E, 0>::type>::type;
        return is_vector_bool_terminal<child0_t>::value;
      } else if constexpr (std::is_same<tag_t, boost::proto::tag::function>::value) {
        using child0_t = typename std::decay<typename boost::proto::result_of::child_c<E, 0>::type>::type;
        if constexpr (std::is_same<typename boost::proto::tag_of<child0_t>::type, boost::proto::tag::terminal>::value) {
          using fn_t = typename std::decay<typename boost::proto::result_of::value<child0_t>::type>::type;
          if constexpr (std::is_same<fn_t, operator_if_then_else>::value) {
            if constexpr (boost::proto::arity_of<E>::value == 4) {
              using child2_t = typename std::decay<typename boost::proto::result_of::child_c<E, 2>::type>::type;
              using child3_t = typename std::decay<typename boost::proto::result_of::child_c<E, 3>::type>::type;
              return is_boolean_proto_expr<child2_t, ElemT>::value &&
                     is_boolean_proto_expr<child3_t, ElemT>::value;
            }
            return false;
          } else {
            return std::is_same<fn_t, operator_inside>::value ||
                   std::is_same<fn_t, operator_if_then>::value ||
                   std::is_same<fn_t, operator_iff>::value ||
                   std::is_same<fn_t, operator_foreach>::value ||
                   std::is_same<fn_t, operator_unique>::value ||
                   std::is_same<fn_t, operator_onehot>::value ||
                   std::is_same<fn_t, operator_onehot0>::value;
          }
        }
        return false;
      } else {
        return false;
      }
    }

    static constexpr bool value = is_bool_tag() || check_terminal_or_fn();
  };

  static constexpr bool value = std::is_same<Decayed, bool>::value ||
                                std::is_same<Decayed, bool_lambda_node_proxy>::value ||
                                check<Decayed>::value;
};

template <typename Expr, typename ElemT = void>
unsigned proto_expr_width(bool sv_rules);

template <typename Expr, typename ElemT = void>
bool proto_expr_signed(bool sv_rules);

template <typename Expr, typename ElemT = void>
inline unsigned proto_promoted_width(bool sv_rules) {
  if constexpr (is_boolean_proto_expr<Expr, ElemT>::value) {
    return sv_rules ? 1u : static_cast<unsigned>(sizeof(int) * 8);
  } else {
    return proto_expr_width<Expr, ElemT>(sv_rules);
  }
}

// Note: Under !sv_signedness(), Context::new_value shrinks runtime non-negative literals (e.g., int literal 1)
// to their minimum unsigned bit-width, whereas crv_object_array<Item, 0> infers width/signedness statically
// from C++ types without instantiating Item (avoiding crv_obj_map side effects and supporting non-default-
// constructible Items).
template <typename T>
struct proto_scalar_traits {
  using unwrapped_t = typename std::decay<typename boost::unwrap_reference<T>::type>::type;

  static unsigned width() {
    if constexpr (is_distribution_scalar<unwrapped_t>::value) {
      return scalar_conv_helper<unwrapped_t>::width();
    } else {
      return 32u;
    }
  }
  static bool is_signed() {
    if constexpr (is_distribution_scalar<unwrapped_t>::value) {
      return scalar_conv_helper<unwrapped_t>::is_signed();
    } else {
      return false;
    }
  }
};

template <typename V>
struct proto_term_meta {
  static unsigned width(bool) { return proto_scalar_traits<V>::width(); }
  static bool is_signed(bool) { return proto_scalar_traits<V>::is_signed(); }
};

template <typename T>
struct proto_term_meta<var_tag<T> > {
  static unsigned width(bool) { return bitsize_traits<T>::value; }
  static bool is_signed(bool) { return crave::is_signed<T>::value; }
};
template <typename T>
struct proto_term_meta<write_ref_tag<T> > {
  static unsigned width(bool) { return bitsize_traits<T>::value; }
  static bool is_signed(bool) { return crave::is_signed<T>::value; }
};
template <typename T>
struct proto_term_meta<read_ref_tag<T> > {
  static unsigned width(bool) { return bitsize_traits<T>::value; }
  static bool is_signed(bool) { return crave::is_signed<T>::value; }
};
template <typename T>
struct proto_term_meta<vector_tag<T> > {
  static unsigned width(bool) { return bitsize_traits<T>::value; }
  static bool is_signed(bool) { return crave::is_signed<T>::value; }
};
template <>
struct proto_term_meta<placeholder_tag> {
  static unsigned width(bool) { return placeholder_bitsize(); }
  static bool is_signed(bool) { return false; }
};
template <typename T, unsigned M, typename Idx>
struct proto_term_meta<array_access_tag<T, M, Idx> > {
  static unsigned width(bool) { return bitsize_traits<T>::value; }
  static bool is_signed(bool) { return crave::is_signed<T>::value; }
};
template <typename Arg>
struct proto_term_meta<bv_unary_expr_tag<operator_countones, Arg> > {
  static unsigned width(bool) { return 32u; }
  static bool is_signed(bool) { return false; }
};
template <typename Arg>
struct proto_term_meta<bv_unary_expr_tag<operator_clog2, Arg> > {
  static unsigned width(bool) { return 32u; }
  static bool is_signed(bool) { return false; }
};
template <typename InnerItem, unsigned M, typename InnerPred, VectorReduction::Kind Kind>
struct proto_term_meta<obj_array_reduce_tag<InnerItem, M, InnerPred, Kind> > {
  using inner_res_t = typename obj_array_pred_result<InnerItem, M, InnerPred>::type;
  static unsigned width(bool sv_rules) {
    if constexpr (Kind == VectorReduction::Sum && is_boolean_proto_expr<inner_res_t>::value) return 32u;
    return proto_expr_width<inner_res_t>(sv_rules);
  }
  static bool is_signed(bool sv_rules) {
    if constexpr (Kind == VectorReduction::Sum && is_boolean_proto_expr<inner_res_t>::value) return false;
    return proto_expr_signed<inner_res_t>(sv_rules);
  }
};
template <typename T, unsigned M, typename InnerPred, VectorReduction::Kind Kind>
struct proto_term_meta<array_reduce_with_tag<T, M, InnerPred, Kind> > {
  using inner_res_t = typename seq_pred_result<T, InnerPred>::type;
  static unsigned width(bool sv_rules) {
    if constexpr (Kind == VectorReduction::Sum && is_boolean_proto_expr<inner_res_t, T>::value) return 32u;
    if constexpr (std::is_base_of<lambda_node_proxy, typename std::decay<inner_res_t>::type>::value) {
      if constexpr (std::is_same<T, bool>::value) return sv_rules ? 1u : static_cast<unsigned>(sizeof(int) * 8);
      return bitsize_traits<T>::value;
    } else {
      return proto_expr_width<inner_res_t, T>(sv_rules);
    }
  }
  static bool is_signed(bool sv_rules) {
    if constexpr (Kind == VectorReduction::Sum && is_boolean_proto_expr<inner_res_t, T>::value) return false;
    if constexpr (std::is_base_of<lambda_node_proxy, typename std::decay<inner_res_t>::type>::value) {
      if constexpr (std::is_same<T, bool>::value) return !sv_rules;
      return crave::is_signed<T>::value;
    } else {
      return proto_expr_signed<inner_res_t, T>(sv_rules);
    }
  }
};
template <typename VecExpr, typename InnerPred, VectorReduction::Kind Kind>
struct proto_term_meta<vector_reduce_with_tag<VecExpr, InnerPred, Kind> > {
  using elem_t = typename vector_proto_elem_type<VecExpr>::type;
  using inner_res_t = typename seq_pred_result<elem_t, InnerPred>::type;
  static unsigned width(bool sv_rules) {
    if constexpr (Kind == VectorReduction::Sum && is_boolean_proto_expr<inner_res_t, elem_t>::value) return 32u;
    if constexpr (std::is_base_of<lambda_node_proxy, typename std::decay<inner_res_t>::type>::value) {
      if constexpr (std::is_same<elem_t, bool>::value) return sv_rules ? 1u : static_cast<unsigned>(sizeof(int) * 8);
      return bitsize_traits<elem_t>::value;
    } else {
      return proto_expr_width<inner_res_t, elem_t>(sv_rules);
    }
  }
  static bool is_signed(bool sv_rules) {
    if constexpr (Kind == VectorReduction::Sum && is_boolean_proto_expr<inner_res_t, elem_t>::value) return false;
    if constexpr (std::is_base_of<lambda_node_proxy, typename std::decay<inner_res_t>::type>::value) {
      if constexpr (std::is_same<elem_t, bool>::value) return !sv_rules;
      return crave::is_signed<elem_t>::value;
    } else {
      return proto_expr_signed<inner_res_t, elem_t>(sv_rules);
    }
  }
};

template <typename Expr, typename ElemT>
unsigned proto_expr_width(bool sv_rules) {
  using E = typename std::decay<Expr>::type;
  if constexpr (is_boolean_proto_expr<E, ElemT>::value) {
    return 1u;
  } else if constexpr (boost::proto::is_expr<E>::value) {
    using tag_t = typename boost::proto::tag_of<E>::type;
    if constexpr (has_crave_reduction_meta<E>::value) {
      if constexpr (E::crave_reduction_kind == VectorReduction::Sum &&
                    std::is_same<typename E::crave_elem_type, bool>::value) {
        return 32u;
      }
      return bitsize_traits<typename E::crave_elem_type>::value;
    } else if constexpr (std::is_same<tag_t, boost::proto::tag::terminal>::value) {
      using val_t = typename std::decay<typename boost::proto::result_of::value<E>::type>::type;
      if constexpr (std::is_same<val_t, item_placeholder_tag>::value) {
        if constexpr (!std::is_void<ElemT>::value) return bitsize_traits<ElemT>::value;
        return 32u;
      } else {
        return proto_term_meta<val_t>::width(sv_rules);
      }
    } else if constexpr (std::is_same<tag_t, boost::proto::tag::subscript>::value) {
      using lhs_t = typename std::decay<typename boost::proto::result_of::child_c<E, 0>::type>::type;
      return proto_expr_width<lhs_t, ElemT>(sv_rules);
    } else if constexpr (std::is_same<tag_t, boost::proto::tag::negate>::value ||
                         std::is_same<tag_t, boost::proto::tag::complement>::value) {
      using child_t = typename std::decay<typename boost::proto::result_of::child_c<E, 0>::type>::type;
      return proto_promoted_width<child_t, ElemT>(sv_rules);
    } else if constexpr (std::is_same<tag_t, boost::proto::tag::shift_left>::value ||
                         std::is_same<tag_t, boost::proto::tag::shift_right>::value) {
      using lhs_t = typename std::decay<typename boost::proto::result_of::child_c<E, 0>::type>::type;
      using rhs_t = typename std::decay<typename boost::proto::result_of::child_c<E, 1>::type>::type;
      unsigned const lw = proto_promoted_width<lhs_t, ElemT>(sv_rules);
      unsigned const rw = proto_promoted_width<rhs_t, ElemT>(sv_rules);
      return sv_rules ? lw : std::max(lw, rw);
    } else if constexpr (std::is_same<tag_t, boost::proto::tag::plus>::value ||
                         std::is_same<tag_t, boost::proto::tag::minus>::value ||
                         std::is_same<tag_t, boost::proto::tag::multiplies>::value ||
                         std::is_same<tag_t, boost::proto::tag::divides>::value ||
                         std::is_same<tag_t, boost::proto::tag::modulus>::value ||
                         std::is_same<tag_t, boost::proto::tag::bitwise_and>::value ||
                         std::is_same<tag_t, boost::proto::tag::bitwise_or>::value ||
                         std::is_same<tag_t, boost::proto::tag::bitwise_xor>::value) {
      using lhs_t = typename std::decay<typename boost::proto::result_of::child_c<E, 0>::type>::type;
      using rhs_t = typename std::decay<typename boost::proto::result_of::child_c<E, 1>::type>::type;
      unsigned const lw = proto_promoted_width<lhs_t, ElemT>(sv_rules);
      unsigned const rw = proto_promoted_width<rhs_t, ElemT>(sv_rules);
      unsigned w = std::max(lw, rw);
      if constexpr (std::is_same<tag_t, boost::proto::tag::divides>::value) {
        if (!sv_rules) {
          bool const ls =
              is_boolean_proto_expr<lhs_t, ElemT>::value ? true : proto_expr_signed<lhs_t, ElemT>(false);
          bool const rs =
              is_boolean_proto_expr<rhs_t, ElemT>::value ? true : proto_expr_signed<rhs_t, ElemT>(false);
          if (ls != rs) w += 1u;
        }
      }
      return w;
    } else if constexpr (std::is_same<tag_t, boost::proto::tag::function>::value) {
      using child0_t = typename std::decay<typename boost::proto::result_of::child_c<E, 0>::type>::type;
      if constexpr (std::is_same<typename boost::proto::tag_of<child0_t>::type, boost::proto::tag::terminal>::value) {
        using fn_t = typename std::decay<typename boost::proto::result_of::value<child0_t>::type>::type;
        if constexpr (std::is_same<fn_t, operator_if_then_else>::value && boost::proto::arity_of<E>::value == 4) {
          using b_t = typename std::decay<typename boost::proto::result_of::child_c<E, 2>::type>::type;
          using c_t = typename std::decay<typename boost::proto::result_of::child_c<E, 3>::type>::type;
          return std::max(proto_promoted_width<b_t, ElemT>(sv_rules),
                          proto_promoted_width<c_t, ElemT>(sv_rules));
        } else if constexpr (std::is_same<fn_t, operator_bitslice>::value && boost::proto::arity_of<E>::value == 4) {
          using v_t = typename std::decay<typename boost::proto::result_of::child_c<E, 3>::type>::type;
          return proto_expr_width<v_t, ElemT>(sv_rules);
        }
      }
      return 32u;
    } else {
      return 32u;
    }
  } else {
    return proto_scalar_traits<E>::width();
  }
}

template <typename Expr, typename ElemT>
bool proto_expr_signed(bool sv_rules) {
  using E = typename std::decay<Expr>::type;
  if constexpr (is_boolean_proto_expr<E, ElemT>::value) {
    return false;
  } else if constexpr (boost::proto::is_expr<E>::value) {
    using tag_t = typename boost::proto::tag_of<E>::type;
    if constexpr (has_crave_reduction_meta<E>::value) {
      if constexpr (E::crave_reduction_kind == VectorReduction::Sum &&
                    std::is_same<typename E::crave_elem_type, bool>::value) {
        return false;
      }
      return crave::is_signed<typename E::crave_elem_type>::value;
    } else if constexpr (std::is_same<tag_t, boost::proto::tag::terminal>::value) {
      using val_t = typename std::decay<typename boost::proto::result_of::value<E>::type>::type;
      if constexpr (std::is_same<val_t, item_placeholder_tag>::value) {
        if constexpr (!std::is_void<ElemT>::value) return crave::is_signed<ElemT>::value;
        return false;
      } else {
        return proto_term_meta<val_t>::is_signed(sv_rules);
      }
    } else if constexpr (std::is_same<tag_t, boost::proto::tag::subscript>::value) {
      using lhs_t = typename std::decay<typename boost::proto::result_of::child_c<E, 0>::type>::type;
      return proto_expr_signed<lhs_t, ElemT>(sv_rules);
    } else if constexpr (std::is_same<tag_t, boost::proto::tag::negate>::value ||
                         std::is_same<tag_t, boost::proto::tag::complement>::value) {
      using child_t = typename std::decay<typename boost::proto::result_of::child_c<E, 0>::type>::type;
      if (sv_rules) {
        return is_boolean_proto_expr<child_t, ElemT>::value ? false : proto_expr_signed<child_t, ElemT>(true);
      }
      return is_boolean_proto_expr<child_t, ElemT>::value ? true : proto_expr_signed<child_t, ElemT>(false);
    } else if constexpr (std::is_same<tag_t, boost::proto::tag::shift_left>::value ||
                         std::is_same<tag_t, boost::proto::tag::shift_right>::value) {
      using lhs_t = typename std::decay<typename boost::proto::result_of::child_c<E, 0>::type>::type;
      if (sv_rules) {
        return is_boolean_proto_expr<lhs_t, ElemT>::value ? false : proto_expr_signed<lhs_t, ElemT>(true);
      }
      return is_boolean_proto_expr<lhs_t, ElemT>::value ? true : proto_expr_signed<lhs_t, ElemT>(false);
    } else if constexpr (std::is_same<tag_t, boost::proto::tag::plus>::value ||
                         std::is_same<tag_t, boost::proto::tag::minus>::value ||
                         std::is_same<tag_t, boost::proto::tag::multiplies>::value ||
                         std::is_same<tag_t, boost::proto::tag::divides>::value ||
                         std::is_same<tag_t, boost::proto::tag::modulus>::value ||
                         std::is_same<tag_t, boost::proto::tag::bitwise_and>::value ||
                         std::is_same<tag_t, boost::proto::tag::bitwise_or>::value ||
                         std::is_same<tag_t, boost::proto::tag::bitwise_xor>::value) {
      using lhs_t = typename std::decay<typename boost::proto::result_of::child_c<E, 0>::type>::type;
      using rhs_t = typename std::decay<typename boost::proto::result_of::child_c<E, 1>::type>::type;
      if (sv_rules) {
        return proto_expr_signed<lhs_t, ElemT>(true) && proto_expr_signed<rhs_t, ElemT>(true);
      }
      if constexpr (is_boolean_proto_expr<lhs_t, ElemT>::value || is_boolean_proto_expr<rhs_t, ElemT>::value) {
        return true;
      }
      return proto_expr_signed<lhs_t, ElemT>(false) || proto_expr_signed<rhs_t, ElemT>(false);
    } else if constexpr (std::is_same<tag_t, boost::proto::tag::function>::value) {
      using child0_t = typename std::decay<typename boost::proto::result_of::child_c<E, 0>::type>::type;
      if constexpr (std::is_same<typename boost::proto::tag_of<child0_t>::type, boost::proto::tag::terminal>::value) {
        using fn_t = typename std::decay<typename boost::proto::result_of::value<child0_t>::type>::type;
        if constexpr (std::is_same<fn_t, operator_if_then_else>::value && boost::proto::arity_of<E>::value == 4) {
          using b_t = typename std::decay<typename boost::proto::result_of::child_c<E, 2>::type>::type;
          using c_t = typename std::decay<typename boost::proto::result_of::child_c<E, 3>::type>::type;
          if (sv_rules) {
            return proto_expr_signed<b_t, ElemT>(true) && proto_expr_signed<c_t, ElemT>(true);
          }
          if constexpr (is_boolean_proto_expr<b_t, ElemT>::value != is_boolean_proto_expr<c_t, ElemT>::value) {
            return true;
          }
          return proto_expr_signed<b_t, ElemT>(false) || proto_expr_signed<c_t, ElemT>(false);
        } else if constexpr (std::is_same<fn_t, operator_bitslice>::value) {
          return false;
        }
      }
      return false;
    } else {
      return false;
    }
  } else {
    return proto_scalar_traits<E>::is_signed();
  }
}
#endif

}  // namespace detail

template <typename Item, unsigned N>
class crv_object_array;

template <typename Item, unsigned N, typename Pred>
struct obj_array_foreach_tag {
  obj_array_foreach_tag(crv_object_array<Item, N> const* a, Pred p) : arr(a), pred(std::move(p)) {}
  crv_object_array<Item, N> const* arr;
  mutable Pred pred;
};

template <typename Item, unsigned N, typename Pred, VectorReduction::Kind Kind>
struct obj_array_reduce_tag {
  static constexpr VectorReduction::Kind kind = Kind;
  obj_array_reduce_tag(crv_object_array<Item, N> const* a, Pred p) : arr(a), pred(std::move(p)) {}
  crv_object_array<Item, N> const* arr;
  mutable Pred pred;
};

template <typename Item, unsigned N>
class crv_object_array : public crv_object {
  static_assert(std::is_base_of<crv_object, Item>::value,
                "crv_object_array requires Item to derive from crv_object");

  template <typename Ptr, typename Ref>
  class iterator_impl {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = Item;
    using difference_type = std::ptrdiff_t;
    using pointer = Ptr;
    using reference = Ref;

    iterator_impl() : ptr_(nullptr) {}
    explicit iterator_impl(std::unique_ptr<Item> const* ptr) : ptr_(ptr) {}

    template <typename OPtr, typename ORef,
              typename std::enable_if<std::is_convertible<OPtr, Ptr>::value, int>::type = 0>
    iterator_impl(iterator_impl<OPtr, ORef> const& other) : ptr_(other.raw()) {}

    std::unique_ptr<Item> const* raw() const { return ptr_; }

    reference operator*() const { return *(*ptr_); }
    pointer operator->() const { return ptr_->get(); }
    reference operator[](difference_type n) const { return *(*(ptr_ + n)); }

    iterator_impl& operator++() {
      ++ptr_;
      return *this;
    }
    iterator_impl operator++(int) {
      iterator_impl tmp(*this);
      ++ptr_;
      return tmp;
    }
    iterator_impl& operator--() {
      --ptr_;
      return *this;
    }
    iterator_impl operator--(int) {
      iterator_impl tmp(*this);
      --ptr_;
      return tmp;
    }
    iterator_impl& operator+=(difference_type n) {
      ptr_ += n;
      return *this;
    }
    iterator_impl& operator-=(difference_type n) {
      ptr_ -= n;
      return *this;
    }
    iterator_impl operator+(difference_type n) const { return iterator_impl(ptr_ + n); }
    iterator_impl operator-(difference_type n) const { return iterator_impl(ptr_ - n); }
    difference_type operator-(iterator_impl const& o) const { return ptr_ - o.ptr_; }

    bool operator==(iterator_impl const& o) const { return ptr_ == o.ptr_; }
    bool operator!=(iterator_impl const& o) const { return ptr_ != o.ptr_; }
    bool operator<(iterator_impl const& o) const { return ptr_ < o.ptr_; }
    bool operator<=(iterator_impl const& o) const { return ptr_ <= o.ptr_; }
    bool operator>(iterator_impl const& o) const { return ptr_ > o.ptr_; }
    bool operator>=(iterator_impl const& o) const { return ptr_ >= o.ptr_; }

   private:
    std::unique_ptr<Item> const* ptr_;
  };

 public:
  using value_type = Item;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = Item&;
  using const_reference = Item const&;
  using pointer = Item*;
  using const_pointer = Item const*;
  using iterator = iterator_impl<Item*, Item&>;
  using const_iterator = iterator_impl<Item const*, Item const&>;

  explicit crv_object_array(crv_object_name = "arr") : arr_() {
    arr_.reserve(N);
    for (unsigned i = 0; i < N; ++i) {
      std::string s = std::string("element[") + std::to_string(i) + "]";
      arr_.emplace_back(create_element(
          s.c_str(), std::integral_constant<bool, std::is_constructible<Item, crv_object_name>::value>{}));
    }
  }

  ~crv_object_array() = default;

  crv_object_array(crv_object_array const&) = delete;
  crv_object_array& operator=(crv_object_array const&) = delete;

  std::string obj_kind() const override { return "crv_object_array"; }

  using crv_object::rand_mode;
  bool rand_mode() const override { return object_rand_mode_; }

  crv_object_array const& operator()() const { return *this; }

  constexpr unsigned size() const noexcept { return N; }
  constexpr bool empty() const noexcept { return N == 0; }

  Item& operator[](unsigned pos) { return *arr_[pos]; }
  Item const& operator[](unsigned pos) const { return *arr_[pos]; }

  Item& at(unsigned pos) {
    if (pos >= N) throw std::out_of_range("crv_object_array::at: index out of range");
    return *arr_[pos];
  }

  Item const& at(unsigned pos) const {
    if (pos >= N) throw std::out_of_range("crv_object_array::at: index out of range");
    return *arr_[pos];
  }

  iterator begin() noexcept { return iterator(arr_.data()); }
  iterator end() noexcept { return iterator(arr_.data() + arr_.size()); }
  const_iterator begin() const noexcept { return const_iterator(arr_.data()); }
  const_iterator end() const noexcept { return const_iterator(arr_.data() + arr_.size()); }
  const_iterator cbegin() const noexcept { return const_iterator(arr_.data()); }
  const_iterator cend() const noexcept { return const_iterator(arr_.data() + arr_.size()); }

  bool randomize() override {
    if (!object_rand_mode_) return true;
    ScopedRngGuard rng_guard(&ensure_rng_());
    pre_randomize();
    for (unsigned i = 0; i < N; ++i) {
      if (arr_[i]->rand_mode() && !arr_[i]->randomize()) return false;
    }
    post_randomize();
    return true;
  }

#if defined(__cplusplus) && (__cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))
  template <typename Pred>
  struct obj_array_with_proxy {
    obj_array_with_proxy(crv_object_array const& arr, Pred pred) : arr_(&arr), pred_(std::move(pred)) {}

    auto sum() const { return make_obj_reduce<VectorReduction::Sum>(); }
    auto product() const { return make_obj_reduce<VectorReduction::Product>(); }
    auto and_() const { return make_obj_reduce<VectorReduction::And>(); }
    auto or_() const { return make_obj_reduce<VectorReduction::Or>(); }
    auto xor_() const { return make_obj_reduce<VectorReduction::Xor>(); }

   private:
    template <VectorReduction::Kind Kind>
    auto make_obj_reduce() const {
      typedef obj_array_reduce_tag<Item, N, Pred, Kind> tag_t;
      return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(tag_t(arr_, pred_));
    }

    crv_object_array const* arr_;
    Pred pred_;
  };

  template <typename Pred>
  auto sum_with(Pred&& pred) const {
    return make_obj_reduce_expr<VectorReduction::Sum>(std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto product_with(Pred&& pred) const {
    return make_obj_reduce_expr<VectorReduction::Product>(std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto and_with(Pred&& pred) const {
    return make_obj_reduce_expr<VectorReduction::And>(std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto or_with(Pred&& pred) const {
    return make_obj_reduce_expr<VectorReduction::Or>(std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto xor_with(Pred&& pred) const {
    return make_obj_reduce_expr<VectorReduction::Xor>(std::forward<Pred>(pred));
  }

  template <typename Pred>
  obj_array_with_proxy<typename std::decay<Pred>::type> with(Pred&& pred) const {
    return obj_array_with_proxy<typename std::decay<Pred>::type>(*this, std::forward<Pred>(pred));
  }

  template <VectorReduction::Kind Kind, typename Pred>
  auto make_obj_reduce_expr(Pred&& pred) const {
    typedef obj_array_reduce_tag<Item, N, typename std::decay<Pred>::type, Kind> tag_t;
    return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(
        tag_t(this, std::forward<Pred>(pred)));
  }

  template <typename Ctx, typename Pred>
  NodePtr eval_foreach_node(Ctx& ctx, Pred& pred) const {
    if (N == 0) {
      return new Constant(true);
    }
    NodePtr acc;
    for (unsigned i = 0; i < N; ++i) {
      NodePtr elem = boost::proto::eval(detail::invoke_obj_arr_pred<Item, N>(pred, i, *arr_[i], 0), ctx);
      acc = acc ? NodePtr(new LogicalAndOpr(acc, elem)) : elem;
    }
    return acc;
  }

  template <typename Ctx, typename Pred>
  NodePtr eval_reduce_node(Ctx& ctx, VectorReduction::Kind kind, Pred& pred) const {
    if (N == 0) {
      using pred_res_t =
          decltype(detail::invoke_obj_arr_pred<Item, N>(pred, 0u, std::declval<Item const&>(), 0));
      if (detail::is_boolean_proto_expr<pred_res_t>::value) {
        if (kind == VectorReduction::Sum) return new Constant(0, 32, false);
        return new Constant(kind == VectorReduction::Product || kind == VectorReduction::And);
      }
      bool const sv_rules = sv_signedness();
      unsigned const w = detail::proto_expr_width<pred_res_t>(sv_rules);
      bool const s = detail::proto_expr_signed<pred_res_t>(sv_rules);
      uint64_t const ones = (s || w >= 64) ? ~uint64_t(0) : ((uint64_t(1) << w) - 1);
      switch (kind) {
        case VectorReduction::Product:
          return new Constant(1, w, s);
        case VectorReduction::And:
          return new Constant(ones, w, s);
        default:
          return new Constant(0, w, s);
      }
    }
    std::vector<NodePtr> nodes;
    nodes.reserve(N);
    for (unsigned i = 0; i < N; ++i) {
      nodes.push_back(boost::proto::eval(detail::invoke_obj_arr_pred<Item, N>(pred, i, *arr_[i], 0), ctx));
    }
    bool const is_bool = is_boolean_ir_node(*nodes[0]);
    NodePtr acc;
    if (kind == VectorReduction::Sum && is_bool) {
      for (NodePtr const& n : nodes) {
        NodePtr elem(new IfThenElse(n, new Constant(1, 32, false), new Constant(0, 32, false)));
        acc = acc ? NodePtr(new PlusOpr(acc, elem)) : elem;
      }
    } else if (is_bool) {
      acc = nodes[0];
      for (std::size_t i = 1; i < nodes.size(); ++i) {
        switch (kind) {
          case VectorReduction::Product:
          case VectorReduction::And:
            acc = NodePtr(new LogicalAndOpr(acc, nodes[i]));
            break;
          case VectorReduction::Or:
            acc = NodePtr(new LogicalOrOpr(acc, nodes[i]));
            break;
          default:
            acc = NodePtr(new NotEqualOpr(acc, nodes[i]));
            break;
        }
      }
    } else {
      acc = nodes[0];
      for (std::size_t i = 1; i < nodes.size(); ++i) {
        switch (kind) {
          case VectorReduction::Sum:
            acc = NodePtr(new PlusOpr(acc, nodes[i]));
            break;
          case VectorReduction::Product:
            acc = NodePtr(new MultipliesOpr(acc, nodes[i]));
            break;
          case VectorReduction::And:
            acc = NodePtr(new AndOpr(acc, nodes[i]));
            break;
          case VectorReduction::Or:
            acc = NodePtr(new OrOpr(acc, nodes[i]));
            break;
          default:
            acc = NodePtr(new XorOpr(acc, nodes[i]));
            break;
        }
      }
    }
    return (!is_bool || kind == VectorReduction::Sum) ? NodePtr(new SelfDeterminedExpr(acc)) : acc;
  }
#endif

 private:
  static std::unique_ptr<Item> create_element(char const* name, std::true_type) {
    return std::unique_ptr<Item>(new Item(crv_object_name(name)));
  }

  static std::unique_ptr<Item> create_element(char const* name, std::false_type) {
    crv_object_name scope_name(name);
    return std::unique_ptr<Item>(new Item());
  }

  std::vector<std::unique_ptr<Item> > arr_;
};

#if defined(__cplusplus) && (__cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))
template <typename Item, unsigned N, typename Pred>
inline auto foreach(crv_object_array<Item, N> const& arr, Pred&& pred) {
  typedef obj_array_foreach_tag<Item, N, typename std::decay<Pred>::type> tag_t;
  return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(
      tag_t(&arr, std::forward<Pred>(pred)));
}
#endif

}  // namespace crave

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
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include "../frontend/PlaceHolder.hpp"
#include "../frontend/ReadReference.hpp"
#include "../frontend/WriteReference.hpp"
#include "../frontend/WeightedRange.hpp"
#include "../ir/Node.hpp"

namespace crave {

template <typename T, unsigned N>
class crv_array;

class __rand_vec_base;

template <typename Pred>
struct captured_lambda_pred {
  explicit captured_lambda_pred(Pred p) : pred(std::move(p)) {}
  Pred pred;
};

namespace detail {
template <typename T>
using op_uncvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

template <typename T, typename Enable = void>
struct is_crv_variable_arg : std::false_type {};

template <typename T>
struct is_lambda_node_proxy : std::false_type {};

template <typename R, bool IsExpr = boost::proto::is_expr<typename std::decay<R>::type>::value>
struct wrap_pred_result {
  typedef typename boost::proto::result_of::make_expr<boost::proto::tag::terminal, Constraint_Domain,
                                                      typename std::decay<R>::type>::type type;
  static type apply(R const& r) { return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(r); }
};

template <typename R>
struct wrap_pred_result<R, true> {
  typedef typename boost::proto::result_of::deep_copy<typename std::decay<R>::type const&>::type type;
  static type apply(R const& r) { return boost::proto::deep_copy(r); }
};

template <typename Pred,
          bool IsExpr = boost::proto::is_expr<typename std::decay<Pred>::type>::value,
          bool IsScalar = is_distribution_scalar<typename std::decay<Pred>::type>::value>
struct pred_capture_helper {
  typedef typename boost::proto::result_of::make_expr<
      boost::proto::tag::terminal, Constraint_Domain,
      captured_lambda_pred<typename std::decay<Pred>::type> >::type type;
  static type capture(Pred const& p) {
    return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(
        captured_lambda_pred<typename std::decay<Pred>::type>(p));
  }
};

template <typename Pred, bool IsScalar>
struct pred_capture_helper<Pred, true, IsScalar> {
  typedef typename boost::proto::result_of::deep_copy<typename std::decay<Pred>::type const&>::type type;
  static type capture(Pred const& p) { return boost::proto::deep_copy(p); }
};

template <typename Pred>
struct pred_capture_helper<Pred, false, true> {
  typedef typename wrap_pred_result<Pred, false>::type type;
  static type capture(Pred const& p) { return wrap_pred_result<Pred, false>::apply(p); }
};

template <typename Pred>
using capture_pred_t = typename pred_capture_helper<typename std::decay<Pred>::type>::type;

template <typename Pred>
capture_pred_t<Pred> capture_predicate(Pred&& p) {
  return pred_capture_helper<typename std::decay<Pred>::type>::capture(p);
}
}  // namespace detail

template <typename VecExpr, typename Pred, VectorReduction::Kind Kind>
struct vector_reduce_with_tag {
  static constexpr VectorReduction::Kind kind = Kind;
  vector_reduce_with_tag(VecExpr v, Pred p) : vec(std::move(v)), pred(std::move(p)) {}
  VecExpr vec;
  Pred pred;
};

template <VectorReduction::Kind Kind, typename VecExpr, typename Pred>
typename boost::proto::result_of::make_expr<
    boost::proto::tag::terminal, Constraint_Domain,
    vector_reduce_with_tag<typename boost::proto::result_of::deep_copy<VecExpr const&>::type,
                           detail::capture_pred_t<Pred>, Kind> >::type
make_vector_reduce_with_expr(VecExpr const& vec, Pred&& pred) {
  typedef vector_reduce_with_tag<typename boost::proto::result_of::deep_copy<VecExpr const&>::type,
                                 detail::capture_pred_t<Pred>, Kind>
      tag_t;
  tag_t tag(boost::proto::deep_copy(vec), detail::capture_predicate(std::forward<Pred>(pred)));
  return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(std::move(tag));
}

}  // namespace crave

#include "../frontend/VectorConstraint.hpp"

namespace crave {

/**
* stream operator for var_tag
* @param out outputstream
* @param tag to be printed value
* @return outputstream
*/
template <typename OUT, typename value_type>
OUT& operator<<(OUT& out, var_tag<value_type> const& tag) {
  out << "var<" << tag.id << ">";
  return out;
}

/**
* stream operator for read_ref_tag
* @param out outputstream
* @param tag to be printed value
* @return outputstream
*/
template <typename OUT, typename value_type>
OUT& operator<<(OUT& out, read_ref_tag<value_type> const& tag) {
  out << "read_ref<" << tag.id << ">";
  return out;
}

/**
* stream operator for write_ref_tag
* @param out outputstream
* @param tag to be printed value
* @return outputstream
*/
template <typename OUT, typename value_type>
OUT& operator<<(OUT& out, write_ref_tag<value_type> const& tag) {
  out << "write_ref<" << tag.id << ">";
  return out;
}

/**
* stream operator for vector_tag
* @param out outputstream
* @param tag to be printed value
* @return outputstream
*/
template <typename OUT, typename value_type>
OUT& operator<<(OUT& out, vector_tag<value_type> const& tag) {
  out << "vector<" << tag.id_ << ">";
  return out;
}

/**
* stream operator for placeholder_tag
* @param out outputstream
* @param tag to be printed value
* @return outputstream
*/
template <typename OUT>
OUT& operator<<(OUT& out, placeholder_tag const& tag);

template <typename OUT>
OUT& operator<<(OUT& out, item_placeholder_tag const& tag);

// special operators

struct operator_inside {};

struct operator_dist {};

struct operator_if_then {};

struct operator_if_then_else : operator_if_then {};

struct operator_iff {};

struct operator_foreach {};

struct operator_unique {};

struct operator_bitslice {};

struct operator_onehot {};

struct operator_onehot0 {};

struct operator_countones {};

struct operator_clog2 {};

/**
 * \ingroup operators
 * \brief Tolerance range [val +/- tol] (closed [val - tol, val + tol]).
 */
template <typename Val, typename Tol>
struct tol_plusminus_expr {
  template <typename V, typename T>
  tol_plusminus_expr(V const& v, T const& t)
      : val_(detail::range_capture<V>::capture(v)), tol_(detail::range_capture<T>::capture(t)) {}
  Val val_;
  Tol tol_;
};

template <typename Val, typename Tol>
tol_plusminus_expr<typename detail::range_capture<Val>::type, typename detail::range_capture<Tol>::type>
tol_plusminus(Val const& val, Tol const& tol) {
  return tol_plusminus_expr<typename detail::range_capture<Val>::type,
                            typename detail::range_capture<Tol>::type>(val, tol);
}

/**
 * \ingroup operators
 * \brief Relative percentage tolerance range [val +%- pct] (IEEE 1800-2023 11.4.13).
 */
template <typename Val, typename Pct>
struct tol_pluspct_expr {
  template <typename V, typename P>
  tol_pluspct_expr(V const& v, P const& p)
      : val_(detail::range_capture<V>::capture(v)), pct_(detail::range_capture<P>::capture(p)) {}
  Val val_;
  Pct pct_;
};

template <typename Val, typename Pct>
tol_pluspct_expr<typename detail::range_capture<Val>::type, typename detail::range_capture<Pct>::type>
tol_pluspct(Val const& val, Pct const& pct) {
  return tol_pluspct_expr<typename detail::range_capture<Val>::type,
                          typename detail::range_capture<Pct>::type>(val, pct);
}

/**
 * \ingroup operators
 * \brief Wildcard mask item (value, care_mask) where care_mask bit i==1 means bit i is compared.
 */
template <typename Val, typename Mask>
struct inside_mask_expr {
  template <typename V, typename M>
  inside_mask_expr(V const& v, M const& m)
      : val_(detail::range_capture<V>::capture(v)), mask_(detail::range_capture<M>::capture(m)) {}
  Val val_;
  Mask mask_;
};

/**
 * \ingroup operators
 * \brief Parsed SystemVerilog or bare bit-pattern wildcard mask item.
 */
struct inside_mask_pattern_expr {
  uint64_t value_;
  uint64_t care_mask_;
  unsigned width_;
  bool extend_dont_care_;
};

namespace detail {
inline uint64_t inside_mask_bits(unsigned width) {
  if (width == 0) return 0;
  return width >= 64 ? ~uint64_t(0) : ((uint64_t(1) << width) - 1);
}

inline inside_mask_pattern_expr parse_inside_mask_pattern(std::string const& pat) {
  if (pat.empty()) throw std::invalid_argument("inside_mask pattern must not be empty");

  auto is_wildcard = [](char ch) {
    char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return c == 'x' || c == 'z' || c == '?';
  };

  std::size_t quote = pat.find('\'');
  uint64_t value = 0;
  uint64_t care_mask = 0;
  unsigned parsed_bits = 0;
  bool has_explicit_width = false;
  unsigned explicit_width = 0;
  bool msb_dont_care = false;

  if (quote == std::string::npos) {
    // Bare binary pattern: [01xXzZ?_]+
    for (char raw : pat) {
      if (raw == '_') continue;
      char c = static_cast<char>(std::tolower(static_cast<unsigned char>(raw)));
      value <<= 1;
      care_mask <<= 1;
      if (c == '0') {
        care_mask |= 1;
      } else if (c == '1') {
        value |= 1;
        care_mask |= 1;
      } else if (!is_wildcard(c)) {
        throw std::invalid_argument("invalid character in bare inside_mask pattern: " + pat);
      }
      ++parsed_bits;
      if (parsed_bits > 64) throw std::invalid_argument("inside_mask pattern exceeds 64 bits: " + pat);
    }
    if (parsed_bits == 0) throw std::invalid_argument("inside_mask pattern has no digits: " + pat);
    return inside_mask_pattern_expr{value & care_mask, care_mask, parsed_bits, false};
  }

  // SystemVerilog literal: [width]'[s][b|o|h|d]<digits>
  if (quote > 0) {
    has_explicit_width = true;
    uint64_t w = 0;
    for (std::size_t i = 0; i < quote; ++i) {
      unsigned char uc = static_cast<unsigned char>(pat[i]);
      if (!std::isdigit(uc)) throw std::invalid_argument("invalid size in inside_mask pattern: " + pat);
      w = w * 10 + static_cast<uint64_t>(uc - '0');
      if (w > 64) throw std::invalid_argument("inside_mask pattern width exceeds 64 bits: " + pat);
    }
    if (w == 0) throw std::invalid_argument("inside_mask pattern width must be > 0: " + pat);
    explicit_width = static_cast<unsigned>(w);
  }

  std::size_t pos = quote + 1;
  if (pos >= pat.size()) throw std::invalid_argument("missing base in inside_mask pattern: " + pat);
  if (std::tolower(static_cast<unsigned char>(pat[pos])) == 's') ++pos;
  if (pos >= pat.size()) throw std::invalid_argument("missing base in inside_mask pattern: " + pat);
  char base = static_cast<char>(std::tolower(static_cast<unsigned char>(pat[pos++])));

  if (base == 'd') {
    std::string digits;
    for (; pos < pat.size(); ++pos) {
      if (pat[pos] != '_') digits.push_back(pat[pos]);
    }
    if (digits.empty()) throw std::invalid_argument("missing digits in inside_mask pattern: " + pat);
    if (digits.size() == 1 && is_wildcard(digits[0])) {
      unsigned width = has_explicit_width ? explicit_width : 32u;
      return inside_mask_pattern_expr{0, 0, width, !has_explicit_width};
    }
    uint64_t dec = 0;
    for (char ch : digits) {
      unsigned char uc = static_cast<unsigned char>(ch);
      if (!std::isdigit(uc)) throw std::invalid_argument("invalid decimal digit in inside_mask pattern: " + pat);
      dec = dec * 10 + static_cast<uint64_t>(uc - '0');
    }
    unsigned width = has_explicit_width ? explicit_width : 32u;
    uint64_t m = inside_mask_bits(width);
    return inside_mask_pattern_expr{dec & m, m, width, false};
  }

  unsigned bits_per_digit = 0;
  if (base == 'b')
    bits_per_digit = 1;
  else if (base == 'o')
    bits_per_digit = 3;
  else if (base == 'h')
    bits_per_digit = 4;
  else
    throw std::invalid_argument("invalid base in inside_mask pattern: " + pat);

  for (; pos < pat.size(); ++pos) {
    if (pat[pos] == '_') continue;
    char c = static_cast<char>(std::tolower(static_cast<unsigned char>(pat[pos])));
    if (parsed_bits == 0 && is_wildcard(c)) msb_dont_care = true;
    value = (bits_per_digit >= 64) ? 0 : (value << bits_per_digit);
    care_mask = (bits_per_digit >= 64) ? 0 : (care_mask << bits_per_digit);
    if (is_wildcard(c)) {
      // leave care bits 0
    } else {
      unsigned digit = 0;
      if (c >= '0' && c <= '9')
        digit = static_cast<unsigned>(c - '0');
      else if (c >= 'a' && c <= 'f')
        digit = static_cast<unsigned>(c - 'a' + 10);
      else
        throw std::invalid_argument("invalid digit in inside_mask pattern: " + pat);
      if ((base == 'b' && digit >= 2) || (base == 'o' && digit >= 8))
        throw std::invalid_argument("digit out of radix in inside_mask pattern: " + pat);
      value |= static_cast<uint64_t>(digit);
      care_mask |= inside_mask_bits(bits_per_digit);
    }
    parsed_bits += bits_per_digit;
    if (!has_explicit_width && parsed_bits > 64)
      throw std::invalid_argument("inside_mask pattern exceeds 64 bits: " + pat);
  }
  if (parsed_bits == 0) throw std::invalid_argument("inside_mask pattern has no digits: " + pat);

  unsigned width = has_explicit_width ? explicit_width : parsed_bits;
  if (has_explicit_width && parsed_bits < width && !msb_dont_care) {
    care_mask |= inside_mask_bits(width) & ~inside_mask_bits(parsed_bits);
  }
  uint64_t m = inside_mask_bits(width);
  care_mask &= m;
  value &= care_mask;
  bool extend_dont_care = !has_explicit_width && msb_dont_care;
  return inside_mask_pattern_expr{value, care_mask, width, extend_dont_care};
}
}  // namespace detail

template <typename Val, typename Mask,
          typename std::enable_if<
              !std::is_convertible<typename std::decay<Mask>::type, std::string>::value &&
                  !std::is_convertible<typename std::decay<Mask>::type, char const*>::value,
              int>::type = 0>
inside_mask_expr<typename detail::range_capture<Val>::type, typename detail::range_capture<Mask>::type>
inside_mask(Val const& val, Mask const& mask) {
  return inside_mask_expr<typename detail::range_capture<Val>::type,
                          typename detail::range_capture<Mask>::type>(val, mask);
}

inline inside_mask_pattern_expr inside_mask(std::string const& pattern) {
  return detail::parse_inside_mask_pattern(pattern);
}

inline inside_mask_pattern_expr inside_mask(char const* pattern) {
  if (!pattern) throw std::invalid_argument("inside_mask pattern must not be null");
  return detail::parse_inside_mask_pattern(pattern);
}

namespace detail {
template <typename T>
struct is_inside_wrapper : std::false_type {};
template <typename T, typename Hi>
struct is_inside_wrapper<range<T, Hi> > : std::true_type {};
template <typename Val, typename Tol>
struct is_inside_wrapper<tol_plusminus_expr<Val, Tol> > : std::true_type {};
template <typename Val, typename Pct>
struct is_inside_wrapper<tol_pluspct_expr<Val, Pct> > : std::true_type {};
template <typename Val, typename Mask>
struct is_inside_wrapper<inside_mask_expr<Val, Mask> > : std::true_type {};
template <>
struct is_inside_wrapper<inside_mask_pattern_expr> : std::true_type {};

template <typename T>
struct inside_ref {
  explicit inside_ref(T* p) : ptr(p) {}
  T& get() const { return *ptr; }
  T* ptr;
};

template <typename T,
          bool IsExpr = boost::proto::is_expr<typename std::decay<T>::type>::value,
          bool IsLvalRef = std::is_lvalue_reference<T>::value,
          bool IsValueObj = is_inside_wrapper<typename std::decay<T>::type>::value ||
                            is_distribution_scalar<typename std::decay<T>::type>::value>
struct inside_capture {
  typedef typename std::decay<T>::type type;
  static type capture(T&& v) { return std::forward<T>(v); }
};

template <typename T, bool IsLvalRef, bool IsValueObj>
struct inside_capture<T, true, IsLvalRef, IsValueObj> {
  typedef typename boost::proto::result_of::deep_copy<typename std::decay<T>::type const&>::type type;
  static type capture(T&& v) { return boost::proto::deep_copy(v); }
};

template <typename T>
struct inside_capture<T, false, true, false> {
  typedef inside_ref<typename std::remove_reference<T>::type const> type;
  static type capture(T&& v) { return type(&v); }
};

template <typename T>
using inside_capture_t = typename inside_capture<T>::type;
}  // namespace detail

template <typename Lhs, typename... Items>
struct inside_expr_tag {
  inside_expr_tag(Lhs l, std::tuple<Items...> its) : lhs(std::move(l)), items(std::move(its)) {}
  Lhs lhs;
  std::tuple<Items...> items;
};

/**
 * \ingroup operators
 * \brief Variadic set membership constraint (IEEE 1800-2023 18.5.3).
 */
template <typename Lhs, typename Item0, typename... Items>
typename boost::proto::result_of::make_expr<
    boost::proto::tag::terminal, Constraint_Domain,
    inside_expr_tag<detail::inside_capture_t<Lhs>, detail::inside_capture_t<Item0>,
                    detail::inside_capture_t<Items>...> >::type
inside(Lhs&& lhs, Item0&& item0, Items&&... items) {
  typedef inside_expr_tag<detail::inside_capture_t<Lhs>, detail::inside_capture_t<Item0>,
                          detail::inside_capture_t<Items>...>
      tag_t;
  tag_t tag(detail::inside_capture<Lhs>::capture(std::forward<Lhs>(lhs)),
            std::tuple<detail::inside_capture_t<Item0>, detail::inside_capture_t<Items>...>(
                detail::inside_capture<Item0>::capture(std::forward<Item0>(item0)),
                detail::inside_capture<Items>::capture(std::forward<Items>(items))...));
  return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(std::move(tag));
}

template <typename Lhs, typename Item>
typename boost::proto::result_of::make_expr<
    boost::proto::tag::terminal, Constraint_Domain,
    inside_expr_tag<detail::inside_capture_t<Lhs>, std::vector<typename detail::range_capture<Item>::type> > >::type
inside(Lhs&& lhs, std::initializer_list<Item> items) {
  typedef typename detail::range_capture<Item>::type captured_item_t;
  std::vector<captured_item_t> vec;
  vec.reserve(items.size());
  for (Item const& it : items) vec.push_back(detail::range_capture<Item>::capture(it));
  typedef inside_expr_tag<detail::inside_capture_t<Lhs>, std::vector<captured_item_t> > tag_t;
  tag_t tag(detail::inside_capture<Lhs>::capture(std::forward<Lhs>(lhs)), std::make_tuple(std::move(vec)));
  return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(std::move(tag));
}

template <typename Lhs, typename Val, typename Mask>
auto inside_mask(Lhs&& lhs, Val const& val, Mask const& mask)
    -> decltype(inside(std::forward<Lhs>(lhs), inside_mask(val, mask))) {
  return inside(std::forward<Lhs>(lhs), inside_mask(val, mask));
}

template <typename Lhs,
          typename std::enable_if<boost::proto::is_expr<typename std::decay<Lhs>::type>::value, int>::type = 0>
auto inside_mask(Lhs&& lhs, std::string const& pattern)
    -> decltype(inside(std::forward<Lhs>(lhs), inside_mask(pattern))) {
  return inside(std::forward<Lhs>(lhs), inside_mask(pattern));
}

template <typename Lhs,
          typename std::enable_if<boost::proto::is_expr<typename std::decay<Lhs>::type>::value, int>::type = 0>
auto inside_mask(Lhs&& lhs, char const* pattern)
    -> decltype(inside(std::forward<Lhs>(lhs), inside_mask(pattern))) {
  return inside(std::forward<Lhs>(lhs), inside_mask(pattern));
}

template <typename... Args>
struct unique_expr_tag {
  explicit unique_expr_tag(std::tuple<Args...> a) : args(std::move(a)) {}
  std::tuple<Args...> args;
};

template <typename OpTag, typename Arg>
struct bv_unary_expr_tag {
  explicit bv_unary_expr_tag(Arg a) : arg(std::move(a)) {}
  Arg arg;
};

template <typename VecExpr, typename Pred>
struct vector_foreach_tag {
  vector_foreach_tag(VecExpr v, Pred p) : vec(std::move(v)), pred(std::move(p)) {}
  VecExpr vec;
  Pred pred;
};

namespace detail {
template <typename... Args>
struct all_not_crv_variable : std::true_type {};
template <typename Head, typename... Tail>
struct all_not_crv_variable<Head, Tail...>
    : std::integral_constant<bool, !is_crv_variable_arg<op_uncvref_t<Head> >::value &&
                                       all_not_crv_variable<Tail...>::value> {};

template <typename T>
struct is_bv_func_arg
    : std::integral_constant<bool, boost::proto::is_expr<op_uncvref_t<T> >::value ||
                                       is_distribution_scalar<op_uncvref_t<T> >::value ||
                                       is_lambda_node_proxy<op_uncvref_t<T> >::value> {};

template <typename OpTag, typename Expr>
auto make_bv_unary_expr_impl(Expr const& e, std::true_type)
    -> typename boost::proto::result_of::make_expr<
        boost::proto::tag::terminal, Constraint_Domain,
        bv_unary_expr_tag<OpTag, typename boost::proto::result_of::deep_copy<Expr const&>::type> >::type {
  typedef bv_unary_expr_tag<OpTag, typename boost::proto::result_of::deep_copy<Expr const&>::type> tag_t;
  return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(tag_t(boost::proto::deep_copy(e)));
}

template <typename OpTag, typename Scalar>
auto make_bv_unary_expr_impl(Scalar const& s, std::false_type)
    -> typename boost::proto::result_of::make_expr<boost::proto::tag::terminal, Constraint_Domain,
                                                   bv_unary_expr_tag<OpTag, Scalar> >::type {
  typedef bv_unary_expr_tag<OpTag, Scalar> tag_t;
  return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(tag_t(s));
}
}  // namespace detail

template <typename T, unsigned N, typename Idx>
struct array_access_tag {
  array_access_tag(crv_array<T, N> const* a, Idx i) : arr(a), idx(std::move(i)) {}
  crv_array<T, N> const* arr;
  Idx idx;
};

template <typename T, unsigned N, typename Idx>
struct array_access_expr
    : public Constraint<typename boost::proto::result_of::make_expr<
          boost::proto::tag::terminal, Constraint_Domain, array_access_tag<T, N, Idx> >::type> {
  typedef Constraint<typename boost::proto::result_of::make_expr<
      boost::proto::tag::terminal, Constraint_Domain, array_access_tag<T, N, Idx> >::type>
      base_type;
  explicit array_access_expr(array_access_tag<T, N, Idx> tag)
      : base_type(boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(std::move(tag))) {}

  array_access_expr const& operator()() const { return *this; }
};

template <typename T, unsigned N, typename Pred, VectorReduction::Kind Kind>
struct array_reduce_with_tag {
  static constexpr VectorReduction::Kind kind = Kind;
  array_reduce_with_tag(crv_array<T, N> const* a, Pred p) : arr(a), pred(std::move(p)) {}
  crv_array<T, N> const* arr;
  Pred pred;
};

template <typename T, unsigned N, typename Pred>
struct array_foreach_tag {
  array_foreach_tag(crv_array<T, N> const* a, Pred p) : arr(a), pred(std::move(p)) {}
  crv_array<T, N> const* arr;
  Pred pred;
};

template <VectorReduction::Kind Kind, typename T, unsigned N, typename Pred>
typename boost::proto::result_of::make_expr<
    boost::proto::tag::terminal, Constraint_Domain,
    array_reduce_with_tag<T, N, detail::capture_pred_t<Pred>, Kind> >::type
make_array_reduce_with_expr(crv_array<T, N> const& arr, Pred&& pred) {
  typedef array_reduce_with_tag<T, N, detail::capture_pred_t<Pred>, Kind> tag_t;
  tag_t tag(&arr, detail::capture_predicate(std::forward<Pred>(pred)));
  return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(std::move(tag));
}

/**
 * \ingroup operators
 * \brief A special operator for distribution constraint specification.
 * 
 * The first parameter is the variable to be randomized.
 * The second parameter is the distributon of values. See crave::distribution for details.
 */
boost::proto::result_of::make_expr<boost::proto::tag::terminal, Constraint_Domain, operator_dist>::type const dist{};

/**
 * \ingroup operators
 * \brief A special operator for describing implications.
 * 
 * The first parameter of the operator is the antecedent.
 * The second parameter is the consequent.
 * crave::if_then_else also adds an else-part as the third parameter.
 */
boost::proto::result_of::make_expr<boost::proto::tag::terminal, Constraint_Domain, operator_if_then>::type const
    if_then{};

/**
 * \ingroup operators
 * \brief A special operator for describing conditional constraints.
 * 
 * See also crave::if_then.
 */
boost::proto::result_of::make_expr<boost::proto::tag::terminal, Constraint_Domain, operator_if_then_else>::type const
    if_then_else{};

/**
 * \ingroup operators
 * \brief Logical equivalence (iff / <->) constraint operator.
 */
boost::proto::result_of::make_expr<boost::proto::tag::terminal, Constraint_Domain, operator_iff>::type const iff{};

/**
 * \ingroup operators
 * \brief Iterates over a symbolic vector or crv_array and applies constraints to its elements.
 */
template <typename T, typename Pred>
typename boost::proto::result_of::make_expr<
    boost::proto::tag::terminal, Constraint_Domain,
    vector_foreach_tag<typename boost::proto::result_of::deep_copy<Vector<T> const&>::type,
                       detail::capture_pred_t<Pred> > >::type
foreach(Vector<T> const& vec, Pred&& pred) {
  typedef vector_foreach_tag<typename boost::proto::result_of::deep_copy<Vector<T> const&>::type,
                             detail::capture_pred_t<Pred> >
      tag_t;
  tag_t tag(boost::proto::deep_copy(vec), detail::capture_predicate(std::forward<Pred>(pred)));
  return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(std::move(tag));
}

template <typename Vec, typename Pred,
          typename std::enable_if<std::is_base_of<__rand_vec_base, detail::op_uncvref_t<Vec> >::value, int>::type = 0>
auto foreach(Vec const& vec, Pred&& pred) -> decltype(foreach(vec(), std::forward<Pred>(pred))) {
  return foreach(vec(), std::forward<Pred>(pred));
}

template <typename T, unsigned N, typename Pred>
typename boost::proto::result_of::make_expr<
    boost::proto::tag::terminal, Constraint_Domain,
    array_foreach_tag<T, N, detail::capture_pred_t<Pred> > >::type
foreach(crv_array<T, N> const& arr, Pred&& pred) {
  typedef array_foreach_tag<T, N, detail::capture_pred_t<Pred> > tag_t;
  tag_t tag(&arr, detail::capture_predicate(std::forward<Pred>(pred)));
  return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(std::move(tag));
}

/**
 * \ingroup operators
 * \brief Variadic uniqueness constraint for vectors, crv_arrays, scalars, and expressions.
 */
inline typename boost::proto::result_of::make_expr<
    boost::proto::tag::terminal, Constraint_Domain, unique_expr_tag<> >::type
unique() {
  return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(
      unique_expr_tag<>(std::tuple<>()));
}

template <typename Arg0, typename... Args>
typename boost::proto::result_of::make_expr<
    boost::proto::tag::terminal, Constraint_Domain,
    unique_expr_tag<detail::inside_capture_t<Arg0>, detail::inside_capture_t<Args>...> >::type
unique(Arg0&& arg0, Args&&... args) {
  static_assert(detail::all_not_crv_variable<Arg0, Args...>::value,
                "crave::unique() requires var() rather than bare crv_variable");
  typedef unique_expr_tag<detail::inside_capture_t<Arg0>, detail::inside_capture_t<Args>...> tag_t;
  tag_t tag(std::tuple<detail::inside_capture_t<Arg0>, detail::inside_capture_t<Args>...>(
      detail::inside_capture<Arg0>::capture(std::forward<Arg0>(arg0)),
      detail::inside_capture<Args>::capture(std::forward<Args>(args))...));
  return boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(std::move(tag));
}

/**
 * \ingroup operators
 * \brief Bit-vector system functions (IEEE 1800-2023 20.8-20.9).
 */
template <typename Expr>
auto onehot(Expr&& e)
    -> decltype(detail::make_bv_unary_expr_impl<operator_onehot>(
        std::declval<detail::op_uncvref_t<Expr> const&>(),
        std::integral_constant<bool, boost::proto::is_expr<detail::op_uncvref_t<Expr> >::value>{})) {
  static_assert(!detail::is_crv_variable_arg<detail::op_uncvref_t<Expr> >::value,
                "crave::onehot() requires var() rather than bare crv_variable");
  static_assert(detail::is_bv_func_arg<Expr>::value, "crave::onehot() requires a CRAVE expression or scalar");
  return detail::make_bv_unary_expr_impl<operator_onehot>(
      e, std::integral_constant<bool, boost::proto::is_expr<detail::op_uncvref_t<Expr> >::value>{});
}

template <typename Expr>
auto onehot0(Expr&& e)
    -> decltype(detail::make_bv_unary_expr_impl<operator_onehot0>(
        std::declval<detail::op_uncvref_t<Expr> const&>(),
        std::integral_constant<bool, boost::proto::is_expr<detail::op_uncvref_t<Expr> >::value>{})) {
  static_assert(!detail::is_crv_variable_arg<detail::op_uncvref_t<Expr> >::value,
                "crave::onehot0() requires var() rather than bare crv_variable");
  static_assert(detail::is_bv_func_arg<Expr>::value, "crave::onehot0() requires a CRAVE expression or scalar");
  return detail::make_bv_unary_expr_impl<operator_onehot0>(
      e, std::integral_constant<bool, boost::proto::is_expr<detail::op_uncvref_t<Expr> >::value>{});
}

template <typename Expr>
auto countones(Expr&& e)
    -> decltype(detail::make_bv_unary_expr_impl<operator_countones>(
        std::declval<detail::op_uncvref_t<Expr> const&>(),
        std::integral_constant<bool, boost::proto::is_expr<detail::op_uncvref_t<Expr> >::value>{})) {
  static_assert(!detail::is_crv_variable_arg<detail::op_uncvref_t<Expr> >::value,
                "crave::countones() requires var() rather than bare crv_variable");
  static_assert(detail::is_bv_func_arg<Expr>::value, "crave::countones() requires a CRAVE expression or scalar");
  return detail::make_bv_unary_expr_impl<operator_countones>(
      e, std::integral_constant<bool, boost::proto::is_expr<detail::op_uncvref_t<Expr> >::value>{});
}

template <typename Expr>
auto clog2(Expr&& e)
    -> decltype(detail::make_bv_unary_expr_impl<operator_clog2>(
        std::declval<detail::op_uncvref_t<Expr> const&>(),
        std::integral_constant<bool, boost::proto::is_expr<detail::op_uncvref_t<Expr> >::value>{})) {
  static_assert(!detail::is_crv_variable_arg<detail::op_uncvref_t<Expr> >::value,
                "crave::clog2() requires var() rather than bare crv_variable");
  static_assert(detail::is_bv_func_arg<Expr>::value, "crave::clog2() requires a CRAVE expression or scalar");
  return detail::make_bv_unary_expr_impl<operator_clog2>(
      e, std::integral_constant<bool, boost::proto::is_expr<detail::op_uncvref_t<Expr> >::value>{});
}

/**
 * \ingroup operators
 * \brief A special operator to extract bits from a symbolic variable.
 * 
 * crave::bitslice creates an expression that captures a bit range within the binary representation of a variable.
 * The first parameter of bitslice is the position to begin the slice.
 * The second parameter is the position to end the slice.
 * Both bounds are inclusive.
 * The third parameter is the variable.
 */
boost::proto::result_of::make_expr<boost::proto::tag::terminal, Constraint_Domain, operator_bitslice>::type const
    bitslice{};

/**
 * outputstreamoperator for operator_inside
 * @param out outputstream
 * @param tag operator_inside
 * @return outputstream
 */
template <typename OUT>
OUT& operator<<(OUT& out, operator_inside const& tag);

/**
* stream operator for operator_dist
* @param out outputstream
* @param tag to be printed object
* @return outputstream
*/
template <typename OUT>
OUT& operator<<(OUT& out, operator_dist const& tag);

/**
* stream operator for operator_if_then
* @param out outputstream
* @param tag to be printed object
* @return outputstream
*/
template <typename OUT>
OUT& operator<<(OUT& out, operator_if_then const& tag);

/**
* stream operator for operator_if_then_else
* @param out outputstream
* @param tag to be printed object
* @return outputstream
*/
template <typename OUT>
OUT& operator<<(OUT& out, operator_if_then_else const& tag);

/**
* stream operator for operator_foreach
* @param out outputstream
* @param tag to be printed object
* @return outputstream
*/
template <typename OUT>
OUT& operator<<(OUT& out, operator_foreach const& tag);

/**
* stream operator for operator_unique
* @param out outputstream
* @param tag to be printed object
* @return outputstream
*/
template <typename OUT>
OUT& operator<<(OUT& out, operator_unique const& tag);

/**
* stream operator for operator_bitslice
* @param out outputstream
* @param tag to be printed object
* @return outputstream
*/
template <typename OUT>
OUT& operator<<(OUT& out, operator_bitslice const& tag);

template <typename OUT>
OUT& operator<<(OUT& out, operator_iff const& tag);

template <typename OUT>
OUT& operator<<(OUT& out, operator_onehot const& tag);

template <typename OUT>
OUT& operator<<(OUT& out, operator_onehot0 const& tag);

template <typename OUT>
OUT& operator<<(OUT& out, operator_countones const& tag);

template <typename OUT>
OUT& operator<<(OUT& out, operator_clog2 const& tag);

template <typename Expr>
struct soft_expr {
  explicit soft_expr(Expr const& e) : expr(e) {}
  Expr expr;
};

template <typename Expr>
struct is_soft_expr : std::false_type {};

template <typename Expr>
struct is_soft_expr<soft_expr<Expr>> : std::true_type {};

/**
 * \ingroup operators
 * \brief Marks an inline constraint expression as a soft constraint (IEEE 1800-2023 18.5.14).
 */
template <typename Expr>
soft_expr<Expr> soft(Expr const& expr) {
  static_assert(!is_soft_expr<Expr>::value, "nested crave::soft() is not allowed");
  return soft_expr<Expr>(expr);
}
}  // namespace crave

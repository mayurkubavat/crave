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

#ifdef _MSC_VER
#pragma warning (disable : 4804)
#endif

#include <boost/intrusive_ptr.hpp>
#include <boost/proto/core.hpp>
#include <boost/proto/debug.hpp>
#include <boost/proto/context/callable.hpp>
#include <boost/proto/context/null.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <boost/range/value_type.hpp>

#include <functional>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

#include "../frontend/bitsize_traits.hpp"
#include "../frontend/Constraint.hpp"
#include "../frontend/AssignResultToRef.hpp"
#include "VariableContainer.hpp"
#include "Node.hpp"
#include "ReferenceExpression.hpp"

namespace crave {

bool sv_signedness();

template <typename T, unsigned N>
class crv_array;

class __rand_vec_base;

namespace detail {
template <typename T>
using uncvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

template <typename Tag>
struct is_vector_tag : std::false_type {};
template <typename T>
struct is_vector_tag<vector_tag<T> > : std::true_type {};

template <typename T>
struct vector_tag_elem;
template <typename T>
struct vector_tag_elem<vector_tag<T> > {
  typedef T type;
};

template <typename T, bool IsTerm>
struct is_vector_proto_term : std::false_type {};

template <typename T>
struct is_vector_proto_term<T, true>
    : is_vector_tag<uncvref_t<typename boost::proto::result_of::value<uncvref_t<T> >::type> > {};

template <typename T, bool IsExpr = boost::proto::is_expr<uncvref_t<T> >::value>
struct is_vector_proto_expr : std::false_type {};

template <typename T>
struct is_vector_proto_expr<T, true>
    : is_vector_proto_term<
          T,
          std::is_same<typename boost::proto::tag_of<uncvref_t<T> >::type,
                       boost::proto::tag::terminal>::value> {};

template <typename T>
struct vector_proto_elem_type {
  typedef typename vector_tag_elem<
      uncvref_t<typename boost::proto::result_of::value<uncvref_t<T> >::type> >::type type;
};

template <typename T>
struct is_crv_array : std::false_type {};
template <typename T, unsigned N>
struct is_crv_array<crv_array<T, N> > : std::true_type {};

template <typename T, typename Enable = void>
struct is_boost_range_container : std::false_type {};

template <typename T>
struct is_boost_range_container<
    T, typename std::enable_if<!std::is_same<uncvref_t<T>, std::string>::value &&
                               !is_crv_array<uncvref_t<T> >::value &&
                               !std::is_base_of<__rand_vec_base, uncvref_t<T> >::value &&
                               !boost::proto::is_expr<uncvref_t<T> >::value &&
                               !is_inside_wrapper<uncvref_t<T> >::value,
                               typename std::conditional<false,
                                                         decltype(boost::begin(std::declval<T const&>()),
                                                                  boost::end(std::declval<T const&>())),
                                                         void>::type>::type> : std::true_type {};

template <typename T, typename Enable = void>
struct scalar_conv_helper {
  static __int128 to_int128(T const& v) { return static_cast<__int128>(static_cast<int>(v)); }
  static uint64_t to_u64(T const& v) { return static_cast<uint64_t>(static_cast<int>(v)); }
  static unsigned width() { return bitsize_traits<int>::value; }
  static bool is_signed() { return crave::is_signed<int>::value; }
};

template <typename T>
struct scalar_conv_helper<T, typename std::enable_if<std::is_enum<T>::value>::type> {
  typedef typename std::underlying_type<T>::type under_t;
  static __int128 to_int128(T const& v) { return static_cast<__int128>(static_cast<under_t>(v)); }
  static uint64_t to_u64(T const& v) { return static_cast<uint64_t>(static_cast<under_t>(v)); }
  static unsigned width() { return bitsize_traits<T>::value; }
  static bool is_signed() { return std::is_signed<under_t>::value; }
};

template <typename T>
struct scalar_conv_helper<T, typename std::enable_if<!std::is_enum<T>::value && std::is_integral<T>::value>::type> {
  static __int128 to_int128(T const& v) { return static_cast<__int128>(v); }
  static uint64_t to_u64(T const& v) { return static_cast<uint64_t>(v); }
  static unsigned width() { return bitsize_traits<T>::value; }
  static bool is_signed() { return crave::is_signed<T>::value; }
};

template <typename T>
struct scalar_conv_helper<
    T, typename std::enable_if<!std::is_enum<T>::value && !std::is_integral<T>::value &&
                               is_distribution_scalar<T>::value>::type> {
  typedef typename distribution_value_type<T>::type val_t;
  static __int128 to_int128(T const& v) {
    val_t x = distribution_value_type<T>::value(v);
    if (crave::is_signed<T>::value) return static_cast<__int128>(static_cast<int64_t>(x));
    return static_cast<__int128>(static_cast<uint64_t>(x));
  }
  static uint64_t to_u64(T const& v) { return static_cast<uint64_t>(distribution_value_type<T>::value(v)); }
  static unsigned width() { return bitsize_traits<T>::value; }
  static bool is_signed() { return crave::is_signed<T>::value; }
};

inline bool scalar_fits_width(__int128 v, unsigned width, bool sign, bool is_bool_lhs) {
  if (is_bool_lhs) return v == 0 || v == 1;
  if (width == 0) return false;
  if (sign) {
    if (width >= 64) {
      return v >= static_cast<__int128>(std::numeric_limits<int64_t>::min()) &&
             v <= static_cast<__int128>(std::numeric_limits<int64_t>::max());
    }
    __int128 const lo = -(static_cast<__int128>(1) << (width - 1));
    __int128 const hi = (static_cast<__int128>(1) << (width - 1)) - 1;
    return v >= lo && v <= hi;
  } else {
    if (width >= 64) {
      return v >= 0 && v <= static_cast<__int128>(std::numeric_limits<uint64_t>::max());
    }
    __int128 const hi = (static_cast<__int128>(1) << width) - 1;
    return v >= 0 && v <= hi;
  }
}

inline bool inside_node_is_bool(Node const& n) {
  if (Terminal const* t = dynamic_cast<Terminal const*>(&n)) return t->isBool();
  if (dynamic_cast<NotOpr const*>(&n) || dynamic_cast<LogicalAndOpr const*>(&n) ||
      dynamic_cast<LogicalOrOpr const*>(&n) || dynamic_cast<EqualOpr const*>(&n) ||
      dynamic_cast<NotEqualOpr const*>(&n) || dynamic_cast<LessOpr const*>(&n) ||
      dynamic_cast<LessEqualOpr const*>(&n) || dynamic_cast<GreaterOpr const*>(&n) ||
      dynamic_cast<GreaterEqualOpr const*>(&n) || dynamic_cast<Inside const*>(&n) ||
      dynamic_cast<RangeExpr const*>(&n) || dynamic_cast<TolPlusMinusExpr const*>(&n) ||
      dynamic_cast<TolPlusPctExpr const*>(&n) || dynamic_cast<VectorContains const*>(&n))
    return true;
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&n)) return inside_node_is_bool(*ite->b());
  if (VectorAccess const* va = dynamic_cast<VectorAccess const*>(&n)) return inside_node_is_bool(*va->lhs());
  return false;
}

inline unsigned inside_node_width(Node const& n) {
  if (Terminal const* t = dynamic_cast<Terminal const*>(&n)) return t->bitsize();
  if (VectorAccess const* va = dynamic_cast<VectorAccess const*>(&n)) return inside_node_width(*va->lhs());
  if (Bitslice const* b = dynamic_cast<Bitslice const*>(&n)) return static_cast<unsigned>(b->r() - b->l() + 1);
  if (inside_node_is_bool(n)) return 1;
  if (ExtendExpression const* e = dynamic_cast<ExtendExpression const*>(&n))
    return inside_node_width(*e->child()) + e->value();
  if (dynamic_cast<NegOpr const*>(&n) || dynamic_cast<ComplementOpr const*>(&n))
    return inside_node_width(*static_cast<UnaryExpression const&>(n).child());
  if (dynamic_cast<ShiftLeftOpr const*>(&n) || dynamic_cast<ShiftRightOpr const*>(&n))
    return inside_node_width(*static_cast<BinaryExpression const&>(n).lhs());
  if (BinaryExpression const* b = dynamic_cast<BinaryExpression const*>(&n))
    return std::max(inside_node_width(*b->lhs()), inside_node_width(*b->rhs()));
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&n))
    return std::max(inside_node_width(*ite->b()), inside_node_width(*ite->c()));
  return 1;
}

inline bool inside_node_signed(Node const& n) {
  if (Terminal const* t = dynamic_cast<Terminal const*>(&n)) return t->sign();
  if (dynamic_cast<Bitslice const*>(&n)) return false;
  if (VectorAccess const* va = dynamic_cast<VectorAccess const*>(&n)) return inside_node_signed(*va->lhs());
  if (dynamic_cast<ExtendExpression const*>(&n) || dynamic_cast<NegOpr const*>(&n) ||
      dynamic_cast<ComplementOpr const*>(&n))
    return inside_node_signed(*static_cast<UnaryExpression const&>(n).child());
  if (dynamic_cast<ShiftLeftOpr const*>(&n) || dynamic_cast<ShiftRightOpr const*>(&n))
    return inside_node_signed(*static_cast<BinaryExpression const&>(n).lhs());
  if (BinaryExpression const* b = dynamic_cast<BinaryExpression const*>(&n))
    return inside_node_signed(*b->lhs()) && inside_node_signed(*b->rhs());
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&n))
    return inside_node_signed(*ite->b()) && inside_node_signed(*ite->c());
  return false;
}
}  // namespace detail

struct Context : boost::proto::callable_context<Context, boost::proto::null_context> {
 public:
  typedef NodePtr result_type;

 private:
  typedef std::pair<int, std::shared_ptr<crave::ReferenceExpression> > ReadRefPair;
  typedef std::pair<int, std::shared_ptr<crave::AssignResult> > WriteRefPair;

 public:
  explicit Context(VariableContainer* vars)
      : variables_(vars->variables),
        vector_variables_(vars->vector_variables),
        read_references_(vars->read_references),
        write_references_(vars->write_references),
        dist_references_(vars->dist_references),
        dist_ref_to_var_map(vars->dist_ref_to_var_map),
        implicit_dist_references_(vars->implicit_dist_references),
        user_dist_references_(vars->user_dist_references) {}

  result_type new_var(unsigned id, unsigned width, bool sign) {
    return (variables_[id] = new VariableExpr(id, width, sign));
  }

  /**
   * \internal
   * Makes the read references to the object at the given address, i.e. crave::reference(object), refer to the
   * variable, which var creates when it is first needed, instead of the value of the object.
   * See crave::randomize().
   */
  template <typename T>
  void bindReference(T const& object, std::function<result_type()> const& var) {
    bound_references_[std::make_pair(static_cast<void const*>(&object), std::type_index(typeid(T)))] = var;
  }

  /**
   * \internal
   * Gets the ids of the variables, which the expressions evaluated since bindReference() refer to.
   */
  std::set<int> const& referenced_variables() const { return referenced_variables_; }

  template <typename value_type>
  result_type new_var(var_tag<value_type> const& tag) {
    unsigned width = bitsize_traits<value_type>::value;
    bool sign = crave::is_signed<value_type>::value;
    return new_var(tag.id, width, sign);
  }

  template <typename value_type>
  result_type operator()(boost::proto::tag::terminal, var_tag<value_type> const& tag) {
    if (!bound_references_.empty()) referenced_variables_.insert(tag.id);
    std::map<int, result_type>::iterator ite(variables_.find(tag.id));
    return ite != variables_.end() ? ite->second : new_var(tag);
  }

  template <typename value_type>
  result_type operator()(boost::proto::tag::terminal, vector_tag<value_type> const& tag) {
    std::map<int, result_type>::const_iterator ite = vector_variables_.find(tag.id_);

    if (ite != vector_variables_.end()) {
      return ite->second;
    } else {
      unsigned width = bitsize_traits<value_type>::value;
      bool sign = crave::is_signed<value_type>::value;

      result_type vec = new VectorExpr(tag.id_, width, sign);
      vector_variables_.insert(std::make_pair(tag.id_, vec));
      return vec;
    }
  }
  result_type operator()(boost::proto::tag::terminal, placeholder_tag const& tag) { return new Placeholder(tag.id); }

  result_type operator()(boost::proto::tag::terminal, result_type const& r) { return r; }

  template <typename value_type>
  result_type operator()(boost::proto::tag::terminal, write_ref_tag<value_type> const& ref) {
    if (!bound_references_.empty()) referenced_variables_.insert(ref.id);
    std::map<int, result_type>::const_iterator ite = variables_.find(ref.id);

    if (ite != variables_.end()) {
      return ite->second;
    } else {
      result_type var = new_var(static_cast<var_tag<value_type> >(ref));

      write_references_.push_back(
          std::make_pair(ref.id, std::shared_ptr<crave::AssignResult>(new AssignResultToRef<value_type>(ref.ref))));
      return var;
    }
  }

#if GCC_VERSION < 40600
  template <typename value_type>
  result_type operator()(boost::proto::tag::terminal t, WriteReference<value_type> const& ref) {
    return operator()(t, boost::proto::value(ref));
  }

  template <typename value_type>
  result_type operator()(boost::proto::tag::terminal t, Variable<value_type> const& var) {
    return operator()(t, boost::proto::value(var));
  }
#endif

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::equal_to, Expr1 const& e1, Expr2 const& e2) {
    return new EqualOpr(boost::proto::eval(e1, (*this)), boost::proto::eval(e2, (*this)));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::not_equal_to, Expr1 const& e1, Expr2 const& e2) {
    return new NotEqualOpr(boost::proto::eval(e1, (*this)), boost::proto::eval(e2, (*this)));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::less_equal, Expr1 const& e1, Expr2 const& e2) {
    return new LessEqualOpr(boost::proto::eval(e1, (*this)), boost::proto::eval(e2, (*this)));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::less, Expr1 const& e1, Expr2 const& e2) {
    return new LessOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::greater, Expr1 const& e1, Expr2 const& e2) {
    return new GreaterOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::greater_equal, Expr1 const& e1, Expr2 const& e2) {
    return new GreaterEqualOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::logical_and, Expr1 const& e1, Expr2 const& e2) {
    return new LogicalAndOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::logical_or, Expr1 const& e1, Expr2 const& e2) {
    return new LogicalOrOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::bitwise_and, Expr1 const& e1, Expr2 const& e2) {
    return new AndOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::bitwise_or, Expr1 const& e1, Expr2 const& e2) {
    return new OrOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::bitwise_xor, Expr1 const& e1, Expr2 const& e2) {
    return new XorOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::function, boost::proto::terminal<operator_if_then>::type const&,
                         Expr1 const& e1, Expr2 const& e2) {
    return new IfThenElse(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this), new Constant(true));
  }

  template <typename Expr1, typename Expr2, typename Expr3>
  result_type operator()(boost::proto::tag::function, boost::proto::terminal<operator_if_then_else>::type const,
                         Expr1 const& e1, Expr2 const& e2, Expr3 const& e3) {
    return new IfThenElse(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this), boost::proto::eval(e3, *this));
  }

  template <typename Expr>
  result_type operator()(boost::proto::tag::negate, Expr const& e) {
    return new NegOpr(boost::proto::eval(e, *this));
  }

  template <typename Expr>
  result_type operator()(boost::proto::tag::complement, Expr const& e) {
    return new ComplementOpr(boost::proto::eval(e, *this));
  }

  template <typename Expr>
  result_type operator()(boost::proto::tag::logical_not, Expr const& e) {
    return new NotOpr(boost::proto::eval(e, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::plus, Expr1 const& e1, Expr2 const& e2) {
    return new PlusOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::minus, Expr1 const& e1, Expr2 const& e2) {
    return new MinusOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::modulus, Expr1 const& e1, Expr2 const& e2) {
    return new ModuloOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::divides, Expr1 const& e1, Expr2 const& e2) {
    return new DevideOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::shift_left, Expr1 const& e1, Expr2 const& e2) {
    return new ShiftLeftOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::shift_right, Expr1 const& e1, Expr2 const& e2) {
    return new ShiftRightOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::multiplies, Expr1 const& e1, Expr2 const& e2) {
    return new MultipliesOpr(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Integer>
  result_type operator()(boost::proto::tag::terminal, read_ref_tag<Integer> const& ref) {
    if (!bound_references_.empty()) {
      BoundReferences::const_iterator bound = bound_references_.find(
          std::make_pair(static_cast<void const*>(&ref.ref), std::type_index(typeid(Integer))));
      if (bound != bound_references_.end()) return bound->second();
    }

    std::map<int, result_type>::const_iterator ite = variables_.find(ref.id);

    if (ite != variables_.end()) {
      return ite->second;
    } else {
      result_type var = new_var(static_cast<var_tag<Integer> >(ref));

      std::shared_ptr<crave::ReferenceExpression> refExpr(new ReferenceExpressionImpl<Integer>(ref.ref, var));
      read_references_.push_back(std::make_pair(ref.id, refExpr));

      return var;
    }
  }

  template <typename Integer>
  static result_type new_value(Integer const& i) {
    unsigned width = detail::scalar_conv_helper<Integer>::width();
    bool sign = detail::scalar_conv_helper<Integer>::is_signed();
    __int128 v128 = detail::scalar_conv_helper<Integer>::to_int128(i);
    uint64_t u64 = detail::scalar_conv_helper<Integer>::to_u64(i);
    // With the rules of SystemVerilog, a constant has the width and the signedness of its type, like an
    // unsized integer literal, which has 32 bits (IEEE 1800-2017 5.7.1 and 11.6.1).
    if (sv_signedness()) return new Constant(u64, width, sign);
    if (v128 >= 0) {
      for (unsigned int j = 1; j < width; ++j)
        if ((u64 >> j) == 0) return new Constant(u64, j, false);
    }
    return new Constant(u64, width, sign);
  }

  static result_type new_value(bool const& i) {
    unsigned width = bitsize_traits<bool>::value;
    bool sign = crave::is_signed<bool>::value;
    return new Constant(i, width, sign);
  }

  template <typename Integer>
  result_type operator()(boost::proto::tag::terminal, Integer const& i) {
    return new_value<Integer>(i);
  }

 private:
  template <typename T>
  static T const& unwrap_inside_arg(T const& v) {
    return v;
  }
  template <typename T>
  static T& unwrap_inside_arg(detail::inside_ref<T> const& r) {
    return r.get();
  }

  template <typename T, typename std::enable_if<boost::proto::is_expr<T>::value, int>::type = 0>
  result_type eval_inside_arg(T const& expr) {
    return boost::proto::eval(expr, *this);
  }
  template <typename T, typename std::enable_if<!boost::proto::is_expr<T>::value, int>::type = 0>
  result_type eval_inside_arg(T const& val) {
    return new_value<T>(val);
  }
  template <typename T>
  result_type eval_inside_arg(detail::inside_ref<T> const& r) {
    return eval_inside_arg(r.get());
  }

  template <typename Tag>
  struct single_var_tag_traits : std::false_type {};
  template <typename Integer>
  struct single_var_tag_traits<var_tag<Integer> > : std::true_type {
    typedef Integer integer_type;
  };
  template <typename Integer>
  struct single_var_tag_traits<write_ref_tag<Integer> > : std::true_type {
    typedef Integer integer_type;
  };

  template <typename Lhs, bool IsTerminal>
  struct is_single_var_terminal_lhs : std::false_type {};

  template <typename Lhs>
  struct is_single_var_terminal_lhs<Lhs, true>
      : single_var_tag_traits<typename std::decay<typename boost::proto::result_of::value<Lhs>::type>::type> {};

  template <typename Lhs, bool IsExpr = boost::proto::is_expr<Lhs>::value>
  struct is_single_var_lhs : std::false_type {};

  template <typename Lhs>
  struct is_single_var_lhs<Lhs, true>
      : is_single_var_terminal_lhs<
            Lhs,
            std::is_same<typename boost::proto::tag_of<Lhs>::type, boost::proto::tag::terminal>::value> {};

  template <typename T>
  struct is_scalar_elem
      : std::integral_constant<bool, !std::is_array<detail::uncvref_t<T> >::value &&
                                         !boost::proto::is_expr<detail::uncvref_t<T> >::value &&
                                         !detail::is_inside_wrapper<detail::uncvref_t<T> >::value &&
                                         !detail::is_boost_range_container<detail::uncvref_t<T> >::value &&
                                         !detail::is_crv_array<detail::uncvref_t<T> >::value &&
                                         !std::is_base_of<__rand_vec_base, detail::uncvref_t<T> >::value> {};

  template <typename Integer, typename VarTerm, typename Container>
  bool try_fast_inside(VarTerm const& var_term, Container const& c, result_type& out) {
    typedef typename boost::range_value<Container>::type CollectionEntry;
    if (!is_scalar_elem<CollectionEntry>::value) return false;
    unsigned width = bitsize_traits<Integer>::value;
    bool sign = crave::is_signed<Integer>::value;
    bool const is_bool_lhs = std::is_same<Integer, bool>::value;
    std::set<Constant> constants;
    for (CollectionEntry const& entry : c) {
      __int128 v128 = detail::scalar_conv_helper<CollectionEntry>::to_int128(entry);
      if (!detail::scalar_fits_width(v128, width, sign, is_bool_lhs)) return false;
      if (is_bool_lhs)
        constants.insert(Constant(v128 != 0));
      else
        constants.insert(Constant(detail::scalar_conv_helper<CollectionEntry>::to_u64(entry), width, sign));
    }
    out = new Inside(boost::proto::eval(var_term, *this), constants);
    return true;
  }

  template <typename Lhs, typename Item0,
            typename std::enable_if<
                is_single_var_lhs<typename std::decay<Lhs>::type>::value &&
                    detail::is_boost_range_container<decltype(unwrap_inside_arg(std::declval<Item0 const&>()))>::value,
                int>::type = 0>
  bool try_fast_inside_dispatch(Lhs const& lhs, Item0 const& item0, result_type& out, int) {
    typedef typename is_single_var_lhs<typename std::decay<Lhs>::type>::integer_type Integer;
    return try_fast_inside<Integer>(lhs, unwrap_inside_arg(item0), out);
  }

  template <typename Lhs, typename Item0>
  bool try_fast_inside_dispatch(Lhs const&, Item0 const&, result_type&, long) {
    return false;
  }

  template <typename Lhs, typename Item0>
  bool try_fast_inside_1(Lhs const& lhs, Item0 const& item0, result_type& out, std::true_type) {
    return try_fast_inside_dispatch(lhs, item0, out, 0);
  }

  template <typename Lhs, typename Item0>
  bool try_fast_inside_1(Lhs const&, Item0 const&, result_type&, std::false_type) {
    return false;
  }

  // Lowering individual inside items:
  template <typename T>
  result_type lower_inside_item(result_type const& lhs_node, detail::inside_ref<T> const& r) {
    return lower_inside_item(lhs_node, r.get());
  }

  template <typename Expr,
            typename std::enable_if<boost::proto::is_expr<Expr>::value &&
                                        !detail::is_vector_proto_expr<Expr>::value,
                                    int>::type = 0>
  result_type lower_inside_item(result_type const& lhs_node, Expr const& expr) {
    return new EqualOpr(lhs_node, boost::proto::eval(expr, *this));
  }

  template <typename VecExpr,
            typename std::enable_if<detail::is_vector_proto_expr<VecExpr>::value, int>::type = 0>
  result_type lower_inside_item(result_type const& lhs_node, VecExpr const& vec) {
    boost::proto::eval(vec, *this);
    typedef typename detail::vector_proto_elem_type<VecExpr>::type ElemT;
    unsigned vec_id = static_cast<unsigned>(boost::proto::value(vec).id_);
    return new VectorContains(lhs_node, vec_id, bitsize_traits<ElemT>::value, crave::is_signed<ElemT>::value);
  }

  template <typename Lo, typename Hi>
  result_type lower_inside_item(result_type const& lhs_node, range<Lo, Hi> const& r) {
    return new RangeExpr(lhs_node, eval_inside_arg(r.lo()), eval_inside_arg(r.hi()));
  }

  template <typename Val, typename Tol>
  result_type lower_inside_item(result_type const& lhs_node, tol_plusminus_expr<Val, Tol> const& t) {
    return new TolPlusMinusExpr(lhs_node, eval_inside_arg(t.val_), eval_inside_arg(t.tol_));
  }

  template <typename Val, typename Pct>
  result_type lower_inside_item(result_type const& lhs_node, tol_pluspct_expr<Val, Pct> const& t) {
    return new TolPlusPctExpr(lhs_node, eval_inside_arg(t.val_), eval_inside_arg(t.pct_));
  }

  template <typename Val, typename Mask>
  result_type lower_inside_item(result_type const& lhs_node, inside_mask_expr<Val, Mask> const& m) {
    result_type val_node = eval_inside_arg(m.val_);
    result_type mask_node = eval_inside_arg(m.mask_);
    return new EqualOpr(new AndOpr(lhs_node, mask_node), new AndOpr(val_node, mask_node));
  }

  result_type lower_inside_item(result_type const& lhs_node, inside_mask_pattern_expr const& m) {
    unsigned lhs_w = detail::inside_node_width(*lhs_node);
    if (m.width_ < lhs_w) {
      if (lhs_w <= 64) {
        uint64_t upper = m.extend_dont_care_
                             ? uint64_t(0)
                             : (detail::inside_mask_bits(lhs_w) & ~detail::inside_mask_bits(m.width_));
        uint64_t eff_mask = m.care_mask_ | upper;
        return new EqualOpr(new AndOpr(lhs_node, new Constant(eff_mask, lhs_w, false)),
                            new Constant(m.value_ & m.care_mask_, lhs_w, false));
      }
      result_type cond = new EqualOpr(
          new AndOpr(new Bitslice(lhs_node, static_cast<int>(m.width_ - 1), 0),
                     new Constant(m.care_mask_, m.width_, false)),
          new Constant(m.value_ & m.care_mask_, m.width_, false));
      if (!m.extend_dont_care_) {
        for (unsigned pos = m.width_; pos < lhs_w; pos += 64) {
          unsigned hi = std::min(lhs_w - 1, pos + 63);
          unsigned sw = hi - pos + 1;
          result_type zero_chk = new EqualOpr(new Bitslice(lhs_node, static_cast<int>(hi), static_cast<int>(pos)),
                                              new Constant(0, sw, false));
          cond = new LogicalAndOpr(cond, zero_chk);
        }
      }
      return cond;
    }
    return new EqualOpr(new AndOpr(lhs_node, new Constant(m.care_mask_, m.width_, false)),
                        new Constant(m.value_ & m.care_mask_, m.width_, false));
  }

  template <typename ElemT, unsigned N>
  result_type lower_inside_item(result_type const& lhs_node, crv_array<ElemT, N> const& arr) {
    result_type acc;
    for (unsigned i = 0; i < arr.size(); ++i) {
      result_type eq = new EqualOpr(lhs_node, boost::proto::eval(arr[i](), *this));
      acc = acc ? result_type(new LogicalOrOpr(acc, eq)) : eq;
    }
    return acc;
  }

  template <typename Vec,
            typename std::enable_if<std::is_base_of<__rand_vec_base, Vec>::value, int>::type = 0>
  result_type lower_inside_item(result_type const& lhs_node, Vec const& vec) {
    return lower_inside_item(lhs_node, vec());
  }

  template <typename Container,
            typename std::enable_if<detail::is_boost_range_container<Container>::value, int>::type = 0>
  result_type lower_inside_item(result_type const& lhs_node, Container const& c) {
    result_type acc;
    for (auto const& entry : c) {
      result_type cond = lower_inside_item(lhs_node, entry);
      if (cond) acc = acc ? result_type(new LogicalOrOpr(acc, cond)) : cond;
    }
    return acc;
  }

  template <typename Scalar,
            typename std::enable_if<is_scalar_elem<Scalar>::value, int>::type = 0>
  result_type lower_inside_item(result_type const& lhs_node, Scalar const& s) {
    result_type val_node = new_value<Scalar>(s);
    if (detail::inside_node_is_bool(*lhs_node) || std::is_same<Scalar, bool>::value) {
      return new EqualOpr(lhs_node, val_node);
    }
    unsigned lhs_w = detail::inside_node_width(*lhs_node);
    bool lhs_s = detail::inside_node_signed(*lhs_node);
    __int128 v128 = detail::scalar_conv_helper<Scalar>::to_int128(s);
    if (detail::scalar_fits_width(v128, lhs_w, lhs_s, false)) {
      return new EqualOpr(lhs_node, val_node);
    }
    return new RangeExpr(lhs_node, val_node, val_node);
  }

  template <typename Tuple, std::size_t... Is>
  result_type lower_inside_tuple(result_type const& lhs_node, Tuple const& items, std::index_sequence<Is...>) {
    result_type acc;
    result_type parts[] = {lower_inside_item(lhs_node, std::get<Is>(items))...};
    for (result_type const& p : parts) {
      if (p) acc = acc ? result_type(new LogicalOrOpr(acc, p)) : p;
    }
    return acc ? acc : result_type(new Constant(false));
  }

 public:
  template <typename Lhs, typename... Items>
  result_type operator()(boost::proto::tag::terminal, inside_expr_tag<Lhs, Items...> const& tag) {
    result_type fast_res;
    if (try_fast_inside_1(tag.lhs, std::get<0>(tag.items), fast_res,
                          std::integral_constant<bool, sizeof...(Items) == 1>())) {
      return fast_res;
    }
    result_type lhs_node = eval_inside_arg(tag.lhs);
    return lower_inside_tuple(lhs_node, tag.items, std::make_index_sequence<sizeof...(Items)>{});
  }

  template <typename Integer, typename DistInt>
  result_type operator()(boost::proto::tag::function, boost::proto::terminal<operator_dist>::type const&,
                         WriteReference<Integer> const& var_term, distribution_tag<DistInt> const& dist_expr) {
    typedef typename distribution_value_type<DistInt>::type DistValue;
    distribution<DistValue> const dist = value_distribution(boost::proto::value(dist_expr));
    unsigned width = bitsize_traits<DistInt>::value;
    bool sign = crave::is_signed<DistInt>::value;

    unsigned id = new_var_id();
    result_type tmp_var = new_var(id, width, sign);
    std::shared_ptr<crave::ReferenceExpression> ref_expr(new DistReferenceExpr<DistInt>(dist, tmp_var));
    dist_references_.push_back(std::make_pair(id, ref_expr));
    if (dist.isImplicit())
      implicit_dist_references_.insert(id);
    else
      user_dist_references_.insert(id);

    result_type in_ranges;
    for(weighted_range<DistValue> const & r : dist.ranges()) {
      result_type left(new Constant(r.left_, width, sign));
      result_type right(new Constant(r.right_, width, sign));
      result_type left_cond(new LessEqualOpr(left, tmp_var));
      result_type right_cond(new LessEqualOpr(tmp_var, right));
      result_type in_range(new LogicalAndOpr(left_cond, right_cond));
      result_type tmp(in_ranges != 0 ? new LogicalOrOpr(in_ranges, in_range) : in_range);
      in_ranges = tmp;
    }

    result_type var_equal_tmp(new EqualOpr(boost::proto::eval(var_term, *this), tmp_var));
    dist_ref_to_var_map[id] = var_term.id();
    return dist.ranges().size() > 0 ? new LogicalAndOpr(var_equal_tmp, in_ranges) : var_equal_tmp;
  }
  
   template <typename Integer, typename DistInt>
  result_type operator()(boost::proto::tag::function, boost::proto::terminal<operator_dist>::type const&,
                         Variable<Integer> const& var_term, distribution_tag<DistInt> const& dist_expr) {
    typedef typename distribution_value_type<DistInt>::type DistValue;
    distribution<DistValue> const dist = value_distribution(boost::proto::value(dist_expr));
    unsigned width = bitsize_traits<DistInt>::value;
    bool sign = crave::is_signed<DistInt>::value;

    unsigned id = new_var_id();
    result_type tmp_var = new_var(id, width, sign);
    std::shared_ptr<crave::ReferenceExpression> ref_expr(new DistReferenceExpr<DistInt>(dist, tmp_var));
    dist_references_.push_back(std::make_pair(id, ref_expr));
    user_dist_references_.insert(id);

    result_type in_ranges;
    for(weighted_range<DistValue> const & r : dist.ranges()) {
      result_type left(new Constant(r.left_, width, sign));
      result_type right(new Constant(r.right_, width, sign));
      result_type left_cond(new LessEqualOpr(left, tmp_var));
      result_type right_cond(new LessEqualOpr(tmp_var, right));
      result_type in_range(new LogicalAndOpr(left_cond, right_cond));
      result_type tmp(in_ranges != 0 ? new LogicalOrOpr(in_ranges, in_range) : in_range);
      in_ranges = tmp;
    }

    result_type var_equal_tmp(new EqualOpr(boost::proto::eval(var_term, *this), tmp_var));
    dist_ref_to_var_map[id] = var_term.id();
    return dist.ranges().size() > 0 ? new LogicalAndOpr(var_equal_tmp, in_ranges) : var_equal_tmp;
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::function, boost::proto::terminal<operator_foreach>::type const&,
                         Expr1 const& e1, Expr2 const& e2) {
    return new ForEach(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Expr>
  result_type operator()(boost::proto::tag::function, boost::proto::terminal<operator_unique>::type const&,
                         Expr const& e) {
    return new Unique(boost::proto::eval(e, *this));
  }

  template <typename Expr1, typename Expr2>
  result_type operator()(boost::proto::tag::subscript, Expr1 const& e1, Expr2 const& e2) {
    return new VectorAccess(boost::proto::eval(e1, *this), boost::proto::eval(e2, *this));
  }

  template <typename Integer1, typename Integer2, typename VarTerm,
            typename std::enable_if<is_single_var_lhs<typename std::decay<VarTerm>::type>::value, int>::type = 0>
  result_type operator()(boost::proto::tag::function, boost::proto::terminal<operator_bitslice>::type const&,
                         Integer1 const& r, Integer2 const& l, VarTerm const& var_term) {
    typedef typename is_single_var_lhs<typename std::decay<VarTerm>::type>::integer_type Integer3;
    unsigned int rb = static_cast<unsigned int>(boost::proto::value(r));
    unsigned int lb = static_cast<unsigned int>(boost::proto::value(l));
    if ((rb < lb) || (rb >= bitsize_traits<Integer3>::value)) {
      throw std::runtime_error("Invalid range of bitslice");
    }
    return new Bitslice(boost::proto::eval(var_term, *this), rb, lb);
  }

 private:
  std::map<int, result_type>& variables_;
  std::map<int, result_type>& vector_variables_;
  std::vector<ReadRefPair>& read_references_;
  std::vector<WriteRefPair>& write_references_;
  std::vector<ReadRefPair>& dist_references_;
  std::map<int, int>& dist_ref_to_var_map;
  std::set<int>& implicit_dist_references_;
  std::set<int>& user_dist_references_;

  typedef std::map<std::pair<void const*, std::type_index>, std::function<result_type()> > BoundReferences;
  BoundReferences bound_references_;
  std::set<int> referenced_variables_;
};
// Context

}  // namespace crave
//  vim: ft=cpp:ts=2:sw=2:expandtab

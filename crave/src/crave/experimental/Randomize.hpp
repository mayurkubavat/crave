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
#include <memory>
#include <type_traits>
#include <vector>

#include "../backend/Generator.hpp"
#include "../frontend/Distribution.hpp"
#include "../frontend/WriteReference.hpp"

namespace crave {

namespace detail {

/**
 * a random value of type T, all values of T have the same probability
 */
template <typename T, typename Enable = void>
struct scope_random_value {
  // e.g. SystemC data types, which take the low bits of a 64-bit value
  static T get() {
    distribution<uint64_t> dist;
    T value;
    value = dist.nextValue();
    return value;
  }
};

template <typename T>
struct scope_random_value<T, typename std::enable_if<std::is_integral<T>::value>::type> {
  static T get() {
    distribution<T> dist;
    return dist.nextValue();
  }
};

/**
 * a variable of a scope randomization
 */
struct scope_variable_base {
  virtual ~scope_variable_base() {}
  virtual void bind(Generator& gen) const = 0;
  virtual void restore() = 0;
};

template <typename T>
struct scope_variable : scope_variable_base {
  explicit scope_variable(T& var) : var_(var), saved_(var), ref_(&var) {}

  void bind(Generator& gen) const override { gen.bindReference(var_, ref_); }

  void restore() override { var_ = saved_; }

  T& var_;
  T saved_;
  WriteReference<T> ref_;
};

}  // namespace detail

/**
 * \ingroup newAPI
 * \brief The randomization of variables in a scope, see crave::randomize().
 */
class scope_randomization {
 public:
  scope_randomization() {}
  scope_randomization(scope_randomization&&) = default;
  scope_randomization& operator=(scope_randomization&&) = default;

  /**
   * \brief Randomizes the variables again, with the given constraints.
   *
   * As std::randomize() with {...} in SystemVerilog, the variables of the scope randomization are random variables
   * in the constraints, where they are referred to with crave::reference(). Other variables referred to with
   * crave::reference() keep their values. If the constraints have no solution, the variables are set back to the
   * values they had before crave::randomize() was called.
   *
   * \param exprs constraint expressions
   * \return true if a solution is found, false otherwise.
   */
  template <typename... Exprs>
  bool with(Exprs... exprs) {
    Generator gen;
    for (std::unique_ptr<detail::scope_variable_base> const& v : vars_) v->bind(gen);
    add(gen, exprs...);
    bool solved = false;
    try {
      solved = gen.next();
    } catch (...) {
      restore();
      throw;
    }
    if (!solved) restore();
    return solved;
  }

  /**
   * \brief Whether the variables have been randomized, which is always the case without constraints.
   */
  operator bool() const { return true; }

  /**
   * \internal
   * Adds a variable and gives it a random value.
   */
  template <typename T>
  void add(T& var) {
    vars_.push_back(std::unique_ptr<detail::scope_variable_base>(new detail::scope_variable<T>(var)));
    var = detail::scope_random_value<T>::get();
  }

 private:
  scope_randomization(scope_randomization const&) = delete;
  scope_randomization& operator=(scope_randomization const&) = delete;

  void restore() {
    for (std::unique_ptr<detail::scope_variable_base> const& v : vars_) v->restore();
  }

  static void add(Generator&) {}

  template <typename Expr, typename... Exprs>
  static void add(Generator& gen, Expr expr, Exprs... exprs) {
    gen(expr);
    add(gen, exprs...);
  }

  std::vector<std::unique_ptr<detail::scope_variable_base> > vars_;
};

/**
 * \ingroup newAPI
 * \brief Randomizes variables, which are not members of a crv_sequence_item, as std::randomize() in SystemVerilog.
 *
 * <p>As in IEEE 1800-2017 18.12, the arguments are the variables to be randomized, e.g. local variables or
 * references. They can be of the builtin integer types, bool, or the SystemC data types supported by
 * crv_variable. Each of them gets a random value, all values of its type have the same probability, and the
 * randomization succeeds.</p>
 * <p>The function with() of the result randomizes the variables with constraints instead, as std::randomize() with
 * {...}. In the constraints, the variables are referred to with crave::reference(), which must appear directly in
 * the arguments of with(): a crave::expression made beforehand keeps the values of the variables. Other variables
 * referred to with crave::reference() are state variables. If the constraints have no solution, with() returns
 * false and leaves the variables unchanged.</p>
 *
 * \code
 * int x = 0;
 * sc_dt::sc_uint<4> y = 0;
 * bool ok = crave::randomize(x, y);  // any values
 * ok = crave::randomize(x, y).with(reference(x) < reference(y), reference(x) > 2);
 * \endcode
 *
 * Inside a crv_sequence_item, call it as crave::randomize(), crv_sequence_item::randomize() hides it otherwise.
 *
 * \param vars the variables to be randomized
 * \return the randomization, which converts to true, or randomizes the variables with constraints by with()
 */
template <typename... Ts>
scope_randomization randomize(Ts&... vars) {
  scope_randomization result;
  int expand[] = {0, (result.add(vars), 0)...};
  (void)expand;
  return result;
}

}  // namespace crave

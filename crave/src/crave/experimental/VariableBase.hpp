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

#include <stdexcept>
#include <string>

#include "../frontend/Constraint.hpp"
#include "../backend/Randc.hpp"
#include "../ir/UserExpression.hpp"
#include "Object.hpp"

namespace crave {
/**
 * base class for crv_variable
 */
struct crv_variable_base_ : public crv_object {
  /**
  * get an instance of Constant, which captures the current value of this variable.
  * @return a Constant
  */
  virtual Constant constant_expr() = 0;

  /**
   * get an expression, which bounds the symbolic value to be equal to another symbolic value.
   * @return bound expression
   */
  virtual expression bound_expr() = 0;

  /**
   * get id
   * @return id
   */
  virtual unsigned id() = 0;

  std::string obj_kind() const override final { return "crv_variable"; }

  /**
   * \brief Enables or disables the randomization of this variable.
   *
   * As rand_mode() of a random variable in SystemVerilog, a variable whose randomization is
   * disabled is not randomized: it keeps its value and constraints treat it as a state variable.
   * Any soft constraint that contradicts the current value of such a variable is dynamically
   * dropped during solving and re-activates when the value becomes compatible again.
   *
   * \param on true to enable, false to disable the randomization.
   */
  void rand_mode(bool on) override {
    // the hooks of the objects, which this variable belongs to, are called again when it is randomized
    if (on)
      for (crv_object* obj = parent_; obj; obj = obj->parent_) obj->object_rand_mode_ = true;
    if (rand_mode_ == on) return;
    rand_mode_ = on;
    if (randc_) set_randc(id(), rand_mode_);
    request_rebuild();
  }

  /**
   * \brief Get whether the randomization of this variable is enabled.
   * \return true if this variable is randomized, false otherwise.
   */
  bool rand_mode() const { return rand_mode_; }

  /**
   * \brief Get whether this variable is a cyclic random variable, see crv_variable_base<T>::randc(bool).
   * \return true if this variable is cyclic, false otherwise.
   */
  bool randc() const { return randc_; }

  void recursive_build(Generator& gen) const override;

 protected:
  /**
   * get an expression, which bounds the symbolic value to the current value.
   * @return state expression
   */
  virtual expression state_expr() const = 0;

  bool rand_mode_ = true;
  bool randc_ = false;
};


struct prev_ {};
/*!
 * \ingroup newAPI
 * \brief A symbol to refer the previously generated value.
 *
 * crave::prev allows you to access the value the crv_variable had before calling randomize().
 * You can use this variable in the parenthese operator () as an argument. 
 */
extern prev_ prev;

template <typename T>
class crv_variable_base : public crv_variable_base_ {
 public:
  operator T() const { return actual_value(); }

  /**
   * stream operator for crv_variable
   */
  friend std::ostream& operator<<(std::ostream& os, const crv_variable_base<T>& e) {
    os << e.actual_value();
    return os;
  }

  /**
   * get WriteReference
   * @return WriteReference
   */
  WriteReference<T> const& operator()() const { return var; }

  /**
   * get ReadReference to the current (i.e. last generated) value, the only supported argument is crave::prev.
   * @return ReadReference
   */
  template <class Q>
  ReadReference<T> const& operator()(Q const&) const {
    static_assert(std::is_same<Q, prev_>::value, "only prev is supported");
    return ref;
  }

  /**
   * get id
   * @return id
   */
  unsigned id() override { return var.id(); }

  /**
   * Bind this variable to another variable.
   * The binding influences the functions bound_expr() and actual_value().
   */
  void bind(crv_variable_base& other) { bound_var = &other; }

  /**
   * Unbind
   */
  void unbind() { bound_var = nullptr; }

  /**
   * get the current value of the crv_variable
   * @return value
   */
  T get() const {
	  return this->actual_value();
  }

  expression bound_expr() override {
    return bound_var ? make_expression(var == bound_var->var) : value_to_expression(true);
  }

  Constant constant_expr() override { return to_constant_expr<T>()(actual_value()); }

  /**
   * \brief Makes this variable a cyclic random variable, as randc in SystemVerilog, or a random variable again.
   *
   * As in IEEE 1800-2017 18.4.2, a cyclic random variable takes all values, which satisfy its constraints, in a
   * random order before a value is repeated. The cycle starts again when the constraints of the variable change,
   * for instance by randomize_with() with other constraints than the previous randomization, by enabling or
   * disabling a constraint, or by rand_mode(), and when none of the remaining values satisfies the
   * constraints. As in SystemVerilog, the value of a randc variable is chosen before the other variables are
   * solved, among the values, for which the constraints have a solution. A variable whose randomization is
   * disabled with rand_mode(false) keeps its value, it is not cyclic until its randomization is enabled again.
   *
   * The variable must not have more than crave::randc_max_bitsize (16) bits, see Generator::randc() for details.
   *
   * \param on true for a cyclic random variable, false for a random variable.
   */
  void randc(bool on) {
    if (on && bitsize_traits<T>::value > randc_max_bitsize)
      throw std::runtime_error("A randc variable must not have more than " + std::to_string(randc_max_bitsize) +
                               " bits.");
    if (randc_ == on) return;
    randc_ = on;
    set_randc(id(), randc_ && rand_mode_);
    request_rebuild();
  }

  using crv_variable_base_::randc;

  ~crv_variable_base() {
    if (randc_) set_randc(var.id(), false);
  }

 protected:
  expression state_expr() const override { return make_expression(var == ref); }

  crv_variable_base() : var(&value), ref(value), value(), bound_var() {}
  crv_variable_base(const crv_variable_base& other)
      : crv_variable_base_(other), var(&value), ref(value), value(other.value), bound_var(other.bound_var) {
    if (randc_) set_randc(var.id(), rand_mode_);
  }

  /**
   * get the current value
   * @return value
   */
  T actual_value() const { return bound_var ? bound_var->value : value; }

  WriteReference<T> var;
  ReadReference<T> ref;
  T value;
  crv_variable_base* bound_var;
};
}

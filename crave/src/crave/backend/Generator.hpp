//****************************************************************************************
//	MIT License
//****************************************************************************************
//	Copyright (c) 2012-2020 University of Bremen, Germany. 
//  	Copyright (c) 2015-2020 DFKI GmbH Bremen, Germany.
//  	Copyright (c) 2020 Johannes Kepler University Linz, Austria.
//    Copyright (c) 2022 - 2023 Coseda Technologies GmbH.
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
#include <vector>
#include "../ir/UserConstraint.hpp"
#include "VariableGenerator.hpp"
#include "../backend/VectorGenerator.hpp"
#include "Randc.hpp"
#include "SolveOrder.hpp"

#include <stdexcept>

namespace crave {

/*!
 * \brief Backend generator to generate random values with constraints.
 *
 * Try to avoid usage of this class directly.
 * CRAVE usually uses this class internally.
 */
struct Generator {
 public:
  Generator()
    : constr_mng_(),
      var_ctn_(variable_container()),
      ctx_(var_ctn_),
      var_gen_(new VariableGenerator(*var_ctn_)),
      var_cov_gen_(*var_ctn_),
      vec_gen_(),
      covered_(false),
      cov_reset_needed_(false),
      order_changed_(false) {}

  template <typename Expr>
  explicit Generator(Expr expr)
    : constr_mng_(),
      var_ctn_(variable_container()),
      ctx_(var_ctn_),
      var_gen_(new VariableGenerator(*var_ctn_)),
      var_cov_gen_(*var_ctn_),
      vec_gen_(),
      covered_(false),
      cov_reset_needed_(false),
      order_changed_(false) {
    (*this)(expr);
  }

  virtual ~Generator() { delete var_gen_; }

  void enable_multithreading();

  template <typename Expr>
  Generator& operator()(Expr expr) {
    constr_mng_.makeConstraint(expr, &ctx_);
    return *this;
  }

  template <typename Expr>
  Generator& operator()(soft_expr<Expr> expr) {
    return soft(expr.expr);
  }

  template <typename Expr>
  Generator& operator()(std::string constraint_name, Expr expr) {
    constr_mng_.makeConstraint(constraint_name, expr, &ctx_);
    return *this;
  }

  template <typename Expr>
  Generator& operator()(std::string constraint_name, soft_expr<Expr> expr) {
    return soft(constraint_name, expr.expr);
  }

  template <typename Expr>
  Generator& soft(Expr e) {
    constr_mng_.makeConstraint(e, &ctx_, true);
    return *this;
  }

  template <typename Expr>
  Generator& soft(soft_expr<Expr> e) {
    return soft(e.expr);
  }

  template <typename Expr>
  Generator& soft(std::string name, Expr e) {
    constr_mng_.makeConstraint(name, e, &ctx_, true);
    return *this;
  }

  template <typename Expr>
  Generator& soft(std::string name, soft_expr<Expr> e) {
    return soft(name, e.expr);
  }

  template <typename Expr>
  Generator& cover(Expr e) {
    constr_mng_.makeConstraint(e, &ctx_, false, true);
    return *this;
  }

  template <typename Expr>
  Generator& cover(std::string name, Expr e) {
    constr_mng_.makeConstraint(name, e, &ctx_, false, true);
    return *this;
  }

  /**
   * \brief Makes a variable a cyclic random variable, as randc in SystemVerilog.
   *
   * As in IEEE 1800-2017 18.4.2, the variable takes all values, which satisfy the constraints, in a random order
   * before a value is repeated. The cycle starts again when the constraints of the variable change, for
   * instance by enabling or disabling a constraint or by adding one, and when none of the remaining values
   * satisfies the constraints. As in SystemVerilog, the value of a randc variable is chosen before the other
   * variables are solved, among the values, for which the hard constraints have a solution. A soft constraint,
   * which contradicts the chosen value, is dropped. A randc variable must not have a distribution by dist(),
   * the generation throws std::runtime_error otherwise.
   *
   * The variable must not have more than crave::randc_max_bitsize (16) bits. The cycle is a uniformly random
   * permutation if at most crave::randc_max_listed (256) values satisfy the constraints, which includes every
   * variable of up to 8 bits. The cycle is kept for the variable, also if it is generated by another Generator with the same
   * constraints. The coverage driven generation by nextCov() and cover constraints does not use the cycle.
   *
   * The state of the cycle of a Variable, up to 512 KB for a 16-bit variable, is kept until randc(var, false) is
   * called, also after the variable and the generator have been destroyed, since the copies of a Variable share
   * its identity. A crv_variable releases the state of its cycle when it is destroyed.
   *
   * \param var the variable, a Variable or a WriteReference.
   * \param on true for a cyclic random variable, false for a random variable again.
   * \return this generator
   */
  template <typename Var>
  Generator& randc(Var const& var, bool on = true) {
    if (on && bitsize_traits<typename Var::value_type>::value > randc_max_bitsize)
      throw std::runtime_error("A randc variable must not have more than " + std::to_string(randc_max_bitsize) +
                               " bits.");
    set_randc(var.id(), on);
    // the variable is generated, also if no other constraint refers to it
    if (on) (*this)(var == var);
    return *this;
  }

  /**
   * \brief Solves a variable before another one, as solve ... before ... in SystemVerilog.
   *
   * As in IEEE 1800-2017 18.5.10, the ordering changes the probability distribution of the solutions, but not the
   * set of solutions. The value of the variable before is chosen first, with equal probability among the values,
   * for which the constraints have a solution, and the value of the variable after is solved for it. For instance,
   * with the constraint <code>if_then(s, d == 0)</code> on a bool s and an unsigned d, s is true with probability
   * 1/2 when s is solved before d, and with probability of about 2^-32 otherwise.
   *
   * The orderings of several calls are combined, they must not be cyclic. The variables, which are not ordered
   * before others, are solved together after them. Variables in the same level of the ordering, e.g. a and b for
   * <code>solve_before(a, c).solve_before(b, c)</code>, are chosen together, with equal probability among the
   * combinations of their values, for which the constraints have a solution. A variable with a distribution by
   * dist() takes the values of its distribution, for which the constraints have a solution.
   *
   * The values are chosen at random, between the least and the greatest value, which have a solution, and found
   * by bisection, see crave::solve_before_max_tries. If none of them has a solution, the solver lists the values,
   * which have one. If there are crave::solve_before_max_listed or more of them, so sparse that the values tried
   * at random miss them, a value cannot be chosen with equal probability at a bounded cost: the variables of the
   * level are then solved together with the variables, which are not ordered, as without the ordering. The bounds
   * and the listed values of the first level, and whether it has too many values, are kept for the next
   * generation, unless they depend on references, randc variables or distributions.
   *
   * solve_before() throws std::invalid_argument if before and after are the same variable. The generation throws
   * std::runtime_error if the ordering is cyclic or contains a cyclic random variable (randc). The ordering is not
   * used by the coverage driven generation of nextCov() for cover constraints.
   *
   * \param before the variable, which is solved first, a Variable or a WriteReference.
   * \param after the variable, which is solved afterwards.
   * \return this generator
   */
  template <typename Before, typename After>
  Generator& solve_before(Before const& before, After const& after) {
    addSolveOrder(before.id(), after.id());
    return *this;
  }

  /**
   * \internal
   * Solves the variable with the id before before the variable with the id after, see solve_before().
   */
  void addSolveOrder(int before, int after);

  bool enableConstraint(std::string const& name);

  bool disableConstraint(std::string const& name);

  bool isConstraintEnabled(std::string const& name);

  bool isChanged();

  Generator& operator()();

  void merge(const Generator& other);

  void reset();

  void rebuild(bool selfInclude = false);

  bool next();

  bool nextCov();

  bool isCovered();

  void resetCoverage();

  std::ostream& printDotGraph(std::ostream& os, bool root = true);

  std::vector<std::vector<std::string> > analyseContradiction();

  std::vector<std::string> getInactiveSofts();

  template <typename T>
  T operator[](Variable<T> const& var) {
    T result;
    if (!var_gen_->read(var, &result)) {
      throw std::runtime_error("Invalid variable read request.");
    }
    return result;
  }

  void addVecId(int id) { to_be_generated_vec_ids_.insert(id); }

  VariableContainer* getVarCtn ();

  /**
   * \internal
   * Makes crave::reference(object) in the constraints, which are added to this generator afterwards, refer to the
   * random variable var instead of the value of the object, see crave::randomize().
   */
  template <typename T>
  void bindReference(T const& object, WriteReference<T> const& var) {
    Context* ctx = &ctx_;
    WriteReference<T> ref(var);
    ctx_.bindReference(object, [ctx, ref]() { return boost::proto::eval(ref, *ctx); });
  }

  /**
   * \internal
   * Makes the variables with values, e.g. the members of a crv_sequence_item, which the constraints added since
   * bindReference() refer to, state variables, which keep their current values, unless their ids are in
   * random_ids. See crave::randomize().
   */
  void keepValues(std::set<int> const& random_ids) {
    for (int id : ctx_.referenced_variables()) {
      if (random_ids.count(id)) continue;
      for (VariableContainer::WriteRefPair const& pair : var_ctn_->write_references) {
        if (pair.first != id) continue;
        NodePtr state(new EqualOpr(var_ctn_->variables[id], new Constant(pair.second->value_as_constant())));
        constr_mng_.makeConstraint(state, &ctx_);
        break;
      }
    }
  }

 private:
  void resetConstraints();

 private:
  // constraints
  ConstraintManager constr_mng_;
  ConstraintPartitioner constr_pttn_;

  // variables
  VariableContainer* var_ctn_;
  std::set<int> to_be_generated_vec_ids_;

  // context
  Context ctx_;

  // variable solvers
  VariableGenerator* var_gen_;
  VariableCoverageGenerator var_cov_gen_;

  // vector solver
  VectorGenerator vec_gen_;

  // coverage
  bool covered_;
  bool cov_reset_needed_;

  // the solve-before ordering of this generator, and the one of the merged generators
  SolveOrder solve_order_;
  SolveOrder merged_order_;
  bool order_changed_;
};

}  // namespace crave
//  vim: ft=cpp:ts=2:sw=2:expandtab

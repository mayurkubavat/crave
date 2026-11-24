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

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "../crave/ir/Node.hpp"
#include "../crave/ir/visitor/metaSMTNodeVisitor.hpp"

namespace crave {

/**
 * \brief Samples the solutions of constraints on a single variable with a BDD.
 *
 * A BDD needs its own CUDD manager, which is expensive in terms of memory, and the BDD of
 * a constraint can be expensive to build. Samplers are therefore shared: constraints of
 * the same shape, i.e. which only differ in the variable they constrain, use the same
 * sampler. This is the common case for the objects of a class and the elements of a vector.
 */
class BddSampler {
 public:
  /**
   * \brief Get the sampler for the conjunction of the given constraints.
   *
   * All constraints have to refer to the same single variable. Constraints, which are too
   * complex according to VariableDefaultSolver::complexity_limit_for_bdd, are left out.
   *
   * \param create If false, only a sampler which already exists is returned.
   * \return The sampler, or a null pointer if there is none. This is the case if CRAVE has been
   * built without CUDD, if no constraint is left, if the constraints have no solution, or if the
   * BDD would need more than max_nodes nodes.
   *
   * Getting a sampler leaves the random number generator of CRAVE unchanged, so the values generated
   * afterwards do not depend on whether the sampler existed before.
   */
  static std::shared_ptr<BddSampler> get(std::vector<NodePtr> const& constraints, bool create = true);

  /**
   * \brief Pick one of the solutions with equal probability.
   * \param bits The value of the variable as a string of bits, most significant bit first.
   */
  bool sample(std::string* bits);

  /** number of samplers which currently exist */
  static std::size_t count();

  /** limit for the number of nodes of a BDD */
  static unsigned max_nodes;
  /** number of the most recently requested samplers, which are kept although they are not in use */
  static unsigned max_recent;

 private:
  BddSampler() {}

  SolverPtr solver_;
  NodePtr var_;

  static std::mutex mutex_;
  static std::map<std::string, std::weak_ptr<BddSampler> > samplers_;
  // shapes for which no sampler can be built, so that this is not tried again and again
  static std::set<std::string> failed_;
  static unsigned failed_max_nodes_;  // the value of max_nodes failed_ is valid for
  static const std::size_t max_failed_ = 100000;
  static std::list<std::shared_ptr<BddSampler> > recent_;
};

}  // namespace crave

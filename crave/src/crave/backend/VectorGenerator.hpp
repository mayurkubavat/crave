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

#include <vector>
#include <map>
#include <memory>
#include <set>

#include "VariableGenerator.hpp"
#include "../ir/visitor/ReplaceVisitor.hpp"

namespace crave {

class BddSampler;

/*
 *
 */
struct VectorSolver {
  typedef boost::intrusive_ptr<VariableExpr> VariablePtr;
  typedef std::vector<VariablePtr> VectorElements;

  explicit VectorSolver(int vector_id);

  void addConstraint(VectorConstraintPtr vc);
  bool solve(const VariableGenerator& var_gen);

 private:
  void resetSolver(unsigned int const size);
  void buildSolver(unsigned int const size);
  void makeSuggestions();

 private:
  std::vector<VectorConstraintPtr> constraints_;
  int vector_id_;
  SolverPtr solver_;
  VectorElements vec_elements_;
  // hard constraints, which involve only a single element, for every element
  std::vector<std::vector<NodePtr> > elem_constraints_;
  // BDD samplers for the solutions of these constraints, by variable id (null if there is none)
  std::map<int, std::shared_ptr<BddSampler> > elem_samplers_;
  // the different samplers in elem_samplers_
  std::set<BddSampler*> samplers_;
};

/*
 *
 */
struct VectorGenerator {
  typedef std::map<int, VectorSolver> VectorSolverMap;

  VectorGenerator();
  bool solve(const VariableGenerator& var_gen, const std::set<int>& vec_ids);
  void reset(const std::vector<VectorConstraintPtr>& v);

 private:
  void addConstraint(VectorConstraintPtr vc);

 private:
  VectorSolverMap vector_solvers_;
};

}  // namespace crave
//  vim: ft=cpp:ts=2:sw=2:expandtab

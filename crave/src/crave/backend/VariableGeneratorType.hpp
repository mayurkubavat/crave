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

#include <set>
#include <string>
#include <vector>

#include "VariableSolver.hpp"
#include "SolveOrder.hpp"

namespace crave {

/**
     *
     */
struct VariableGenerator {
  typedef std::shared_ptr<VariableSolver> VarSolverPtr;

  explicit VariableGenerator(const VariableContainer& vcon);
  
  virtual ~VariableGenerator(){}

  virtual void reset(std::vector<ConstraintPartition>& partitions);

  virtual bool solve();

  /**
   * Sets the solve-before ordering of the variables for the solvers created by reset().
   * Throws std::runtime_error if the ordering is cyclic.
   */
  void setSolveOrder(SolveOrder const& order) { solve_levels_ = order.levels(); }

  template <typename T>
  bool read(const Variable<T>& var, T* value) const {
    auto ov = override_values_.find(var.id());
    if (ov != override_values_.end()) {
      AssignResultToRef<T> result(value);
      result.set_value(ov->second);
      return true;
    }
    for(VarSolverPtr vs : solvers_) {
      if (vs->read(var, value)) return true;
    }
    return false;
  }

  bool read(int id, std::string& value) const {
    auto ov = override_values_.find(id);
    if (ov != override_values_.end()) {
      value = ov->second;
      return true;
    }
    for (VarSolverPtr vs : solvers_) {
      if (vs->read(id, value)) return true;
    }
    return false;
  }

  void setOverride(int id, std::string const& bits) {
    override_values_[id] = bits;
    for (VarSolverPtr const& vs : solvers_) {
      if (vs->containsVar(id)) vs->setOverride(id, bits);
    }
  }

  void clearOverrides() {
    override_values_.clear();
    for (VarSolverPtr const& vs : solvers_) vs->clearOverrides();
  }

  void undoRandc() { undoRandc(solvers_.size()); }
  void undoRandc(std::set<int> const& ids);
  std::string randcSignature(int id) const;

  std::vector<std::vector<std::string> > analyseContradiction();

  std::vector<std::string> getInactiveSofts() const;

 protected:
  // makes the solvers again, if a variable has been made cyclic or not since they have been made
  void refreshRandc();
  // gives the values back to the cycles of the randc variables, which the first count solvers have taken
  void undoRandc(std::size_t count);
  std::vector<ConstraintPartition>* partitions_ = nullptr;
  unsigned randc_epoch_ = 0;

  const VariableContainer& var_ctn_;
  std::vector<VarSolverPtr> solvers_;
  std::vector<std::vector<int> > solve_levels_;
  std::map<int, std::string> override_values_;
};
}  // namespace crave

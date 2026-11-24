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
#include <vector>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "VariableSolver.hpp"
#include "Randc.hpp"

namespace crave {

class BddSampler;

struct VariableDefaultSolver : VariableSolver {
  static bool bypass_constraint_analysis;
  static unsigned complexity_limit_for_bdd;

  static std::size_t analyse_hards_count();
  static void reset_analyse_hards_count();
  static std::size_t solver_build_count();
  static void reset_solver_build_count();

  /**
   * \param solve_levels the variables, which are solved before others, by level, see SolveOrder::levels()
   */
  VariableDefaultSolver(const VariableContainer& vcon, const ConstraintPartition& cp,
                        std::vector<std::vector<int> > const& solve_levels = std::vector<std::vector<int> >());

  bool solve() override;
  bool solveCover(std::set<std::string> const& skip_covers, std::string* hit_name) override;

  std::vector<std::vector<std::string> > getContradictions() const override;
  std::vector<std::string> getInactiveSofts() const override;

  void undoRandc() override;
  void undoRandc(std::set<int> const& ids) override;
  std::string randcSignature(int id) override;

 private:
  typedef std::map<int, std::shared_ptr<RandcState> > RandcMap;

  void analyseHards() const;
  void analyseSofts() const;
  bool readRefsUnchanged() const;
  void updateInactiveSofts(std::vector<unsigned int> const& inactive_indices) const;
  bool solveWith(std::vector<NodePtr> const& assumptions, RandcMap const& randc, bool ignore_softs = false);
  bool solveRandc(int id, RandcState& state, std::vector<NodePtr>& assumptions, RandcMap const& randc);
  void listWindows(NodePtr const& var, RandcState const& state, std::vector<NodePtr>& assumptions,
                   RandcMap const& randc, std::vector<uint64_t>& listed);
  std::string const& signature();
  bool solveOrdered(std::vector<NodePtr>& assumptions, RandcMap const& randc);
  bool solveLevel(std::size_t index, std::vector<NodePtr>& assumptions, RandcMap const& randc);
  bool solveUnordered(std::vector<int> const& level);
  bool feasible(std::vector<NodePtr> const& assumptions);
  uint64_t readKey(Node const& var);
  std::pair<uint64_t, uint64_t> keyBounds(NodePtr const& var, uint64_t key, std::vector<NodePtr>& assumptions);

  std::map<int, std::shared_ptr<BddSampler> > bdd_samplers_;
  std::map<int, std::vector<Constant> > inside_samplers_;
  std::set<int> replaced_dist_refs_;
  // values of cyclic random variables, which the last solve() has taken, and the state before a new cycle
  std::vector<RandcUndo> randc_undo_;
  std::vector<VariableContainer::WriteRefPair> random_write_refs_;
  std::string signature_;
  std::vector<ConstraintPtr> soft_constraints_;
  std::vector<std::vector<int> > solve_levels_;  // the levels of the solve-before ordering in this partition
  std::set<int> ordered_vars_;                   // the variables in solve_levels_
  std::vector<std::vector<Constant> > first_level_values_;  // the values of the first level, which have a solution
  std::vector<std::pair<uint64_t, uint64_t> > first_level_bounds_;  // the bounds of their keys
  bool first_level_unordered_ = false;  // whether the first level has too many sparse values to be ordered
  std::set<int> unordered_vars_;        // the ordered variables, which are solved without the ordering this time
  mutable bool hards_analysed_ = false;
  mutable bool softs_analysed_ = false;
  bool solve_attempted_ = false;
  mutable std::vector<Constant> analysed_read_refs_;
};
}  // namespace crave

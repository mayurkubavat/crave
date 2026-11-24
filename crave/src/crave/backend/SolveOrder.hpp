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
#include <utility>
#include <vector>

namespace crave {

/**
 * \brief The number of values tried at random for the variables of a level of a solve-before ordering, before the
 * least and the greatest of their values, which have a solution, are found.
 */
unsigned const solve_before_first_tries = 4;

/**
 * \brief The number of values tried at random between the least and the greatest value, which have a solution, of
 * the variables of a level of a solve-before ordering, see Generator::solve_before().
 */
unsigned const solve_before_max_tries = 64;

/**
 * \brief The maximal number of values, which the solver lists for the variables of a level of a solve-before
 * ordering, if the values tried at random have no solution.
 *
 * If fewer values have a solution, one of them is chosen with equal probability. Otherwise, since a value cannot be
 * chosen with equal probability at a bounded cost, the variables of the level are solved without the ordering.
 */
unsigned const solve_before_max_listed = 256;

/**
 * \internal
 * The solve-before ordering of variables, as solve ... before ... in SystemVerilog (IEEE 1800-2017 18.5.10).
 */
class SolveOrder {
 public:
  /**
   * Orders the variable with the id before before the variable with the id after.
   * Throws std::invalid_argument if both are the same variable.
   */
  void add(int before, int after);

  /** adds the orderings of another SolveOrder */
  void merge(SolveOrder const& other);

  void clear() { pairs_.clear(); }

  bool empty() const { return pairs_.empty(); }

  std::set<std::pair<int, int> > const& pairs() const { return pairs_; }

  /**
   * Gets the variables, which are ordered before other variables, by level: the variables of a level are only
   * ordered after the ones of the previous levels. A variable, which is not ordered before another one, is in
   * no level, it is solved together with the variables that are not ordered.
   * Throws std::runtime_error if the ordering is cyclic.
   */
  std::vector<std::vector<int> > levels() const;

 private:
  std::set<std::pair<int, int> > pairs_;
};

}  // namespace crave

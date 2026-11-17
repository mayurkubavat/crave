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


#include "../crave/backend/SolveOrder.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>

namespace crave {

void SolveOrder::add(int before, int after) {
  if (before == after) throw std::invalid_argument("A variable must not be solved before itself.");
  pairs_.insert(std::make_pair(before, after));
}

void SolveOrder::merge(SolveOrder const& other) { pairs_.insert(other.pairs_.begin(), other.pairs_.end()); }

std::vector<std::vector<int> > SolveOrder::levels() const {
  std::map<int, std::vector<int> > successors;
  std::map<int, unsigned> predecessors;
  for (std::pair<int, int> const& p : pairs_) {
    successors[p.first].push_back(p.second);
    predecessors[p.first];
    predecessors[p.second]++;
  }

  // the level of a variable is the length of the longest chain of variables ordered before it
  std::map<int, unsigned> level;
  std::vector<int> ready;
  for (std::pair<const int, unsigned> const& p : predecessors)
    if (p.second == 0) ready.push_back(p.first);
  std::size_t done = 0;
  while (!ready.empty()) {
    int id = ready.back();
    ready.pop_back();
    ++done;
    std::map<int, std::vector<int> >::const_iterator ite = successors.find(id);
    if (ite == successors.end()) continue;
    for (int succ : ite->second) {
      level[succ] = std::max(level[succ], level[id] + 1);
      if (--predecessors[succ] == 0) ready.push_back(succ);
    }
  }
  if (done != predecessors.size()) throw std::runtime_error("The solve-before ordering must not be cyclic.");

  std::vector<std::vector<int> > result;
  for (std::pair<const int, std::vector<int> > const& p : successors) {
    unsigned l = level[p.first];
    if (result.size() <= l) result.resize(l + 1);
    result[l].push_back(p.first);
  }
  return result;
}

}  // namespace crave

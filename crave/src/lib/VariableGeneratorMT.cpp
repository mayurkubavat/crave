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

#include "../crave/backend/VariableGeneratorMT.hpp"
#include "../crave/backend/VariableDefaultSolver.hpp"
#include "../crave/RandomSeedManager.hpp"
#include <cstdint>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace crave {
VariableGeneratorMT::VariableGeneratorMT(VariableContainer const& vcon) : VariableGenerator(vcon) {}

void VariableGeneratorMT::createNewSolver(ConstraintPartition& partition, unsigned int index) {
  static std::mutex solver_ctor_mutex;
  std::lock_guard<std::mutex> lock(solver_ctor_mutex);
  solvers_[index] = std::make_shared<VariableDefaultSolver>(var_ctn_, partition, solve_levels_);
}

void VariableGeneratorMT::reset(std::vector<ConstraintPartition>& partitions) {
  partitions_ = &partitions;
  randc_epoch_ = randc_epoch();
  solvers_.clear();
  solvers_.resize(partitions.size());
  override_values_.clear();
  std::vector<std::thread> threads;
  threads.reserve(partitions.size());
  for (unsigned i = 0; i < partitions.size(); i++) {
    threads.emplace_back(&VariableGeneratorMT::createNewSolver, this, std::ref(partitions.at(i)), i);
  }
  for (std::thread& thread : threads) thread.join();
}

void VariableGeneratorMT::solve(VarSolverPtr vs) {
  if (!result_.load()) return;
  if (!vs->solve()) result_.store(false);
}

bool VariableGeneratorMT::solve() {
  refreshRandc();
  clearOverrides();
  result_.store(true);
  std::vector<uint32_t> worker_seeds;
  bool const has_active = active_rng() != nullptr;
  if (has_active) {
    worker_seeds.reserve(solvers_.size());
    std::mt19937* parent_rng = active_rng();
    for (unsigned i = 0; i < solvers_.size(); ++i) {
      worker_seeds.push_back((*parent_rng)());
    }
  }
  std::vector<std::thread> threads;
  threads.reserve(solvers_.size());
  for (unsigned i = 0; i < solvers_.size(); i++) {
    if (!result_.load()) break;
    threads.emplace_back([i, this, has_active, &worker_seeds]() {
      if (has_active) {
        std::mt19937 worker_rng(worker_seeds[i]);
        ScopedRngGuard guard(&worker_rng);
        this->solve(this->solvers_.at(i));
      } else {
        this->solve(this->solvers_.at(i));
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (!result_.load()) {
    undoRandc(threads.size());
    for (VarSolverPtr const& vs : solvers_) vs->clearInactiveSofts();
  }
  return result_.load();
}

bool VariableGeneratorMT::nextCov(std::set<std::string> const& skip_covers, std::vector<std::string>* hit_names) {
  refreshRandc();
  clearOverrides();
  if (hit_names) hit_names->clear();
  result_.store(true);
  std::vector<uint32_t> worker_seeds;
  bool const has_active = active_rng() != nullptr;
  if (has_active) {
    worker_seeds.reserve(solvers_.size());
    std::mt19937* parent_rng = active_rng();
    for (unsigned i = 0; i < solvers_.size(); ++i) {
      worker_seeds.push_back((*parent_rng)());
    }
  }
  std::vector<std::string> hits(solvers_.size());
  std::vector<std::thread> threads;
  threads.reserve(solvers_.size());
  for (unsigned i = 0; i < solvers_.size(); i++) {
    if (!result_.load()) break;
    threads.emplace_back([i, this, &skip_covers, &hits, has_active, &worker_seeds]() {
      if (!result_.load()) return;
      if (has_active) {
        std::mt19937 worker_rng(worker_seeds[i]);
        ScopedRngGuard guard(&worker_rng);
        if (!this->solvers_.at(i)->solveCover(skip_covers, &hits[i])) result_.store(false);
      } else {
        if (!this->solvers_.at(i)->solveCover(skip_covers, &hits[i])) result_.store(false);
      }
    });
  }
  for (std::thread& thread : threads) thread.join();
  if (!result_.load()) {
    undoRandc(threads.size());
    for (VarSolverPtr const& vs : solvers_) vs->clearInactiveSofts();
    return false;
  }
  if (hit_names) {
    for (std::string const& h : hits) {
      if (!h.empty()) hit_names->push_back(h);
    }
  }
  return true;
}
}

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

#include "../crave/backend/Generator.hpp"

namespace crave {

void Generator::enable_multithreading() {
  delete var_gen_;
  var_gen_ = new VariableGeneratorMT(*var_ctn_);
  if (cov_reset_needed_) {
    if (!isChanged()) {
      try {
        var_gen_->setSolveOrder(merged_order_);
      } catch (...) {
        order_changed_ = true;
        throw;
      }
      var_gen_->reset(constr_pttn_.getPartitions());
    } else if (internal_size_exclusion_dirty_ && !constr_mng_.isChanged() && !order_changed_) {
      resetConstraints(true);
      rebuild(true);
    }
  }
}

bool Generator::enableConstraint(std::string const& name) { return constr_mng_.enableConstraint(name); }

bool Generator::disableConstraint(std::string const& name) { return constr_mng_.disableConstraint(name); }

bool Generator::isConstraintEnabled(std::string const& name) { return constr_mng_.isConstraintEnabled(name); }

bool Generator::isChanged() { return constr_mng_.isChanged() || order_changed_ || internal_size_exclusion_dirty_; }

void Generator::addSolveOrder(int before, int after) {
  solve_order_.add(before, after);
  order_changed_ = true;
}

Generator& Generator::operator()() {
  if (!next()) {
    throw std::runtime_error("Generator constraint unsatisfiable.");
  }
  return *this;
}

void Generator::merge(const Generator& other) {
  constr_pttn_.mergeConstraints(other.constr_mng_);
  merged_order_.merge(other.solve_order_);
}

void Generator::reset() {
  resetConstraints();
  to_be_generated_vec_ids_.clear();
}

void Generator::resetConstraints(bool keep_coverage) {
  constr_mng_.resetChanged();
  order_changed_ = false;
  internal_size_exclusion_dirty_ = false;
  constr_pttn_.reset();
  merged_order_.clear();
  if (!keep_coverage) resetCoverage();
}

std::set<int> Generator::activeVarIds_() const {
  std::set<int> active;
  for (ConstraintPartition const& cp : constr_pttn_.getPartitions()) {
    std::set<int> const& s = cp.supportSet();
    active.insert(s.begin(), s.end());
  }
  for (VectorConstraintPtr const& vc : constr_pttn_.getVectorConstraints()) {
    std::set<int> const& s = vc->supportVars();
    active.insert(s.begin(), s.end());
  }
  for (auto const& p : merged_order_.pairs()) {
    active.insert(p.first);
    active.insert(p.second);
  }
  for (auto const& p : solve_order_.pairs()) {
    active.insert(p.first);
    active.insert(p.second);
  }
  for (int vid : to_be_generated_vec_ids_) {
    active.insert(vid);
    auto vbm_it = vectorBaseMap.find(vid);
    if (vbm_it != vectorBaseMap.end()) {
      active.insert(static_cast<int>(vbm_it->second->size_var().id()));
    }
  }
  return active;
}

void Generator::snapshotCoveredReadRefs_() {
  covered_read_refs_.clear();
  if (!var_ctn_) return;
  std::set<int> const active = activeVarIds_();
  if (active.empty()) return;
  for (VariableContainer::ReadRefPair const& pair : var_ctn_->read_references) {
    if (!active.count(pair.first)) continue;
    covered_read_refs_.emplace_back(pair.first, pair.second->constantValue());
  }
}

bool Generator::readRefsChangedSinceCovered_() const {
  if (!var_ctn_ || covered_read_refs_.empty()) return false;
  std::set<int> const active = activeVarIds_();
  std::size_t idx = 0;
  for (VariableContainer::ReadRefPair const& pair : var_ctn_->read_references) {
    if (!active.count(pair.first)) continue;
    if (idx >= covered_read_refs_.size() || covered_read_refs_[idx].first != pair.first) return true;
    Constant cur = pair.second->constantValue();
    Constant const& prev = covered_read_refs_[idx].second;
    if (cur.value() != prev.value() || cur.bitsize() != prev.bitsize() || cur.sign() != prev.sign()) {
      return true;
    }
    ++idx;
  }
  return idx != covered_read_refs_.size();
}

void Generator::rebuild(bool selfInclude) {
  if (selfInclude) merge(*this);
  constr_pttn_.partition(var_ctn_);
  has_cover_ = false;
  std::set<std::string> cover_names;
  for (ConstraintPartition const& cp : constr_pttn_.getPartitions()) {
    for (ConstraintPtr const& c : cp) {
      if (c->isCover()) {
        has_cover_ = true;
        cover_names.insert(c->name());
      }
    }
  }
  total_cover_count_ = cover_names.size();
  if (!has_cover_) covered_ = true;
  // currently, every hard/soft/cover constraint is considered for
  // partitioning, this is suboptimal.
  try {
    var_gen_->setSolveOrder(merged_order_);
  } catch (...) {
    order_changed_ = true;  // the ordering is checked again by the next generation
    throw;
  }
  var_gen_->reset(constr_pttn_.getPartitions());
  vec_gen_.reset(constr_pttn_.getVectorConstraints());
  cov_reset_needed_ = true;
}

bool Generator::next() {
  if (isChanged()) {
    bool const external = constr_mng_.isChanged() || order_changed_;
    // keep the vectors without constraints, which have been added with addVecId()
    resetConstraints(!external);
    rebuild(true);
  }
  // The size of a vector is solved before its elements. If a constraint on a reduction of the elements does not
  // hold for the size, the other variables are solved again without that size, up to a bound of sizes.
  static unsigned const max_excluded_sizes = 64;
  std::vector<std::string> excluded;
  bool result = false;
  while (var_gen_->solve()) {
    if (vec_gen_.solve(*var_gen_, to_be_generated_vec_ids_, &constr_pttn_.getPartitions(), var_ctn_)) {
      result = true;
      break;
    }
    var_gen_->undoRandc();
    int id;
    unsigned size;
    if (excluded.size() >= max_excluded_sizes || !vec_gen_.failedSize(&id, &size)) break;
    std::string const name = "vector_" + std::to_string(id) + "_size_not_" + std::to_string(size);
    if (!constr_mng_.enableConstraint(name)) constr_mng_.makeConstraint(name, vectorBaseMap[id]->size_var() != size, &ctx_);
    excluded.push_back(name);
    resetConstraints(true);
    rebuild(true);
  }
  // the excluded sizes may hold after the constraints are changed
  for (std::string const& name : excluded) constr_mng_.disableConstraint(name);
  if (!excluded.empty()) {
    constr_mng_.resetChanged();
    internal_size_exclusion_dirty_ = true;
  }
  return result;
}

bool Generator::nextCov() {
  if (isChanged()) {
    bool const external = constr_mng_.isChanged() || order_changed_;
    // keep the vectors without constraints, which have been added with addVecId()
    resetConstraints(!external);
    rebuild(true);
  }
  if (covered_ && has_cover_ && covered_set_.size() < total_cover_count_ &&
      !covered_read_refs_.empty() && readRefsChangedSinceCovered_()) {
    covered_ = false;
    unachievable_covers_.clear();
  }
  static unsigned const max_excluded_sizes = 64;
  std::vector<std::string> excluded;
  std::set<std::string> last_coupled;
  std::set<int> last_coupled_vecs;
  std::set<std::string> temp_defer;
  bool result = false;
  bool solved_without_new_cover = false;
  auto unwind_excluded = [&]() {
    for (std::string const& name : excluded) constr_mng_.disableConstraint(name);
    excluded.clear();
    resetConstraints(true);
    rebuild(true);
  };
  auto isolate_coupled = [&](std::set<std::string> const& cset) {
    if (cset.size() > 1) {
      temp_defer.insert(std::next(cset.begin()), cset.end());
    } else {
      unachievable_covers_.insert(cset.begin(), cset.end());
      temp_defer.clear();
    }
  };
  while (!covered_) {
    std::set<std::string> skip = covered_set_;
    skip.insert(unachievable_covers_.begin(), unachievable_covers_.end());
    skip.insert(temp_defer.begin(), temp_defer.end());
    std::vector<std::string> hits;
    if (!var_gen_->nextCov(skip, &hits)) {
      if (!excluded.empty()) {
        unwind_excluded();
        if (!last_coupled.empty()) {
          isolate_coupled(last_coupled);
          last_coupled.clear();
          last_coupled_vecs.clear();
          continue;
        }
        last_coupled_vecs.clear();
      }
      result = false;
      break;
    }
    if (hits.empty()) {
      if (!excluded.empty()) {
        unwind_excluded();
        if (!last_coupled.empty()) {
          isolate_coupled(last_coupled);
          last_coupled.clear();
          last_coupled_vecs.clear();
          continue;
        }
        last_coupled_vecs.clear();
        continue;
      }
      if (!temp_defer.empty()) {
        temp_defer.clear();
        continue;
      }
      covered_ = true;
      snapshotCoveredReadRefs_();
      solved_without_new_cover = true;
      break;
    }
    std::set<std::string> active_covers(hits.begin(), hits.end());
    if (vec_gen_.solve(*var_gen_, to_be_generated_vec_ids_, &constr_pttn_.getPartitions(), var_ctn_, &active_covers)) {
      covered_set_.insert(hits.begin(), hits.end());
      result = true;
      break;
    }
    var_gen_->undoRandc();
    std::set<int> coupled_vecs;
    std::set<std::string> coupled = vec_gen_.coupledCovers(constr_pttn_.getPartitions(), hits, &coupled_vecs);
    bool same_component = false;
    for (int vid : coupled_vecs) {
      if (last_coupled_vecs.count(vid)) {
        same_component = true;
        break;
      }
    }
    int id;
    unsigned size;
    if (excluded.size() < max_excluded_sizes && vec_gen_.failedSize(&id, &size)) {
      std::string const name = "vector_" + std::to_string(id) + "_size_not_" + std::to_string(size);
      if (!constr_mng_.enableConstraint(name))
        constr_mng_.makeConstraint(name, vectorBaseMap[id]->size_var() != size, &ctx_);
      if (!same_component || last_coupled.empty()) {
        last_coupled = std::move(coupled);
        last_coupled_vecs = std::move(coupled_vecs);
      }
      excluded.push_back(name);
      resetConstraints(true);
      rebuild(true);
      continue;
    }
    if (!excluded.empty()) {
      unwind_excluded();
      std::set<std::string> target =
          (same_component && !last_coupled.empty()) ? std::move(last_coupled) : std::move(coupled);
      last_coupled.clear();
      last_coupled_vecs.clear();
      if (!target.empty()) {
        isolate_coupled(target);
        continue;
      }
      result = false;
      break;
    }
    last_coupled.clear();
    last_coupled_vecs.clear();
    if (coupled.empty()) {
      result = false;
      break;
    }
    isolate_coupled(coupled);
  }
  for (std::string const& name : excluded) constr_mng_.disableConstraint(name);
  if (!excluded.empty()) {
    constr_mng_.resetChanged();
    internal_size_exclusion_dirty_ = true;
  }
  if (result) return true;
  if (!covered_) return false;
  if (solved_without_new_cover) var_gen_->undoRandc();
  return next();
}

bool Generator::isCovered() { return covered_ && !constr_mng_.isChanged() && !order_changed_; }

void Generator::resetCoverage() {
  covered_ = false;
  cov_reset_needed_ = true;
  covered_set_.clear();
  unachievable_covers_.clear();
  covered_read_refs_.clear();
}

std::ostream& Generator::printDotGraph(std::ostream& os, bool root) {
  if (root) os << "digraph AST {" << std::endl;
  constr_mng_.printDotGraph(os);
  if (root) os << "}" << std::endl;
  return os;
}

std::vector<std::vector<std::string> > Generator::analyseContradiction() {
  if (isChanged()) {
    bool const external = constr_mng_.isChanged() || order_changed_;
    resetConstraints(!external);
    rebuild(true);
  }
  return var_gen_->analyseContradiction();
}

std::vector<std::string> Generator::getInactiveSofts() {
  if (constr_mng_.isChanged() || order_changed_) {
    resetConstraints();
    rebuild(true);
  }
  return var_gen_->getInactiveSofts();
}
VariableContainer* Generator:: getVarCtn () {return var_ctn_;}
}

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

#include "../crave/backend/VariableDefaultSolver.hpp"
#include "../crave/utils/Logging.hpp"
#include "RandomShuffle.hpp"

#include <atomic>
#include <set>
#include <functional>
#include <algorithm>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <typeinfo>
#include <random>
#include <unordered_map>

#include "BddSampler.hpp"
#include "../crave/backend/SolveOrder.hpp"
#include "../crave/RandomSeedManager.hpp"

namespace crave {

extern RandomSeedManager rng;
extern std::function<bool(void)> random_bit;

namespace {
unsigned canonicalId(std::unordered_map<unsigned, unsigned>& canon_ids, unsigned raw_id) {
  auto it = canon_ids.find(raw_id);
  if (it != canon_ids.end()) return it->second;
  unsigned const next = static_cast<unsigned>(canon_ids.size());
  canon_ids.emplace(raw_id, next);
  return next;
}

// writes the structure of an expression, which identifies the constraints of a cycle of a randc variable
void writeSignature(Node const& n, std::ostream& os, std::unordered_map<unsigned, unsigned>& canon_ids) {
  os << typeid(n).name();
  if (VariableExpr const* v = dynamic_cast<VariableExpr const*>(&n)) os << '#' << canonicalId(canon_ids, v->id());
  if (VectorExpr const* v = dynamic_cast<VectorExpr const*>(&n)) os << '#' << canonicalId(canon_ids, v->id());
  if (Placeholder const* p = dynamic_cast<Placeholder const*>(&n)) os << '#' << canonicalId(canon_ids, p->id());
  if (Terminal const* t = dynamic_cast<Terminal const*>(&n)) os << ':' << t->bitsize() << (t->sign() ? 's' : 'u');
  if (Constant const* c = dynamic_cast<Constant const*>(&n)) os << '=' << c->value();
  if (ExtendExpression const* e = dynamic_cast<ExtendExpression const*>(&n)) os << '+' << e->value();
  if (Bitslice const* b = dynamic_cast<Bitslice const*>(&n)) os << '[' << b->r() << ':' << b->l() << ']';
  if (Inside const* in = dynamic_cast<Inside const*>(&n)) {
    os << '{';
    for (Constant const& c : in->collection()) os << c.value() << ':' << c.bitsize() << (c.sign() ? 's' : 'u') << ',';
    os << '}';
  }
  os << '(';
  if (UnaryExpression const* u = dynamic_cast<UnaryExpression const*>(&n)) {
    writeSignature(*u->child(), os, canon_ids);
  } else if (BinaryExpression const* b = dynamic_cast<BinaryExpression const*>(&n)) {
    writeSignature(*b->lhs(), os, canon_ids);
    os << ',';
    writeSignature(*b->rhs(), os, canon_ids);
  } else if (TernaryExpression const* t = dynamic_cast<TernaryExpression const*>(&n)) {
    writeSignature(*t->a(), os, canon_ids);
    os << ',';
    writeSignature(*t->b(), os, canon_ids);
    os << ',';
    writeSignature(*t->c(), os, canon_ids);
  }
  os << ')';
}

// a constant of the value of a randc variable
Constant randcValue(VariableExpr const& var, uint64_t value) {
  if (var.isBool()) return Constant(value != 0);
  return Constant(value, var.bitsize(), false);
}

// the expression, which is true if the randc variable has one of the given values
NodePtr randcInside(NodePtr var, std::vector<uint64_t> const& values) {
  VariableExpr const& v = static_cast<VariableExpr const&>(*var);
  std::set<Constant> constants;
  for (uint64_t value : values) constants.insert(randcValue(v, value));
  return new Inside(var, constants);
}

void removeValue(std::vector<uint64_t>& values, uint64_t value) {
  std::vector<uint64_t>::iterator ite = std::find(values.begin(), values.end(), value);
  if (ite == values.end()) return;
  *ite = values.back();
  values.pop_back();
}

// sign extends the value of a variable to 64 bits, as Constant expects for a signed variable
uint64_t signExtend(VariableExpr const& var, uint64_t value) {
  unsigned const width = var.bitsize();
  if (var.sign() && !var.isBool() && width < 64 && ((value >> (width - 1)) & 1)) value |= ~uint64_t(0) << width;
  return value;
}

// a constant of the value of a variable, which the solver has read as a string of bits
Constant readValue(VariableExpr const& var, std::string const& str) {
  if (var.isBool()) return Constant(str == "true" || str == "1");
  uint64_t value = 0;
  for (char c : str) value = (value << 1) | (c == '1' ? 1 : 0);
  return Constant(signExtend(var, value), var.bitsize(), var.sign());
}

// a constant sampled from a BDD bit-string where don't-care bits ('X'/'-') are randomized
Constant sampleConstant(VariableExpr const& var, std::string const& str) {
  if (var.isBool()) {
    if (str == "true" || str == "1") return Constant(true);
    if (str == "false" || str == "0") return Constant(false);
    return Constant(random_bit());
  }
  uint64_t value = 0;
  for (char c : str) {
    value <<= 1;
    if (c == '1' || (c != '0' && random_bit())) value |= 1;
  }
  return Constant(signExtend(var, value), var.bitsize(), var.sign());
}

// the most significant bit of a signed variable, which is inverted to order its values as unsigned keys
uint64_t keySign(VariableExpr const& var) {
  return var.sign() && !var.isBool() ? uint64_t(1) << (var.bitsize() - 1) : 0;
}

// the key of a value of a variable, an unsigned number, which keeps the order of the values
uint64_t valueKey(VariableExpr const& var, Constant const& c) {
  if (var.isBool()) return c.value() ? 1 : 0;
  unsigned const width = var.bitsize();
  uint64_t const mask = width >= 64 ? ~uint64_t(0) : (uint64_t(1) << width) - 1;
  return (c.value() & mask) ^ keySign(var);
}

// the value of a variable with the given key
Constant keyValue(VariableExpr const& var, uint64_t key) {
  if (var.isBool()) return Constant(key != 0);
  return Constant(signExtend(var, key ^ keySign(var)), var.bitsize(), var.sign());
}

// a constant of a value of a variable, all values of the variable have the same probability
Constant randomValue(VariableExpr const& var) {
  if (var.isBool()) return Constant(random_unsigned(2) != 0);
  unsigned const width = var.bitsize();
  uint64_t value = with_active_or_global_rng([width](std::mt19937& gen) {
    uint64_t v = 0;
    for (unsigned bits = 0; bits < width; bits += 32) v = (v << 32) | gen();
    return v;
  });
  if (width < 64) value &= (uint64_t(1) << width) - 1;
  return Constant(signExtend(var, value), width, var.sign());
}

void startCycle(RandcState& state, unsigned width) {
  state.used.clear();
  state.listed.clear();
  state.complete = false;
  state.unused.resize(uint64_t(1) << width);
  for (uint64_t i = 0; i < state.unused.size(); ++i) state.unused[i] = i;
  state.started = true;
}
}  // namespace

namespace {
std::atomic<std::size_t> g_analyse_hards_count{0};
std::atomic<std::size_t> g_solver_build_count{0};
}  // namespace

bool VariableDefaultSolver::bypass_constraint_analysis = false;

unsigned VariableDefaultSolver::complexity_limit_for_bdd = 400;

std::size_t VariableDefaultSolver::analyse_hards_count() {
  return g_analyse_hards_count.load(std::memory_order_relaxed);
}

void VariableDefaultSolver::reset_analyse_hards_count() {
  g_analyse_hards_count.store(0, std::memory_order_relaxed);
}

std::size_t VariableDefaultSolver::solver_build_count() {
  return g_solver_build_count.load(std::memory_order_relaxed);
}

void VariableDefaultSolver::reset_solver_build_count() {
  g_solver_build_count.store(0, std::memory_order_relaxed);
}

VariableDefaultSolver::VariableDefaultSolver(const VariableContainer& vcon, const ConstraintPartition& cp,
                                             std::vector<std::vector<int> > const& solve_levels)
    : VariableSolver(vcon, cp) {
  g_solver_build_count.fetch_add(1, std::memory_order_relaxed);
  LOG(INFO) << "Create solver for partition " << constr_pttn_;

  // the ordering of the variables, which are not in this partition, does not change the distribution
  for (std::vector<int> const& level : solve_levels) {
    std::vector<int> ids;
    for (int id : level)
      if (var_ctn_.variables.count(id)) ids.push_back(id);
    if (ids.empty()) continue;
    solve_levels_.push_back(ids);
    ordered_vars_.insert(ids.begin(), ids.end());
  }

  for(ConstraintPtr c : constr_pttn_) {
    if (c->isCover()) continue;  // default solver ignores cover constraints
    if (c->isSoft()) {
      soft_constraints_.push_back(c);
      solver_->makeSoftAssertion(*c->expr());
    } else {
      solver_->makeAssertion(*c->expr());
    }
  }

  if (bypass_constraint_analysis) return;

  std::set<int> vars_with_dist;
  std::set<int> vars_with_user_dist;
  for(VariableContainer::ReadRefPair & pair : var_ctn_.dist_references) {
    if (!constr_pttn_.supportSet().count(pair.first)) continue;
    assert(var_ctn_.dist_ref_to_var_map.find(pair.first) != var_ctn_.dist_ref_to_var_map.end());
    int var_id = var_ctn_.dist_ref_to_var_map.at(pair.first);
    vars_with_dist.insert(var_id);
    if (var_ctn_.implicit_dist_references.count(pair.first) == 0) vars_with_user_dist.insert(var_id);
  }

  std::set<int> read_ref_ids;
  for (VariableContainer::ReadRefPair const& pair : var_ctn_.read_references) read_ref_ids.insert(pair.first);
  std::set<int> write_ref_ids;
  for (VariableContainer::WriteRefPair const& pair : var_ctn_.write_references) write_ref_ids.insert(pair.first);

  std::map<int, ConstraintList> const& svc_map = constr_pttn_.singleVariableConstraintMap();
  if (FactoryMetaSMT::isDefined(CUDD)) {
    LOG(INFO) << "Get BDD samplers for constraints involving single variable";
    for (VariableContainer::WriteRefPair& pair : var_ctn_.write_references) {
      int id = pair.first;
      if (svc_map.find(id) == svc_map.end()) continue;
      // The default distribution of a variable ranges over all values of its type and does not
      // take the constraints into account, so sampling their solutions is the better choice.
      if (vars_with_user_dist.find(id) != vars_with_user_dist.end()) {
        LOG(INFO) << "  Skip var #" << id << " due to existing distribution constraints";
        continue;
      }
      std::vector<NodePtr> exprs;
      for (ConstraintPtr c : svc_map.at(id)) exprs.push_back(c->expr());
      std::shared_ptr<BddSampler> sampler = BddSampler::get(exprs);
      if (!sampler) {
        // the constraints are too complex for a BDD, the variable is randomized like the ones without
        LOG(INFO) << "  No BDD sampler for var #" << id;
        continue;
      }
      bdd_samplers_[id] = sampler;
      vars_with_dist.insert(id);
      LOG(INFO) << "  BDD sampler for var #" << id << " found";
    }
  }

  for (std::pair<const int, NodePtr> const& pair : var_ctn_.variables) {
    int id = pair.first;
    if (read_ref_ids.count(id) || var_ctn_.dist_ref_to_var_map.count(id)) continue;
    if (bdd_samplers_.count(id) || vars_with_user_dist.count(id)) continue;
    if (svc_map.find(id) == svc_map.end() || svc_map.at(id).size() != 1) continue;
    if (Inside const* in = dynamic_cast<Inside const*>(svc_map.at(id).front()->expr().get())) {
      if (dynamic_cast<VariableExpr const*>(in->child().get()) && !in->collection().empty()) {
        inside_samplers_[id].assign(in->collection().begin(), in->collection().end());
        if (write_ref_ids.count(id)) vars_with_dist.insert(id);
        LOG(INFO) << "  Inside sampler for var #" << id << " found";
      }
    }
  }

  // A distribution of the user or a BDD sampler replaces the default distribution of a variable.
  // Without them, e.g. if the constraints are too complex for a BDD, it stays in effect.
  for(VariableContainer::ReadRefPair & pair : var_ctn_.dist_references) {
    if (!constr_pttn_.supportSet().count(pair.first)) continue;
    if (var_ctn_.implicit_dist_references.count(pair.first) == 0) continue;
    int var_id = var_ctn_.dist_ref_to_var_map.at(pair.first);
    if (vars_with_user_dist.count(var_id) || bdd_samplers_.count(var_id) || inside_samplers_.count(var_id))
      replaced_dist_refs_.insert(pair.first);
  }

  if (2 * vars_with_dist.size() > var_ctn_.write_references.size()) return;  // not necessary to randomize more

  for(VariableContainer::WriteRefPair & pair : var_ctn_.write_references) {
    // the values of the variables, which are solved before others, are chosen by solveLevel()
    if (vars_with_dist.find(pair.first) == vars_with_dist.end() && !ordered_vars_.count(pair.first))
      random_write_refs_.push_back(pair);
  }
}

bool VariableDefaultSolver::readRefsUnchanged() const {
  if (analysed_read_refs_.size() != var_ctn_.read_references.size()) return false;
  for (std::size_t i = 0; i < var_ctn_.read_references.size(); ++i) {
    Constant cur = var_ctn_.read_references[i].second->constantValue();
    Constant const& prev = analysed_read_refs_[i];
    if (cur.value() != prev.value() || cur.bitsize() != prev.bitsize() || cur.sign() != prev.sign()) {
      return false;
    }
  }
  return true;
}

std::vector<std::vector<std::string> > VariableDefaultSolver::getContradictions() const {
  if (!hards_analysed_ || !readRefsUnchanged()) {
    analyseHards();
  }
  return contradictions_;
}

std::vector<std::string> VariableDefaultSolver::getInactiveSofts() const {
  if (!solve_attempted_ && (!softs_analysed_ || !readRefsUnchanged())) {
    if (!soft_constraints_.empty() && getContradictions().empty()) {
      for (VariableContainer::ReadRefPair const& pair : var_ctn_.read_references) {
        solver_->makeAssumption(*pair.second->expr());
      }
      analyseSofts();
    } else {
      inactive_softs_.clear();
    }
    softs_analysed_ = true;
  }
  return inactive_softs_;
}

bool VariableDefaultSolver::solveCover(std::set<std::string> const& skip_covers, std::string* hit_name) {
  if (hit_name) hit_name->clear();
  solve_attempted_ = true;
  has_solution_ = false;
  if (hards_analysed_ && readRefsUnchanged() && !contradictions_.empty()) {
    inactive_softs_.clear();
    LOG(INFO) << "Failed because partition has been analyzed to be unsolvable";
    return false;
  }
  randc_undo_.clear();

  for (ConstraintPtr c : constr_pttn_) {
    if (!c->isCover()) continue;
    if (skip_covers.count(c->name())) continue;
    std::vector<NodePtr> assumptions(1, c->expr());
    if (solveWith(assumptions, RandcMap(), false)) {
      if (soft_constraints_.empty())
        inactive_softs_.clear();
      else
        updateInactiveSofts(solver_->getInactiveSofts());
      has_solution_ = true;
      for (VariableContainer::WriteRefPair& pair : var_ctn_.write_references) {
        solver_->read(*var_ctn_.variables[pair.first], *pair.second);
      }
      if (hit_name) *hit_name = c->name();
      LOG(INFO) << "Solve partition " << constr_pttn_ << " hitting constraint " << c->name();
      return true;
    }
  }
  return solve();
}

bool VariableDefaultSolver::solve() {
  LOG(INFO) << "Solve constraints in partition " << constr_pttn_;
  solve_attempted_ = true;
  has_solution_ = false;
  if (hards_analysed_ && readRefsUnchanged() && !contradictions_.empty()) {
    inactive_softs_.clear();
    LOG(INFO) << "Failed because partition has been analyzed to be unsolvable";
    return false;
  }

  randc_undo_.clear();
  RandcMap randc;
  for (std::pair<const int, NodePtr> const& pair : var_ctn_.variables) {
    std::shared_ptr<RandcState> state = randc_state(pair.first);
    if (state) randc[pair.first] = state;
  }
  // As in SystemVerilog, a randc variable must not have a distribution.
  for (VariableContainer::ReadRefPair& pair : var_ctn_.dist_references) {
    if (!constr_pttn_.supportSet().count(pair.first)) continue;
    if (!var_ctn_.user_dist_references.count(pair.first)) continue;
    if (randc.count(var_ctn_.dist_ref_to_var_map.at(pair.first)))
      throw std::runtime_error("A randc variable must not have a distribution.");
  }

  // IEEE 1800-2017 18.5.10: randc variables are not allowed in a solve-before ordering.
  for (int id : ordered_vars_)
    if (randc.count(id)) throw std::runtime_error("A randc variable must not be solved before other variables.");

  // As in SystemVerilog, the values of the randc variables are chosen before the other variables are solved.
  std::vector<NodePtr> assumptions;
  bool solved = true;
  if (randc.empty() && solve_levels_.empty()) {
    solved = solveWith(assumptions, randc, false);
  } else {
    for (RandcMap::value_type const& pair : randc) {
      if (!solved) break;
      solved = solveRandc(pair.first, *pair.second, assumptions, randc);
    }

    if (solved && !solve_levels_.empty()) {
      if (!soft_constraints_.empty()) {
        for (VariableContainer::ReadRefPair& pair : var_ctn_.read_references)
          solver_->makeAssumption(*pair.second->expr());
        for (NodePtr const& assumption : assumptions) solver_->makeAssumption(*assumption);
        std::vector<unsigned int> inactive = solver_->resolveSofts();
        updateInactiveSofts(inactive);
        std::size_t inact_idx = 0;
        for (std::size_t i = 0; i < soft_constraints_.size(); ++i) {
          if (inact_idx < inactive.size() && inactive[inact_idx] == i) {
            ++inact_idx;
          } else {
            assumptions.push_back(soft_constraints_[i]->expr());
          }
        }
      } else {
        inactive_softs_.clear();
      }
      solved = solveOrdered(assumptions, randc);
    } else if (solved && !randc.empty() && !soft_constraints_.empty()) {
      solved = solveWith(assumptions, randc, false);
    }
  }

  if (solved) {
    if (solve_levels_.empty()) {
      if (soft_constraints_.empty())
        inactive_softs_.clear();
      else
        updateInactiveSofts(solver_->getInactiveSofts());
    }
    has_solution_ = true;
    for(VariableContainer::WriteRefPair & pair : var_ctn_.write_references) {
      solver_->read(*var_ctn_.variables[pair.first], *pair.second);
    }
    LOG(INFO) << "Done solving partition " << constr_pttn_;
    return true;
  }
  inactive_softs_.clear();
  LOG(INFO) << "Failed due to conflict with read references";
  return false;
}

bool VariableDefaultSolver::solveWith(std::vector<NodePtr> const& assumptions, RandcMap const& randc,
                                      bool ignore_softs) {
  // the random values of the randc variables are given by their cycles
  for (auto const& entry : bdd_samplers_) {
    int id = entry.first;
    if (randc.count(id)) continue;
    if (ordered_vars_.count(id) && !unordered_vars_.count(id)) continue;
    std::string str;
    CHECK(entry.second->sample(&str));  // otherwise, contradiction must have been found!
    NodePtr var = var_ctn_.variables[id];
    NodePtr value(new Constant(sampleConstant(static_cast<VariableExpr const&>(*var), str)));
    NodePtr eq(new EqualOpr(var, value));
    solver_->makeSuggestion(*eq);
  }
  for (auto const& entry : inside_samplers_) {
    int id = entry.first;
    if (randc.count(id)) continue;
    if (ordered_vars_.count(id) && !unordered_vars_.count(id)) continue;
    Constant const& chosen = entry.second[random_unsigned(entry.second.size())];
    NodePtr var = var_ctn_.variables[id];
    NodePtr eq(new EqualOpr(var, new Constant(chosen)));
    solver_->makeSuggestion(*eq);
  }

  for(VariableContainer::ReadRefPair & pair : var_ctn_.read_references) {
    solver_->makeAssumption(*pair.second->expr());
  }

  for(VariableContainer::ReadRefPair & pair : var_ctn_.dist_references) {
    if (!constr_pttn_.supportSet().count(pair.first)) continue;
    if (replaced_dist_refs_.count(pair.first)) continue;
    int const var_id = var_ctn_.dist_ref_to_var_map.at(pair.first);
    if (randc.count(var_id) || (ordered_vars_.count(var_id) && !unordered_vars_.count(var_id))) continue;
    solver_->makeSuggestion(*pair.second->expr());
  }

  for (NodePtr const& assumption : assumptions) solver_->makeAssumption(*assumption);

  if (!random_write_refs_.empty()) {
    crave::random_shuffle(random_write_refs_.begin(), random_write_refs_.end());
    for (unsigned i = 0; i < (random_write_refs_.size() + 1) / 2; i++) {
      if (randc.count(random_write_refs_[i].first)) continue;
      NodePtr value(new Constant(random_write_refs_[i].second->value_as_constant()));  // reuse the random value generated by next()
      NodePtr var = var_ctn_.variables[random_write_refs_[i].first];
      NodePtr eq(new EqualOpr(var, value));
      solver_->makeSuggestion(*eq);
    }
  }

  return solver_->solve(ignore_softs);
}

// lists unused values of a randc variable, which satisfy the constraints, window by window, see solveRandc()
void VariableDefaultSolver::listWindows(NodePtr const& var, RandcState const& state, std::vector<NodePtr>& assumptions,
                                        RandcMap const& randc, std::vector<uint64_t>& listed) {
  unsigned const width = static_cast<VariableExpr const&>(*var).bitsize();
  std::vector<uint64_t> used(state.used);
  std::sort(used.begin(), used.end());
  unsigned const windows = 1u << (width - randc_window_bits);
  unsigned const first = random_unsigned(windows);
  std::size_t const n = assumptions.size();
  for (unsigned w = 0; w < windows && listed.size() < randc_max_listed; ++w) {
    uint64_t const prefix = (first + w) % windows;
    std::vector<uint64_t> excluded(std::lower_bound(used.begin(), used.end(), prefix << randc_window_bits),
                                   std::lower_bound(used.begin(), used.end(), (prefix + 1) << randc_window_bits));
    NodePtr in_window(new EqualOpr(new Bitslice(var, width - 1, randc_window_bits),
                                   new Constant(prefix, width - randc_window_bits, false)));
    while (listed.size() < randc_max_listed) {
      assumptions.push_back(in_window);
      if (!excluded.empty()) assumptions.push_back(new NotOpr(randcInside(var, excluded)));
      bool const found = solveWith(assumptions, randc, true);
      assumptions.resize(n);
      if (!found) break;
      std::string str;
      solver_->read(*var, str);
      uint64_t value = 0;
      for (char c : str) value = (value << 1) | (c == '1' ? 1 : 0);
      if (std::find(excluded.begin(), excluded.end(), value) != excluded.end()) break;  // cannot happen
      excluded.push_back(value);
      listed.push_back(value);
    }
  }
}

// As SystemVerilog keeps the values of the variables when randomize() fails, the values of the randc variables
// are not used up by a failed generation.
void VariableDefaultSolver::undoRandc() { undoRandcEntries(randc_undo_); }

void VariableDefaultSolver::undoRandc(std::set<int> const& ids) {
  std::vector<RandcUndo> remaining;
  for (std::vector<RandcUndo>::reverse_iterator ite = randc_undo_.rbegin(); ite != randc_undo_.rend(); ++ite) {
    if (ids.count(ite->id)) {
      std::vector<RandcUndo> single(1, *ite);
      undoRandcEntries(single);
    } else {
      remaining.push_back(*ite);
    }
  }
  std::reverse(remaining.begin(), remaining.end());
  randc_undo_.swap(remaining);
}

std::string VariableDefaultSolver::randcSignature(int id) {
  return containsVar(id) ? signature() : std::string();
}

// IEEE 1800-2017 18.4.2: a randc variable takes the values, which satisfy the constraints, in a random order
// before a value is repeated. The cycle starts again when all of them have been generated, or when the
// constraints of the variable change. On success, the value is added to the assumptions, and the solver holds
// a solution of the partition.
bool VariableDefaultSolver::solveRandc(int id, RandcState& state, std::vector<NodePtr>& assumptions,
                                       RandcMap const& randc) {
  NodePtr var = var_ctn_.variables[id];
  VariableExpr const* v = dynamic_cast<VariableExpr const*>(var.get());
  if (!v) throw std::runtime_error("A randc variable must be a variable.");
  if (v->bitsize() > randc_max_bitsize)
    throw std::runtime_error("A randc variable must not have more than " + std::to_string(randc_max_bitsize) +
                             " bits.");

  // the state before this call, if a new cycle starts, to give the value back on failure
  RandcUndo undo = {id, &state, std::shared_ptr<RandcState>(), 0, false};
  if (!state.started || state.signature != signature()) {
    undo.saved = std::make_shared<RandcState>(state);
    startCycle(state, v->bitsize());
    state.signature = signature();
  }

  // tries the given values in a random order, the ones, which have been tried, are moved behind n
  auto tryValues = [&](std::vector<uint64_t>& values, unsigned max_tries) {
    std::size_t n = values.size();
    for (unsigned tries = 0; n > 0 && tries < max_tries; ++tries) {
      std::swap(values[random_unsigned(n)], values[n - 1]);
      --n;
      uint64_t const value = values[n];
      assumptions.push_back(new EqualOpr(var, new Constant(randcValue(*v, value))));
      if (solveWith(assumptions, randc, true)) {
        undo.value = value;
        undo.listed = std::find(state.listed.begin(), state.listed.end(), value) != state.listed.end();
        randc_undo_.push_back(undo);
        removeValue(state.unused, value);
        removeValue(state.listed, value);
        state.used.push_back(value);
        return true;
      }
      assumptions.pop_back();
    }
    return false;
  };

  for (unsigned round = 0; round < 2; ++round) {
    if (round == 1) {
      // either no value satisfies the constraints, or all of them have been generated in this cycle
      if (state.used.empty()) break;
      LOG(INFO) << "Start a new cycle of randc variable #" << id;
      if (!undo.saved) undo.saved = std::make_shared<RandcState>(state);
      startCycle(state, v->bitsize());
    }

    // Unless all unused values, which satisfy the constraints, have been listed, values are tried at random.
    if (!state.complete && tryValues(state.unused, randc_max_tries)) return true;
    if (tryValues(state.listed, randc_max_listed)) return true;

    // The solver lists unused values, which satisfy the constraints. Each search excludes the used values and
    // the values found before. As its cost grows with the number of excluded values, the values of a variable
    // of more than randc_window_bits bits are searched window by window, among the values with the same upper
    // bits, from a window chosen at random, once more than randc_max_listed values of the cycle have been used.
    std::vector<uint64_t> listed;
    unsigned const width = v->isBool() ? 1 : v->bitsize();
    bool const windows = width > randc_window_bits && state.used.size() > randc_max_listed;
    if (windows) listWindows(var, state, assumptions, randc, listed);
    std::set<uint64_t> excluded;
    if (!windows) excluded.insert(state.used.begin(), state.used.end());
    while (!windows && listed.size() < randc_max_listed) {
      std::vector<uint64_t> values;
      bool const inside = state.unused.size() - listed.size() < excluded.size();
      if (inside) {
        for (uint64_t value : state.unused)
          if (!excluded.count(value)) values.push_back(value);
      } else {
        values.assign(excluded.begin(), excluded.end());
      }
      assumptions.push_back(inside ? randcInside(var, values) : NodePtr(new NotOpr(randcInside(var, values))));
      bool const found = solveWith(assumptions, randc, true);
      assumptions.pop_back();
      if (!found) break;
      std::string str;
      solver_->read(*var, str);
      uint64_t value = 0;
      for (char c : str) value = (value << 1) | (c == '1' ? 1 : 0);
      if (v->isBool() && (str == "true" || str == "1")) value = 1;
      if (excluded.count(value)) break;  // cannot happen, as the value satisfies the constraint above
      excluded.insert(value);
      listed.push_back(value);
    }
    if (listed.empty()) continue;
    std::sort(listed.begin(), listed.end());
    state.complete = listed.size() < randc_max_listed;
    state.listed.swap(listed);
    if (tryValues(state.listed, randc_max_listed)) return true;
  }
  // a new cycle, which has been started, is given back as well
  if (undo.saved) randc_undo_.push_back(undo);
  return false;
}

// IEEE 1800-2017 18.5.10: the variables, which are ordered before others, are chosen level by level, with equal
// probability among the values, for which the constraints have a solution. Then the other variables are solved.
// On success, the values are added to the assumptions, and the solver holds a solution of the partition.
bool VariableDefaultSolver::solveOrdered(std::vector<NodePtr>& assumptions, RandcMap const& randc) {
  unordered_vars_.clear();
  for (std::size_t i = 0; i < solve_levels_.size(); ++i)
    if (!solveLevel(i, assumptions, randc)) return false;
  return solveWith(assumptions, randc, true);
}

bool VariableDefaultSolver::solveLevel(std::size_t index, std::vector<NodePtr>& assumptions, RandcMap const& randc) {
  std::vector<int> const& level = solve_levels_[index];
  std::vector<NodePtr> vars;
  for (int id : level) {
    NodePtr var = var_ctn_.variables[id];
    VariableExpr const* v = dynamic_cast<VariableExpr const*>(var.get());
    if (!v) throw std::runtime_error("A variable solved before other variables must be a variable.");
    if (v->bitsize() > 64)
      throw std::runtime_error("A variable solved before other variables must not have more than 64 bits.");
    vars.push_back(var);
  }
  // a variable with a distribution by dist() takes the values of its distribution
  std::vector<std::shared_ptr<ReferenceExpression> > dists(level.size());
  for (VariableContainer::ReadRefPair& pair : var_ctn_.dist_references) {
    if (!constr_pttn_.supportSet().count(pair.first)) continue;
    if (!var_ctn_.user_dist_references.count(pair.first)) continue;
    int const var_id = var_ctn_.dist_ref_to_var_map.at(pair.first);
    std::vector<int>::const_iterator ite = std::find(level.begin(), level.end(), var_id);
    if (ite != level.end()) dists[ite - level.begin()] = pair.second;
  }

  bool has_dist = false;
  for (std::shared_ptr<ReferenceExpression> const& d : dists) has_dist = has_dist || d;

  // The values of the first level, which have a solution, stay the same unless the solution depends on the
  // values of references or of randc variables.
  bool const constant_values = index == 0 && !has_dist && var_ctn_.read_references.empty() && randc.empty();

  std::size_t const n = assumptions.size();
  // Each of the following ways proposes values with equal probability among a set of values, which includes all
  // values with a solution. The values, which are accepted because they have a solution, have equal probability.
  std::vector<std::vector<Constant> > listed;
  if (constant_values && !first_level_values_.empty()) {
    listed = first_level_values_;
  } else if (constant_values && first_level_unordered_) {
    return solveUnordered(level);
  } else {
    // 1. A few values at random, from the BDD of the constraints on a single variable if there is one.
    for (unsigned tries = 0; tries < solve_before_first_tries; ++tries) {
      for (std::size_t i = 0; i < vars.size(); ++i) {
        VariableExpr const& v = static_cast<VariableExpr const&>(*vars[i]);
        if (dists[i]) {
          assumptions.push_back(dists[i]->expr());
          continue;
        }
        std::map<int, std::shared_ptr<BddSampler> >::iterator sampler = bdd_samplers_.find(level[i]);
        std::string str;
        if (sampler != bdd_samplers_.end() && sampler->second->sample(&str))
          assumptions.push_back(new EqualOpr(vars[i], new Constant(sampleConstant(v, str))));
        else
          assumptions.push_back(new EqualOpr(vars[i], new Constant(randomValue(v))));
      }
      if (feasible(assumptions)) return true;
      assumptions.resize(n);
    }

    // 2. Values at random between the least and the greatest value of each variable, which have a solution.
    std::vector<std::pair<uint64_t, uint64_t> > bounds;
    if (constant_values && !first_level_bounds_.empty()) {
      bounds = first_level_bounds_;
    } else {
      if (!feasible(assumptions)) return false;
      std::vector<uint64_t> model;
      for (NodePtr const& var : vars) model.push_back(readKey(*var));
      for (std::size_t i = 0; i < vars.size(); ++i) bounds.push_back(keyBounds(vars[i], model[i], assumptions));
      if (constant_values) first_level_bounds_ = bounds;
    }
    for (unsigned tries = 0; tries < solve_before_max_tries; ++tries) {
      for (std::size_t i = 0; i < vars.size(); ++i) {
        VariableExpr const& v = static_cast<VariableExpr const&>(*vars[i]);
        uint64_t const lo = bounds[i].first;
        uint64_t const hi = bounds[i].second;
        uint64_t const key = with_active_or_global_rng(
            [lo, hi](std::mt19937& g) { return std::uniform_int_distribution<uint64_t>(lo, hi)(g); });
        assumptions.push_back(new EqualOpr(vars[i], new Constant(keyValue(v, key))));
      }
      if (feasible(assumptions)) return true;
      assumptions.resize(n);
    }

    // 3. The values, which have a solution, if there are few of them.
    NodePtr excluded;
    std::set<Constant> excluded_values;  // the listed values of a single variable
    while (listed.size() < solve_before_max_listed) {
      if (vars.size() == 1 && !excluded_values.empty())
        assumptions.push_back(new NotOpr(new Inside(vars[0], excluded_values)));
      else if (excluded)
        assumptions.push_back(new NotOpr(excluded));
      bool const found = feasible(assumptions);
      assumptions.resize(n);
      if (!found) break;
      std::vector<Constant> values;
      NodePtr match;
      for (NodePtr const& var : vars) {
        std::string str;
        solver_->read(*var, str);
        values.push_back(readValue(static_cast<VariableExpr const&>(*var), str));
        NodePtr eq(new EqualOpr(var, new Constant(values.back())));
        match = match ? NodePtr(new LogicalAndOpr(match, eq)) : eq;
      }
      if (vars.size() == 1)
        excluded_values.insert(values[0]);
      else
        excluded = excluded ? NodePtr(new LogicalOrOpr(excluded, match)) : match;
      listed.push_back(values);
    }
    if (listed.empty()) return false;
    if (listed.size() >= solve_before_max_listed) {
      if (constant_values) first_level_unordered_ = true;
      return solveUnordered(level);
    }
    if (constant_values) first_level_values_ = listed;
  }

  // one of the listed values is chosen with equal probability
  std::vector<Constant> const& values = listed[random_unsigned(listed.size())];
  for (std::size_t i = 0; i < vars.size(); ++i) assumptions.push_back(new EqualOpr(vars[i], new Constant(values[i])));
  return true;
}

// The values of the variables of a level cannot be chosen with equal probability at a bounded cost: more than
// crave::solve_before_max_listed values have a solution, and they are too sparse to be found at random. The
// variables are solved together with the variables, which are not ordered, as without the ordering.
bool VariableDefaultSolver::solveUnordered(std::vector<int> const& level) {
  LOG(INFO) << "Too many sparse values to order, the variables of a level are solved without the ordering";
  unordered_vars_.insert(level.begin(), level.end());
  return true;
}

// The values of a variable are ordered by keys, unsigned numbers, which keep the order of signed values.
uint64_t VariableDefaultSolver::readKey(Node const& var) {
  VariableExpr const& v = static_cast<VariableExpr const&>(var);
  std::string str;
  solver_->read(var, str);
  return valueKey(v, readValue(v, str));
}

// the least and the greatest key of a variable, for which the constraints have a solution, found by bisection
std::pair<uint64_t, uint64_t> VariableDefaultSolver::keyBounds(NodePtr const& var, uint64_t key,
                                                               std::vector<NodePtr>& assumptions) {
  VariableExpr const& v = static_cast<VariableExpr const&>(*var);
  unsigned const width = v.isBool() ? 1 : v.bitsize();
  uint64_t const max_key = width >= 64 ? ~uint64_t(0) : (uint64_t(1) << width) - 1;
  if (v.isBool()) return std::make_pair(uint64_t(0), max_key);
  uint64_t lo = 0, hi = key;
  while (lo < hi) {
    uint64_t const mid = lo + (hi - lo) / 2;
    assumptions.push_back(new LessEqualOpr(var, new Constant(keyValue(v, mid))));
    bool const found = feasible(assumptions);
    assumptions.pop_back();
    if (found)
      hi = std::min(mid, readKey(*var));
    else
      lo = mid + 1;
  }
  uint64_t const least = lo;
  lo = key;
  hi = max_key;
  while (lo < hi) {
    uint64_t const mid = lo + (hi - lo) / 2 + 1;
    assumptions.push_back(new GreaterEqualOpr(var, new Constant(keyValue(v, mid))));
    bool const found = feasible(assumptions);
    assumptions.pop_back();
    if (found)
      lo = std::max(mid, readKey(*var));
    else
      hi = mid - 1;
  }
  return std::make_pair(least, lo);
}

// whether the constraints have a solution under the assumptions, without suggestions of random values
bool VariableDefaultSolver::feasible(std::vector<NodePtr> const& assumptions) {
  for (VariableContainer::ReadRefPair& pair : var_ctn_.read_references) solver_->makeAssumption(*pair.second->expr());
  for (NodePtr const& assumption : assumptions) solver_->makeAssumption(*assumption);
  return solver_->solve();
}

std::string const& VariableDefaultSolver::signature() {
  if (signature_.empty()) {
    std::ostringstream os;
    std::unordered_map<unsigned, unsigned> canon_ids;
    for (ConstraintPtr c : constr_pttn_) {
      if (c->isCover()) continue;
      os << (c->isSoft() ? "soft " : "");
      writeSignature(*c->expr(), os, canon_ids);
      os << ';';
    }
    signature_ = os.str();
  }
  return signature_;
}

void VariableDefaultSolver::analyseHards() const {
  g_analyse_hards_count.fetch_add(1, std::memory_order_relaxed);
  contradictions_.clear();
  analysed_read_refs_.clear();
  analysed_read_refs_.reserve(var_ctn_.read_references.size());

  std::map<unsigned int, NodePtr> s;
  std::vector<std::string> out;

  for (ConstraintPtr c : constr_pttn_) {
    if (!c->isSoft() && !c->isCover()) {
      s.insert(std::make_pair(s.size(), c->expr()));
      out.push_back(c->name());
    }
  }

  for (VariableContainer::ReadRefPair const& pair : var_ctn_.read_references) {
    analysed_read_refs_.push_back(pair.second->constantValue());
  }

  if (!s.empty()) {
    std::unique_ptr<metaSMTVisitor> solver(FactoryMetaSMT::getNewInstance());
    for (VariableContainer::ReadRefPair const& pair : var_ctn_.read_references) {
      solver->makeAssertion(*pair.second->expr());
    }
    std::vector<std::vector<unsigned int> > results = solver->analyseContradiction(s);

    for (std::vector<unsigned int> const& result : results) {
      std::vector<std::string> vec;
      for (unsigned int i : result) {
        vec.push_back(out[i]);
      }
      contradictions_.push_back(vec);
    }
  }
  hards_analysed_ = true;
}

void VariableDefaultSolver::updateInactiveSofts(std::vector<unsigned int> const& inactive_indices) const {
  inactive_softs_.clear();
  for (unsigned int idx : inactive_indices) {
    if (idx < soft_constraints_.size()) inactive_softs_.push_back(soft_constraints_[idx]->name());
  }
}

void VariableDefaultSolver::analyseSofts() const { updateInactiveSofts(solver_->resolveSofts()); }
}

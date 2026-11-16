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

#include <set>
#include <functional>
#include <algorithm>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <typeinfo>

#include "BddSampler.hpp"
#include "../crave/backend/SolveOrder.hpp"
#include "../crave/RandomSeedManager.hpp"

namespace crave {

extern RandomSeedManager rng;

namespace {
// writes the structure of an expression, which identifies the constraints of a cycle of a randc variable
void writeSignature(Node const& n, std::ostream& os) {
  os << typeid(n).name();
  if (VariableExpr const* v = dynamic_cast<VariableExpr const*>(&n)) os << '#' << v->id();
  if (VectorExpr const* v = dynamic_cast<VectorExpr const*>(&n)) os << '#' << v->id();
  if (Placeholder const* p = dynamic_cast<Placeholder const*>(&n)) os << '#' << p->id();
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
    writeSignature(*u->child(), os);
  } else if (BinaryExpression const* b = dynamic_cast<BinaryExpression const*>(&n)) {
    writeSignature(*b->lhs(), os);
    os << ',';
    writeSignature(*b->rhs(), os);
  } else if (TernaryExpression const* t = dynamic_cast<TernaryExpression const*>(&n)) {
    writeSignature(*t->a(), os);
    os << ',';
    writeSignature(*t->b(), os);
    os << ',';
    writeSignature(*t->c(), os);
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

// a constant of a value of a variable, all values of the variable have the same probability
Constant randomValue(VariableExpr const& var) {
  if (var.isBool()) return Constant(random_unsigned(2) != 0);
  unsigned const width = var.bitsize();
  uint64_t value = 0;
  for (unsigned bits = 0; bits < width; bits += 32) value = (value << 32) | (*rng.get())();
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

bool VariableDefaultSolver::bypass_constraint_analysis = false;

unsigned VariableDefaultSolver::complexity_limit_for_bdd = 400;

VariableDefaultSolver::VariableDefaultSolver(const VariableContainer& vcon, const ConstraintPartition& cp,
                                             std::vector<std::vector<int> > const& solve_levels)
    : VariableSolver(vcon, cp) {
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

  bool has_randc = false;
  for (std::pair<const int, NodePtr> const& pair : var_ctn_.variables)
    if (randc_state(pair.first)) has_randc = true;

  for(ConstraintPtr c : constr_pttn_) {
    if (c->isCover()) continue;  // default solver ignores cover constraints
    if (c->isSoft() && has_randc) {
      // the soft constraints are decided after the values of the randc variables have been chosen
      randc_softs_.push_back(c->expr());
    } else if (c->isSoft()) {
      solver_->makeSoftAssertion(*c->expr());
    } else {
      solver_->makeAssertion(*c->expr());
    }
  }

  if (bypass_constraint_analysis) return;

  analyseConstraints();

  std::set<int> vars_with_dist;
  std::set<int> vars_with_user_dist;
  for(VariableContainer::ReadRefPair & pair : var_ctn_.dist_references) {
    assert(var_ctn_.dist_ref_to_var_map.find(pair.first) != var_ctn_.dist_ref_to_var_map.end());
    int var_id = var_ctn_.dist_ref_to_var_map.at(pair.first);
    vars_with_dist.insert(var_id);
    if (var_ctn_.implicit_dist_references.count(pair.first) == 0) vars_with_user_dist.insert(var_id);
  }

  if (FactoryMetaSMT::isDefined(CUDD)) {
    LOG(INFO) << "Get BDD samplers for constraints involving single variable";
    std::map<int, ConstraintList> const& svc_map = constr_pttn_.singleVariableConstraintMap();
    for(VariableContainer::WriteRefPair & pair : var_ctn_.write_references) {
      int id = pair.first;
      if (svc_map.find(id) == svc_map.end()) continue;
      // The default distribution of a variable ranges over all values of its type and does not
      // take the constraints into account, so sampling their solutions is the better choice.
      if (vars_with_user_dist.find(id) != vars_with_user_dist.end()) {
        LOG(INFO) << "  Skip var #" << id << " due to existing distribution constraints";
        continue;
      }
      std::vector<NodePtr> exprs;
      for(ConstraintPtr c : svc_map.at(id)) exprs.push_back(c->expr());
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

  // A distribution of the user or a BDD sampler replaces the default distribution of a variable.
  // Without them, e.g. if the constraints are too complex for a BDD, it stays in effect.
  for(VariableContainer::ReadRefPair & pair : var_ctn_.dist_references) {
    if (var_ctn_.implicit_dist_references.count(pair.first) == 0) continue;
    int var_id = var_ctn_.dist_ref_to_var_map.at(pair.first);
    if (vars_with_user_dist.count(var_id) || bdd_samplers_.count(var_id)) replaced_dist_refs_.insert(pair.first);
  }

  if (2 * vars_with_dist.size() > var_ctn_.write_references.size()) return;  // not necessary to randomize more

  for(VariableContainer::WriteRefPair & pair : var_ctn_.write_references) {
    // the values of the variables, which are solved before others, are chosen by solveLevel()
    if (vars_with_dist.find(pair.first) == vars_with_dist.end() && !ordered_vars_.count(pair.first))
      random_write_refs_.push_back(pair);
  }
}

void VariableDefaultSolver::analyseConstraints() {
  analyseHards();
  if (contradictions_.empty()) {
    analyseSofts();
    LOG(INFO) << "Partition is solvable with " << inactive_softs_.size() << " soft constraint(s) deactivated:";

    for(std::string & s : inactive_softs_) { LOG(INFO) << " " << s; }
  } else {
    LOG(INFO) << "Partition has unsatisfiable hard constraints:";
    unsigned cnt = 0;

    for(std::vector<std::string> & vs : contradictions_) {
      LOG(INFO) << "  set #" << ++cnt;

      for(std::string & s : vs) { LOG(INFO) << "   " << s; }
    }
  }
}

bool VariableDefaultSolver::solve() {
  LOG(INFO) << "Solve constraints in partition " << constr_pttn_;
  if (!contradictions_.empty()) {
    LOG(INFO) << "Failed because partition has been analyzed to be unsolvable";
    return false;
  }

  RandcMap randc;
  for (std::pair<const int, NodePtr> const& pair : var_ctn_.variables) {
    std::shared_ptr<RandcState> state = randc_state(pair.first);
    if (state) randc[pair.first] = state;
  }
  // As in SystemVerilog, a randc variable must not have a distribution.
  for (VariableContainer::ReadRefPair& pair : var_ctn_.dist_references) {
    if (!var_ctn_.user_dist_references.count(pair.first)) continue;
    if (randc.count(var_ctn_.dist_ref_to_var_map.at(pair.first)))
      throw std::runtime_error("A randc variable must not have a distribution.");
  }

  // IEEE 1800-2017 18.5.10: randc variables are not allowed in a solve-before ordering.
  for (int id : ordered_vars_)
    if (randc.count(id)) throw std::runtime_error("A randc variable must not be solved before other variables.");

  // As in SystemVerilog, the values of the randc variables are chosen before the other variables are solved.
  std::vector<NodePtr> assumptions;
  bool solved = randc.empty() && solve_levels_.empty() ? solveWith(assumptions, randc) : true;
  for (RandcMap::value_type const& pair : randc) {
    if (!solved) break;
    solved = solveRandc(pair.first, *pair.second, assumptions, randc);
  }

  // As the randc variables are solved first, a soft constraint, which contradicts their values, is dropped.
  if (solved && !randc_softs_.empty()) {
    bool model = true;
    for (NodePtr const& soft : randc_softs_) {
      assumptions.push_back(soft);
      model = solveWith(assumptions, randc);
      if (!model) assumptions.pop_back();
    }
    if (!model) solved = solveWith(assumptions, randc);
  }

  if (solved && !solve_levels_.empty()) solved = solveOrdered(assumptions, randc);

  if (solved) {
    for(VariableContainer::WriteRefPair & pair : var_ctn_.write_references) {
      solver_->read(*var_ctn_.variables[pair.first], *pair.second);
    }
    LOG(INFO) << "Done solving partition " << constr_pttn_;
    return true;
  }
  LOG(INFO) << "Failed due to conflict with read references";
  return false;
}

bool VariableDefaultSolver::solveWith(std::vector<NodePtr> const& assumptions, RandcMap const& randc) {
  // the random values of the randc variables are given by their cycles
  for(VariableContainer::WriteRefPair & pair : var_ctn_.write_references) {
    int id = pair.first;
    if (bdd_samplers_.find(id) == bdd_samplers_.end() || randc.count(id) || ordered_vars_.count(id)) continue;
    std::string str;
    CHECK(bdd_samplers_[id]->sample(&str));  // otherwise, contradiction must have been found!
    NodePtr value(new Constant(pair.second->to_constant(str)));
    NodePtr var = var_ctn_.variables[id];
    NodePtr eq(new EqualOpr(var, value));
    solver_->makeSuggestion(*eq);
  }

  for(VariableContainer::ReadRefPair & pair : var_ctn_.read_references) {
    solver_->makeAssumption(*pair.second->expr());
  }

  for(VariableContainer::ReadRefPair & pair : var_ctn_.dist_references) {
    if (replaced_dist_refs_.count(pair.first)) continue;
    int const var_id = var_ctn_.dist_ref_to_var_map.at(pair.first);
    if (randc.count(var_id) || ordered_vars_.count(var_id)) continue;
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

  return solver_->solve();
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

  if (!state.started || state.signature != signature()) {
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
      if (solveWith(assumptions, randc)) {
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
      if (state.used.empty()) return false;
      LOG(INFO) << "Start a new cycle of randc variable #" << id;
      startCycle(state, v->bitsize());
    }

    // Unless all unused values, which satisfy the constraints, have been listed, values are tried at random.
    if (!state.complete && tryValues(state.unused, randc_max_tries)) return true;
    if (tryValues(state.listed, randc_max_listed)) return true;

    // The solver lists unused values, which satisfy the constraints.
    std::vector<uint64_t> listed;
    std::set<uint64_t> excluded(state.used.begin(), state.used.end());
    while (listed.size() < randc_max_listed) {
      std::vector<uint64_t> values;
      bool const inside = state.unused.size() - listed.size() < excluded.size();
      if (inside) {
        for (uint64_t value : state.unused)
          if (!excluded.count(value)) values.push_back(value);
      } else {
        values.assign(excluded.begin(), excluded.end());
      }
      assumptions.push_back(inside ? randcInside(var, values) : NodePtr(new NotOpr(randcInside(var, values))));
      bool const found = solveWith(assumptions, randc);
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
    state.complete = listed.size() < randc_max_listed;
    state.listed.swap(listed);
    if (tryValues(state.listed, randc_max_listed)) return true;
  }
  return false;
}

// IEEE 1800-2017 18.5.10: the variables, which are ordered before others, are chosen level by level, with equal
// probability among the values, for which the constraints have a solution. Then the other variables are solved.
// On success, the values are added to the assumptions, and the solver holds a solution of the partition.
bool VariableDefaultSolver::solveOrdered(std::vector<NodePtr>& assumptions, RandcMap const& randc) {
  for (std::size_t i = 0; i < solve_levels_.size(); ++i)
    if (!solveLevel(i, assumptions, randc)) return false;
  return solveWith(assumptions, randc);
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

  std::vector<std::vector<Constant> > listed;
  if (constant_values && !first_level_values_.empty()) {
    listed = first_level_values_;
  } else {
    std::size_t const n = assumptions.size();

    // Values tried at random, which have a solution, have equal probability. The BDD of the constraints on a
    // single variable picks one of their solutions with equal probability, which keeps the probability equal.
    for (unsigned tries = 0; tries < solve_before_max_tries; ++tries) {
      for (std::size_t i = 0; i < vars.size(); ++i) {
        VariableExpr const& v = static_cast<VariableExpr const&>(*vars[i]);
        if (dists[i]) {
          assumptions.push_back(dists[i]->expr());
          continue;
        }
        std::map<int, std::shared_ptr<BddSampler> >::iterator sampler = bdd_samplers_.find(level[i]);
        std::string str;
        if (sampler != bdd_samplers_.end() && sampler->second->sample(&str))
          assumptions.push_back(new EqualOpr(vars[i], new Constant(readValue(v, str))));
        else
          assumptions.push_back(new EqualOpr(vars[i], new Constant(randomValue(v))));
      }
      if (feasible(assumptions)) return true;
      assumptions.resize(n);
    }

    // The solver lists the values, which have a solution.
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
    if (constant_values && listed.size() < solve_before_max_listed) first_level_values_ = listed;
  }

  // one of the listed values is chosen with equal probability
  std::vector<Constant> values;
  if (listed.size() < solve_before_max_listed) {
    values = listed[random_unsigned(listed.size())];
  } else {
    // too many values to list them all, the values of a solution are taken
    if (!solveWith(assumptions, randc)) return false;
    for (NodePtr const& var : vars) {
      std::string str;
      solver_->read(*var, str);
      values.push_back(readValue(static_cast<VariableExpr const&>(*var), str));
    }
  }
  for (std::size_t i = 0; i < vars.size(); ++i) assumptions.push_back(new EqualOpr(vars[i], new Constant(values[i])));
  return true;
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
    for (ConstraintPtr c : constr_pttn_) {
      if (c->isCover()) continue;
      os << (c->isSoft() ? "soft " : "");
      writeSignature(*c->expr(), os);
      os << ';';
    }
    signature_ = os.str();
  }
  return signature_;
}

void VariableDefaultSolver::analyseHards() {
  std::unique_ptr<metaSMTVisitor> solver(FactoryMetaSMT::getNewInstance());

  std::map<unsigned int, NodePtr> s;
  std::vector<std::string> out;
  std::vector<std::vector<unsigned int> > results;

  for(ConstraintPtr c : constr_pttn_) {
    if (!c->isSoft() && !c->isCover()) {
      s.insert(std::make_pair(s.size(), c->expr()));
      out.push_back(c->name());
    }
  }
  results = solver->analyseContradiction(s);

  for(std::vector<unsigned int> result : results) {
    std::vector<std::string> vec;

    for(unsigned int i : result) { vec.push_back(out[i]); }
    contradictions_.push_back(vec);
  }
}

void VariableDefaultSolver::analyseSofts() {
  std::vector<unsigned int> result = solver_->analyseSofts();
  std::vector<unsigned int>::iterator ite = result.begin();
  unsigned cnt = 0;

  for(ConstraintPtr c : constr_pttn_) {
    if (ite == result.end()) break;
    if (c->isSoft()) {
      if (*ite == cnt) {
        ite++;
        inactive_softs_.push_back(c->name());
      }
      cnt++;
    }
  }
}
}

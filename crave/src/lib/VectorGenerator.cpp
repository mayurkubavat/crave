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

#include "../crave/backend/VectorGenerator.hpp"
#include "../crave/ir/visitor/GetSupportSetVisitor.hpp"
#include "../crave/utils/Logging.hpp"
#include "BddSampler.hpp"
#include "RandomShuffle.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>

namespace crave {

extern std::function<bool(void)> random_bit;

namespace {
void materializeBits(VariableExpr const& ve, std::string const& str, std::string& concrete_bits,
                     bool& had_dont_care) {
  if (ve.isBool()) {
    if (str == "1" || str == "true") {
      concrete_bits = "1";
      had_dont_care = (str != "1");
    } else if (str == "0" || str == "false") {
      concrete_bits = "0";
      had_dont_care = (str != "0");
    } else {
      concrete_bits = random_bit() ? "1" : "0";
      had_dont_care = true;
    }
    return;
  }
  concrete_bits = str;
  had_dont_care = false;
  for (char& c : concrete_bits) {
    if (c != '0' && c != '1') {
      c = random_bit() ? '1' : '0';
      had_dont_care = true;
    }
  }
}

Constant bitsToConstant(VariableExpr const& ve, std::string const& concrete_bits) {
  if (ve.isBool()) return Constant(concrete_bits == "1");
  uint64_t val = (ve.sign() && !concrete_bits.empty() && concrete_bits[0] == '1') ? ~uint64_t(0) : 0;
  for (char c : concrete_bits) {
    val <<= 1;
    if (c == '1') val |= 1;
  }
  return Constant(val, ve.bitsize(), ve.sign());
}

Constant materializeToConstant(VariableExpr const& ve, std::string const& str, std::string& concrete_bits,
                               bool& had_dont_care) {
  materializeBits(ve, str, concrete_bits, had_dont_care);
  return bitsToConstant(ve, concrete_bits);
}

void applyScalarOverrideAndWriteRefs(VariableContainer* vcon, VariableGenerator& var_gen, int id,
                                     std::string const& concrete_bits) {
  var_gen.setOverride(id, concrete_bits);
  for (VariableContainer::WriteRefPair const& pair : vcon->write_references) {
    if (pair.first == id) pair.second->set_value(concrete_bits);
  }
}

// converts a value read from a solver, where a bit can also be a don't care
Constant to_constant(std::string const& str, unsigned bitsize, bool sign) {
  std::string concrete(str);
  for (char& c : concrete) {
    if (c != '0' && c != '1') c = random_bit() ? '1' : '0';
  }
  uint64_t val = (sign && !concrete.empty() && concrete[0] == '1') ? ~uint64_t(0) : 0;
  for (char c : concrete) {
    val <<= 1;
    if (c == '1') val |= 1;
  }
  return Constant(val, bitsize, sign);
}

Constant randcValue(VariableExpr const& var, uint64_t value) {
  if (var.isBool()) return Constant(value != 0);
  return Constant(value, var.bitsize(), false);
}

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

void startCycle(RandcState& state, unsigned width) {
  state.used.clear();
  state.listed.clear();
  state.complete = false;
  state.unused.resize(uint64_t(1) << width);
  for (uint64_t i = 0; i < state.unused.size(); ++i) state.unused[i] = i;
  state.started = true;
}

struct VectorElementTypeVisitor : public NodeVisitor {
  explicit VectorElementTypeVisitor(unsigned vector_id)
      : vector_id_(vector_id), found_(false), bitsize_(1u), sign_(true) {}

  bool find(std::vector<VectorConstraintPtr> const& constraints, unsigned& bitsize, bool& sign) {
    for (VectorConstraintPtr const& vc : constraints) {
      vc->expr()->visit(this);
      if (found_) {
        bitsize = bitsize_;
        sign = sign_;
        return true;
      }
    }
    return false;
  }

  void visitNode(Node const&) {}
  void visitTerminal(Terminal const&) {}
  void visitUnaryExpr(UnaryExpression const& u) { u.child()->visit(this); }
  void visitUnaryOpr(UnaryOperator const&) {}
  void visitBinaryExpr(BinaryExpression const& b) {
    b.lhs()->visit(this);
    if (!found_) b.rhs()->visit(this);
  }
  void visitBinaryOpr(BinaryOperator const&) {}
  void visitTernaryExpr(TernaryExpression const& t) {
    t.a()->visit(this);
    if (!found_) t.b()->visit(this);
    if (!found_) t.c()->visit(this);
  }
  void visitPlaceholder(Placeholder const&) {}
  void visitVariableExpr(VariableExpr const&) {}
  void visitConstant(Constant const&) {}
  void visitVectorExpr(VectorExpr const& v) {
    if (v.id() == vector_id_) {
      found_ = true;
      bitsize_ = v.bitsize();
      sign_ = v.sign();
    }
  }
  void visitVectorReduction(VectorReduction const& r) {
    if (r.id() == vector_id_) {
      found_ = true;
      bitsize_ = r.elemBitsize();
      sign_ = r.elemSign();
    }
  }
  void visitNotOpr(NotOpr const& n) { visitUnaryExpr(n); }
  void visitNegOpr(NegOpr const& n) { visitUnaryExpr(n); }
  void visitComplementOpr(ComplementOpr const& c) { visitUnaryExpr(c); }
  void visitInside(Inside const& i) { visitUnaryExpr(i); }
  void visitExtendExpr(ExtendExpression const& e) { visitUnaryExpr(e); }
  void visitAndOpr(AndOpr const& a) { visitBinaryExpr(a); }
  void visitOrOpr(const OrOpr& o) { visitBinaryExpr(o); }
  void visitLogicalAndOpr(LogicalAndOpr const& la) { visitBinaryExpr(la); }
  void visitLogicalOrOpr(LogicalOrOpr const& lo) { visitBinaryExpr(lo); }
  void visitLogicalIffOpr(LogicalIffOpr const& liff) { visitBinaryExpr(liff); }
  void visitXorOpr(XorOpr const& x) { visitBinaryExpr(x); }
  void visitEqualOpr(EqualOpr const& eq) { visitBinaryExpr(eq); }
  void visitNotEqualOpr(NotEqualOpr const& neq) { visitBinaryExpr(neq); }
  void visitLessOpr(LessOpr const& l) { visitBinaryExpr(l); }
  void visitLessEqualOpr(LessEqualOpr const& le) { visitBinaryExpr(le); }
  void visitGreaterOpr(GreaterOpr const& g) { visitBinaryExpr(g); }
  void visitGreaterEqualOpr(GreaterEqualOpr const& ge) { visitBinaryExpr(ge); }
  void visitPlusOpr(PlusOpr const& p) { visitBinaryExpr(p); }
  void visitMinusOpr(MinusOpr const& m) { visitBinaryExpr(m); }
  void visitMultipliesOpr(MultipliesOpr const& m) { visitBinaryExpr(m); }
  void visitDevideOpr(DevideOpr const& d) { visitBinaryExpr(d); }
  void visitModuloOpr(ModuloOpr const& m) { visitBinaryExpr(m); }
  void visitShiftLeftOpr(ShiftLeftOpr const& shl) { visitBinaryExpr(shl); }
  void visitShiftRightOpr(ShiftRightOpr const& shr) { visitBinaryExpr(shr); }
  void visitVectorAccess(VectorAccess const& va) { visitBinaryExpr(va); }
  void visitIfThenElse(IfThenElse const& ite) { visitTernaryExpr(ite); }
  void visitForEach(ForEach const& fe) { visitBinaryExpr(fe); }
  void visitUnique(Unique const& u) { visitUnaryExpr(u); }
  void visitBitslice(Bitslice const& b) { visitUnaryExpr(b); }
  void visitRangeExpr(RangeExpr const& r) { visitTernaryExpr(r); }
  void visitTolPlusMinusExpr(TolPlusMinusExpr const& t) { visitTernaryExpr(t); }
  void visitTolPlusPctExpr(TolPlusPctExpr const& t) { visitTernaryExpr(t); }
  void visitVectorContains(VectorContains const& vc) {
    if (vc.vec_id() == vector_id_) {
      found_ = true;
      bitsize_ = vc.elem_bitsize();
      sign_ = vc.elem_sign();
    } else if (!found_) {
      vc.child()->visit(this);
    }
  }
  void visitOneHotExpr(OneHotExpr const& o) { visitUnaryExpr(o); }
  void visitOneHot0Expr(OneHot0Expr const& o) { visitUnaryExpr(o); }
  void visitCountOnesExpr(CountOnesExpr const& o) { visitUnaryExpr(o); }
  void visitClog2Expr(Clog2Expr const& o) { visitUnaryExpr(o); }
  void visitSelfDeterminedExpr(SelfDeterminedExpr const& s) { visitUnaryExpr(s); }
  void visitBoundsCheckExpr(BoundsCheckExpr const& b) { visitBinaryExpr(b); }
  void visitVectorReduceWith(VectorReduceWith const& vrw) {
    if (vrw.vec_id() == vector_id_) {
      found_ = true;
      bitsize_ = vrw.elemBitsize();
      sign_ = vrw.elemSign();
    } else if (!found_) {
      vrw.child()->visit(this);
    }
  }

 private:
  unsigned vector_id_;
  bool found_;
  unsigned bitsize_;
  bool sign_;
};
}  // namespace

VectorSolver::VectorSolver(int vector_id)
    : constraints_(),
      vector_id_(vector_id),
      scalar_vars_(),
      solver_(FactoryMetaSMT::getNewInstance()),
      vec_elements_(),
      failed_with_solved_size_(false) {}

void VectorSolver::addConstraint(VectorConstraintPtr vc) {
  auto ite = constraints_.begin();
  while (ite != constraints_.end() && (*ite)->id() < vc->id()) ++ite;
  constraints_.insert(ite, vc);
  for (int id : vc->supportVars()) {
    if (id != vector_id_) scalar_vars_.insert(id);
  }
  elem_samplers_.clear();
  samplers_.clear();
}

bool VectorSolver::solve(VariableGenerator& var_gen, VariableContainer* vcon) {
  if (!vcon) vcon = variable_container();
  std::string cstr_name_list;
  for(VectorConstraintPtr constraint : constraints_) {
    cstr_name_list += " " + constraint->name();
  }
  LOG(INFO) << "Solve constraints of vector " << vector_id_ << " [" << cstr_name_list << "]";

  __rand_vec_base* vector = vectorBaseMap[vector_id_];

  unsigned int size = default_rand_vec_size();
  bool const solved_size = var_gen.read(vector->size_var(), &size);
  if (!solved_size) {
    LOG(INFO) << "Use default size for vector " << vector_id_;
  } else {
    LOG(INFO) << "Size of vector " << vector_id_ << " = " << size;
  }
  last_size_ = size;
  failed_with_solved_size_ = false;
  resetSolver(size, vcon);

  std::vector<int> unpinned_scalars;
  std::vector<std::pair<int, std::string> > materialized_overrides;
  if (!scalar_vars_.empty()) {
    int const size_var_id = static_cast<int>(vector->size_var().id());
    for (int id : scalar_vars_) {
      auto var_ite = vcon->variables.find(id);
      if (var_ite == vcon->variables.end()) continue;
      NodePtr var_node = var_ite->second;
      if (id == size_var_id) {
        solver_->makeAssertion(EqualOpr(
            var_node, new Constant(size, bitsize_traits<unsigned int>::value, crave::is_signed<unsigned int>::value)));
        continue;
      }
      bool matched_ref = false;
      for (VariableContainer::ReadRefPair const& pair : vcon->read_references) {
        if (pair.first == id) {
          solver_->makeAssertion(*pair.second->expr());
          matched_ref = true;
          break;
        }
      }
      if (matched_ref) continue;
      std::string str;
      if (var_gen.read(id, str)) {
        if (VariableExpr const* ve = dynamic_cast<VariableExpr const*>(var_node.get())) {
          std::string concrete_bits;
          bool had_dont_care = false;
          Constant c = materializeToConstant(*ve, str, concrete_bits, had_dont_care);
          solver_->makeAssertion(EqualOpr(var_node, new Constant(c)));
          if (had_dont_care) materialized_overrides.push_back(std::make_pair(id, concrete_bits));
        }
      } else {
        unpinned_scalars.push_back(id);
      }
    }
  }

  makeSuggestions();
  bool result = solver_->solve(false);
  if (result) {
    solver_->readVector(vec_elements_, vector);
    if (!materialized_overrides.empty() || !unpinned_scalars.empty()) {
      for (auto const& ov : materialized_overrides) {
        applyScalarOverrideAndWriteRefs(vcon, var_gen, ov.first, ov.second);
      }
      for (int id : unpinned_scalars) {
        auto var_ite = vcon->variables.find(id);
        if (var_ite == vcon->variables.end()) continue;
        std::string bits;
        if (solver_->read(*var_ite->second, bits)) {
          if (VariableExpr const* ve = dynamic_cast<VariableExpr const*>(var_ite->second.get())) {
            std::string concrete_bits;
            bool had_dont_care = false;
            materializeBits(*ve, bits, concrete_bits, had_dont_care);
            applyScalarOverrideAndWriteRefs(vcon, var_gen, id, concrete_bits);
          } else {
            applyScalarOverrideAndWriteRefs(vcon, var_gen, id, bits);
          }
        }
      }
    }
    LOG(INFO) << "Done solving vector " << vector_id_;
  }
  else {
    LOG(INFO) << "Failed solving vector " << vector_id_;
    if (solved_size)
      for (VectorConstraintPtr constraint : constraints_)
        if (constraint->isReduction() && !constraint->isSoft()) failed_with_solved_size_ = true;
  }
  return result;
}

void VectorSolver::resetSolver(unsigned int const size, VariableContainer const* vcon) {
  solver_.reset(FactoryMetaSMT::getNewInstance());
  buildSolver(size, vcon);
}

// Without suggestions the solver returns the same solution in every run. So sample the solutions of
// the hard constraints on each single element with a BDD, as VariableDefaultSolver does for the
// constraints on a single variable, and suggest the values to the solver. Suggestions which contradict
// other constraints, e.g. between elements, are dropped by the solver.
void VectorSolver::makeSuggestions() {
  // Elements with the same constraints share a sampler. Constraints which depend on the index need
  // a sampler per element, so only the first of these elements get one.
  static const unsigned max_samplers = 16;

  for (unsigned int i = 0u; i < elem_constraints_.size(); ++i) {
    if (elem_constraints_[i].empty()) continue;
    VariablePtr var = vec_elements_[i];
    auto ite = elem_samplers_.find(var->id());
    if (ite == elem_samplers_.end()) {
      std::shared_ptr<BddSampler> sampler = BddSampler::get(elem_constraints_[i], samplers_.size() < max_samplers);
      if (sampler) samplers_.insert(sampler.get());
      ite = elem_samplers_.insert(std::make_pair(var->id(), sampler)).first;
    }
    std::string str;
    if (!ite->second || !ite->second->sample(&str)) continue;
    NodePtr value(new Constant(to_constant(str, var->bitsize(), var->sign())));
    NodePtr eq(new EqualOpr(var, value));
    solver_->makeSuggestion(*eq);
  }
}

void VectorSolver::buildSolver(unsigned int const size, VariableContainer const* vcon) {
  if (!vcon) vcon = variable_container();
  if (vec_elements_.size() != size) {
    unsigned int old_size = vec_elements_.size();
    if (size < old_size) {
      for (unsigned int i = size; i < old_size; ++i) elem_samplers_.erase(vec_elements_[i]->id());
      samplers_.clear();
      for (auto const& entry : elem_samplers_)
        if (entry.second) samplers_.insert(entry.second.get());
    }
    vec_elements_.resize(size);
    if (old_size < size) {
      unsigned elem_bitsize = 1u;
      bool elem_sign = true;
      auto vec_ite = vcon->vector_variables.find(vector_id_);
      if (vec_ite != vcon->vector_variables.end()) {
        if (VectorExpr const* vec = dynamic_cast<VectorExpr const*>(vec_ite->second.get())) {
          elem_bitsize = vec->bitsize();
          elem_sign = vec->sign();
        }
      } else {
        VectorElementTypeVisitor vetv(static_cast<unsigned>(vector_id_));
        vetv.find(constraints_, elem_bitsize, elem_sign);
      }
      for (unsigned int i = old_size; i < size; ++i) {
        vec_elements_[i] = new VariableExpr(new_var_id(), elem_bitsize, elem_sign);
      }
    }
  }

  elem_constraints_.assign(size, std::vector<NodePtr>());

  for(VectorConstraintPtr constraint : constraints_) {
    if (constraint->isReduction()) {
      // a constraint on the reductions of all elements, which is solved once
      ReplaceVisitor replacer(&vec_elements_, nullptr, -2);
      constraint->expr()->visit(&replacer);
      if (replacer.okay()) {
        NodePtr res = replacer.result();
        if (replacer.resultBoundsGuard()) {
          res = new LogicalAndOpr(replacer.resultBoundsGuard(), res);
        }
        if (constraint->isSoft())
          solver_->makeSoftAssertion(*res);
        else
          solver_->makeAssertion(*res);
      } else if (!constraint->isSoft()) {
        // an element outside the vector is accessed, the constraint does not hold for this size
        LOG(INFO) << "Constraint " << constraint->name() << " accesses an element outside vector " << vector_id_;
        solver_->makeAssertion(Constant(false));
      }
    } else if (!constraint->isUnique()) {
      ForEach const* fe = dynamic_cast<ForEach const*>(constraint->expr().get());
      bool const allow_oob_skip = !(fe && dynamic_cast<VectorAccess const*>(fe->rhs().get()));
      ReplaceVisitor replacer(&vec_elements_, nullptr, -1, allow_oob_skip);
      for (unsigned int i = 0u; i < size; ++i) {
        replacer.setVecIdx(i);
        constraint->expr()->visit(&replacer);

        if (replacer.okay()) {
          NodePtr iter_res = replacer.result();
          if (replacer.resultBoundsGuard()) {
            iter_res = new LogicalAndOpr(replacer.resultBoundsGuard(), iter_res);
          }
          if (constraint->isSoft()) {
            solver_->makeSoftAssertion(*iter_res);
          } else {
            solver_->makeAssertion(*iter_res);

            GetSupportSetVisitor gssv;
            iter_res->visit(&gssv);
            std::set<int> const& support = gssv.getSupportVars();
            if (support.size() == 1 && *support.begin() == static_cast<int>(vec_elements_[i]->id()))
              elem_constraints_[i].push_back(iter_res);
          }
        } else if (replacer.resultBoundsGuard()) {
          NodePtr bg = replacer.resultBoundsGuard();
          if (constraint->isSoft()) {
            solver_->makeSoftAssertion(*bg);
          } else {
            solver_->makeAssertion(*bg);
          }
        }

        replacer.reset();
      }
    } else {
      for (unsigned i = 0; i < vec_elements_.size(); i++)
        for (unsigned j = i + 1; j < vec_elements_.size(); ++j) {
          NotEqualOpr neOp(vec_elements_[i], vec_elements_[j]);
          if (constraint->isSoft())
            solver_->makeSoftAssertion(neOp);
          else
            solver_->makeAssertion(neOp);
        }
    }
  }
}

VectorGenerator::VectorGenerator() : vector_solvers_(), failed_(nullptr), joint_randc_undo_() {}

bool VectorGenerator::failedSize(int* id, unsigned int* size) const {
  if (!failed_ || !failed_->failedWithSolvedSize()) return false;
  *id = failed_->id();
  *size = failed_->size();
  return true;
}

std::set<std::string> VectorGenerator::coupledCovers(std::vector<ConstraintPartition> const& partitions,
                                                     std::vector<std::string> const& hits,
                                                     std::set<int>* coupled_vecs) const {
  if (coupled_vecs) coupled_vecs->clear();
  std::set<std::string> result;
  if (!failed_ || hits.empty()) return result;

  int const failed_vec_id = failed_->id();
  std::set<int> coupled_vars = failed_->scalarVars();
  auto vbm_it = vectorBaseMap.find(failed_vec_id);
  if (vbm_it != vectorBaseMap.end()) {
    coupled_vars.insert(static_cast<int>(vbm_it->second->size_var().id()));
  }

  std::set<int> included_vecs;
  included_vecs.insert(failed_vec_id);
  std::vector<bool> included_parts(partitions.size(), false);

  bool changed = true;
  while (changed) {
    changed = false;
    for (std::size_t i = 0; i < partitions.size(); ++i) {
      if (included_parts[i]) continue;
      std::set<int> const& pvars = partitions[i].supportSet();
      bool intersects = false;
      for (int id : coupled_vars) {
        if (pvars.count(id)) {
          intersects = true;
          break;
        }
      }
      if (intersects) {
        included_parts[i] = true;
        coupled_vars.insert(pvars.begin(), pvars.end());
        changed = true;
      }
    }
    for (auto const& kv : vector_solvers_) {
      if (included_vecs.count(kv.first)) continue;
      std::set<int> const& svars = kv.second.scalarVars();
      bool intersects = false;
      for (int id : coupled_vars) {
        if (svars.count(id)) {
          intersects = true;
          break;
        }
      }
      if (!intersects) {
        auto kv_vbm = vectorBaseMap.find(kv.first);
        if (kv_vbm != vectorBaseMap.end() &&
            coupled_vars.count(static_cast<int>(kv_vbm->second->size_var().id()))) {
          intersects = true;
        }
      }
      if (intersects) {
        included_vecs.insert(kv.first);
        coupled_vars.insert(svars.begin(), svars.end());
        auto kv_vbm = vectorBaseMap.find(kv.first);
        if (kv_vbm != vectorBaseMap.end()) {
          coupled_vars.insert(static_cast<int>(kv_vbm->second->size_var().id()));
        }
        changed = true;
      }
    }
  }

  std::set<std::string> hit_set(hits.begin(), hits.end());
  for (std::size_t i = 0; i < partitions.size(); ++i) {
    if (!included_parts[i]) continue;
    for (ConstraintPtr const& c : partitions[i]) {
      if (c->isCover() && hit_set.count(c->name())) {
        result.insert(c->name());
      }
    }
  }
  if (coupled_vecs) *coupled_vecs = std::move(included_vecs);
  return result;
}

bool VectorGenerator::solveJoint(VariableGenerator& var_gen,
                                 std::vector<ConstraintPartition> const& partitions,
                                 int failed_vec_id,
                                 std::set<int>& solved_vecs,
                                 VariableContainer* vcon,
                                 std::set<std::string> const* active_covers) {
  if (!vcon) vcon = variable_container();
  auto failed_it = vector_solvers_.find(failed_vec_id);
  if (failed_it == vector_solvers_.end()) return false;
  if (failed_it->second.scalarVars().empty()) return false;

  std::set<int> included_vecs;
  included_vecs.insert(failed_vec_id);
  std::set<int> joint_scalar_ids = failed_it->second.scalarVars();
  std::vector<bool> included_parts(partitions.size(), false);

  bool changed = true;
  while (changed) {
    changed = false;
    for (std::size_t i = 0; i < partitions.size(); ++i) {
      if (included_parts[i]) continue;
      std::set<int> const& pvars = partitions[i].supportSet();
      bool intersects = false;
      for (int id : joint_scalar_ids) {
        if (pvars.count(id)) {
          intersects = true;
          break;
        }
      }
      if (intersects) {
        included_parts[i] = true;
        joint_scalar_ids.insert(pvars.begin(), pvars.end());
        changed = true;
      }
    }
    for (auto const& kv : vector_solvers_) {
      if (included_vecs.count(kv.first)) continue;
      std::set<int> const& svars = kv.second.scalarVars();
      bool intersects = false;
      for (int id : joint_scalar_ids) {
        if (svars.count(id)) {
          intersects = true;
          break;
        }
      }
      if (intersects) {
        included_vecs.insert(kv.first);
        joint_scalar_ids.insert(svars.begin(), svars.end());
        changed = true;
      }
    }
  }

  bool has_any_part = false;
  for (bool inc : included_parts)
    if (inc) has_any_part = true;
  bool has_write_ref = false;
  for (VariableContainer::WriteRefPair const& pair : vcon->write_references) {
    if (joint_scalar_ids.count(pair.first)) {
      has_write_ref = true;
      break;
    }
  }
  if (!has_any_part && !has_write_ref && included_vecs.size() == 1) return false;

  LOG(INFO) << "Attempting joint scalar+vector solve for vector " << failed_vec_id;
  SolverPtr joint_solver(FactoryMetaSMT::getNewInstance());

  for (std::size_t i = 0; i < partitions.size(); ++i) {
    if (!included_parts[i]) continue;
    for (ConstraintPtr const& c : partitions[i]) {
      if (c->isCover()) {
        if (active_covers && active_covers->count(c->name())) {
          joint_solver->makeAssertion(*c->expr());
        }
        continue;
      }
      if (c->isSoft())
        joint_solver->makeSoftAssertion(*c->expr());
      else
        joint_solver->makeAssertion(*c->expr());
    }
  }

  std::map<int, VectorSolver::VectorElements> joint_elems;
  for (int vid : included_vecs) {
    VectorSolver& vs = vector_solvers_.at(vid);
    __rand_vec_base* vector = vectorBaseMap[vid];
    unsigned int sz = default_rand_vec_size();
    if (vid == failed_vec_id) {
      sz = vs.lastSize();
    } else if (!var_gen.read(vector->size_var(), &sz)) {
      sz = default_rand_vec_size();
    }

    int const size_var_id = static_cast<int>(vector->size_var().id());
    auto sv_it = vcon->variables.find(size_var_id);
    if (sv_it != vcon->variables.end()) {
      joint_solver->makeAssertion(EqualOpr(
          sv_it->second, new Constant(sz, bitsize_traits<unsigned int>::value, crave::is_signed<unsigned int>::value)));
    }

    unsigned elem_bitsize = 1u;
    bool elem_sign = true;
    auto vec_ite = vcon->vector_variables.find(vid);
    if (vec_ite != vcon->vector_variables.end()) {
      if (VectorExpr const* vec = dynamic_cast<VectorExpr const*>(vec_ite->second.get())) {
        elem_bitsize = vec->bitsize();
        elem_sign = vec->sign();
      }
    } else {
      VectorElementTypeVisitor vetv(static_cast<unsigned>(vid));
      vetv.find(vs.constraints(), elem_bitsize, elem_sign);
    }

    VectorSolver::VectorElements elems(sz);
    for (unsigned int i = 0; i < sz; ++i) {
      elems[i] = new VariableExpr(new_var_id(), elem_bitsize, elem_sign);
    }

    for (VectorConstraintPtr const& constraint : vs.constraints()) {
      if (constraint->isReduction()) {
        ReplaceVisitor replacer(&elems, nullptr, -2);
        constraint->expr()->visit(&replacer);
        if (replacer.okay()) {
          NodePtr res = replacer.result();
          if (replacer.resultBoundsGuard()) {
            res = new LogicalAndOpr(replacer.resultBoundsGuard(), res);
          }
          if (constraint->isSoft())
            joint_solver->makeSoftAssertion(*res);
          else
            joint_solver->makeAssertion(*res);
        } else if (!constraint->isSoft()) {
          joint_solver->makeAssertion(Constant(false));
        }
      } else if (!constraint->isUnique()) {
        ForEach const* fe = dynamic_cast<ForEach const*>(constraint->expr().get());
        bool const allow_oob_skip = !(fe && dynamic_cast<VectorAccess const*>(fe->rhs().get()));
        ReplaceVisitor replacer(&elems, nullptr, -1, allow_oob_skip);
        for (unsigned int i = 0u; i < sz; ++i) {
          replacer.setVecIdx(i);
          constraint->expr()->visit(&replacer);
          if (replacer.okay()) {
            NodePtr iter_res = replacer.result();
            if (replacer.resultBoundsGuard()) {
              iter_res = new LogicalAndOpr(replacer.resultBoundsGuard(), iter_res);
            }
            if (constraint->isSoft())
              joint_solver->makeSoftAssertion(*iter_res);
            else
              joint_solver->makeAssertion(*iter_res);
          } else if (replacer.resultBoundsGuard()) {
            NodePtr bg = replacer.resultBoundsGuard();
            if (constraint->isSoft())
              joint_solver->makeSoftAssertion(*bg);
            else
              joint_solver->makeAssertion(*bg);
          }
          replacer.reset();
        }
      } else {
        for (unsigned i = 0; i < elems.size(); ++i) {
          for (unsigned j = i + 1; j < elems.size(); ++j) {
            NotEqualOpr neOp(elems[i], elems[j]);
            if (constraint->isSoft())
              joint_solver->makeSoftAssertion(neOp);
            else
              joint_solver->makeAssertion(neOp);
          }
        }
      }
    }
    joint_elems[vid] = std::move(elems);
  }

  auto solveJointWith = [&](std::vector<NodePtr> const& assumptions, bool ignore_softs) -> bool {
    for (VariableContainer::ReadRefPair const& pair : vcon->read_references) {
      if (joint_scalar_ids.count(pair.first)) joint_solver->makeAssumption(*pair.second->expr());
    }
    if (!ignore_softs) {
      for (VariableContainer::ReadRefPair const& pair : vcon->dist_references) {
        if (!joint_scalar_ids.count(pair.first)) continue;
        auto map_it = vcon->dist_ref_to_var_map.find(pair.first);
        if (map_it == vcon->dist_ref_to_var_map.end()) continue;
        if (randc_state(map_it->second)) continue;
        joint_solver->makeSuggestion(*pair.second->expr());
      }
    }
    for (NodePtr const& assumption : assumptions) joint_solver->makeAssumption(*assumption);
    return joint_solver->solve(ignore_softs);
  };

  std::map<int, std::shared_ptr<RandcState> > joint_randc;
  std::vector<NodePtr> stage1_assumptions;
  for (int id : joint_scalar_ids) {
    std::shared_ptr<RandcState> st = randc_state(id);
    if (!st) continue;
    joint_randc[id] = st;
    auto var_it = vcon->variables.find(id);
    if (var_it == vcon->variables.end()) continue;
    std::string str;
    if (var_gen.read(id, str)) {
      if (VariableExpr const* ve = dynamic_cast<VariableExpr const*>(var_it->second.get())) {
        std::string concrete_bits;
        bool had_dont_care = false;
        Constant c = materializeToConstant(*ve, str, concrete_bits, had_dont_care);
        stage1_assumptions.push_back(new EqualOpr(var_it->second, new Constant(c)));
      }
    }
  }

  bool solved = solveJointWith(stage1_assumptions, false);
  if (!solved && !joint_randc.empty()) {
    var_gen.undoRandc(joint_scalar_ids);
    std::vector<RandcUndo> local_randc_undo;
    std::vector<NodePtr> randc_assumptions;

    auto solveJointRandc = [&](int id, RandcState& state) -> bool {
      auto var_it = vcon->variables.find(id);
      if (var_it == vcon->variables.end()) return false;
      NodePtr var_node = var_it->second;
      VariableExpr const* v = dynamic_cast<VariableExpr const*>(var_node.get());
      if (!v) throw std::runtime_error("A randc variable must be a variable.");
      if (v->bitsize() > randc_max_bitsize)
        throw std::runtime_error("A randc variable must not have more than " + std::to_string(randc_max_bitsize) +
                                 " bits.");

      std::string sig = var_gen.randcSignature(id);
      if (sig.empty()) sig = "joint#" + std::to_string(id);

      RandcUndo undo = {id, &state, std::shared_ptr<RandcState>(), 0, false};
      if (!state.started || state.signature != sig) {
        undo.saved = std::make_shared<RandcState>(state);
        startCycle(state, v->bitsize());
        state.signature = sig;
      }

      auto tryValues = [&](std::vector<uint64_t>& values, unsigned max_tries) -> bool {
        std::size_t n = values.size();
        for (unsigned tries = 0; n > 0 && tries < max_tries; ++tries) {
          std::swap(values[random_unsigned(n)], values[n - 1]);
          --n;
          uint64_t const value = values[n];
          randc_assumptions.push_back(new EqualOpr(var_node, new Constant(randcValue(*v, value))));
          if (solveJointWith(randc_assumptions, true)) {
            undo.value = value;
            undo.listed = std::find(state.listed.begin(), state.listed.end(), value) != state.listed.end();
            local_randc_undo.push_back(undo);
            removeValue(state.unused, value);
            removeValue(state.listed, value);
            state.used.push_back(value);
            return true;
          }
          randc_assumptions.pop_back();
        }
        return false;
      };

      for (unsigned round = 0; round < 2; ++round) {
        if (round == 1) {
          if (state.used.empty()) break;
          LOG(INFO) << "Start a new cycle of randc variable #" << id;
          if (!undo.saved) undo.saved = std::make_shared<RandcState>(state);
          startCycle(state, v->bitsize());
        }

        if (!state.complete && tryValues(state.unused, randc_max_tries)) return true;
        if (tryValues(state.listed, randc_max_listed)) return true;

        std::vector<uint64_t> listed;
        unsigned const width = v->isBool() ? 1u : v->bitsize();
        bool const windows = width > randc_window_bits && state.used.size() > randc_max_listed;
        if (windows) {
          std::vector<uint64_t> used(state.used);
          std::sort(used.begin(), used.end());
          unsigned const windows_cnt = 1u << (width - randc_window_bits);
          unsigned const first = random_unsigned(windows_cnt);
          std::size_t const n = randc_assumptions.size();
          for (unsigned w = 0; w < windows_cnt && listed.size() < randc_max_listed; ++w) {
            uint64_t const prefix = (first + w) % windows_cnt;
            std::vector<uint64_t> excluded(std::lower_bound(used.begin(), used.end(), prefix << randc_window_bits),
                                           std::lower_bound(used.begin(), used.end(), (prefix + 1) << randc_window_bits));
            NodePtr in_window(new EqualOpr(new Bitslice(var_node, width - 1, randc_window_bits),
                                           new Constant(prefix, width - randc_window_bits, false)));
            while (listed.size() < randc_max_listed) {
              randc_assumptions.push_back(in_window);
              if (!excluded.empty()) randc_assumptions.push_back(new NotOpr(randcInside(var_node, excluded)));
              bool const found = solveJointWith(randc_assumptions, true);
              randc_assumptions.resize(n);
              if (!found) break;
              std::string str;
              joint_solver->read(*var_node, str);
              uint64_t value = 0;
              for (char c : str) value = (value << 1) | (c == '1' ? 1 : 0);
              if (v->isBool() && (str == "true" || str == "1")) value = 1;
              if (std::find(excluded.begin(), excluded.end(), value) != excluded.end()) break;
              excluded.push_back(value);
              listed.push_back(value);
            }
          }
        } else {
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
            randc_assumptions.push_back(inside ? randcInside(var_node, values)
                                               : NodePtr(new NotOpr(randcInside(var_node, values))));
            bool const found = solveJointWith(randc_assumptions, true);
            randc_assumptions.pop_back();
            if (!found) break;
            std::string str;
            joint_solver->read(*var_node, str);
            uint64_t value = 0;
            for (char c : str) value = (value << 1) | (c == '1' ? 1 : 0);
            if (v->isBool() && (str == "true" || str == "1")) value = 1;
            if (excluded.count(value)) break;
            excluded.insert(value);
            listed.push_back(value);
          }
        }
        if (listed.empty()) continue;
        state.complete = listed.size() < randc_max_listed;
        state.listed.swap(listed);
        if (tryValues(state.listed, randc_max_listed)) return true;
      }
      if (undo.saved) local_randc_undo.push_back(undo);
      return false;
    };

    bool randc_ok = true;
    for (auto const& kv : joint_randc) {
      if (!solveJointRandc(kv.first, *kv.second)) {
        randc_ok = false;
        break;
      }
    }
    if (randc_ok) {
      solved = solveJointWith(randc_assumptions, false);
    }
    if (!solved) {
      undoRandcEntries(local_randc_undo);
    } else {
      joint_randc_undo_.insert(joint_randc_undo_.end(), local_randc_undo.begin(), local_randc_undo.end());
    }
  }

  if (!solved) {
    LOG(INFO) << "Joint scalar+vector solve failed for vector " << failed_vec_id;
    return false;
  }

  for (auto const& kv : joint_elems) {
    joint_solver->readVector(kv.second, vectorBaseMap[kv.first]);
    solved_vecs.insert(kv.first);
  }

  for (int id : joint_scalar_ids) {
    auto var_it = vcon->variables.find(id);
    if (var_it == vcon->variables.end()) continue;
    std::string bits;
    if (joint_solver->read(*var_it->second, bits)) {
      if (VariableExpr const* ve = dynamic_cast<VariableExpr const*>(var_it->second.get())) {
        std::string concrete_bits;
        bool had_dont_care = false;
        materializeBits(*ve, bits, concrete_bits, had_dont_care);
        applyScalarOverrideAndWriteRefs(vcon, var_gen, id, concrete_bits);
      } else {
        applyScalarOverrideAndWriteRefs(vcon, var_gen, id, bits);
      }
    }
  }

  LOG(INFO) << "Joint scalar+vector solve succeeded for vector " << failed_vec_id;
  return true;
}

bool VectorGenerator::solve(VariableGenerator& var_gen, const std::set<int>& vec_ids,
                            std::vector<ConstraintPartition> const* partitions, VariableContainer* vcon,
                            std::set<std::string> const* active_covers) {
  if (!vcon) vcon = variable_container();
  failed_ = nullptr;
  joint_randc_undo_.clear();
  std::set<int> solved_vecs;
  for(VectorSolverMap::value_type & c_pair : vector_solvers_) {
    if (solved_vecs.count(c_pair.first)) continue;
    if (!c_pair.second.solve(var_gen, vcon)) {
      if (partitions && solveJoint(var_gen, *partitions, c_pair.first, solved_vecs, vcon, active_covers)) {
        continue;
      }
      undoRandcEntries(joint_randc_undo_);
      failed_ = &c_pair.second;
      return false;
    }
    solved_vecs.insert(c_pair.first);
  }
  for(int id : vec_ids) {
    if (vector_solvers_.find(id) != vector_solvers_.end()) continue;
    // unconstrained vector
    __rand_vec_base* vector = vectorBaseMap[id];
    LOG(INFO) << "Generate unconstrained vector " << id;
    unsigned int size = default_rand_vec_size();
    if (!var_gen.read(vector->size_var(), &size)) {
      LOG(INFO) << "Use default size for vector " << id;
    } else {
      LOG(INFO) << "Size of vector " << id << " = " << size;
    }
    vector->gen_values(size);
  }
  return true;
}

void VectorGenerator::reset(const std::vector<VectorConstraintPtr>& v) {
  failed_ = nullptr;
  joint_randc_undo_.clear();
  vector_solvers_.clear();
  for(VectorConstraintPtr vc : v) { addConstraint(vc); }
}

void VectorGenerator::addConstraint(VectorConstraintPtr vc) {
  int v_id = vc->getVectorId();
  VectorSolverMap::iterator ite(vector_solvers_.lower_bound(v_id));
  if (ite != vector_solvers_.end() && !(vector_solvers_.key_comp()(v_id, ite->first))) {
    ite->second.addConstraint(vc);
  } else {
    VectorSolver vs(v_id);
    vs.addConstraint(vc);
    vector_solvers_.insert(std::make_pair(v_id, vs));
  }
}
}

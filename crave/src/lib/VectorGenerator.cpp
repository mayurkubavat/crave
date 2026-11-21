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

#include <functional>
#include <string>

namespace crave {

extern std::function<bool(void)> random_bit;

namespace {
// converts a value read from a solver, where a bit can also be a don't care
Constant to_constant(std::string const& str, unsigned bitsize, bool sign) {
  uint64_t val = (sign && !str.empty() && str[0] == '1') ? ~uint64_t(0) : 0;
  for (char c : str) {
    val <<= 1;
    if (c == '1' || (c != '0' && random_bit())) val |= 1;
  }
  return Constant(val, bitsize, sign);
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
      bitsize_ = r.bitsize();
      sign_ = r.sign();
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

bool VectorSolver::solve(const VariableGenerator& var_gen) {
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
  failed_with_solved_size_ = false;
  resetSolver(size);

  if (!scalar_vars_.empty()) {
    VariableContainer* vcon = variable_container();
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
      for (VariableContainer::WriteRefPair const& pair : vcon->write_references) {
        if (pair.first == id) {
          solver_->makeAssertion(EqualOpr(var_node, new Constant(pair.second->value_as_constant())));
          matched_ref = true;
          break;
        }
      }
      if (matched_ref) continue;
      std::string str;
      if (var_gen.read(id, str)) {
        if (VariableExpr const* ve = dynamic_cast<VariableExpr const*>(var_node.get())) {
          Constant c = ve->isBool() ? Constant(str == "true" || str == "1"
                                                   ? true
                                                   : (str == "false" || str == "0" ? false : random_bit()))
                                    : to_constant(str, ve->bitsize(), ve->sign());
          solver_->makeAssertion(EqualOpr(var_node, new Constant(c)));
        }
      }
    }
  }

  makeSuggestions();
  bool result = solver_->solve(false);
  if (result) {
    solver_->readVector(vec_elements_, vector);
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

void VectorSolver::resetSolver(unsigned int const size) {
  solver_.reset(FactoryMetaSMT::getNewInstance());
  buildSolver(size);
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

void VectorSolver::buildSolver(unsigned int const size) {
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
      auto vec_ite = variable_container()->vector_variables.find(vector_id_);
      if (vec_ite != variable_container()->vector_variables.end()) {
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
      ReplaceVisitor replacer(&vec_elements_);
      constraint->expr()->visit(&replacer);
      if (replacer.okay()) {
        if (constraint->isSoft())
          solver_->makeSoftAssertion(*replacer.result());
        else
          solver_->makeAssertion(*replacer.result());
      } else if (!constraint->isSoft()) {
        // an element outside the vector is accessed, the constraint does not hold for this size
        LOG(INFO) << "Constraint " << constraint->name() << " accesses an element outside vector " << vector_id_;
        solver_->makeAssertion(Constant(false));
      }
    } else if (!constraint->isUnique()) {
      ReplaceVisitor replacer(&vec_elements_);
      for (unsigned int i = 0u; i < size; ++i) {
        replacer.setVecIdx(i);
        constraint->expr()->visit(&replacer);

        if (replacer.okay()) {
          if (constraint->isSoft()) {
            solver_->makeSoftAssertion(*replacer.result());
          } else {
            solver_->makeAssertion(*replacer.result());

            GetSupportSetVisitor gssv;
            replacer.result()->visit(&gssv);
            std::set<int> const& support = gssv.getSupportVars();
            if (support.size() == 1 && *support.begin() == static_cast<int>(vec_elements_[i]->id()))
              elem_constraints_[i].push_back(replacer.result());
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

VectorGenerator::VectorGenerator() : vector_solvers_(), failed_(nullptr) {}

bool VectorGenerator::failedSize(int* id, unsigned int* size) const {
  if (!failed_ || !failed_->failedWithSolvedSize()) return false;
  *id = failed_->id();
  *size = failed_->size();
  return true;
}

bool VectorGenerator::solve(const VariableGenerator& var_gen, const std::set<int>& vec_ids) {
  failed_ = nullptr;
  for(VectorSolverMap::value_type & c_pair : vector_solvers_) {
    if (!c_pair.second.solve(var_gen)) {
      failed_ = &c_pair.second;
      return false;
    }
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

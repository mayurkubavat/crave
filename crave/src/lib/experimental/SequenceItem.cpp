//****************************************************************************************
//	MIT License
//****************************************************************************************
//	Copyright (c) 2012-2020 University of Bremen, Germany. 
//  	Copyright (c) 2015-2020 DFKI GmbH Bremen, Germany.
//  	Copyright (c) 2020 Johannes Kepler University Linz, Austria.
//      Copyright (c) 2022 - 2023 Coseda Technologies GmbH.
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

#include "../../crave/experimental/SequenceItem.hpp"
#include "../../crave/RandomSeedManager.hpp"
#include "../../crave/backend/FactoryMetaSMT.hpp"
#include "../../crave/backend/Generator.hpp"
#include "../../crave/backend/VariableDefaultSolver.hpp"
#include "../../crave/frontend/RandomBase.hpp"
#include "../../crave/ir/Node.hpp"
#include "../../crave/ir/visitor/NodeVisitor.hpp"

#include <algorithm>
#include <cassert>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace crave {

bool sv_signedness();
int new_var_id();

namespace {

class SlotReferenceExpression : public ReferenceExpression {
 public:
  SlotReferenceExpression(int slot_id, unsigned width, bool sign)
      : slot_var_(new VariableExpr(slot_id, width, sign)),
        current_val_(0, width, sign) {}

  void setValue(Constant const& c) { current_val_ = c; }

  NodePtr const& slotVar() const { return slot_var_; }

  Constant constantValue() const override { return current_val_; }

  ReferenceExpression::result_type expr() const override {
    return new EqualOpr(slot_var_, new Constant(current_val_));
  }

 private:
  NodePtr slot_var_;
  Constant current_val_;
};

class StructuralSignatureVisitor : public NodeVisitor {
 public:
  StructuralSignatureVisitor(std::unordered_map<int, std::size_t> const& inline_read_ids,
                             std::unordered_map<int, std::size_t> const& inline_dist_ids,
                             std::vector<detail::InlineDistCapture> const& inline_dists)
      : inline_read_ids_(inline_read_ids),
        inline_dist_ids_(inline_dist_ids),
        inline_dists_(inline_dists) {}

  void appendClause(NodePtr const& n, bool is_soft) {
    os_ << (is_soft ? ";S:" : ";H:");
    n->visit(this);
  }

  std::string str() const { return os_.str(); }

  std::vector<std::size_t> const& slotReadCaptureIndices() const { return slot_read_capture_indices_; }
  std::vector<std::pair<unsigned, bool> > const& slotReadTypes() const { return slot_read_types_; }
  std::vector<std::size_t> const& slotDistCaptureIndices() const { return slot_dist_capture_indices_; }
  std::vector<std::pair<unsigned, bool> > const& slotDistTypes() const { return slot_dist_types_; }

  void visitNode(Node const&) override { os_ << "NODE"; }
  void visitTerminal(Terminal const& t) override {
    os_ << "TERM(" << t.bitsize() << ":" << t.sign() << ")";
  }
  void visitUnaryExpr(UnaryExpression const& u) override {
    os_ << "UE(";
    u.child()->visit(this);
    os_ << ")";
  }
  void visitUnaryOpr(UnaryOperator const& u) override {
    os_ << "UO(";
    u.child()->visit(this);
    os_ << ")";
  }
  void visitBinaryExpr(BinaryExpression const& b) override {
    os_ << "BE(";
    b.lhs()->visit(this);
    os_ << ",";
    b.rhs()->visit(this);
    os_ << ")";
  }
  void visitBinaryOpr(BinaryOperator const& b) override {
    os_ << "BO(";
    b.lhs()->visit(this);
    os_ << ",";
    b.rhs()->visit(this);
    os_ << ")";
  }
  void visitTernaryExpr(TernaryExpression const& t) override {
    os_ << "TE(";
    t.a()->visit(this);
    os_ << ",";
    t.b()->visit(this);
    os_ << ",";
    t.c()->visit(this);
    os_ << ")";
  }

  void visitPlaceholder(Placeholder const& p) override {
    for (std::size_t i = bound_phs_.size(); i > 0; --i) {
      if (bound_phs_[i - 1] == static_cast<int>(p.id())) {
        os_ << "P($" << (i - 1) << ")";
        return;
      }
    }
    os_ << "P(" << p.id() << ")";
  }

  void visitVariableExpr(VariableExpr const& v) override {
    int const id = static_cast<int>(v.id());
    auto r_it = inline_read_ids_.find(id);
    if (r_it != inline_read_ids_.end()) {
      auto s_it = read_slot_map_.find(id);
      std::size_t slot_idx = 0;
      if (s_it == read_slot_map_.end()) {
        slot_idx = slot_read_capture_indices_.size();
        read_slot_map_[id] = slot_idx;
        slot_read_capture_indices_.push_back(r_it->second);
        slot_read_types_.push_back(std::make_pair(v.bitsize(), v.sign()));
      } else {
        slot_idx = s_it->second;
      }
      os_ << "R(" << slot_idx << ":" << v.bitsize() << ":" << v.sign() << ")";
      return;
    }

    auto d_it = inline_dist_ids_.find(id);
    if (d_it != inline_dist_ids_.end()) {
      auto s_it = dist_slot_map_.find(id);
      std::size_t slot_idx = 0;
      if (s_it == dist_slot_map_.end()) {
        slot_idx = slot_dist_capture_indices_.size();
        dist_slot_map_[id] = slot_idx;
        slot_dist_capture_indices_.push_back(d_it->second);
        slot_dist_types_.push_back(std::make_pair(v.bitsize(), v.sign()));
      } else {
        slot_idx = s_it->second;
      }
      detail::InlineDistCapture const& dc = inline_dists_[d_it->second];
      os_ << "D(" << slot_idx << ":" << v.bitsize() << ":" << v.sign() << ":"
          << dc.target_var_id << ":" << dc.is_implicit << ":"
          << (dc.ref_expr ? dc.ref_expr->distSignature() : std::string()) << ")";
      return;
    }

    os_ << "V(" << id << ":" << v.bitsize() << ":" << v.sign() << ")";
  }

  void visitConstant(Constant const& c) override {
    os_ << "C(" << c.value() << ":" << c.bitsize() << ":" << c.sign() << ")";
  }

  void visitVectorExpr(VectorExpr const& v) override {
    os_ << "VE(" << v.id() << ":" << v.bitsize() << ":" << v.sign() << ")";
  }

  void visitVectorReduction(VectorReduction const& r) override {
    os_ << "VRED(" << r.id() << ":" << r.bitsize() << ":" << r.sign() << ":"
        << static_cast<int>(r.kind()) << ":" << r.elemBitsize() << ":" << r.elemSign() << ")";
  }

  void visitNotOpr(NotOpr const& n) override { unary("NOT", n); }
  void visitNegOpr(NegOpr const& n) override { unary("NEG", n); }
  void visitComplementOpr(ComplementOpr const& n) override { unary("COMP", n); }

  void visitInside(Inside const& i) override {
    os_ << "IN(";
    i.child()->visit(this);
    os_ << ",{";
    for (Constant const& c : i.collection()) {
      os_ << c.value() << ":" << c.bitsize() << ":" << c.sign() << ",";
    }
    os_ << "})";
  }

  void visitExtendExpr(ExtendExpression const& e) override {
    os_ << "EXT(" << e.value() << ",";
    e.child()->visit(this);
    os_ << ")";
  }

  void visitAndOpr(AndOpr const& n) override { binary("BAND", n); }
  void visitOrOpr(OrOpr const& n) override { binary("BOR", n); }
  void visitLogicalAndOpr(LogicalAndOpr const& n) override { binary("LAND", n); }
  void visitLogicalOrOpr(LogicalOrOpr const& n) override { binary("LOR", n); }
  void visitLogicalIffOpr(LogicalIffOpr const& n) override { binary("LIFF", n); }
  void visitXorOpr(XorOpr const& n) override { binary("BXOR", n); }
  void visitEqualOpr(EqualOpr const& n) override { binary("EQ", n); }
  void visitNotEqualOpr(NotEqualOpr const& n) override { binary("NE", n); }
  void visitLessOpr(LessOpr const& n) override { binary("LT", n); }
  void visitLessEqualOpr(LessEqualOpr const& n) override { binary("LE", n); }
  void visitGreaterOpr(GreaterOpr const& n) override { binary("GT", n); }
  void visitGreaterEqualOpr(GreaterEqualOpr const& n) override { binary("GE", n); }
  void visitPlusOpr(PlusOpr const& n) override { binary("ADD", n); }
  void visitMinusOpr(MinusOpr const& n) override { binary("SUB", n); }
  void visitMultipliesOpr(MultipliesOpr const& n) override { binary("MUL", n); }
  void visitDevideOpr(DevideOpr const& n) override { binary("DIV", n); }
  void visitModuloOpr(ModuloOpr const& n) override { binary("MOD", n); }
  void visitShiftLeftOpr(ShiftLeftOpr const& n) override { binary("SHL", n); }
  void visitShiftRightOpr(ShiftRightOpr const& n) override { binary("SHR", n); }
  void visitVectorAccess(VectorAccess const& n) override { binary("VA", n); }
  void visitIfThenElse(IfThenElse const& n) override { ternary("ITE", n); }
  void visitForEach(ForEach const& n) override { binary("FE", n); }
  void visitUnique(Unique const& n) override { unary("UNIQ", n); }

  void visitBitslice(Bitslice const& b) override {
    os_ << "BS(" << b.r() << ":" << b.l() << ",";
    b.child()->visit(this);
    os_ << ")";
  }

  void visitRangeExpr(RangeExpr const& n) override { ternary("RNG", n); }
  void visitTolPlusMinusExpr(TolPlusMinusExpr const& n) override { ternary("TPM", n); }
  void visitTolPlusPctExpr(TolPlusPctExpr const& n) override { ternary("TPP", n); }

  void visitVectorContains(VectorContains const& vc) override {
    os_ << "VC(" << vc.vec_id() << ":" << vc.elem_bitsize() << ":" << vc.elem_sign()
        << ":" << vc.cmp_width() << ":" << vc.cmp_sign() << ",";
    vc.child()->visit(this);
    os_ << ")";
  }

  void visitOneHotExpr(OneHotExpr const& n) override { unary("OH", n); }
  void visitOneHot0Expr(OneHot0Expr const& n) override { unary("OH0", n); }
  void visitCountOnesExpr(CountOnesExpr const& n) override { unary("CO", n); }
  void visitClog2Expr(Clog2Expr const& n) override { unary("CL2", n); }
  void visitSelfDeterminedExpr(SelfDeterminedExpr const& n) override { unary("SD", n); }
  void visitBoundsCheckExpr(BoundsCheckExpr const& n) override { binary("BC", n); }

  void visitVectorReduceWith(VectorReduceWith const& vrw) override {
    os_ << "VRW(" << vrw.vec_id() << ":" << vrw.elemBitsize() << ":" << vrw.elemSign()
        << ":" << static_cast<int>(vrw.kind()) << ":";
    if (vrw.placeholder_id() >= 0) {
      std::size_t const depth = bound_phs_.size();
      os_ << "$" << depth << ",";
      bound_phs_.push_back(vrw.placeholder_id());
      vrw.child()->visit(this);
      bound_phs_.pop_back();
    } else {
      os_ << vrw.placeholder_id() << ",";
      vrw.child()->visit(this);
    }
    os_ << ")";
  }

 private:
  void unary(char const* op, UnaryExpression const& u) {
    os_ << op << "(";
    u.child()->visit(this);
    os_ << ")";
  }

  void binary(char const* op, BinaryExpression const& b) {
    os_ << op << "(";
    b.lhs()->visit(this);
    os_ << ",";
    b.rhs()->visit(this);
    os_ << ")";
  }

  void ternary(char const* op, TernaryExpression const& t) {
    os_ << op << "(";
    t.a()->visit(this);
    os_ << ",";
    t.b()->visit(this);
    os_ << ",";
    t.c()->visit(this);
    os_ << ")";
  }

  std::unordered_map<int, std::size_t> const& inline_read_ids_;
  std::unordered_map<int, std::size_t> const& inline_dist_ids_;
  std::vector<detail::InlineDistCapture> const& inline_dists_;
  std::ostringstream os_;
  std::unordered_map<int, std::size_t> read_slot_map_;
  std::vector<std::size_t> slot_read_capture_indices_;
  std::vector<std::pair<unsigned, bool> > slot_read_types_;
  std::unordered_map<int, std::size_t> dist_slot_map_;
  std::vector<std::size_t> slot_dist_capture_indices_;
  std::vector<std::pair<unsigned, bool> > slot_dist_types_;
  std::vector<int> bound_phs_;
};

class NodeCloneRemapVisitor : public NodeVisitor {
 public:
  explicit NodeCloneRemapVisitor(std::unordered_map<int, int> const& remap) : remap_(remap) {}

  NodePtr clone(NodePtr const& n) {
    result_.reset();
    n->visit(this);
    return result_;
  }

  void visitNode(Node const&) override { assert(false && "Unhandled Node in NodeCloneRemapVisitor"); }
  void visitTerminal(Terminal const&) override { assert(false && "Unhandled Terminal in NodeCloneRemapVisitor"); }
  void visitUnaryExpr(UnaryExpression const&) override {
    assert(false && "Unhandled UnaryExpression in NodeCloneRemapVisitor");
  }
  void visitUnaryOpr(UnaryOperator const&) override {
    assert(false && "Unhandled UnaryOperator in NodeCloneRemapVisitor");
  }
  void visitBinaryExpr(BinaryExpression const&) override {
    assert(false && "Unhandled BinaryExpression in NodeCloneRemapVisitor");
  }
  void visitBinaryOpr(BinaryOperator const&) override {
    assert(false && "Unhandled BinaryOperator in NodeCloneRemapVisitor");
  }
  void visitTernaryExpr(TernaryExpression const&) override {
    assert(false && "Unhandled TernaryExpression in NodeCloneRemapVisitor");
  }

  void visitPlaceholder(Placeholder const& p) override { result_ = new Placeholder(p); }

  void visitVariableExpr(VariableExpr const& v) override {
    int id = static_cast<int>(v.id());
    auto it = remap_.find(id);
    if (it != remap_.end()) id = it->second;
    result_ = new VariableExpr(static_cast<unsigned int>(id), v.bitsize(), v.sign());
  }

  void visitConstant(Constant const& c) override { result_ = new Constant(c); }
  void visitVectorExpr(VectorExpr const& v) override { result_ = new VectorExpr(v); }
  void visitVectorReduction(VectorReduction const& r) override { result_ = new VectorReduction(r); }

  void visitNotOpr(NotOpr const& n) override { result_ = new NotOpr(clone(n.child())); }
  void visitNegOpr(NegOpr const& n) override { result_ = new NegOpr(clone(n.child())); }
  void visitComplementOpr(ComplementOpr const& n) override { result_ = new ComplementOpr(clone(n.child())); }
  void visitInside(Inside const& i) override { result_ = new Inside(clone(i.child()), i.collection()); }
  void visitExtendExpr(ExtendExpression const& e) override {
    result_ = new ExtendExpression(clone(e.child()), e.value());
  }

  void visitAndOpr(AndOpr const& n) override { result_ = new AndOpr(clone(n.lhs()), clone(n.rhs())); }
  void visitOrOpr(OrOpr const& n) override { result_ = new OrOpr(clone(n.lhs()), clone(n.rhs())); }
  void visitLogicalAndOpr(LogicalAndOpr const& n) override {
    result_ = new LogicalAndOpr(clone(n.lhs()), clone(n.rhs()));
  }
  void visitLogicalOrOpr(LogicalOrOpr const& n) override {
    result_ = new LogicalOrOpr(clone(n.lhs()), clone(n.rhs()));
  }
  void visitLogicalIffOpr(LogicalIffOpr const& n) override {
    result_ = new LogicalIffOpr(clone(n.lhs()), clone(n.rhs()));
  }
  void visitXorOpr(XorOpr const& n) override { result_ = new XorOpr(clone(n.lhs()), clone(n.rhs())); }
  void visitEqualOpr(EqualOpr const& n) override { result_ = new EqualOpr(clone(n.lhs()), clone(n.rhs())); }
  void visitNotEqualOpr(NotEqualOpr const& n) override {
    result_ = new NotEqualOpr(clone(n.lhs()), clone(n.rhs()));
  }
  void visitLessOpr(LessOpr const& n) override { result_ = new LessOpr(clone(n.lhs()), clone(n.rhs())); }
  void visitLessEqualOpr(LessEqualOpr const& n) override {
    result_ = new LessEqualOpr(clone(n.lhs()), clone(n.rhs()));
  }
  void visitGreaterOpr(GreaterOpr const& n) override { result_ = new GreaterOpr(clone(n.lhs()), clone(n.rhs())); }
  void visitGreaterEqualOpr(GreaterEqualOpr const& n) override {
    result_ = new GreaterEqualOpr(clone(n.lhs()), clone(n.rhs()));
  }
  void visitPlusOpr(PlusOpr const& n) override { result_ = new PlusOpr(clone(n.lhs()), clone(n.rhs())); }
  void visitMinusOpr(MinusOpr const& n) override { result_ = new MinusOpr(clone(n.lhs()), clone(n.rhs())); }
  void visitMultipliesOpr(MultipliesOpr const& n) override {
    result_ = new MultipliesOpr(clone(n.lhs()), clone(n.rhs()));
  }
  void visitDevideOpr(DevideOpr const& n) override { result_ = new DevideOpr(clone(n.lhs()), clone(n.rhs())); }
  void visitModuloOpr(ModuloOpr const& n) override { result_ = new ModuloOpr(clone(n.lhs()), clone(n.rhs())); }
  void visitShiftLeftOpr(ShiftLeftOpr const& n) override {
    result_ = new ShiftLeftOpr(clone(n.lhs()), clone(n.rhs()));
  }
  void visitShiftRightOpr(ShiftRightOpr const& n) override {
    result_ = new ShiftRightOpr(clone(n.lhs()), clone(n.rhs()));
  }
  void visitVectorAccess(VectorAccess const& n) override {
    result_ = new VectorAccess(clone(n.lhs()), clone(n.rhs()));
  }
  void visitIfThenElse(IfThenElse const& n) override {
    result_ = new IfThenElse(clone(n.a()), clone(n.b()), clone(n.c()));
  }
  void visitForEach(ForEach const& n) override { result_ = new ForEach(clone(n.lhs()), clone(n.rhs())); }
  void visitUnique(Unique const& n) override { result_ = new Unique(clone(n.child())); }
  void visitBitslice(Bitslice const& b) override { result_ = new Bitslice(clone(b.child()), b.r(), b.l()); }
  void visitRangeExpr(RangeExpr const& n) override {
    result_ = new RangeExpr(clone(n.a()), clone(n.b()), clone(n.c()));
  }
  void visitTolPlusMinusExpr(TolPlusMinusExpr const& n) override {
    result_ = new TolPlusMinusExpr(clone(n.a()), clone(n.b()), clone(n.c()));
  }
  void visitTolPlusPctExpr(TolPlusPctExpr const& n) override {
    result_ = new TolPlusPctExpr(clone(n.a()), clone(n.b()), clone(n.c()));
  }
  void visitVectorContains(VectorContains const& vc) override {
    result_ = new VectorContains(clone(vc.child()), vc.vec_id(), vc.elem_bitsize(), vc.elem_sign(),
                                 vc.cmp_width(), vc.cmp_sign());
  }
  void visitOneHotExpr(OneHotExpr const& n) override { result_ = new OneHotExpr(clone(n.child())); }
  void visitOneHot0Expr(OneHot0Expr const& n) override { result_ = new OneHot0Expr(clone(n.child())); }
  void visitCountOnesExpr(CountOnesExpr const& n) override { result_ = new CountOnesExpr(clone(n.child())); }
  void visitClog2Expr(Clog2Expr const& n) override { result_ = new Clog2Expr(clone(n.child())); }
  void visitSelfDeterminedExpr(SelfDeterminedExpr const& n) override {
    result_ = new SelfDeterminedExpr(clone(n.child()));
  }
  void visitBoundsCheckExpr(BoundsCheckExpr const& n) override {
    result_ = new BoundsCheckExpr(clone(n.lhs()), clone(n.rhs()));
  }
  void visitVectorReduceWith(VectorReduceWith const& vrw) override {
    result_ = new VectorReduceWith(vrw.vec_id(), vrw.elemBitsize(), vrw.elemSign(), vrw.kind(),
                                   clone(vrw.child()), vrw.placeholder_id());
  }

 private:
  std::unordered_map<int, int> const& remap_;
  NodePtr result_;
};

void populateOwnedVarContainer(Generator& gen,
                               std::vector<std::pair<int, std::shared_ptr<SlotReferenceExpression> > > const& slot_reads,
                               std::vector<std::pair<int, std::pair<NodePtr, std::shared_ptr<ReferenceExpression> > > > const& slot_dists,
                               std::vector<int> const& slot_dist_targets,
                               std::vector<bool> const& slot_dist_implicit) {
  VariableContainer const* global = variable_container();
  VariableContainer* local = gen.getVarCtn();

  std::set<int> needed_ids;
  for (ConstraintPtr const& c : gen.constraintManager().constraints()) {
    if (!c->isEnabled()) continue;
    needed_ids.insert(c->supportVars().begin(), c->supportVars().end());
  }
  for (int vid : gen.vecIds()) {
    needed_ids.insert(vid);
  }
  for (auto const& p : gen.solveOrder().pairs()) {
    needed_ids.insert(p.first);
    needed_ids.insert(p.second);
  }

  std::vector<int> extra_ids;
  {
    std::lock_guard<std::recursive_mutex> vbm_lock(vector_base_map_mutex());
    for (int id : needed_ids) {
      auto vbm_it = vectorBaseMap.find(id);
      if (vbm_it != vectorBaseMap.end()) {
        extra_ids.push_back(static_cast<int>(vbm_it->second->size_var().id()));
      }
      auto dmap_it = global->dist_ref_to_var_map.find(id);
      if (dmap_it != global->dist_ref_to_var_map.end()) {
        extra_ids.push_back(dmap_it->second);
      }
    }
  }
  needed_ids.insert(extra_ids.begin(), extra_ids.end());

  for (int id : needed_ids) {
    auto vit = global->variables.find(id);
    if (vit != global->variables.end()) local->variables[id] = vit->second;
    auto vec_it = global->vector_variables.find(id);
    if (vec_it != global->vector_variables.end()) local->vector_variables[id] = vec_it->second;
  }
  std::set<int> local_write_ids;
  for (VariableContainer::WriteRefPair const& pair : local->write_references) {
    local_write_ids.insert(pair.first);
  }
  std::set<int> local_read_ids;
  for (VariableContainer::ReadRefPair const& pair : local->read_references) {
    local_read_ids.insert(pair.first);
  }
  std::set<int> local_dist_ids;
  for (VariableContainer::ReadRefPair const& pair : local->dist_references) {
    local_dist_ids.insert(pair.first);
  }

  for (VariableContainer::WriteRefPair const& pair : global->write_references) {
    if (needed_ids.count(pair.first) && !local_write_ids.count(pair.first)) {
      local->write_references.push_back(pair);
    }
  }
  for (VariableContainer::ReadRefPair const& pair : global->read_references) {
    if (needed_ids.count(pair.first) && !local_read_ids.count(pair.first)) {
      local->read_references.push_back(pair);
    }
  }
  for (VariableContainer::ReadRefPair const& pair : global->dist_references) {
    if (needed_ids.count(pair.first) && !local_dist_ids.count(pair.first)) {
      local->dist_references.push_back(pair);
      auto map_it = global->dist_ref_to_var_map.find(pair.first);
      if (map_it != global->dist_ref_to_var_map.end()) {
        local->dist_ref_to_var_map[pair.first] = map_it->second;
      }
      if (global->implicit_dist_references.count(pair.first)) {
        local->implicit_dist_references.insert(pair.first);
      }
      if (global->user_dist_references.count(pair.first)) {
        local->user_dist_references.insert(pair.first);
      }
    }
  }

  for (auto const& sr : slot_reads) {
    int const slot_id = sr.first;
    local->variables[slot_id] = sr.second->slotVar();
    local->read_references.push_back(std::make_pair(slot_id, sr.second));
  }
  for (std::size_t i = 0; i < slot_dists.size(); ++i) {
    int const slot_dist_id = slot_dists[i].first;
    local->variables[slot_dist_id] = slot_dists[i].second.first;
    local->dist_references.push_back(std::make_pair(slot_dist_id, slot_dists[i].second.second));
    local->dist_ref_to_var_map[slot_dist_id] = slot_dist_targets[i];
    if (slot_dist_implicit[i])
      local->implicit_dist_references.insert(slot_dist_id);
    else
      local->user_dist_references.insert(slot_dist_id);
  }
}

}  // namespace

struct RandWithCache {
  static constexpr std::size_t kMaxEntries = 64;

  struct Entry {
    std::string key;
    std::shared_ptr<Generator> gen;
    std::vector<std::shared_ptr<SlotReferenceExpression> > slots;
    std::vector<std::size_t> slot_capture_indices;
  };

  std::shared_ptr<Entry> lookup(std::string const& key) {
    auto it = index.find(key);
    if (it == index.end()) return std::shared_ptr<Entry>();
    lru.splice(lru.begin(), lru, it->second);
    return *(it->second);
  }

  void insert(std::shared_ptr<Entry> const& entry) {
    auto it = index.find(entry->key);
    if (it != index.end()) {
      *(it->second) = entry;
      lru.splice(lru.begin(), lru, it->second);
      return;
    }
    lru.push_front(entry);
    index[entry->key] = lru.begin();
    while (lru.size() > kMaxEntries) {
      auto last = std::prev(lru.end());
      index.erase((*last)->key);
      lru.pop_back();
    }
  }

  void clear() {
    index.clear();
    lru.clear();
  }

  std::list<std::shared_ptr<Entry> > lru;
  std::unordered_map<std::string, std::list<std::shared_ptr<Entry> >::iterator> index;
};

void crv_covergroup::attach_item_(crv_sequence_item* item) {
  if (!item) return;
  if (std::find(goal_items_.begin(), goal_items_.end(), item) == goal_items_.end()) {
    goal_items_.push_back(item);
  }
}

void crv_covergroup::detach_item_(crv_sequence_item* item) {
  goal_items_.erase(std::remove(goal_items_.begin(), goal_items_.end(), item), goal_items_.end());
}

crv_covergroup::~crv_covergroup() {
  std::vector<crv_sequence_item*> items;
  items.swap(goal_items_);
  for (crv_sequence_item* item : items) {
    if (item) item->detach_goal_(this);
  }
}

crv_sequence_item::crv_sequence_item()
    : gen_(),
      rand_with_gen_(),
      last_gen_(),
      built_(false),
      cloned_(false),
      goals_(),
      goal_epoch_(0),
      next_goal_uid_(0),
      rand_with_cache_(std::make_shared<RandWithCache>()),
      rand_with_cache_hits_(0),
      rand_with_cache_misses_(0) {}

crv_sequence_item::crv_sequence_item(crv_sequence_item const& other)
    : crv_object(other),
      gen_(),
      rand_with_gen_(),
      last_gen_(),
      built_(false),
      cloned_(true),
      goals_(),
      goal_epoch_(other.goal_epoch_),
      next_goal_uid_(other.next_goal_uid_),
      rand_with_cache_(),
      rand_with_cache_hits_(0),
      rand_with_cache_misses_(0) {}

crv_sequence_item& crv_sequence_item::operator=(crv_sequence_item const& other) {
  if (this == &other) return *this;
  detach_all_goals_();
  gen_.reset();
  rand_with_gen_.reset();
  last_gen_.reset();
  built_ = false;
  cloned_ = true;
  goal_epoch_ = std::max(goal_epoch_, other.goal_epoch_) + 1;
  next_goal_uid_ = std::max(next_goal_uid_, other.next_goal_uid_);
  clear_rand_with_cache_();
  rand_with_cache_hits_ = 0;
  rand_with_cache_misses_ = 0;
  return *this;
}

crv_sequence_item::~crv_sequence_item() { detach_all_goals_(); }

void crv_sequence_item::detach_all_goals_() {
  std::vector<SavedGoal> local_goals;
  local_goals.swap(goals_);
  if (!cloned_) {
    for (SavedGoal const& sg : local_goals) {
      if (sg.group) sg.group->detach_item_(this);
    }
  }
}

void crv_sequence_item::detach_goal_(crv_covergroup const* group) {
  if (!group || cloned_) return;
  std::size_t const old_size = goals_.size();
  goals_.erase(
      std::remove_if(goals_.begin(), goals_.end(),
                     [group](SavedGoal const& sg) { return sg.group == group; }),
      goals_.end());
  if (goals_.size() == old_size) return;
  ++goal_epoch_;
  clear_rand_with_cache_();
  built_ = false;
  gen_.reset();
  rand_with_gen_.reset();
  last_gen_.reset();
}

std::string crv_sequence_item::obj_kind() const { return "crv_sequence_item"; }

void crv_sequence_item::crosscheck_constraints() {
  // Implicit default distributions (c_var_uniform) are suppressed per-Generator
  // during partitioning whenever an active user dist() constraint targets the
  // same variable, so c_var_uniform must never be permanently deactivated.
}

void crv_sequence_item::clear_rand_with_cache_() const {
  if (rand_with_cache_) rand_with_cache_->clear();
}

std::size_t crv_sequence_item::randomize_with_cache_size() const {
  return rand_with_cache_ ? rand_with_cache_->lru.size() : 0;
}

void crv_sequence_item::apply_goals_(Generator& target) {
  for (SavedGoal& g : goals_) {
    if (g.group) {
      g.bindings = g.group->bound_var_expr_list();
      expression_list uncovered = g.group->uncovered_as_list();
      std::uint64_t const uid = ++next_goal_uid_;
      g.covers.clear();
      g.covers.reserve(uncovered.size());
      for (std::size_t i = 0; i < uncovered.size(); ++i) {
        g.covers.push_back(
            SavedCover{"__goal_" + std::to_string(uid) + "_" + std::to_string(i), uncovered[i]});
      }
    }
    for (auto e : g.bindings) target(e);
    for (SavedCover const& sc : g.covers) target.cover(sc.name, sc.expr);
  }
}

Generator& crv_sequence_item::ensure_generator_() {
  assert(!cloned_ && "cloned crv_sequence_item cannot be randomized");
  if (!last_gen_) {
    crosscheck_constraints();
    prune_completed_goals_();
    if (!built_) {
      std::lock_guard<std::recursive_mutex> vc_lock(variable_container_mutex());
      auto owned_vcon = std::unique_ptr<VariableContainer>(new VariableContainer());
      gen_ = std::make_shared<Generator>(std::move(owned_vcon));
      recursive_build(*gen_);
      apply_goals_(*gen_);
      populateOwnedVarContainer(*gen_, {}, {}, {}, {});
      built_ = true;
    }
    last_gen_ = gen_;
  }
  return *last_gen_;
}

std::vector<std::vector<std::string> > crv_sequence_item::analyse_contradiction() {
  return ensure_generator_().analyseContradiction();
}

std::vector<std::string> crv_sequence_item::inactive_soft_constraints() {
  return ensure_generator_().getInactiveSofts();
}

bool crv_sequence_item::randomize() {
  assert(!cloned_ && "cloned crv_sequence_item cannot be randomized");
  ScopedRngGuard rng_guard(&ensure_rng_());
  recursive_pre_randomize();
  crosscheck_constraints();
  prune_completed_goals_();

  bool refreshed = false;
  if (built_ && gen_ && gen_->isCovered() && !goals_.empty()) {
    refresh_goals_();
    refreshed = true;
  }

  if (!built_) {
    std::lock_guard<std::recursive_mutex> vc_lock(variable_container_mutex());
    auto owned_vcon = std::unique_ptr<VariableContainer>(new VariableContainer());
    gen_ = std::make_shared<Generator>(std::move(owned_vcon));
    recursive_build(*gen_);
    apply_goals_(*gen_);
    populateOwnedVarContainer(*gen_, {}, {}, {}, {});
    built_ = true;
  }
  last_gen_ = gen_;
  bool const was_covered = gen_->isCovered();
  bool ok = gen_->nextCov();
  if (ok && !refreshed && !was_covered && gen_->isCovered() && !goals_.empty()) {
    refresh_goals_();
    if (!goals_.empty()) {
      std::lock_guard<std::recursive_mutex> vc_lock(variable_container_mutex());
      auto owned_vcon = std::unique_ptr<VariableContainer>(new VariableContainer());
      gen_ = std::make_shared<Generator>(std::move(owned_vcon));
      recursive_build(*gen_);
      apply_goals_(*gen_);
      populateOwnedVarContainer(*gen_, {}, {}, {}, {});
      built_ = true;
      last_gen_ = gen_;
      ok = gen_->nextCov();
    }
  }
  if (!ok) return false;
  recursive_post_randomize();
  return true;
}

void crv_sequence_item::prune_completed_goals_() {
  if (goals_.empty()) return;
  std::size_t const old_goals = goals_.size();
  prune_covered_goals_();
  if (goals_.size() < old_goals) {
    ++goal_epoch_;
    clear_rand_with_cache_();
    built_ = false;
    gen_.reset();
    rand_with_gen_.reset();
    last_gen_.reset();
  }
}

bool crv_sequence_item::prune_covered_goals_() {
  if (goals_.empty()) return false;

  bool pruned_any = false;
  for (auto it = goals_.begin(); it != goals_.end();) {
    if (!it->group || it->group->covered() || it->group->uncovered_as_list().size() == 0) {
      pruned_any = true;
      if (it->group) it->group->detach_item_(this);
      it = goals_.erase(it);
    } else {
      ++it;
    }
  }
  return pruned_any;
}

void crv_sequence_item::refresh_goals_() {
  if (goals_.empty()) return;
  for (auto it = goals_.begin(); it != goals_.end();) {
    if (!it->group || it->group->covered()) {
      if (it->group) it->group->detach_item_(this);
      it = goals_.erase(it);
      continue;
    }
    expression_list covers = it->group->uncovered_as_list();
    if (covers.size() == 0) {
      it->group->detach_item_(this);
      it = goals_.erase(it);
      continue;
    }
    it->bindings = it->group->bound_var_expr_list();
    std::uint64_t const uid = ++next_goal_uid_;
    it->covers.clear();
    it->covers.reserve(covers.size());
    for (std::size_t i = 0; i < covers.size(); ++i) {
      it->covers.push_back(
          SavedCover{"__goal_" + std::to_string(uid) + "_" + std::to_string(i), covers[i]});
    }
    ++it;
  }
  ++goal_epoch_;
  clear_rand_with_cache_();
  built_ = false;
  gen_.reset();
  rand_with_gen_.reset();
  last_gen_.reset();
}

void crv_sequence_item::goal(crv_covergroup& group) {
  assert(!cloned_ && "cloned crv_sequence_item cannot be randomized");
  if (cloned_) return;
  bool const pruned_any = prune_covered_goals_();

  expression_list bindings = group.bound_var_expr_list();
  expression_list covers = group.uncovered_as_list();

  auto existing = std::find_if(goals_.begin(), goals_.end(),
                               [&group](SavedGoal const& g) { return g.group == &group; });

  if (covers.size() == 0) {
    if (existing != goals_.end() || pruned_any) {
      if (existing != goals_.end()) {
        if (existing->group) existing->group->detach_item_(this);
        goals_.erase(existing);
      }
      ++goal_epoch_;
      clear_rand_with_cache_();
      last_gen_.reset();
      rand_with_gen_.reset();
      if (built_) {
        built_ = false;
        gen_.reset();
      }
    }
    return;
  }

  std::uint64_t const uid = ++next_goal_uid_;
  std::vector<SavedCover> named_covers;
  named_covers.reserve(covers.size());
  for (std::size_t i = 0; i < covers.size(); ++i) {
    named_covers.push_back(
        SavedCover{"__goal_" + std::to_string(uid) + "_" + std::to_string(i), covers[i]});
  }

  SavedGoal updated{&group, bindings, std::move(named_covers)};
  bool const is_new = (existing == goals_.end());
  group.attach_item_(this);
  try {
    if (!is_new) {
      *existing = std::move(updated);
    } else {
      goals_.push_back(std::move(updated));
    }
  } catch (...) {
    if (is_new) group.detach_item_(this);
    throw;
  }
  ++goal_epoch_;
  clear_rand_with_cache_();
  last_gen_.reset();
  rand_with_gen_.reset();
  if (built_) {
    built_ = false;
    gen_.reset();
  }
}

void crv_sequence_item::on_rng_state_changed_() const {
  built_ = false;
  gen_.reset();
  rand_with_gen_.reset();
  last_gen_.reset();
  clear_rand_with_cache_();
}

void crv_sequence_item::request_rebuild() {
  if (prune_covered_goals_()) ++goal_epoch_;
  built_ = false;
  gen_.reset();
  rand_with_gen_.reset();
  last_gen_.reset();
  clear_rand_with_cache_();
  crv_object::request_rebuild();
}

bool crv_sequence_item::randomize_with_expr_list(expression_list const& list) {
  std::vector<detail::InlineReadCapture> empty_reads;
  std::vector<detail::InlineDistCapture> empty_dists;
  return randomize_with_expr_list_impl_(list, empty_reads, empty_dists);
}

bool crv_sequence_item::randomize_with_expr_list_impl_(
    expression_list const& list,
    std::vector<detail::InlineReadCapture> const& inline_reads,
    std::vector<detail::InlineDistCapture> const& inline_dists) {
  assert(!cloned_ && "cloned crv_sequence_item cannot be randomized");
  ScopedRngGuard rng_guard(&ensure_rng_());
  recursive_pre_randomize();
  crosscheck_constraints();
  prune_completed_goals_();

  if (!rand_with_cache_) rand_with_cache_ = std::make_shared<RandWithCache>();

  std::unordered_map<int, std::size_t> inline_read_ids;
  inline_read_ids.reserve(inline_reads.size());
  for (std::size_t i = 0; i < inline_reads.size(); ++i) {
    inline_read_ids[inline_reads[i].id] = i;
  }

  std::unordered_map<int, std::size_t> inline_dist_ids;
  inline_dist_ids.reserve(inline_dists.size());
  for (std::size_t i = 0; i < inline_dists.size(); ++i) {
    inline_dist_ids[inline_dists[i].id] = i;
  }

  StructuralSignatureVisitor sig(inline_read_ids, inline_dist_ids, inline_dists);
  for (std::size_t i = 0; i < list.size(); ++i) {
    NodePtr raw = boost::proto::value(list[i]);
    sig.appendClause(raw, list.is_soft(i));
  }
  std::string const body_sig = sig.str();
  auto make_key = [&](std::size_t epoch) {
    std::ostringstream kos;
    kos << "H(" << sv_signedness() << ":" << static_cast<int>(FactoryMetaSMT::effectiveSolverType()) << ":"
        << VariableDefaultSolver::bypass_constraint_analysis << ":" << epoch << ":" << list.size() << ")";
    return kos.str() + body_sig;
  };

  auto build_entry = [&](std::string const& entry_key) -> std::shared_ptr<RandWithCache::Entry> {
    auto new_entry = std::make_shared<RandWithCache::Entry>();
    new_entry->key = entry_key;
    new_entry->slot_capture_indices = sig.slotReadCaptureIndices();

    std::unordered_map<int, int> remap;
    std::vector<std::pair<int, std::shared_ptr<SlotReferenceExpression> > > slot_reads;
    auto const& read_cap_indices = sig.slotReadCaptureIndices();
    auto const& read_types = sig.slotReadTypes();
    new_entry->slots.reserve(read_cap_indices.size());
    slot_reads.reserve(read_cap_indices.size());
    for (std::size_t k = 0; k < read_cap_indices.size(); ++k) {
      int const orig_id = inline_reads[read_cap_indices[k]].id;
      int const slot_id = new_var_id();
      remap[orig_id] = slot_id;
      auto slot_ref = std::make_shared<SlotReferenceExpression>(slot_id, read_types[k].first, read_types[k].second);
      new_entry->slots.push_back(slot_ref);
      slot_reads.push_back(std::make_pair(slot_id, slot_ref));
    }

    auto const& dist_cap_indices = sig.slotDistCaptureIndices();
    auto const& dist_types = sig.slotDistTypes();
    std::vector<std::pair<int, std::pair<NodePtr, std::shared_ptr<ReferenceExpression> > > > slot_dists;
    std::vector<int> slot_dist_targets;
    std::vector<bool> slot_dist_implicit;
    slot_dists.reserve(dist_cap_indices.size());
    slot_dist_targets.reserve(dist_cap_indices.size());
    slot_dist_implicit.reserve(dist_cap_indices.size());
    for (std::size_t k = 0; k < dist_cap_indices.size(); ++k) {
      detail::InlineDistCapture const& dc = inline_dists[dist_cap_indices[k]];
      int const slot_dist_id = new_var_id();
      remap[dc.id] = slot_dist_id;
      NodePtr dist_var(new VariableExpr(static_cast<unsigned int>(slot_dist_id),
                                        dist_types[k].first, dist_types[k].second));
      std::shared_ptr<ReferenceExpression> cloned_dist = dc.ref_expr->cloneWithVar(dist_var);
      slot_dists.push_back(std::make_pair(slot_dist_id, std::make_pair(dist_var, cloned_dist)));
      slot_dist_targets.push_back(dc.target_var_id);
      slot_dist_implicit.push_back(dc.is_implicit);
    }

    {
      std::lock_guard<std::recursive_mutex> vc_lock(variable_container_mutex());
      auto owned_vcon = std::unique_ptr<VariableContainer>(new VariableContainer());
      new_entry->gen = std::make_shared<Generator>(std::move(owned_vcon));
      recursive_build(*new_entry->gen);
      apply_goals_(*new_entry->gen);

      NodeCloneRemapVisitor cloner(remap);
      for (std::size_t i = 0; i < list.size(); ++i) {
        std::string const cname = fullname() + ".randomize_with#" + std::to_string(i);
        NodePtr raw = boost::proto::value(list[i]);
        NodePtr remapped = remap.empty() ? raw : cloner.clone(raw);
        expression remapped_expr = boost::proto::make_expr<boost::proto::tag::terminal, Constraint_Domain>(remapped);
        if (list.is_soft(i))
          new_entry->gen->soft(cname, remapped_expr);
        else
          (*new_entry->gen)(cname, remapped_expr);
      }

      populateOwnedVarContainer(*new_entry->gen, slot_reads, slot_dists, slot_dist_targets, slot_dist_implicit);
    }
    return new_entry;
  };

  std::string key = make_key(goal_epoch_);
  std::shared_ptr<RandWithCache::Entry> entry = rand_with_cache_->lookup(key);
  bool refreshed = false;
  if (entry) {
    ++rand_with_cache_hits_;
    if (entry->gen && entry->gen->isCovered() && !goals_.empty()) {
      refresh_goals_();
      refreshed = true;
      key = make_key(goal_epoch_);
      entry = build_entry(key);
      rand_with_cache_->insert(entry);
    }
    rand_with_gen_ = entry->gen;
  } else {
    ++rand_with_cache_misses_;
    entry = build_entry(key);
    rand_with_cache_->insert(entry);
    rand_with_gen_ = entry->gen;
  }

  std::vector<Constant> slot_values;
  slot_values.reserve(entry->slots.size());
  for (std::size_t k = 0; k < entry->slots.size(); ++k) {
    std::size_t const cap_idx = entry->slot_capture_indices[k];
    Constant val = inline_reads[cap_idx].ref_expr->constantValue();
    slot_values.push_back(val);
    entry->slots[k]->setValue(val);
  }

  last_gen_ = rand_with_gen_;
  bool const was_covered = rand_with_gen_->isCovered();
  bool ok = rand_with_gen_->nextCov();
  if (ok && !refreshed && !was_covered && rand_with_gen_->isCovered() && !goals_.empty()) {
    refresh_goals_();
    key = make_key(goal_epoch_);
    entry = build_entry(key);
    rand_with_cache_->insert(entry);
    rand_with_gen_ = entry->gen;
    for (std::size_t k = 0; k < entry->slots.size(); ++k) {
      entry->slots[k]->setValue(slot_values[k]);
    }
    last_gen_ = rand_with_gen_;
    ok = rand_with_gen_->nextCov();
  }
  if (!ok) return false;
  recursive_post_randomize();
  return true;
}

}  // namespace crave

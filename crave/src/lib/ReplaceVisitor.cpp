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


#include "../crave/ir/visitor/ReplaceVisitor.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>

namespace crave {

bool sv_signedness();

namespace {

uint64_t maskBits(unsigned w) {
  if (w == 0) return 0ULL;
  return (w >= 64) ? ~uint64_t(0) : ((uint64_t(1) << w) - 1ULL);
}

int64_t signExtendBits(uint64_t bits, unsigned w) {
  if (w == 0) return 0;
  uint64_t masked = bits & maskBits(w);
  if (w < 64 && ((masked >> (w - 1)) & 1ULL)) {
    masked |= ~maskBits(w);
  }
  return static_cast<int64_t>(masked);
}

uint64_t extendBits(uint64_t bits, unsigned from_w, bool is_signed, unsigned to_w) {
  uint64_t v = bits & maskBits(from_w);
  if (is_signed && from_w > 0 && from_w < 64 && ((v >> (from_w - 1)) & 1ULL)) {
    v |= ~maskBits(from_w);
  }
  return v & maskBits(to_w);
}

__int128 toInt128Val(ReplaceVisitor::SubscriptVal const& v) {
  if (v.is_bool) return v.bits ? 1 : 0;
  if (v.is_signed) return static_cast<__int128>(signExtendBits(v.bits, v.width));
  return static_cast<__int128>(v.bits & maskBits(v.width));
}

ReplaceVisitor::SubscriptVal makeInvalidSubscript(bool has_ph = false, bool used_ph = false) {
  return ReplaceVisitor::SubscriptVal{0ULL, 1u, false, false, false, has_ph, used_ph};
}

ReplaceVisitor::SubscriptVal promoteSubscript(ReplaceVisitor::SubscriptVal v, bool sv_rules) {
  if (!v.is_bool) return v;
  v.bits = v.bits ? 1ULL : 0ULL;
  v.width = sv_rules ? 1u : static_cast<unsigned>(sizeof(int) * 8);
  v.is_signed = !sv_rules;
  v.is_bool = false;
  return v;
}

ReplaceVisitor::SubscriptVal convertSubscript(ReplaceVisitor::SubscriptVal v, unsigned target_w, bool target_s,
                                              bool sv_rules) {
  v = promoteSubscript(v, sv_rules);
  v.bits = extendBits(v.bits, v.width, v.is_signed, target_w);
  v.width = target_w;
  v.is_signed = target_s;
  return v;
}

NodePtr bgAnd(NodePtr const& a, NodePtr const& b) {
  if (!a) return b;
  if (!b) return a;
  return new LogicalAndOpr(a, b);
}

NodePtr bgImplies(NodePtr const& cond, NodePtr const& g) {
  if (!g) return NodePtr();
  return new IfThenElse(cond, g, new Constant(true));
}

NodePtr bgOr(NodePtr const& cond, NodePtr const& g) {
  if (!g) return NodePtr();
  return new IfThenElse(cond, new Constant(true), g);
}

NodePtr bgIte(NodePtr const& cond, NodePtr const& t_g, NodePtr const& e_g) {
  if (!t_g && !e_g) return NodePtr();
  NodePtr t_node = t_g ? t_g : NodePtr(new Constant(true));
  NodePtr e_node = e_g ? e_g : NodePtr(new Constant(true));
  return new IfThenElse(cond, t_node, e_node);
}

bool containsBoolExpr(Node const& n) {
  if (is_boolean_ir_node(n)) return true;
  if (VectorAccess const* va = dynamic_cast<VectorAccess const*>(&n)) {
    return containsBoolExpr(*va->rhs());
  }
  if (dynamic_cast<ForEach const*>(&n) || dynamic_cast<Unique const*>(&n)) {
    return false;
  }
  if (VectorReduceWith const* vrw = dynamic_cast<VectorReduceWith const*>(&n)) {
    return containsBoolExpr(*vrw->child());
  }
  if (BoundsCheckExpr const* bce = dynamic_cast<BoundsCheckExpr const*>(&n)) {
    return containsBoolExpr(*bce->lhs()) || containsBoolExpr(*bce->rhs());
  }
  if (UnaryExpression const* u = dynamic_cast<UnaryExpression const*>(&n)) {
    return containsBoolExpr(*u->child());
  }
  if (BinaryExpression const* b = dynamic_cast<BinaryExpression const*>(&n)) {
    return containsBoolExpr(*b->lhs()) || containsBoolExpr(*b->rhs());
  }
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&n)) {
    return containsBoolExpr(*ite->a()) || containsBoolExpr(*ite->b()) || containsBoolExpr(*ite->c());
  }
  return false;
}

bool allowComparisonSkip(Node const& lhs, Node const& rhs, bool current_skip) {
  if (!current_skip) return false;
  bool const l_bool = is_boolean_ir_node(lhs);
  bool const r_bool = is_boolean_ir_node(rhs);
  if (l_bool || r_bool) {
    return l_bool && r_bool && dynamic_cast<VectorAccess const*>(&lhs) &&
           dynamic_cast<VectorAccess const*>(&rhs);
  }
  return !containsBoolExpr(lhs) && !containsBoolExpr(rhs);
}
}  // namespace

void ReplaceVisitor::evalUnaryExpr(UnaryExpression const& unary_expr, NodePtr& node) {
  visitUnaryExpr(unary_expr);
  assert(aux_stack_.size() >= 1);
  node = aux_stack_.top();
  aux_stack_.pop();
}

void ReplaceVisitor::evalBinExpr(BinaryExpression const& bin_expr, NodePtr& lhs, NodePtr& rhs) {
  visitBinaryExpr(bin_expr);
  assert(aux_stack_.size() >= 2);
  rhs = aux_stack_.top();
  aux_stack_.pop();
  lhs = aux_stack_.top();
  aux_stack_.pop();
}

void ReplaceVisitor::evalTernExpr(TernaryExpression const& tern_expr, NodePtr& a, NodePtr& b, NodePtr& c) {
  visitTernaryExpr(tern_expr);
  assert(aux_stack_.size() >= 3);
  c = aux_stack_.top();
  aux_stack_.pop();
  b = aux_stack_.top();
  aux_stack_.pop();
  a = aux_stack_.top();
  aux_stack_.pop();
}

void ReplaceVisitor::evalBinSubscript(SubscriptVal& lhs, SubscriptVal& rhs) {
  assert(subscript_stack_.size() >= 2);
  rhs = subscript_stack_.top();
  subscript_stack_.pop();
  lhs = subscript_stack_.top();
  subscript_stack_.pop();
}

void ReplaceVisitor::evalTernSubscript(SubscriptVal& a, SubscriptVal& b, SubscriptVal& c) {
  assert(subscript_stack_.size() >= 3);
  c = subscript_stack_.top();
  subscript_stack_.pop();
  b = subscript_stack_.top();
  subscript_stack_.pop();
  a = subscript_stack_.top();
  subscript_stack_.pop();
}

NodePtr ReplaceVisitor::popBounds() {
  assert(bounds_stack_.size() >= 1);
  NodePtr bg = bounds_stack_.top();
  bounds_stack_.pop();
  return bg;
}

void ReplaceVisitor::evalBinBounds(NodePtr& lhs, NodePtr& rhs) {
  assert(bounds_stack_.size() >= 2);
  rhs = bounds_stack_.top();
  bounds_stack_.pop();
  lhs = bounds_stack_.top();
  bounds_stack_.pop();
}

void ReplaceVisitor::evalTernBounds(NodePtr& a, NodePtr& b, NodePtr& c) {
  assert(bounds_stack_.size() >= 3);
  c = bounds_stack_.top();
  bounds_stack_.pop();
  b = bounds_stack_.top();
  bounds_stack_.pop();
  a = bounds_stack_.top();
  bounds_stack_.pop();
}

void ReplaceVisitor::visitNode(Node const&) {}
void ReplaceVisitor::visitTerminal(Terminal const&) {}
void ReplaceVisitor::visitUnaryExpr(UnaryExpression const& e) { e.child()->visit(this); }
void ReplaceVisitor::visitUnaryOpr(UnaryOperator const&) {}
void ReplaceVisitor::visitBinaryExpr(BinaryExpression const& e) {
  e.lhs()->visit(this);
  e.rhs()->visit(this);
}
void ReplaceVisitor::visitBinaryOpr(BinaryOperator const&) {}
void ReplaceVisitor::visitTernaryExpr(TernaryExpression const& t) {
  t.a()->visit(this);
  t.b()->visit(this);
  t.c()->visit(this);
}
void ReplaceVisitor::visitPlaceholder(Placeholder const& p) {
  bool const match = (target_placeholder_id_ == -1) || (p.id() == target_placeholder_id_);
  std::map<int, NodePtr>::iterator ite(terminals_.lower_bound(p.id()));
  if (ite != terminals_.end() && !(terminals_.key_comp()(p.id(), ite->first))) {
    aux_stack_.push(ite->second);
  } else {
    NodePtr node =
        match ? NodePtr(new Constant(vec_idx_, placeholder_bitsize(), false)) : NodePtr(new Placeholder(p));
    aux_stack_.push(node);
    terminals_.insert(std::make_pair(p.id(), node));
  }
  subscript_stack_.push(SubscriptVal{match ? static_cast<uint64_t>(vec_idx_) : 0ULL, placeholder_bitsize(), false,
                                     false, match, !match, match});
  bounds_stack_.push(NodePtr());
  updateResult();
}
void ReplaceVisitor::visitVariableExpr(VariableExpr const& v) {
  std::map<int, NodePtr>::iterator ite(terminals_.lower_bound(v.id()));
  if (ite != terminals_.end() && !(terminals_.key_comp()(v.id(), ite->first))) {
    aux_stack_.push(ite->second);
  } else {
    aux_stack_.push(new VariableExpr(v));
    terminals_.insert(std::make_pair(v.id(), aux_stack_.top()));
  }
  subscript_stack_.push(makeInvalidSubscript());
  bounds_stack_.push(NodePtr());
  updateResult();
}
void ReplaceVisitor::visitConstant(Constant const& c) {
  aux_stack_.push(new Constant(c));
  if (c.isBool()) {
    subscript_stack_.push(SubscriptVal{c.value() ? 1ULL : 0ULL, 1u, false, true, true, false, false});
  } else if (c.bitsize() == 0 || c.bitsize() > 64) {
    subscript_stack_.push(makeInvalidSubscript());
  } else {
    uint64_t const val = c.value() & maskBits(c.bitsize());
    subscript_stack_.push(SubscriptVal{val, c.bitsize(), c.sign(), false, true, false, false});
  }
  bounds_stack_.push(NodePtr());
  updateResult();
}
void ReplaceVisitor::visitVectorExpr(VectorExpr const& v) {
  std::map<int, NodePtr>::iterator ite(terminals_.lower_bound(v.id()));
  if (ite != terminals_.end() && !(terminals_.key_comp()(v.id(), ite->first))) {
    aux_stack_.push(ite->second);
  } else {
    aux_stack_.push(new VectorExpr(v));
    terminals_.insert(std::make_pair(v.id(), aux_stack_.top()));
  }
  subscript_stack_.push(makeInvalidSubscript());
  bounds_stack_.push(NodePtr());
  updateResult();
}
namespace {
// combines the reduction of the elements before an element with the element
NodePtr reduce(VectorReduction const& r, NodePtr const& lhs, NodePtr const& rhs) {
  if (r.isBool()) {
    switch (r.kind()) {
      case VectorReduction::Product:
      case VectorReduction::And:
        return new LogicalAndOpr(lhs, rhs);
      case VectorReduction::Or:
        return new LogicalOrOpr(lhs, rhs);
      case VectorReduction::Xor:
        return new NotEqualOpr(lhs, rhs);
      default:
        throw std::runtime_error("Unsupported reduction on a bool vector.");
    }
  }
  switch (r.kind()) {
    case VectorReduction::Sum:
      return new PlusOpr(lhs, rhs);
    case VectorReduction::Product:
      return new MultipliesOpr(lhs, rhs);
    case VectorReduction::And:
      return new AndOpr(lhs, rhs);
    case VectorReduction::Or:
      return new OrOpr(lhs, rhs);
    default:
      return new XorOpr(lhs, rhs);
  }
}

// the reduction of no elements
NodePtr identity(VectorReduction const& r) {
  if (r.isBool()) {
    return new Constant(r.kind() == VectorReduction::Product || r.kind() == VectorReduction::And);
  }
  uint64_t const ones = r.sign() || r.bitsize() >= 64 ? ~uint64_t(0) : (uint64_t(1) << r.bitsize()) - 1;
  switch (r.kind()) {
    case VectorReduction::Product:
      return new Constant(1, r.bitsize(), r.sign());
    case VectorReduction::And:
      return new Constant(ones, r.bitsize(), r.sign());
    default:
      return new Constant(0, r.bitsize(), r.sign());
  }
}
}  // namespace

// The reduction of the elements of the vector, in the width and the signedness of the elements.
void ReplaceVisitor::visitVectorReduction(VectorReduction const& r) {
  if (!variables_) {
    throw std::runtime_error("crv_vector operation cannot be expanded inside crv_array foreach/reduction.");
  }
  NodePtr result;
  bool const bool_sum = (r.kind() == VectorReduction::Sum && r.elemIsBool());
  for (unsigned int i = 0; i < variables_->size(); ++i) {
    VariableExpr& var = *variables_->at(i);
    variables_->at(i) = new VariableExpr(var.id(), r.elemBitsize(), r.elemSign());
    NodePtr elem = variables_->at(i);
    if (bool_sum) {
      elem = new IfThenElse(elem, new Constant(1, r.bitsize(), r.sign()),
                            new Constant(0, r.bitsize(), r.sign()));
    }
    result = result ? reduce(r, result, elem) : elem;
  }
  aux_stack_.push(result ? result : identity(r));
  subscript_stack_.push(makeInvalidSubscript());
  bounds_stack_.push(NodePtr());
  updateResult();
}

void ReplaceVisitor::visitNotOpr(NotOpr const& n) {
  ScopedSkip guard(allow_unguarded_oob_skip_, false);
  NodePtr child;
  evalUnaryExpr(n, child);
  NodePtr bg = popBounds();
  SubscriptVal idx = subscript_stack_.top();
  subscript_stack_.pop();
  aux_stack_.push(new NotOpr(child));
  subscript_stack_.push(SubscriptVal{idx.valid ? (idx.bits == 0 ? 1ULL : 0ULL) : 0ULL, 1u, false, true, idx.valid,
                                     idx.has_placeholder, idx.used_target_placeholder});
  bounds_stack_.push(bg);
  updateResult();
}
void ReplaceVisitor::visitNegOpr(NegOpr const& n) {
  ScopedSkip guard(allow_unguarded_oob_skip_, containsBoolExpr(*n.child()) ? false : allow_unguarded_oob_skip_);
  NodePtr child;
  evalUnaryExpr(n, child);
  NodePtr bg = popBounds();
  SubscriptVal idx = subscript_stack_.top();
  subscript_stack_.pop();
  aux_stack_.push(new NegOpr(child));
  bool const sv_rules = sv_signedness();
  if (idx.valid) {
    idx = promoteSubscript(idx, sv_rules);
    if (idx.width == 0 || idx.width > 64) {
      subscript_stack_.push(makeInvalidSubscript(idx.has_placeholder, idx.used_target_placeholder));
    } else {
      uint64_t const res = (uint64_t(0) - idx.bits) & maskBits(idx.width);
      subscript_stack_.push(
          SubscriptVal{res, idx.width, idx.is_signed, false, true, idx.has_placeholder, idx.used_target_placeholder});
    }
  } else {
    subscript_stack_.push(makeInvalidSubscript(idx.has_placeholder, idx.used_target_placeholder));
  }
  bounds_stack_.push(bg);
  updateResult();
}
void ReplaceVisitor::visitComplementOpr(ComplementOpr const& c) {
  ScopedSkip guard(allow_unguarded_oob_skip_, containsBoolExpr(*c.child()) ? false : allow_unguarded_oob_skip_);
  NodePtr child;
  evalUnaryExpr(c, child);
  NodePtr bg = popBounds();
  SubscriptVal idx = subscript_stack_.top();
  subscript_stack_.pop();
  aux_stack_.push(new ComplementOpr(child));
  bool const sv_rules = sv_signedness();
  if (idx.valid) {
    idx = promoteSubscript(idx, sv_rules);
    if (idx.width == 0 || idx.width > 64) {
      subscript_stack_.push(makeInvalidSubscript(idx.has_placeholder, idx.used_target_placeholder));
    } else {
      uint64_t const res = (~idx.bits) & maskBits(idx.width);
      subscript_stack_.push(
          SubscriptVal{res, idx.width, idx.is_signed, false, true, idx.has_placeholder, idx.used_target_placeholder});
    }
  } else {
    subscript_stack_.push(makeInvalidSubscript(idx.has_placeholder, idx.used_target_placeholder));
  }
  bounds_stack_.push(bg);
  updateResult();
}
void ReplaceVisitor::visitInside(Inside const& i) {
  ScopedSkip guard(allow_unguarded_oob_skip_, false);
  NodePtr child;
  evalUnaryExpr(i, child);
  NodePtr bg = popBounds();
  SubscriptVal child_sub = subscript_stack_.top();
  subscript_stack_.pop();
  aux_stack_.push(new Inside(child, i.collection()));
  subscript_stack_.push(makeInvalidSubscript(child_sub.has_placeholder, child_sub.used_target_placeholder));
  bounds_stack_.push(bg);
  updateResult();
}
void ReplaceVisitor::visitExtendExpr(ExtendExpression const& e) {
  ScopedSkip guard(allow_unguarded_oob_skip_, containsBoolExpr(*e.child()) ? false : allow_unguarded_oob_skip_);
  NodePtr child;
  evalUnaryExpr(e, child);
  NodePtr bg = popBounds();
  SubscriptVal s = subscript_stack_.top();
  subscript_stack_.pop();
  aux_stack_.push(new ExtendExpression(child, e.value()));
  if (s.valid && !s.is_bool && s.width + e.value() <= 64) {
    uint64_t const ext = extendBits(s.bits, s.width, s.is_signed, s.width + e.value());
    subscript_stack_.push(
        SubscriptVal{ext, s.width + e.value(), s.is_signed, false, true, s.has_placeholder, s.used_target_placeholder});
  } else {
    subscript_stack_.push(makeInvalidSubscript(s.has_placeholder, s.used_target_placeholder));
  }
  bounds_stack_.push(bg);
  updateResult();
}
void ReplaceVisitor::visitAndOpr(AndOpr const& a) {
  bool const has_bool = containsBoolExpr(*a.lhs()) || containsBoolExpr(*a.rhs());
  ScopedSkip guard(allow_unguarded_oob_skip_, has_bool ? false : allow_unguarded_oob_skip_);
  NodePtr lhs, rhs;
  evalBinExpr(a, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new AndOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  bool const sv_rules = sv_signedness();
  if (l_val.valid && r_val.valid) {
    l_val = promoteSubscript(l_val, sv_rules);
    r_val = promoteSubscript(r_val, sv_rules);
    unsigned const w = std::max(l_val.width, r_val.width);
    if (w == 0 || w > 64) {
      subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
    } else {
      bool const s = sv_rules ? (l_val.is_signed && r_val.is_signed) : (l_val.is_signed || r_val.is_signed);
      uint64_t const lb = sv_rules && !s ? (l_val.bits & maskBits(l_val.width))
                                         : extendBits(l_val.bits, l_val.width, l_val.is_signed, w);
      uint64_t const rb = sv_rules && !s ? (r_val.bits & maskBits(r_val.width))
                                         : extendBits(r_val.bits, r_val.width, r_val.is_signed, w);
      subscript_stack_.push(SubscriptVal{(lb & rb) & maskBits(w), w, s, false, true, has_ph, used_ph});
    }
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitOrOpr(OrOpr const& o) {
  bool const has_bool = containsBoolExpr(*o.lhs()) || containsBoolExpr(*o.rhs());
  ScopedSkip guard(allow_unguarded_oob_skip_, has_bool ? false : allow_unguarded_oob_skip_);
  NodePtr lhs, rhs;
  evalBinExpr(o, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new OrOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  bool const sv_rules = sv_signedness();
  if (l_val.valid && r_val.valid) {
    l_val = promoteSubscript(l_val, sv_rules);
    r_val = promoteSubscript(r_val, sv_rules);
    unsigned const w = std::max(l_val.width, r_val.width);
    if (w == 0 || w > 64) {
      subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
    } else {
      bool const s = sv_rules ? (l_val.is_signed && r_val.is_signed) : (l_val.is_signed || r_val.is_signed);
      uint64_t const lb = sv_rules && !s ? (l_val.bits & maskBits(l_val.width))
                                         : extendBits(l_val.bits, l_val.width, l_val.is_signed, w);
      uint64_t const rb = sv_rules && !s ? (r_val.bits & maskBits(r_val.width))
                                         : extendBits(r_val.bits, r_val.width, r_val.is_signed, w);
      subscript_stack_.push(SubscriptVal{(lb | rb) & maskBits(w), w, s, false, true, has_ph, used_ph});
    }
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitLogicalAndOpr(LogicalAndOpr const& a) {
  ScopedSkip guard(allow_unguarded_oob_skip_, false);
  a.lhs()->visit(this);
  assert(aux_stack_.size() >= 1 && subscript_stack_.size() >= 1 && bounds_stack_.size() >= 1);
  NodePtr lhs = aux_stack_.top();
  aux_stack_.pop();
  SubscriptVal l_val = subscript_stack_.top();
  subscript_stack_.pop();
  NodePtr l_bg = bounds_stack_.top();
  bounds_stack_.pop();

  if (l_val.valid && l_val.bits == 0) {
    aux_stack_.push(new Constant(false));
    subscript_stack_.push(
        SubscriptVal{0ULL, 1u, false, true, true, l_val.has_placeholder, l_val.used_target_placeholder});
    bounds_stack_.push(l_bg);
    updateResult();
    return;
  }

  a.rhs()->visit(this);
  assert(aux_stack_.size() >= 1 && subscript_stack_.size() >= 1 && bounds_stack_.size() >= 1);
  NodePtr rhs = aux_stack_.top();
  aux_stack_.pop();
  SubscriptVal r_val = subscript_stack_.top();
  subscript_stack_.pop();
  NodePtr r_bg = bounds_stack_.top();
  bounds_stack_.pop();

  aux_stack_.push(new LogicalAndOpr(lhs, rhs));
  bool const ok = l_val.valid && r_val.valid;
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  subscript_stack_.push(SubscriptVal{ok ? ((l_val.bits != 0 && r_val.bits != 0) ? 1ULL : 0ULL) : 0ULL, 1u, false, true,
                                     ok, has_ph, used_ph});
  bounds_stack_.push(bgAnd(l_bg, bgImplies(lhs, r_bg)));
  updateResult();
}
void ReplaceVisitor::visitLogicalOrOpr(LogicalOrOpr const& o) {
  ScopedSkip guard(allow_unguarded_oob_skip_, false);
  o.lhs()->visit(this);
  assert(aux_stack_.size() >= 1 && subscript_stack_.size() >= 1 && bounds_stack_.size() >= 1);
  NodePtr lhs = aux_stack_.top();
  aux_stack_.pop();
  SubscriptVal l_val = subscript_stack_.top();
  subscript_stack_.pop();
  NodePtr l_bg = bounds_stack_.top();
  bounds_stack_.pop();

  if (l_val.valid && l_val.bits != 0) {
    aux_stack_.push(new Constant(true));
    subscript_stack_.push(
        SubscriptVal{1ULL, 1u, false, true, true, l_val.has_placeholder, l_val.used_target_placeholder});
    bounds_stack_.push(l_bg);
    updateResult();
    return;
  }

  o.rhs()->visit(this);
  assert(aux_stack_.size() >= 1 && subscript_stack_.size() >= 1 && bounds_stack_.size() >= 1);
  NodePtr rhs = aux_stack_.top();
  aux_stack_.pop();
  SubscriptVal r_val = subscript_stack_.top();
  subscript_stack_.pop();
  NodePtr r_bg = bounds_stack_.top();
  bounds_stack_.pop();

  aux_stack_.push(new LogicalOrOpr(lhs, rhs));
  bool const ok = l_val.valid && r_val.valid;
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  subscript_stack_.push(SubscriptVal{ok ? ((l_val.bits != 0 || r_val.bits != 0) ? 1ULL : 0ULL) : 0ULL, 1u, false, true,
                                     ok, has_ph, used_ph});
  bounds_stack_.push(bgAnd(l_bg, bgOr(lhs, r_bg)));
  updateResult();
}
void ReplaceVisitor::visitLogicalIffOpr(LogicalIffOpr const& o) {
  ScopedSkip guard(allow_unguarded_oob_skip_, false);
  NodePtr lhs, rhs;
  evalBinExpr(o, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new LogicalIffOpr(lhs, rhs));
  bool const ok = l_val.valid && r_val.valid;
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  subscript_stack_.push(SubscriptVal{ok ? (((l_val.bits != 0) == (r_val.bits != 0)) ? 1ULL : 0ULL) : 0ULL, 1u, false,
                                     true, ok, has_ph, used_ph});
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitXorOpr(XorOpr const& x) {
  bool const has_bool = containsBoolExpr(*x.lhs()) || containsBoolExpr(*x.rhs());
  ScopedSkip guard(allow_unguarded_oob_skip_, has_bool ? false : allow_unguarded_oob_skip_);
  NodePtr lhs, rhs;
  evalBinExpr(x, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new XorOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  bool const sv_rules = sv_signedness();
  if (l_val.valid && r_val.valid) {
    l_val = promoteSubscript(l_val, sv_rules);
    r_val = promoteSubscript(r_val, sv_rules);
    unsigned const w = std::max(l_val.width, r_val.width);
    if (w == 0 || w > 64) {
      subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
    } else {
      bool const s = sv_rules ? (l_val.is_signed && r_val.is_signed) : (l_val.is_signed || r_val.is_signed);
      uint64_t const lb = sv_rules && !s ? (l_val.bits & maskBits(l_val.width))
                                         : extendBits(l_val.bits, l_val.width, l_val.is_signed, w);
      uint64_t const rb = sv_rules && !s ? (r_val.bits & maskBits(r_val.width))
                                         : extendBits(r_val.bits, r_val.width, r_val.is_signed, w);
      subscript_stack_.push(SubscriptVal{(lb ^ rb) & maskBits(w), w, s, false, true, has_ph, used_ph});
    }
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitEqualOpr(EqualOpr const& e) {
  ScopedSkip guard(allow_unguarded_oob_skip_, allowComparisonSkip(*e.lhs(), *e.rhs(), allow_unguarded_oob_skip_));
  NodePtr lhs, rhs;
  evalBinExpr(e, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new EqualOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  bool const sv_rules = sv_signedness();
  if (l_val.valid && r_val.valid) {
    if (l_val.is_bool && r_val.is_bool) {
      subscript_stack_.push(
          SubscriptVal{((l_val.bits != 0) == (r_val.bits != 0)) ? 1ULL : 0ULL, 1u, false, true, true, has_ph, used_ph});
    } else {
      l_val = promoteSubscript(l_val, sv_rules);
      r_val = promoteSubscript(r_val, sv_rules);
      unsigned const w = std::max(l_val.width, r_val.width);
      if (w == 0 || w > 64) {
        subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
      } else {
        bool const common_s = sv_rules ? (l_val.is_signed && r_val.is_signed) : false;
        uint64_t const lb = sv_rules && !common_s ? (l_val.bits & maskBits(l_val.width))
                                                  : extendBits(l_val.bits, l_val.width, l_val.is_signed, w);
        uint64_t const rb = sv_rules && !common_s ? (r_val.bits & maskBits(r_val.width))
                                                  : extendBits(r_val.bits, r_val.width, r_val.is_signed, w);
        subscript_stack_.push(SubscriptVal{(lb == rb) ? 1ULL : 0ULL, 1u, false, true, true, has_ph, used_ph});
      }
    }
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitNotEqualOpr(NotEqualOpr const& n) {
  ScopedSkip guard(allow_unguarded_oob_skip_, allowComparisonSkip(*n.lhs(), *n.rhs(), allow_unguarded_oob_skip_));
  NodePtr lhs, rhs;
  evalBinExpr(n, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new NotEqualOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  bool const sv_rules = sv_signedness();
  if (l_val.valid && r_val.valid) {
    if (l_val.is_bool && r_val.is_bool) {
      subscript_stack_.push(
          SubscriptVal{((l_val.bits != 0) != (r_val.bits != 0)) ? 1ULL : 0ULL, 1u, false, true, true, has_ph, used_ph});
    } else {
      l_val = promoteSubscript(l_val, sv_rules);
      r_val = promoteSubscript(r_val, sv_rules);
      unsigned const w = std::max(l_val.width, r_val.width);
      if (w == 0 || w > 64) {
        subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
      } else {
        bool const common_s = sv_rules ? (l_val.is_signed && r_val.is_signed) : false;
        uint64_t const lb = sv_rules && !common_s ? (l_val.bits & maskBits(l_val.width))
                                                  : extendBits(l_val.bits, l_val.width, l_val.is_signed, w);
        uint64_t const rb = sv_rules && !common_s ? (r_val.bits & maskBits(r_val.width))
                                                  : extendBits(r_val.bits, r_val.width, r_val.is_signed, w);
        subscript_stack_.push(SubscriptVal{(lb != rb) ? 1ULL : 0ULL, 1u, false, true, true, has_ph, used_ph});
      }
    }
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}

namespace {
bool evalRelational(ReplaceVisitor::SubscriptVal l_val, ReplaceVisitor::SubscriptVal r_val, bool sv_rules, int& cmp_out) {
  l_val = promoteSubscript(l_val, sv_rules);
  r_val = promoteSubscript(r_val, sv_rules);
  unsigned const w = std::max(l_val.width, r_val.width);
  if (w == 0 || w > 64) return false;
  if (sv_rules) {
    bool const common_s = l_val.is_signed && r_val.is_signed;
    uint64_t const lb = common_s ? extendBits(l_val.bits, l_val.width, l_val.is_signed, w)
                                 : (l_val.bits & maskBits(l_val.width));
    uint64_t const rb = common_s ? extendBits(r_val.bits, r_val.width, r_val.is_signed, w)
                                 : (r_val.bits & maskBits(r_val.width));
    if (common_s) {
      int64_t const ls = signExtendBits(lb, w);
      int64_t const rs = signExtendBits(rb, w);
      cmp_out = (ls < rs) ? -1 : ((ls > rs) ? 1 : 0);
    } else {
      cmp_out = (lb < rb) ? -1 : ((lb > rb) ? 1 : 0);
    }
    return true;
  }
  uint64_t const lb = extendBits(l_val.bits, l_val.width, l_val.is_signed, w);
  uint64_t const rb = extendBits(r_val.bits, r_val.width, r_val.is_signed, w);
  if (l_val.is_signed && r_val.is_signed) {
    int64_t const ls = signExtendBits(lb, w);
    int64_t const rs = signExtendBits(rb, w);
    cmp_out = (ls < rs) ? -1 : ((ls > rs) ? 1 : 0);
  } else if (!l_val.is_signed && !r_val.is_signed) {
    cmp_out = (lb < rb) ? -1 : ((lb > rb) ? 1 : 0);
  } else {
    __int128 const li = l_val.is_signed ? static_cast<__int128>(signExtendBits(lb, w)) : static_cast<__int128>(lb);
    __int128 const ri = r_val.is_signed ? static_cast<__int128>(signExtendBits(rb, w)) : static_cast<__int128>(rb);
    cmp_out = (li < ri) ? -1 : ((li > ri) ? 1 : 0);
  }
  return true;
}
}  // namespace

void ReplaceVisitor::visitLessOpr(LessOpr const& l) {
  ScopedSkip guard(allow_unguarded_oob_skip_, allowComparisonSkip(*l.lhs(), *l.rhs(), allow_unguarded_oob_skip_));
  NodePtr lhs, rhs;
  evalBinExpr(l, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new LessOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  int cmp = 0;
  if (l_val.valid && r_val.valid && evalRelational(l_val, r_val, sv_signedness(), cmp)) {
    subscript_stack_.push(SubscriptVal{(cmp < 0) ? 1ULL : 0ULL, 1u, false, true, true, has_ph, used_ph});
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitLessEqualOpr(LessEqualOpr const& l) {
  ScopedSkip guard(allow_unguarded_oob_skip_, allowComparisonSkip(*l.lhs(), *l.rhs(), allow_unguarded_oob_skip_));
  NodePtr lhs, rhs;
  evalBinExpr(l, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new LessEqualOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  int cmp = 0;
  if (l_val.valid && r_val.valid && evalRelational(l_val, r_val, sv_signedness(), cmp)) {
    subscript_stack_.push(SubscriptVal{(cmp <= 0) ? 1ULL : 0ULL, 1u, false, true, true, has_ph, used_ph});
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitGreaterOpr(GreaterOpr const& g) {
  ScopedSkip guard(allow_unguarded_oob_skip_, allowComparisonSkip(*g.lhs(), *g.rhs(), allow_unguarded_oob_skip_));
  NodePtr lhs, rhs;
  evalBinExpr(g, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new GreaterOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  int cmp = 0;
  if (l_val.valid && r_val.valid && evalRelational(l_val, r_val, sv_signedness(), cmp)) {
    subscript_stack_.push(SubscriptVal{(cmp > 0) ? 1ULL : 0ULL, 1u, false, true, true, has_ph, used_ph});
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitGreaterEqualOpr(GreaterEqualOpr const& g) {
  ScopedSkip guard(allow_unguarded_oob_skip_, allowComparisonSkip(*g.lhs(), *g.rhs(), allow_unguarded_oob_skip_));
  NodePtr lhs, rhs;
  evalBinExpr(g, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new GreaterEqualOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  int cmp = 0;
  if (l_val.valid && r_val.valid && evalRelational(l_val, r_val, sv_signedness(), cmp)) {
    subscript_stack_.push(SubscriptVal{(cmp >= 0) ? 1ULL : 0ULL, 1u, false, true, true, has_ph, used_ph});
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitPlusOpr(PlusOpr const& p) {
  bool const has_bool = containsBoolExpr(*p.lhs()) || containsBoolExpr(*p.rhs());
  ScopedSkip guard(allow_unguarded_oob_skip_, has_bool ? false : allow_unguarded_oob_skip_);
  NodePtr lhs, rhs;
  evalBinExpr(p, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new PlusOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  bool const sv_rules = sv_signedness();
  if (l_val.valid && r_val.valid) {
    l_val = promoteSubscript(l_val, sv_rules);
    r_val = promoteSubscript(r_val, sv_rules);
    unsigned const w = std::max(l_val.width, r_val.width);
    if (w == 0 || w > 64) {
      subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
    } else {
      bool const s = sv_rules ? (l_val.is_signed && r_val.is_signed) : (l_val.is_signed || r_val.is_signed);
      uint64_t const lb = sv_rules && !s ? (l_val.bits & maskBits(l_val.width))
                                         : extendBits(l_val.bits, l_val.width, l_val.is_signed, w);
      uint64_t const rb = sv_rules && !s ? (r_val.bits & maskBits(r_val.width))
                                         : extendBits(r_val.bits, r_val.width, r_val.is_signed, w);
      subscript_stack_.push(SubscriptVal{(lb + rb) & maskBits(w), w, s, false, true, has_ph, used_ph});
    }
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitMinusOpr(MinusOpr const& m) {
  bool const has_bool = containsBoolExpr(*m.lhs()) || containsBoolExpr(*m.rhs());
  ScopedSkip guard(allow_unguarded_oob_skip_, has_bool ? false : allow_unguarded_oob_skip_);
  NodePtr lhs, rhs;
  evalBinExpr(m, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new MinusOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  bool const sv_rules = sv_signedness();
  if (l_val.valid && r_val.valid) {
    l_val = promoteSubscript(l_val, sv_rules);
    r_val = promoteSubscript(r_val, sv_rules);
    unsigned const w = std::max(l_val.width, r_val.width);
    if (w == 0 || w > 64) {
      subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
    } else {
      bool const s = sv_rules ? (l_val.is_signed && r_val.is_signed) : (l_val.is_signed || r_val.is_signed);
      uint64_t const lb = sv_rules && !s ? (l_val.bits & maskBits(l_val.width))
                                         : extendBits(l_val.bits, l_val.width, l_val.is_signed, w);
      uint64_t const rb = sv_rules && !s ? (r_val.bits & maskBits(r_val.width))
                                         : extendBits(r_val.bits, r_val.width, r_val.is_signed, w);
      subscript_stack_.push(SubscriptVal{(lb - rb) & maskBits(w), w, s, false, true, has_ph, used_ph});
    }
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitMultipliesOpr(MultipliesOpr const& m) {
  bool const has_bool = containsBoolExpr(*m.lhs()) || containsBoolExpr(*m.rhs());
  ScopedSkip guard(allow_unguarded_oob_skip_, has_bool ? false : allow_unguarded_oob_skip_);
  NodePtr lhs, rhs;
  evalBinExpr(m, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new MultipliesOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  bool const sv_rules = sv_signedness();
  if (l_val.valid && r_val.valid) {
    l_val = promoteSubscript(l_val, sv_rules);
    r_val = promoteSubscript(r_val, sv_rules);
    unsigned const w = std::max(l_val.width, r_val.width);
    if (w == 0 || w > 64) {
      subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
    } else {
      bool const s = sv_rules ? (l_val.is_signed && r_val.is_signed) : (l_val.is_signed || r_val.is_signed);
      uint64_t const lb = sv_rules && !s ? (l_val.bits & maskBits(l_val.width))
                                         : extendBits(l_val.bits, l_val.width, l_val.is_signed, w);
      uint64_t const rb = sv_rules && !s ? (r_val.bits & maskBits(r_val.width))
                                         : extendBits(r_val.bits, r_val.width, r_val.is_signed, w);
      subscript_stack_.push(SubscriptVal{(lb * rb) & maskBits(w), w, s, false, true, has_ph, used_ph});
    }
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitDevideOpr(DevideOpr const& d) {
  bool const has_bool = containsBoolExpr(*d.lhs()) || containsBoolExpr(*d.rhs());
  ScopedSkip guard(allow_unguarded_oob_skip_, has_bool ? false : allow_unguarded_oob_skip_);
  NodePtr lhs, rhs;
  evalBinExpr(d, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new DevideOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  bool const sv_rules = sv_signedness();
  if (l_val.valid && r_val.valid) {
    l_val = promoteSubscript(l_val, sv_rules);
    r_val = promoteSubscript(r_val, sv_rules);
    unsigned w = std::max(l_val.width, r_val.width);
    if (!sv_rules && l_val.is_signed != r_val.is_signed) w += 1u;
    if (w == 0 || w > 64) {
      subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
    } else {
      bool const s = sv_rules ? (l_val.is_signed && r_val.is_signed) : (l_val.is_signed || r_val.is_signed);
      uint64_t const lb = sv_rules && !s ? (l_val.bits & maskBits(l_val.width))
                                         : extendBits(l_val.bits, l_val.width, l_val.is_signed, w);
      uint64_t const rb = sv_rules && !s ? (r_val.bits & maskBits(r_val.width))
                                         : extendBits(r_val.bits, r_val.width, r_val.is_signed, w);
      if (rb == 0) {
        okay_ = false;
        subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
      } else if (s) {
        __int128 const li = static_cast<__int128>(signExtendBits(lb, w));
        __int128 const ri = static_cast<__int128>(signExtendBits(rb, w));
        uint64_t const q = static_cast<uint64_t>(li / ri) & maskBits(w);
        subscript_stack_.push(SubscriptVal{q, w, s, false, true, has_ph, used_ph});
      } else {
        uint64_t const q = (lb / rb) & maskBits(w);
        subscript_stack_.push(SubscriptVal{q, w, s, false, true, has_ph, used_ph});
      }
    }
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitModuloOpr(ModuloOpr const& m) {
  bool const has_bool = containsBoolExpr(*m.lhs()) || containsBoolExpr(*m.rhs());
  ScopedSkip guard(allow_unguarded_oob_skip_, has_bool ? false : allow_unguarded_oob_skip_);
  NodePtr lhs, rhs;
  evalBinExpr(m, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new ModuloOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  bool const sv_rules = sv_signedness();
  if (l_val.valid && r_val.valid) {
    l_val = promoteSubscript(l_val, sv_rules);
    r_val = promoteSubscript(r_val, sv_rules);
    unsigned const w = std::max(l_val.width, r_val.width);
    if (w == 0 || w > 64) {
      subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
    } else {
      bool const s = sv_rules ? (l_val.is_signed && r_val.is_signed) : (l_val.is_signed || r_val.is_signed);
      bool const div_signed = sv_rules ? s : l_val.is_signed;
      uint64_t const lb = sv_rules && !s ? (l_val.bits & maskBits(l_val.width))
                                         : extendBits(l_val.bits, l_val.width, l_val.is_signed, w);
      uint64_t const rb = sv_rules && !s ? (r_val.bits & maskBits(r_val.width))
                                         : extendBits(r_val.bits, r_val.width, r_val.is_signed, w);
      if (rb == 0) {
        okay_ = false;
        subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
      } else if (div_signed) {
        __int128 const li = static_cast<__int128>(signExtendBits(lb, w));
        __int128 const ri = static_cast<__int128>(signExtendBits(rb, w));
        uint64_t const rem = static_cast<uint64_t>(li % ri) & maskBits(w);
        subscript_stack_.push(SubscriptVal{rem, w, s, false, true, has_ph, used_ph});
      } else {
        uint64_t const rem = (lb % rb) & maskBits(w);
        subscript_stack_.push(SubscriptVal{rem, w, s, false, true, has_ph, used_ph});
      }
    }
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitShiftLeftOpr(ShiftLeftOpr const& s) {
  bool const has_bool = containsBoolExpr(*s.lhs()) || containsBoolExpr(*s.rhs());
  ScopedSkip guard(allow_unguarded_oob_skip_, has_bool ? false : allow_unguarded_oob_skip_);
  NodePtr lhs, rhs;
  evalBinExpr(s, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new ShiftLeftOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  bool const sv_rules = sv_signedness();
  if (l_val.valid && r_val.valid) {
    l_val = promoteSubscript(l_val, sv_rules);
    r_val = promoteSubscript(r_val, sv_rules);
    unsigned const w = sv_rules ? l_val.width : std::max(l_val.width, r_val.width);
    if (w == 0 || w > 64) {
      subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
    } else {
      uint64_t const lb = extendBits(l_val.bits, l_val.width, l_val.is_signed, w);
      uint64_t const amt = sv_rules ? (r_val.bits & maskBits(r_val.width))
                                    : extendBits(r_val.bits, r_val.width, r_val.is_signed, w);
      uint64_t const res = (amt >= w) ? 0ULL : ((lb << amt) & maskBits(w));
      subscript_stack_.push(SubscriptVal{res, w, l_val.is_signed, false, true, has_ph, used_ph});
    }
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitShiftRightOpr(ShiftRightOpr const& s) {
  bool const has_bool = containsBoolExpr(*s.lhs()) || containsBoolExpr(*s.rhs());
  ScopedSkip guard(allow_unguarded_oob_skip_, has_bool ? false : allow_unguarded_oob_skip_);
  NodePtr lhs, rhs;
  evalBinExpr(s, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_val, r_val;
  evalBinSubscript(l_val, r_val);
  aux_stack_.push(new ShiftRightOpr(lhs, rhs));
  bool const has_ph = l_val.has_placeholder || r_val.has_placeholder;
  bool const used_ph = l_val.used_target_placeholder || r_val.used_target_placeholder;
  bool const sv_rules = sv_signedness();
  if (l_val.valid && r_val.valid) {
    l_val = promoteSubscript(l_val, sv_rules);
    r_val = promoteSubscript(r_val, sv_rules);
    unsigned const w = sv_rules ? l_val.width : std::max(l_val.width, r_val.width);
    if (w == 0 || w > 64) {
      subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
    } else {
      uint64_t const lb = extendBits(l_val.bits, l_val.width, l_val.is_signed, w);
      uint64_t const amt = sv_rules ? (r_val.bits & maskBits(r_val.width))
                                    : extendBits(r_val.bits, r_val.width, r_val.is_signed, w);
      uint64_t const res = (amt >= w) ? 0ULL : ((lb & maskBits(w)) >> amt);
      subscript_stack_.push(SubscriptVal{res, w, l_val.is_signed, false, true, has_ph, used_ph});
    }
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(l_bg, r_bg));
  updateResult();
}
void ReplaceVisitor::visitVectorAccess(VectorAccess const& v) {
  ScopedSkip guard(allow_unguarded_oob_skip_, containsBoolExpr(*v.rhs()) ? false : allow_unguarded_oob_skip_);
  NodePtr lhs, rhs;
  evalBinExpr(v, lhs, rhs);
  NodePtr l_bg, r_bg;
  evalBinBounds(l_bg, r_bg);
  SubscriptVal l_sub, r_sub;
  evalBinSubscript(l_sub, r_sub);
  NodePtr child_bg = bgAnd(l_bg, r_bg);

  VectorExpr* vec_ptr = dynamic_cast<VectorExpr*>(lhs.get());
  if (!vec_ptr) {
    throw std::runtime_error("VectorAccess LHS must be a VectorExpr in ReplaceVisitor.");
  }
  std::vector<boost::intrusive_ptr<VariableExpr> >* target_vars = variables_;
  bool is_external_array = false;
  if (multi_vec_vars_) {
    auto it = multi_vec_vars_->find(vec_ptr->id());
    if (it != multi_vec_vars_->end()) {
      target_vars = const_cast<std::vector<boost::intrusive_ptr<VariableExpr> >*>(&it->second);
      is_external_array = true;
    }
  }

  if (!target_vars) {
    throw std::runtime_error("crv_vector element access cannot be expanded inside crv_array foreach/reduction.");
  }

  auto get_elem = [&](std::size_t idx) -> NodePtr {
    if (is_external_array) return target_vars->at(idx);
    VariableExpr& var = *static_cast<VariableExpr*>(target_vars->at(idx).get());
    target_vars->at(idx) = new VariableExpr(var.id(), vec_ptr->bitsize(), vec_ptr->sign());
    return target_vars->at(idx);
  };

  if (!r_sub.valid) {
    if (r_sub.has_placeholder) {
      aux_stack_.push(new VectorAccess(lhs, rhs));
      subscript_stack_.push(makeInvalidSubscript(true, r_sub.used_target_placeholder));
      bounds_stack_.push(child_bg);
      updateResult();
      return;
    }
    if (target_vars->empty()) {
      aux_stack_.push(new Constant(0, vec_ptr->bitsize(), vec_ptr->sign()));
      subscript_stack_.push(makeInvalidSubscript(false, r_sub.used_target_placeholder));
      bounds_stack_.push(new Constant(false));
      updateResult();
      return;
    }
    std::size_t const n = target_vars->size();
    if (is_boolean_ir_node(*rhs)) {
      if (n == 1) {
        aux_stack_.push(get_elem(0));
        bounds_stack_.push(bgAnd(child_bg, new NotOpr(rhs)));
      } else {
        aux_stack_.push(new IfThenElse(rhs, get_elem(1), get_elem(0)));
        bounds_stack_.push(child_bg);
      }
      subscript_stack_.push(makeInvalidSubscript(false, r_sub.used_target_placeholder));
      updateResult();
      return;
    }
    bool const sv_rules = sv_signedness();
    unsigned const rhs_w = expr_width_ir_node(*rhs, sv_rules);
    bool const rhs_s = expr_signed_ir_node(*rhs, sv_rules);
    uint64_t max_pos = 0;
    if (rhs_s) {
      max_pos = (rhs_w >= 64) ? static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
                              : ((rhs_w <= 1) ? 0ULL : ((uint64_t(1) << (rhs_w - 1)) - 1ULL));
    } else {
      max_pos = (rhs_w >= 64) ? ~uint64_t(0) : ((uint64_t(1) << rhs_w) - 1ULL);
    }
    NodePtr bounds_cond;
    if (rhs_s) {
      NodePtr ge_zero(new GreaterEqualOpr(rhs, new Constant(0, rhs_w, true)));
      bounds_cond = ge_zero;
      if (static_cast<uint64_t>(n - 1) < max_pos) {
        NodePtr le_max(new LessEqualOpr(rhs, new Constant(static_cast<uint64_t>(n - 1), rhs_w, true)));
        bounds_cond = new LogicalAndOpr(ge_zero, le_max);
      }
    } else {
      if (static_cast<uint64_t>(n - 1) < max_pos) {
        bounds_cond = new LessEqualOpr(rhs, new Constant(static_cast<uint64_t>(n - 1), rhs_w, false));
      }
    }

    std::size_t const limit =
        static_cast<std::size_t>(std::min<uint64_t>(static_cast<uint64_t>(n - 1), max_pos));
    NodePtr mux = get_elem(limit);
    for (std::size_t k = limit; k-- > 0;) {
      NodePtr cond(new EqualOpr(rhs, new Constant(static_cast<uint64_t>(k), rhs_w, rhs_s)));
      mux = new IfThenElse(cond, get_elem(k), mux);
    }
    aux_stack_.push(mux);
    subscript_stack_.push(makeInvalidSubscript(false, r_sub.used_target_placeholder));
    bounds_stack_.push(bgAnd(child_bg, bounds_cond));
    updateResult();
    return;
  }

  bool in_bounds = false;
  std::size_t idx = 0;
  if (r_sub.is_bool) {
    idx = (r_sub.bits != 0) ? 1u : 0u;
    in_bounds = idx < target_vars->size();
  } else if (r_sub.is_signed) {
    int64_t const sval = signExtendBits(r_sub.bits, r_sub.width);
    if (sval >= 0 && static_cast<uint64_t>(sval) < target_vars->size()) {
      idx = static_cast<std::size_t>(sval);
      in_bounds = true;
    }
  } else {
    uint64_t const uval = r_sub.bits & maskBits(r_sub.width);
    if (uval < target_vars->size()) {
      idx = static_cast<std::size_t>(uval);
      in_bounds = true;
    }
  }

  if (in_bounds) {
    aux_stack_.push(get_elem(idx));
    bounds_stack_.push(child_bg);
  } else if (r_sub.used_target_placeholder && allow_unguarded_oob_skip_) {
    if (target_vars->empty())
      aux_stack_.push(new Constant(0, vec_ptr->bitsize(), vec_ptr->sign()));
    else
      aux_stack_.push(target_vars->at(0));
    okay_ = false;
    bounds_stack_.push(child_bg);
  } else {
    aux_stack_.push(new Constant(0, vec_ptr->bitsize(), vec_ptr->sign()));
    bounds_stack_.push(new Constant(false));
  }
  subscript_stack_.push(makeInvalidSubscript(false, r_sub.used_target_placeholder));
  updateResult();
}
void ReplaceVisitor::visitIfThenElse(IfThenElse const& i) {
  ScopedSkip guard(allow_unguarded_oob_skip_, false);
  i.a()->visit(this);
  assert(aux_stack_.size() >= 1 && subscript_stack_.size() >= 1 && bounds_stack_.size() >= 1);
  NodePtr a = aux_stack_.top();
  aux_stack_.pop();
  SubscriptVal a_val = subscript_stack_.top();
  subscript_stack_.pop();
  NodePtr a_bg = bounds_stack_.top();
  bounds_stack_.pop();

  bool const sv_rules = sv_signedness();
  auto makeDummyBranch = [sv_rules](Node const& n) -> NodePtr {
    if (is_boolean_ir_node(n)) return new Constant(false);
    unsigned const w = expr_width_ir_node(n, sv_rules);
    bool const s = expr_signed_ir_node(n, sv_rules);
    return new Constant(0, w > 0 ? w : 1u, s);
  };

  if (a_val.valid) {
    bool const take_then = (a_val.bits != 0);
    if (take_then)
      i.b()->visit(this);
    else
      i.c()->visit(this);
    assert(aux_stack_.size() >= 1 && subscript_stack_.size() >= 1 && bounds_stack_.size() >= 1);
    NodePtr taken_node = aux_stack_.top();
    aux_stack_.pop();
    SubscriptVal taken_val = subscript_stack_.top();
    subscript_stack_.pop();
    NodePtr taken_bg = bounds_stack_.top();
    bounds_stack_.pop();

    if (take_then) {
      aux_stack_.push(new IfThenElse(new Constant(true), taken_node, makeDummyBranch(*i.c())));
    } else {
      aux_stack_.push(new IfThenElse(new Constant(false), makeDummyBranch(*i.b()), taken_node));
    }
    bool const has_ph = a_val.has_placeholder || taken_val.has_placeholder;
    bool const used_ph = a_val.used_target_placeholder || taken_val.used_target_placeholder;
    if (taken_val.valid) {
      if (is_boolean_ir_node(i)) {
        subscript_stack_.push(
            SubscriptVal{(taken_val.bits != 0) ? 1ULL : 0ULL, 1u, false, true, true, has_ph, used_ph});
      } else {
        unsigned const res_w = expr_width_ir_node(i, sv_rules);
        bool const res_s = expr_signed_ir_node(i, sv_rules);
        if (res_w == 0 || res_w > 64) {
          subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
        } else {
          SubscriptVal conv = convertSubscript(taken_val, res_w, res_s, sv_rules);
          conv.has_placeholder = has_ph;
          conv.used_target_placeholder = used_ph;
          subscript_stack_.push(conv);
        }
      }
    } else {
      subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
    }
    bounds_stack_.push(bgAnd(a_bg, taken_bg));
    updateResult();
    return;
  }

  i.b()->visit(this);
  assert(aux_stack_.size() >= 1 && subscript_stack_.size() >= 1 && bounds_stack_.size() >= 1);
  NodePtr b = aux_stack_.top();
  aux_stack_.pop();
  SubscriptVal b_val = subscript_stack_.top();
  subscript_stack_.pop();
  NodePtr b_bg = bounds_stack_.top();
  bounds_stack_.pop();

  i.c()->visit(this);
  assert(aux_stack_.size() >= 1 && subscript_stack_.size() >= 1 && bounds_stack_.size() >= 1);
  NodePtr c = aux_stack_.top();
  aux_stack_.pop();
  SubscriptVal c_val = subscript_stack_.top();
  subscript_stack_.pop();
  NodePtr c_bg = bounds_stack_.top();
  bounds_stack_.pop();

  aux_stack_.push(new IfThenElse(a, b, c));
  subscript_stack_.push(makeInvalidSubscript(
      a_val.has_placeholder || b_val.has_placeholder || c_val.has_placeholder,
      a_val.used_target_placeholder || b_val.used_target_placeholder || c_val.used_target_placeholder));
  bounds_stack_.push(bgAnd(a_bg, bgIte(a, b_bg, c_bg)));
  updateResult();
}

void ReplaceVisitor::visitForEach(ForEach const& fe) {
  ScopedSkip guard(allow_unguarded_oob_skip_,
                   dynamic_cast<VectorAccess const*>(fe.rhs().get()) ? false : allow_unguarded_oob_skip_);
  fe.rhs()->visit(this);
}

void ReplaceVisitor::visitUnique(Unique const&) {
  throw std::runtime_error("Unique is not allowed in ReplaceVisitor.");
}

void ReplaceVisitor::visitBitslice(Bitslice const& b) {
  ScopedSkip guard(allow_unguarded_oob_skip_, containsBoolExpr(*b.child()) ? false : allow_unguarded_oob_skip_);
  NodePtr child;
  evalUnaryExpr(b, child);
  NodePtr bg = popBounds();
  SubscriptVal idx = subscript_stack_.top();
  subscript_stack_.pop();
  aux_stack_.push(new Bitslice(child, b.r(), b.l()));
  if (idx.valid && b.l() >= 0 && b.r() >= b.l() && b.r() < 64) {
    unsigned const width = static_cast<unsigned>(b.r() - b.l() + 1);
    uint64_t const wide = idx.is_bool ? (idx.bits ? 1ULL : 0ULL)
                                      : static_cast<uint64_t>(idx.is_signed ? signExtendBits(idx.bits, idx.width)
                                                                            : (idx.bits & maskBits(idx.width)));
    uint64_t const sliced = (wide >> b.l()) & maskBits(width);
    subscript_stack_.push(
        SubscriptVal{sliced, width, false, false, true, idx.has_placeholder, idx.used_target_placeholder});
  } else {
    subscript_stack_.push(makeInvalidSubscript(idx.has_placeholder, idx.used_target_placeholder));
  }
  bounds_stack_.push(bg);
  updateResult();
}

void ReplaceVisitor::visitRangeExpr(RangeExpr const& r) {
  ScopedSkip guard(allow_unguarded_oob_skip_, false);
  NodePtr a, b, c;
  evalTernExpr(r, a, b, c);
  NodePtr a_bg, b_bg, c_bg;
  evalTernBounds(a_bg, b_bg, c_bg);
  SubscriptVal a_val, b_val, c_val;
  evalTernSubscript(a_val, b_val, c_val);
  aux_stack_.push(new RangeExpr(a, b, c));
  bool const ok = a_val.valid && b_val.valid && c_val.valid;
  bool const has_ph = a_val.has_placeholder || b_val.has_placeholder || c_val.has_placeholder;
  bool const used_ph =
      a_val.used_target_placeholder || b_val.used_target_placeholder || c_val.used_target_placeholder;
  if (ok) {
    __int128 const av = toInt128Val(a_val);
    __int128 const bv = toInt128Val(b_val);
    __int128 const cv = toInt128Val(c_val);
    subscript_stack_.push(SubscriptVal{(bv <= av && av <= cv) ? 1ULL : 0ULL, 1u, false, true, true, has_ph, used_ph});
  } else {
    subscript_stack_.push(makeInvalidSubscript(has_ph, used_ph));
  }
  bounds_stack_.push(bgAnd(a_bg, bgAnd(b_bg, c_bg)));
  updateResult();
}

void ReplaceVisitor::visitTolPlusMinusExpr(TolPlusMinusExpr const& t) {
  ScopedSkip guard(allow_unguarded_oob_skip_, false);
  NodePtr a, b, c;
  evalTernExpr(t, a, b, c);
  NodePtr a_bg, b_bg, c_bg;
  evalTernBounds(a_bg, b_bg, c_bg);
  SubscriptVal a_val, b_val, c_val;
  evalTernSubscript(a_val, b_val, c_val);
  aux_stack_.push(new TolPlusMinusExpr(a, b, c));
  subscript_stack_.push(makeInvalidSubscript(
      a_val.has_placeholder || b_val.has_placeholder || c_val.has_placeholder,
      a_val.used_target_placeholder || b_val.used_target_placeholder || c_val.used_target_placeholder));
  bounds_stack_.push(bgAnd(a_bg, bgAnd(b_bg, c_bg)));
  updateResult();
}

void ReplaceVisitor::visitTolPlusPctExpr(TolPlusPctExpr const& t) {
  ScopedSkip guard(allow_unguarded_oob_skip_, false);
  NodePtr a, b, c;
  evalTernExpr(t, a, b, c);
  NodePtr a_bg, b_bg, c_bg;
  evalTernBounds(a_bg, b_bg, c_bg);
  SubscriptVal a_val, b_val, c_val;
  evalTernSubscript(a_val, b_val, c_val);
  aux_stack_.push(new TolPlusPctExpr(a, b, c));
  subscript_stack_.push(makeInvalidSubscript(
      a_val.has_placeholder || b_val.has_placeholder || c_val.has_placeholder,
      a_val.used_target_placeholder || b_val.used_target_placeholder || c_val.used_target_placeholder));
  bounds_stack_.push(bgAnd(a_bg, bgAnd(b_bg, c_bg)));
  updateResult();
}

void ReplaceVisitor::visitVectorContains(VectorContains const& vc) {
  if (!variables_) {
    throw std::runtime_error("crv_vector operation cannot be expanded inside crv_array foreach/reduction.");
  }
  ScopedSkip guard(allow_unguarded_oob_skip_, false);
  NodePtr child;
  evalUnaryExpr(vc, child);
  NodePtr bg = popBounds();
  SubscriptVal c_sub = subscript_stack_.top();
  subscript_stack_.pop();
  if (variables_->empty()) {
    aux_stack_.push(new Constant(false));
    subscript_stack_.push(SubscriptVal{0ULL, 1u, false, true, true, false, false});
    bounds_stack_.push(bg);
    updateResult();
    return;
  }
  bool const bool_vec = (vc.elem_bitsize() == 1 && vc.elem_sign());
  bool const bool_cmp = (vc.cmp_width() == 1 && vc.cmp_sign());
  NodePtr disj;
  for (unsigned int k = 0; k < variables_->size(); ++k) {
    VariableExpr& var = *variables_->at(k);
    variables_->at(k) = new VariableExpr(var.id(), vc.elem_bitsize(), vc.elem_sign());
    NodePtr elem = variables_->at(k);
    if (!bool_cmp) {
      if (bool_vec) {
        elem = new IfThenElse(elem, new Constant(1, vc.cmp_width(), vc.cmp_sign()),
                              new Constant(0, vc.cmp_width(), vc.cmp_sign()));
      } else {
        if (!vc.cmp_sign() && vc.elem_sign() && vc.cmp_width() > vc.elem_bitsize()) {
          elem = new Bitslice(elem, vc.elem_bitsize() - 1, 0);
        }
        if (vc.cmp_width() > vc.elem_bitsize()) {
          elem = new ExtendExpression(elem, vc.cmp_width() - vc.elem_bitsize());
        }
      }
    }
    NodePtr eq(new EqualOpr(child, elem));
    disj = disj ? NodePtr(new LogicalOrOpr(disj, eq)) : eq;
  }
  aux_stack_.push(disj);
  subscript_stack_.push(makeInvalidSubscript(c_sub.has_placeholder, c_sub.used_target_placeholder));
  bounds_stack_.push(bg);
  updateResult();
}

void ReplaceVisitor::visitOneHotExpr(OneHotExpr const& o) {
  ScopedSkip guard(allow_unguarded_oob_skip_, false);
  NodePtr child;
  evalUnaryExpr(o, child);
  NodePtr bg = popBounds();
  SubscriptVal s = subscript_stack_.top();
  subscript_stack_.pop();
  aux_stack_.push(new OneHotExpr(child));
  subscript_stack_.push(makeInvalidSubscript(s.has_placeholder, s.used_target_placeholder));
  bounds_stack_.push(bg);
  updateResult();
}

void ReplaceVisitor::visitOneHot0Expr(OneHot0Expr const& o) {
  ScopedSkip guard(allow_unguarded_oob_skip_, false);
  NodePtr child;
  evalUnaryExpr(o, child);
  NodePtr bg = popBounds();
  SubscriptVal s = subscript_stack_.top();
  subscript_stack_.pop();
  aux_stack_.push(new OneHot0Expr(child));
  subscript_stack_.push(makeInvalidSubscript(s.has_placeholder, s.used_target_placeholder));
  bounds_stack_.push(bg);
  updateResult();
}

void ReplaceVisitor::visitCountOnesExpr(CountOnesExpr const& o) {
  ScopedSkip guard(allow_unguarded_oob_skip_, containsBoolExpr(*o.child()) ? false : allow_unguarded_oob_skip_);
  NodePtr child;
  evalUnaryExpr(o, child);
  NodePtr bg = popBounds();
  SubscriptVal s = subscript_stack_.top();
  subscript_stack_.pop();
  aux_stack_.push(new CountOnesExpr(child));
  subscript_stack_.push(makeInvalidSubscript(s.has_placeholder, s.used_target_placeholder));
  bounds_stack_.push(bg);
  updateResult();
}

void ReplaceVisitor::visitClog2Expr(Clog2Expr const& o) {
  ScopedSkip guard(allow_unguarded_oob_skip_, containsBoolExpr(*o.child()) ? false : allow_unguarded_oob_skip_);
  NodePtr child;
  evalUnaryExpr(o, child);
  NodePtr bg = popBounds();
  SubscriptVal s = subscript_stack_.top();
  subscript_stack_.pop();
  aux_stack_.push(new Clog2Expr(child));
  subscript_stack_.push(makeInvalidSubscript(s.has_placeholder, s.used_target_placeholder));
  bounds_stack_.push(bg);
  updateResult();
}

void ReplaceVisitor::visitSelfDeterminedExpr(SelfDeterminedExpr const& s) {
  ScopedSkip guard(allow_unguarded_oob_skip_, false);
  NodePtr child;
  evalUnaryExpr(s, child);
  NodePtr bg = popBounds();
  aux_stack_.push(new SelfDeterminedExpr(child));
  bounds_stack_.push(bg);
  updateResult();
}

void ReplaceVisitor::visitBoundsCheckExpr(BoundsCheckExpr const& b) {
  ScopedSkip guard(allow_unguarded_oob_skip_, false);
  b.lhs()->visit(this);
  assert(aux_stack_.size() >= 1 && subscript_stack_.size() >= 1 && bounds_stack_.size() >= 1);
  NodePtr expr_node = aux_stack_.top();
  aux_stack_.pop();
  SubscriptVal expr_sub = subscript_stack_.top();
  subscript_stack_.pop();
  NodePtr expr_bg = bounds_stack_.top();
  bounds_stack_.pop();

  b.rhs()->visit(this);
  assert(aux_stack_.size() >= 1 && subscript_stack_.size() >= 1 && bounds_stack_.size() >= 1);
  NodePtr guard_node = aux_stack_.top();
  aux_stack_.pop();
  SubscriptVal guard_sub = subscript_stack_.top();
  subscript_stack_.pop();
  NodePtr guard_bg = bounds_stack_.top();
  bounds_stack_.pop();

  expr_sub.has_placeholder = expr_sub.has_placeholder || guard_sub.has_placeholder;
  expr_sub.used_target_placeholder = expr_sub.used_target_placeholder || guard_sub.used_target_placeholder;

  aux_stack_.push(expr_node);
  subscript_stack_.push(expr_sub);
  bounds_stack_.push(bgAnd(expr_bg, bgAnd(guard_node, guard_bg)));
  updateResult();
}

void ReplaceVisitor::visitVectorReduceWith(VectorReduceWith const& vrw) {
  if (!variables_) {
    throw std::runtime_error("crv_vector operation cannot be expanded inside crv_array foreach/reduction.");
  }
  bool const pred_is_bool = is_boolean_ir_node(*vrw.child());
  NodePtr result;
  NodePtr all_bounds;
  int const sub_ph = (vrw.placeholder_id() >= 0) ? vrw.placeholder_id() : -1;
  for (unsigned int i = 0; i < variables_->size(); ++i) {
    ReplaceVisitor sub_replacer(variables_, multi_vec_vars_, sub_ph);
    sub_replacer.setVecIdx(i);
    vrw.child()->visit(&sub_replacer);
    all_bounds = bgAnd(all_bounds, sub_replacer.resultBoundsGuard());
    if (!sub_replacer.okay()) continue;
    NodePtr elem = sub_replacer.result();
    if (vrw.kind() == VectorReduction::Sum && pred_is_bool) {
      elem = new IfThenElse(elem, new Constant(1, 32, false), new Constant(0, 32, false));
      result = result ? NodePtr(new PlusOpr(result, elem)) : elem;
    } else if (pred_is_bool) {
      if (!result) {
        result = elem;
      } else {
        switch (vrw.kind()) {
          case VectorReduction::Product:
          case VectorReduction::And:
            result = new LogicalAndOpr(result, elem);
            break;
          case VectorReduction::Or:
            result = new LogicalOrOpr(result, elem);
            break;
          default:
            result = new NotEqualOpr(result, elem);
            break;
        }
      }
    } else {
      if (!result) {
        result = elem;
      } else {
        switch (vrw.kind()) {
          case VectorReduction::Sum:
            result = new PlusOpr(result, elem);
            break;
          case VectorReduction::Product:
            result = new MultipliesOpr(result, elem);
            break;
          case VectorReduction::And:
            result = new AndOpr(result, elem);
            break;
          case VectorReduction::Or:
            result = new OrOpr(result, elem);
            break;
          default:
            result = new XorOpr(result, elem);
            break;
        }
      }
    }
  }
  if (!result) {
    if (vrw.kind() == VectorReduction::Sum && pred_is_bool) {
      result = new Constant(0, 32, false);
    } else if (pred_is_bool) {
      result = new Constant(vrw.kind() == VectorReduction::Product || vrw.kind() == VectorReduction::And);
    } else {
      bool const sv_rules = sv_signedness();
      unsigned const w = expr_width_ir_node(*vrw.child(), sv_rules);
      bool const s = expr_signed_ir_node(*vrw.child(), sv_rules);
      uint64_t const ones = s || w >= 64 ? ~uint64_t(0) : (uint64_t(1) << w) - 1;
      switch (vrw.kind()) {
        case VectorReduction::Product:
          result = new Constant(1, w, s);
          break;
        case VectorReduction::And:
          result = new Constant(ones, w, s);
          break;
        default:
          result = new Constant(0, w, s);
          break;
      }
    }
  }
  if (vrw.placeholder_id() >= 0 && target_placeholder_id_ != -2 &&
      vrw.placeholder_id() != target_placeholder_id_) {
    ScopedSkip guard(allow_unguarded_oob_skip_, false);
    result->visit(this);
    if (all_bounds) {
      bool const saved_okay = okay_;
      all_bounds->visit(this);
      NodePtr ab_node = aux_stack_.top();
      aux_stack_.pop();
      subscript_stack_.pop();
      NodePtr ab_bg = bounds_stack_.top();
      bounds_stack_.pop();
      okay_ = saved_okay && okay_;
      NodePtr cur_bg = bounds_stack_.top();
      bounds_stack_.pop();
      bounds_stack_.push(bgAnd(cur_bg, bgAnd(ab_node, ab_bg)));
      updateResult();
    }
    return;
  }
  aux_stack_.push(result);
  subscript_stack_.push(makeInvalidSubscript());
  bounds_stack_.push(all_bounds);
  updateResult();
}

void ReplaceVisitor::updateResult() {
  result_ = aux_stack_.top();
  result_bounds_ = bounds_stack_.top();
}

void ReplaceVisitor::reset() {
  terminals_.clear();
  while (!aux_stack_.empty()) aux_stack_.pop();
  while (!subscript_stack_.empty()) subscript_stack_.pop();
  while (!bounds_stack_.empty()) bounds_stack_.pop();
  result_.reset();
  result_bounds_.reset();
  okay_ = true;
}

}  // end namespace crave

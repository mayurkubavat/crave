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

#include <boost/intrusive_ptr.hpp>

#include <algorithm>
#include <ostream>
#include <set>

#include <stdint.h>

#include "visitor/NodeVisitor.hpp"

namespace crave {

class Node;

typedef boost::intrusive_ptr<Node> NodePtr;

class Node {
 protected:
  Node() : count_(0) {}
  virtual ~Node() {}
  Node(Node const&) : count_(0) {}

 public:
  virtual void visit(NodeVisitor* v) const { v->visitNode(*this); }
  std::ostream& printDot(std::ostream& out) const;

  // reference counting
  friend inline void intrusive_ptr_add_ref(Node* n) { ++(n->count_); }
  friend inline void intrusive_ptr_release(Node* n) {
    if (--(n->count_) == 0) delete n;
  }

 private:
  unsigned int count_;
};

class Placeholder : public Node {
 public:
  explicit Placeholder(unsigned int id) : Node(), id_(id) {}
  Placeholder(Placeholder const& p) : Node(p), id_(p.id()) {}

  void visit(NodeVisitor* v) const { v->visitPlaceholder(*this); }

  unsigned int id() const { return id_; }

 private:
  unsigned int id_;
};

class Terminal : public Node {
 protected:
  Terminal(unsigned int bs, bool s) : Node(), bitsize_(bs), sign_(s) {}
  Terminal(Terminal const& t) : Node(t), bitsize_(t.bitsize()), sign_(t.sign()) {}

 public:
  virtual void visit(NodeVisitor* v) const { v->visitTerminal(*this); }

  unsigned int bitsize() const { return bitsize_; }
  bool sign() const { return sign_; }
  bool isBool() const { return bitsize() == 1 && sign(); }

 private:
  unsigned int bitsize_;
  bool sign_;
};

class VariableExpr : public Terminal {
 public:
  VariableExpr(unsigned int id, unsigned int bs, bool s) : Terminal(bs, s), id_(id) {}
  VariableExpr(VariableExpr const& v) : Terminal(v.bitsize(), v.sign()), id_(v.id()) {}

  void visit(NodeVisitor* v) const { v->visitVariableExpr(*this); }

  unsigned int id() const { return id_; }

 private:
  unsigned int id_;
};

class Constant : public Terminal {
 public:
  Constant() : Terminal(1, true), value_(false) {}
  Constant(uint64_t val, unsigned int bs, bool s) : Terminal(bs, s), value_(val) {}
  explicit Constant(bool b) : Terminal(1, true), value_(b) {}
  Constant(Constant const& c) : Terminal(c.bitsize(), c.sign()), value_(c.value()) {}

  void visit(NodeVisitor* v) const { v->visitConstant(*this); }

  operator uint64_t() const { return value_; }

  uint64_t value() const { return value_; }

 private:
  uint64_t value_;
};

class VectorExpr : public Terminal {
 public:
  VectorExpr(unsigned int id, unsigned int bs, bool s) : Terminal(bs, s), id_(id) {}
  VectorExpr(VectorExpr const& v) : Terminal(v.bitsize(), v.sign()), id_(v.id()) {}

  void visit(NodeVisitor* v) const { v->visitVectorExpr(*this); }

  unsigned int id() const { return id_; }

 private:
  unsigned int id_;
};

/**
 * The reduction of the elements of a vector by an operator, like the reduction methods of the arrays of
 * SystemVerilog (IEEE 1800-2017 7.12.3). It has the width and the signedness of the elements. The vector solver
 * replaces it by the reduction of the elements, whose number is given by the size of the vector.
 */
class VectorReduction : public Terminal {
 public:
  enum Kind { Sum, Product, And, Or, Xor };

  VectorReduction(unsigned int id, unsigned int bs, bool s, Kind k)
      : Terminal(bs, s), elem_bitsize_(bs), elem_sign_(s), id_(id), kind_(k) {}
  VectorReduction(unsigned int id, unsigned int res_bs, bool res_s, Kind k, unsigned int elem_bs, bool elem_s)
      : Terminal(res_bs, res_s), elem_bitsize_(elem_bs), elem_sign_(elem_s), id_(id), kind_(k) {}
  VectorReduction(VectorReduction const& r)
      : Terminal(r.bitsize(), r.sign()),
        elem_bitsize_(r.elemBitsize()),
        elem_sign_(r.elemSign()),
        id_(r.id()),
        kind_(r.kind()) {}

  void visit(NodeVisitor* v) const { v->visitVectorReduction(*this); }

  /**
   * The id of the vector.
   */
  unsigned int id() const { return id_; }

  Kind kind() const { return kind_; }

  unsigned int elemBitsize() const { return elem_bitsize_; }
  bool elemSign() const { return elem_sign_; }
  bool elemIsBool() const { return elem_bitsize_ == 1 && elem_sign_; }

 private:
  unsigned int elem_bitsize_;
  bool elem_sign_;
  unsigned int id_;
  Kind kind_;
};

inline void NodeVisitor::visitVectorReduction(VectorReduction const& r) { visitTerminal(r); }

class UnaryExpression : public Node {
 protected:
  explicit UnaryExpression(NodePtr c) : Node(), child_(c) {}
  UnaryExpression(UnaryExpression const& u) : Node(), child_() { child_.operator=(u.child().get()); }

 public:
  virtual void visit(NodeVisitor* v) const { v->visitUnaryExpr(*this); }

  NodePtr child() const { return child_; }

 private:
  NodePtr child_;
};

class UnaryOperator : public UnaryExpression {
 protected:
  explicit UnaryOperator(NodePtr c) : UnaryExpression(c) {}
  UnaryOperator(UnaryOperator const& u) : UnaryExpression(u) {}

 public:
  virtual void visit(NodeVisitor* v) const { v->visitUnaryOpr(*this); }
};

class NotOpr : public UnaryOperator {
 public:
  explicit NotOpr(NodePtr c) : UnaryOperator(c) {}
  NotOpr(NotOpr const& n) : UnaryOperator(n) {}

  void visit(NodeVisitor* v) const { v->visitNotOpr(*this); }
};

class NegOpr : public UnaryOperator {
 public:
  explicit NegOpr(NodePtr c) : UnaryOperator(c) {}
  NegOpr(NegOpr const& n) : UnaryOperator(n) {}

  void visit(NodeVisitor* v) const { v->visitNegOpr(*this); }
};

class ComplementOpr : public UnaryOperator {
 public:
  explicit ComplementOpr(NodePtr c) : UnaryOperator(c) {}
  ComplementOpr(ComplementOpr const& n) : UnaryOperator(n) {}

  void visit(NodeVisitor* v) const { v->visitComplementOpr(*this); }
};

class Inside : public UnaryExpression {
 public:
  Inside(NodePtr v, std::set<Constant> const& c) : UnaryExpression(v), collection_(c) {}
  Inside(Inside const& i) : UnaryExpression(i), collection_(i.collection()) {}

  void visit(NodeVisitor* v) const { v->visitInside(*this); }

  std::set<Constant> const& collection() const { return collection_; }

 private:
  std::set<Constant> collection_;
};

class ExtendExpression : public UnaryExpression {
 public:
  ExtendExpression(NodePtr v, unsigned int i) : UnaryExpression(v), value_(i) {}
  ExtendExpression(ExtendExpression const& e) : UnaryExpression(e), value_(e.value()) {}

  void visit(NodeVisitor* v) const { v->visitExtendExpr(*this); }

  unsigned int const& value() const { return value_; }

 private:
  unsigned int value_;
};

class BinaryExpression : public Node {
 protected:
  BinaryExpression(NodePtr lhs, NodePtr rhs) : lhs_(lhs), rhs_(rhs) {}
  BinaryExpression(BinaryExpression const& b) : Node(), lhs_(), rhs_() {
    lhs_.operator=(b.lhs().get());
    rhs_.operator=(b.rhs().get());
  }

 public:
  virtual void visit(NodeVisitor* v) const { v->visitBinaryExpr(*this); }

  NodePtr lhs() const { return lhs_; }
  NodePtr rhs() const { return rhs_; }

 private:
  NodePtr lhs_;
  NodePtr rhs_;
};

class BinaryOperator : public BinaryExpression {
 protected:
  BinaryOperator(NodePtr lhs, NodePtr rhs) : BinaryExpression(lhs, rhs) {}
  BinaryOperator(BinaryOperator const& b) : BinaryExpression(b) {}

 public:
  virtual void visit(NodeVisitor* v) const { v->visitBinaryOpr(*this); }
};

class AndOpr : public BinaryOperator {
 public:
  AndOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  AndOpr(AndOpr const& a) : BinaryOperator(a) {}

  void visit(NodeVisitor* v) const { v->visitAndOpr(*this); }
};

class OrOpr : public BinaryOperator {
 public:
  OrOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  OrOpr(OrOpr const& o) : BinaryOperator(o) {}

  void visit(NodeVisitor* v) const { v->visitOrOpr(*this); }
};

class LogicalAndOpr : public BinaryOperator {
 public:
  LogicalAndOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  LogicalAndOpr(LogicalAndOpr const& a) : BinaryOperator(a) {}

  void visit(NodeVisitor* v) const { v->visitLogicalAndOpr(*this); }
};

class LogicalOrOpr : public BinaryOperator {
 public:
  LogicalOrOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  LogicalOrOpr(LogicalOrOpr const& o) : BinaryOperator(o) {}

  void visit(NodeVisitor* v) const { v->visitLogicalOrOpr(*this); }
};

class LogicalIffOpr : public BinaryOperator {
 public:
  LogicalIffOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  LogicalIffOpr(LogicalIffOpr const& o) : BinaryOperator(o) {}

  void visit(NodeVisitor* v) const { v->visitLogicalIffOpr(*this); }
};

class XorOpr : public BinaryOperator {
 public:
  XorOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  XorOpr(XorOpr const& x) : BinaryOperator(x) {}

  void visit(NodeVisitor* v) const { v->visitXorOpr(*this); }
};

class EqualOpr : public BinaryOperator {
 public:
  EqualOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  EqualOpr(EqualOpr const& e) : BinaryOperator(e) {}

  void visit(NodeVisitor* v) const { v->visitEqualOpr(*this); }
};

class NotEqualOpr : public BinaryOperator {
 public:
  NotEqualOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  NotEqualOpr(NotEqualOpr const& n) : BinaryOperator(n) {}

  void visit(NodeVisitor* v) const { v->visitNotEqualOpr(*this); }
};

class LessOpr : public BinaryOperator {
 public:
  LessOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  LessOpr(LessOpr const& l) : BinaryOperator(l) {}

  void visit(NodeVisitor* v) const { v->visitLessOpr(*this); }
};

class LessEqualOpr : public BinaryOperator {
 public:
  LessEqualOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  LessEqualOpr(LessEqualOpr const& l) : BinaryOperator(l) {}

  void visit(NodeVisitor* v) const { v->visitLessEqualOpr(*this); }
};

class GreaterOpr : public BinaryOperator {
 public:
  GreaterOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  GreaterOpr(GreaterOpr const& g) : BinaryOperator(g) {}

  void visit(NodeVisitor* v) const { v->visitGreaterOpr(*this); }
};

class GreaterEqualOpr : public BinaryOperator {
 public:
  GreaterEqualOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  GreaterEqualOpr(GreaterEqualOpr const& g) : BinaryOperator(g) {}

  void visit(NodeVisitor* v) const { v->visitGreaterEqualOpr(*this); }
};

class PlusOpr : public BinaryOperator {
 public:
  PlusOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  PlusOpr(PlusOpr const& p) : BinaryOperator(p) {}

  void visit(NodeVisitor* v) const { v->visitPlusOpr(*this); }
};

class MinusOpr : public BinaryOperator {
 public:
  MinusOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  MinusOpr(MinusOpr const& m) : BinaryOperator(m) {}

  void visit(NodeVisitor* v) const { v->visitMinusOpr(*this); }
};

class MultipliesOpr : public BinaryOperator {
 public:
  MultipliesOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  MultipliesOpr(MultipliesOpr const& m) : BinaryOperator(m) {}

  void visit(NodeVisitor* v) const { v->visitMultipliesOpr(*this); }
};

class DevideOpr : public BinaryOperator {
 public:
  DevideOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  DevideOpr(DevideOpr const& d) : BinaryOperator(d) {}

  void visit(NodeVisitor* v) const { v->visitDevideOpr(*this); }
};

class ModuloOpr : public BinaryOperator {
 public:
  ModuloOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  ModuloOpr(ModuloOpr const& m) : BinaryOperator(m) {}

  void visit(NodeVisitor* v) const { v->visitModuloOpr(*this); }
};

class ShiftLeftOpr : public BinaryOperator {
 public:
  ShiftLeftOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  ShiftLeftOpr(ShiftLeftOpr const& s) : BinaryOperator(s) {}

  void visit(NodeVisitor* v) const { v->visitShiftLeftOpr(*this); }
};

class ShiftRightOpr : public BinaryOperator {
 public:
  ShiftRightOpr(NodePtr lhs, NodePtr rhs) : BinaryOperator(lhs, rhs) {}
  ShiftRightOpr(ShiftRightOpr const& s) : BinaryOperator(s) {}

  void visit(NodeVisitor* v) const { v->visitShiftRightOpr(*this); }
};

class VectorAccess : public BinaryExpression {
 public:
  VectorAccess(NodePtr lhs, NodePtr rhs) : BinaryExpression(lhs, rhs) {}
  VectorAccess(VectorAccess const& v) : BinaryExpression(v) {}

  void visit(NodeVisitor* v) const { v->visitVectorAccess(*this); }
};

class TernaryExpression : public Node {
 protected:
  explicit TernaryExpression(NodePtr a, NodePtr b, NodePtr c) : Node(), a_(a), b_(b), c_(c) {}
  TernaryExpression(TernaryExpression const& t) : Node(t), a_(), b_(), c_() {
    a_.operator=(t.a().get());
    b_.operator=(t.b().get());
    c_.operator=(t.c().get());
  }

 public:
  virtual void visit(NodeVisitor* v) const { v->visitTernaryExpr(*this); }

  NodePtr a() const { return a_; }
  NodePtr b() const { return b_; }
  NodePtr c() const { return c_; }

 private:
  NodePtr a_;
  NodePtr b_;
  NodePtr c_;
};

class IfThenElse : public TernaryExpression {
 public:
  IfThenElse(NodePtr a, NodePtr b, NodePtr c) : TernaryExpression(a, b, c) {}
  IfThenElse(IfThenElse const& t) : TernaryExpression(t) {}

  void visit(NodeVisitor* v) const { v->visitIfThenElse(*this); }
};

class ForEach : public BinaryOperator {
 public:
  ForEach(NodePtr a, NodePtr b) : BinaryOperator(a, b) {}
  ForEach(ForEach const& fe) : BinaryOperator(fe) {}

  void visit(NodeVisitor* v) const { v->visitForEach(*this); }
};

class Unique : public UnaryOperator {
 public:
  explicit Unique(NodePtr a) : UnaryOperator(a) {}
  Unique(Unique const& u) : UnaryOperator(u) {}

  void visit(NodeVisitor* v) const { v->visitUnique(*this); }
};

class Bitslice : public UnaryExpression {
 public:
  Bitslice(NodePtr v, int r, int l) : UnaryExpression(v), r_(r), l_(l) {}

  Bitslice(Bitslice const& b) : UnaryExpression(b), r_(b.r_), l_(b.l_) {}

  void visit(NodeVisitor* v) const { v->visitBitslice(*this); }

  int r() const { return r_; }
  int l() const { return l_; }

 private:
  int r_, l_;
};

class RangeExpr : public TernaryExpression {
 public:
  RangeExpr(NodePtr expr, NodePtr lo, NodePtr hi) : TernaryExpression(expr, lo, hi) {}
  RangeExpr(RangeExpr const& r) : TernaryExpression(r) {}

  void visit(NodeVisitor* v) const { v->visitRangeExpr(*this); }
};

class TolPlusMinusExpr : public TernaryExpression {
 public:
  TolPlusMinusExpr(NodePtr expr, NodePtr val, NodePtr tol) : TernaryExpression(expr, val, tol) {}
  TolPlusMinusExpr(TolPlusMinusExpr const& t) : TernaryExpression(t) {}

  void visit(NodeVisitor* v) const { v->visitTolPlusMinusExpr(*this); }
};

class TolPlusPctExpr : public TernaryExpression {
 public:
  TolPlusPctExpr(NodePtr expr, NodePtr val, NodePtr pct) : TernaryExpression(expr, val, pct) {}
  TolPlusPctExpr(TolPlusPctExpr const& t) : TernaryExpression(t) {}

  void visit(NodeVisitor* v) const { v->visitTolPlusPctExpr(*this); }
};

class VectorContains : public UnaryExpression {
 public:
  VectorContains(NodePtr child, unsigned int vec_id, unsigned int elem_bs, bool elem_sign,
                 unsigned int cmp_width = 0, bool cmp_sign = false)
      : UnaryExpression(child),
        vec_id_(vec_id),
        elem_bitsize_(elem_bs),
        elem_sign_(elem_sign),
        cmp_width_(cmp_width),
        cmp_sign_(cmp_sign) {}
  VectorContains(VectorContains const& vc)
      : UnaryExpression(vc),
        vec_id_(vc.vec_id_),
        elem_bitsize_(vc.elem_bitsize_),
        elem_sign_(vc.elem_sign_),
        cmp_width_(vc.cmp_width_),
        cmp_sign_(vc.cmp_sign_) {}

  void visit(NodeVisitor* v) const { v->visitVectorContains(*this); }

  unsigned int vec_id() const { return vec_id_; }
  unsigned int elem_bitsize() const { return elem_bitsize_; }
  bool elem_sign() const { return elem_sign_; }
  unsigned int cmp_width() const { return cmp_width_; }
  bool cmp_sign() const { return cmp_sign_; }

 private:
  unsigned int vec_id_;
  unsigned int elem_bitsize_;
  bool elem_sign_;
  unsigned int cmp_width_;
  bool cmp_sign_;
};

inline void NodeVisitor::visitRangeExpr(RangeExpr const& r) { visitTernaryExpr(r); }
inline void NodeVisitor::visitTolPlusMinusExpr(TolPlusMinusExpr const& t) { visitTernaryExpr(t); }
inline void NodeVisitor::visitTolPlusPctExpr(TolPlusPctExpr const& t) { visitTernaryExpr(t); }
inline void NodeVisitor::visitVectorContains(VectorContains const& vc) { visitUnaryExpr(vc); }

class OneHotExpr : public UnaryExpression {
 public:
  explicit OneHotExpr(NodePtr c) : UnaryExpression(c) {}
  OneHotExpr(OneHotExpr const& o) : UnaryExpression(o) {}

  void visit(NodeVisitor* v) const { v->visitOneHotExpr(*this); }
};

class OneHot0Expr : public UnaryExpression {
 public:
  explicit OneHot0Expr(NodePtr c) : UnaryExpression(c) {}
  OneHot0Expr(OneHot0Expr const& o) : UnaryExpression(o) {}

  void visit(NodeVisitor* v) const { v->visitOneHot0Expr(*this); }
};

class CountOnesExpr : public UnaryExpression {
 public:
  explicit CountOnesExpr(NodePtr c) : UnaryExpression(c) {}
  CountOnesExpr(CountOnesExpr const& o) : UnaryExpression(o) {}

  void visit(NodeVisitor* v) const { v->visitCountOnesExpr(*this); }
};

class Clog2Expr : public UnaryExpression {
 public:
  explicit Clog2Expr(NodePtr c) : UnaryExpression(c) {}
  Clog2Expr(Clog2Expr const& o) : UnaryExpression(o) {}

  void visit(NodeVisitor* v) const { v->visitClog2Expr(*this); }
};

class SelfDeterminedExpr : public UnaryExpression {
 public:
  explicit SelfDeterminedExpr(NodePtr c) : UnaryExpression(c) {}
  SelfDeterminedExpr(SelfDeterminedExpr const& o) : UnaryExpression(o) {}

  void visit(NodeVisitor* v) const { v->visitSelfDeterminedExpr(*this); }
};

class BoundsCheckExpr : public BinaryExpression {
 public:
  BoundsCheckExpr(NodePtr expr, NodePtr guard) : BinaryExpression(expr, guard) {}
  BoundsCheckExpr(BoundsCheckExpr const& o) : BinaryExpression(o) {}

  void visit(NodeVisitor* v) const { v->visitBoundsCheckExpr(*this); }
};

class VectorReduceWith : public UnaryExpression {
 public:
  VectorReduceWith(unsigned int vec_id, unsigned int elem_bs, bool elem_s, VectorReduction::Kind k, NodePtr pred,
                   int placeholder_id = -1)
      : UnaryExpression(pred),
        vec_id_(vec_id),
        elem_bitsize_(elem_bs),
        elem_sign_(elem_s),
        kind_(k),
        placeholder_id_(placeholder_id) {}
  VectorReduceWith(VectorReduceWith const& r)
      : UnaryExpression(r),
        vec_id_(r.vec_id_),
        elem_bitsize_(r.elem_bitsize_),
        elem_sign_(r.elem_sign_),
        kind_(r.kind_),
        placeholder_id_(r.placeholder_id_) {}

  void visit(NodeVisitor* v) const { v->visitVectorReduceWith(*this); }

  unsigned int vec_id() const { return vec_id_; }
  unsigned int elemBitsize() const { return elem_bitsize_; }
  bool elemSign() const { return elem_sign_; }
  bool elemIsBool() const { return elem_bitsize_ == 1 && elem_sign_; }
  VectorReduction::Kind kind() const { return kind_; }
  int placeholder_id() const { return placeholder_id_; }

 private:
  unsigned int vec_id_;
  unsigned int elem_bitsize_;
  bool elem_sign_;
  VectorReduction::Kind kind_;
  int placeholder_id_;
};

inline bool is_boolean_ir_node(Node const& n) {
  if (Terminal const* t = dynamic_cast<Terminal const*>(&n)) return t->isBool();
  if (dynamic_cast<NotOpr const*>(&n) || dynamic_cast<LogicalAndOpr const*>(&n) ||
      dynamic_cast<LogicalOrOpr const*>(&n) || dynamic_cast<LogicalIffOpr const*>(&n) ||
      dynamic_cast<EqualOpr const*>(&n) || dynamic_cast<NotEqualOpr const*>(&n) ||
      dynamic_cast<LessOpr const*>(&n) || dynamic_cast<LessEqualOpr const*>(&n) ||
      dynamic_cast<GreaterOpr const*>(&n) || dynamic_cast<GreaterEqualOpr const*>(&n) ||
      dynamic_cast<Inside const*>(&n) || dynamic_cast<RangeExpr const*>(&n) ||
      dynamic_cast<TolPlusMinusExpr const*>(&n) || dynamic_cast<TolPlusPctExpr const*>(&n) ||
      dynamic_cast<VectorContains const*>(&n) || dynamic_cast<ForEach const*>(&n) ||
      dynamic_cast<Unique const*>(&n) || dynamic_cast<OneHotExpr const*>(&n) ||
      dynamic_cast<OneHot0Expr const*>(&n))
    return true;
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&n))
    return is_boolean_ir_node(*ite->b()) && is_boolean_ir_node(*ite->c());
  if (VectorAccess const* va = dynamic_cast<VectorAccess const*>(&n)) return is_boolean_ir_node(*va->lhs());
  if (BoundsCheckExpr const* bce = dynamic_cast<BoundsCheckExpr const*>(&n))
    return is_boolean_ir_node(*bce->lhs());
  if (SelfDeterminedExpr const* sde = dynamic_cast<SelfDeterminedExpr const*>(&n))
    return is_boolean_ir_node(*sde->child());
  if (VectorReduceWith const* vrw = dynamic_cast<VectorReduceWith const*>(&n))
    return vrw->kind() != VectorReduction::Sum && is_boolean_ir_node(*vrw->child());
  return false;
}

inline unsigned ir_node_width(Node const& n) {
  if (Terminal const* t = dynamic_cast<Terminal const*>(&n)) return t->bitsize();
  if (dynamic_cast<Placeholder const*>(&n)) return placeholder_bitsize();
  if (VectorAccess const* va = dynamic_cast<VectorAccess const*>(&n)) return ir_node_width(*va->lhs());
  if (BoundsCheckExpr const* bce = dynamic_cast<BoundsCheckExpr const*>(&n)) return ir_node_width(*bce->lhs());
  if (Bitslice const* b = dynamic_cast<Bitslice const*>(&n)) return static_cast<unsigned>(b->r() - b->l() + 1);
  if (dynamic_cast<CountOnesExpr const*>(&n) || dynamic_cast<Clog2Expr const*>(&n)) return 32u;
  if (SelfDeterminedExpr const* sde = dynamic_cast<SelfDeterminedExpr const*>(&n))
    return ir_node_width(*sde->child());
  if (VectorReduceWith const* vrw = dynamic_cast<VectorReduceWith const*>(&n))
    return (vrw->kind() == VectorReduction::Sum && is_boolean_ir_node(*vrw->child())) ? 32u
                                                                                       : ir_node_width(*vrw->child());
  if (is_boolean_ir_node(n)) return 1u;
  if (ExtendExpression const* e = dynamic_cast<ExtendExpression const*>(&n))
    return ir_node_width(*e->child()) + e->value();
  if (dynamic_cast<NegOpr const*>(&n) || dynamic_cast<ComplementOpr const*>(&n))
    return ir_node_width(*static_cast<UnaryExpression const&>(n).child());
  if (dynamic_cast<ShiftLeftOpr const*>(&n) || dynamic_cast<ShiftRightOpr const*>(&n))
    return ir_node_width(*static_cast<BinaryExpression const&>(n).lhs());
  if (BinaryExpression const* b = dynamic_cast<BinaryExpression const*>(&n))
    return std::max(ir_node_width(*b->lhs()), ir_node_width(*b->rhs()));
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&n))
    return std::max(ir_node_width(*ite->b()), ir_node_width(*ite->c()));
  return 1u;
}

inline bool ir_node_signed(Node const& n) {
  if (dynamic_cast<Bitslice const*>(&n) || dynamic_cast<CountOnesExpr const*>(&n) ||
      dynamic_cast<Clog2Expr const*>(&n) || is_boolean_ir_node(n))
    return false;
  if (Terminal const* t = dynamic_cast<Terminal const*>(&n)) return t->sign();
  if (VectorAccess const* va = dynamic_cast<VectorAccess const*>(&n)) return ir_node_signed(*va->lhs());
  if (BoundsCheckExpr const* bce = dynamic_cast<BoundsCheckExpr const*>(&n)) return ir_node_signed(*bce->lhs());
  if (SelfDeterminedExpr const* sde = dynamic_cast<SelfDeterminedExpr const*>(&n))
    return ir_node_signed(*sde->child());
  if (VectorReduceWith const* vrw = dynamic_cast<VectorReduceWith const*>(&n))
    return (vrw->kind() == VectorReduction::Sum && is_boolean_ir_node(*vrw->child())) ? false
                                                                                       : ir_node_signed(*vrw->child());
  if (dynamic_cast<ExtendExpression const*>(&n) || dynamic_cast<NegOpr const*>(&n) ||
      dynamic_cast<ComplementOpr const*>(&n))
    return ir_node_signed(*static_cast<UnaryExpression const&>(n).child());
  if (dynamic_cast<ShiftLeftOpr const*>(&n) || dynamic_cast<ShiftRightOpr const*>(&n))
    return ir_node_signed(*static_cast<BinaryExpression const&>(n).lhs());
  if (BinaryExpression const* b = dynamic_cast<BinaryExpression const*>(&n))
    return ir_node_signed(*b->lhs()) && ir_node_signed(*b->rhs());
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&n))
    return ir_node_signed(*ite->b()) && ir_node_signed(*ite->c());
  return false;
}

inline bool solver_signed_ir_node(Node const& n) {
  if (Terminal const* t = dynamic_cast<Terminal const*>(&n)) return t->sign();
  if (dynamic_cast<Bitslice const*>(&n) || dynamic_cast<CountOnesExpr const*>(&n) ||
      dynamic_cast<Clog2Expr const*>(&n))
    return false;
  if (VectorAccess const* va = dynamic_cast<VectorAccess const*>(&n)) return solver_signed_ir_node(*va->lhs());
  if (BoundsCheckExpr const* bce = dynamic_cast<BoundsCheckExpr const*>(&n))
    return solver_signed_ir_node(*bce->lhs());
  if (SelfDeterminedExpr const* sde = dynamic_cast<SelfDeterminedExpr const*>(&n))
    return solver_signed_ir_node(*sde->child());
  if (VectorReduceWith const* vrw = dynamic_cast<VectorReduceWith const*>(&n))
    return (vrw->kind() == VectorReduction::Sum && is_boolean_ir_node(*vrw->child()))
               ? false
               : solver_signed_ir_node(*vrw->child());
  if (ExtendExpression const* e = dynamic_cast<ExtendExpression const*>(&n))
    return solver_signed_ir_node(*e->child());
  if (NegOpr const* o = dynamic_cast<NegOpr const*>(&n))
    return is_boolean_ir_node(*o->child()) ? true : solver_signed_ir_node(*o->child());
  if (ComplementOpr const* o = dynamic_cast<ComplementOpr const*>(&n))
    return is_boolean_ir_node(*o->child()) ? true : solver_signed_ir_node(*o->child());
  if (ShiftLeftOpr const* o = dynamic_cast<ShiftLeftOpr const*>(&n))
    return is_boolean_ir_node(*o->lhs()) ? true : solver_signed_ir_node(*o->lhs());
  if (ShiftRightOpr const* o = dynamic_cast<ShiftRightOpr const*>(&n))
    return is_boolean_ir_node(*o->lhs()) ? true : solver_signed_ir_node(*o->lhs());
  if (dynamic_cast<PlusOpr const*>(&n) || dynamic_cast<MinusOpr const*>(&n) ||
      dynamic_cast<MultipliesOpr const*>(&n) || dynamic_cast<DevideOpr const*>(&n) ||
      dynamic_cast<ModuloOpr const*>(&n) || dynamic_cast<AndOpr const*>(&n) ||
      dynamic_cast<OrOpr const*>(&n) || dynamic_cast<XorOpr const*>(&n)) {
    BinaryExpression const& b = static_cast<BinaryExpression const&>(n);
    if (is_boolean_ir_node(*b.lhs()) || is_boolean_ir_node(*b.rhs())) return true;
    return solver_signed_ir_node(*b.lhs()) || solver_signed_ir_node(*b.rhs());
  }
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&n)) {
    if (is_boolean_ir_node(*ite->b()) && is_boolean_ir_node(*ite->c())) return false;
    if (is_boolean_ir_node(*ite->b()) != is_boolean_ir_node(*ite->c())) return true;
    return solver_signed_ir_node(*ite->b()) || solver_signed_ir_node(*ite->c());
  }
  return false;
}

inline unsigned expr_width_ir_node(Node const& n, bool sv_rules) {
  if (Terminal const* t = dynamic_cast<Terminal const*>(&n)) return t->bitsize();
  if (dynamic_cast<Placeholder const*>(&n)) return placeholder_bitsize();
  if (VectorAccess const* va = dynamic_cast<VectorAccess const*>(&n)) return expr_width_ir_node(*va->lhs(), sv_rules);
  if (BoundsCheckExpr const* bce = dynamic_cast<BoundsCheckExpr const*>(&n))
    return expr_width_ir_node(*bce->lhs(), sv_rules);
  if (Bitslice const* b = dynamic_cast<Bitslice const*>(&n)) return static_cast<unsigned>(b->r() - b->l() + 1);
  if (dynamic_cast<CountOnesExpr const*>(&n) || dynamic_cast<Clog2Expr const*>(&n)) return 32u;
  if (SelfDeterminedExpr const* sde = dynamic_cast<SelfDeterminedExpr const*>(&n))
    return expr_width_ir_node(*sde->child(), sv_rules);
  if (VectorReduceWith const* vrw = dynamic_cast<VectorReduceWith const*>(&n))
    return (vrw->kind() == VectorReduction::Sum && is_boolean_ir_node(*vrw->child()))
               ? 32u
               : expr_width_ir_node(*vrw->child(), sv_rules);
  if (is_boolean_ir_node(n)) return 1u;
  if (ExtendExpression const* e = dynamic_cast<ExtendExpression const*>(&n)) {
    unsigned const cw = expr_width_ir_node(*e->child(), sv_rules);
    return sv_rules ? cw : (cw + e->value());
  }
  auto promoted_width = [sv_rules](Node const& child) -> unsigned {
    if (is_boolean_ir_node(child)) return sv_rules ? 1u : static_cast<unsigned>(sizeof(int) * 8);
    return expr_width_ir_node(child, sv_rules);
  };
  if (UnaryExpression const* u = dynamic_cast<UnaryExpression const*>(&n)) {
    if (dynamic_cast<NegOpr const*>(&n) || dynamic_cast<ComplementOpr const*>(&n)) {
      return promoted_width(*u->child());
    }
  }
  if (dynamic_cast<ShiftLeftOpr const*>(&n) || dynamic_cast<ShiftRightOpr const*>(&n)) {
    BinaryExpression const& b = static_cast<BinaryExpression const&>(n);
    return sv_rules ? promoted_width(*b.lhs())
                    : std::max(promoted_width(*b.lhs()), promoted_width(*b.rhs()));
  }
  if (BinaryExpression const* b = dynamic_cast<BinaryExpression const*>(&n)) {
    unsigned const lw = promoted_width(*b->lhs());
    unsigned const rw = promoted_width(*b->rhs());
    unsigned w = std::max(lw, rw);
    if (!sv_rules && dynamic_cast<DevideOpr const*>(&n)) {
      bool const ls = is_boolean_ir_node(*b->lhs()) ? true : solver_signed_ir_node(*b->lhs());
      bool const rs = is_boolean_ir_node(*b->rhs()) ? true : solver_signed_ir_node(*b->rhs());
      if (ls != rs) w += 1u;
    }
    return w;
  }
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&n)) {
    return std::max(promoted_width(*ite->b()), promoted_width(*ite->c()));
  }
  return 1u;
}

inline bool expr_signed_ir_node(Node const& n, bool sv_rules) {
  return sv_rules ? ir_node_signed(n) : solver_signed_ir_node(n);
}

}  // end namespace crave

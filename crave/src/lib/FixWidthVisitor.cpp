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


#include "../crave/ir/visitor/FixWidthVisitor.hpp"

#include <stdexcept>

namespace crave {

void FixWidthVisitor::pop(stack_entry& fst) {
  assert(exprStack_.size() >= 1);
  fst = exprStack_.top();
  exprStack_.pop();
}

void FixWidthVisitor::pop2(stack_entry& fst, stack_entry& snd) {
  assert(exprStack_.size() >= 2);
  fst = exprStack_.top();
  exprStack_.pop();
  snd = exprStack_.top();
  exprStack_.pop();
}

void FixWidthVisitor::pop3(stack_entry& fst, stack_entry& snd, stack_entry& trd) {
  assert(exprStack_.size() >= 3);
  fst = exprStack_.top();
  exprStack_.pop();
  snd = exprStack_.top();
  exprStack_.pop();
  trd = exprStack_.top();
  exprStack_.pop();
}

void FixWidthVisitor::evalBinExpr(BinaryExpression const& bin, stack_entry& fst, stack_entry& snd,
                                  bool fixWidth = true) {
  visitBinaryExpr(bin);
  pop2(snd, fst);
  if (!fixWidth) return;
  if (fst.second < snd.second) {
    unsigned int diff = snd.second - fst.second;
    fst.first = result_type(new ExtendExpression(fst.first.get(), diff));
    fst.second = snd.second;
  } else if (fst.second > snd.second) {
    unsigned int diff = fst.second - snd.second;
    snd.first = result_type(new ExtendExpression(snd.first.get(), diff));
    snd.second = fst.second;
  }
}

void FixWidthVisitor::evalTernExpr(TernaryExpression const& tern, stack_entry& fst, stack_entry& snd,
                                   stack_entry& trd) {
  visitTernaryExpr(tern);
  pop3(trd, snd, fst);
}

void FixWidthVisitor::visitNode(const Node&) {}

void FixWidthVisitor::visitTerminal(const Terminal&) {}

void FixWidthVisitor::visitUnaryExpr(const UnaryExpression& u) { u.child()->visit(this); }

void FixWidthVisitor::visitUnaryOpr(const UnaryOperator&) {}

void FixWidthVisitor::visitBinaryExpr(const BinaryExpression& b) {
  b.lhs()->visit(this);
  b.rhs()->visit(this);
}

void FixWidthVisitor::visitBinaryOpr(const BinaryOperator&) {}

void FixWidthVisitor::visitTernaryExpr(const TernaryExpression& t) {
  t.a()->visit(this);
  t.b()->visit(this);
  t.c()->visit(this);
}

template <typename T>
void FixWidthVisitor::visitNumberResultBinExpr(const T& object) {
  stack_entry lhs, rhs;
  evalBinExpr(object, lhs, rhs);
  exprStack_.push(std::make_pair(new T(lhs.first, rhs.first), lhs.second));
}

template <typename T>
void FixWidthVisitor::visitNumberResultUnaryExpr(const T& object) {
  visitUnaryExpr(object);
  stack_entry e;
  pop(e);
  exprStack_.push(std::make_pair(new T(e.first), e.second));
}

template <typename T>
void FixWidthVisitor::visitBooleanResultBinExpr(const T& object) {
  stack_entry lhs, rhs;
  evalBinExpr(object, lhs, rhs);
  exprStack_.push(std::make_pair(new T(lhs.first, rhs.first), 1));
}

void FixWidthVisitor::visitPlaceholder(const Placeholder& pl) {
  exprStack_.push(std::make_pair(new Placeholder(pl), placeholder_bitsize()));
}

void FixWidthVisitor::visitVariableExpr(const VariableExpr& v) {
  exprStack_.push(std::make_pair(new VariableExpr(v), v.bitsize()));
}

void FixWidthVisitor::visitConstant(const Constant& c) {
  exprStack_.push(std::make_pair(new Constant(c), c.bitsize()));
}

void FixWidthVisitor::visitVectorExpr(const VectorExpr& v) {
  exprStack_.push(std::make_pair(new VectorExpr(v), v.bitsize()));
}

void FixWidthVisitor::visitNotOpr(const NotOpr& n) { visitNumberResultUnaryExpr(n); }

void FixWidthVisitor::visitNegOpr(const NegOpr& n) { visitNumberResultUnaryExpr(n); }

void FixWidthVisitor::visitComplementOpr(const ComplementOpr& c) { visitNumberResultUnaryExpr(c); }

void FixWidthVisitor::visitInside(const Inside& i) {
  visitUnaryExpr(i);

  stack_entry e;
  pop(e);

  exprStack_.push(std::make_pair(new Inside(e.first, i.collection()), 1));
}

void FixWidthVisitor::visitExtendExpr(const ExtendExpression& e) {
  visitUnaryExpr(e);

  stack_entry entry;
  pop(entry);

  exprStack_.push(std::make_pair(new ExtendExpression(entry.first, e.value()), entry.second));
}

void FixWidthVisitor::visitAndOpr(const AndOpr& a) { visitNumberResultBinExpr(a); }

void FixWidthVisitor::visitOrOpr(const OrOpr& o) { visitNumberResultBinExpr(o); }

void FixWidthVisitor::visitLogicalAndOpr(const LogicalAndOpr& la) { visitBooleanResultBinExpr(la); }

void FixWidthVisitor::visitLogicalOrOpr(const LogicalOrOpr& lo) { visitBooleanResultBinExpr(lo); }

void FixWidthVisitor::visitXorOpr(const XorOpr& x) { visitNumberResultBinExpr(x); }

void FixWidthVisitor::visitEqualOpr(const EqualOpr& eq) { visitBooleanResultBinExpr(eq); }

void FixWidthVisitor::visitNotEqualOpr(const NotEqualOpr& neq) { visitBooleanResultBinExpr(neq); }

void FixWidthVisitor::visitLessOpr(const LessOpr& l) { visitBooleanResultBinExpr(l); }

void FixWidthVisitor::visitLessEqualOpr(const LessEqualOpr& le) { visitBooleanResultBinExpr(le); }

void FixWidthVisitor::visitGreaterOpr(const GreaterOpr& g) { visitBooleanResultBinExpr(g); }

void FixWidthVisitor::visitGreaterEqualOpr(const GreaterEqualOpr& ge) { visitBooleanResultBinExpr(ge); }

void FixWidthVisitor::visitPlusOpr(const PlusOpr& p) { visitNumberResultBinExpr(p); }

void FixWidthVisitor::visitMinusOpr(const MinusOpr& m) { visitNumberResultBinExpr(m); }

void FixWidthVisitor::visitMultipliesOpr(const MultipliesOpr& m) { visitNumberResultBinExpr(m); }

namespace {
// whether the solver treats an expression as signed: an operation is signed if one of its operands is
bool solverSigned(Node const& n) {
  if (Terminal const* t = dynamic_cast<Terminal const*>(&n)) return t->sign();
  if (dynamic_cast<Bitslice const*>(&n)) return false;
  // an element of a vector has the type of the vector
  if (VectorAccess const* va = dynamic_cast<VectorAccess const*>(&n)) return solverSigned(*va->lhs());
  if (ExtendExpression const* e = dynamic_cast<ExtendExpression const*>(&n)) return solverSigned(*e->child());
  if (NegOpr const* o = dynamic_cast<NegOpr const*>(&n)) return solverSigned(*o->child());
  if (ComplementOpr const* o = dynamic_cast<ComplementOpr const*>(&n)) return solverSigned(*o->child());
  if (ShiftLeftOpr const* o = dynamic_cast<ShiftLeftOpr const*>(&n)) return solverSigned(*o->lhs());
  if (ShiftRightOpr const* o = dynamic_cast<ShiftRightOpr const*>(&n)) return solverSigned(*o->lhs());
  if (dynamic_cast<PlusOpr const*>(&n) || dynamic_cast<MinusOpr const*>(&n) || dynamic_cast<MultipliesOpr const*>(&n) ||
      dynamic_cast<DevideOpr const*>(&n) || dynamic_cast<ModuloOpr const*>(&n) || dynamic_cast<AndOpr const*>(&n) ||
      dynamic_cast<OrOpr const*>(&n) || dynamic_cast<XorOpr const*>(&n)) {
    BinaryExpression const& b = static_cast<BinaryExpression const&>(n);
    return solverSigned(*b.lhs()) || solverSigned(*b.rhs());
  }
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&n))
    return solverSigned(*ite->b()) || solverSigned(*ite->c());
  return false;
}
}  // namespace

// The quotient of a signed and an unsigned operand is the one of their values, as a comparison of them compares
// their values. It may need one bit more than the operands, e.g. 129u / -1, so both operands are extended by a
// bit, the signed one by its sign, and the quotient has one bit more than the operands.
void FixWidthVisitor::visitDevideOpr(const DevideOpr& d) {
  visitNumberResultBinExpr(d);
  stack_entry e;
  pop(e);
  DevideOpr const& q = static_cast<DevideOpr const&>(*e.first);
  if (solverSigned(*q.lhs()) != solverSigned(*q.rhs())) {
    e.first = new DevideOpr(new ExtendExpression(q.lhs(), 1), new ExtendExpression(q.rhs(), 1));
    e.second += 1;
  }
  exprStack_.push(e);
}

void FixWidthVisitor::visitModuloOpr(const ModuloOpr& m) { visitNumberResultBinExpr(m); }

void FixWidthVisitor::visitShiftLeftOpr(const ShiftLeftOpr& shl) { visitNumberResultBinExpr(shl); }

void FixWidthVisitor::visitShiftRightOpr(const ShiftRightOpr& shr) { visitNumberResultBinExpr(shr); }

void FixWidthVisitor::visitVectorAccess(const VectorAccess& va) {
  stack_entry lhs, rhs;
  evalBinExpr(va, lhs, rhs, false);
  exprStack_.push(std::make_pair(new VectorAccess(lhs.first, rhs.first), lhs.second));
}

void FixWidthVisitor::visitForEach(const ForEach& fe) {
  stack_entry lhs, rhs;
  evalBinExpr(fe, lhs, rhs, false);
  exprStack_.push(std::make_pair(new ForEach(lhs.first, rhs.first), 1));
}

void FixWidthVisitor::visitUnique(const Unique& u) { visitNumberResultUnaryExpr(u); }

void FixWidthVisitor::visitIfThenElse(const IfThenElse& ite) {
  stack_entry a, b, c;
  evalTernExpr(ite, a, b, c);

  exprStack_.push(std::make_pair(new IfThenElse(a.first, b.first, c.first), a.second));
}

void FixWidthVisitor::visitBitslice(const Bitslice& b) {
  visitUnaryExpr(b);

  stack_entry e;
  pop(e);

  exprStack_.push(std::make_pair(new Bitslice(e.first, b.r(), b.l()), b.r() - b.l() + 1));
}

}  // end namespace crave

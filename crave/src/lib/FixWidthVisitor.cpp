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
#include "../crave/frontend/bitsize_traits.hpp"

#include <stdexcept>

namespace crave {

namespace {
bool isBoolean(Node const& n) {
  if (Terminal const* t = dynamic_cast<Terminal const*>(&n)) return t->isBool();
  if (dynamic_cast<NotOpr const*>(&n) || dynamic_cast<LogicalAndOpr const*>(&n) ||
      dynamic_cast<LogicalOrOpr const*>(&n) || dynamic_cast<EqualOpr const*>(&n) ||
      dynamic_cast<NotEqualOpr const*>(&n) || dynamic_cast<LessOpr const*>(&n) ||
      dynamic_cast<LessEqualOpr const*>(&n) || dynamic_cast<GreaterOpr const*>(&n) ||
      dynamic_cast<GreaterEqualOpr const*>(&n) || dynamic_cast<Inside const*>(&n) ||
      dynamic_cast<ForEach const*>(&n) || dynamic_cast<Unique const*>(&n))
    return true;
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&n)) return isBoolean(*ite->b());
  if (VectorAccess const* va = dynamic_cast<VectorAccess const*>(&n)) return isBoolean(*va->lhs());
  return false;
}
}  // namespace

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
  // As in C++, a boolean operand of an operation with an integer operand is promoted to an integer.
  if (isBoolean(*fst.first) != isBoolean(*snd.first)) {
    promoteToInteger(fst);
    promoteToInteger(snd);
  }
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
  // As in C++, the boolean operands of an arithmetic or bitwise operator are promoted to integers.
  promoteToInteger(lhs);
  promoteToInteger(rhs);
  exprStack_.push(std::make_pair(new T(lhs.first, rhs.first), lhs.second));
}

template <typename T>
void FixWidthVisitor::visitNumberResultUnaryExpr(const T& object) {
  visitUnaryExpr(object);
  stack_entry e;
  pop(e);
  exprStack_.push(std::make_pair(new T(e.first), e.second));
}

// As in C++, a boolean operand of an arithmetic or bitwise unary operator is promoted to an integer.
template <typename T>
void FixWidthVisitor::visitPromotedUnaryExpr(const T& object) {
  visitUnaryExpr(object);
  stack_entry e;
  pop(e);
  promoteToInteger(e);
  exprStack_.push(std::make_pair(new T(e.first), e.second));
}

template <typename T>
void FixWidthVisitor::visitBooleanResultBinExpr(const T& object) {
  stack_entry lhs, rhs;
  evalBinExpr(object, lhs, rhs);
  exprStack_.push(std::make_pair(new T(lhs.first, rhs.first), 1));
}

// As in C++, the boolean operands of a relational operator are promoted to integers, e.g. false < true holds.
template <typename T>
void FixWidthVisitor::visitRelationalBinExpr(const T& object) {
  stack_entry lhs, rhs;
  evalBinExpr(object, lhs, rhs);
  promoteToInteger(lhs);
  promoteToInteger(rhs);
  exprStack_.push(std::make_pair(new T(lhs.first, rhs.first), 1));
}

// As in C++, an integer operand of a logical operator stands for its comparison with zero.
FixWidthVisitor::result_type FixWidthVisitor::toBoolean(stack_entry const& e) {
  if (isBoolean(*e.first)) return e.first;
  return new NotEqualOpr(e.first, new Constant(0, e.second, false));
}

// As in C++, a boolean promotes to the int 1 for true and 0 for false.
void FixWidthVisitor::promoteToInteger(stack_entry& e) {
  if (!isBoolean(*e.first)) return;
  unsigned const width = bitsize_traits<int>::value;
  bool const sign = crave::is_signed<int>::value;
  e.first = new IfThenElse(e.first, new Constant(1, width, sign), new Constant(0, width, sign));
  e.second = width;
}

template <typename T>
void FixWidthVisitor::visitLogicalBinExpr(const T& object) {
  stack_entry lhs, rhs;
  evalBinExpr(object, lhs, rhs, false);
  exprStack_.push(std::make_pair(new T(toBoolean(lhs), toBoolean(rhs)), 1));
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

void FixWidthVisitor::visitNotOpr(const NotOpr& n) {
  visitUnaryExpr(n);
  stack_entry e;
  pop(e);
  exprStack_.push(std::make_pair(new NotOpr(toBoolean(e)), 1));
}

void FixWidthVisitor::visitNegOpr(const NegOpr& n) { visitPromotedUnaryExpr(n); }

void FixWidthVisitor::visitComplementOpr(const ComplementOpr& c) { visitPromotedUnaryExpr(c); }

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

void FixWidthVisitor::visitLogicalAndOpr(const LogicalAndOpr& la) { visitLogicalBinExpr(la); }

void FixWidthVisitor::visitLogicalOrOpr(const LogicalOrOpr& lo) { visitLogicalBinExpr(lo); }

void FixWidthVisitor::visitXorOpr(const XorOpr& x) { visitNumberResultBinExpr(x); }

void FixWidthVisitor::visitEqualOpr(const EqualOpr& eq) { visitBooleanResultBinExpr(eq); }

void FixWidthVisitor::visitNotEqualOpr(const NotEqualOpr& neq) { visitBooleanResultBinExpr(neq); }

void FixWidthVisitor::visitLessOpr(const LessOpr& l) { visitRelationalBinExpr(l); }

void FixWidthVisitor::visitLessEqualOpr(const LessEqualOpr& le) { visitRelationalBinExpr(le); }

void FixWidthVisitor::visitGreaterOpr(const GreaterOpr& g) { visitRelationalBinExpr(g); }

void FixWidthVisitor::visitGreaterEqualOpr(const GreaterEqualOpr& ge) { visitRelationalBinExpr(ge); }

void FixWidthVisitor::visitPlusOpr(const PlusOpr& p) { visitNumberResultBinExpr(p); }

void FixWidthVisitor::visitMinusOpr(const MinusOpr& m) { visitNumberResultBinExpr(m); }

void FixWidthVisitor::visitMultipliesOpr(const MultipliesOpr& m) { visitNumberResultBinExpr(m); }

void FixWidthVisitor::visitDevideOpr(const DevideOpr& d) { visitNumberResultBinExpr(d); }

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

  // As in C++, an integer condition stands for its comparison with zero.
  result_type cond = toBoolean(a);
  // If one branch is a boolean and the other an integer, the boolean is promoted to an integer.
  if (isBoolean(*b.first) != isBoolean(*c.first)) {
    promoteToInteger(b);
    promoteToInteger(c);
  }
  if (isBoolean(*b.first)) {
    exprStack_.push(std::make_pair(new IfThenElse(cond, b.first, c.first), 1));
    return;
  }
  if (b.second < c.second) {
    b.first = result_type(new ExtendExpression(b.first.get(), c.second - b.second));
    b.second = c.second;
  } else if (b.second > c.second) {
    c.first = result_type(new ExtendExpression(c.first.get(), b.second - c.second));
    c.second = b.second;
  }
  exprStack_.push(std::make_pair(new IfThenElse(cond, b.first, c.first), b.second));
}

void FixWidthVisitor::visitBitslice(const Bitslice& b) {
  visitUnaryExpr(b);

  stack_entry e;
  pop(e);

  exprStack_.push(std::make_pair(new Bitslice(e.first, b.r(), b.l()), b.r() - b.l() + 1));
}

}  // end namespace crave

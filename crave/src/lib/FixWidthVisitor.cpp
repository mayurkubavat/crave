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
#include "../crave/ConstrainedRandom.hpp"

#include <algorithm>
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

template <typename T>
bool is(Node const& n) {
  return dynamic_cast<T const*>(&n) != 0;
}

// IEEE 1800-2017 11.8.1: an expression is unsigned if any of its operands is unsigned, a bit-select or
// part-select is unsigned, and the type of a shift is the type of its left operand.
bool svSigned(Node const& n) {
  if (Terminal const* t = dynamic_cast<Terminal const*>(&n)) return t->sign();
  if (is<Bitslice>(n)) return false;
  if (VectorAccess const* va = dynamic_cast<VectorAccess const*>(&n)) return svSigned(*va->lhs());
  if (is<ExtendExpression>(n) || is<NegOpr>(n) || is<ComplementOpr>(n))
    return svSigned(*static_cast<UnaryExpression const&>(n).child());
  if (is<ShiftLeftOpr>(n) || is<ShiftRightOpr>(n)) return svSigned(*static_cast<BinaryExpression const&>(n).lhs());
  if (is<PlusOpr>(n) || is<MinusOpr>(n) || is<MultipliesOpr>(n) || is<DevideOpr>(n) || is<ModuloOpr>(n) ||
      is<AndOpr>(n) || is<OrOpr>(n) || is<XorOpr>(n)) {
    BinaryExpression const& b = static_cast<BinaryExpression const&>(n);
    return svSigned(*b.lhs()) && svSigned(*b.rhs());
  }
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&n)) return svSigned(*ite->b()) && svSigned(*ite->c());
  return false;
}

typedef boost::intrusive_ptr<Node> NodeRef;

// The test of an integer for zero, which stands for the integer where a boolean is expected, e.g. as an operand of
// a logical operator. With the rules of SystemVerilog, the integer is self-determined there (IEEE 1800-2017
// 11.6.1), so this test is not a comparison, whose operands are context-determined.
class ZeroTest : public NotEqualOpr {
 public:
  ZeroTest(NodePtr lhs, NodePtr rhs) : NotEqualOpr(lhs, rhs) {}
};

NodeRef svInteger(NodeRef const& n, unsigned width, bool sign);
NodeRef svBoolean(NodeRef const& n);

uint64_t svMask(unsigned width) { return width >= 64 ? ~uint64_t(0) : (uint64_t(1) << width) - 1; }

bool isArithmetic(Node const& n) {
  return is<PlusOpr>(n) || is<MinusOpr>(n) || is<MultipliesOpr>(n) || is<DevideOpr>(n) || is<ModuloOpr>(n) ||
         is<AndOpr>(n) || is<OrOpr>(n) || is<XorOpr>(n);
}

// IEEE 1800-2017 11.6.1 (Table 11-21): the self-determined width of an expression. The operands of an
// arithmetic or bitwise operator and the branches of crave::if_then_else have the width of the widest one,
// a unary operator and a shift the width of its (left) operand, a bit slice its own width and a boolean,
// like a comparison, one bit. The extensions, which were added to equalize the widths of the operands, are
// not counted.
unsigned svWidth(Node const& n) {
  if (Terminal const* t = dynamic_cast<Terminal const*>(&n)) return t->bitsize();
  if (is<Placeholder>(n)) return placeholder_bitsize();
  if (VectorAccess const* va = dynamic_cast<VectorAccess const*>(&n)) return svWidth(*va->lhs());
  if (Bitslice const* b = dynamic_cast<Bitslice const*>(&n)) return b->r() - b->l() + 1;
  if (isBoolean(n)) return 1;
  if (is<ExtendExpression>(n) || is<NegOpr>(n) || is<ComplementOpr>(n))
    return svWidth(*static_cast<UnaryExpression const&>(n).child());
  if (is<ShiftLeftOpr>(n) || is<ShiftRightOpr>(n)) return svWidth(*static_cast<BinaryExpression const&>(n).lhs());
  if (isArithmetic(n)) {
    BinaryExpression const& b = static_cast<BinaryExpression const&>(n);
    return std::max(svWidth(*b.lhs()), svWidth(*b.rhs()));
  }
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&n))
    return std::max(svWidth(*ite->b()), svWidth(*ite->c()));
  return 1;
}

// A self-determined operand has its own width and type.
NodeRef svSelf(NodeRef const& n) { return svInteger(n, svWidth(*n), svSigned(*n)); }

NodeRef svExtend(NodeRef const& n, unsigned from, unsigned to) {
  return from < to ? NodeRef(new ExtendExpression(n, to - from)) : n;
}

template <typename T>
NodeRef svBinary(Node const& n, unsigned width, bool sign) {
  T const& b = static_cast<T const&>(n);
  return new T(svInteger(b.lhs(), width, sign), svInteger(b.rhs(), width, sign));
}

// IEEE 1800-2017 11.4.10: the shift amount is self-determined and unsigned. As the solver shifts operands of
// one width, the amount is zero-extended to the width of the shift. An amount, which is wider than the shift,
// shifts out all bits if it is not less than the width of the shift.
template <typename T>
NodeRef svShift(Node const& n, unsigned width, bool sign) {
  T const& b = static_cast<T const&>(n);
  NodeRef value = svInteger(b.lhs(), width, sign);
  unsigned const amountWidth = svWidth(*b.rhs());
  NodeRef amount = svSelf(b.rhs());
  if (svSigned(*b.rhs())) amount = new Bitslice(amount, amountWidth - 1, 0);
  if (amountWidth <= width) return new T(value, svExtend(amount, amountWidth, width));
  return new IfThenElse(new GreaterEqualOpr(amount, new Constant(width, amountWidth, false)),
                        new Constant(0, width, sign), new T(value, new Bitslice(amount, width - 1, 0)));
}

// IEEE 1800-2017 11.6.1 and 11.8.1: the operands of a relational or an equality operator are
// context-determined among themselves. They are extended to the width of the wider one and converted to a
// common type, which is unsigned if one of them is unsigned. The result is a 1-bit unsigned value.
template <typename T>
NodeRef svCompare(Node const& n) {
  T const& b = static_cast<T const&>(n);
  // A boolean compared with a boolean, e.g. a comparison in an equality, keeps its own rules. Integers are
  // promoted from booleans before, so a boolean is compared with another boolean only.
  if (isBoolean(*b.lhs()) && isBoolean(*b.rhs())) {
    if (is<EqualOpr>(n) || is<NotEqualOpr>(n)) return new T(svBoolean(b.lhs()), svBoolean(b.rhs()));
    // a relational operator compares the booleans as 1-bit unsigned values
    return new T(new IfThenElse(svBoolean(b.lhs()), new Constant(1, 1, false), new Constant(0, 1, false)),
                 new IfThenElse(svBoolean(b.rhs()), new Constant(1, 1, false), new Constant(0, 1, false)));
  }
  unsigned const width = std::max(svWidth(*b.lhs()), svWidth(*b.rhs()));
  bool const sign = svSigned(*b.lhs()) && svSigned(*b.rhs());
  return new T(svInteger(b.lhs(), width, sign), svInteger(b.rhs(), width, sign));
}

// IEEE 1800-2017 11.6.2 and 11.8.2: the width and the type of an expression are propagated to its
// context-determined operands. Every operand is extended to that width before the operation, and it is
// sign-extended only if that type is signed. A signed operand in an unsigned context is converted by a bit
// slice of all its bits, which is unsigned, so every extension in an unsigned context is a zero extension.
// The result has the given width, and it is signed if the given type is signed.
NodeRef svInteger(NodeRef const& n, unsigned width, bool sign) {
  Node const& node = *n;
  if (isBoolean(node)) return svBoolean(n);
  // the extensions of the default rules are replaced
  if (ExtendExpression const* e = dynamic_cast<ExtendExpression const*>(&node))
    return svInteger(e->child(), width, sign);
  if (Constant const* c = dynamic_cast<Constant const*>(&node)) {
    uint64_t value = c->value() & svMask(c->bitsize());
    if (sign) {
      // a signed constant is kept sign-extended, as by crave::Context
      if (c->bitsize() < 64 && ((value >> (c->bitsize() - 1)) & 1)) value |= ~svMask(c->bitsize());
      return new Constant(value, width, true);
    }
    return new Constant(value, width, false);
  }
  if (is<Terminal>(node) || is<VectorAccess>(node) || is<Placeholder>(node)) {
    unsigned const own = svWidth(node);
    NodeRef operand = n;
    if (!sign && svSigned(node)) operand = new Bitslice(n, own - 1, 0);
    return svExtend(operand, own, width);
  }
  if (Bitslice const* b = dynamic_cast<Bitslice const*>(&node))
    return svExtend(new Bitslice(svSelf(b->child()), b->r(), b->l()), b->r() - b->l() + 1, width);
  if (NegOpr const* o = dynamic_cast<NegOpr const*>(&node)) return new NegOpr(svInteger(o->child(), width, sign));
  if (ComplementOpr const* o = dynamic_cast<ComplementOpr const*>(&node))
    return new ComplementOpr(svInteger(o->child(), width, sign));
  if (is<ShiftLeftOpr>(node)) return svShift<ShiftLeftOpr>(node, width, sign);
  if (is<ShiftRightOpr>(node)) return svShift<ShiftRightOpr>(node, width, sign);
  if (is<PlusOpr>(node)) return svBinary<PlusOpr>(node, width, sign);
  if (is<MinusOpr>(node)) return svBinary<MinusOpr>(node, width, sign);
  if (is<MultipliesOpr>(node)) return svBinary<MultipliesOpr>(node, width, sign);
  if (is<DevideOpr>(node)) return svBinary<DevideOpr>(node, width, sign);
  if (is<ModuloOpr>(node)) return svBinary<ModuloOpr>(node, width, sign);
  if (is<AndOpr>(node)) return svBinary<AndOpr>(node, width, sign);
  if (is<OrOpr>(node)) return svBinary<OrOpr>(node, width, sign);
  if (is<XorOpr>(node)) return svBinary<XorOpr>(node, width, sign);
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&node))
    return new IfThenElse(svBoolean(ite->a()), svInteger(ite->b(), width, sign), svInteger(ite->c(), width, sign));
  return n;
}

NodeRef svBoolean(NodeRef const& n) {
  Node const& node = *n;
  if (ZeroTest const* z = dynamic_cast<ZeroTest const*>(&node)) {
    NodeRef const& e = z->lhs();
    return new NotEqualOpr(svSelf(e), new Constant(0, svWidth(*e), svSigned(*e)));
  }
  if (NotOpr const* o = dynamic_cast<NotOpr const*>(&node)) return new NotOpr(svBoolean(o->child()));
  if (LogicalAndOpr const* o = dynamic_cast<LogicalAndOpr const*>(&node))
    return new LogicalAndOpr(svBoolean(o->lhs()), svBoolean(o->rhs()));
  if (LogicalOrOpr const* o = dynamic_cast<LogicalOrOpr const*>(&node))
    return new LogicalOrOpr(svBoolean(o->lhs()), svBoolean(o->rhs()));
  if (is<EqualOpr>(node)) return svCompare<EqualOpr>(node);
  if (is<NotEqualOpr>(node)) return svCompare<NotEqualOpr>(node);
  if (is<LessOpr>(node)) return svCompare<LessOpr>(node);
  if (is<LessEqualOpr>(node)) return svCompare<LessEqualOpr>(node);
  if (is<GreaterOpr>(node)) return svCompare<GreaterOpr>(node);
  if (is<GreaterEqualOpr>(node)) return svCompare<GreaterEqualOpr>(node);
  if (IfThenElse const* ite = dynamic_cast<IfThenElse const*>(&node))
    return new IfThenElse(svBoolean(ite->a()), svBoolean(ite->b()), svBoolean(ite->c()));
  if (Inside const* in = dynamic_cast<Inside const*>(&node)) return new Inside(svSelf(in->child()), in->collection());
  if (ForEach const* fe = dynamic_cast<ForEach const*>(&node)) return new ForEach(fe->lhs(), svBoolean(fe->rhs()));
  return n;
}
}  // namespace

FixWidthVisitor::FixWidthVisitor() : NodeVisitor(), exprStack_(), sv_signedness_(sv_signedness()) {}

FixWidthVisitor::result_type FixWidthVisitor::fixWidth(Node const& expr) {
  expr.visit(this);
  stack_entry entry;
  pop(entry);
  // As in C++, a constraint, which is an integer expression, stands for its comparison with zero.
  result_type result = toBoolean(entry);
  return sv_signedness_ ? svBoolean(result) : result;
}

FixWidthVisitor::result_type FixWidthVisitor::fixWidthExpr(Node const& expr) {
  expr.visit(this);
  stack_entry entry;
  pop(entry);
  if (!sv_signedness_) return entry.first;
  return isBoolean(*entry.first) ? svBoolean(entry.first) : svSelf(entry.first);
}

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
  if (sv_signedness_) return new ZeroTest(e.first, new Constant(0, e.second, false));
  return new NotEqualOpr(e.first, new Constant(0, e.second, false));
}

// As in C++, a boolean promotes to the int 1 for true and 0 for false.
void FixWidthVisitor::promoteToInteger(stack_entry& e) {
  if (!isBoolean(*e.first)) return;
  // A boolean is a 1-bit unsigned value in SystemVerilog, which is zero-extended.
  unsigned const width = sv_signedness_ ? 1 : bitsize_traits<int>::value;
  bool const sign = sv_signedness_ ? false : crave::is_signed<int>::value;
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

void FixWidthVisitor::visitVectorReduction(const VectorReduction& r) {
  exprStack_.push(std::make_pair(new VectorReduction(r), r.bitsize()));
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

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


#include "../crave/ir/visitor/EvalVisitor.hpp"

#include <algorithm>
#include <stdexcept>
#include <set>

namespace crave {

namespace {
uint64_t mask(unsigned width) { return width >= 64 ? ~uint64_t(0) : (uint64_t(1) << width) - 1; }

bool isNegative(Constant const& c) { return c.sign() && !c.isBool() && (c.value() >> 63) != 0; }

__int128 toInt128(Constant const& c) {
  if (c.isBool()) return c.value() ? 1 : 0;
  unsigned w = c.bitsize();
  unsigned __int128 raw = static_cast<unsigned __int128>(c.value());
  if (w > 0 && w < 64) raw &= ((static_cast<unsigned __int128>(1) << w) - 1);
  if (c.sign() && w > 0 && w <= 64) {
    unsigned __int128 sign_bit = static_cast<unsigned __int128>(1) << (w - 1);
    if (raw & sign_bit) raw |= (~static_cast<unsigned __int128>(0)) << w;
  }
  return static_cast<__int128>(raw);
}

// compares the values of the integers, also if one is signed and the other unsigned, as the solver does
int compare(Constant const& a, Constant const& b) {
  if (isNegative(a) != isNegative(b)) return isNegative(a) ? -1 : 1;
  return a.value() < b.value() ? -1 : (a.value() > b.value() ? 1 : 0);
}
}  // namespace

// In the exact mode, a value is kept truncated to its width, and sign-extended if it is signed.
void EvalVisitor::pushExact(uint64_t value, unsigned width, bool sign, bool valid) {
  value &= mask(width);
  if (sign && width > 0 && width < 64 && ((value >> (width - 1)) & 1)) value |= ~mask(width);
  exprStack_.push(std::make_pair(Constant(value, width, sign), valid));
}

// evaluates the arithmetic and bitwise operators in the exact mode, as the solver does
bool EvalVisitor::exactBinExpr(BinaryExpression const& b) {
  if (!exact_) return false;
  stack_entry lhs, rhs;
  evalBinExpr(b, lhs, rhs);
  Constant const& l = lhs.first;
  Constant const& r = rhs.first;
  bool valid = lhs.second && rhs.second;
  unsigned const width = l.bitsize();
  bool const sign = l.sign() || r.sign();
  uint64_t value = 0;
  if (dynamic_cast<AndOpr const*>(&b)) {
    value = l.value() & r.value();
  } else if (dynamic_cast<OrOpr const*>(&b)) {
    value = l.value() | r.value();
  } else if (dynamic_cast<XorOpr const*>(&b)) {
    value = l.value() ^ r.value();
  } else if (dynamic_cast<PlusOpr const*>(&b)) {
    value = l.value() + r.value();
  } else if (dynamic_cast<MinusOpr const*>(&b)) {
    value = l.value() - r.value();
  } else if (dynamic_cast<MultipliesOpr const*>(&b)) {
    value = l.value() * r.value();
  } else if (dynamic_cast<DevideOpr const*>(&b) || dynamic_cast<ModuloOpr const*>(&b)) {
    bool const divide = dynamic_cast<DevideOpr const*>(&b) != 0;
    if (r.value() == 0) {
      exprStack_.push(std::make_pair(Constant(), false));
      return true;
    }
    // as the solver, a division is signed if both operands are signed, a remainder if its dividend is
    if (divide ? (l.sign() && r.sign()) : l.sign()) {
      int64_t const x = static_cast<int64_t>(l.value());
      int64_t const y = static_cast<int64_t>(r.value());
      if (y == -1)
        value = divide ? uint64_t(0) - l.value() : 0;
      else
        value = static_cast<uint64_t>(divide ? x / y : x % y);
    } else {
      uint64_t const x = l.value() & mask(width);
      uint64_t const y = r.value() & mask(width);
      value = divide ? x / y : x % y;
    }
  } else {
    // shifts, whose type is the type of the left operand; a right shift is a logical shift
    uint64_t const amount = r.value() & mask(r.bitsize());
    uint64_t const v = l.value() & mask(width);
    if (amount >= width)
      value = 0;
    else if (dynamic_cast<ShiftLeftOpr const*>(&b))
      value = v << amount;
    else
      value = v >> amount;
    pushExact(value, width, l.sign(), valid);
    return true;
  }
  pushExact(value, width, sign, valid);
  return true;
}

// evaluates the relational and equality operators in the exact mode, as the solver does
bool EvalVisitor::exactCompare(BinaryExpression const& b) {
  if (!exact_) return false;
  stack_entry lhs, rhs;
  evalBinExpr(b, lhs, rhs);
  Constant const& l = lhs.first;
  Constant const& r = rhs.first;
  bool result;
  if (dynamic_cast<EqualOpr const*>(&b) || dynamic_cast<NotEqualOpr const*>(&b)) {
    uint64_t const m = mask(std::max(l.bitsize(), r.bitsize()));
    result = ((l.value() & m) == (r.value() & m)) == (dynamic_cast<EqualOpr const*>(&b) != 0);
  } else {
    int const c = compare(l, r);
    if (dynamic_cast<LessOpr const*>(&b))
      result = c < 0;
    else if (dynamic_cast<LessEqualOpr const*>(&b))
      result = c <= 0;
    else if (dynamic_cast<GreaterOpr const*>(&b))
      result = c > 0;
    else
      result = c >= 0;
  }
  exprStack_.push(std::make_pair(Constant(result), lhs.second && rhs.second));
  return true;
}

void EvalVisitor::visitNode(const Node&) {}

void EvalVisitor::visitTerminal(const Terminal&) {}

void EvalVisitor::visitUnaryExpr(const UnaryExpression& u) { u.child()->visit(this); }

void EvalVisitor::visitUnaryOpr(const UnaryOperator&) {}

void EvalVisitor::visitBinaryExpr(const BinaryExpression& b) {
  b.lhs()->visit(this);
  b.rhs()->visit(this);
}

void EvalVisitor::visitBinaryOpr(const BinaryOperator&) {}

void EvalVisitor::visitTernaryExpr(const TernaryExpression& t) {
  t.a()->visit(this);
  t.b()->visit(this);
  t.c()->visit(this);
}

void EvalVisitor::visitPlaceholder(const Placeholder&) {}

void EvalVisitor::visitVariableExpr(const VariableExpr& v) {
  eval_map::const_iterator ite(evalMap_->find(v.id()));
  if (ite != evalMap_->end())
    visitConstant(ite->second);
  else
    exprStack_.push(std::make_pair(Constant(), false));
}

void EvalVisitor::visitConstant(const Constant& c) {
  if (exact_ && !c.isBool())
    pushExact(c.value(), c.bitsize(), c.sign(), true);
  else
    exprStack_.push(std::make_pair(c, true));
}

// The elements of a vector are not assigned, so its reduction has no value.
void EvalVisitor::visitVectorReduction(const VectorReduction&) { exprStack_.push(std::make_pair(Constant(), false)); }

void EvalVisitor::visitVectorExpr(const VectorExpr& v) {
  eval_map::const_iterator ite(evalMap_->find(v.id()));
  if (ite != evalMap_->end())
    visitConstant(ite->second);
  else
    exprStack_.push(std::make_pair(Constant(), false));
}

void EvalVisitor::visitNotOpr(const NotOpr& n) {
  visitUnaryExpr(n);

  stack_entry e;
  pop(e);

  exprStack_.push(std::make_pair(Constant(!e.first.value()), e.second));
}

void EvalVisitor::visitNegOpr(const NegOpr& n) {
  visitUnaryExpr(n);

  stack_entry e;
  pop(e);

  if (exact_)
    pushExact(uint64_t(0) - e.first.value(), e.first.bitsize(), e.first.sign(), e.second);
  else
    exprStack_.push(std::make_pair(Constant(-e.first.value(), e.first.bitsize(), e.first.sign()), e.second));
}

void EvalVisitor::visitComplementOpr(const ComplementOpr& c) {
  visitUnaryExpr(c);

  stack_entry e;
  pop(e);

  if (exact_)
    pushExact(~e.first.value(), e.first.bitsize(), e.first.sign(), e.second);
  else
    exprStack_.push(std::make_pair(Constant(~e.first.value(), e.first.bitsize(), e.first.sign()), e.second));
}

void EvalVisitor::visitInside(const Inside& i) {
  visitUnaryExpr(i);

  stack_entry e;
  pop(e);

  bool result = false;

  uint64_t const m = exact_ ? mask(e.first.bitsize()) : ~uint64_t(0);
  for (std::set<Constant>::iterator ite = i.collection().begin(); ite != i.collection().end(); ++ite)
    if ((ite->value() & m) == (e.first.value() & m)) result = true;

  exprStack_.push(std::make_pair(Constant(result), e.second));
}

void EvalVisitor::visitExtendExpr(const ExtendExpression& e) {
  visitUnaryExpr(e);

  stack_entry entry;
  pop(entry);

  Constant const& child = entry.first;
  if (exact_) {
    // the value of the child is already sign-extended if it is signed
    pushExact(child.value(), child.bitsize() + e.value(), child.sign(), entry.second);
    return;
  }
  Constant constant(child.value(), child.bitsize() + e.value(), child.sign());
  exprStack_.push(std::make_pair(constant, entry.second));
}

void EvalVisitor::visitAndOpr(const AndOpr& a) {
  if (exactBinExpr(a)) return;
  stack_entry lhs, rhs;
  evalBinExpr(a, lhs, rhs);

  exprStack_.push(std::make_pair(
      Constant(lhs.first.value() & rhs.first.value(), lhs.first.bitsize(), lhs.first.sign() || rhs.first.sign()),
      lhs.second && rhs.second));
}

void EvalVisitor::visitOrOpr(const OrOpr& o) {
  if (exactBinExpr(o)) return;
  stack_entry lhs, rhs;
  evalBinExpr(o, lhs, rhs);

  exprStack_.push(std::make_pair(
      Constant(lhs.first.value() | rhs.first.value(), lhs.first.bitsize(), lhs.first.sign() || rhs.first.sign()),
      lhs.second && rhs.second));
}

void EvalVisitor::visitLogicalAndOpr(const LogicalAndOpr& la) {
  stack_entry lhs, rhs;
  evalBinExpr(la, lhs, rhs);

  exprStack_.push(std::make_pair(Constant(lhs.first.value() && rhs.first.value()), lhs.second && rhs.second));
}

void EvalVisitor::visitLogicalOrOpr(const LogicalOrOpr& lo) {
  stack_entry lhs, rhs;
  evalBinExpr(lo, lhs, rhs);

  exprStack_.push(std::make_pair(Constant(lhs.first.value() || rhs.first.value()), lhs.second && rhs.second));
}

void EvalVisitor::visitXorOpr(const XorOpr& x) {
  if (exactBinExpr(x)) return;
  stack_entry lhs, rhs;
  evalBinExpr(x, lhs, rhs);

  exprStack_.push(std::make_pair(
      Constant(lhs.first.value() ^ rhs.first.value(), lhs.first.bitsize(), lhs.first.sign() || rhs.first.sign()),
      lhs.second && rhs.second));
}

void EvalVisitor::visitEqualOpr(const EqualOpr& eq) {
  if (exactCompare(eq)) return;
  stack_entry lhs, rhs;
  evalBinExpr(eq, lhs, rhs);

  exprStack_.push(std::make_pair(Constant(lhs.first.value() == rhs.first.value()), lhs.second && rhs.second));
}

void EvalVisitor::visitNotEqualOpr(const NotEqualOpr& neq) {
  if (exactCompare(neq)) return;
  stack_entry lhs, rhs;
  evalBinExpr(neq, lhs, rhs);

  exprStack_.push(std::make_pair(Constant(lhs.first.value() != rhs.first.value()), lhs.second && rhs.second));
}

void EvalVisitor::visitLessOpr(const LessOpr& l) {
  if (exactCompare(l)) return;
  stack_entry lhs, rhs;
  evalBinExpr(l, lhs, rhs);

  exprStack_.push(std::make_pair(Constant(lhs.first.value() < rhs.first.value()), lhs.second && rhs.second));
}

void EvalVisitor::visitLessEqualOpr(const LessEqualOpr& le) {
  if (exactCompare(le)) return;
  stack_entry lhs, rhs;
  evalBinExpr(le, lhs, rhs);

  exprStack_.push(std::make_pair(Constant(lhs.first.value() <= rhs.first.value()), lhs.second && rhs.second));
}

void EvalVisitor::visitGreaterOpr(const GreaterOpr& g) {
  if (exactCompare(g)) return;
  stack_entry lhs, rhs;
  evalBinExpr(g, lhs, rhs);

  exprStack_.push(std::make_pair(Constant(lhs.first.value() > rhs.first.value()), lhs.second && rhs.second));
}

void EvalVisitor::visitGreaterEqualOpr(const GreaterEqualOpr& ge) {
  if (exactCompare(ge)) return;
  stack_entry lhs, rhs;
  evalBinExpr(ge, lhs, rhs);

  exprStack_.push(std::make_pair(Constant(lhs.first.value() >= rhs.first.value()), lhs.second && rhs.second));
}

void EvalVisitor::visitPlusOpr(const PlusOpr& p) {
  if (exactBinExpr(p)) return;
  stack_entry lhs, rhs;
  evalBinExpr(p, lhs, rhs);

  exprStack_.push(std::make_pair(
      Constant(lhs.first.value() + rhs.first.value(), lhs.first.bitsize(), lhs.first.sign() || rhs.first.sign()),
      lhs.second && rhs.second));
}

void EvalVisitor::visitMinusOpr(const MinusOpr& m) {
  if (exactBinExpr(m)) return;
  stack_entry lhs, rhs;
  evalBinExpr(m, lhs, rhs);

  exprStack_.push(std::make_pair(
      Constant(lhs.first.value() - rhs.first.value(), lhs.first.bitsize(), lhs.first.sign() || rhs.first.sign()),
      lhs.second && rhs.second));
}

void EvalVisitor::visitMultipliesOpr(const MultipliesOpr& m) {
  if (exactBinExpr(m)) return;
  stack_entry lhs, rhs;
  evalBinExpr(m, lhs, rhs);

  exprStack_.push(std::make_pair(
      Constant(lhs.first.value() * rhs.first.value(), lhs.first.bitsize(), lhs.first.sign() || rhs.first.sign()),
      lhs.second && rhs.second));
}

void EvalVisitor::visitDevideOpr(const DevideOpr& d) {
  if (exactBinExpr(d)) return;
  stack_entry lhs, rhs;
  evalBinExpr(d, lhs, rhs);

  bool result;
  Constant constant;

  if (0 == rhs.first.value()) {
    result = false;
    constant = Constant();
  } else {
    result = lhs.second && rhs.second;

    if (lhs.first.sign() || rhs.first.sign())
      constant = Constant(static_cast<long long>(lhs.first.value()) / static_cast<long long>(rhs.first.value()),
                          lhs.first.bitsize(), true);
    else
      constant = Constant(lhs.first.value() / rhs.first.value(), lhs.first.bitsize(), false);
  }
  exprStack_.push(std::make_pair(constant, result));
}

void EvalVisitor::visitModuloOpr(const ModuloOpr& m) {
  if (exactBinExpr(m)) return;
  stack_entry lhs, rhs;
  evalBinExpr(m, lhs, rhs);

  bool result;
  Constant constant;

  if (0 == rhs.first.value()) {
    result = false;
    constant = Constant();
  } else {
    result = lhs.second && rhs.second;

    if (lhs.first.sign() || rhs.first.sign())
      constant = Constant(static_cast<long long>(lhs.first.value()) % static_cast<long long>(rhs.first.value()),
                          lhs.first.bitsize(), true);
    else
      constant = Constant(lhs.first.value() % rhs.first.value(), lhs.first.bitsize(), false);
  }
  exprStack_.push(std::make_pair(constant, result));
}

void EvalVisitor::visitShiftLeftOpr(const ShiftLeftOpr& shl) {
  if (exactBinExpr(shl)) return;
  stack_entry lhs, rhs;
  evalBinExpr(shl, lhs, rhs);

  exprStack_.push(std::make_pair(
      Constant(lhs.first.value() << rhs.first.value(), lhs.first.bitsize(), lhs.first.sign() || rhs.first.sign()),
      lhs.second && rhs.second));
}

void EvalVisitor::visitShiftRightOpr(const ShiftRightOpr& shr) {
  if (exactBinExpr(shr)) return;
  stack_entry lhs, rhs;
  evalBinExpr(shr, lhs, rhs);

  exprStack_.push(std::make_pair(
      Constant(lhs.first.value() >> rhs.first.value(), lhs.first.bitsize(), lhs.first.sign() || rhs.first.sign()),
      lhs.second && rhs.second));
}

void EvalVisitor::visitVectorAccess(const VectorAccess&) {
  throw std::runtime_error("VectorAccess is not allowed in EvalVisitor.");
}
void EvalVisitor::visitForEach(const ForEach&) {
  throw std::runtime_error("ForEach is not allowed in EvalVisitor.");
}
void EvalVisitor::visitUnique(const Unique&) { throw std::runtime_error("Unique is not allowed in EvalVisitor."); }

void EvalVisitor::visitIfThenElse(const IfThenElse& ite) {
  stack_entry a, b, c;
  evalTernExpr(ite, a, b, c);

  if (a.first.value())
    exprStack_.push(b);
  else
    exprStack_.push(c);
}

void EvalVisitor::visitBitslice(const Bitslice& b) {
  visitUnaryExpr(b);

  stack_entry entry;
  pop(entry);

  Constant const& child = entry.first;
  if (b.l() < 0 || b.r() < b.l() || b.r() >= 64) {
    exprStack_.push(std::make_pair(Constant(), false));
    return;
  }
  unsigned long long v = 0;
  for (int i = b.l(); i <= b.r(); i++) v |= (1ULL << i);
  Constant constant((v & child.value()) >> b.l(), b.r() - b.l() + 1, false);
  exprStack_.push(std::make_pair(constant, exact_ ? entry.second : false));
}

void EvalVisitor::visitRangeExpr(const RangeExpr& r) {
  stack_entry a, b, c;
  evalTernExpr(r, a, b, c);
  __int128 const x = toInt128(a.first);
  __int128 const lo = toInt128(b.first);
  __int128 const hi = toInt128(c.first);
  bool const res = (lo <= x) && (x <= hi);
  exprStack_.push(std::make_pair(Constant(res), a.second && b.second && c.second));
}

void EvalVisitor::visitTolPlusMinusExpr(const TolPlusMinusExpr& t) {
  stack_entry a, b, c;
  evalTernExpr(t, a, b, c);
  __int128 const x = toInt128(a.first);
  __int128 const val = toInt128(b.first);
  __int128 const tol = toInt128(c.first);
  bool const res = ((val - tol) <= x) && (x <= (val + tol));
  exprStack_.push(std::make_pair(Constant(res), a.second && b.second && c.second));
}

void EvalVisitor::visitTolPlusPctExpr(const TolPlusPctExpr& t) {
  stack_entry a, b, c;
  evalTernExpr(t, a, b, c);
  __int128 const x = toInt128(a.first);
  __int128 const val = toInt128(b.first);
  __int128 const pct = toInt128(c.first);
  bool res = false;
  if (pct >= 0) {
    unsigned __int128 const mag =
        val < 0 ? static_cast<unsigned __int128>(-val) : static_cast<unsigned __int128>(val);
    __int128 const delta = static_cast<__int128>((mag * static_cast<unsigned __int128>(pct)) / 100);
    res = ((val - delta) <= x) && (x <= (val + delta));
  }
  exprStack_.push(std::make_pair(Constant(res), a.second && b.second && c.second));
}

void EvalVisitor::visitVectorContains(const VectorContains&) {
  throw std::runtime_error("VectorContains is not allowed in EvalVisitor.");
}

}  // end namespace crave

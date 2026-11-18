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


#include <boost/test/unit_test.hpp>

#include <crave/utils/Evaluator.hpp>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <stdexcept>
#include <vector>

using namespace crave;

namespace {
// selects the signedness rules for the lifetime of the object
struct sv_signedness_guard {
  explicit sv_signedness_guard(bool on) : old_(sv_signedness()) { set_sv_signedness(on); }
  ~sv_signedness_guard() { set_sv_signedness(old_); }
  bool old_;
};

template <typename Expr>
bool satisfiable(Expr e) {
  Generator gen(e);
  return gen.next();
}
}  // namespace

BOOST_FIXTURE_TEST_SUITE(SvSignedness_t, Context_Fixture)

// By default, an operation with a signed operand is signed, a signed operand is sign-extended, and
// a comparison of a signed and an unsigned operand compares their values.
BOOST_AUTO_TEST_CASE(default_signedness) {
  sv_signedness_guard guard(false);
  Variable<bool> f;
  Variable<signed char> s;
  Variable<unsigned> u;

  BOOST_REQUIRE(!satisfiable(f && s == -1 && u == 200u && if_then_else(f, s, u) > 5u));
  BOOST_REQUIRE(!satisfiable(s == -1 && s + 0u > 5u));
  BOOST_REQUIRE(satisfiable(s == -1 && s == 0xFFFFFFFFu));
  BOOST_REQUIRE(!satisfiable(f && f - 2 > 0));
  BOOST_REQUIRE(!satisfiable(s > 200u));
  BOOST_REQUIRE(satisfiable(s == -1 && s - 1 < 0));
}

// IEEE 1800-2017 11.8.1: an expression is unsigned if one of its operands is unsigned.
// IEEE 1800-2017 11.8.2: an operand is sign-extended only if the type of the expression is signed.
BOOST_AUTO_TEST_CASE(sv_signedness_rules) {
  sv_signedness_guard guard(true);
  Variable<bool> f;
  Variable<signed char> s;
  Variable<unsigned> u;
  Variable<int> x;

  // -1 is zero-extended to 255
  BOOST_REQUIRE(satisfiable(f && s == -1 && u == 200u && if_then_else(f, s, u) > 5u));
  BOOST_REQUIRE(satisfiable(s == -1 && s + 0u > 5u));
  BOOST_REQUIRE(satisfiable(s == -1 && s == 255u));
  BOOST_REQUIRE(!satisfiable(s == -1 && s == 0xFFFFFFFFu));
  // an unsigned division
  BOOST_REQUIRE(satisfiable(x == -8 && x / 2u == 0x7FFFFFFCu));
  // a boolean is an unsigned bit
  BOOST_REQUIRE(satisfiable(f && f - 2 > 0));
  // a signed expression is unchanged
  BOOST_REQUIRE(satisfiable(s == -1 && s - 1 < 0));
  BOOST_REQUIRE(!satisfiable(s == -1 && s - 1 > 0));
  // a self-determined condition keeps its type
  BOOST_REQUIRE(satisfiable(s == -1 && if_then_else(s < 0, 1u, 2u) == 1u));
}

// The values of a signed variable that satisfy a comparison with an unsigned value
BOOST_AUTO_TEST_CASE(sv_signedness_solutions) {
  sv_signedness_guard guard(true);
  Variable<signed char> s;
  Generator gen(s > 200u);

  std::set<int> generated;
  for (int i = 0; i < 55; ++i) {
    BOOST_REQUIRE(gen.next());
    int const v = gen[s];
    BOOST_REQUIRE(v >= -55 && v <= -1);
    BOOST_REQUIRE(generated.insert(v).second);
    gen(s != v);
  }
  BOOST_REQUIRE_EQUAL(generated.size(), 55u);
}

// The setting applies to the constraints defined after it is changed.
BOOST_AUTO_TEST_CASE(sv_signedness_at_definition) {
  Variable<signed char> s;
  Generator gen(s == -1);
  {
    sv_signedness_guard guard(true);
    gen(s + 0u > 5u);
  }
  BOOST_REQUIRE(gen.next());
  {
    sv_signedness_guard guard(false);
    gen(s + 0u > 5u);
  }
  BOOST_REQUIRE(!gen.next());
}

// The solver and the evaluator agree on the value of an expression for every value of its variable.
BOOST_AUTO_TEST_CASE(sv_signedness_solver_evaluator_agree) {
  sv_signedness_guard guard(true);
  Variable<signed char> s;
  std::vector<expression> exprs;
  exprs.push_back(make_expression(s + 0u > 5u));
  exprs.push_back(make_expression(if_then_else(s < 0, s, 200u) > 5u));
  exprs.push_back(make_expression(s == 255u));
  exprs.push_back(make_expression(s / 2u < 100u));
  exprs.push_back(make_expression(s % 3u == 1u));
  exprs.push_back(make_expression(s % 3 == -1));
  exprs.push_back(make_expression((s >> 1) > 100));
  exprs.push_back(make_expression((s >> 1u) > 100));
  exprs.push_back(make_expression((s << 1u) > 100u));
  exprs.push_back(make_expression(-s > 100u));
  exprs.push_back(make_expression(~s < 100u));
  exprs.push_back(make_expression((s & 0xF0u) == 0xF0u));
  exprs.push_back(make_expression(s * 2u > 300u));
  exprs.push_back(make_expression(s - 1 < 0));
  exprs.push_back(make_expression((s < 0) - 2 > 0));
  exprs.push_back(make_expression(bitslice(7, 4, s) + s > 20));

  for (unsigned i = 0; i < exprs.size(); ++i) {
    for (int v = -128; v < 128; ++v) {
      Generator gen(s == v);
      gen(exprs[i]);
      Evaluator eval;
      eval.assign(s, static_cast<signed char>(v));
      BOOST_REQUIRE(eval.evaluate(exprs[i]));
      if (eval.result<bool>() != gen.next()) BOOST_FAIL("expression " << i << " disagrees for " << v);
    }
  }
}

// The values follow SystemVerilog, where a signed operand of an unsigned expression is zero-extended.
BOOST_AUTO_TEST_CASE(sv_signedness_evaluator_values) {
  sv_signedness_guard guard(true);
  Variable<signed char> s;
  Variable<unsigned short> w;
  Evaluator eval;

  for (int v = -128; v < 128; ++v) {
    unsigned char const z = static_cast<unsigned char>(v);
    eval.assign(s, static_cast<signed char>(v));
    eval.assign(w, 1000u);

    BOOST_REQUIRE(eval.evaluate(s + 0u > 5u));
    BOOST_REQUIRE_EQUAL(eval.result<bool>(), z > 5u);
    BOOST_REQUIRE(eval.evaluate(s == 255u));
    BOOST_REQUIRE_EQUAL(eval.result<bool>(), z == 255u);
    BOOST_REQUIRE(eval.evaluate(s + 0u));
    BOOST_REQUIRE_EQUAL(eval.result<unsigned>(), z);
    BOOST_REQUIRE(eval.evaluate(s + w));
    BOOST_REQUIRE_EQUAL(eval.result<unsigned short>(), static_cast<unsigned short>(z + 1000u));
    BOOST_REQUIRE(eval.evaluate(s - 1 < 0));
    // the operation has the width of its context, which is the width of int
    BOOST_REQUIRE_EQUAL(eval.result<bool>(), v - 1 < 0);
  }
}

// IEEE 1800-2017 11.8.1 applies to every comparison, also to one, which is an operand of an equality, and a
// boolean compared with a comparison stays a boolean.
BOOST_AUTO_TEST_CASE(sv_signedness_nested_comparisons) {
  sv_signedness_guard guard(true);
  Variable<bool> f;
  Variable<unsigned> x;
  Variable<short> s;
  Variable<unsigned short> u;

  Generator gen(f == (x > 3u));
  gen((x > 3u) != !f);
  for (int i = 0; i < 20; ++i) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_EQUAL(gen[f], gen[x] > 3u);
  }

  // -1 is converted to 65535, as u is unsigned
  BOOST_REQUIRE(!satisfiable(s == -1 && u == 5u && (s < u)));
  BOOST_REQUIRE(!satisfiable(s == -1 && u == 5u && ((s < u) != (u < u))));
  BOOST_REQUIRE(!satisfiable(s == -1 && u == 5u && f && ((s < u) == f)));
  BOOST_REQUIRE(satisfiable(s == -1 && u == 5u && ((s > u) == (u > 4u))));

  Evaluator eval;
  eval.assign(s, static_cast<short>(-1));
  eval.assign(u, static_cast<unsigned short>(5));
  eval.assign(f, true);
  BOOST_REQUIRE(eval.evaluate((s < u) == f));
  BOOST_REQUIRE(!eval.result<bool>());
  BOOST_REQUIRE(eval.evaluate((s < u) != (u < u)));
  BOOST_REQUIRE(!eval.result<bool>());
  BOOST_REQUIRE(eval.evaluate((s > u) == (u > 4u)));
  BOOST_REQUIRE(eval.result<bool>());
}

// IEEE 1800-2017 11.6: the operands of an expression are extended to the width of the whole expression before
// the operations, and an unsized literal, like an int constant, has 32 bits.
BOOST_AUTO_TEST_CASE(sv_expression_widths) {
  sv_signedness_guard guard(true);
  Variable<signed char> c;
  Variable<unsigned char> a, b;
  Variable<short> s;

  // c is zero-extended to 32 bits before ~
  BOOST_REQUIRE(!satisfiable(c == -1 && ~c + 0u == 0u));
  BOOST_REQUIRE(satisfiable(c == -1 && ~c + 0u == 0xFFFFFF00u));
  // the sum has 32 bits in the context of an int, and 8 bits in the context of an unsigned char
  BOOST_REQUIRE(satisfiable(a == 200 && b == 200 && a + b == 400));
  BOOST_REQUIRE(satisfiable(a == 200 && b == 200 && a + b == static_cast<unsigned char>(144)));
  BOOST_REQUIRE(!satisfiable(a == 200 && b == 200 && a + b == 144));
  // a signed operand is sign-extended in a signed context
  BOOST_REQUIRE(satisfiable(c == -128 && c - 1 < 0));
  BOOST_REQUIRE(satisfiable(c == -128 && c - 1 == -129));
  // a shift has the width of its context, the shift amount is self-determined
  BOOST_REQUIRE(satisfiable(a == 255 && (a << 4) >> 4 == 255));
  BOOST_REQUIRE(!satisfiable(a == 255 && (a << 4) >> 4 == a));
  BOOST_REQUIRE(satisfiable(a == 255 && s == 4 && (a << s) == 0xFF0));
  BOOST_REQUIRE(satisfiable(a == 1 && s == 8 && (a << s) == a - a));
  // a comparison is a 1-bit value, its operands are context-determined among themselves
  BOOST_REQUIRE(satisfiable(a == 255 && b == 1 && (a + b > 255) + 0u == 1u));
  BOOST_REQUIRE(satisfiable(a == 255 && b == 1 && (a + b > static_cast<unsigned char>(254)) + 0u == 0u));
  // the condition of if_then_else is self-determined, its branches are context-determined
  BOOST_REQUIRE(satisfiable(a == 255 && if_then_else(a + 1u > 255u, a, b) + 1 == 256));

  Evaluator eval;
  eval.assign(c, static_cast<signed char>(-1));
  eval.assign(a, static_cast<unsigned char>(200));
  eval.assign(b, static_cast<unsigned char>(200));
  BOOST_REQUIRE(eval.evaluate(~c + 0u == 0u));
  BOOST_REQUIRE(!eval.result<bool>());
  BOOST_REQUIRE(eval.evaluate(~c + 0u));
  BOOST_REQUIRE_EQUAL(eval.result<unsigned>(), 0xFFFFFF00u);
  BOOST_REQUIRE(eval.evaluate(a + b));
  BOOST_REQUIRE_EQUAL(eval.result<unsigned>(), 144u);
  BOOST_REQUIRE(eval.evaluate(a + b + 0));
  BOOST_REQUIRE_EQUAL(eval.result<int>(), 400);
}

namespace {
// A model of the rules of IEEE 1800-2017 11.6 and 11.8 for the widths and the types of expressions, which
// computes the values of an expression of the operands of the test, independent of crave.
namespace sv_model {
enum Op { Var, Const, Slice, Neg, Compl, Add, Sub, Mul, Div, Mod, And, Or, Xor, Shl, Shr, Ite,
          Lt, Le, Gt, Ge, Eq, Ne, LAnd, LOr, LNot };

struct E;
typedef std::shared_ptr<E> P;
struct E {
  Op op;
  unsigned w;    // the width of a variable or a constant
  bool s;        // the signedness of a variable or a constant
  uint64_t v;    // the value of a constant, the index of a variable
  unsigned r, l; // the range of a bit slice
  P a, b, c;
};

uint64_t mask(unsigned w) { return w >= 64 ? ~uint64_t(0) : (uint64_t(1) << w) - 1; }
uint64_t sext(uint64_t v, unsigned w) {
  v &= mask(w);
  return (w < 64 && ((v >> (w - 1)) & 1)) ? v | ~mask(w) : v;
}

bool isBool(E const& e) { return e.op >= Lt; }

unsigned width(E const& e) {
  switch (e.op) {
    case Var: case Const: return e.w;
    case Slice: return e.r - e.l + 1;
    case Neg: case Compl: case Shl: case Shr: return width(*e.a);
    case Ite: return std::max(width(*e.b), width(*e.c));
    case Add: case Sub: case Mul: case Div: case Mod: case And: case Or: case Xor:
      return std::max(width(*e.a), width(*e.b));
    default: return 1;
  }
}

bool sign(E const& e) {
  switch (e.op) {
    case Var: case Const: return e.s;
    case Neg: case Compl: case Shl: case Shr: return sign(*e.a);
    case Ite: return sign(*e.b) && sign(*e.c);
    case Add: case Sub: case Mul: case Div: case Mod: case And: case Or: case Xor: return sign(*e.a) && sign(*e.b);
    default: return false;
  }
}

struct Values { uint64_t var[4]; };

bool evalBool(E const& e, Values const& x);

// the value of e in a context of width w and signedness s, in w bits
uint64_t eval(E const& e, unsigned w, bool s, Values const& x) {
  uint64_t const m = mask(w);
  if (isBool(e)) return evalBool(e, x) ? 1 : 0;
  switch (e.op) {
    case Var: case Const: {
      uint64_t const v = e.op == Var ? x.var[e.v] : e.v;
      return (s ? sext(v, e.w) : (v & mask(e.w))) & m;
    }
    case Slice: return (x.var[e.v] >> e.l) & mask(e.r - e.l + 1);
    case Neg: return (uint64_t(0) - eval(*e.a, w, s, x)) & m;
    case Compl: return ~eval(*e.a, w, s, x) & m;
    case Shl: case Shr: {
      uint64_t const v = eval(*e.a, w, s, x);
      uint64_t const amount = eval(*e.b, width(*e.b), sign(*e.b), x) & mask(width(*e.b));
      if (amount >= w) return 0;
      return (e.op == Shl ? v << amount : v >> amount) & m;
    }
    case Ite: return evalBool(*e.a, x) ? eval(*e.b, w, s, x) : eval(*e.c, w, s, x);
    default: break;
  }
  uint64_t const p = eval(*e.a, w, s, x);
  uint64_t const q = eval(*e.b, w, s, x);
  switch (e.op) {
    case Add: return (p + q) & m;
    case Sub: return (p - q) & m;
    case Mul: return (p * q) & m;
    case And: return p & q;
    case Or: return p | q;
    case Xor: return p ^ q;
    case Div: case Mod: {
      if (!s) return (e.op == Div ? p / q : p % q) & m;
      int64_t const i = static_cast<int64_t>(sext(p, w));
      int64_t const j = static_cast<int64_t>(sext(q, w));
      if (j == -1) return e.op == Div ? (uint64_t(0) - p) & m : 0;
      return static_cast<uint64_t>(e.op == Div ? i / j : i % j) & m;
    }
    default: throw std::logic_error("not an integer operator");
  }
}

bool evalBool(E const& e, Values const& x) {
  switch (e.op) {
    case LAnd: return evalBool(*e.a, x) && evalBool(*e.b, x);
    case LOr: return evalBool(*e.a, x) || evalBool(*e.b, x);
    case LNot: return !evalBool(*e.a, x);
    default: break;
  }
  if (isBool(*e.a) && isBool(*e.b)) {
    bool const p = evalBool(*e.a, x), q = evalBool(*e.b, x);
    return e.op == Eq ? p == q : p != q;
  }
  unsigned const w = std::max(width(*e.a), width(*e.b));
  bool const s = sign(*e.a) && sign(*e.b);
  uint64_t const p = eval(*e.a, w, s, x), q = eval(*e.b, w, s, x);
  bool less, equal = p == q;
  if (s)
    less = static_cast<int64_t>(sext(p, w)) < static_cast<int64_t>(sext(q, w));
  else
    less = p < q;
  switch (e.op) {
    case Lt: return less;
    case Le: return less || equal;
    case Gt: return !less && !equal;
    case Ge: return !less;
    case Eq: return equal;
    case Ne: return !equal;
    default: throw std::logic_error("not a boolean operator");
  }
}

// the operands: signed char, unsigned char, short and unsigned short
unsigned const varWidth[4] = {8, 8, 16, 16};
bool const varSign[4] = {true, false, true, false};

struct Builder {
  explicit Builder(unsigned seed) : rng(seed) {}
  std::mt19937 rng;
  unsigned pick(unsigned n) { return rng() % n; }

  P node(Op op, P a = P(), P b = P(), P c = P()) {
    P e(new E());
    e->op = op;
    e->a = a;
    e->b = b;
    e->c = c;
    return e;
  }

  P leaf(Op op, uint64_t v, unsigned w = 0, bool s = false) {
    P e = node(op);
    e->v = v;
    e->w = op == Var ? varWidth[v] : w;
    e->s = op == Var ? varSign[v] : s;
    return e;
  }

  P var() {
    P e = node(Var);
    e->v = pick(4);
    e->w = varWidth[e->v];
    e->s = varSign[e->v];
    return e;
  }

  // a constant of one of the integer types, which is not zero if nonZero is set
  P constant(bool nonZero = false) {
    static unsigned const widths[] = {8, 16, 32, 64};
    P e = node(Const);
    e->w = widths[pick(10) < 7 ? 2 : pick(4)];
    e->s = pick(2) != 0;
    uint64_t v;
    switch (pick(4)) {
      case 0: v = pick(10); break;
      case 1: v = uint64_t(0) - 1 - pick(3); break;
      case 2: v = uint64_t(1) << (e->w - 1); break;
      default: v = (uint64_t(rng()) << 32) | rng(); break;
    }
    e->v = v & mask(e->w);
    if (nonZero && e->v == 0) e->v = 3;
    return e;
  }

  P integer(int depth) {
    if (depth <= 0) {
      unsigned const k = pick(10);
      if (k < 5) return var();
      if (k < 9) return constant();
      P e = var();
      e->op = Slice;
      e->l = pick(e->w);
      e->r = e->l + pick(e->w - e->l);
      return e;
    }
    switch (pick(12)) {
      case 0: return node(pick(2) ? Neg : Compl, integer(depth - 1));
      case 1: return node(Add, integer(depth - 1), integer(depth - 1));
      case 2: return node(Sub, integer(depth - 1), integer(depth - 1));
      case 3: return node(Mul, integer(depth - 1), constant());
      case 4: return node(pick(2) ? Div : Mod, integer(depth - 1), constant(true));
      case 5: return node(And, integer(depth - 1), integer(depth - 1));
      case 6: return node(Or, integer(depth - 1), integer(depth - 1));
      case 7: return node(Xor, integer(depth - 1), integer(depth - 1));
      case 8: return node(pick(2) ? Shl : Shr, integer(depth - 1), pick(2) ? var() : constant());
      case 9: return node(Ite, boolean(depth - 1), integer(depth - 1), integer(depth - 1));
      case 10: return boolean(depth - 1);
      default: return integer(depth - 1);
    }
  }

  P boolean(int depth) {
    unsigned const k = depth <= 0 ? 0 : pick(8);
    if (k == 6) return node(pick(2) ? LAnd : LOr, boolean(depth - 1), boolean(depth - 1));
    if (k == 7) return node(LNot, boolean(depth - 1));
    static Op const cmp[] = {Lt, Le, Gt, Ge, Eq, Ne};
    return node(cmp[pick(6)], integer(depth), integer(depth));
  }
};

template <typename T>
expression constantExpr(uint64_t v) {
  return value_to_expression(static_cast<T>(v));
}

struct Crave {
  Variable<signed char> sc;
  Variable<unsigned char> uc;
  Variable<short> ss;
  Variable<unsigned short> us;

  expression build(E const& e) {
    switch (e.op) {
      case Var:
        switch (e.v) {
          case 0: return make_expression(sc);
          case 1: return make_expression(uc);
          case 2: return make_expression(ss);
          default: return make_expression(us);
        }
      case Slice:
        switch (e.v) {
          case 0: return make_expression(bitslice(e.r, e.l, sc));
          case 1: return make_expression(bitslice(e.r, e.l, uc));
          case 2: return make_expression(bitslice(e.r, e.l, ss));
          default: return make_expression(bitslice(e.r, e.l, us));
        }
      case Const:
        switch (e.w) {
          case 8: return e.s ? constantExpr<signed char>(e.v) : constantExpr<unsigned char>(e.v);
          case 16: return e.s ? constantExpr<short>(e.v) : constantExpr<unsigned short>(e.v);
          case 32: return e.s ? constantExpr<int>(e.v) : constantExpr<unsigned>(e.v);
          default: return e.s ? constantExpr<long long>(e.v) : constantExpr<unsigned long long>(e.v);
        }
      case Neg: return make_expression(-build(*e.a));
      case Compl: return make_expression(~build(*e.a));
      case LNot: return make_expression(!build(*e.a));
      case Ite: return make_expression(if_then_else(build(*e.a), build(*e.b), build(*e.c)));
      default: break;
    }
    expression const p = build(*e.a);
    expression const q = build(*e.b);
    switch (e.op) {
      case Add: return make_expression(p + q);
      case Sub: return make_expression(p - q);
      case Mul: return make_expression(p * q);
      case Div: return make_expression(p / q);
      case Mod: return make_expression(p % q);
      case And: return make_expression(p & q);
      case Or: return make_expression(p | q);
      case Xor: return make_expression(p ^ q);
      case Shl: return make_expression(p << q);
      case Shr: return make_expression(p >> q);
      case Lt: return make_expression(p < q);
      case Le: return make_expression(p <= q);
      case Gt: return make_expression(p > q);
      case Ge: return make_expression(p >= q);
      case Eq: return make_expression(p == q);
      case Ne: return make_expression(p != q);
      case LAnd: return make_expression(p && q);
      default: return make_expression(p || q);
    }
  }

  void assign(Evaluator& eval, Values const& x) {
    eval.assign(sc, static_cast<signed char>(x.var[0]));
    eval.assign(uc, static_cast<unsigned char>(x.var[1]));
    eval.assign(ss, static_cast<short>(x.var[2]));
    eval.assign(us, static_cast<unsigned short>(x.var[3]));
  }
};

// the values of the operands, which are raw bits of their widths
Values values(unsigned i8, unsigned i16) {
  static uint64_t const v16[] = {0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x00FF, 0x0100, 0xFF00, 0x1234, 0xFEDC, 0x0008};
  Values x;
  x.var[0] = i8 & 0xFF;
  x.var[1] = (i8 * 167 + 13) & 0xFF;
  x.var[2] = v16[i16 % 11];
  x.var[3] = v16[(i16 * 7 + 3) % 11];
  return x;
}
}  // namespace sv_model
}  // namespace

// The evaluator and the solver agree with a model of the rules of SystemVerilog for the widths and the types
// of expressions, for many random expressions over 8-bit and 16-bit operands.
BOOST_AUTO_TEST_CASE(sv_expression_widths_model) {
  using namespace sv_model;
  sv_signedness_guard guard(true);
  Builder builder(20261117);
  Crave crv;

  for (unsigned n = 0; n < 120; ++n) {
    P const b = builder.boolean(1 + n % 3);
    P const i = builder.integer(1 + n % 3);
    expression const be = crv.build(*b);
    expression const ie = crv.build(*i);
    unsigned const iw = width(*i);
    bool const is = sign(*i);

    // the evaluator, for every value of the 8-bit operands
    Evaluator ev;
    for (unsigned v8 = 0; v8 < 256; ++v8) {
      for (unsigned v16 = 0; v16 < 11; v16 += 2) {
        Values const x = values(v8, v16);
        crv.assign(ev, x);
        BOOST_REQUIRE(ev.evaluate(be));
        if (ev.result<bool>() != evalBool(*b, x))
          BOOST_FAIL("boolean expression " << n << " disagrees for " << v8 << ", " << v16);
        if (!isBool(*i)) {
          BOOST_REQUIRE(ev.evaluate(ie));
          uint64_t const expected = is ? sext(eval(*i, iw, is, x), iw) : eval(*i, iw, is, x);
          if (ev.result<unsigned long long>() != expected)
            BOOST_FAIL("integer expression " << n << " disagrees for " << v8 << ", " << v16 << ": "
                                             << ev.result<unsigned long long>() << " != " << expected);
        }
      }
    }

    // the solver, which finds the value of the boolean expression for given values of the operands
    signed char sc;
    unsigned char uc;
    short ss;
    unsigned short us;
    Variable<bool> r;
    Generator gen;
    gen(crv.sc == reference(sc))(crv.uc == reference(uc))(crv.ss == reference(ss))(crv.us == reference(us));
    gen(r == be);
    for (unsigned k = 0; k < 24; ++k) {
      unsigned const v8 = (k * 89 + n) & 0xFF;
      Values const x = values(v8, k + n);
      sc = static_cast<signed char>(x.var[0]);
      uc = static_cast<unsigned char>(x.var[1]);
      ss = static_cast<short>(x.var[2]);
      us = static_cast<unsigned short>(x.var[3]);
      BOOST_REQUIRE(gen.next());
      if (gen[r] != evalBool(*b, x))
        BOOST_FAIL("the solver disagrees for boolean expression " << n << " for " << v8 << ", " << (k + n));
    }
  }
}

// The evaluator agrees with the model for every value of a 16-bit operand.
BOOST_AUTO_TEST_CASE(sv_expression_widths_16bit) {
  using namespace sv_model;
  sv_signedness_guard guard(true);
  Builder m(0);
  Crave crv;
  // ~ss + 0u == 0u and us + us > (unsigned short)0x7FFF
  P const b = m.node(Eq, m.node(Add, m.node(Compl, m.leaf(Var, 2)), m.leaf(Const, 0, 32, false)),
                     m.leaf(Const, 0, 32, false));
  P const c = m.node(Gt, m.node(Add, m.leaf(Var, 3), m.leaf(Var, 3)), m.leaf(Const, 0x7FFF, 16, false));
  expression const be = crv.build(*b);
  expression const ce = crv.build(*c);
  Evaluator ev;
  unsigned holds = 0;
  for (unsigned v = 0; v < 0x10000; ++v) {
    Values x = values(0, 0);
    x.var[2] = x.var[3] = v;
    crv.assign(ev, x);
    BOOST_REQUIRE(ev.evaluate(be));
    BOOST_REQUIRE_EQUAL(ev.result<bool>(), evalBool(*b, x));
    BOOST_REQUIRE(ev.evaluate(ce));
    BOOST_REQUIRE_EQUAL(ev.result<bool>(), evalBool(*c, x));
    holds += ev.result<bool>();
  }
  // the sum wraps around at 16 bits
  BOOST_REQUIRE_EQUAL(holds, 0x8000u);
}

// IEEE 1800-2017 11.6.1: an integer, which stands for a boolean as an operand of a logical operator, as the
// condition of if_then_else or as a constraint, is self-determined. Its comparison with zero is not a comparison,
// whose operands are context-determined.
BOOST_AUTO_TEST_CASE(sv_integer_as_boolean) {
  sv_signedness_guard guard(true);
  Variable<signed char> c;
  Variable<unsigned char> a, b;
  Variable<int> n, x;

  // c & short(0x7F00) is a signed 16-bit value, c is sign-extended: 0x7F00
  BOOST_REQUIRE(satisfiable(c == -1 && (c & static_cast<short>(0x7F00))));
  BOOST_REQUIRE(satisfiable(c == -1 && if_then_else(c & static_cast<short>(0x7F00), 1, 0) == 1));
  BOOST_REQUIRE(!satisfiable(c == -1 && !(c & static_cast<short>(0x7F00))));
  // a << n has the 8 bits of a
  BOOST_REQUIRE(satisfiable(a == 1 && n == 8 && !(a << n)));
  BOOST_REQUIRE(!satisfiable(a == 1 && n == 8 && (a << n)));
  // (a - b) / c is an unsigned 8-bit value: 1 / 2 is 0
  BOOST_REQUIRE(satisfiable(a == 0 && b == 255 && c == 2 && !((a - b) / c)));
  // a constraint, which is an integer
  Generator gen(x == -3);
  gen(x / 9);
  BOOST_REQUIRE(!gen.next());
  Generator gen2(x == -9);
  gen2(x / 9);
  BOOST_REQUIRE(gen2.next());

  Evaluator eval;
  eval.assign(c, static_cast<signed char>(-1));
  eval.assign(a, static_cast<unsigned char>(1));
  eval.assign(n, 8);
  eval.assign(x, -3);
  BOOST_REQUIRE(eval.evaluate(c & static_cast<short>(0x7F00)));
  BOOST_REQUIRE(eval.result<bool>());
  BOOST_REQUIRE(eval.evaluate(!(a << n)));
  BOOST_REQUIRE(eval.result<bool>());
  BOOST_REQUIRE(eval.evaluate(x / 9));
  BOOST_REQUIRE(!eval.result<bool>());
  BOOST_REQUIRE(eval.evaluate(if_then_else(x / 9, 1, 2) == 2));
  BOOST_REQUIRE(eval.result<bool>());
}

namespace {
namespace sv_cases {
// Expressions with values of their operands and their values by the rules of SystemVerilog. Each case has the
// kind (B for a boolean, also of an integer expression, I for the value of an integer expression), the values of
// the operands v0 to v8 in hexadecimal and the expression in prefix notation. The values were computed by a model
// of IEEE 1800-2017 11.6 and 11.8 and agree with a SystemVerilog simulator.
struct Case {
  char const* input;
  char const* expected;
  bool solve;
};
Case const cases[] = {
#include "test_SvExpressionWidths_cases.inc"
};

struct Operands {
  Variable<signed char> v0;
  Variable<unsigned char> v1;
  Variable<short> v2;
  Variable<unsigned short> v3;
  Variable<int> v4;
  Variable<unsigned> v5;
  Variable<long long> v6;
  Variable<unsigned long long> v7;
  Variable<bool> v8;
};

template <typename T>
expression constant(uint64_t v) {
  return value_to_expression(static_cast<T>(v));
}

template <typename V>
expression slice(V const& v, int r, int l) {
  return make_expression(bitslice(r, l, v));
}

struct Parser {
  Operands& o;
  std::istringstream& is;

  expression var(int i) {
    switch (i) {
      case 0: return make_expression(o.v0);
      case 1: return make_expression(o.v1);
      case 2: return make_expression(o.v2);
      case 3: return make_expression(o.v3);
      case 4: return make_expression(o.v4);
      case 5: return make_expression(o.v5);
      case 6: return make_expression(o.v6);
      case 7: return make_expression(o.v7);
      default: return make_expression(o.v8);
    }
  }

  expression parse() {
    std::string t;
    is >> t;
    if (t[0] == 'v') return var(std::stoi(t.substr(1)));
    if (t.compare(0, 2, "sl") == 0) {
      int i, r, l;
      sscanf(t.c_str(), "sl%d:%d:%d", &i, &r, &l);
      switch (i) {
        case 0: return slice(o.v0, r, l);
        case 1: return slice(o.v1, r, l);
        case 2: return slice(o.v2, r, l);
        case 3: return slice(o.v3, r, l);
        case 4: return slice(o.v4, r, l);
        case 5: return slice(o.v5, r, l);
        case 6: return slice(o.v6, r, l);
        default: return slice(o.v7, r, l);
      }
    }
    if (t[0] == 'k') {
      unsigned w;
      char s;
      unsigned long long v;
      sscanf(t.c_str(), "k%u%c:%llx", &w, &s, &v);
      bool const sign = s == 's';
      switch (w) {
        case 8: return sign ? constant<signed char>(v) : constant<unsigned char>(v);
        case 16: return sign ? constant<short>(v) : constant<unsigned short>(v);
        case 32: return sign ? constant<int>(v) : constant<unsigned>(v);
        default: return sign ? constant<long long>(v) : constant<unsigned long long>(v);
      }
    }
    if (t == "neg") return make_expression(-parse());
    if (t == "~") return make_expression(~parse());
    if (t == "!") return make_expression(!parse());
    if (t == "ite") {
      expression const a = parse();
      expression const b = parse();
      expression const c = parse();
      return make_expression(if_then_else(a, b, c));
    }
    expression const p = parse();
    expression const q = parse();
    if (t == "+") return make_expression(p + q);
    if (t == "-") return make_expression(p - q);
    if (t == "*") return make_expression(p * q);
    if (t == "/") return make_expression(p / q);
    if (t == "%") return make_expression(p % q);
    if (t == "&") return make_expression(p & q);
    if (t == "|") return make_expression(p | q);
    if (t == "^") return make_expression(p ^ q);
    if (t == "<<") return make_expression(p << q);
    if (t == ">>") return make_expression(p >> q);
    if (t == "<") return make_expression(p < q);
    if (t == "<=") return make_expression(p <= q);
    if (t == ">") return make_expression(p > q);
    if (t == ">=") return make_expression(p >= q);
    if (t == "==") return make_expression(p == q);
    if (t == "!=") return make_expression(p != q);
    if (t == "&&") return make_expression(p && q);
    if (t == "||") return make_expression(p || q);
    throw std::runtime_error("unknown operator " + t);
  }
};
}  // namespace sv_cases
}  // namespace

// The evaluator and the solver agree with the values of SystemVerilog for random expressions over operands of 1 to
// 64 bits, also for integers, which stand for booleans as operands of logical operators, conditions of
// if_then_else and constraints.
BOOST_AUTO_TEST_CASE(sv_expression_widths_reference_values) {
  using namespace sv_cases;
  sv_signedness_guard guard(true);
  Operands o;
  for (unsigned n = 0; n < sizeof(cases) / sizeof(cases[0]); ++n) {
    std::istringstream is(cases[n].input);
    std::string kind;
    unsigned long long x[9];
    is >> kind;
    for (int i = 0; i < 9; ++i) is >> std::hex >> x[i];
    Parser parser = {o, is};
    expression const e = parser.parse();

    Evaluator eval;
    eval.assign(o.v0, static_cast<signed char>(x[0]));
    eval.assign(o.v1, static_cast<unsigned char>(x[1]));
    eval.assign(o.v2, static_cast<short>(x[2]));
    eval.assign(o.v3, static_cast<unsigned short>(x[3]));
    eval.assign(o.v4, static_cast<int>(x[4]));
    eval.assign(o.v5, static_cast<unsigned>(x[5]));
    eval.assign(o.v6, static_cast<long long>(x[6]));
    eval.assign(o.v7, static_cast<unsigned long long>(x[7]));
    eval.assign(o.v8, x[8] != 0);
    BOOST_REQUIRE(eval.evaluate(e));
    std::ostringstream value;
    if (kind == "B")
      value << eval.result<bool>();
    else
      value << std::hex << eval.result<unsigned long long>();
    if (value.str() != cases[n].expected)
      BOOST_FAIL("the evaluator gives " << value.str() << " for case " << n << ": " << cases[n].input);

    if (!cases[n].solve) continue;
    Generator gen;
    gen(o.v0 == static_cast<signed char>(x[0]))(o.v1 == static_cast<unsigned char>(x[1]));
    gen(o.v2 == static_cast<short>(x[2]))(o.v3 == static_cast<unsigned short>(x[3]));
    gen(o.v4 == static_cast<int>(x[4]))(o.v5 == static_cast<unsigned>(x[5]));
    gen(o.v6 == static_cast<long long>(x[6]))(o.v7 == static_cast<unsigned long long>(x[7]))(o.v8 == (x[8] != 0));
    gen(e);
    if (gen.next() != (std::string(cases[n].expected) == "1"))
      BOOST_FAIL("the solver disagrees for case " << n << ": " << cases[n].input);
  }
}

BOOST_AUTO_TEST_SUITE_END()  // SvSignedness_t

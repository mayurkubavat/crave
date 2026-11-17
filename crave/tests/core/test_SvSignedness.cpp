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
    // the operation has the width of its widest operand
    BOOST_REQUIRE_EQUAL(eval.result<bool>(), static_cast<signed char>(v - 1) < 0);
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

BOOST_AUTO_TEST_SUITE_END()  // SvSignedness_t

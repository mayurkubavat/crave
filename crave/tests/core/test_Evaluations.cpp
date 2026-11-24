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
#include <crave/ir/UserConstraint.hpp>
#include <crave/ir/UserExpression.hpp>

#include <cstdio>
#include <fstream>
#include <set>
#include <iostream>

// using namespace std;
using namespace crave;

BOOST_FIXTURE_TEST_SUITE(Evaluations_t, Context_Fixture)

BOOST_AUTO_TEST_CASE(logical_not_t1) {
  Variable<unsigned int> a;
  Evaluator evaluator;

  evaluator.assign(a, 0u);

  BOOST_REQUIRE(evaluator.evaluate(!(a != 0)));
  BOOST_REQUIRE(evaluator.result<bool>());

  evaluator.assign(a, 42u);

  BOOST_REQUIRE(evaluator.evaluate(!(a == 0)));
  BOOST_REQUIRE(evaluator.result<bool>());
}

BOOST_AUTO_TEST_CASE(logical_not_t2) {
  Variable<unsigned char> a;
  Variable<unsigned int> b;
  Evaluator eval;
  expression expr = make_expression(if_then_else(!(a % 2 == 0), b > 0 && b <= 50, b > 50 && b <= 100));

  eval.assign(a, 1u);

  BOOST_REQUIRE(!eval.evaluate(expr));

  eval.assign(b, 35u);

  BOOST_REQUIRE(eval.evaluate(expr));
  BOOST_REQUIRE(eval.result<bool>());

  eval.assign(a, 2u);
  eval.assign(b, 75u);

  BOOST_REQUIRE(eval.evaluate(expr));
  BOOST_REQUIRE(eval.result<bool>());

  eval.assign(a, 1u);

  BOOST_REQUIRE(eval.evaluate(expr));
  BOOST_REQUIRE(!eval.result<bool>());
}

BOOST_AUTO_TEST_CASE(logical_and_t1) {
  Variable<bool> a;
  Variable<bool> b;
  Variable<bool> c;
  Evaluator eval;

  eval.assign(a, true);
  eval.assign(b, true);

  BOOST_REQUIRE(eval.evaluate(a && b));
  BOOST_REQUIRE(eval.result<bool>());

  eval.assign(c, false);

  BOOST_REQUIRE(eval.evaluate(c == (a && b)));
  BOOST_REQUIRE(!eval.result<bool>());

  eval.assign(a, false);
  eval.assign(b, false);

  BOOST_REQUIRE(eval.evaluate(c == (a && b)));
  BOOST_REQUIRE(eval.result<bool>());
}

BOOST_AUTO_TEST_CASE(logical_or_t1) {
  Variable<bool> a;
  Variable<bool> b;
  Variable<bool> c;
  Evaluator eval;

  eval.assign(a, false);
  eval.assign(b, false);

  BOOST_REQUIRE(eval.evaluate(a || b));
  BOOST_REQUIRE(!eval.result<bool>());

  eval.assign(c, false);

  BOOST_REQUIRE(eval.evaluate(c == (a || b)));
  BOOST_REQUIRE(eval.result<bool>());

  eval.assign(a, true);
  eval.assign(c, true);

  BOOST_REQUIRE(eval.evaluate(c == (a || b)));
  BOOST_REQUIRE(eval.result<bool>());
}

BOOST_AUTO_TEST_CASE(equal_t1) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Evaluator eval;

  eval.assign(a, 65535);

  BOOST_REQUIRE(eval.evaluate(a));
  BOOST_REQUIRE_EQUAL(eval.result<unsigned int>(), 65535);
  BOOST_REQUIRE(eval.evaluate(a == 65535));
  BOOST_REQUIRE(eval.result<bool>());

  eval.assign(b, 5);

  BOOST_REQUIRE(eval.evaluate(a == b));
  BOOST_REQUIRE(!eval.result<bool>());
}

BOOST_AUTO_TEST_CASE(not_equal_t1) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Evaluator eval;

  eval.assign(a, 25u);

  for (unsigned int i = 0; i < 50u; ++i) {
    eval.assign(b, i);
    BOOST_REQUIRE(eval.evaluate(a != b));

    if (i != 25u)
      BOOST_REQUIRE(eval.result<bool>());
    else
      BOOST_REQUIRE(!eval.result<bool>());
  }
}

BOOST_AUTO_TEST_CASE(less) {
  Variable<unsigned> a;
  Variable<unsigned> b;
  Evaluator eval;

  for (unsigned int i = 0u; i < 50u; ++i) {
    eval.assign(a, i);
    BOOST_REQUIRE(eval.evaluate(a < 50u));
    BOOST_REQUIRE(eval.result<bool>());

    eval.assign(b, i);
    BOOST_REQUIRE(eval.evaluate(b < 50u));
    BOOST_REQUIRE(eval.result<bool>());
  }
}

BOOST_AUTO_TEST_CASE(less_equal) {
  Variable<unsigned> a;
  Variable<unsigned> b;
  Evaluator eval;

  for (unsigned int i = 0u; i <= 50u; ++i) {
    eval.assign(a, i);
    BOOST_REQUIRE(eval.evaluate(a <= 50u));
    BOOST_REQUIRE(eval.result<bool>());

    eval.assign(b, i);
    BOOST_REQUIRE(eval.evaluate(b <= 50u));
    BOOST_REQUIRE(eval.result<bool>());
  }
}

BOOST_AUTO_TEST_CASE(greater) {
  Variable<unsigned> a;
  Variable<unsigned> b;
  Evaluator eval;

  for (int i = 50; i > 0; --i) {
    eval.assign(a, i);
    BOOST_REQUIRE(eval.evaluate(a > 0u));
    BOOST_REQUIRE(eval.result<bool>());

    eval.assign(b, i);
    BOOST_REQUIRE(eval.evaluate(b > 0u));
    BOOST_REQUIRE(eval.result<bool>());
  }
}

BOOST_AUTO_TEST_CASE(greater_equal) {
  Variable<unsigned> a;
  Variable<unsigned> b;
  Evaluator eval;

  for (int i = 50; i >= 0; --i) {
    eval.assign(a, i);
    BOOST_REQUIRE(eval.evaluate(a >= 0u));
    BOOST_REQUIRE(eval.result<bool>());

    eval.assign(b, i);
    BOOST_REQUIRE(eval.evaluate(b >= 0u));
    BOOST_REQUIRE(eval.result<bool>());
  }
}

BOOST_AUTO_TEST_CASE(neg_t1) {
  Variable<int> a;
  Variable<int> b;
  Evaluator eval;

  eval.assign(a, 1337);
  BOOST_REQUIRE(eval.evaluate(-a == 1337));
  BOOST_REQUIRE(!eval.result<bool>());

  eval.assign(b, -1337);
  BOOST_REQUIRE(eval.evaluate(a == -b));
  BOOST_REQUIRE(eval.result<bool>());
}

BOOST_AUTO_TEST_CASE(neg_t2) {
  int a = 1337;
  Variable<int> b;
  Evaluator eval;

  eval.assign(b, -a);
  BOOST_REQUIRE(eval.evaluate(b));
  BOOST_REQUIRE_EQUAL(eval.result<int>(), -1337);
}

BOOST_AUTO_TEST_CASE(complement_t1) {
  Variable<int> a;
  Variable<int> b;
  Evaluator eval;

  eval.assign(a, 0);
  eval.assign(b, -1);
  BOOST_REQUIRE(eval.evaluate(~a == b));
  BOOST_REQUIRE(eval.result<bool>());
}

BOOST_AUTO_TEST_CASE(complement_t2) {
  int a = 42;
  Variable<int> b;
  Evaluator eval;
  eval.assign(b, a);
  BOOST_REQUIRE(eval.evaluate(~b));
  BOOST_REQUIRE_EQUAL(eval.result<int>(), -43);
}

BOOST_AUTO_TEST_CASE(bitwise_and_t1) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Evaluator eval;

  eval.assign(a, 42);
  eval.assign(b, 1337);

  BOOST_REQUIRE(eval.evaluate(a & b));
  BOOST_REQUIRE_EQUAL(eval.result<unsigned int>(), 40);
}

BOOST_AUTO_TEST_CASE(bitwise_or_t1) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Evaluator eval;

  eval.assign(a, 42);
  eval.assign(b, 1337);

  BOOST_REQUIRE(eval.evaluate(a | b));
  BOOST_REQUIRE_EQUAL(eval.result<unsigned int>(), 1339);
}

BOOST_AUTO_TEST_CASE(xor_t1) {
  Variable<bool> a;
  Variable<bool> b;
  Evaluator eval;

  eval.assign(a, false);
  eval.assign(b, false);

  BOOST_REQUIRE(eval.evaluate(a ^ b));
  BOOST_REQUIRE_EQUAL(eval.result<bool>(), false);

  eval.assign(b, true);

  BOOST_REQUIRE(eval.evaluate(a ^ b));
  BOOST_REQUIRE_EQUAL(eval.result<bool>(), true);
}

BOOST_AUTO_TEST_CASE(xor_t2) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Evaluator eval;

  eval.assign(a, 65535);
  eval.assign(b, 4080);

  BOOST_REQUIRE(eval.evaluate(a ^ b));
  BOOST_REQUIRE_EQUAL(eval.result<unsigned int>(), 61455);
}

BOOST_AUTO_TEST_CASE(shiftleft) {
  Variable<unsigned> a;
  Variable<char> b;
  Evaluator eval;

  unsigned count = 0;
  while (++count < 256) {
    eval.assign(a, count);
    eval.assign(b, count % (sizeof(unsigned) << 3u));

    BOOST_REQUIRE(eval.evaluate(a << b));
    BOOST_REQUIRE_EQUAL(eval.result<unsigned>(), count << (count % (sizeof(unsigned) << 3u)));
  }
}

BOOST_AUTO_TEST_CASE(shiftright) {
  Variable<unsigned> a;
  Variable<char> b;
  Evaluator eval;

  int count = 0;
  while (256 > ++count) {
    eval.assign(a, count + 256);
    eval.assign(b, count % 8);

    BOOST_REQUIRE(eval.evaluate(a >> b));
    BOOST_REQUIRE_EQUAL(eval.result<unsigned>(), (count + 256) >> (count % 8));
  }
}

BOOST_AUTO_TEST_CASE(plus_minus) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Evaluator eval;

  unsigned int cnt = 0;
  while (cnt++ < 300) {
    eval.assign(a, cnt * cnt);
    eval.assign(b, cnt + cnt);

    BOOST_REQUIRE(eval.evaluate(a + b));
    BOOST_REQUIRE_EQUAL(eval.result<unsigned>(), (cnt * cnt) + (cnt + cnt));

    BOOST_REQUIRE(eval.evaluate(a - b));
    BOOST_REQUIRE_EQUAL(eval.result<unsigned>(), (cnt * cnt) - (cnt + cnt));
  }
}

BOOST_AUTO_TEST_CASE(mult_mod) {
  Variable<int> a;
  Variable<int> b;
  Evaluator eval;

  for (int i = -3; i <= 3; i++) {
    for (int j = -3; j <= 3; j++) {
      eval.assign(a, i);
      eval.assign(b, j);

      BOOST_REQUIRE(eval.evaluate(a * b % 6));
      BOOST_REQUIRE_EQUAL(eval.result<int>(), i * j % 6);
    }
  }

  eval.assign(b, 0);

  BOOST_REQUIRE(!eval.evaluate(a % b));
}

BOOST_AUTO_TEST_CASE(divide) {
  Variable<short> a;
  Variable<short> b;
  Evaluator eval;

  unsigned int cnt = 1;
  while (cnt++ < 256) {
    eval.assign(a, cnt * cnt);
    eval.assign(b, cnt + cnt);

    BOOST_REQUIRE(eval.evaluate(a / b));
    BOOST_REQUIRE_EQUAL(eval.result<short>(), (cnt * cnt) / (cnt + cnt));

    BOOST_REQUIRE(eval.evaluate(a % b));
    BOOST_REQUIRE_EQUAL(eval.result<short>(), (cnt * cnt) % (cnt + cnt));
  }

  eval.assign(b, 0u);
  BOOST_REQUIRE(!eval.evaluate(a / b));
}

BOOST_AUTO_TEST_CASE(element_inside_set) {
  std::set<unsigned> s;
  s.insert(1);
  s.insert(7);
  s.insert(9);

  Variable<unsigned> x;
  Evaluator eval;

  eval.assign(x, 1);

  BOOST_REQUIRE(eval.evaluate(inside(x, s)));
  BOOST_REQUIRE(eval.result<bool>());

  eval.assign(x, 5);

  BOOST_REQUIRE(eval.evaluate(inside(x, s)));
  BOOST_REQUIRE(!eval.result<bool>());
}

BOOST_AUTO_TEST_CASE(element_inside_vec) {
  std::vector<unsigned> v;
  v.push_back(1);
  v.push_back(7);
  v.push_back(9);

  Variable<unsigned> x;
  Evaluator eval;

  eval.assign(x, 7u);

  BOOST_REQUIRE(eval.evaluate(inside(x, v)));
  BOOST_REQUIRE(eval.result<bool>());

  eval.assign(x, 5u);

  BOOST_REQUIRE(eval.evaluate(inside(x, v)));
  BOOST_REQUIRE(!eval.result<bool>());
}

BOOST_AUTO_TEST_CASE(element_inside_array) {
  unsigned a[3];
  a[0] = 1;
  a[1] = 7;
  a[2] = 9;

  Variable<unsigned> x;
  Evaluator eval;

  eval.assign(x, 9);

  BOOST_REQUIRE(eval.evaluate(inside(x, a)));
  BOOST_REQUIRE(eval.result<bool>());

  eval.assign(x, 5u);

  BOOST_REQUIRE(eval.evaluate(inside(x, a)));
  BOOST_REQUIRE(!eval.result<bool>());
}

BOOST_AUTO_TEST_CASE(element_inside_list) {
  std::list<unsigned> l;
  l.push_back(1);
  l.push_back(7);
  l.push_back(9);

  Variable<unsigned> x;
  Evaluator eval;

  eval.assign(x, 7u);

  BOOST_REQUIRE(eval.evaluate(inside(x, l)));
  BOOST_REQUIRE(eval.result<bool>());

  eval.assign(x, 5u);

  BOOST_REQUIRE(eval.evaluate(inside(x, l)));
  BOOST_REQUIRE(!eval.result<bool>());
}

BOOST_AUTO_TEST_CASE(element_not_inside) {
  Evaluator eval;
  {
    std::set<unsigned> s;
    Variable<unsigned> x;
    eval.assign(x, 1u);

    BOOST_REQUIRE(eval.evaluate(inside(x, s)));
    BOOST_REQUIRE(!eval.result<bool>());

    s.insert(1u);

    BOOST_REQUIRE(eval.evaluate(inside(x, s)));
    BOOST_REQUIRE(eval.result<bool>());
  }
  {
    std::vector<unsigned> v;

    Variable<unsigned> x;
    eval.assign(x, 1u);

    BOOST_REQUIRE(eval.evaluate(inside(x, v)));
    BOOST_REQUIRE(!eval.result<bool>());

    v.push_back(1u);

    BOOST_REQUIRE(eval.evaluate(inside(x, v)));
    BOOST_REQUIRE(eval.result<bool>());
  }
  {
    std::vector<unsigned> l;
    Variable<unsigned> x;
    eval.assign(x, 1u);

    BOOST_REQUIRE(eval.evaluate(inside(x, l)));
    BOOST_REQUIRE(!eval.result<bool>());

    l.push_back(1u);

    BOOST_REQUIRE(eval.evaluate(inside(x, l)));
    BOOST_REQUIRE(eval.result<bool>());
  }
}

BOOST_AUTO_TEST_CASE(if_then_else_t1) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Evaluator eval;
  expression expr = make_expression(if_then_else(a<5, b> 0 && b <= 50, b > 50 && b <= 100));

  for (int i = 0; i < 10; ++i) {
    eval.assign(a, i);
    eval.assign(b, 25);
    BOOST_REQUIRE(eval.evaluate(expr));

    if (i < 5) {
      BOOST_REQUIRE(eval.result<bool>());
    } else {
      BOOST_REQUIRE(!eval.result<bool>());
    }
    eval.assign(b, 75);
    BOOST_REQUIRE(eval.evaluate(expr));

    if (i < 5) {
      BOOST_REQUIRE(!eval.result<bool>());
    } else {
      BOOST_REQUIRE(eval.result<bool>());
    }
  }
}

BOOST_AUTO_TEST_CASE(if_then_t1) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Evaluator eval;
  expression expr = make_expression(if_then(a<5, b> 0 && b <= 100));

  for (int i = 0; i < 10; ++i) {
    eval.assign(a, i);
    eval.assign(b, 25u);
    BOOST_REQUIRE(eval.evaluate(expr));
    BOOST_REQUIRE(eval.result<bool>());

    eval.assign(b, 705u);
    BOOST_REQUIRE(eval.evaluate(expr));

    if (i < 5) {
      BOOST_REQUIRE(!eval.result<bool>());
    } else {
      BOOST_REQUIRE(eval.result<bool>());
    }
  }
}

BOOST_AUTO_TEST_CASE(equal_t2) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Evaluator eval;

  eval.assign(a, 1);
  eval.assign(b, 2);

  BOOST_REQUIRE(eval.evaluate(a == b));
  BOOST_REQUIRE(!eval.result<bool>());
}

BOOST_AUTO_TEST_CASE(equal_t3) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Variable<unsigned int> c;
  Evaluator eval;

  eval.assign(a, 1);
  eval.assign(b, 2);
  eval.assign(c, 3);

  BOOST_REQUIRE(eval.evaluate(a + b == c));
  BOOST_REQUIRE(eval.result<bool>());
}

// The option sv_signedness of the config file selects the signedness rules of SystemVerilog.
BOOST_AUTO_TEST_CASE(sv_signedness_config) {
  bool const old_setting = sv_signedness();
  std::string const old_file = get_config_file_name();
  char const* file = "sv_signedness_test.cfg";
  Variable<signed char> s;
  Evaluator eval;
  eval.assign(s, -1);

  {
    std::ofstream cfg(file);
    cfg << "<crave><sv_signedness>1</sv_signedness></crave>" << std::endl;
  }
  init(file);
  BOOST_REQUIRE(sv_signedness());
  BOOST_REQUIRE(eval.evaluate(s == 255u));
  BOOST_REQUIRE(eval.result<bool>());

  {
    std::ofstream cfg(file);
    cfg << "<crave><sv_signedness>0</sv_signedness></crave>" << std::endl;
  }
  init(file);
  BOOST_REQUIRE(!sv_signedness());
  BOOST_REQUIRE(eval.evaluate(s == 255u));
  BOOST_REQUIRE(!eval.result<bool>());

  // without the option, the setting is kept and saved
  set_sv_signedness(true);
  {
    std::ofstream cfg(file);
    cfg << "<crave></crave>" << std::endl;
  }
  init(file);
  BOOST_REQUIRE(sv_signedness());
  set_sv_signedness(false);
  init(file);
  BOOST_REQUIRE(sv_signedness());

  set_sv_signedness(old_setting);
  set_config_file_name(old_file);
  std::remove(file);
}

BOOST_AUTO_TEST_CASE(inside_negation_and_conditional_eval) {
  bool const old_setting = sv_signedness();
  for (bool sv : {false, true}) {
    set_sv_signedness(sv);
    std::set<unsigned> s{1, 7, 9};
    Variable<unsigned> x;
    Variable<unsigned> y;
    Evaluator eval;

    eval.assign(x, 7u);
    eval.assign(y, 2u);
    BOOST_REQUIRE(eval.evaluate(!inside(x, s)));
    BOOST_REQUIRE(!eval.result<bool>());
    BOOST_REQUIRE(eval.evaluate(if_then(inside(x, s), y == 1u)));
    BOOST_REQUIRE(!eval.result<bool>());

    eval.assign(x, 5u);
    BOOST_REQUIRE(eval.evaluate(!inside(x, s)));
    BOOST_REQUIRE(eval.result<bool>());
    BOOST_REQUIRE(eval.evaluate(if_then(inside(x, s), y == 1u)));
    BOOST_REQUIRE(eval.result<bool>());

    Variable<unsigned> a;
    Variable<unsigned> b;
    eval.assign(a, 5u);
    BOOST_REQUIRE(!eval.evaluate(a == b && b == 5u));
  }
  set_sv_signedness(old_setting);
}

BOOST_AUTO_TEST_CASE(inside_ranges_tolerances_mask_eval) {
  bool const old_setting = sv_signedness();
  for (bool sv : {false, true}) {
    set_sv_signedness(sv);
    Evaluator eval;

    // range(lo, hi) & inverted range & 64-bit bounds
    {
      Variable<unsigned char> x8;
      Variable<int> lo, hi;
      eval.assign(lo, -5);
      eval.assign(hi, 10);
      eval.assign(x8, static_cast<unsigned char>(0));
      BOOST_REQUIRE(eval.evaluate(inside(x8, range(lo, hi))));
      BOOST_REQUIRE(eval.result<bool>());
      eval.assign(x8, static_cast<unsigned char>(11));
      BOOST_REQUIRE(eval.evaluate(inside(x8, range(lo, hi))));
      BOOST_REQUIRE(!eval.result<bool>());
      BOOST_REQUIRE(eval.evaluate(inside(x8, range(20, 10))));
      BOOST_REQUIRE(!eval.result<bool>());

      Variable<uint64_t> u64;
      eval.assign(u64, std::numeric_limits<uint64_t>::max() - 1ULL);
      BOOST_REQUIRE(eval.evaluate(inside(u64, range(std::numeric_limits<uint64_t>::max() - 5ULL,
                                                      std::numeric_limits<uint64_t>::max()))));
      BOOST_REQUIRE(eval.result<bool>());
    }

    // tol_plusminus & tol_pluspct
    {
      Variable<unsigned char> x8;
      eval.assign(x8, static_cast<unsigned char>(0));
      BOOST_REQUIRE(eval.evaluate(inside(x8, tol_plusminus(static_cast<unsigned char>(5), static_cast<unsigned char>(10)))));
      BOOST_REQUIRE(eval.result<bool>());
      eval.assign(x8, static_cast<unsigned char>(255));
      BOOST_REQUIRE(eval.evaluate(inside(x8, tol_plusminus(static_cast<unsigned char>(250), static_cast<unsigned char>(10)))));
      BOOST_REQUIRE(eval.result<bool>());
      BOOST_REQUIRE(eval.evaluate(inside(x8, tol_plusminus(10, -1))));
      BOOST_REQUIRE(!eval.result<bool>());

      eval.assign(x8, static_cast<unsigned char>(14));
      BOOST_REQUIRE(eval.evaluate(inside(x8, tol_pluspct(15, 10))));
      BOOST_REQUIRE(eval.result<bool>());
      eval.assign(x8, static_cast<unsigned char>(100));
      BOOST_REQUIRE(eval.evaluate(inside(x8, tol_pluspct(static_cast<unsigned char>(200), static_cast<unsigned char>(50)))));
      BOOST_REQUIRE(eval.result<bool>());

      Variable<short> s16;
      eval.assign(s16, static_cast<short>(-110));
      BOOST_REQUIRE(eval.evaluate(inside(s16, tol_pluspct(-100, 10))));
      BOOST_REQUIRE(eval.result<bool>());
      BOOST_REQUIRE(eval.evaluate(inside(s16, tol_pluspct(-100, -10))));
      BOOST_REQUIRE(!eval.result<bool>());
    }

    // inside_mask & initializer_list & unsized vs sized vs bare patterns
    {
      Variable<unsigned char> x8;
      eval.assign(x8, static_cast<unsigned char>(0xA7u));
      BOOST_REQUIRE(eval.evaluate(inside(x8, inside_mask("8'hA?"))));
      BOOST_REQUIRE(eval.result<bool>());
      BOOST_REQUIRE(eval.evaluate(inside(x8, {static_cast<unsigned char>(0x10u), static_cast<unsigned char>(0xA7u)})));
      BOOST_REQUIRE(eval.result<bool>());
      eval.assign(x8, static_cast<unsigned char>(0xB7u));
      BOOST_REQUIRE(eval.evaluate(!inside(x8, inside_mask("8'hA?"))));
      BOOST_REQUIRE(eval.result<bool>());

      eval.assign(x8, static_cast<unsigned char>(0xF1u));
      BOOST_REQUIRE(eval.evaluate(inside(x8, inside_mask("'bx1"))));
      BOOST_REQUIRE(eval.result<bool>());
      BOOST_REQUIRE(eval.evaluate(inside(x8, inside_mask("4'bx1"))));
      BOOST_REQUIRE(!eval.result<bool>());
      BOOST_REQUIRE(eval.evaluate(inside(x8, inside_mask("'b01"))));
      BOOST_REQUIRE(!eval.result<bool>());
      eval.assign(x8, static_cast<unsigned char>(0x09u));
      BOOST_REQUIRE(eval.evaluate(inside(x8, inside_mask("10x?"))));
      BOOST_REQUIRE(eval.result<bool>());

      Variable<uint16_t> x16;
      eval.assign(x16, static_cast<uint16_t>(0xABC1u));
      BOOST_REQUIRE(eval.evaluate(inside(x16, inside_mask("'h?1"))));
      BOOST_REQUIRE(eval.result<bool>());
    }

    // Plain scalar literal LHS
    {
      Variable<unsigned> x;
      eval.assign(x, 800u);
      BOOST_REQUIRE(eval.evaluate(inside(1000u, tol_pluspct(x, 25u))));
      BOOST_REQUIRE(eval.result<bool>());
      eval.assign(x, 500u);
      BOOST_REQUIRE(eval.evaluate(inside(1000u, tol_pluspct(x, 25u))));
      BOOST_REQUIRE(!eval.result<bool>());
    }
  }

  // SV ternary operand width promotion under sv_signedness(true)
  set_sv_signedness(true);
  {
    Evaluator eval;
    Variable<uint16_t> x;
    Variable<uint8_t> a, b, tol, pct;
    eval.assign(a, static_cast<uint8_t>(200));
    eval.assign(b, static_cast<uint8_t>(100));
    eval.assign(tol, static_cast<uint8_t>(10));
    eval.assign(pct, static_cast<uint8_t>(10));

    eval.assign(x, static_cast<uint16_t>(100));
    BOOST_REQUIRE(eval.evaluate(inside(x, range(a + b, static_cast<uint16_t>(400)))));
    BOOST_REQUIRE(!eval.result<bool>());
    BOOST_REQUIRE(eval.evaluate(inside(x, tol_plusminus(a + b, tol))));
    BOOST_REQUIRE(!eval.result<bool>());
    BOOST_REQUIRE(eval.evaluate(inside(x, tol_pluspct(a + b, pct))));
    BOOST_REQUIRE(!eval.result<bool>());

    eval.assign(x, static_cast<uint16_t>(350));
    BOOST_REQUIRE(eval.evaluate(inside(x, range(a + b, static_cast<uint16_t>(400)))));
    BOOST_REQUIRE(eval.result<bool>());
    eval.assign(x, static_cast<uint16_t>(305));
    BOOST_REQUIRE(eval.evaluate(inside(x, tol_plusminus(a + b, tol))));
    BOOST_REQUIRE(eval.result<bool>());
    BOOST_REQUIRE(eval.evaluate(inside(x, tol_pluspct(a + b, pct))));
    BOOST_REQUIRE(eval.result<bool>());
  }
  set_sv_signedness(old_setting);
}

BOOST_AUTO_TEST_CASE(sv_signedness_false_evaluator_parity) {
  struct Guard {
    Guard() : prev_sv(sv_signedness()) {}
    ~Guard() {
      set_sv_signedness(prev_sv);
      set_solver_backend("Z3");
    }
    bool prev_sv;
  } guard;

  set_sv_signedness(false);
  Variable<uint8_t> a, b, c;
  Variable<uint16_t> u16;
  Evaluator eval;

  eval.assign(a, uint8_t(255));
  eval.assign(c, uint8_t(0));
  BOOST_REQUIRE(eval.evaluate(iff(~a, c)));
  BOOST_REQUIRE(eval.result<bool>());

  eval.assign(a, uint8_t(200));
  eval.assign(b, uint8_t(56));
  BOOST_REQUIRE(eval.evaluate(iff(a + b, c)));
  BOOST_REQUIRE(eval.result<bool>());

  eval.assign(u16, uint16_t(56));
  BOOST_REQUIRE(eval.evaluate(iff(a + u16, c)));
  BOOST_REQUIRE(!eval.result<bool>());

  eval.assign(a, uint8_t(0xFF));
  BOOST_REQUIRE(eval.evaluate(countones(a) > -1));
  BOOST_REQUIRE(eval.result<bool>());

  eval.assign(a, uint8_t(200));
  eval.assign(u16, uint16_t(56));
  BOOST_REQUIRE(eval.evaluate(onehot(a + u16)));
  BOOST_REQUIRE(eval.result<bool>());

  eval.assign(a, uint8_t(0x50));
  BOOST_REQUIRE(eval.evaluate(clog2(bitslice(7, 4, a))));
  BOOST_REQUIRE_EQUAL(eval.result<uint32_t>(), 3u);

  eval.assign(a, uint8_t(0x0F));
  BOOST_REQUIRE(eval.evaluate(iff(bitslice(7, 4, a), c)));
  BOOST_REQUIRE(eval.result<bool>());

  eval.assign(a, uint8_t(0xF0));
  BOOST_REQUIRE(eval.evaluate(iff(bitslice(7, 4, a), c)));
  BOOST_REQUIRE(!eval.result<bool>());
  BOOST_REQUIRE(eval.evaluate(iff(bitslice(7, 4, a), uint8_t(1))));
  BOOST_REQUIRE(eval.result<bool>());

  // Solver parity across Z3 and Cudd under sv_signedness(false)
  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    Generator g1(a == uint8_t(255) && c == uint8_t(0) && iff(~a, c));
    BOOST_REQUIRE(g1.next());
    Generator g2(a == uint8_t(200) && b == uint8_t(56) && c == uint8_t(0) && iff(a + b, c));
    BOOST_REQUIRE(g2.next());
    Generator g3(a == uint8_t(0xFF) && countones(a) > -1);
    BOOST_REQUIRE(g3.next());
    Generator g4(a == uint8_t(200) && u16 == uint16_t(56) && onehot(a + u16));
    BOOST_REQUIRE(g4.next());
    Generator g5(a == uint8_t(0x50) && clog2(bitslice(7, 4, a)) == 3u);
    BOOST_REQUIRE(g5.next());
    Generator g6(a == uint8_t(0x0F) && c == uint8_t(0) && iff(bitslice(7, 4, a), c));
    BOOST_REQUIRE(g6.next());
  }
}

BOOST_AUTO_TEST_CASE(eval_visitor_bounds_check_and_short_circuit_parity) {
  struct Guard {
    Guard() : prev_sv(sv_signedness()) {}
    ~Guard() { set_sv_signedness(prev_sv); }
    bool prev_sv;
  } guard;

  Variable<uint8_t> x, y, unassigned;
  for (bool sv : {false, true}) {
    set_sv_signedness(sv);
    EvalVisitor::eval_map m;
    m[x.id()] = Constant(5, 8, false);
    m[y.id()] = Constant(1, 8, false);
    EvalVisitor ev(&m);
    ev.set_exact(sv);

    NodePtr vx(new VariableExpr(x.id(), 8, false));
    NodePtr vy(new VariableExpr(y.id(), 8, false));
    NodePtr vu(new VariableExpr(unassigned.id(), 8, false));
    NodePtr guard_fail(new LessOpr(vx, new Constant(4, 8, false)));
    NodePtr guard_pass(new LessOpr(vy, new Constant(4, 8, false)));

    // Direct boolean BoundsCheckExpr: guard fail -> valid=true, res=false across !, == false, ||
    NodePtr bool_bce_fail(new BoundsCheckExpr(new EqualOpr(vx, new Constant(5, 8, false)), guard_fail));
    BOOST_REQUIRE(ev.evaluate(*bool_bce_fail));
    BOOST_REQUIRE(!ev.result().value());

    NodePtr not_bool_bce(new NotOpr(bool_bce_fail));
    BOOST_REQUIRE(ev.evaluate(*not_bool_bce));
    BOOST_REQUIRE(!ev.result().value());

    NodePtr eq_false_bce(new EqualOpr(bool_bce_fail, new Constant(false)));
    BOOST_REQUIRE(ev.evaluate(*eq_false_bce));
    BOOST_REQUIRE(!ev.result().value());

    NodePtr or_bool_bce(new LogicalOrOpr(bool_bce_fail, new EqualOpr(vy, new Constant(1, 8, false))));
    BOOST_REQUIRE(ev.evaluate(*or_bool_bce));
    BOOST_REQUIRE(!ev.result().value());

    // Direct non-boolean BoundsCheckExpr: guard fail -> valid=false; short-circuited in || / if_then_else -> valid=true
    NodePtr int_bce_fail(new BoundsCheckExpr(vx, guard_fail));
    BOOST_REQUIRE(!ev.evaluate(*int_bce_fail));

    NodePtr y_eq_1_or_int_bce(
        new LogicalOrOpr(new EqualOpr(vy, new Constant(1, 8, false)),
                         new EqualOpr(int_bce_fail, new Constant(5, 8, false))));
    BOOST_REQUIRE(ev.evaluate(*y_eq_1_or_int_bce));
    BOOST_REQUIRE(ev.result().value() != 0);

    NodePtr y_eq_0_or_int_bce(
        new LogicalOrOpr(new EqualOpr(vy, new Constant(0, 8, false)),
                         new EqualOpr(int_bce_fail, new Constant(5, 8, false))));
    BOOST_REQUIRE(ev.evaluate(*y_eq_0_or_int_bce));
    BOOST_REQUIRE(!ev.result().value());

    NodePtr ite_skip_oob(new IfThenElse(new EqualOpr(vy, new Constant(1, 8, false)),
                                        new Constant(42, 8, false), int_bce_fail));
    BOOST_REQUIRE(ev.evaluate(*ite_skip_oob));
    BOOST_REQUIRE_EQUAL(ev.result().value(), 42u);

    NodePtr ite_take_oob(new IfThenElse(new EqualOpr(vy, new Constant(0, 8, false)),
                                        new Constant(42, 8, false), int_bce_fail));
    BOOST_REQUIRE(!ev.evaluate(*ite_take_oob));

    // Guard pass -> normal evaluation
    NodePtr bool_bce_pass(new BoundsCheckExpr(new EqualOpr(vy, new Constant(1, 8, false)), guard_pass));
    BOOST_REQUIRE(ev.evaluate(*bool_bce_pass));
    BOOST_REQUIRE(ev.result().value() != 0);
    NodePtr int_bce_pass(new BoundsCheckExpr(vy, guard_pass));
    BOOST_REQUIRE(ev.evaluate(*int_bce_pass));
    BOOST_REQUIRE_EQUAL(ev.result().value(), 1u);

    // Invalid condition in IfThenElse must return valid=false even if fallback branch is valid
    NodePtr invalid_cond_ite(new IfThenElse(new EqualOpr(vu, new Constant(1, 8, false)),
                                            new Constant(10, 8, false), new Constant(20, 8, false)));
    BOOST_REQUIRE(!ev.evaluate(*invalid_cond_ite));

    // A5 & guard=false preservation across /, %, <<, >>, bitslice, onehot, extend, range
    NodePtr div_by_bce(new EqualOpr(new DevideOpr(new Constant(10, 8, false), int_bce_fail), new Constant(2, 8, false)));
    BOOST_REQUIRE(ev.evaluate(*div_by_bce));
    BOOST_REQUIRE(!ev.result().value());
    NodePtr div_or_y(new LogicalOrOpr(div_by_bce, new EqualOpr(vy, new Constant(1, 8, false))));
    BOOST_REQUIRE(ev.evaluate(*div_or_y));
    BOOST_REQUIRE(!ev.result().value());
    NodePtr mod_by_bce(new EqualOpr(new ModuloOpr(new Constant(10, 8, false), int_bce_fail), new Constant(0, 8, false)));
    BOOST_REQUIRE(ev.evaluate(*mod_by_bce));
    BOOST_REQUIRE(!ev.result().value());
    NodePtr shl_bce(new EqualOpr(new ShiftLeftOpr(new Constant(1, 8, false), int_bce_fail), new Constant(1, 8, false)));
    BOOST_REQUIRE(ev.evaluate(*shl_bce));
    BOOST_REQUIRE(!ev.result().value());
    NodePtr shr_bce(new EqualOpr(new ShiftRightOpr(new Constant(4, 8, false), int_bce_fail), new Constant(4, 8, false)));
    BOOST_REQUIRE(ev.evaluate(*shr_bce));
    BOOST_REQUIRE(!ev.result().value());
    NodePtr bs_bce(new EqualOpr(new Bitslice(int_bce_fail, 3, 0), new Constant(0, 4, false)));
    BOOST_REQUIRE(ev.evaluate(*bs_bce));
    BOOST_REQUIRE(!ev.result().value());
    NodePtr oh_bce(new OneHotExpr(int_bce_fail));
    BOOST_REQUIRE(ev.evaluate(*oh_bce));
    BOOST_REQUIRE(!ev.result().value());
    NodePtr ext_bce(new EqualOpr(new ExtendExpression(int_bce_fail, 8), new Constant(0, 16, false)));
    BOOST_REQUIRE(ev.evaluate(*ext_bce));
    BOOST_REQUIRE(!ev.result().value());
    NodePtr rng_bce(new RangeExpr(int_bce_fail, new Constant(0, 8, false), new Constant(10, 8, false)));
    BOOST_REQUIRE(ev.evaluate(*rng_bce));
    BOOST_REQUIRE(!ev.result().value());
    // Non-bool root with guard=false must return evaluate() == false
    NodePtr raw_div_bce(new DevideOpr(new Constant(10, 8, false), int_bce_fail));
    BOOST_REQUIRE(!ev.evaluate(*raw_div_bce));
  }
}

BOOST_AUTO_TEST_SUITE_END()

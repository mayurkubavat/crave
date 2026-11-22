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

#include <set>
#include <iostream>

// using namespace std;
using namespace crave;

BOOST_FIXTURE_TEST_SUITE(Operators_t, Context_Fixture)

BOOST_AUTO_TEST_CASE(logical_not_t1) {
  Variable<unsigned int> a;
  Generator gen;
  gen(!(a != 0));

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(gen[a], 0);
}

BOOST_AUTO_TEST_CASE(logical_not_t2) {
  Variable<unsigned char> a;
  Variable<unsigned int> b;
  Generator gen;

  gen(if_then_else(!(a % 2 == 0), b > 0 && b <= 50, b > 50 && b <= 100));

  BOOST_REQUIRE(gen.next());
  std::cout << "a =" << gen[a] << ", b = " << gen[b] << std::endl;
  if (gen[a] % 2 != 0) {
    BOOST_REQUIRE_GT(gen[b], 0u);
    BOOST_REQUIRE_LE(gen[b], 50u);
  } else {
    BOOST_REQUIRE_GT(gen[b], 50u);
    BOOST_REQUIRE_LE(gen[b], 100u);
  }
}

BOOST_AUTO_TEST_CASE(logical_and_t1) {
  Variable<bool> a;
  Variable<bool> b;
  Variable<bool> c;
  Generator gen(a == true);
  gen(b == true);

  BOOST_REQUIRE(gen.next());
  gen(c == (a && b));

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(gen[c], true);

  Generator gen2(a == true);
  gen2(b == false);

  BOOST_REQUIRE(gen2.next());
  gen2(c == (a && b));

  BOOST_REQUIRE(gen2.next());
  BOOST_REQUIRE_EQUAL(gen2[c], false);
}

BOOST_AUTO_TEST_CASE(logical_or_t1) {
  Variable<bool> a;
  Variable<bool> b;
  Variable<bool> c;
  Generator gen(a == false);
  gen(b == false);

  BOOST_REQUIRE(gen.next());
  gen(c == (a || b));

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(gen[c], false);

  Generator gen2(a == true);
  gen2(b == false);

  BOOST_REQUIRE(gen2.next());
  gen2(c == (a || b));

  BOOST_REQUIRE(gen2.next());
  BOOST_REQUIRE_EQUAL(gen2[c], true);
}

BOOST_AUTO_TEST_CASE(equal_t1) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Generator gen(a == 65535);
  gen(b == a);

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(gen[a], gen[b]);
}

BOOST_AUTO_TEST_CASE(not_equal_t1) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Generator gen(a < 65535);
  gen(b != a);

  for (int i = 0; i < 300; ++i) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_NE(gen[a], gen[b]);
  }
}

BOOST_AUTO_TEST_CASE(less) {
  VariableDefaultSolver::bypass_constraint_analysis = true;

  Variable<unsigned> a;

  Generator gen;
  gen(a < 256u);

  std::set<unsigned> generated;
  for (unsigned iterations = 0; gen.next(); ++iterations) {
    unsigned av = gen[a];
    generated.insert(av);
    gen(a != av);
    BOOST_REQUIRE_LT(iterations, 300);
  }

  BOOST_REQUIRE_EQUAL(generated.size(), 256);

  VariableDefaultSolver::bypass_constraint_analysis = false;
}

BOOST_AUTO_TEST_CASE(less_equal) {
  VariableDefaultSolver::bypass_constraint_analysis = true;

  Variable<unsigned> a;

  Generator gen;
  gen(a <= 256u);

  std::set<unsigned> generated;
  for (unsigned iterations = 0; gen.next(); ++iterations) {
    unsigned av = gen[a];
    generated.insert(av);
    gen(a != av);
    BOOST_REQUIRE_LT(iterations, 300);
  }

  BOOST_REQUIRE_EQUAL(generated.size(), 257);

  VariableDefaultSolver::bypass_constraint_analysis = false;
}

BOOST_AUTO_TEST_CASE(greater) {
  VariableDefaultSolver::bypass_constraint_analysis = true;

  Variable<unsigned> a;

  Generator gen;
  gen(a > (std::numeric_limits<unsigned>::max() - 256));

  std::set<unsigned> generated;
  for (unsigned iterations = 0; gen.next(); ++iterations) {
    unsigned av = gen[a];
    generated.insert(av);
    gen(a != av);
    BOOST_REQUIRE_LT(iterations, 300);
  }

  BOOST_REQUIRE_EQUAL(generated.size(), 256);

  VariableDefaultSolver::bypass_constraint_analysis = false;
}

BOOST_AUTO_TEST_CASE(greater_equal) {
  VariableDefaultSolver::bypass_constraint_analysis = true;

  Variable<unsigned> a;

  Generator gen;
  gen(a >= (std::numeric_limits<unsigned>::max() - 256));

  std::set<unsigned> generated;
  for (unsigned iterations = 0; gen.next(); ++iterations) {
    unsigned av = gen[a];
    generated.insert(av);
    gen(a != av);
    BOOST_REQUIRE_LT(iterations, 300);
  }

  BOOST_REQUIRE_EQUAL(generated.size(), 257);

  VariableDefaultSolver::bypass_constraint_analysis = false;
}

BOOST_AUTO_TEST_CASE(neg_t1) {
  Variable<int> a;
  Variable<int> b;
  Generator gen(a == -1337);

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(gen[a], -1337);
  gen(b == -a);

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(gen[b], 1337);
}

BOOST_AUTO_TEST_CASE(neg_t2) {
  Variable<bool> a;
  Variable<int> b;
  Variable<int> c;
  Variable<int> d;
  Generator gen(dist(a, distribution<bool>::create(0.5)));
  gen(b == 1337 && c == 42);
  gen(if_then_else(a, d == -b, d == -c));

  for (int i = 0; i < 50; ++i) {
    BOOST_REQUIRE(gen.next());
    if (gen[a])
      BOOST_REQUIRE_EQUAL(gen[d], -1337);
    else
      BOOST_REQUIRE_EQUAL(gen[d], -42);
  }
}

BOOST_AUTO_TEST_CASE(complement_t1) {
  Variable<int> a;
  Variable<int> b;
  Generator gen(a == 0);

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(gen[a], 0);
  gen(b == ~a);

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(gen[b], -1);
}

BOOST_AUTO_TEST_CASE(bitwise_and_t1) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Variable<unsigned int> c;
  Generator gen(a == 42);
  gen(b == 1337);

  BOOST_REQUIRE(gen.next());
  gen(c == (a & b));

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(gen[c], 40);
}

BOOST_AUTO_TEST_CASE(bitwise_or_t1) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Variable<unsigned int> c;
  Generator gen(a == 42);
  gen(b == 1337);

  BOOST_REQUIRE(gen.next());
  gen(c == (a | b));

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(gen[c], 1339);
}

BOOST_AUTO_TEST_CASE(bitwise_xor_t1) {
  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Variable<unsigned int> c;
  Generator gen(a == 65535);
  gen(b == 4080);

  BOOST_REQUIRE(gen.next());
  gen(c == (a ^ b));

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(gen[c], 61455);
}

BOOST_AUTO_TEST_CASE(shiftleft) {
  VariableDefaultSolver::bypass_constraint_analysis = true;

  Variable<unsigned> a;
  Variable<unsigned> b;
  Variable<unsigned> c;

  Generator gen;
  gen(a < 256u)(b < (unsigned)(sizeof(unsigned) * 8u))(c == (a << b));

  int count = 0;
  int max_count = 200;
  while (gen.next() && ++count < max_count) {
    unsigned av = gen[a];
    unsigned bv = gen[b];
    unsigned r = av << bv;

    BOOST_REQUIRE_EQUAL(r, gen[c]);

    gen(a != gen[a] || b != gen[b]);
  }

  VariableDefaultSolver::bypass_constraint_analysis = false;
  BOOST_REQUIRE_EQUAL(count, max_count);
}

BOOST_AUTO_TEST_CASE(shiftright) {
  VariableDefaultSolver::bypass_constraint_analysis = true;

  Variable<unsigned> a;
  Variable<unsigned> b;
  Variable<unsigned> c;

  Generator gen;
  gen(a > 256u)(b < 8u)(c == (a >> b));

  int count = 0;
  int max_count = 300;
  while (gen.next() && ++count < max_count) {
    unsigned av = gen[a];
    unsigned bv = gen[b];
    unsigned r = av >> bv;

    BOOST_REQUIRE_EQUAL(r, gen[c]);

    gen(a != gen[a] || b != gen[b]);
  }

  VariableDefaultSolver::bypass_constraint_analysis = false;
  BOOST_REQUIRE_EQUAL(count, max_count);
}

BOOST_AUTO_TEST_CASE(plus_minus) {
  VariableDefaultSolver::bypass_constraint_analysis = true;

  Variable<unsigned int> a;
  Variable<unsigned int> b;
  Variable<unsigned int> q;
  Variable<unsigned int> r;

  Generator gen;
  gen(b != 0u)(b < a)(q == a + b)(r == a - b);

  unsigned int cnt = 0u;
  while (gen.next() && cnt < 300) {
    BOOST_REQUIRE_EQUAL(gen[a] + gen[b], gen[q]);
    BOOST_REQUIRE_EQUAL(gen[a] - gen[b], gen[r]);

    gen(a != gen[a] || b != gen[b]);
    std::cout << "#" << cnt++ << ": result: a=" << gen[a] << ", b=" << gen[b] << ", q=" << gen[q] << ", r=" << gen[r]
              << "\n" << std::endl;
  }

  VariableDefaultSolver::bypass_constraint_analysis = false;
}

BOOST_AUTO_TEST_CASE(mult_mod) {
  Variable<int> a;
  Variable<int> b;

  Generator gen;
  gen(-3 <= a && a <= 3)(-3 <= b && b <= 3)(a * b % 6 == 0);

  int cnt = 0;
  for (int i = -3; i <= 3; i++)
    for (int j = -3; j <= 3; j++)
      if (i * j % 6 == 0) cnt++;

  int cnt1 = 0;
  while (gen.next()) {
    cnt1++;
    BOOST_REQUIRE_EQUAL(gen[a] * gen[b] % 6, 0);
    gen(a != gen[a] || b != gen[b]);
    std::cout << "result: a1=" << gen[a] << ", b1=" << gen[b] << "\n" << std::endl;
    BOOST_REQUIRE_LE(cnt1, cnt);
  }

  BOOST_REQUIRE_EQUAL(cnt, cnt1);
}

BOOST_AUTO_TEST_CASE(divide) {
  VariableDefaultSolver::bypass_constraint_analysis = true;

  Variable<unsigned char> a;
  Variable<unsigned char> b;
  Variable<unsigned char> q;
  Variable<unsigned char> r;

  Generator gen;
  gen(b != (unsigned char)0u)(a < (unsigned char)16u)(b < (unsigned char)16u)(q == a / b)(r == a % b);

  while (gen.next()) {
    BOOST_REQUIRE_EQUAL(gen[a] / gen[b], gen[q]);
    BOOST_REQUIRE_EQUAL(gen[a] % gen[b], gen[r]);

    gen(a != gen[a] || b != gen[b]);
    std::cout << "result: a=" << gen[a] << ", b=" << gen[b] << ", q=" << gen[q] << ", r=" << gen[r] << "\n"
              << std::endl;
  }

  VariableDefaultSolver::bypass_constraint_analysis = false;
}

// The quotient of a signed and an unsigned operand is the one of their values, as in C++ after the promotion
// of both operands to int.
BOOST_AUTO_TEST_CASE(divide_mixed_signedness) {
  Variable<signed char> s;
  Variable<unsigned char> u;
  Variable<short> q;
  Variable<short> s16;
  Variable<short> u16;

  // for all values of both 8-bit operands, the quotients equal the ones of the 16-bit signed operands
  Generator gen1(u != 0u && s16 == s && u16 == u);
  gen1(q == s / u)(q != s16 / u16);
  BOOST_REQUIRE(!gen1.next());
  Generator gen2(s != 0 && s16 == s && u16 == u);
  gen2(q == u / s)(q != u16 / s16);
  BOOST_REQUIRE(!gen2.next());

  int const values[][2] = {{-1, 129}, {1, 128}, {1, 255}, {-1, 255}, {-128, 255}, {-7, 2}, {-128, 1}, {127, 200}};
  for (int const* v : values) {
    Generator gen(s == v[0] && u == v[1]);
    Variable<int> a;
    Variable<int> b;
    gen(a == u / s)(b == s / u);
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_EQUAL(gen[a], v[1] / v[0]);
    BOOST_REQUIRE_EQUAL(gen[b], v[0] / v[1]);
  }

  Variable<int> x;
  Variable<int> xq;
  Variable<long long> l;
  Variable<long long> lq;
  Generator gen(x == -8 && l == -9);
  gen(xq == x / 2u)(lq == l / 2u);
  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(gen[xq], -4);
  BOOST_REQUIRE_EQUAL(gen[lq], -4);
}

// The elements of a vector are divided as scalars of their type.
BOOST_AUTO_TEST_CASE(divide_mixed_signedness_vector) {
  placeholder i;
  __rand_vec<int> v;
  Generator gen;
  // the quotient of an int and an unsigned constant greater than any int is 0, as for long long operands
  gen(v().size() == 4u)(foreach (v(), v()[i] / 3000000000u != 0));
  BOOST_REQUIRE(!gen.next());

  __rand_vec<unsigned char> w;
  Generator gen2;
  gen2(w().size() == 4u)(foreach (w(), w()[i] / (signed char)-1 == -129));
  BOOST_REQUIRE(gen2.next());
  for (unsigned k = 0; k < w.size(); ++k) BOOST_REQUIRE_EQUAL(w[k], 129);
}

BOOST_AUTO_TEST_CASE(element_inside_set) {
  std::set<unsigned> s;
  s.insert(1);
  s.insert(7);
  s.insert(9);

  Variable<unsigned> x;

  Generator gen;
  gen(inside(x, s));

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE(s.find(gen[x]) != s.end());

  unsigned first = gen[x];
  gen(x != first);

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE(s.find(gen[x]) != s.end());

  unsigned second = gen[x];
  BOOST_REQUIRE_NE(first, second);
  gen(x != second);

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE(s.find(gen[x]) != s.end());

  unsigned third = gen[x];
  BOOST_REQUIRE_NE(third, second);
  BOOST_REQUIRE_NE(first, third);
  gen(x != third);

  BOOST_REQUIRE(!gen.next());
}

BOOST_AUTO_TEST_CASE(element_inside_vec) {
  std::vector<unsigned> v;
  v.push_back(1);
  v.push_back(7);
  v.push_back(9);

  Variable<unsigned> x;

  Generator gen;
  gen(inside(x, v));

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE(find(v.begin(), v.end(), gen[x]) != v.end());

  unsigned first = gen[x];
  gen(x != first);

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE(find(v.begin(), v.end(), gen[x]) != v.end());

  unsigned second = gen[x];
  BOOST_REQUIRE_NE(first, second);
  gen(x != second);

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE(find(v.begin(), v.end(), gen[x]) != v.end());

  unsigned third = gen[x];
  BOOST_REQUIRE_NE(third, second);
  BOOST_REQUIRE_NE(first, third);
  gen(x != third);

  BOOST_REQUIRE(!gen.next());
}

BOOST_AUTO_TEST_CASE(element_inside_array) {
  unsigned a[3];
  a[0] = 1;
  a[1] = 7;
  a[2] = 9;

  Variable<unsigned> x;
  Generator gen;
  gen(inside(x, a));

  BOOST_REQUIRE(gen.next());

  unsigned first = gen[x];
  gen(x != first);

  BOOST_REQUIRE(gen.next());

  unsigned second = gen[x];
  BOOST_REQUIRE_NE(first, second);
  gen(x != second);

  BOOST_REQUIRE(gen.next());
  unsigned third = gen[x];
  BOOST_REQUIRE_NE(third, second);
  BOOST_REQUIRE_NE(first, third);
  gen(x != third);

  BOOST_REQUIRE(!gen.next());
}

BOOST_AUTO_TEST_CASE(element_inside_list) {
  std::list<unsigned> l;
  l.push_back(1);
  l.push_back(7);
  l.push_back(9);

  Variable<unsigned> x;

  Generator gen;
  gen(inside(x, l));

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE(find(l.begin(), l.end(), gen[x]) != l.end());

  unsigned first = gen[x];
  gen(x != first);

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE(find(l.begin(), l.end(), gen[x]) != l.end());

  unsigned second = gen[x];
  BOOST_REQUIRE_NE(first, second);
  gen(x != second);

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE(find(l.begin(), l.end(), gen[x]) != l.end());

  unsigned third = gen[x];
  BOOST_REQUIRE_NE(third, second);
  BOOST_REQUIRE_NE(first, third);
  gen(x != third);

  BOOST_REQUIRE(!gen.next());
}

BOOST_AUTO_TEST_CASE(element_not_inside) {
  {
    std::set<unsigned> s;

    Variable<unsigned> x;

    Generator gen;
    gen(inside(x, s));

    BOOST_REQUIRE(!gen.next());
  }
  {
    std::vector<unsigned> v;

    Variable<unsigned> x;

    Generator gen;
    gen(inside(x, v));

    BOOST_REQUIRE(!gen.next());
  }
  {
    std::vector<unsigned> l;

    Variable<unsigned> x;

    Generator gen;
    gen(inside(x, l));

    BOOST_REQUIRE(!gen.next());
  }
}

BOOST_AUTO_TEST_CASE(if_then_else_t1) {
  unsigned int a;
  Variable<unsigned int> b;
  Generator gen;

  gen(if_then_else(reference(a)<5, b> 0 && b <= 50, b > 50 && b <= 100));

  for (a = 0; a < 10; ++a) {
    BOOST_REQUIRE(gen.next());
    std::cout << "a =" << a << ", b = " << gen[b] << std::endl;
    if (a < 5) {
      BOOST_REQUIRE_GT(gen[b], 0);
      BOOST_REQUIRE_LE(gen[b], 50);
    } else {
      BOOST_REQUIRE_GT(gen[b], 50);
      BOOST_REQUIRE_LE(gen[b], 100);
    }
  }
}

BOOST_AUTO_TEST_CASE(if_then_t1) {
  unsigned int a;
  Variable<unsigned int> b;
  Generator gen;

  gen(if_then(reference(a)<5, b> 0 && b <= 100));
  gen(if_then(reference(a) >= 5, b > 100 && b <= 1000));

  for (a = 0; a < 10; ++a) {
    BOOST_REQUIRE(gen.next());
    std::cout << "a =" << a << ", b = " << gen[b] << std::endl;
    if (a < 5) {
      BOOST_REQUIRE_GT(gen[b], 0);
      BOOST_REQUIRE_LE(gen[b], 100);
    } else {
      BOOST_REQUIRE_GT(gen[b], 100);
      BOOST_REQUIRE_LE(gen[b], 1000);
    }
  }
}

BOOST_AUTO_TEST_CASE(if_then_else_int_t1) {
  Variable<unsigned> a;
  Variable<unsigned> b;
  Variable<int> c;
  Generator gen(a < 4);
  gen(if_then(a, b == 1));
  gen(if_then_else(a == 0, b, b + 1u) == 0u);
  gen(if_then_else(c < 0, c, -c) < -3 && c > -10 && c < 10);

  for (int i = 0; i < 20; ++i) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_EQUAL(gen[a], 0u);
    BOOST_REQUIRE_EQUAL(gen[b], 0u);
    BOOST_REQUIRE(gen[c] > 3 || gen[c] < -3);
  }
}

// As in C++, a boolean branch is promoted to an integer if the other branch is an integer.
BOOST_AUTO_TEST_CASE(if_then_else_mixed_branches) {
  for (unsigned av = 0; av < 2; ++av) {
    Variable<bool> f;
    Variable<unsigned> a;
    Variable<unsigned> b;
    Variable<unsigned> x;
    Generator gen(a == av && x < 4);
    gen(if_then_else(a == 0, f, x) == 1u);
    gen(if_then_else(a == 0, b, b == 7));

    for (int i = 0; i < 10; ++i) {
      BOOST_REQUIRE(gen.next());
      if (av == 0) {
        BOOST_REQUIRE(gen[f]);
        BOOST_REQUIRE_NE(gen[b], 0u);
      } else {
        BOOST_REQUIRE_EQUAL(gen[x], 1u);
        BOOST_REQUIRE_EQUAL(gen[b], 7u);
      }
    }
  }
}

// As in C++, a boolean operand of an operation with an integer operand is promoted to an integer.
BOOST_AUTO_TEST_CASE(bool_integer_operands) {
  Variable<bool> f;
  Variable<unsigned> x;
  Variable<unsigned> y;
  Variable<unsigned> z;
  Generator gen(x < 4 && z < 3);
  gen(f == x);
  gen(y == f + 1u);
  gen(z);

  for (int i = 0; i < 20; ++i) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_LT(gen[x], 2u);
    BOOST_REQUIRE_EQUAL(gen[f], gen[x] == 1);
    BOOST_REQUIRE_EQUAL(gen[y], gen[f] ? 2u : 1u);
    BOOST_REQUIRE_NE(gen[z], 0u);
  }
}

// As in C++, a boolean is promoted to an int, also if both operands are booleans.
BOOST_AUTO_TEST_CASE(bool_integer_promotion_width) {
  Variable<bool> f;
  Variable<bool> g;
  Variable<int> x;
  Variable<int> y;
  Variable<int> z;
  Variable<int> w;
  Generator gen(f == true && g == true);
  gen(x == f - 2);
  gen(y == f + g);
  gen(z == -f);
  gen(w == (f & g) + (f ^ g));

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(gen[x], -1);
  BOOST_REQUIRE_EQUAL(gen[y], 2);
  BOOST_REQUIRE_EQUAL(gen[z], -1);
  BOOST_REQUIRE_EQUAL(gen[w], 1);
}

// As in C++, the boolean operands of a relational operator are promoted to integers.
BOOST_AUTO_TEST_CASE(bool_relational_operands) {
  Variable<bool> f;
  Variable<bool> g;
  Variable<unsigned> a;
  Variable<unsigned> b;
  Generator gen((a > 3u) < (b > 3u));
  gen(f <= g)(f != g);
  gen((a > 3u) >= f);
  for (int i = 0; i < 20; ++i) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_LE(gen[a], 3u);
    BOOST_REQUIRE_GT(gen[b], 3u);
    BOOST_REQUIRE(!gen[f]);
    BOOST_REQUIRE(gen[g]);
  }
  Generator gen2(f > g || g >= f);
  BOOST_REQUIRE(gen2.next());
  Generator gen3(f > g && f <= g);
  BOOST_REQUIRE(!gen3.next());
}

BOOST_AUTO_TEST_CASE(bitslice_t) {
  Variable<short> x;

  Generator gen;
  gen(bitslice(10, 3, x) == 0xFF);
  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL((gen[x] >> 3) & 0xFF, 0xFF);

  BOOST_REQUIRE_THROW(gen(bitslice(3, 10, x) == 0xFF), std::runtime_error);
  BOOST_REQUIRE_THROW(gen(bitslice(16, 3, x) == 0xFF), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(inside_negation_soundness) {
  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    Variable<unsigned> x;
    std::set<unsigned> s{5, 6};
    Generator gen(x == 5 && !inside(x, s));
    BOOST_REQUIRE(!gen.next());

    Variable<unsigned> x2;
    std::set<unsigned> s2{1, 2, 3};
    Generator gen2(!inside(x2, s2) && x2 < 5);
    for (int i = 0; i < 30; ++i) {
      BOOST_REQUIRE(gen2.next());
      unsigned const val = gen2[x2];
      BOOST_REQUIRE(val == 0u || val == 4u);
    }

    std::set<unsigned> empty;
    Variable<unsigned> x3;
    Generator gen3(!inside(x3, empty) && x3 == 42);
    BOOST_REQUIRE(gen3.next());
    BOOST_REQUIRE_EQUAL(gen3[x3], 42u);

    std::vector<unsigned> dup{1, 1, 2};
    Variable<unsigned> x4;
    Generator gen4(inside(x4, dup) && x4 == 2);
    BOOST_REQUIRE(gen4.next());
    BOOST_REQUIRE_EQUAL(gen4[x4], 2u);

    Variable<unsigned> x5;
    Generator gen5(!inside(x5, dup) && x5 < 4);
    for (int i = 0; i < 20; ++i) {
      BOOST_REQUIRE(gen5.next());
      unsigned const val = gen5[x5];
      BOOST_REQUIRE(val == 0u || val == 3u);
    }
  }
  set_solver_backend("Z3");
}

BOOST_AUTO_TEST_CASE(inside_conditional_disjunction_bool) {
  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    std::set<unsigned> s{5, 6};
    Variable<unsigned> x, y;
    Generator gen(x == 5 && if_then(inside(x, s), y == 1) && y == 2);
    BOOST_REQUIRE(!gen.next());

    std::set<unsigned> s2{2, 5};
    randv<unsigned> a(NULL), b(NULL);
    Generator gen2;
    gen2(a() < 8)(if_then_else(inside(a(), s2), b() == 10, b() == 20));
    bool hit_in = false, hit_out = false;
    for (int i = 0; i < 60; ++i) {
      BOOST_REQUIRE(gen2.next());
      if (a == 2u || a == 5u) {
        BOOST_REQUIRE_EQUAL(b, 10u);
        hit_in = true;
      } else {
        BOOST_REQUIRE_EQUAL(b, 20u);
        hit_out = true;
      }
    }
    BOOST_REQUIRE(hit_in);
    BOOST_REQUIRE(hit_out);

    Variable<unsigned> d1, d2;
    Generator gen3(d1 == 5 && (inside(d1, s) || d2 == 1) && d2 == 2);
    BOOST_REQUIRE(gen3.next());

    Variable<unsigned> d3, d4;
    Generator gen4(d3 == 4 && (inside(d3, s) || d4 == 1) && d4 == 2);
    BOOST_REQUIRE(!gen4.next());

    Variable<unsigned> d5, d6;
    Generator gen5(d5 == 5 && !(inside(d5, s) || d6 == 1));
    BOOST_REQUIRE(!gen5.next());

    Variable<unsigned> bx;
    Variable<bool> bf;
    Generator gen6(bx == 5 && (inside(bx, s) == bf));
    BOOST_REQUIRE(gen6.next());
    BOOST_REQUIRE(gen6[bf]);

    Variable<unsigned> bx2;
    Variable<bool> bf2;
    Generator gen7(bx2 == 4 && (inside(bx2, s) == bf2));
    BOOST_REQUIRE(gen7.next());
    BOOST_REQUIRE(!gen7[bf2]);
  }
  set_solver_backend("Z3");
}

BOOST_AUTO_TEST_CASE(inside_variadic_and_compound_lhs) {
  struct Guard {
    bool sv;
    Guard() : sv(sv_signedness()) {}
    ~Guard() {
      set_sv_signedness(sv);
      set_solver_backend("Z3");
    }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    // >10 variadic items (proving BOOST_PROTO_MAX_ARITY limit is removed)
    {
      Variable<unsigned> x;
      Generator gen(inside(x, 1u, 3u, 5u, 7u, 9u, 11u, 13u, 15u, 17u, 19u, 21u, 23u, 25u));
      std::set<unsigned> allowed{1u, 3u, 5u, 7u, 9u, 11u, 13u, 15u, 17u, 19u, 21u, 23u, 25u};
      for (int i = 0; i < 25; ++i) {
        BOOST_REQUIRE(gen.next());
        BOOST_REQUIRE(allowed.count(gen[x]) == 1u);
      }
      Generator gen_out(x == 23u && inside(x, 1u, 3u, 5u, 7u, 9u, 11u, 13u, 15u, 17u, 19u, 21u, 23u, 25u));
      BOOST_REQUIRE(gen_out.next());
      BOOST_REQUIRE_EQUAL(gen_out[x], 23u);
    }

    // initializer_list, C-array, std::array, empty container
    {
      Variable<unsigned> x;
      Generator gen(inside(x, {2u, 6u, 14u}));
      for (int i = 0; i < 15; ++i) {
        BOOST_REQUIRE(gen.next());
        unsigned v = gen[x];
        BOOST_REQUIRE(v == 2u || v == 6u || v == 14u);
      }

      unsigned carr[3] = {10u, 20u, 30u};
      Generator gen_carr(inside(x, carr) && x >= 20u);
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(gen_carr.next());
        BOOST_REQUIRE(gen_carr[x] == 20u || gen_carr[x] == 30u);
      }

      std::vector<unsigned> empty_v;
      Generator gen_empty(inside(x, empty_v));
      BOOST_REQUIRE(!gen_empty.next());
      Generator gen_empty_neg(!inside(x, empty_v) && x == 77u);
      BOOST_REQUIRE(gen_empty_neg.next());
      BOOST_REQUIRE_EQUAL(gen_empty_neg[x], 77u);
    }

    // Compound LHS: arithmetic expression, bitslice, ReadReference
    {
      Variable<unsigned> a, b;
      Generator gen(a < 10u && b < 10u && inside(a + b, 5u, 11u));
      for (int i = 0; i < 15; ++i) {
        BOOST_REQUIRE(gen.next());
        unsigned s = gen[a] + gen[b];
        BOOST_REQUIRE(s == 5u || s == 11u);
      }

      Variable<unsigned char> byte_v;
      Generator gen_bs(inside(bitslice(7, 4, byte_v), range(4u, 7u)) && bitslice(3, 0, byte_v) == 0xAu);
      for (int i = 0; i < 15; ++i) {
        BOOST_REQUIRE(gen_bs.next());
        unsigned hi = (gen_bs[byte_v] >> 4) & 0xFu;
        BOOST_REQUIRE(hi >= 4u && hi <= 7u);
        BOOST_REQUIRE_EQUAL(gen_bs[byte_v] & 0xFu, 0xAu);
      }

      unsigned ext = 15u;
      Variable<unsigned> y;
      Generator gen_ref(if_then_else(inside(reference(ext), range(10u, 20u)), y == 1u, y == 2u));
      BOOST_REQUIRE(gen_ref.next());
      BOOST_REQUIRE_EQUAL(gen_ref[y], 1u);
      ext = 25u;
      BOOST_REQUIRE(gen_ref.next());
      BOOST_REQUIRE_EQUAL(gen_ref[y], 2u);
    }

    // Out-of-range constants on uint8_t (must NOT truncate 300 -> 44 or wrap -5 -> 251)
    {
      for (bool sv : {false, true}) {
        set_sv_signedness(sv);
        Variable<unsigned char> x8;
        std::vector<int> vals{300, -5, 7};
        Generator gen(inside(x8, vals));
        for (int i = 0; i < 10; ++i) {
          BOOST_REQUIRE(gen.next());
          BOOST_REQUIRE_EQUAL(static_cast<unsigned>(gen[x8]), 7u);
        }
        Generator gen_unsat(inside(x8, std::vector<int>{300, -5}));
        BOOST_REQUIRE(!gen_unsat.next());
      }
      set_sv_signedness(false);
    }
  }
}

BOOST_AUTO_TEST_CASE(inside_range_symbolic_and_mixed) {
  struct Guard {
    bool sv;
    Guard() : sv(sv_signedness()) {}
    ~Guard() {
      set_sv_signedness(sv);
      set_solver_backend("Z3");
    }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    // CTAD constant & symbolic bounds, inverted range, compound bounds
    {
      Variable<unsigned> x, lo, hi;
      Generator gen(lo >= 5u && lo <= 8u && hi >= 12u && hi <= 15u && inside(x, range(lo + 1u, hi - 1u)));
      for (int i = 0; i < 20; ++i) {
        BOOST_REQUIRE(gen.next());
        BOOST_REQUIRE_GE(gen[x], gen[lo] + 1u);
        BOOST_REQUIRE_LE(gen[x], gen[hi] - 1u);
      }

      Generator gen_inv(inside(x, range(20u, 10u)));
      BOOST_REQUIRE(!gen_inv.next());
      Generator gen_inv_neg(!inside(x, range(20u, 10u)) && x == 5u);
      BOOST_REQUIRE(gen_inv_neg.next());
      BOOST_REQUIRE_EQUAL(gen_inv_neg[x], 5u);
    }

    // Mixed widths & signedness in both sv_signedness modes
    for (bool sv : {false, true}) {
      set_sv_signedness(sv);

      Variable<unsigned char> x8;
      Generator g1(inside(x8, range(-5, 10)));
      for (int i = 0; i < 15; ++i) {
        BOOST_REQUIRE(g1.next());
        BOOST_REQUIRE_LE(static_cast<unsigned>(g1[x8]), 10u);
      }
      Generator g1_neg(x8 == 0 && !inside(x8, range(-5, 10)));
      BOOST_REQUIRE(!g1_neg.next());

      Generator g2(inside(x8, range(300, 400)));
      BOOST_REQUIRE(!g2.next());

      Variable<signed char> s8;
      Generator g3(inside(s8, range(static_cast<uint16_t>(10), static_cast<uint16_t>(20))));
      for (int i = 0; i < 15; ++i) {
        BOOST_REQUIRE(g3.next());
        BOOST_REQUIRE_GE(static_cast<int>(g3[s8]), 10);
        BOOST_REQUIRE_LE(static_cast<int>(g3[s8]), 20);
      }

      // 64-bit boundary ranges under both sv_signedness modes
      Variable<uint64_t> u64;
      uint64_t const u_hi = std::numeric_limits<uint64_t>::max();
      uint64_t const u_lo = u_hi - 5ULL;
      Generator gu(inside(u64, range(u_lo, u_hi)));
      for (int i = 0; i < 5; ++i) {
        BOOST_REQUIRE(gu.next());
        BOOST_REQUIRE_GE(gu[u64], u_lo);
      }

      Variable<int64_t> i64;
      int64_t const s_lo = std::numeric_limits<int64_t>::min();
      int64_t const s_hi = s_lo + 5LL;
      Generator gi(inside(i64, range(s_lo, s_hi)));
      for (int i = 0; i < 5; ++i) {
        BOOST_REQUIRE(gi.next());
        BOOST_REQUIRE_LE(gi[i64], s_hi);
      }
    }
    set_sv_signedness(false);

    // Plain scalar literal LHS
    {
      Variable<unsigned> x;
      Generator g_lit_sat(inside(1000u, range(500u, 1500u)) && x == 1u);
      BOOST_REQUIRE(g_lit_sat.next());
      BOOST_REQUIRE_EQUAL(g_lit_sat[x], 1u);
      Generator g_lit_unsat(inside(1000u, range(100u, 200u)) && x == 1u);
      BOOST_REQUIRE(!g_lit_unsat.next());
    }

    // soft(inside(...)) last-wins priority
    {
      Variable<unsigned> x;
      Generator gen;
      gen.soft(inside(x, range(10u, 20u)));
      gen.soft(inside(x, range(30u, 40u)));
      BOOST_REQUIRE(gen.next());
      BOOST_REQUIRE_GE(gen[x], 30u);
      BOOST_REQUIRE_LE(gen[x], 40u);

      gen(x < 25u);
      BOOST_REQUIRE(gen.next());
      BOOST_REQUIRE_GE(gen[x], 10u);
      BOOST_REQUIRE_LE(gen[x], 20u);
    }
  }
}

BOOST_AUTO_TEST_CASE(inside_tolerances_plusminus_and_pluspct) {
  struct Guard {
    bool sv;
    Guard() : sv(sv_signedness()) {}
    ~Guard() {
      set_sv_signedness(sv);
      set_solver_backend("Z3");
    }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    for (bool sv : {false, true}) {
      set_sv_signedness(sv);

      // tol_plusminus unsigned underflow & overflow
      {
        Variable<unsigned char> x;
        Generator g_under(inside(x, tol_plusminus(static_cast<unsigned char>(5), static_cast<unsigned char>(10))));
        for (int i = 0; i < 15; ++i) {
          BOOST_REQUIRE(g_under.next());
          BOOST_REQUIRE_LE(static_cast<unsigned>(g_under[x]), 15u);
        }
        Generator g_under0(x == 0 && inside(x, tol_plusminus(static_cast<unsigned char>(5), static_cast<unsigned char>(10))));
        BOOST_REQUIRE(g_under0.next());
        Generator g_under16(x == 16 && inside(x, tol_plusminus(static_cast<unsigned char>(5), static_cast<unsigned char>(10))));
        BOOST_REQUIRE(!g_under16.next());

        Generator g_over(inside(x, tol_plusminus(static_cast<unsigned char>(250), static_cast<unsigned char>(10))));
        for (int i = 0; i < 15; ++i) {
          BOOST_REQUIRE(g_over.next());
          BOOST_REQUIRE_GE(static_cast<unsigned>(g_over[x]), 240u);
        }
        Generator g_over255(x == 255 && inside(x, tol_plusminus(static_cast<unsigned char>(250), static_cast<unsigned char>(10))));
        BOOST_REQUIRE(g_over255.next());
        Generator g_over4(x == 4 && inside(x, tol_plusminus(static_cast<unsigned char>(250), static_cast<unsigned char>(10))));
        BOOST_REQUIRE(!g_over4.next());
      }

      // tol_plusminus signed underflow/overflow & negative/zero tol
      {
        Variable<signed char> s8;
        Generator g_sunder(inside(s8, tol_plusminus(static_cast<signed char>(-120), static_cast<signed char>(20))));
        for (int i = 0; i < 10; ++i) {
          BOOST_REQUIRE(g_sunder.next());
          BOOST_REQUIRE_LE(static_cast<int>(g_sunder[s8]), -100);
        }
        Generator g_sover(inside(s8, tol_plusminus(static_cast<signed char>(120), static_cast<signed char>(20))));
        for (int i = 0; i < 10; ++i) {
          BOOST_REQUIRE(g_sover.next());
          BOOST_REQUIRE_GE(static_cast<int>(g_sover[s8]), 100);
        }

        // negative tol -> empty
        Generator g_neg_tol(inside(s8, tol_plusminus(10, -1)));
        BOOST_REQUIRE(!g_neg_tol.next());
        Generator g_neg_tol_n(s8 == 10 && !inside(s8, tol_plusminus(10, -1)));
        BOOST_REQUIRE(g_neg_tol_n.next());

        Variable<signed char> tol_v;
        Generator g_sym_neg(tol_v == -2 && inside(s8, tol_plusminus(10, tol_v)));
        BOOST_REQUIRE(!g_sym_neg.next());

        // tol == 0 -> exact singleton
        Generator g_zero(inside(s8, tol_plusminus(static_cast<signed char>(42), static_cast<signed char>(0))));
        BOOST_REQUIRE(g_zero.next());
        BOOST_REQUIRE_EQUAL(static_cast<int>(g_zero[s8]), 42);

        // mixed signedness: int8_t vs tol_plusminus(uint16_t(10), uint8_t(20)) -> [-10, 30]
        Generator g_mix(inside(s8, tol_plusminus(static_cast<uint16_t>(10), static_cast<unsigned char>(20))) && s8 < 0);
        for (int i = 0; i < 10; ++i) {
          BOOST_REQUIRE(g_mix.next());
          BOOST_REQUIRE_GE(static_cast<int>(g_mix[s8]), -10);
          BOOST_REQUIRE_LT(static_cast<int>(g_mix[s8]), 0);
        }
      }

      // tol_pluspct truncation, mult overflow, negative val/pct, pct==0, pct>100, symbolic
      {
        Variable<unsigned char> x8;
        Generator g_trunc1(inside(x8, tol_pluspct(15, 10)));
        for (int i = 0; i < 10; ++i) {
          BOOST_REQUIRE(g_trunc1.next());
          BOOST_REQUIRE_GE(static_cast<unsigned>(g_trunc1[x8]), 14u);
          BOOST_REQUIRE_LE(static_cast<unsigned>(g_trunc1[x8]), 16u);
        }
        Generator g_trunc2(inside(x8, tol_pluspct(9, 10)));
        BOOST_REQUIRE(g_trunc2.next());
        BOOST_REQUIRE_EQUAL(static_cast<unsigned>(g_trunc2[x8]), 9u);

        // 200u8 * 50u8 = 10000 -> delta 100 -> [100, 255]
        Generator g_mulov(inside(x8, tol_pluspct(static_cast<unsigned char>(200), static_cast<unsigned char>(50))));
        for (int i = 0; i < 15; ++i) {
          BOOST_REQUIRE(g_mulov.next());
          BOOST_REQUIRE_GE(static_cast<unsigned>(g_mulov[x8]), 100u);
        }
        Generator g_mulov16(x8 == 16 && inside(x8, tol_pluspct(static_cast<unsigned char>(200), static_cast<unsigned char>(50))));
        BOOST_REQUIRE(!g_mulov16.next());

        // negative val: -100 +%- 10 -> [-110, -90]
        Variable<short> s16;
        Generator g_negval(inside(s16, tol_pluspct(-100, 10)));
        for (int i = 0; i < 15; ++i) {
          BOOST_REQUIRE(g_negval.next());
          BOOST_REQUIRE_GE(g_negval[s16], -110);
          BOOST_REQUIRE_LE(g_negval[s16], -90);
        }
        Generator g_negpct(inside(s16, tol_pluspct(-100, -10)));
        BOOST_REQUIRE(!g_negpct.next());

        // pct == 0 and pct > 100
        Generator g_p0(inside(x8, tol_pluspct(10, 0)));
        BOOST_REQUIRE(g_p0.next());
        BOOST_REQUIRE_EQUAL(static_cast<unsigned>(g_p0[x8]), 10u);

        Generator g_p150_u(inside(x8, tol_pluspct(10, 150)));
        for (int i = 0; i < 10; ++i) {
          BOOST_REQUIRE(g_p150_u.next());
          BOOST_REQUIRE_LE(static_cast<unsigned>(g_p150_u[x8]), 25u);
        }

        Variable<signed char> s8;
        Generator g_p150_s(inside(s8, tol_pluspct(10, 150)) && s8 < 0);
        for (int i = 0; i < 10; ++i) {
          BOOST_REQUIRE(g_p150_s.next());
          BOOST_REQUIRE_GE(static_cast<int>(g_p150_s[s8]), -5);
          BOOST_REQUIRE_LT(static_cast<int>(g_p150_s[s8]), 0);
        }

        // symbolic val and pct (8-bit, fast on both Z3 and CUDD)
        Variable<unsigned char> val8, pct8;
        Generator g_sym(val8 == 200 && pct8 == 25 && inside(x8, tol_pluspct(val8, pct8)));
        for (int i = 0; i < 10; ++i) {
          BOOST_REQUIRE(g_sym.next());
          BOOST_REQUIRE_GE(static_cast<unsigned>(g_sym[x8]), 150u);
          BOOST_REQUIRE_LE(static_cast<unsigned>(g_sym[x8]), 250u);
        }

        // symbolic negative pct on signed type
        Variable<signed char> pct_s;
        Generator g_sym_negpct(pct_s == -5 && inside(s8, tol_pluspct(static_cast<signed char>(20), pct_s)));
        BOOST_REQUIRE(!g_sym_negpct.next());

        // bounded 8-bit unpinned symbolic tol_pluspct (fast on both Z3 and CUDD)
        Variable<unsigned char> x8_u, val8_u, pct8_u;
        Generator g_unpinned_8(val8_u >= 100 && val8_u <= 104 && inside(pct8_u, 0, 10, 50, 100) &&
                               inside(x8_u, tol_pluspct(val8_u, pct8_u)));
        for (int i = 0; i < 4; ++i) {
          BOOST_REQUIRE(g_unpinned_8.next());
          unsigned val_v = static_cast<unsigned>(g_unpinned_8[val8_u]);
          unsigned pct_v = static_cast<unsigned>(g_unpinned_8[pct8_u]);
          unsigned x_v = static_cast<unsigned>(g_unpinned_8[x8_u]);
          unsigned delta = (val_v * pct_v) / 100u;
          BOOST_REQUIRE(x_v + delta >= val_v && x_v >= val_v - delta && x_v <= val_v + delta);
        }
      }

      // 64-bit tolerance near UINT64_MAX & INT64_MIN + 64-bit constant tol_pluspct under both sv modes
      {
        Variable<uint64_t> u64;
        uint64_t const u_near = std::numeric_limits<uint64_t>::max() - 10ULL;
        Generator gu(inside(u64, tol_plusminus(u_near, 20ULL)));
        for (int i = 0; i < 5; ++i) {
          BOOST_REQUIRE(gu.next());
          BOOST_REQUIRE_GE(gu[u64], u_near - 20ULL);
        }

        Variable<int64_t> i64;
        int64_t const s_near = std::numeric_limits<int64_t>::min() + 10LL;
        Generator gi(inside(i64, tol_plusminus(s_near, 20LL)));
        for (int i = 0; i < 5; ++i) {
          BOOST_REQUIRE(gi.next());
          BOOST_REQUIRE_LE(gi[i64], s_near + 20LL);
        }

        Generator gp0(inside(u64, tol_pluspct(u_near, 0ULL)));
        BOOST_REQUIRE(gp0.next());
        BOOST_REQUIRE_EQUAL(gp0[u64], u_near);

        int64_t const s_half_min = std::numeric_limits<int64_t>::min() / 2;
        Generator gp10(inside(i64, tol_pluspct(s_half_min, 10LL)));
        for (int i = 0; i < 5; ++i) {
          BOOST_REQUIRE(gp10.next());
          int64_t delta = (-(s_half_min / 10));
          BOOST_REQUIRE(gi[i64] <= s_near + 20LL);
          BOOST_REQUIRE(gp10[i64] >= s_half_min - delta && gp10[i64] <= s_half_min + delta);
        }

        int64_t const s_half_max = std::numeric_limits<int64_t>::max() / 2;
        Generator gp50(inside(i64, tol_pluspct(s_half_max, 50LL)));
        for (int i = 0; i < 5; ++i) {
          BOOST_REQUIRE(gp50.next());
          int64_t delta = s_half_max / 2;
          BOOST_REQUIRE(gp50[i64] >= s_half_max - delta && gp50[i64] <= s_half_max + delta);
        }
      }
    }
    set_sv_signedness(false);

    // Plain scalar literal LHS with tol_pluspct
    {
      Variable<unsigned> x;
      Generator g_lit_pct_sat(inside(1000u, tol_pluspct(x, 25u)) && x == 800u);
      BOOST_REQUIRE(g_lit_pct_sat.next());
      BOOST_REQUIRE_EQUAL(g_lit_pct_sat[x], 800u);
      Generator g_lit_pct_unsat(inside(1000u, tol_pluspct(x, 25u)) && x == 500u);
      BOOST_REQUIRE(!g_lit_pct_unsat.next());
    }
  }
}

BOOST_AUTO_TEST_CASE(inside_mask_patterns_and_masks) {
  struct Guard {
    bool sv;
    Guard() : sv(sv_signedness()) {}
    ~Guard() {
      set_sv_signedness(sv);
      set_solver_backend("Z3");
    }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    for (bool sv : {false, true}) {
      set_sv_signedness(sv);
      Variable<unsigned char> x;

      // (val, care_mask) with don't-care bits ignored even when non-zero in val
      Generator g1(inside(x, inside_mask(static_cast<unsigned char>(0xAFu), static_cast<unsigned char>(0xF0u))));
      for (int i = 0; i < 15; ++i) {
        BOOST_REQUIRE(g1.next());
        BOOST_REQUIRE_EQUAL(static_cast<unsigned>(g1[x]) & 0xF0u, 0xA0u);
      }

      // mask == 0 matches all values, mask == 0xFF matches exact
      Generator g_all(inside(x, inside_mask(0x55u, 0u)) && x == 0x12u);
      BOOST_REQUIRE(g_all.next());
      Generator g_exact(inside_mask(x, static_cast<unsigned char>(0x3Cu), static_cast<unsigned char>(0xFFu)));
      BOOST_REQUIRE(g_exact.next());
      BOOST_REQUIRE_EQUAL(static_cast<unsigned>(g_exact[x]), 0x3Cu);

      // symbolic val & mask
      Variable<unsigned char> val, mask;
      Generator g_sym(val == 0xB3u && mask == 0xF0u && inside(x, inside_mask(val, mask)));
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(g_sym.next());
        BOOST_REQUIRE_EQUAL(static_cast<unsigned>(g_sym[x]) & 0xF0u, 0xB0u);
      }

      // SV & bare pattern strings
      Generator g_pat1(inside(x, inside_mask("8'hA?")));
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(g_pat1.next());
        BOOST_REQUIRE_EQUAL(static_cast<unsigned>(g_pat1[x]) & 0xF0u, 0xA0u);
      }

      Generator g_pat2(inside_mask(x, "8'b1100_xz10"));
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(g_pat2.next());
        BOOST_REQUIRE_EQUAL(static_cast<unsigned>(g_pat2[x]) & 0xF3u, 0xC2u);
      }

      // Narrower pattern zero-extends upper care bits per IEEE 1800-2023 11.4.6
      Generator g_narrow(inside(x, inside_mask("4'b10x?")));
      for (int i = 0; i < 15; ++i) {
        BOOST_REQUIRE(g_narrow.next());
        unsigned v = static_cast<unsigned>(g_narrow[x]);
        BOOST_REQUIRE(v >= 0x08u && v <= 0x0Bu);
      }

      // Wider pattern with non-zero upper bits -> UNSAT on uint8_t
      Generator g_wide(inside(x, inside_mask("16'h01A?")));
      BOOST_REQUIRE(!g_wide.next());

      // Sized pattern left-extended with 'x' -> upper bits don't-care
      Generator g_xext(inside(x, inside_mask("8'bx1")));
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(g_xext.next());
        BOOST_REQUIRE_EQUAL(static_cast<unsigned>(g_xext[x]) & 0x01u, 0x01u);
      }

      // Unsized SV leading-wildcard patterns vs sized vs bare patterns
      {
        Generator g_unsized_b(x == 0xF1u && inside(x, inside_mask("'bx1")));
        BOOST_REQUIRE(g_unsized_b.next());
        Generator g_unsized_b_neg(x == 0xF0u && inside(x, inside_mask("'bx1")));
        BOOST_REQUIRE(!g_unsized_b_neg.next());

        Generator g_sized_x_unsat(x == 0xF1u && inside(x, inside_mask("4'bx1")));
        BOOST_REQUIRE(!g_sized_x_unsat.next());
        Generator g_sized_x_sat(x == 0x03u && inside(x, inside_mask("4'bx1")));
        BOOST_REQUIRE(g_sized_x_sat.next());

        Generator g_unsized_0_unsat(x == 0xF1u && inside(x, inside_mask("'b01")));
        BOOST_REQUIRE(!g_unsized_0_unsat.next());
        Generator g_unsized_0_sat(x == 0x01u && inside(x, inside_mask("'b01")));
        BOOST_REQUIRE(g_unsized_0_sat.next());

        Variable<uint16_t> x16;
        Generator g_unsized_h(x16 == 0xABC1u && inside(x16, inside_mask("'h?1")));
        BOOST_REQUIRE(g_unsized_h.next());
        Generator g_unsized_h_neg(x16 == 0xABC0u && inside(x16, inside_mask("'h?1")));
        BOOST_REQUIRE(!g_unsized_h_neg.next());

        Generator g_bare_sat(x == 0x09u && inside(x, inside_mask("10x?")));
        BOOST_REQUIRE(g_bare_sat.next());
        Generator g_bare_unsat(x == 0x19u && inside(x, inside_mask("10x?")));
        BOOST_REQUIRE(!g_bare_unsat.next());
      }

      // Mixed variadic + negation + if_then
      Variable<bool> flag;
      Generator g_mix(inside(x, 0x00u, inside_mask("8'hF?"), range(0x10u, 0x12u)) &&
                      !inside(x, inside_mask("8'hF0")) &&
                      if_then(inside(x, range(0x10u, 0x12u)), flag == true));
      for (int i = 0; i < 15; ++i) {
        BOOST_REQUIRE(g_mix.next());
        unsigned v = static_cast<unsigned>(g_mix[x]);
        BOOST_REQUIRE(v == 0x00u || (v >= 0xF1u && v <= 0xFFu) || (v >= 0x10u && v <= 0x12u));
        if (v >= 0x10u && v <= 0x12u) BOOST_REQUIRE(g_mix[flag]);
      }
    }
    set_sv_signedness(false);

    // 64-bit pattern
    {
      Variable<uint64_t> u64;
      Generator g64(inside(u64, inside_mask("64'h8000_0000_0000_000?")));
      BOOST_REQUIRE(g64.next());
      BOOST_REQUIRE_EQUAL(g64[u64] & ~uint64_t(0xFULL), 0x8000000000000000ULL);
    }

    // Invalid patterns throw std::invalid_argument
    BOOST_REQUIRE_THROW(inside_mask(""), std::invalid_argument);
    BOOST_REQUIRE_THROW(inside_mask("0'b0"), std::invalid_argument);
    BOOST_REQUIRE_THROW(inside_mask("65'b1"), std::invalid_argument);
    BOOST_REQUIRE_THROW(inside_mask("4'b102"), std::invalid_argument);
    BOOST_REQUIRE_THROW(inside_mask("8'hG0"), std::invalid_argument);
    BOOST_REQUIRE_THROW(inside_mask("8'b"), std::invalid_argument);
    BOOST_REQUIRE_THROW(inside_mask(static_cast<char const*>(nullptr)), std::invalid_argument);
  }
}

BOOST_AUTO_TEST_CASE(sv_ternary_width_and_assign_result_ubsan) {
  struct Guard {
    bool sv;
    Guard() : sv(sv_signedness()) {}
    ~Guard() {
      set_sv_signedness(sv);
      set_solver_backend("Z3");
    }
  } guard;

  // Direct AssignResultToRef signed negative shift UBSan regression checks
  {
    int8_t v8 = 0;
    AssignResultToRef<int8_t> r8(&v8);
    r8.set_value("11111111");
    BOOST_REQUIRE_EQUAL(static_cast<int>(v8), -1);
    r8.set_value("10000000");
    BOOST_REQUIRE_EQUAL(v8, std::numeric_limits<int8_t>::min());

    int16_t v16 = 0;
    AssignResultToRef<int16_t> r16(&v16);
    r16.set_value("1111111111111111");
    BOOST_REQUIRE_EQUAL(v16, -1);
    r16.set_value("1000000000000000");
    BOOST_REQUIRE_EQUAL(v16, std::numeric_limits<int16_t>::min());

    int64_t v64 = 0;
    AssignResultToRef<int64_t> r64(&v64);
    r64.set_value(std::string(64, '1'));
    BOOST_REQUIRE_EQUAL(v64, -1LL);
    r64.set_value("1" + std::string(63, '0'));
    BOOST_REQUIRE_EQUAL(v64, std::numeric_limits<int64_t>::min());

    bool vb = false;
    AssignResultToRef<bool> rb(&vb);
    rb.set_value("true");
    BOOST_REQUIRE(vb);
    rb.set_value("0");
    BOOST_REQUIRE(!vb);
    rb.set_value("");
    BOOST_REQUIRE(!vb);
  }

  // EvalVisitor bitslice(63, 32, u64) signed shift regression check under sv_signedness(true)
  {
    set_sv_signedness(true);
    Variable<uint64_t> u64;
    Evaluator eval;
    eval.assign(u64, 0x123456789ABCDEF0ULL);
    BOOST_REQUIRE(eval.evaluate(bitslice(63, 32, u64)));
    BOOST_REQUIRE_EQUAL(eval.result<uint32_t>(), 0x12345678u);
  }

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    // Negative int8_t solved through inside_mask under ASan+UBSan
    {
      Variable<int8_t> s8;
      set_sv_signedness(false);
      Generator g_neg_mask_cpp(inside(s8, inside_mask("8'hA?")));
      for (int i = 0; i < 8; ++i) {
        BOOST_REQUIRE(g_neg_mask_cpp.next());
        int v = static_cast<int>(g_neg_mask_cpp[s8]);
        BOOST_REQUIRE(v >= -96 && v <= -81);
      }
      Generator g_neg_mask16_cpp(inside(s8, inside_mask("16'hFFA?")));
      for (int i = 0; i < 4; ++i) {
        BOOST_REQUIRE(g_neg_mask16_cpp.next());
        int v = static_cast<int>(g_neg_mask16_cpp[s8]);
        BOOST_REQUIRE(v >= -96 && v <= -81);
      }

      set_sv_signedness(true);
      Generator g_neg_mask_sv(inside(s8, inside_mask("8'hA?")));
      for (int i = 0; i < 4; ++i) {
        BOOST_REQUIRE(g_neg_mask_sv.next());
        int v = static_cast<int>(g_neg_mask_sv[s8]);
        BOOST_REQUIRE(v >= -96 && v <= -81);
      }
      Generator g_neg_mask16_sv(inside(s8, inside_mask("16'h00A?")));
      for (int i = 0; i < 4; ++i) {
        BOOST_REQUIRE(g_neg_mask16_sv.next());
        int v = static_cast<int>(g_neg_mask16_sv[s8]);
        BOOST_REQUIRE(v >= -96 && v <= -81);
      }
    }

    set_sv_signedness(true);
    {
      Variable<uint16_t> x;
      Variable<uint8_t> a, b, tol, pct;

      Generator g_rng_unsat(a == 200 && b == 100 && x == 100 && inside(x, range(a + b, static_cast<uint16_t>(400))));
      BOOST_REQUIRE(!g_rng_unsat.next());
      Generator g_rng_sat(a == 200 && b == 100 && x == 350 && inside(x, range(a + b, static_cast<uint16_t>(400))));
      BOOST_REQUIRE(g_rng_sat.next());

      Variable<uint8_t> x8;
      Generator g_rng_self(a == 200 && b == 100 && x8 == 44 && inside(x8, range(a + b, static_cast<uint8_t>(255))));
      BOOST_REQUIRE(g_rng_self.next());

      Variable<int8_t> s8, sa, sb;
      Generator g_rng_signed(sa == -100 && sb == -100 && s8 == -50 &&
                             inside(s8, range(sa + sb, static_cast<int16_t>(0))));
      BOOST_REQUIRE(g_rng_signed.next());

      Generator g_pm_unsat(a == 200 && b == 100 && tol == 10 && x < 290 && inside(x, tol_plusminus(a + b, tol)));
      BOOST_REQUIRE(!g_pm_unsat.next());
      Generator g_pm_sat(a == 200 && b == 100 && tol == 10 && x == 305 && inside(x, tol_plusminus(a + b, tol)));
      BOOST_REQUIRE(g_pm_sat.next());

      if (std::string(backend) == "Z3") {
        Generator g_pct_unsat(a == 200 && b == 100 && pct == 10 && x < 270 && inside(x, tol_pluspct(a + b, pct)));
        BOOST_REQUIRE(!g_pct_unsat.next());
        Generator g_pct_sat(a == 200 && b == 100 && pct == 10 && x == 305 && inside(x, tol_pluspct(a + b, pct)));
        BOOST_REQUIRE(g_pct_sat.next());
      } else {
        Generator g_pct_const_unsat(a == 200 && b == 100 && x < 270 &&
                                    inside(x, tol_pluspct(a + b, static_cast<uint8_t>(10))));
        BOOST_REQUIRE(!g_pct_const_unsat.next());
        Generator g_pct_const_sat(a == 200 && b == 100 && x == 305 &&
                                  inside(x, tol_pluspct(a + b, static_cast<uint8_t>(10))));
        BOOST_REQUIRE(g_pct_const_sat.next());

        Generator g_pct_sym_unsat(bitslice(3, 0, a) == 10 && bitslice(3, 0, b) == 10 &&
                                  bitslice(3, 0, pct) == 10 && x8 < 18 &&
                                  inside(x8, tol_pluspct(bitslice(3, 0, a) + bitslice(3, 0, b),
                                                         bitslice(3, 0, pct))));
        BOOST_REQUIRE(!g_pct_sym_unsat.next());
        Generator g_pct_sym_sat(bitslice(3, 0, a) == 10 && bitslice(3, 0, b) == 10 &&
                                bitslice(3, 0, pct) == 10 && x8 == 21 &&
                                inside(x8, tol_pluspct(bitslice(3, 0, a) + bitslice(3, 0, b),
                                                       bitslice(3, 0, pct))));
        BOOST_REQUIRE(g_pct_sym_sat.next());
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(inside_two_item_variadic_types) {
  struct Guard {
    ~Guard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    // 2-item inside(x, y, z) with Variable terminals
    {
      Variable<uint8_t> x, y, z;
      Generator g(y == 10u && z == 20u && inside(x, y, z));
      for (int i = 0; i < 6; ++i) {
        BOOST_REQUIRE(g.next());
        uint8_t xv = g[x];
        BOOST_REQUIRE(xv == 10u || xv == 20u);
      }
      Generator g_unsat(y == 10u && z == 20u && x == 15u && inside(x, y, z));
      BOOST_REQUIRE(!g_unsat.next());
      Generator g_neg(y == 10u && z == 20u && x < 25u && x >= 10u && !inside(x, y, z));
      for (int i = 0; i < 6; ++i) {
        BOOST_REQUIRE(g_neg.next());
        uint8_t xv = g_neg[x];
        BOOST_REQUIRE(xv != 10u && xv != 20u);
      }
    }

    // 2-item inside(x, y + 1, z + 2) with compound arithmetic expressions
    {
      Variable<uint8_t> x, y, z;
      Generator g(y == 10u && z == 20u && inside(x, y + 1u, z + 2u));
      for (int i = 0; i < 6; ++i) {
        BOOST_REQUIRE(g.next());
        uint8_t xv = g[x];
        BOOST_REQUIRE(xv == 11u || xv == 22u);
      }
      Generator g_unsat(y == 10u && z == 20u && x == 10u && inside(x, y + 1u, z + 2u));
      BOOST_REQUIRE(!g_unsat.next());
    }

    // 2-item inside(x, v(), 99u) with rand_vec Vector terminal + scalar
    {
      Variable<uint8_t> x;
      rand_vec<uint8_t> v(NULL);
      Generator g;
      g(v().size() == 2u)(foreach (v(), v()[_i] == 5u || v()[_i] == 6u))(inside(x, v(), 99u));
      for (int i = 0; i < 6; ++i) {
        BOOST_REQUIRE(g.next());
        uint8_t xv = g[x];
        BOOST_REQUIRE(xv == 5u || xv == 6u || xv == 99u);
      }
    }

    // 2-item inside(x, range(a, b), y + 1) and ReadReference/WriteReference/tolerance/mask pairs
    {
      Variable<uint8_t> x, a, b, y;
      uint8_t ref_val = 77u;
      uint8_t wval = 0u;
      WriteReference<uint8_t> wr(&wval);
      Generator g(a == 10u && b == 12u && y == 30u && inside(x, range(a, b), y + 1u));
      for (int i = 0; i < 6; ++i) {
        BOOST_REQUIRE(g.next());
        uint8_t xv = g[x];
        BOOST_REQUIRE((xv >= 10u && xv <= 12u) || xv == 31u);
      }

      Generator g_rw(wr == 42u && inside(x, wr, reference(ref_val)));
      for (int i = 0; i < 6; ++i) {
        BOOST_REQUIRE(g_rw.next());
        uint8_t xv = g_rw[x];
        BOOST_REQUIRE(xv == 42u || xv == 77u);
      }

      Generator g_tol(a == 20u && y == 50u && inside(x, tol_plusminus(a, static_cast<uint8_t>(2u)), y));
      for (int i = 0; i < 6; ++i) {
        BOOST_REQUIRE(g_tol.next());
        uint8_t xv = g_tol[x];
        BOOST_REQUIRE((xv >= 18u && xv <= 22u) || xv == 50u);
      }

      Generator g_msk(y == 5u && inside(x, inside_mask("8'hA?"), y));
      for (int i = 0; i < 6; ++i) {
        BOOST_REQUIRE(g_msk.next());
        uint8_t xv = g_msk[x];
        BOOST_REQUIRE((xv >= 0xA0u && xv <= 0xAFu) || xv == 5u);
      }
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()  // Context

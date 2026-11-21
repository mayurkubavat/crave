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

BOOST_AUTO_TEST_SUITE_END()  // Context

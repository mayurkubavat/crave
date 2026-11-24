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
#include <crave/experimental/Experimental.hpp>
#include <crave/experimental/Variable.hpp>
#include <lib/BddSampler.hpp>

#include <set>
#include <iostream>

using namespace crave;

CRAVE_BETTER_ENUM(inside_color_enum, INSIDE_RED, INSIDE_GREEN, INSIDE_BLUE);

BOOST_FIXTURE_TEST_SUITE(Ex_Operators_t, Context_Fixture)

struct logical_not_s1 : crv_sequence_item {
  logical_not_s1(crv_object_name) {}

  crv_variable<unsigned> a;
  crv_constraint con{!(a() != 0)};
};

BOOST_AUTO_TEST_CASE(logical_not_t1) {
  logical_not_s1 item("Item");
  BOOST_REQUIRE(item.con.active());
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.a, 0);
}

struct logical_not_s2 : crv_sequence_item {
  logical_not_s2(crv_object_name) {}

  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_constraint con{if_then_else(!(a() % 2 == 0), b() > 0 && b() <= 50, b() > 50 && b() <= 100)};
};

BOOST_AUTO_TEST_CASE(logical_not_t2) {
  logical_not_s2 item("item");

  BOOST_REQUIRE(item.randomize());
  std::cout << "a =" << item.a << ", b = " << item.b << std::endl;
  if (item.a % 2 != 0) {
    BOOST_REQUIRE_GT(item.b, 0u);
    BOOST_REQUIRE_LE(item.b, 50u);
  } else {
    BOOST_REQUIRE_GT(item.b, 50u);
    BOOST_REQUIRE_LE(item.b, 100u);
  }
}

struct logical_not_s3 : crv_sequence_item {
  logical_not_s3(crv_object_name) {}

  crv_variable<unsigned> a;
  crv_variable<unsigned char> b;
  crv_constraint con1{!(a() == 0)};
  crv_constraint con2{!(a() & (a() - 1)), !b()};
};

BOOST_AUTO_TEST_CASE(logical_not_t3) {
  logical_not_s3 item("item");

  for (unsigned i = 0; i < 20; ++i) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_NE(item.a, 0u);
    BOOST_REQUIRE_EQUAL(item.a & (item.a - 1), 0u);
    BOOST_REQUIRE_EQUAL(item.b, 0u);
  }
}

struct logical_and_s1 : crv_sequence_item {
  crv_variable<bool> a;
  crv_variable<bool> b;
  crv_variable<bool> c;
  crv_constraint con1{a() == true, b() == true};
  crv_constraint con2{c() == (a() && b())};

  logical_and_s1(crv_object_name) { con2.deactivate(); }
};

BOOST_AUTO_TEST_CASE(logical_and_t1) {
  logical_and_s1 item("item");
  BOOST_REQUIRE(item.con1.active());
  BOOST_REQUIRE(!item.con2.active());
  BOOST_REQUIRE(item.randomize());
  item.con2.activate();
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.c, true);
  item.con1 = {item.a() == true, item.b() == false};
  item.con2.deactivate();
  BOOST_REQUIRE(item.randomize());
  item.con2.activate();
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.c, false);
}

struct logical_or_s1 : crv_sequence_item {
  crv_variable<bool> a;
  crv_variable<bool> b;
  crv_variable<bool> c;
  crv_constraint con1{a() == false, b() == false};
  crv_constraint con2{c() == (a() || b())};

  logical_or_s1(crv_object_name) { con2.deactivate(); }
};

BOOST_AUTO_TEST_CASE(logical_or_t1) {
  logical_or_s1 item("item");

  BOOST_REQUIRE(item.randomize());
  item.con2.activate();

  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.c, false);

  item.con1 = {item.a() == true, item.b() == false};
  item.con2.deactivate();
  BOOST_REQUIRE(item.randomize());
  item.con2.activate();

  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.c, true);
}

struct logical_int_operands_s1 : crv_sequence_item {
  logical_int_operands_s1(crv_object_name) {}

  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_variable<int> c;
  crv_variable<int> d;
  crv_constraint con{a() && b() < 4, !(c() || d())};
};

BOOST_AUTO_TEST_CASE(logical_int_operands_t1) {
  logical_int_operands_s1 item("item");

  for (unsigned i = 0; i < 20; ++i) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_NE(item.a, 0u);
    BOOST_REQUIRE_LT(item.b, 4u);
    BOOST_REQUIRE_EQUAL(item.c, 0);
    BOOST_REQUIRE_EQUAL(item.d, 0);
  }
}

struct if_then_int_s1 : crv_sequence_item {
  if_then_int_s1(crv_object_name) {}

  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_variable<unsigned> c;
  crv_variable<unsigned> e;
  crv_constraint con1{a() < 4, if_then(a(), b() == 1), if_then_else(b() - 1u, c() == 7, c() == 0)};
  crv_constraint con2{e() < 10, if_then(e() & 1u, e() > 5)};
};

BOOST_AUTO_TEST_CASE(if_then_int_t1) {
  if_then_int_s1 item("item");

  for (unsigned i = 0; i < 20; ++i) {
    BOOST_REQUIRE(item.randomize());
    if (item.a != 0) BOOST_REQUIRE_EQUAL(item.b, 1u);
    BOOST_REQUIRE_EQUAL(item.c, item.b != 1 ? 7u : 0u);
    BOOST_REQUIRE_LT(item.e, 10u);
    if (item.e & 1) BOOST_REQUIRE_GT(item.e, 5u);
  }
}

struct if_then_else_int_s1 : crv_sequence_item {
  if_then_else_int_s1(crv_object_name) {}

  crv_variable<int> d;
  crv_variable<unsigned> w;
  crv_variable<unsigned> x;
  crv_variable<unsigned char> y;
  crv_variable<unsigned> z;
  crv_constraint con1{if_then_else(d() < 0, d(), -d()) < -3, d() > -10, d() < 10};
  crv_constraint con2{!if_then_else(w() == 0, w(), w() - 5u)};
  crv_constraint con3{if_then_else(x(), y(), z()) == 300u};
};

BOOST_AUTO_TEST_CASE(if_then_else_int_t1) {
  if_then_else_int_s1 item("item");

  for (unsigned i = 0; i < 20; ++i) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE(item.d > 3 || item.d < -3);
    BOOST_REQUIRE(item.w == 0 || item.w == 5);
    BOOST_REQUIRE_EQUAL(item.x, 0u);
    BOOST_REQUIRE_EQUAL(item.z, 300u);
  }
}

struct if_then_else_mixed_s1 : crv_sequence_item {
  if_then_else_mixed_s1(crv_object_name) {}

  crv_variable<bool> f;
  crv_variable<unsigned> a;
  crv_variable<unsigned> x;
  crv_variable<unsigned> y;
  crv_constraint con1{a() < 2u, x() < 4u, if_then_else(a() == 0u, f(), x()) == 1u};
  crv_constraint con2{if_then_else(a() == 0u, x(), f()), y() == f() + x()};
};

BOOST_AUTO_TEST_CASE(if_then_else_mixed_t1) {
  if_then_else_mixed_s1 item("item");

  for (unsigned i = 0; i < 20; ++i) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE(item.f);
    if (item.a == 0)
      BOOST_REQUIRE_NE(item.x, 0u);
    else
      BOOST_REQUIRE_EQUAL(item.x, 1u);
    BOOST_REQUIRE_EQUAL(item.y, item.x + 1);
  }
}

struct logical_equal_s1 : crv_sequence_item {
  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_constraint con{a() == 65535, a() == b()};

  logical_equal_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(equal_t1) {
  logical_equal_s1 item("equal");
  BOOST_REQUIRE(item.con.active());
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.a, item.b);
}

struct not_equal_s1 : crv_sequence_item {
  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_constraint con{a() < 65535, a() != b()};

  not_equal_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(not_equal_t1) {
  not_equal_s1 item("item");
  for (int i = 0; i < 300; ++i) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_NE(item.a, item.b);
  }
}

struct neg_s1 : crv_sequence_item {
  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_constraint con{a() == -1337, b() == -a()};

  neg_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(neg_t1) {
  neg_s1 item("item");
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.a, -1337);
  BOOST_REQUIRE_EQUAL(item.b, 1337);
}

struct neg_s2 : crv_sequence_item {
  crv_variable<bool> a;
  crv_variable<int> b;
  crv_variable<int> c;
  crv_variable<int> d;
  crv_constraint con1 = {dist(a(), distribution<bool>::create(0.5)) && b() == 1337 && c() == 42 &&
                         if_then_else(a(), d() == -b(), d() == -c())};

  neg_s2(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(neg_t2) {
  neg_s2 item("item");
  for (int i = 0; i < 50; ++i) {
    BOOST_REQUIRE(item.randomize());
    if (item.a)
      BOOST_REQUIRE_EQUAL(item.d, -1337);
    else
      BOOST_REQUIRE_EQUAL(item.d, -42);
  }
}

struct comp_s1 : crv_sequence_item {
  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_constraint con1 = {a() == 0, b() == ~a()};

  comp_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(complement_t1) {
  comp_s1 item("item");
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.a, 0);
  BOOST_REQUIRE_EQUAL(item.b, -1);
}

struct bitwise_and_s1 : crv_sequence_item {
  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_variable<unsigned> c;
  crv_constraint con1 = {a() == 42, b() == 1337, c() == (a() & b())};

  bitwise_and_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(bitwise_and_t1) {
  bitwise_and_s1 item("item");
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.c, 40);
}

struct bitwise_or_s1 : crv_sequence_item {
  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_variable<unsigned> c;
  crv_constraint con1 = {a() == 42, b() == 1337, c() == (a() | b())};

  bitwise_or_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(bitwise_or_t1) {
  bitwise_or_s1 item("item");
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.c, 1339);
}

struct xor_s : crv_sequence_item {
  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_variable<unsigned> c;
  crv_constraint con = {a() == 65535, b() == 4080, c() == (a() ^ b())};

  xor_s(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(bitwise_xor_t1) {
  xor_s item("item");
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.c, 61455);
}

struct element_inside_set_s1 : crv_sequence_item {
  crv_variable<unsigned> a;
  std::set<unsigned> s{1, 7, 9};
  crv_constraint con1{inside(a(), s)};

  element_inside_set_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(element_inside_set_t1) {
  element_inside_set_s1 item("item");
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE(item.s.find(item.a) != item.s.end());

  unsigned first = item.a;
  item.con1 &= {item.a() != first};

  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE(item.s.find(item.a) != item.s.end());

  unsigned second = item.a;
  BOOST_REQUIRE_NE(first, second);

  item.con1 &= {item.a() != second};

  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE(item.s.find(item.a) != item.s.end());

  unsigned third = item.a;
  BOOST_REQUIRE_NE(third, second);
  BOOST_REQUIRE_NE(first, third);
  item.con1 &= {item.a() != third};

  BOOST_REQUIRE(!item.randomize());
}

struct element_inside_vec_s1 : crv_sequence_item {
  crv_variable<unsigned> x;
  std::vector<unsigned> v{1, 7, 9};
  crv_constraint con1 = {inside(x(), v)};

  element_inside_vec_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(element_inside_vec_t1) {
  element_inside_vec_s1 item("item");
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE(find(item.v.begin(), item.v.end(), item.x) != item.v.end());

  unsigned first = item.x;
  item.con1 &= {item.x() != first};

  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE(find(item.v.begin(), item.v.end(), item.x) != item.v.end());

  unsigned second = item.x;
  BOOST_REQUIRE_NE(first, second);
  item.con1 &= {item.x() != second};

  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE(find(item.v.begin(), item.v.end(), item.x) != item.v.end());

  unsigned third = item.x;
  BOOST_REQUIRE_NE(third, second);
  BOOST_REQUIRE_NE(first, third);
  item.con1 &= {item.x() != third};
  BOOST_REQUIRE(!item.randomize());
}

struct element_inside_array_s1 : crv_sequence_item {
  crv_variable<unsigned> x;
  unsigned a[3]{1, 7, 9};
  crv_constraint con1{inside(x(), a)};

  element_inside_array_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(element_inside_array_t) {
  element_inside_array_s1 item("item");
  BOOST_REQUIRE(item.randomize());

  unsigned first = item.x;
  item.con1 &= {item.x() != first};

  BOOST_REQUIRE(item.randomize());

  unsigned second = item.x;
  BOOST_REQUIRE_NE(first, second);
  item.con1 &= {item.x() != second};

  BOOST_REQUIRE(item.randomize());
  unsigned third = item.x;
  BOOST_REQUIRE_NE(third, second);
  BOOST_REQUIRE_NE(first, third);
  item.con1 &= {item.x() != third};

  BOOST_REQUIRE(!item.randomize());
}

struct element_inside_list_s1 : crv_sequence_item {
  crv_variable<unsigned> x;
  std::list<unsigned> l{1, 7, 9};
  crv_constraint con1{inside(x(), l)};

  element_inside_list_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(element_inside_list_t1) {
  element_inside_list_s1 item("item");

  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE(find(item.l.begin(), item.l.end(), item.x) != item.l.end());

  unsigned first = item.x;
  item.con1 &= {item.x() != first};

  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE(find(item.l.begin(), item.l.end(), item.x) != item.l.end());

  unsigned second = item.x;
  BOOST_REQUIRE_NE(first, second);
  item.con1 &= {item.x() != second};

  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE(find(item.l.begin(), item.l.end(), item.x) != item.l.end());

  unsigned third = item.x;
  BOOST_REQUIRE_NE(third, second);
  BOOST_REQUIRE_NE(first, third);
  item.con1 &= {item.x() != third};

  BOOST_REQUIRE(!item.randomize());
}

struct element_not_inside_s1 : crv_sequence_item {
  crv_variable<unsigned> x;
  std::set<unsigned> s;
  crv_constraint con1{inside(x(), s)};

  element_not_inside_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(element_not_inside_t1) {
  element_not_inside_s1 item("item");
  BOOST_REQUIRE(!item.randomize());
}

struct element_not_inside_s2 : crv_sequence_item {
  crv_variable<unsigned> x;
  std::vector<unsigned> s;
  crv_constraint con1{inside(x(), s)};

  element_not_inside_s2(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(element_not_inside_t2) {
  element_not_inside_s2 item("item");
  BOOST_REQUIRE(!item.randomize());
}

struct element_not_inside_s3 : crv_sequence_item {
  crv_variable<unsigned> x;
  std::list<unsigned> s;
  crv_constraint con1{inside(x(), s)};

  element_not_inside_s3(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(element_not_inside_t3) {
  element_not_inside_s3 item("item");
  BOOST_REQUIRE(!item.randomize());
}

struct if_then_else_s1 : crv_sequence_item {
  unsigned a;
  crv_variable<unsigned> b;
  crv_constraint con1{if_then_else(reference(a)<5, b()> 0 && b() <= 50, b() > 50 && b() <= 100)};

  if_then_else_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(if_then_else_t1) {
  if_then_else_s1 item("item");
  for (item.a = 0; item.a < 10; ++item.a) {
    BOOST_REQUIRE(item.randomize());
    std::cout << "a =" << item.a << ", b = " << item.b << std::endl;
    if (item.a < 5) {
      BOOST_REQUIRE_GT(item.b, 0);
      BOOST_REQUIRE_LE(item.b, 50);
    } else {
      BOOST_REQUIRE_GT(item.b, 50);
      BOOST_REQUIRE_LE(item.b, 100);
    }
  }
}

struct if_then_s1 : crv_sequence_item {
  unsigned a;
  crv_variable<unsigned> b;
  crv_constraint con1{if_then(reference(a)<5, b()> 0 && b() <= 100),
                      if_then(reference(a) >= 5, b() > 100 && b() <= 1000)};

  if_then_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(if_then_t1) {
  if_then_s1 item("item");

  for (item.a = 0; item.a < 10; ++item.a) {
    BOOST_REQUIRE(item.randomize());
    std::cout << "a =" << item.a << ", b = " << item.b << std::endl;
    if (item.a < 5) {
      BOOST_REQUIRE_GT(item.b, 0);
      BOOST_REQUIRE_LE(item.b, 100);
    } else {
      BOOST_REQUIRE_GT(item.b, 100);
      BOOST_REQUIRE_LE(item.b, 1000);
    }
  }
}

struct bitslice_s1 : crv_sequence_item {
  crv_variable<short> x;
  crv_constraint con1{bitslice(10, 3, x()) == 0xFF};

  bitslice_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(bitslice_t) {
  bitslice_s1 item("item");
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL((item.x >> 3) & 0xFF, 0xFF);

  BOOST_REQUIRE_THROW(item.con1 &= {bitslice(3, 10, item.x()) == 0xFF}, std::runtime_error);
  BOOST_REQUIRE_THROW(item.con1 &= {bitslice(16, 3, item.x()) == 0xFF}, std::runtime_error);
}

struct s_shiftleft : crv_sequence_item {
  s_shiftleft(crv_object_name) {}
  crv_variable<unsigned> a, b, c;
  crv_constraint con = {a() < 256u, b() < ((unsigned)(sizeof(unsigned) * 8u)), c() == (a() << b())};
};
BOOST_AUTO_TEST_CASE(shiftleft) {
  VariableDefaultSolver::bypass_constraint_analysis = true;
  s_shiftleft item("item");
  int count = 0;
  int max_count = 200;
  while (item.randomize() && ++count < max_count) {
    unsigned av = item.a;
    unsigned bv = item.b;
    unsigned r = av << bv;

    BOOST_REQUIRE_EQUAL(r, item.c);
    item.con &= {item.a() != item.a || item.b() != item.b};
  }

  VariableDefaultSolver::bypass_constraint_analysis = false;
  BOOST_REQUIRE_EQUAL(count, max_count);
}

struct s_shiftright : public crv_sequence_item {
  s_shiftright(crv_object_name) {}
  crv_variable<unsigned> a, b, c;
  crv_constraint con = {a() > 256u, b() < 8u, c() == (a() >> b())};
};

BOOST_AUTO_TEST_CASE(shiftright) {
  VariableDefaultSolver::bypass_constraint_analysis = true;
  s_shiftright item("shiftright");
  int count = 0;
  int max_count = 300;
  while (item.randomize() && ++count < max_count) {
    unsigned av = item.a;
    unsigned bv = item.b;
    unsigned r = av >> bv;

    BOOST_REQUIRE_EQUAL(r, item.c);
    item.con &= {item.a != item.a() || item.b() != item.b};
  }

  VariableDefaultSolver::bypass_constraint_analysis = false;
  BOOST_REQUIRE_EQUAL(count, max_count);
}

struct s_plus_minus : crv_sequence_item {
  s_plus_minus(crv_object_name) {}
  crv_variable<unsigned> a, b, q, r;
  crv_constraint con = {b() != 0u, b() < a(), (q() == a() + b()), r() == a() - b()};
};
BOOST_AUTO_TEST_CASE(plus_minus) {
  VariableDefaultSolver::bypass_constraint_analysis = true;
  s_plus_minus item("item");

  unsigned cnt = 0u;
  while (item.randomize() && cnt < 300) {
    BOOST_REQUIRE_EQUAL(item.a + item.b, item.q);
    BOOST_REQUIRE_EQUAL(item.a - item.b, item.r);

    item.con &= {(item.a() != item.a || item.b() != item.b)};
    std::cout << "#" << cnt++ << ": result: a=" << item.a << ", b=" << item.b << ", q=" << item.q << ", r=" << item.r
              << "\n" << std::endl;
  }

  VariableDefaultSolver::bypass_constraint_analysis = false;
}

struct mult_mod_s1 : crv_sequence_item {
  crv_variable<int> a;
  crv_variable<int> b;
  crv_constraint con1 = {-3 <= a() && a() <= 3, -3 <= b() && b() <= 3, a() * b() % 6 == 0};

  mult_mod_s1(crv_object_name) {}
};

BOOST_AUTO_TEST_CASE(mult_mod_t1) {
  mult_mod_s1 item("item");

  int cnt = 0;
  for (int i = -3; i <= 3; i++)
    for (int j = -3; j <= 3; j++)
      if (i * j % 6 == 0) cnt++;

  int cnt1 = 0;

  while (item.randomize()) {
    cnt1++;
    BOOST_REQUIRE_EQUAL(item.a * item.b % 6, 0);
    item.con1 &= {item.a() != item.a || item.b() != item.b};
    std::cout << "result: a1=" << item.a << ", b1=" << item.b << "\n" << std::endl;
    BOOST_REQUIRE_LE(cnt1, cnt);
  }

  BOOST_REQUIRE_EQUAL(cnt, cnt1);
}

struct s_divide : public crv_sequence_item {
  s_divide(crv_object_name) {}
  crv_variable<unsigned char> a, b, q, r;
  crv_constraint con = {b() != (unsigned char)0u, a() < (unsigned char)16u, b() < (unsigned char)16u, q() == a() / b(),
                        r() == a() % b()};
};

BOOST_AUTO_TEST_CASE(divide) {
  s_divide item("item");
  VariableDefaultSolver::bypass_constraint_analysis = true;

  while (item.randomize()) {
    BOOST_REQUIRE_EQUAL(item.a / item.b, item.q);
    BOOST_REQUIRE_EQUAL(item.a % item.b, item.r);
    item.con &= {item.a() != item.a, item.b() != item.b};
    std::cout << "result: a=" << item.a << ", b=" << item.b << ", q=" << item.q << ", r=" << item.r << "\n"
              << std::endl;
  }

  VariableDefaultSolver::bypass_constraint_analysis = false;
}
struct array_product_s1 : public crv_sequence_item {
  array_product_s1(crv_object_name) {}
  crv_array<unsigned, 3> arr;
  crv_constraint con = {arr.product() == 30u, arr[0]() > 1u, arr[1]() > 1u, arr[2]() > 1u,
                        arr[0]() < 30u, arr[1]() < 30u, arr[2]() < 30u};
};

BOOST_AUTO_TEST_CASE(array_product_t1) {
  array_product_s1 item("item");

  std::set<unsigned> elements;
  for (unsigned i = 0; i < 10; ++i) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_EQUAL(item.arr[0] * item.arr[1] * item.arr[2], 30u);
    for (unsigned j = 0; j < 3; ++j) elements.insert(item.arr[j]);
  }
  BOOST_REQUIRE(elements == std::set<unsigned>({2, 3, 5}));
}

// As in SystemVerilog, the product has the width of the elements and wraps around.
struct array_product_s2 : public crv_sequence_item {
  array_product_s2(crv_object_name) {}
  crv_array<unsigned char, 3> arr;
  crv_constraint con = {arr.product() == 0u, arr[0]() != 0u, arr[1]() != 0u, arr[2]() != 0u};
};

BOOST_AUTO_TEST_CASE(array_product_t2) {
  array_product_s2 item("item");

  for (unsigned i = 0; i < 10; ++i) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_NE(item.arr[0], 0u);
    BOOST_REQUIRE_NE(item.arr[1], 0u);
    BOOST_REQUIRE_NE(item.arr[2], 0u);
    BOOST_REQUIRE_EQUAL((unsigned char)(item.arr[0] * item.arr[1] * item.arr[2]), 0u);
  }
}

struct array_bitwise_s1 : public crv_sequence_item {
  array_bitwise_s1(crv_object_name) {}
  crv_array<unsigned char, 3> arr;
  crv_constraint con = {arr.and_() == 0x01u, arr.or_() == 0x0fu, arr.xor_() == 0x03u};
};

BOOST_AUTO_TEST_CASE(array_bitwise_t1) {
  array_bitwise_s1 item("item");

  for (unsigned i = 0; i < 10; ++i) {
    BOOST_REQUIRE(item.randomize());
    unsigned char a = item.arr[0], b = item.arr[1], c = item.arr[2];
    BOOST_REQUIRE_EQUAL(a & b & c, 0x01);
    BOOST_REQUIRE_EQUAL(a | b | c, 0x0f);
    BOOST_REQUIRE_EQUAL(a ^ b ^ c, 0x03);
  }
}

struct array_bool_s1 : public crv_sequence_item {
  array_bool_s1(crv_object_name) {}
  crv_array<bool, 3> arr;
  crv_constraint con = {arr.and_() == false, arr.or_() == true, arr.xor_() == false};
};

struct array_bool_s2 : public crv_sequence_item {
  array_bool_s2(crv_object_name) {}
  crv_array<bool, 3> arr;
  crv_constraint con = {arr.product()};
};

BOOST_AUTO_TEST_CASE(array_bool_t1) {
  array_bool_s1 item("item");

  for (unsigned i = 0; i < 10; ++i) {
    BOOST_REQUIRE(item.randomize());
    // exactly two elements are true
    BOOST_REQUIRE_EQUAL(item.arr[0] + item.arr[1] + item.arr[2], 2);
  }

  array_bool_s2 item2("item2");
  BOOST_REQUIRE(item2.randomize());
  BOOST_REQUIRE(item2.arr[0] && item2.arr[1] && item2.arr[2]);
}

struct inside_soundness_item : public crv_sequence_item {
  inside_soundness_item(crv_object_name) {}
  crv_variable<unsigned> x;
  crv_variable<unsigned> y;
  crv_variable<inside_color_enum> c;
  std::set<unsigned> s{5, 6};
  std::set<unsigned> s2{1, 2, 3};
  std::set<inside_color_enum> cs{inside_color_enum::INSIDE_RED};
  crv_constraint con{"con"};
};

BOOST_AUTO_TEST_CASE(inside_negation_and_if_then) {
  inside_soundness_item item("item");
  item.con = {item.x() == 5u, !inside(item.x(), item.s)};
  BOOST_REQUIRE(!item.randomize());

  item.con = {!inside(item.x(), item.s2), item.x() < 5u};
  for (int i = 0; i < 20; ++i) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE(item.x == 0u || item.x == 4u);
  }

  item.con = {item.x() == 5u, if_then(inside(item.x(), item.s), item.y() == 1u), item.y() == 2u};
  BOOST_REQUIRE(!item.randomize());

  item.con = {!inside(item.c(), item.cs)};
  for (int i = 0; i < 20; ++i) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE(item.c == inside_color_enum::INSIDE_GREEN || item.c == inside_color_enum::INSIDE_BLUE);
  }
}

struct inside_crv_array_item : public crv_sequence_item {
  inside_crv_array_item(crv_object_name) {}
  crv_variable<unsigned char> x;
  crv_variable<unsigned char> a;
  crv_variable<unsigned char> b;
  crv_variable<bool> flag;
  crv_array<unsigned char, 4> arr;
  crv_constraint c_arr{arr[0]() == static_cast<unsigned char>(10u), arr[1]() == static_cast<unsigned char>(20u),
                       arr[2]() == static_cast<unsigned char>(30u), arr[3]() == static_cast<unsigned char>(40u)};
  crv_constraint con{"con"};
};

struct inside_crv_array2_item : public crv_sequence_item {
  inside_crv_array2_item(crv_object_name) {}
  crv_variable<unsigned char> a;
  crv_variable<unsigned char> b;
  crv_array<unsigned char, 2> arr;
  crv_constraint c_arr{arr[0]() == static_cast<unsigned char>(10u), arr[1]() == static_cast<unsigned char>(20u)};
  crv_constraint con{"con"};
};

struct inside_empty_array_item : public crv_sequence_item {
  inside_empty_array_item(crv_object_name) {}
  crv_variable<unsigned> x;
  crv_array<unsigned, 0> arr;
  crv_constraint con{"con"};
};

BOOST_AUTO_TEST_CASE(crv_array_membership_and_experimental_inside) {
  struct Guard {
    ~Guard() { set_solver_backend("Z3"); }
  } guard;
  struct BypassAnalysisGuard {
    bool prev_;
    explicit BypassAnalysisGuard(bool enable) : prev_(VariableDefaultSolver::bypass_constraint_analysis) {
      if (enable) VariableDefaultSolver::bypass_constraint_analysis = true;
    }
    ~BypassAnalysisGuard() { VariableDefaultSolver::bypass_constraint_analysis = prev_; }
  };

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    {
      inside_crv_array_item item("item");
      item.con = {inside(item.x(), item.arr)};
      for (int i = 0; i < 15; ++i) {
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE(item.x == 10u || item.x == 20u || item.x == 30u || item.x == 40u);
      }

      {
        BypassAnalysisGuard bypass(std::string(backend) == "Cudd");
        item.con = {item.x() == static_cast<unsigned char>(20u), !inside(item.x(), item.arr)};
        BOOST_REQUIRE(!item.randomize());
      }

      {
        BypassAnalysisGuard bypass(std::string(backend) == "Cudd");
        item.con = {if_then(inside(item.x(), item.arr), item.flag() == true), !item.flag(),
                    item.x() < static_cast<unsigned char>(50u)};
        for (int i = 0; i < 10; ++i) {
          BOOST_REQUIRE(item.randomize());
          BOOST_REQUIRE(item.x != 10u && item.x != 20u && item.x != 30u && item.x != 40u);
        }
      }

      {
        BypassAnalysisGuard bypass(std::string(backend) == "Cudd");
        item.con = {item.a() < static_cast<unsigned char>(25u), item.b() < static_cast<unsigned char>(25u),
                    inside(item.a() + item.b(), item.arr, static_cast<unsigned char>(99u))};
        for (int i = 0; i < 10; ++i) {
          BOOST_REQUIRE(item.randomize());
          unsigned sum = item.a + item.b;
          BOOST_REQUIRE(sum == 10u || sum == 20u || sum == 30u || sum == 40u);
        }
      }
      {
        inside_crv_array2_item arr2("arr2");
        arr2.con = {arr2.a() < static_cast<unsigned char>(15u), arr2.b() < static_cast<unsigned char>(15u),
                    inside(arr2.a() + arr2.b(), arr2.arr, static_cast<unsigned char>(99u))};
        for (int i = 0; i < 10; ++i) {
          BOOST_REQUIRE(arr2.randomize());
          unsigned sum = arr2.a + arr2.b;
          BOOST_REQUIRE(sum == 10u || sum == 20u);
        }
      }
    }

    {
      inside_empty_array_item empty_it("empty_it");
      empty_it.con = {inside(empty_it.x(), empty_it.arr)};
      BOOST_REQUIRE(!empty_it.randomize());
      empty_it.con = {!inside(empty_it.x(), empty_it.arr), empty_it.x() == 7u};
      BOOST_REQUIRE(empty_it.randomize());
      BOOST_REQUIRE_EQUAL(empty_it.x, 7u);
    }

    // crave::randomize().with() + soft(inside(...)) + inside(reference(x), rand_vec)
    {
      unsigned v = 0u;
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(crave::randomize(v).with(inside(reference(v), range(16u, 31u)),
                                               inside(reference(v), inside_mask("8'b0001_00xx"))));
        BOOST_REQUIRE(v >= 16u && v <= 19u);
      }

      unsigned rval = 3u;
      rand_vec<unsigned> rv(NULL);
      std::set<unsigned> r_seen;
      const int iters = (std::string(backend) == "Cudd") ? 8 : 25;
      for (int i = 0; i < iters; ++i) {
        BOOST_REQUIRE(crave::randomize(rval).with(rv().size() == 3u, foreach (rv(), rv()[_i] < 10u),
                                                  inside(reference(rval), rv)));
        BOOST_REQUIRE(rval == rv[0] || rval == rv[1] || rval == rv[2]);
        r_seen.insert(rval);
      }
      BOOST_REQUIRE_GT(r_seen.size(), 1u);
    }

    {
      inside_crv_array_item item("soft_it");
      item.c_arr.deactivate();
      item.con = {soft(inside(item.x(), tol_pluspct(100u, 10u))), soft(inside(item.x(), range(200u, 210u)))};
      BOOST_REQUIRE(item.randomize());
      BOOST_REQUIRE(item.x >= 200u && item.x <= 210u);
    }
  }
}

struct inside_two_item_exp_item : public crv_sequence_item {
  inside_two_item_exp_item(crv_object_name) {}
  crv_variable<unsigned char> x;
  crv_variable<unsigned char> y;
  crv_variable<unsigned char> z;
  crv_variable<unsigned char> a;
  crv_variable<unsigned char> b;
  crv_vector<unsigned char> vec;
  crv_constraint c_vec{vec().size() == 2u, foreach (vec(), vec()[_i] == static_cast<unsigned char>(7u) ||
                                                             vec()[_i] == static_cast<unsigned char>(8u))};
  crv_constraint con{"con"};
};

BOOST_AUTO_TEST_CASE(inside_two_item_experimental) {
  struct Guard {
    ~Guard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    inside_two_item_exp_item item("item");
    item.c_vec.deactivate();

    // 2-item inside(x(), y(), z())
    item.con = {item.y() == static_cast<unsigned char>(10u), item.z() == static_cast<unsigned char>(20u),
                inside(item.x(), item.y(), item.z())};
    for (int i = 0; i < 6; ++i) {
      BOOST_REQUIRE(item.randomize());
      BOOST_REQUIRE(item.x == 10u || item.x == 20u);
    }

    // 2-item inside(x(), y() + 1, z() + 2)
    item.con = {item.y() == static_cast<unsigned char>(10u), item.z() == static_cast<unsigned char>(20u),
                inside(item.x(), item.y() + static_cast<unsigned char>(1u),
                       item.z() + static_cast<unsigned char>(2u))};
    for (int i = 0; i < 6; ++i) {
      BOOST_REQUIRE(item.randomize());
      BOOST_REQUIRE(item.x == 11u || item.x == 22u);
    }

    // 2-item inside(x(), range(a(), b()), y() + 1)
    item.con = {item.a() == static_cast<unsigned char>(30u), item.b() == static_cast<unsigned char>(32u),
                item.y() == static_cast<unsigned char>(40u),
                inside(item.x(), range(item.a(), item.b()), item.y() + static_cast<unsigned char>(1u))};
    for (int i = 0; i < 6; ++i) {
      BOOST_REQUIRE(item.randomize());
      BOOST_REQUIRE((item.x >= 30u && item.x <= 32u) || item.x == 41u);
    }

    // 2-item with ReadReference (prev) + WriteReference
    item.y = static_cast<unsigned char>(55u);
    item.con = {item.z() == static_cast<unsigned char>(66u), inside(item.x(), item.y(prev), item.z())};
    for (int i = 0; i < 6; ++i) {
      BOOST_REQUIRE(item.randomize());
      BOOST_REQUIRE(item.x == 55u || item.x == 66u);
      item.y = static_cast<unsigned char>(55u);
    }

    // 2-item with crv_vector Vector terminal + scalar
    item.c_vec.activate();
    item.con = {inside(item.x(), item.vec(), static_cast<unsigned char>(99u))};
    for (int i = 0; i < 6; ++i) {
      BOOST_REQUIRE(item.randomize());
      BOOST_REQUIRE(item.x == 7u || item.x == 8u || item.x == 99u);
    }
  }
}

struct array_m2_item : public crv_sequence_item {
  array_m2_item(crv_object_name) {}
  crv_array<uint8_t, 4> arr;
  crv_array<uint8_t, 4> brr;
  crv_array<uint8_t, 1> arr1;
  crv_array<uint8_t, 0> arr0;
  crv_array<bool, 4> barr;
  crv_variable<uint8_t> extra;
  crv_constraint con{"con"};
};

BOOST_AUTO_TEST_CASE(crv_array_unique_size_foreach_and_with) {
  struct Guard {
    ~Guard() {
      set_solver_backend("Z3");
      set_sv_signedness(false);
    }
  } guard;

  for (bool sv : {false, true}) {
    set_sv_signedness(sv);
    for (const char* backend : {"Z3", "Cudd"}) {
      if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
      set_solver_backend(backend);

      array_m2_item item("item");
      static_assert(item.arr.size() == 4u, "crv_array::size() must be constexpr");
      static_assert(item.arr0.size() == 0u, "empty crv_array::size() must be 0");
      static_assert(item.arr0.empty(), "empty crv_array::empty() must be true");
      BOOST_REQUIRE_EQUAL(item.arr.size(), 4u);
      BOOST_REQUIRE(!item.arr.empty());

      // unique(crv_array) + unique(arr1) + unique(arr0) + foreach(crv_array, ...)
      item.con = {
          unique(item.arr),
          unique(item.arr1),
          unique(item.arr0),
          foreach (item.arr0, item.arr0[_i] == 99u),
          foreach (item.arr1, item.arr1[_i] == 42u),
          foreach (item.arr, item.arr[_i] < 10u),
          foreach (item.arr, if_then(_i > 0u, item.arr[_i] > item.arr[_i - 1u])),
      };
      if (std::string(backend) == "Z3") {
        item.con = {
            unique(item.arr),
            unique(item.arr1),
            unique(item.arr0),
            foreach (item.arr0, item.arr0[_i] == 99u),
            foreach (item.arr1, item.arr1[_i] == 42u),
            foreach (item.arr, item.arr[_i] < 10u),
            foreach (item.arr, if_then(_i > 0u, item.arr[_i] > item.arr[_i - 1u])),
            foreach (item.brr, item.brr[_i]() == item.arr[_i]()),
        };
      }
      for (int iter = 0; iter < 5; ++iter) {
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_EQUAL(item.arr1[0], 42u);
        std::set<uint8_t> seen;
        for (unsigned i = 0; i < item.arr.size(); ++i) {
          BOOST_REQUIRE_LT(item.arr[i], 10u);
          if (std::string(backend) == "Z3") {
            BOOST_REQUIRE_EQUAL(item.brr[i], item.arr[i]);
          }
          seen.insert(item.arr[i]);
          if (i > 0) BOOST_REQUIRE_GT(item.arr[i], item.arr[i - 1]);
        }
        BOOST_REQUIRE_EQUAL(seen.size(), 4u);
      }

      // _item, lambdas, !foreach, soft(foreach), and predicate reductions on crv_array
      item.con = {
          foreach (item.arr, _item < 20u),
          foreach (item.brr, [](auto const& idx, auto const& val) { return val == idx + 10u; }),
          item.arr.sum_with(item.arr[_i] > 10u) == 3u,
          item.arr.with(_item <= 10u).sum() == 1u,
          item.arr.sum().with([](auto const& v) { return v == 15u; }) == 2u,
          item.arr.and_with(item.arr[_i] < 20u),
          item.arr.or_with(item.arr[_i] == 5u),
          item.arr.product_with([](auto const& v) { return v < 20u; }),
          item.arr.xor_with(item.arr[_i] == 5u) == true,
          item.barr.sum_with(item.barr[_i]) == 3u,
          !foreach (item.arr, item.arr[_i] == 0u),
      };
      BOOST_REQUIRE(item.randomize());
      unsigned gt10 = 0, eq15 = 0, eq5 = 0, btrue = 0;
      for (unsigned i = 0; i < 4; ++i) {
        BOOST_REQUIRE_LT(item.arr[i], 20u);
        BOOST_REQUIRE_EQUAL(item.brr[i], i + 10u);
        if (item.arr[i] > 10u) ++gt10;
        if (item.arr[i] == 15u) ++eq15;
        if (item.arr[i] == 5u) ++eq5;
        if (item.barr[i]) ++btrue;
      }
      BOOST_REQUIRE_EQUAL(gt10, 3u);
      BOOST_REQUIRE_EQUAL(eq15, 2u);
      BOOST_REQUIRE_EQUAL(eq5 % 2u, 1u);
      BOOST_REQUIRE_EQUAL(btrue, 3u);

      // SelfDeterminedExpr width isolation on crv_array<uint8_t, 4>::sum_with(arr[_i])
      item.con = {item.arr.sum_with(item.arr[_i]) == 400u};
      BOOST_REQUIRE(!item.randomize());

      // Variadic unique mixing crv_array + scalar crv_variable()
      item.con = {foreach (item.arr, item.arr[_i] < 5u), item.extra() < 5u, unique(item.arr, item.extra())};
      BOOST_REQUIRE(item.randomize());
      std::set<uint8_t> uvals{item.extra};
      for (unsigned i = 0; i < 4; ++i) uvals.insert(item.arr[i]);
      BOOST_REQUIRE_EQUAL(uvals.size(), 5u);
    }
  }
}

struct exp_d4_d5_item : public crv_sequence_item {
  exp_d4_d5_item(crv_object_name) {}
  crv_variable<int8_t> s8;
  crv_variable<uint8_t> u8;
  crv_variable<bool> flag;
  crv_constraint con{"con"};
};

BOOST_AUTO_TEST_CASE(experimental_iff_onehot_countones_clog2) {
  struct Guard {
    ~Guard() {
      set_solver_backend("Z3");
      set_sv_signedness(false);
    }
  } guard;

  for (bool sv : {false, true}) {
    set_sv_signedness(sv);
    for (const char* backend : {"Z3", "Cudd"}) {
      if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
      set_solver_backend(backend);

      exp_d4_d5_item item("item");
      item.con = {
          iff(item.flag(), item.s8() == int8_t(-128)),
          item.flag() == true,
          onehot(item.s8()),
          onehot0(item.s8()),
          countones(item.s8()) == 1u,
          bitcount(item.s8()) == 1u,
          clog2(item.s8()) == 7u,
          countones(item.u8()) == 4u,
          clog2(item.u8()) == 4u,
      };
      BOOST_REQUIRE(item.randomize());
      BOOST_REQUIRE_EQUAL(static_cast<int>(item.s8), -128);
      BOOST_REQUIRE(item.flag);
      exp_d4_d5_item item2("item2");
      item2.con = {iff(item2.u8() > 0u, item2.flag())};
      BOOST_REQUIRE(item2.randomize_with(item2.u8() == 0u));
      BOOST_REQUIRE(!item2.flag);
    }
  }
}

BOOST_AUTO_TEST_CASE(crv_array_rejects_nested_crv_vector_ops) {
  crv_array<uint8_t, 3> arr{"arr"};
  crv_vector<uint8_t> vec{"vec"};
  BOOST_CHECK_THROW((crv_constraint{"c1", foreach (arr, arr[_i] == vec.sum())}), std::runtime_error);
  BOOST_CHECK_THROW((crv_constraint{"c2", foreach (arr, inside(arr[_i], vec))}), std::runtime_error);
  BOOST_CHECK_THROW((crv_constraint{"c3", foreach (arr, arr[_i] == vec.sum_with(_item > 0u))}), std::runtime_error);
  BOOST_CHECK_THROW((crv_constraint{"c4", foreach (arr, arr[_i] == vec()[_i])}), std::runtime_error);
  BOOST_CHECK_THROW((crv_constraint{"c5", arr.sum_with(arr[_i] == vec.sum()) == 1u}), std::runtime_error);
  BOOST_CHECK_THROW((crv_constraint{"c6", foreach (arr, arr[vec.sum()] == 1u)}), std::runtime_error);
}

struct nested_arr_item : public crv_sequence_item {
  nested_arr_item(crv_object_name) {}
  crv_array<uint8_t, 3> arr{"arr"};
  crv_array<uint8_t, 3> brr{"brr"};
  crv_array<uint8_t, 3> crr{"crr"};
  crv_constraint con{"con"};
};

BOOST_AUTO_TEST_CASE(crv_array_nested_foreach_and_sum_with) {
  struct Guard {
    ~Guard() {
      set_solver_backend("Z3");
      set_sv_signedness(false);
    }
  } guard;

  for (bool sv : {false, true}) {
    set_sv_signedness(sv);
    for (const char* backend : {"Z3", "Cudd"}) {
      if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
      set_solver_backend(backend);

      // Challenger 1 Repro1
      {
        nested_arr_item item("r1");
        item.con = {item.arr[0]() == 1u,
                    item.arr[1]() == 2u,
                    item.arr[2]() == 3u,
                    item.brr[0]() == 1u,
                    item.brr[1]() == 2u,
                    item.brr[2]() == 3u,
                    foreach (item.arr, item.arr[_i] == item.brr.sum_with(item.brr[_i] <= item.arr[_i]))};
        BOOST_REQUIRE(item.randomize());
      }

      // Challenger 1 Repro2
      {
        nested_arr_item item("r2");
        item.con = {item.arr[0]() == 1u,
                    item.arr[1]() == 2u,
                    item.arr[2]() == 3u,
                    item.brr[0]() == 1u,
                    item.brr[1]() == 2u,
                    item.brr[2]() == 3u,
                    item.arr.sum_with(item.arr[_i] == item.brr.sum_with(item.brr[_i] <= item.arr[_i])) == 3u};
        BOOST_REQUIRE(item.randomize());
      }

      // Challenger 1 Repro3
      {
        nested_arr_item item("r3");
        item.con = {item.arr[0]() == 1u,
                    item.arr[1]() == 5u,
                    item.arr[2]() == 5u,
                    item.brr[0]() == 2u,
                    item.brr[1]() == 2u,
                    item.brr[2]() == 2u,
                    foreach (item.arr, if_then(item.arr[_i] < 3u, foreach (item.brr, item.arr[_i] < item.brr[_i])))};
        BOOST_REQUIRE(item.randomize());
      }

      // Same-array nesting and multi-array 3-way nesting (Z3: 9 coupled uint8_t vars)
      if (std::string(backend) == "Z3") {
        nested_arr_item item("same_multi");
        item.con = {
            foreach (item.arr, item.arr[_i] > 0u && item.arr[_i] <= 4u),
            foreach (item.arr, item.arr[_i] <= item.arr.sum_with(_item > 0u)),
            item.arr.sum_with([&](auto const& a) {
              return item.arr.sum_with([&](auto const& b) { return b > a; }) == 0u;
            }) == 1u,
            item.brr[0]() == 2u,
            item.brr[1]() == 3u,
            item.brr[2]() == 4u,
            foreach (item.arr,
                     if_then(item.arr[_i] < 2u,
                             foreach (item.brr, item.arr[_i] < item.brr[_i] && item.crr[_i] == item.brr[_i]))),
        };
        BOOST_REQUIRE(item.randomize());
        bool has_lt2 = false;
        for (unsigned i = 0; i < 3; ++i) {
          if (item.arr[i] < 2u) has_lt2 = true;
        }
        if (has_lt2) {
          for (unsigned j = 0; j < 3; ++j) BOOST_REQUIRE_EQUAL(item.crr[j], item.brr[j]);
        }
      }

      // Nested 2-arg lambdas + guarded OOB arr[_i - 1] / brr[_i - 1] in sum_with (unguarded OOB in *_with fails)
      if (std::string(backend) == "Z3") {
        nested_arr_item item("oob");
        item.con = {
            item.arr[0]() == 1u,
            item.arr[1]() == 2u,
            item.arr[2]() == 3u,
            item.brr[0]() == 1u,
            item.brr[1]() == 2u,
            item.brr[2]() == 4u,
            item.arr.sum_with(if_then_else(_i > 0u, item.arr[_i] > item.arr[_i - 1u], false)) == 2u,
            foreach (item.arr,
                     if_then(_i > 0u,
                             item.brr.sum_with(if_then_else(_i > 0u, item.brr[_i] > item.brr[_i - 1u], false) &&
                                               item.arr[_i] >= 2u) == 2u)),
        };
        BOOST_REQUIRE(item.randomize());

        nested_arr_item item_unguarded_oob("unguarded_sum_oob");
        item_unguarded_oob.con = {
            item_unguarded_oob.arr[0]() == 1u,
            item_unguarded_oob.arr[1]() == 2u,
            item_unguarded_oob.arr[2]() == 3u,
            item_unguarded_oob.arr.sum_with(item_unguarded_oob.arr[_i] > item_unguarded_oob.arr[_i - 1u]) == 2u,
        };
        BOOST_REQUIRE(!item_unguarded_oob.randomize());
      } else {
        struct oob2_item : public crv_sequence_item {
          oob2_item(crv_object_name) {}
          crv_array<uint8_t, 2> arr{"arr"};
          crv_array<uint8_t, 2> brr{"brr"};
          crv_constraint con{"con"};
        } item("oob2"), item_unguarded_oob("oob2_unguarded");
        item.con = {
            item.arr[0]() == 1u,
            item.arr[1]() == 2u,
            item.brr[0]() == 1u,
            item.brr[1]() == 4u,
            item.arr.sum_with(if_then_else(_i > 0u, item.arr[_i] > item.arr[_i - 1u], false)) == 1u,
            foreach (item.arr,
                     if_then(_i > 0u,
                             item.brr.sum_with(if_then_else(_i > 0u, item.brr[_i] > item.brr[_i - 1u], false) &&
                                               item.arr[_i] >= 2u) == 1u)),
        };
        BOOST_REQUIRE(item.randomize());

        item_unguarded_oob.con = {
            item_unguarded_oob.arr[0]() == 1u,
            item_unguarded_oob.arr[1]() == 2u,
            item_unguarded_oob.arr.sum_with(item_unguarded_oob.arr[_i] > item_unguarded_oob.arr[_i - 1u]) == 1u,
        };
        BOOST_REQUIRE(!item_unguarded_oob.randomize());
      }

      if (std::string(backend) == "Z3") {
        nested_arr_item item("lambdas");
        item.con = {
            item.arr[0]() == 1u,
            item.arr[1]() == 2u,
            item.arr[2]() == 3u,
            item.brr[0]() == 2u,
            item.brr[1]() == 3u,
            item.brr[2]() == 4u,
            item.arr.sum_with([&](auto const& idx, auto const& a) {
              return item.brr.sum_with([&](auto const& j, auto const& b) {
                return (idx == j) && (a + 1u == b) && (item.arr[j] + 1u == item.brr[idx]);
              });
            }) == 3u,
        };
        BOOST_REQUIRE(item.randomize());
      }
    }
  }
}

struct lambda_proxy_item : public crv_sequence_item {
  lambda_proxy_item(crv_object_name) {}
  crv_array<uint8_t, 4> arr{"arr"};
  crv_vector<uint8_t> vec{"vec"};
  crv_variable<uint8_t> x;
  crv_constraint con{"con"};
};

BOOST_AUTO_TEST_CASE(lambda_node_proxy_operator_completeness) {
  struct Guard {
    ~Guard() {
      set_solver_backend("Z3");
      set_sv_signedness(false);
    }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    // Array lambdas using if_then, if_then_else, iff, inside, unique, onehot, onehot0, countones, clog2, arr[idx], arr[idx-1]
    if (std::string(backend) == "Z3") {
      lambda_proxy_item item("lp_arr");
      item.con = {
          item.x() == 8u,
          foreach (item.arr, [&](auto const& idx, auto const& it) { return if_then(idx > 0u, it >= 2u); }),
          foreach (item.arr, [&](auto const& idx, auto const& it) {
            return iff(idx == 0u, it == 1u) && if_then_else(idx == 0u, it == 1u, it >= 2u);
          }),
          foreach (item.arr, [&](auto const& it) {
            return inside(it, 1u, 2u, 4u) && unique(it, item.x()) && onehot(it) && onehot0(it) &&
                   countones(it) == 1u && clog2(it) <= 2u;
          }),
          foreach (item.arr, [&](auto const& idx, auto const&) {
            return item.arr[idx] >= item.arr[idx - 1u];
          }),
          foreach (item.arr, [&](auto const& it) { return !inside(item.x(), it); }),
      };
      BOOST_REQUIRE(item.randomize());
      BOOST_REQUIRE_EQUAL(item.arr[0], 1u);
      for (unsigned i = 1; i < 4; ++i) {
        BOOST_REQUIRE(item.arr[i] == 2u || item.arr[i] == 4u);
        BOOST_REQUIRE_GE(item.arr[i], item.arr[i - 1]);
      }
    } else {
      struct lp_cudd_item : public crv_sequence_item {
        lp_cudd_item(crv_object_name) {}
        crv_array<uint8_t, 2> arr{"arr"};
        crv_variable<uint8_t> x;
        crv_constraint con{"con"};
      } item("lp_arr_cudd");
      item.con = {
          item.x() == 8u,
          foreach (item.arr, [&](auto const& idx, auto const& it) { return if_then(idx > 0u, it >= 2u); }),
          foreach (item.arr, [&](auto const& idx, auto const& it) {
            return iff(idx == 0u, it == 1u) && if_then_else(idx == 0u, it == 1u, it >= 2u);
          }),
          foreach (item.arr, [&](auto const& it) {
            return inside(it, 1u, 2u, 4u) && unique(it, item.x()) && onehot(it) && onehot0(it) &&
                   countones(it) == 1u && clog2(it) <= 2u;
          }),
          foreach (item.arr, [&](auto const& idx, auto const&) {
            return item.arr[idx] >= item.arr[idx - 1u];
          }),
          foreach (item.arr, [&](auto const& it) { return !inside(item.x(), it); }),
      };
      BOOST_REQUIRE(item.randomize());
      BOOST_REQUIRE_EQUAL(item.arr[0], 1u);
      BOOST_REQUIRE(item.arr[1] == 2u || item.arr[1] == 4u);
      BOOST_REQUIRE_GE(item.arr[1], item.arr[0]);
    }

    // Vector lambda using v()[idx], v[idx], v()[idx - 1], if_then_else
    {
      lambda_proxy_item item("lp_vec");
      item.con = {
          item.vec().size() == 3u,
          foreach (item.vec(), [&](auto const& idx, auto const& it) {
            return if_then_else(idx == 0u, item.vec()[idx] == 1u, it < 10u);
          }),
          foreach (item.vec(), [&](auto const& idx, auto const& it) {
            return if_then(idx > 0u, item.vec[idx] == item.vec()[idx - 1u] + 1u) && it < 10u;
          }),
      };
      BOOST_REQUIRE(item.randomize());
      BOOST_REQUIRE_EQUAL(item.vec.size(), 3u);
      BOOST_REQUIRE_EQUAL(item.vec[0], 1u);
      BOOST_REQUIRE_EQUAL(item.vec[1], 2u);
      BOOST_REQUIRE_EQUAL(item.vec[2], 3u);

      // Negative UNSAT check: unguarded OOB inside && must fail UNSAT
      lambda_proxy_item item_unguarded_and("lp_vec_unguarded_and");
      item_unguarded_and.con = {
          item_unguarded_and.vec().size() == 3u,
          foreach (item_unguarded_and.vec(), [&](auto const& idx, auto const& it) {
            return item_unguarded_and.vec[idx] == item_unguarded_and.vec()[idx - 1u] + 1u && it < 10u;
          }),
      };
      BOOST_REQUIRE(!item_unguarded_and.randomize());
    }

    // ReplaceVisitor::visitBitslice preserves subscript stack and bit-slicing inside crv_array::foreach
    {
      struct bs_sub_item : public crv_sequence_item {
        bs_sub_item(crv_object_name) {}
        crv_variable<uint8_t> x;
        crv_array<uint8_t, 2> arr{"arr"};
        crv_array<uint8_t, 2> brr{"brr"};
        crv_constraint con{"con"};
      } item("bs_sub");
      item.con = {
          item.x() == 0x5Au,
          foreach (item.arr, item.brr[_i] == bitslice(1, 0, item.x())),
      };
      BOOST_REQUIRE(item.randomize());
      BOOST_REQUIRE_EQUAL(item.brr[0], 2u);
      BOOST_REQUIRE_EQUAL(item.brr[1], 2u);
    }
  }
}

BOOST_AUTO_TEST_CASE(symbolic_subscript_soundness) {
  struct Guard {
    ~Guard() {
      set_solver_backend("Z3");
      set_sv_signedness(false);
    }
  } guard;

  for (bool sv : {false, true}) {
    set_sv_signedness(sv);
    for (const char* backend : {"Z3", "Cudd"}) {
      if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
      set_solver_backend(backend);

      // Case 1 (SAT): arr[bitslice(1, 0, x())] == 22u when x == 2, arr[0] == 10, arr[2] == 22
      {
        struct sym_arr_item : public crv_sequence_item {
          sym_arr_item(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_array<uint8_t, 4> arr{"arr"};
          crv_constraint con{"con"};
        } item("c1_sat");
        item.con = {
            item.x() == 2u,
            item.arr[0]() == 10u,
            item.arr[2]() == 22u,
            foreach (item.arr, item.arr[bitslice(1, 0, item.x())] == 22u),
        };
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_EQUAL(item.arr[2], 22u);
      }

      // Case 2 (UNSAT): arr[bitslice(1, 0, x())] == 10u when x == 2, arr[0] == 10, arr[2] == 22
      {
        struct sym_arr_item : public crv_sequence_item {
          sym_arr_item(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_array<uint8_t, 4> arr{"arr"};
          crv_constraint con{"con"};
        } item("c2_unsat");
        item.con = {
            item.x() == 2u,
            item.arr[0]() == 10u,
            item.arr[2]() == 22u,
            foreach (item.arr, item.arr[bitslice(1, 0, item.x())] == 10u),
        };
        BOOST_REQUIRE(!item.randomize());
      }

      // Case 3 (SAT & UNSAT): v()[bitslice(1, 0, x())] == 22u / 10u when x == 2, v[0] == 10, v[2] == 22
      {
        struct sym_vec_item : public crv_sequence_item {
          sym_vec_item(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_vector<uint8_t> v{"v"};
          crv_constraint con{"con"};
        } item_sat("c3_sat"), item_unsat("c3_unsat");

        item_sat.con = {
            item_sat.x() == 2u,
            item_sat.v().size() == 4u,
            foreach (item_sat.v(), if_then(_i == 0u, item_sat.v()[_i] == 10u)),
            foreach (item_sat.v(), if_then(_i == 2u, item_sat.v()[_i] == 22u)),
            foreach (item_sat.v(), item_sat.v()[bitslice(1, 0, item_sat.x())] == 22u),
        };
        BOOST_REQUIRE(item_sat.randomize());
        BOOST_REQUIRE_EQUAL(item_sat.v[2], 22u);

        item_unsat.con = {
            item_unsat.x() == 2u,
            item_unsat.v().size() == 4u,
            foreach (item_unsat.v(), if_then(_i == 0u, item_unsat.v()[_i] == 10u)),
            foreach (item_unsat.v(), if_then(_i == 2u, item_unsat.v()[_i] == 22u)),
            foreach (item_unsat.v(), item_unsat.v()[bitslice(1, 0, item_unsat.x())] == 10u),
        };
        BOOST_REQUIRE(!item_unsat.randomize());
      }

      // Case 4 (SAT & UNSAT): arr[x()] == 22u / 10u when x == 2, arr[0] == 10, arr[2] == 22
      {
        struct sym_var_item : public crv_sequence_item {
          sym_var_item(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_array<uint8_t, 4> arr{"arr"};
          crv_constraint con{"con"};
        } item_sat("c4_sat"), item_unsat("c4_unsat"), item_oob("c4_oob"), item_unconstr("c4_unconstr");

        item_sat.con = {
            item_sat.x() == 2u,
            item_sat.arr[0]() == 10u,
            item_sat.arr[2]() == 22u,
            foreach (item_sat.arr, item_sat.arr[item_sat.x()] == 22u),
        };
        BOOST_REQUIRE(item_sat.randomize());

        item_unsat.con = {
            item_unsat.x() == 2u,
            item_unsat.arr[0]() == 10u,
            item_unsat.arr[2]() == 22u,
            foreach (item_unsat.arr, item_unsat.arr[item_unsat.x()] == 10u),
        };
        BOOST_REQUIRE(!item_unsat.randomize());

        item_oob.con = {
            item_oob.x() == 5u,
            item_oob.arr[0]() == 10u,
            foreach (item_oob.arr, item_oob.arr[item_oob.x()] == 10u),
        };
        BOOST_REQUIRE(!item_oob.randomize());

        item_unconstr.con = {
            foreach (item_unconstr.arr, item_unconstr.arr[_i] == 0u),
            foreach (item_unconstr.arr, item_unconstr.arr[item_unconstr.x()] == 10u),
        };
        BOOST_REQUIRE(!item_unconstr.randomize());
      }

      // Case 5 (SAT & UNSAT): arr.sum_with([&](auto, auto){ return arr[bitslice(1, 0, x())] == 10u; }) == 4u
      {
        struct sym_sum_item : public crv_sequence_item {
          sym_sum_item(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_array<uint8_t, 4> arr{"arr"};
          crv_constraint con{"con"};
        } item_unsat("c5_unsat"), item_sat("c5_sat");

        item_unsat.con = {
            item_unsat.x() == 2u,
            item_unsat.arr[0]() == 10u,
            item_unsat.arr[2]() == 22u,
            item_unsat.arr.sum_with([&](auto, auto) { return item_unsat.arr[bitslice(1, 0, item_unsat.x())] == 10u; }) ==
                4u,
        };
        BOOST_REQUIRE(!item_unsat.randomize());

        item_sat.con = {
            item_sat.x() == 0u,
            item_sat.arr[0]() == 10u,
            item_sat.arr[2]() == 22u,
            item_sat.arr.sum_with([&](auto, auto) { return item_sat.arr[bitslice(1, 0, item_sat.x())] == 10u; }) ==
                4u,
        };
        BOOST_REQUIRE(item_sat.randomize());
      }

      // Case 6 (UNSAT & SAT): foreach(arr, arr[arr[_i]] == 2u)
      {
        struct sym_nest_item : public crv_sequence_item {
          sym_nest_item(crv_object_name) {}
          crv_array<uint8_t, 3> arr{"arr"};
          crv_constraint con{"con"};
        } item_unsat("c6_unsat"), item_oob("c6_oob"), item_sat("c6_sat");

        item_unsat.con = {
            item_unsat.arr[0]() == 2u,
            item_unsat.arr[1]() == 2u,
            item_unsat.arr[2]() == 22u,
            foreach (item_unsat.arr, item_unsat.arr[item_unsat.arr[_i]] == 2u),
        };
        BOOST_REQUIRE(!item_unsat.randomize());

        item_oob.con = {
            item_oob.arr[0]() == 22u,
            item_oob.arr[1]() == 22u,
            item_oob.arr[2]() == 22u,
            foreach (item_oob.arr, item_oob.arr[item_oob.arr[_i]] == 2u),
        };
        BOOST_REQUIRE(!item_oob.randomize());

        item_sat.con = {
            item_sat.arr[0]() == 1u,
            item_sat.arr[1]() == 2u,
            foreach (item_sat.arr, item_sat.arr[item_sat.arr[_i]] == 2u),
        };
        BOOST_REQUIRE(item_sat.randomize());
        BOOST_REQUIRE_EQUAL(item_sat.arr[2], 2u);
      }

      // Signed subscript (negative OOB) and vector sum_with dynamic subscript
      {
        struct sym_signed_item : public crv_sequence_item {
          sym_signed_item(crv_object_name) {}
          crv_variable<int8_t> sx;
          crv_array<uint8_t, 3> arr{"arr"};
          crv_vector<uint8_t> vec{"vec"};
          crv_constraint con{"con"};
        } item_neg("signed_neg"), item_pos("signed_pos"), item_vec_sum("vec_sum");

        item_neg.con = {
            item_neg.sx() == int8_t(-1),
            item_neg.arr[0]() == 7u,
            foreach (item_neg.arr, item_neg.arr[item_neg.sx()] == 7u),
        };
        BOOST_REQUIRE(!item_neg.randomize());

        item_pos.con = {
            item_pos.sx() == int8_t(1),
            item_pos.arr[0]() == 7u,
            item_pos.arr[1]() == 9u,
            foreach (item_pos.arr, item_pos.arr[item_pos.sx()] == 9u),
        };
        BOOST_REQUIRE(item_pos.randomize());

        item_vec_sum.con = {
            item_vec_sum.sx() == int8_t(2),
            item_vec_sum.vec().size() == 3u,
            foreach (item_vec_sum.vec(), item_vec_sum.vec()[_i] == _i + 10u),
            item_vec_sum.vec().sum_with([&](auto, auto) { return item_vec_sum.vec()[item_vec_sum.sx()] == 12u; }) == 3u,
        };
        BOOST_REQUIRE(item_vec_sum.randomize());
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(dynamic_bounds_path_scoping_and_edge_cases) {
  struct Guard {
    ~Guard() {
      set_solver_backend("Z3");
      set_sv_signedness(false);
    }
  } guard;

  for (bool sv : {false, true}) {
    set_sv_signedness(sv);
    for (const char* backend : {"Z3", "Cudd"}) {
      if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
      set_solver_backend(backend);

      // 1. Guarded / conditional / disjunctive foreach (if_then, if_then_else, ||, &&, !)
      {
        struct guarded_item : public crv_sequence_item {
          guarded_item(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_variable<uint8_t> y;
          crv_array<uint8_t, 4> arr{"arr"};
          crv_constraint con{"con"};
        };

        // if_then inactive branch with OOB x == 5 -> SAT
        guarded_item it_sat("it_sat");
        it_sat.con = {it_sat.x() == 5u, it_sat.arr[0]() == 1u, it_sat.arr[1]() == 2u, it_sat.arr[2]() == 3u,
                      it_sat.arr[3]() == 4u,
                      foreach (it_sat.arr, if_then(it_sat.x() < 4u, it_sat.arr[it_sat.x()] == 10u))};
        BOOST_REQUIRE(it_sat.randomize());

        // if_then active branch contradicting arr[2] == 22 -> UNSAT
        guarded_item it_unsat("it_unsat");
        it_unsat.con = {it_unsat.x() == 2u, it_unsat.arr[0]() == 10u, it_unsat.arr[1]() == 10u,
                        it_unsat.arr[2]() == 22u, it_unsat.arr[3]() == 10u,
                        foreach (it_unsat.arr, if_then(it_unsat.x() < 4u, it_unsat.arr[it_unsat.x()] == 10u))};
        BOOST_REQUIRE(!it_unsat.randomize());

        // if_then_else taking false branch when x == 5 && y == 1 -> SAT; y == 0 -> UNSAT
        guarded_item ite_sat("ite_sat"), ite_unsat("ite_unsat");
        ite_sat.con = {ite_sat.x() == 5u, ite_sat.y() == 1u, ite_sat.arr[0]() == 10u, ite_sat.arr[1]() == 10u,
                       ite_sat.arr[2]() == 10u, ite_sat.arr[3]() == 10u,
                       foreach (ite_sat.arr,
                                if_then_else(ite_sat.x() < 4u, ite_sat.arr[ite_sat.x()] == 10u, ite_sat.y() == 1u))};
        BOOST_REQUIRE(ite_sat.randomize());
        ite_unsat.con = {
            ite_unsat.x() == 5u, ite_unsat.y() == 0u, ite_unsat.arr[0]() == 10u, ite_unsat.arr[1]() == 10u,
            ite_unsat.arr[2]() == 10u, ite_unsat.arr[3]() == 10u,
            foreach (ite_unsat.arr,
                     if_then_else(ite_unsat.x() < 4u, ite_unsat.arr[ite_unsat.x()] == 10u, ite_unsat.y() == 1u))};
        BOOST_REQUIRE(!ite_unsat.randomize());

        // LogicalOr short-circuit: (y == 1) || (arr[x] == 10) with y == 1, x == 5 -> SAT; y == 0, x == 5 -> UNSAT
        guarded_item or_sat("or_sat"), or_unsat("or_unsat");
        or_sat.con = {or_sat.x() == 5u, or_sat.y() == 1u, or_sat.arr[0]() == 10u, or_sat.arr[1]() == 10u,
                      or_sat.arr[2]() == 10u, or_sat.arr[3]() == 10u,
                      foreach (or_sat.arr, (or_sat.y() == 1u) || (or_sat.arr[or_sat.x()] == 10u))};
        BOOST_REQUIRE(or_sat.randomize());
        or_unsat.con = {or_unsat.x() == 5u, or_unsat.y() == 0u, or_unsat.arr[0]() == 10u, or_unsat.arr[1]() == 10u,
                        or_unsat.arr[2]() == 10u, or_unsat.arr[3]() == 10u,
                        foreach (or_unsat.arr, (or_unsat.y() == 1u) || (or_unsat.arr[or_unsat.x()] == 10u))};
        BOOST_REQUIRE(!or_unsat.randomize());

        // LogicalAnd short-circuit inside LogicalOr: ((x < 4) && (arr[x] == 10)) || (y == 1)
        guarded_item and_or_sat("and_or_sat"), and_or_unsat("and_or_unsat");
        and_or_sat.con = {
            and_or_sat.x() == 5u, and_or_sat.y() == 1u, and_or_sat.arr[0]() == 10u, and_or_sat.arr[1]() == 10u,
            and_or_sat.arr[2]() == 10u, and_or_sat.arr[3]() == 10u,
            foreach (and_or_sat.arr, ((and_or_sat.x() < 4u) && (and_or_sat.arr[and_or_sat.x()] == 10u)) ||
                                         (and_or_sat.y() == 1u))};
        BOOST_REQUIRE(and_or_sat.randomize());
        and_or_unsat.con = {
            and_or_unsat.x() == 5u, and_or_unsat.y() == 0u, and_or_unsat.arr[0]() == 10u, and_or_unsat.arr[1]() == 10u,
            and_or_unsat.arr[2]() == 10u, and_or_unsat.arr[3]() == 10u,
            foreach (and_or_unsat.arr, ((and_or_unsat.x() < 4u) && (and_or_unsat.arr[and_or_unsat.x()] == 10u)) ||
                                           (and_or_unsat.y() == 1u))};
        BOOST_REQUIRE(!and_or_unsat.randomize());

        // Negated foreach with OOB -> UNSAT; in-bounds mismatch -> SAT
        guarded_item not_oob("not_oob"), not_sat("not_sat");
        not_oob.con = {not_oob.x() == 5u, not_oob.arr[0]() == 20u, not_oob.arr[1]() == 20u,
                       not_oob.arr[2]() == 20u, not_oob.arr[3]() == 20u,
                       !foreach (not_oob.arr, not_oob.arr[not_oob.x()] == 10u)};
        BOOST_REQUIRE(!not_oob.randomize());
        not_sat.con = {not_sat.x() == 1u, not_sat.arr[0]() == 10u, not_sat.arr[1]() == 20u,
                       not_sat.arr[2]() == 10u, not_sat.arr[3]() == 10u,
                       !foreach (not_sat.arr, not_sat.arr[not_sat.x()] == 10u)};
        BOOST_REQUIRE(not_sat.randomize());
      }

      // Guarded crv_vector foreach
      {
        struct vec_guarded_item : public crv_sequence_item {
          vec_guarded_item(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_variable<uint8_t> y;
          crv_vector<uint8_t> v{"v"};
          crv_constraint con{"con"};
        } vsat("vsat"), vunsat("vunsat"), vor("vor");
        vsat.con = {vsat.v().size() == 3u, vsat.x() == 5u, foreach (vsat.v(), vsat.v()[_i] == 20u),
                    foreach (vsat.v(), if_then(vsat.x() < 3u, vsat.v()[vsat.x()] == 10u))};
        BOOST_REQUIRE(vsat.randomize());
        vunsat.con = {vunsat.v().size() == 3u, vunsat.x() == 5u, vunsat.y() == 0u,
                      foreach (vunsat.v(), vunsat.v()[_i] == 10u),
                      foreach (vunsat.v(), (vunsat.y() == 1u) || (vunsat.v()[vunsat.x()] == 10u))};
        BOOST_REQUIRE(!vunsat.randomize());
        vor.con = {vor.v().size() == 3u, vor.x() == 5u, vor.y() == 1u, foreach (vor.v(), vor.v()[_i] == 20u),
                   foreach (vor.v(), (vor.y() == 1u) || (vor.v()[vor.x()] == 10u))};
        BOOST_REQUIRE(vor.randomize());
      }

      // 2. Partial OOB in foreach & nested foreach (E1a-E1c, E5e)
      {
        struct part_arr_item : public crv_sequence_item {
          part_arr_item(crv_object_name) {}
          crv_array<uint8_t, 3> arr{"arr"};
          crv_array<uint8_t, 3> brr{"brr"};
          crv_constraint con{"con"};
        } e1a("e1a"), e1b("e1b");

        e1a.con = {e1a.arr[0]() == 22u, e1a.arr[1]() == 2u, e1a.arr[2]() == 2u,
                   foreach (e1a.arr, e1a.arr[e1a.arr[_i]] == 2u)};
        BOOST_REQUIRE(!e1a.randomize());

        e1b.con = {e1b.arr[0]() == 22u, e1b.arr[1]() == 0u, e1b.arr[2]() == 0u,
                   foreach (e1b.arr, if_then(_i == 0u, e1b.arr[e1b.arr[_i]] == 2u))};
        BOOST_REQUIRE(!e1b.randomize());

        struct part_vec_item : public crv_sequence_item {
          part_vec_item(crv_object_name) {}
          crv_vector<uint8_t> v{"v"};
          crv_constraint con{"con"};
        } e1c("e1c");
        e1c.con = {e1c.v().size() == 3u,
                   foreach (e1c.v(), if_then(_i == 0u, e1c.v()[_i] == 22u)),
                   foreach (e1c.v(), if_then(_i > 0u, e1c.v()[_i] == 2u)),
                   foreach (e1c.v(), e1c.v()[e1c.v()[_i]] == 2u)};
        BOOST_REQUIRE(!e1c.randomize());

        struct nest_oob_item : public crv_sequence_item {
          nest_oob_item(crv_object_name) {}
          crv_array<uint8_t, 2> arr{"arr"};
          crv_array<uint8_t, 3> brr{"brr"};
          crv_constraint con{"con"};
        } e5e("e5e");
        e5e.con = {e5e.arr[0]() == 5u, e5e.arr[1]() == 1u,
                   e5e.brr[0]() == 7u, e5e.brr[1]() == 7u, e5e.brr[2]() == 7u,
                   foreach (e5e.arr, [&](auto const& idx, auto const&) {
                     return foreach (e5e.brr, [&](auto const&, auto const&) {
                       return e5e.brr[e5e.arr[idx]] == 7u;
                     });
                   })};
        BOOST_REQUIRE(!e5e.randomize());
      }

      // 3. OOB reductions (sum_with, product_with, and_with, or_with, xor_with) on crv_array & crv_vector
      // (Split into its own test case below so CUDD stays well below timeout.)
    }
  }
}

BOOST_AUTO_TEST_CASE(dynamic_bounds_oob_reductions_and_folding) {
  struct Guard {
    ~Guard() {
      set_solver_backend("Z3");
      set_sv_signedness(false);
    }
  } guard;

  for (bool sv : {false, true}) {
    set_sv_signedness(sv);
    for (const char* backend : {"Z3", "Cudd"}) {
      if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
      set_solver_backend(backend);

      // OOB reductions (sum_with, product_with, and_with, or_with, xor_with) on crv_array & crv_vector
      {
        auto run_arr_red_oob = [&](auto& r, unsigned n_val) {
          using ItemT = typename std::decay<decltype(r)>::type;
          {
            ItemT t("arr_sum_bool_oob");
            t.con = {foreach (t.arr, t.arr[_i] == 10u),
                     t.arr.sum_with([&](auto, auto) { return t.arr[t.x()] == 10u; }) == 0u};
            BOOST_REQUIRE(!t.randomize());
          }
          {
            ItemT t("arr_sum_val_oob");
            t.con = {foreach (t.arr, t.arr[_i] == 10u),
                     t.arr.sum_with([&](auto, auto) { return t.arr[t.x()]; }) == 0u};
            BOOST_REQUIRE(!t.randomize());
          }
          {
            ItemT t("arr_prod_oob");
            t.con = {foreach (t.arr, t.arr[_i] == 10u),
                     t.arr.product_with([&](auto, auto) { return t.arr[t.x()]; }) == 1u};
            BOOST_REQUIRE(!t.randomize());
          }
          {
            ItemT t("arr_and_oob");
            t.con = {foreach (t.arr, t.arr[_i] == 10u),
                     t.arr.and_with([&](auto, auto) { return t.arr[t.x()] == 99u; })};
            BOOST_REQUIRE(!t.randomize());
          }
          {
            ItemT t("arr_or_oob");
            t.con = {foreach (t.arr, t.arr[_i] == 10u),
                     t.arr.or_with([&](auto, auto) { return t.arr[t.x()] == 10u; }) == false};
            BOOST_REQUIRE(!t.randomize());
          }
          {
            ItemT t("arr_xor_oob");
            t.con = {foreach (t.arr, t.arr[_i] == 10u), t.x() == 5u,
                     t.arr.xor_with([&](auto, auto) { return t.arr[t.x()] == 10u; }) == false};
            BOOST_REQUIRE(!t.randomize());
          }
          {
            ItemT t("arr_xor_unconstr_oob");
            t.con = {foreach (t.arr, t.arr[_i] == 10u),
                     t.arr.xor_with([&](auto, auto) { return t.arr[t.x()] == 10u; }) == true};
            BOOST_REQUIRE(!t.randomize());
          }
          {
            ItemT t("arr_guarded_red_sat");
            t.con = {foreach (t.arr, t.arr[_i] == 10u), t.x() == 5u,
                     if_then(t.x() < n_val, t.arr.sum_with([&](auto, auto) { return t.arr[t.x()] == 10u; }) == 0u)};
            BOOST_REQUIRE(t.randomize());
          }
        };

        if (std::string(backend) == "Z3") {
          struct red_oob_arr4 : public crv_sequence_item {
            red_oob_arr4(crv_object_name) {}
            crv_variable<uint8_t> x;
            crv_array<uint8_t, 4> arr{"arr"};
            crv_constraint con{"con"};
          } dummy("dummy4");
          run_arr_red_oob(dummy, 4u);
        } else {
          struct red_oob_arr2 : public crv_sequence_item {
            red_oob_arr2(crv_object_name) {}
            crv_variable<uint8_t> x;
            crv_array<uint8_t, 2> arr{"arr"};
            crv_constraint con{"con"};
          } dummy("dummy2");
          run_arr_red_oob(dummy, 2u);
        }

        unsigned const vsz = (std::string(backend) == "Z3") ? 4u : 2u;
        struct red_oob_vec : public crv_sequence_item {
          red_oob_vec(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_vector<uint8_t> v{"v"};
          crv_constraint con{"con"};
        };
        {
          red_oob_vec r("vec_sum_bool_oob");
          r.con = {r.v().size() == vsz, foreach (r.v(), r.v()[_i] == 10u),
                   r.v().sum_with([&](auto, auto) { return r.v()[r.x()] == 10u; }) == 0u};
          BOOST_REQUIRE(!r.randomize());
        }
        {
          red_oob_vec r("vec_sum_val_oob");
          r.con = {r.v().size() == vsz, foreach (r.v(), r.v()[_i] == 10u),
                   r.v().sum_with([&](auto, auto) { return r.v()[r.x()]; }) == 0u};
          BOOST_REQUIRE(!r.randomize());
        }
        {
          red_oob_vec r("vec_prod_oob");
          r.con = {r.v().size() == vsz, foreach (r.v(), r.v()[_i] == 10u),
                   r.v().product_with([&](auto, auto) { return r.v()[r.x()]; }) == 1u};
          BOOST_REQUIRE(!r.randomize());
        }
        {
          red_oob_vec r("vec_and_oob");
          r.con = {r.v().size() == vsz, foreach (r.v(), r.v()[_i] == 10u),
                   r.v().and_with([&](auto, auto) { return r.v()[r.x()] == 99u; })};
          BOOST_REQUIRE(!r.randomize());
        }
        {
          red_oob_vec r("vec_or_oob");
          r.con = {r.v().size() == vsz, foreach (r.v(), r.v()[_i] == 10u),
                   r.v().or_with([&](auto, auto) { return r.v()[r.x()] == 10u; }) == false};
          BOOST_REQUIRE(!r.randomize());
        }
        {
          red_oob_vec r("vec_xor_oob");
          r.con = {r.v().size() == vsz, foreach (r.v(), r.v()[_i] == 10u), r.x() == 5u,
                   r.v().xor_with([&](auto, auto) { return r.v()[r.x()] == 10u; }) == false};
          BOOST_REQUIRE(!r.randomize());
        }
        {
          red_oob_vec r("vec_guarded_red_sat");
          r.con = {r.v().size() == vsz, foreach (r.v(), r.v()[_i] == 10u), r.x() == 5u,
                   if_then(r.x() < vsz, r.v().sum_with([&](auto, auto) { return r.v()[r.x()] == 10u; }) == 0u)};
          BOOST_REQUIRE(r.randomize());
        }
      }

      // 4. Empty target array (n == 0)
      {
        struct empty_arr_item : public crv_sequence_item {
          empty_arr_item(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_array<uint8_t, 3> a{"a"};
          crv_array<uint8_t, 0> b{"b"};
          crv_constraint con{"con"};
        } e_dyn("e_dyn"), e_const("e_const"), e_guarded("e_guarded");
        e_dyn.con = {foreach (e_dyn.a, e_dyn.b[e_dyn.x()] == 10u)};
        BOOST_REQUIRE(!e_dyn.randomize());
        e_const.con = {foreach (e_const.a, e_const.b[value_to_expression(0u)] == 10u)};
        BOOST_REQUIRE(!e_const.randomize());
        e_guarded.con = {foreach (e_guarded.a, if_then(false, e_guarded.b[e_guarded.x()] == 10u))};
        BOOST_REQUIRE(e_guarded.randomize());
      }

      // 5. Constant(bool) folding (true -> +1, not -1)
      {
        struct bool_fold_item : public crv_sequence_item {
          bool_fold_item(crv_object_name) {}
          crv_array<uint8_t, 3> arr{"arr"};
          crv_constraint con{"con"};
        } bf("bf");
        bf.con = {
            bf.arr[0]() == 11u,
            bf.arr[1]() == 55u,
            bf.arr[2]() == 66u,
            foreach (bf.arr, if_then(_i == 0u, bf.arr[_i + value_to_expression(true)] == 55u)),
            foreach (bf.arr, bf.arr[value_to_expression(true) + 1u] == 66u),
        };
        BOOST_REQUIRE(bf.randomize());
      }

      // 6. SubscriptVal unsigned wrap & logical shift (E6)
      {
        struct wrap_item : public crv_sequence_item {
          wrap_item(crv_object_name) {}
          crv_array<uint8_t, 3> arr{"arr"};
          crv_constraint con{"con"};
        } e6("e6");
        e6.con = {
            e6.arr[0]() == 20u,
            e6.arr[1]() == 10u,
            e6.arr[2]() == 30u,
            foreach (e6.arr, if_then(_i == 0u, e6.arr[if_then_else(_i - 1u < 2u, 0u, 1u)] == 10u)),
            foreach (e6.arr, if_then(_i == 0u, e6.arr[if_then_else(((_i - 1u) >> 31u) == 1u, 1u, 0u)] == 10u)),
            foreach (e6.arr, if_then(_i == 0u, e6.arr[(~value_to_expression(uint8_t(128))) - 126u] == 10u)),
        };
        BOOST_REQUIRE(e6.randomize());
      }

      // 7. Boolean-to-int promotion signedness (E9) & mixed bool/int IfThenElse (E10)
      {
        if (std::string(backend) == "Z3") {
          struct e9_item : public crv_sequence_item {
            e9_item(crv_object_name) {}
            crv_variable<uint8_t> x;
            crv_array<uint8_t, 4> arr{"arr"};
            crv_constraint con{"con"};
          } e9_oob("e9_oob"), e9_ok("e9_ok");
          e9_oob.con = {
              e9_oob.x() == 0u,
              e9_oob.arr[0]() == 10u, e9_oob.arr[1]() == 20u, e9_oob.arr[2]() == 30u, e9_oob.arr[3]() == 40u,
              foreach (e9_oob.arr, e9_oob.arr[(e9_oob.x() == 2u) - 1u] == 40u),
          };
          BOOST_REQUIRE(!e9_oob.randomize());
          e9_ok.con = {
              e9_ok.x() == 2u,
              e9_ok.arr[0]() == 10u, e9_ok.arr[1]() == 20u, e9_ok.arr[2]() == 30u, e9_ok.arr[3]() == 40u,
              foreach (e9_ok.arr, e9_ok.arr[(e9_ok.x() == 2u) - 1u] == 10u),
          };
          BOOST_REQUIRE(e9_ok.randomize());
        } else {
          struct e9_cudd_item : public crv_sequence_item {
            e9_cudd_item(crv_object_name) {}
            crv_variable<uint8_t> x;
            crv_array<uint8_t, 2> arr{"arr"};
            crv_constraint con{"con"};
          } e9_oob("e9_c_oob"), e9_ok("e9_c_ok");
          e9_oob.con = {
              e9_oob.x() == 0u,
              e9_oob.arr[0]() == 10u, e9_oob.arr[1]() == 40u,
              foreach (e9_oob.arr, if_then(_i == 0u, e9_oob.arr[(e9_oob.x() == 2u) - 1u] == 40u)),
          };
          BOOST_REQUIRE(!e9_oob.randomize());
          e9_ok.con = {
              e9_ok.x() == 2u,
              e9_ok.arr[0]() == 10u, e9_ok.arr[1]() == 40u,
              foreach (e9_ok.arr, if_then(_i == 0u, e9_ok.arr[(e9_ok.x() == 2u) - 1u] == 10u)),
          };
          BOOST_REQUIRE(e9_ok.randomize());
        }

        if (std::string(backend) == "Z3") {
          struct e10_item : public crv_sequence_item {
            e10_item(crv_object_name) {}
            crv_variable<bool> cond;
            crv_variable<bool> b1;
            crv_variable<uint8_t> x;
            crv_array<uint8_t, 4> arr{"arr"};
            crv_constraint con{"con"};
          } e10_sat("e10_sat"), e10_unsat("e10_unsat"), e10_oob("e10_oob");

          e10_sat.con = {
              e10_sat.cond() == false, e10_sat.b1() == true, e10_sat.x() == 2u,
              e10_sat.arr[0]() == 10u, e10_sat.arr[1]() == 20u, e10_sat.arr[2]() == 30u, e10_sat.arr[3]() == 40u,
              foreach (e10_sat.arr, e10_sat.arr[if_then_else(e10_sat.cond(), e10_sat.b1(), e10_sat.x())] == 30u),
          };
          BOOST_REQUIRE(e10_sat.randomize());

          e10_unsat.con = {
              e10_unsat.cond() == false, e10_unsat.b1() == true, e10_unsat.x() == 2u,
              e10_unsat.arr[0]() == 10u, e10_unsat.arr[1]() == 20u, e10_unsat.arr[2]() == 30u, e10_unsat.arr[3]() == 40u,
              foreach (e10_unsat.arr, e10_unsat.arr[if_then_else(e10_unsat.cond(), e10_unsat.b1(), e10_unsat.x())] == 20u),
          };
          BOOST_REQUIRE(!e10_unsat.randomize());

          e10_oob.con = {
              e10_oob.cond() == false, e10_oob.b1() == true, e10_oob.x() == 5u,
              e10_oob.arr[0]() == 10u, e10_oob.arr[1]() == 20u, e10_oob.arr[2]() == 30u, e10_oob.arr[3]() == 40u,
              foreach (e10_oob.arr, e10_oob.arr[if_then_else(e10_oob.cond(), e10_oob.b1(), e10_oob.x())] == 20u),
          };
          BOOST_REQUIRE(!e10_oob.randomize());
        } else {
          struct e10_cudd_item : public crv_sequence_item {
            e10_cudd_item(crv_object_name) {}
            crv_variable<bool> cond;
            crv_variable<bool> b1;
            crv_variable<uint8_t> x;
            crv_array<uint8_t, 3> arr{"arr"};
            crv_constraint con{"con"};
          } e10_sat("e10_c_sat"), e10_unsat("e10_c_unsat"), e10_oob("e10_c_oob");

          e10_sat.con = {
              e10_sat.cond() == false, e10_sat.b1() == true, e10_sat.x() == 2u,
              e10_sat.arr[0]() == 10u, e10_sat.arr[1]() == 20u, e10_sat.arr[2]() == 30u,
              foreach (e10_sat.arr,
                       if_then(_i == 0u, e10_sat.arr[if_then_else(e10_sat.cond(), e10_sat.b1(), e10_sat.x())] == 30u)),
          };
          BOOST_REQUIRE(e10_sat.randomize());

          e10_unsat.con = {
              e10_unsat.cond() == false, e10_unsat.b1() == true, e10_unsat.x() == 2u,
              e10_unsat.arr[0]() == 10u, e10_unsat.arr[1]() == 20u, e10_unsat.arr[2]() == 30u,
              foreach (e10_unsat.arr,
                       if_then(_i == 0u,
                               e10_unsat.arr[if_then_else(e10_unsat.cond(), e10_unsat.b1(), e10_unsat.x())] == 20u)),
          };
          BOOST_REQUIRE(!e10_unsat.randomize());

          e10_oob.con = {
              e10_oob.cond() == false, e10_oob.b1() == true, e10_oob.x() == 5u,
              e10_oob.arr[0]() == 10u, e10_oob.arr[1]() == 20u, e10_oob.arr[2]() == 30u,
              foreach (e10_oob.arr,
                       if_then(_i == 0u, e10_oob.arr[if_then_else(e10_oob.cond(), e10_oob.b1(), e10_oob.x())] == 20u)),
          };
          BOOST_REQUIRE(!e10_oob.randomize());
        }
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(path_scoped_placeholder_oob_and_bdd_bounds_check) {
  struct Guard {
    ~Guard() {
      set_solver_backend("Z3");
      set_sv_signedness(false);
    }
  } guard;

  for (bool sv : {false, true}) {
    set_sv_signedness(sv);
    for (const char* backend : {"Z3", "Cudd"}) {
      if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
      set_solver_backend(backend);

      // S1: foreach(arr, if_then(_i == 2u, arr[_i + 1u] == 10u)) -> UNSAT
      {
        struct s1_item : public crv_sequence_item {
          s1_item(crv_object_name) {}
          crv_array<uint8_t, 3> arr{"arr"};
          crv_constraint con{"con"};
        } item("s1");
        item.con = {foreach (item.arr, if_then(_i == 2u, item.arr[_i + 1u] == 10u))};
        BOOST_REQUIRE(!item.randomize());
      }

      // S2: foreach(v(), if_then(_i == 2u, v()[_i + 1u] == 10u)) -> UNSAT
      {
        struct s2_item : public crv_sequence_item {
          s2_item(crv_object_name) {}
          crv_vector<uint8_t> v{"v"};
          crv_constraint con{"con"};
        } item("s2");
        item.con = {item.v().size() == 3u, foreach (item.v(), if_then(_i == 2u, item.v()[_i + 1u] == 10u))};
        BOOST_REQUIRE(!item.randomize());
      }

      // S3: foreach(arr, (_i < 2u) || (arr[_i + 1u] == 10u)) -> UNSAT
      {
        struct s3_item : public crv_sequence_item {
          s3_item(crv_object_name) {}
          crv_array<uint8_t, 3> arr{"arr"};
          crv_constraint con{"con"};
        } item("s3");
        item.con = {foreach (item.arr, (_i < 2u) || (item.arr[_i + 1u] == 10u))};
        BOOST_REQUIRE(!item.randomize());
      }

      // S4: arr.sum_with(if_then_else(i == 2u, arr[i + 1u], uint8_t(5u))) == 10u -> UNSAT
      {
        struct s4_item : public crv_sequence_item {
          s4_item(crv_object_name) {}
          crv_array<uint8_t, 3> arr{"arr"};
          crv_constraint con{"con"};
        } item("s4");
        item.con = {item.arr.sum_with([&](auto const& i, auto const&) {
                      return if_then_else(i == 2u, item.arr[i + 1u], uint8_t(5u));
                    }) == 10u};
        BOOST_REQUIRE(!item.randomize());
      }

      // S5: v().sum_with(v()[i + 1u]) == 10u with v = {5, 10} -> UNSAT
      {
        struct s5_item : public crv_sequence_item {
          s5_item(crv_object_name) {}
          crv_vector<uint8_t> v{"v"};
          crv_constraint con{"con"};
        } item("s5");
        item.con = {item.v().size() == 2u,
                    foreach (item.v(), if_then_else(_i == 0u, item.v()[_i] == 5u, item.v()[_i] == 10u)),
                    item.v().sum_with([&](auto const& i, auto const&) { return item.v()[i + 1u]; }) == 10u};
        BOOST_REQUIRE(!item.randomize());
      }

      // S6: nested if_then(i == 1u, v().sum_with(v()[i + j]) == 4) -> UNSAT
      {
        struct s6_item : public crv_sequence_item {
          s6_item(crv_object_name) {}
          crv_vector<uint8_t> v{"v"};
          crv_constraint con{"con"};
        } item("s6");
        item.con = {item.v().size() == 2u,
                    foreach (item.v(), if_then_else(_i == 0u, item.v()[_i] == 1u, item.v()[_i] == 4u)),
                    foreach (item.v(), [&](auto const& i, auto const&) {
                      return if_then(i == 1u,
                                     item.v().sum_with([&](auto const& j, auto const&) { return item.v()[i + j]; }) ==
                                         4u);
                    })};
        BOOST_REQUIRE(!item.randomize());
      }

      // S10: if_then_else(_i > 0u, arr[_i] > arr[_i - 1u], arr[0]() == 99u) with arr={1,2,3} -> UNSAT (else enforced!)
      {
        struct s10_item : public crv_sequence_item {
          s10_item(crv_object_name) {}
          crv_array<uint8_t, 3> arr{"arr"};
          crv_constraint con{"con"};
        } item_unsat("s10_unsat"), item_sat("s10_sat");
        item_unsat.con = {item_unsat.arr[0]() == 1u, item_unsat.arr[1]() == 2u, item_unsat.arr[2]() == 3u,
                          foreach (item_unsat.arr, if_then_else(_i > 0u, item_unsat.arr[_i] > item_unsat.arr[_i - 1u],
                                                                item_unsat.arr[0]() == 99u))};
        BOOST_REQUIRE(!item_unsat.randomize());

        item_sat.con = {item_sat.arr[0]() == 1u, item_sat.arr[1]() == 2u, item_sat.arr[2]() == 3u,
                        foreach (item_sat.arr, if_then_else(_i > 0u, item_sat.arr[_i] > item_sat.arr[_i - 1u],
                                                            item_sat.arr[0]() == 1u))};
        BOOST_REQUIRE(item_sat.randomize());
      }

      // S11a/S11b: arr.sum_with(if_then_else(i > 0u, arr[i - 1u], uint8_t(5u))) == 15 (not 10!)
      {
        struct s11_arr_item : public crv_sequence_item {
          s11_arr_item(crv_object_name) {}
          crv_array<uint8_t, 3> arr{"arr"};
          crv_constraint con{"con"};
        } a_10("s11a"), a_15("s11b");
        a_10.con = {foreach (a_10.arr, a_10.arr[_i] == 5u),
                    a_10.arr.sum_with([&](auto const& i, auto const&) {
                      return if_then_else(i > 0u, a_10.arr[i - 1u], value_to_expression(uint8_t(5u)));
                    }) == 10u};
        BOOST_REQUIRE(!a_10.randomize());
        a_15.con = {foreach (a_15.arr, a_15.arr[_i] == 5u),
                    a_15.arr.sum_with([&](auto const& i, auto const&) {
                      return if_then_else(i > 0u, a_15.arr[i - 1u], value_to_expression(uint8_t(5u)));
                    }) == 15u};
        BOOST_REQUIRE(a_15.randomize());
      }

      // S11v: v().sum_with(if_then_else(i > 0u, v()[i - 1u], uint8_t(3u))) == 9 (not 6!)
      {
        struct s11_vec_item : public crv_sequence_item {
          s11_vec_item(crv_object_name) {}
          crv_vector<uint8_t> v{"v"};
          crv_constraint con{"con"};
        } v_6("s11v_6"), v_9("s11v_9");
        v_6.con = {v_6.v().size() == 3u, foreach (v_6.v(), v_6.v()[_i] == 3u),
                   v_6.v().sum_with([&](auto const& i, auto const&) {
                     return if_then_else(i > 0u, v_6.v()[i - 1u], value_to_expression(uint8_t(3u)));
                   }) == 6u};
        BOOST_REQUIRE(!v_6.randomize());
        v_9.con = {v_9.v().size() == 3u, foreach (v_9.v(), v_9.v()[_i] == 3u),
                   v_9.v().sum_with([&](auto const& i, auto const&) {
                     return if_then_else(i > 0u, v_9.v()[i - 1u], value_to_expression(uint8_t(3u)));
                   }) == 9u};
        BOOST_REQUIRE(v_9.randomize());
      }

      // Challenger 2 if_then_else cases on both crv_array and crv_vector
      {
        struct ch2_arr_item : public crv_sequence_item {
          ch2_arr_item(crv_object_name) {}
          crv_variable<uint8_t> y;
          crv_array<uint8_t, 3> arr{"arr"};
          crv_constraint con{"con"};
        } c_unsat("ch2_a_unsat"), c_sat("ch2_a_sat"), c_sum("ch2_a_sum");
        c_unsat.con = {c_unsat.y() == 0u, foreach (c_unsat.arr, c_unsat.arr[_i] == 10u),
                       foreach (c_unsat.arr, if_then_else(_i > 0u, c_unsat.arr[_i - 1u] == 10u, c_unsat.y() == 1u))};
        BOOST_REQUIRE(!c_unsat.randomize());
        c_sat.con = {c_sat.y() == 1u, foreach (c_sat.arr, c_sat.arr[_i] == 10u),
                     foreach (c_sat.arr, if_then_else(_i > 0u, c_sat.arr[_i - 1u] == 10u, c_sat.y() == 1u))};
        BOOST_REQUIRE(c_sat.randomize());
        c_sum.con = {foreach (c_sum.arr, c_sum.arr[_i] == 10u),
                     c_sum.arr.sum_with([&](auto idx, auto) {
                       return if_then_else(idx > 0u, c_sum.arr[idx - 1u], uint8_t(100u));
                     }) == 120u};
        BOOST_REQUIRE(c_sum.randomize());
      }
      {
        struct ch2_vec_item : public crv_sequence_item {
          ch2_vec_item(crv_object_name) {}
          crv_variable<uint8_t> y;
          crv_vector<uint8_t> v{"v"};
          crv_constraint con{"con"};
        } c_unsat("ch2_v_unsat"), c_sat("ch2_v_sat"), c_sum("ch2_v_sum");
        c_unsat.con = {c_unsat.v().size() == 3u, c_unsat.y() == 0u, foreach (c_unsat.v(), c_unsat.v()[_i] == 10u),
                       foreach (c_unsat.v(), if_then_else(_i > 0u, c_unsat.v()[_i - 1u] == 10u, c_unsat.y() == 1u))};
        BOOST_REQUIRE(!c_unsat.randomize());
        c_sat.con = {c_sat.v().size() == 3u, c_sat.y() == 1u, foreach (c_sat.v(), c_sat.v()[_i] == 10u),
                     foreach (c_sat.v(), if_then_else(_i > 0u, c_sat.v()[_i - 1u] == 10u, c_sat.y() == 1u))};
        BOOST_REQUIRE(c_sat.randomize());
        c_sum.con = {c_sum.v().size() == 3u, foreach (c_sum.v(), c_sum.v()[_i] == 10u),
                     c_sum.v().sum_with([&](auto idx, auto) {
                       return if_then_else(idx > 0u, c_sum.v()[idx - 1u], uint8_t(100u));
                     }) == 120u};
        BOOST_REQUIRE(c_sum.randomize());
      }

      // product_with, and_with, or_with, xor_with with guarded if_then_else(_i > 0u, ...)
      {
        struct other_reds_item : public crv_sequence_item {
          other_reds_item(crv_object_name) {}
          crv_array<uint8_t, 3> arr{"arr"};
          crv_vector<uint8_t> v{"v"};
          crv_constraint con{"con"};
        } item("other_reds");
        item.con = {
            foreach (item.arr, item.arr[_i] == 2u),
            item.v().size() == 3u,
            foreach (item.v(), item.v()[_i] == 2u),
            item.arr.product_with([&](auto const& idx, auto const&) {
              return if_then_else(idx > 0u, item.arr[idx - 1u], uint8_t(5u));
            }) == 20u,
            item.v().product_with([&](auto const& idx, auto const&) {
              return if_then_else(idx > 0u, item.v()[idx - 1u], uint8_t(5u));
            }) == 20u,
            item.arr.and_with([&](auto const& idx, auto const&) {
              return if_then_else(idx > 0u, item.arr[idx - 1u] == 2u, true);
            }),
            item.v().and_with([&](auto const& idx, auto const&) {
              return if_then_else(idx > 0u, item.v()[idx - 1u] == 2u, true);
            }),
            item.arr.or_with([&](auto const& idx, auto const&) {
              return if_then_else(idx > 0u, item.arr[idx - 1u] == 99u, true);
            }),
            item.v().or_with([&](auto const& idx, auto const&) {
              return if_then_else(idx > 0u, item.v()[idx - 1u] == 99u, true);
            }),
            item.arr.xor_with([&](auto const& idx, auto const&) {
              return if_then_else(idx > 0u, item.arr[idx - 1u] == 2u, true);
            }) == true,
            item.v().xor_with([&](auto const& idx, auto const&) {
              return if_then_else(idx > 0u, item.v()[idx - 1u] == 2u, true);
            }) == true,
        };
        BOOST_REQUIRE(item.randomize());
      }

      // Dynamic condition with placeholder OOB in one branch: active branch succeeds, OOB branch fails
      {
        struct dyn_ite_item : public crv_sequence_item {
          dyn_ite_item(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_array<uint8_t, 2> arr{"arr"};
          crv_constraint con{"con"};
        } ok("dyn_ok"), bad("dyn_bad");
        ok.con = {ok.x() == 0u, foreach (ok.arr, ok.arr[_i] == 10u),
                  foreach (ok.arr, if_then_else(ok.x() > 0u, ok.arr[_i - 1u] == 10u, ok.arr[_i] == 10u))};
        BOOST_REQUIRE(ok.randomize());
        bad.con = {bad.x() == 1u, foreach (bad.arr, bad.arr[_i] == 10u),
                   foreach (bad.arr, if_then_else(bad.x() > 0u, bad.arr[_i - 1u] == 10u, bad.arr[_i] == 10u))};
        BOOST_REQUIRE(!bad.randomize());
      }

      // A1: && LHS must not drop dynamic bounds guard (arr[x()] with x == 5 on size-1 array/vector)
      {
        struct a1_arr_item : public crv_sequence_item {
          a1_arr_item(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_array<uint8_t, 1> arr{"arr"};
          crv_constraint con{"con"};
        } a1_arr("a1_arr");
        a1_arr.con = {a1_arr.x() == 5u, a1_arr.arr[0]() == 10u,
                      foreach (a1_arr.arr, (a1_arr.arr[_i - 1u] + a1_arr.arr[a1_arr.x()] == 10u) &&
                                               (a1_arr.arr[_i] == 10u))};
        BOOST_REQUIRE(!a1_arr.randomize());

        struct a1_vec_item : public crv_sequence_item {
          a1_vec_item(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_vector<uint8_t> v{"v"};
          crv_constraint con{"con"};
        } a1_vec("a1_vec");
        a1_vec.con = {a1_vec.x() == 5u, a1_vec.v().size() == 1u, foreach (a1_vec.v(), a1_vec.v()[_i] == 10u),
                      foreach (a1_vec.v(), (a1_vec.v()[_i - 1u] + a1_vec.v()[a1_vec.x()] == 10u) &&
                                               (a1_vec.v()[_i] == 10u))};
        BOOST_REQUIRE(!a1_vec.randomize());
      }

      // A2: && commutativity with unguarded OOB (both LHS and RHS must fail UNSAT)
      {
        struct a2_item : public crv_sequence_item {
          a2_item(crv_object_name) {}
          crv_array<uint8_t, 2> arr{"arr"};
          crv_constraint con{"con"};
        } lhs_oob("a2_lhs"), rhs_oob("a2_rhs");
        lhs_oob.con = {foreach (lhs_oob.arr, lhs_oob.arr[_i] == 10u),
                       foreach (lhs_oob.arr, (lhs_oob.arr[_i - 1u] == 10u) && (lhs_oob.arr[_i] == 10u))};
        rhs_oob.con = {foreach (rhs_oob.arr, rhs_oob.arr[_i] == 10u),
                       foreach (rhs_oob.arr, (rhs_oob.arr[_i] == 10u) && (rhs_oob.arr[_i - 1u] == 10u))};
        BOOST_REQUIRE(!lhs_oob.randomize());
        BOOST_REQUIRE(!rhs_oob.randomize());

        struct a2_s1_item : public crv_sequence_item {
          a2_s1_item(crv_object_name) {}
          crv_array<uint8_t, 1> arr{"arr"};
          crv_constraint con{"con"};
        } s1("a2_s1");
        s1.con = {foreach (s1.arr, (s1.arr[_i - 1u] == 10u) && (_i == 0u))};
        BOOST_REQUIRE(!s1.randomize());
      }

      // A3: bool array/vector comparisons against Constant/VariableExpr must fail UNSAT on OOB
      // while VectorAccess vs VectorAccess (barr[_i] != barr[_i - 1u]) remains SAT
      {
        struct a3_arr_item : public crv_sequence_item {
          a3_arr_item(crv_object_name) {}
          crv_variable<bool> b;
          crv_array<bool, 3> barr{"barr"};
          crv_constraint con{"con"};
        } a_c("a3_ac"), a_t("a3_at"), a_v("a3_av"), a_legacy("a3_aleg");
        a_c.con = {foreach (a_c.barr, a_c.barr[_i + 1u] != false)};
        BOOST_REQUIRE(!a_c.randomize());
        a_t.con = {foreach (a_t.barr, a_t.barr[_i - 1u] == true)};
        BOOST_REQUIRE(!a_t.randomize());
        a_v.con = {a_v.b() == false, foreach (a_v.barr, a_v.barr[_i + 1u] != a_v.b())};
        BOOST_REQUIRE(!a_v.randomize());
        a_legacy.con = {foreach (a_legacy.barr, a_legacy.barr[_i] != a_legacy.barr[_i - 1u])};
        BOOST_REQUIRE(a_legacy.randomize());
        for (unsigned i = 1; i < 3; ++i) BOOST_REQUIRE_NE(a_legacy.barr[i], a_legacy.barr[i - 1]);

        struct a3_vec_item : public crv_sequence_item {
          a3_vec_item(crv_object_name) {}
          crv_variable<bool> b;
          crv_vector<bool> barr{"barr"};
          crv_constraint con{"con"};
        } v_c("a3_vc"), v_t("a3_vt"), v_v("a3_vv");
        v_c.con = {v_c.barr().size() == 3u, foreach (v_c.barr(), v_c.barr()[_i + 1u] != false)};
        BOOST_REQUIRE(!v_c.randomize());
        v_t.con = {v_t.barr().size() == 3u, foreach (v_t.barr(), v_t.barr()[_i - 1u] == true)};
        BOOST_REQUIRE(!v_t.randomize());
        v_v.con = {v_v.barr().size() == 3u, v_v.b() == false, foreach (v_v.barr(), v_v.barr()[_i + 1u] != v_v.b())};
        BOOST_REQUIRE(!v_v.randomize());
      }

      // Reviewer 2 Minor 2: arithmetic / bv operators wrapping boolean subexpressions inside top-level foreach
      {
        struct wrap_item : public crv_sequence_item {
          wrap_item(crv_object_name) {}
          crv_array<uint8_t, 2> arr{"arr"};
          crv_constraint con{"con"};
        } w1("w1"), w2("w2"), w3("w3"), w4("w4");
        w1.con = {foreach (w1.arr, w1.arr[_i] == 10u), foreach (w1.arr, onehot(w1.arr[_i - 1u] == 10u))};
        BOOST_REQUIRE(!w1.randomize());
        w2.con = {foreach (w2.arr, w2.arr[_i] == 10u),
                  foreach (w2.arr, (w2.arr[_i - 1u] == 10u) + uint8_t(1u) == 2u)};
        BOOST_REQUIRE(!w2.randomize());
        w3.con = {foreach (w3.arr, w3.arr[_i] == 10u),
                  foreach (w3.arr, countones(w3.arr[_i - 1u] == 10u) == 1u)};
        BOOST_REQUIRE(!w3.randomize());
        w4.con = {foreach (w4.arr, w4.arr[_i] == 10u),
                  foreach (w4.arr, ((w4.arr[_i - 1u] == 10u) + uint8_t(1u)) == w4.arr[_i - 1u])};
        BOOST_REQUIRE(!w4.randomize());
      }

      // Finding 1: Unconditional ScopedSkip(..., false) on all boolean-producing visitors
      {
        struct f1_arr_item : public crv_sequence_item {
          f1_arr_item(crv_object_name) {}
          crv_array<uint8_t, 2> arr{"arr"};
          crv_constraint con{"con"};
        } f1_oh("f1_oh"), f1_oh0("f1_oh0"), f1_in("f1_in"), f1_rng("f1_rng"), f1_rng_b("f1_rng_b"),
            f1_pm("f1_pm"), f1_pm_b("f1_pm_b"), f1_pct("f1_pct"), f1_pct_b("f1_pct_b"), f1_sat("f1_sat");
        f1_oh.con = {foreach (f1_oh.arr, f1_oh.arr[_i] == 1u),
                     foreach (f1_oh.arr, onehot(f1_oh.arr[_i + 1u]))};
        BOOST_REQUIRE(!f1_oh.randomize());
        f1_oh0.con = {foreach (f1_oh0.arr, f1_oh0.arr[_i] == 1u),
                      foreach (f1_oh0.arr, onehot0(f1_oh0.arr[_i + 1u]))};
        BOOST_REQUIRE(!f1_oh0.randomize());
        f1_in.con = {foreach (f1_in.arr, f1_in.arr[_i] == 1u),
                     foreach (f1_in.arr, inside(f1_in.arr[_i + 1u], 1u, 2u))};
        BOOST_REQUIRE(!f1_in.randomize());
        f1_rng.con = {foreach (f1_rng.arr, f1_rng.arr[_i] == 1u),
                      foreach (f1_rng.arr, inside(f1_rng.arr[_i + 1u], range(uint8_t(1u), uint8_t(2u))))};
        BOOST_REQUIRE(!f1_rng.randomize());
        f1_rng_b.con = {foreach (f1_rng_b.arr, f1_rng_b.arr[_i] == 1u),
                        foreach (f1_rng_b.arr, inside(f1_rng_b.arr[_i], range(f1_rng_b.arr[_i + 1u], uint8_t(2u))))};
        BOOST_REQUIRE(!f1_rng_b.randomize());
        f1_pm.con = {foreach (f1_pm.arr, f1_pm.arr[_i] == 5u),
                     foreach (f1_pm.arr, inside(f1_pm.arr[_i + 1u], tol_plusminus(uint8_t(5u), uint8_t(1u))))};
        BOOST_REQUIRE(!f1_pm.randomize());
        f1_pm_b.con = {foreach (f1_pm_b.arr, f1_pm_b.arr[_i] == 5u),
                       foreach (f1_pm_b.arr, inside(f1_pm_b.arr[_i], tol_plusminus(f1_pm_b.arr[_i + 1u], uint8_t(1u))))};
        BOOST_REQUIRE(!f1_pm_b.randomize());
        f1_pct.con = {foreach (f1_pct.arr, f1_pct.arr[_i] == 100u),
                      foreach (f1_pct.arr, inside(f1_pct.arr[_i + 1u], tol_pluspct(uint8_t(100u), uint8_t(10u))))};
        BOOST_REQUIRE(!f1_pct.randomize());
        f1_pct_b.con = {foreach (f1_pct_b.arr, f1_pct_b.arr[_i] == 100u),
                        foreach (f1_pct_b.arr, inside(f1_pct_b.arr[_i], tol_pluspct(uint8_t(100u), f1_pct_b.arr[_i + 1u])))};
        BOOST_REQUIRE(!f1_pct_b.randomize());
        f1_sat.con = {foreach (f1_sat.arr, f1_sat.arr[_i] == 2u),
                      foreach (f1_sat.arr, onehot(f1_sat.arr[_i])),
                      foreach (f1_sat.arr, inside(f1_sat.arr[_i], range(uint8_t(1u), uint8_t(4u))))};
        BOOST_REQUIRE(f1_sat.randomize());

        struct f1_vec_item : public crv_sequence_item {
          f1_vec_item(crv_object_name) {}
          crv_vector<uint8_t> v{"v"};
          crv_constraint con{"con"};
        } v_oh("f1_voh"), v_oh0("f1_voh0"), v_in("f1_vin"), v_vc("f1_vvc");
        v_oh.con = {v_oh.v().size() == 2u, foreach (v_oh.v(), v_oh.v()[_i] == 1u),
                    foreach (v_oh.v(), onehot(v_oh.v()[_i + 1u]))};
        BOOST_REQUIRE(!v_oh.randomize());
        v_oh0.con = {v_oh0.v().size() == 2u, foreach (v_oh0.v(), v_oh0.v()[_i] == 1u),
                     foreach (v_oh0.v(), onehot0(v_oh0.v()[_i + 1u]))};
        BOOST_REQUIRE(!v_oh0.randomize());
        v_in.con = {v_in.v().size() == 2u, foreach (v_in.v(), v_in.v()[_i] == 1u),
                    foreach (v_in.v(), inside(v_in.v()[_i + 1u], 1u, 2u))};
        BOOST_REQUIRE(!v_in.randomize());
        v_vc.con = {v_vc.v().size() == 2u, foreach (v_vc.v(), v_vc.v()[_i] == 1u),
                    foreach (v_vc.v(), inside(v_vc.v()[_i + 1u], v_vc.v()))};
        BOOST_REQUIRE(!v_vc.randomize());

        // Single-item inside(...) & inside_mask(...) OOB soundness (crv_array & crv_vector)
        struct f1_single_arr_item : public crv_sequence_item {
          f1_single_arr_item(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_array<uint8_t, 2> arr{"arr"};
          crv_array<uint8_t, 1> arr1{"arr1"};
          crv_constraint con{"con"};
        } sa_1c("sa_1c"), sa_1sv("sa_1sv"), sa_m1("sa_m1"), sa_m2("sa_m2"), sa_m3("sa_m3"), sa_m4("sa_m4"),
            sa_sym("sa_sym"), sa_arr1("sa_arr1"), sa_neg("sa_neg"), sa_eq("sa_eq"), sa_sat("sa_sat");
        sa_1c.con = {foreach (sa_1c.arr, sa_1c.arr[_i] == 1u),
                     foreach (sa_1c.arr, inside(sa_1c.arr[_i + 1u], uint8_t(1u)))};
        BOOST_REQUIRE(!sa_1c.randomize());
        sa_1sv.con = {foreach (sa_1sv.arr, sa_1sv.arr[_i] == 1u),
                      foreach (sa_1sv.arr, inside(sa_1sv.arr[_i + 1u], std::vector<uint8_t>{1u}))};
        BOOST_REQUIRE(!sa_1sv.randomize());
        sa_m1.con = {foreach (sa_m1.arr, sa_m1.arr[_i] == 0x10u),
                     foreach (sa_m1.arr, inside(sa_m1.arr[_i + 1u], inside_mask("8'b0001_00xx")))};
        BOOST_REQUIRE(!sa_m1.randomize());
        sa_m2.con = {foreach (sa_m2.arr, sa_m2.arr[_i] == 0x10u),
                     foreach (sa_m2.arr, inside_mask(sa_m2.arr[_i + 1u], "8'b0001_00xx"))};
        BOOST_REQUIRE(!sa_m2.randomize());
        sa_m3.con = {foreach (sa_m3.arr, sa_m3.arr[_i] == 0x10u),
                     foreach (sa_m3.arr, inside(sa_m3.arr[_i + 1u], inside_mask(uint8_t(0x10u), uint8_t(0xfcu))))};
        BOOST_REQUIRE(!sa_m3.randomize());
        sa_m4.con = {foreach (sa_m4.arr, sa_m4.arr[_i] == 0x10u),
                     foreach (sa_m4.arr, inside_mask(sa_m4.arr[_i + 1u], uint8_t(0x10u), uint8_t(0xfcu)))};
        BOOST_REQUIRE(!sa_m4.randomize());
        sa_sym.con = {sa_sym.x() == 1u, foreach (sa_sym.arr, sa_sym.arr[_i] == 1u),
                      foreach (sa_sym.arr, inside(sa_sym.arr[_i + 1u], sa_sym.x()))};
        BOOST_REQUIRE(!sa_sym.randomize());
        sa_arr1.con = {sa_arr1.arr1[0]() == 1u, foreach (sa_arr1.arr, sa_arr1.arr[_i] == 1u),
                       foreach (sa_arr1.arr, inside(sa_arr1.arr[_i + 1u], sa_arr1.arr1))};
        BOOST_REQUIRE(!sa_arr1.randomize());
        sa_neg.con = {foreach (sa_neg.arr, sa_neg.arr[_i] == 2u),
                      foreach (sa_neg.arr, !inside(sa_neg.arr[_i + 1u], uint8_t(1u)))};
        BOOST_REQUIRE(!sa_neg.randomize());
        sa_eq.con = {foreach (sa_eq.arr, sa_eq.arr[_i] == 1u),
                     foreach (sa_eq.arr, inside(sa_eq.arr[_i + 1u], uint8_t(1u)) == true)};
        BOOST_REQUIRE(!sa_eq.randomize());
        sa_sat.con = {sa_sat.x() == 0x12u, sa_sat.arr1[0]() == 0x12u,
                      foreach (sa_sat.arr, inside(sa_sat.arr[_i], uint8_t(0x12u))),
                      foreach (sa_sat.arr, inside(sa_sat.arr[_i], std::vector<uint8_t>{0x12u})),
                      foreach (sa_sat.arr, inside(sa_sat.arr[_i], inside_mask("8'b0001_00xx"))),
                      foreach (sa_sat.arr, inside_mask(sa_sat.arr[_i], uint8_t(0x10u), uint8_t(0xfcu))),
                      foreach (sa_sat.arr, inside(sa_sat.arr[_i], sa_sat.x())),
                      foreach (sa_sat.arr, inside(sa_sat.arr[_i], sa_sat.arr1))};
        BOOST_REQUIRE(sa_sat.randomize());
        for (unsigned i = 0; i < 2; ++i) BOOST_REQUIRE_EQUAL(sa_sat.arr[i], 0x12u);

        struct f1_single_vec_item : public crv_sequence_item {
          f1_single_vec_item(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_vector<uint8_t> v{"v"};
          crv_array<uint8_t, 1> arr1{"arr1"};
          crv_constraint con{"con"};
        } sv_1c("sv_1c"), sv_1sv("sv_1sv"), sv_m1("sv_m1"), sv_m2("sv_m2"), sv_sym("sv_sym"), sv_arr1("sv_arr1"),
            sv_neg("sv_neg"), sv_sat("sv_sat");
        sv_1c.con = {sv_1c.v().size() == 2u, foreach (sv_1c.v(), sv_1c.v()[_i] == 1u),
                     foreach (sv_1c.v(), inside(sv_1c.v()[_i + 1u], uint8_t(1u)))};
        BOOST_REQUIRE(!sv_1c.randomize());
        sv_1sv.con = {sv_1sv.v().size() == 2u, foreach (sv_1sv.v(), sv_1sv.v()[_i] == 1u),
                      foreach (sv_1sv.v(), inside(sv_1sv.v()[_i + 1u], std::vector<uint8_t>{1u}))};
        BOOST_REQUIRE(!sv_1sv.randomize());
        sv_m1.con = {sv_m1.v().size() == 2u, foreach (sv_m1.v(), sv_m1.v()[_i] == 0x10u),
                     foreach (sv_m1.v(), inside(sv_m1.v()[_i + 1u], inside_mask("8'b0001_00xx")))};
        BOOST_REQUIRE(!sv_m1.randomize());
        sv_m2.con = {sv_m2.v().size() == 2u, foreach (sv_m2.v(), sv_m2.v()[_i] == 0x10u),
                     foreach (sv_m2.v(), inside_mask(sv_m2.v()[_i + 1u], uint8_t(0x10u), uint8_t(0xfcu)))};
        BOOST_REQUIRE(!sv_m2.randomize());
        sv_sym.con = {sv_sym.x() == 1u, sv_sym.v().size() == 2u, foreach (sv_sym.v(), sv_sym.v()[_i] == 1u),
                      foreach (sv_sym.v(), inside(sv_sym.v()[_i + 1u], sv_sym.x()))};
        BOOST_REQUIRE(!sv_sym.randomize());
        sv_arr1.con = {sv_arr1.arr1[0]() == 1u, sv_arr1.v().size() == 2u,
                       foreach (sv_arr1.v(), sv_arr1.v()[_i] == 1u),
                       foreach (sv_arr1.v(), inside(sv_arr1.v()[_i + 1u], sv_arr1.arr1))};
        BOOST_REQUIRE(!sv_arr1.randomize());
        sv_neg.con = {sv_neg.x() == 1u, sv_neg.v().size() == 2u, foreach (sv_neg.v(), sv_neg.v()[_i] == 2u),
                      foreach (sv_neg.v(), !inside(sv_neg.v()[_i + 1u], sv_neg.x()))};
        BOOST_REQUIRE(!sv_neg.randomize());
        sv_sat.con = {sv_sat.x() == 0x12u, sv_sat.arr1[0]() == 0x12u, sv_sat.v().size() == 2u,
                      foreach (sv_sat.v(), inside(sv_sat.v()[_i], uint8_t(0x12u))),
                      foreach (sv_sat.v(), inside(sv_sat.v()[_i], std::vector<uint8_t>{0x12u})),
                      foreach (sv_sat.v(), inside(sv_sat.v()[_i], inside_mask("8'b0001_00xx"))),
                      foreach (sv_sat.v(), inside_mask(sv_sat.v()[_i], uint8_t(0x10u), uint8_t(0xfcu))),
                      foreach (sv_sat.v(), inside(sv_sat.v()[_i], sv_sat.x())),
                      foreach (sv_sat.v(), inside(sv_sat.v()[_i], sv_sat.arr1))};
        BOOST_REQUIRE(sv_sat.randomize());
        for (unsigned i = 0; i < 2; ++i) BOOST_REQUIRE_EQUAL(sv_sat.v[i], 0x12u);

        // Bare bool VectorAccess at foreach root must fail UNSAT on OOB while legacy != remains SAT
        struct f1_bool_arr_item : public crv_sequence_item {
          f1_bool_arr_item(crv_object_name) {}
          crv_array<bool, 3> barr{"barr"};
          crv_constraint con{"con"};
        } ba_oob("ba_oob"), ba_sat("ba_sat"), ba_leg("ba_leg");
        ba_oob.con = {foreach (ba_oob.barr, ba_oob.barr[_i + 1u])};
        BOOST_REQUIRE(!ba_oob.randomize());
        ba_sat.con = {foreach (ba_sat.barr, ba_sat.barr[_i])};
        BOOST_REQUIRE(ba_sat.randomize());
        for (unsigned i = 0; i < 3; ++i) BOOST_REQUIRE(ba_sat.barr[i]);
        ba_leg.con = {foreach (ba_leg.barr, ba_leg.barr[_i] != ba_leg.barr[_i - 1u])};
        BOOST_REQUIRE(ba_leg.randomize());
        for (unsigned i = 1; i < 3; ++i) BOOST_REQUIRE_NE(ba_leg.barr[i], ba_leg.barr[i - 1]);

        struct f1_bool_vec_item : public crv_sequence_item {
          f1_bool_vec_item(crv_object_name) {}
          crv_variable<bool> b;
          crv_vector<bool> bv{"bv"};
          crv_constraint con{"con"};
        } bv_oob("bv_oob"), bv_joint("bv_joint"), bv_sat("bv_sat"), bv_leg("bv_leg");
        bv_oob.con = {bv_oob.bv().size() == 3u, foreach (bv_oob.bv(), bv_oob.bv()[_i + 1u])};
        BOOST_REQUIRE(!bv_oob.randomize());
        bv_joint.con = {bv_joint.b() == true, bv_joint.bv().size() == 3u,
                        foreach (bv_joint.bv(), bv_joint.bv()[_i] == bv_joint.b()),
                        foreach (bv_joint.bv(), bv_joint.bv()[_i + 1u])};
        BOOST_REQUIRE(!bv_joint.randomize());
        bv_sat.con = {bv_sat.bv().size() == 3u, foreach (bv_sat.bv(), bv_sat.bv()[_i])};
        BOOST_REQUIRE(bv_sat.randomize());
        for (unsigned i = 0; i < 3; ++i) BOOST_REQUIRE(bv_sat.bv[i]);
        bv_leg.con = {bv_leg.bv().size() == 3u, foreach (bv_leg.bv(), bv_leg.bv()[_i] != bv_leg.bv()[_i - 1u])};
        BOOST_REQUIRE(bv_leg.randomize());
        for (unsigned i = 1; i < 3; ++i) BOOST_REQUIRE_NE(bv_leg.bv[i], bv_leg.bv[i - 1]);
      }

      // Finding 2: Complete containsBoolExpr traversal & preserve resultBoundsGuard() across !okay()
      {
        struct f2_arr2_item : public crv_sequence_item {
          f2_arr2_item(crv_object_name) {}
          crv_array<uint8_t, 2> arr{"arr"};
          crv_array<uint8_t, 1> v{"v"};
          crv_constraint con{"con"};
        } fa_idx("f2_idx"), fa_ite("f2_ite"), fa_sw_bool("f2_sw_b"), fa_sw_oob2("f2_sw_oob2"), fa_sat("f2_sat");
        fa_idx.con = {foreach (fa_idx.arr, fa_idx.arr[_i] == 10u),
                      foreach (fa_idx.arr, fa_idx.arr[fa_idx.arr[_i - 1u] == 10u] == fa_idx.arr[_i - 1u])};
        BOOST_REQUIRE(!fa_idx.randomize());
        fa_ite.con = {foreach (fa_ite.arr, fa_ite.arr[_i] == 10u),
                      foreach (fa_ite.arr, if_then_else(fa_ite.arr[_i - 1u] == 10u, uint8_t(1u), uint8_t(2u)) ==
                                               fa_ite.arr[_i - 1u])};
        BOOST_REQUIRE(!fa_ite.randomize());
        fa_sw_bool.con = {foreach (fa_sw_bool.arr, fa_sw_bool.arr[_i] == 10u),
                          foreach (fa_sw_bool.arr,
                                   fa_sw_bool.v.sum_with([&](auto, auto) { return fa_sw_bool.arr[_i - 1u] == 10u; }) ==
                                       fa_sw_bool.arr[_i - 1u])};
        BOOST_REQUIRE(!fa_sw_bool.randomize());
        fa_sw_oob2.con = {foreach (fa_sw_oob2.arr, fa_sw_oob2.arr[_i] == 0u),
                          foreach (fa_sw_oob2.arr,
                                   fa_sw_oob2.v.sum_with([&](auto j, auto) { return fa_sw_oob2.v[j + 1u]; }) ==
                                       fa_sw_oob2.arr[_i - 1u])};
        BOOST_REQUIRE(!fa_sw_oob2.randomize());
        fa_sat.con = {foreach (fa_sat.arr, fa_sat.arr[_i] == 5u),
                      foreach (fa_sat.arr, if_then_else(_i > 0u, fa_sat.arr[_i - 1u], uint8_t(5u)) <= 10u)};
        BOOST_REQUIRE(fa_sat.randomize());

        struct f2_arr1_item : public crv_sequence_item {
          f2_arr1_item(crv_object_name) {}
          crv_array<uint8_t, 1> arr{"arr"};
          crv_array<uint8_t, 1> v{"v"};
          crv_constraint con{"con"};
        } fa_s1("f2_a1"), fa_s1_neg("f2_a1_neg");
        fa_s1.con = {foreach (fa_s1.arr, fa_s1.v.sum_with([&](auto j, auto) { return fa_s1.v[j + 1u]; }) ==
                                             fa_s1.arr[_i - 1u])};
        BOOST_REQUIRE(!fa_s1.randomize());
        fa_s1_neg.con = {!foreach (fa_s1_neg.arr, fa_s1_neg.v.sum_with([&](auto j, auto) { return fa_s1_neg.v[j + 1u]; }) ==
                                                  fa_s1_neg.arr[_i - 1u])};
        BOOST_REQUIRE(!fa_s1_neg.randomize());

        struct f2_vec_item : public crv_sequence_item {
          f2_vec_item(crv_object_name) {}
          crv_variable<uint8_t> x;
          crv_vector<uint8_t> v{"v"};
          crv_constraint con{"con"};
        } fv_s1("f2_v1"), fv_s2("f2_v2"), fv_bool("f2_vb"), fv_joint("f2_vj");
        fv_s1.con = {fv_s1.v().size() == 1u,
                     foreach (fv_s1.v(), fv_s1.v().sum_with([&](auto j, auto) { return fv_s1.v()[j + 1u]; }) ==
                                             fv_s1.v()[_i - 1u])};
        BOOST_REQUIRE(!fv_s1.randomize());
        fv_s2.con = {fv_s2.v().size() == 2u, foreach (fv_s2.v(), fv_s2.v()[_i] == 0u),
                     foreach (fv_s2.v(), fv_s2.v().sum_with([&](auto j, auto) { return fv_s2.v()[j + 1u]; }) ==
                                             fv_s2.v()[_i - 1u])};
        BOOST_REQUIRE(!fv_s2.randomize());
        fv_bool.con = {fv_bool.v().size() == 2u, foreach (fv_bool.v(), fv_bool.v()[_i] == 10u),
                       foreach (fv_bool.v(),
                                fv_bool.v().sum_with([&](auto, auto) { return fv_bool.v()[_i - 1u] == 10u; }) ==
                                    fv_bool.v()[_i - 1u])};
        BOOST_REQUIRE(!fv_bool.randomize());
        fv_joint.con = {fv_joint.x() == 1u, fv_joint.v().size() == 1u,
                        foreach (fv_joint.v(),
                                 fv_joint.v().sum_with([&](auto j, auto) { return fv_joint.v()[j + 1u]; }) ==
                                     fv_joint.v()[_i - 1u] + fv_joint.x())};
        BOOST_REQUIRE(!fv_joint.randomize());
      }
    }
  }

  // Direct BddSampler::get({BoundsCheckExpr}) alignment
  if (FactoryMetaSMT::isDefined(CUDD)) {
    for (bool sv : {false, true}) {
      set_sv_signedness(sv);
      NodePtr v(new VariableExpr(new_var_id(), 8, false));
      NodePtr eq(new EqualOpr(v, new Constant(2, 8, false)));
      NodePtr guard_node(new LessOpr(v, new Constant(4, 8, false)));
      NodePtr bce(new BoundsCheckExpr(eq, guard_node));
      std::shared_ptr<BddSampler> sampler = BddSampler::get({bce});
      BOOST_REQUIRE(sampler != nullptr);
      std::string bits;
      BOOST_REQUIRE(sampler->sample(&bits));
      BOOST_REQUIRE_EQUAL(bits, "00000010");

      NodePtr neg_bce(new NotOpr(bce));
      std::shared_ptr<BddSampler> neg_sampler = BddSampler::get({neg_bce});
      BOOST_REQUIRE(neg_sampler != nullptr);
      BOOST_REQUIRE(neg_sampler->sample(&bits));
      unsigned val = 0;
      for (char c : bits) val = (val << 1) | (c == '1' ? 1u : 0u);
      BOOST_REQUIRE_LT(val, 4u);
      BOOST_REQUIRE_NE(val, 2u);

      NodePtr eq_oob(new EqualOpr(v, new Constant(5, 8, false)));
      NodePtr unsat_bce(new BoundsCheckExpr(eq_oob, guard_node));
      BOOST_REQUIRE(BddSampler::get({unsat_bce}) == nullptr);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()

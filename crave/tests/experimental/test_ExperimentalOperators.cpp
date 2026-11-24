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

BOOST_AUTO_TEST_SUITE_END()

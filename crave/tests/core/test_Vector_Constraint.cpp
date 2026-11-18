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

#include <boost/format.hpp>

#include <set>
#include <iostream>

using boost::format;
using namespace crave;

template <typename T>
bool check_unique(rand_vec<T>& v) {
  for (unsigned i = 0; i < v.size(); i++)
    for (unsigned j = 0; j < i; j++)
      if (v[i] == v[j]) return false;
  return true;
}

BOOST_FIXTURE_TEST_SUITE(Vector_Constraint_t, Context_Fixture)

struct Item : public rand_obj {
  Item() : i_(), v(this) {
    constraint(30 <= v().size() && v().size() <= 50);
    constraint(foreach (v(), v()[i_] == v()[i_ - 1] + v()[i_ - 2]));
    constraint(foreach (v(), if_then(i_ <= 1, v()[i_] == i_)));
  }

  placeholder i_;
  rand_vec<unsigned int> v;
};

BOOST_AUTO_TEST_CASE(fibo_test) {
  Item it;
  BOOST_REQUIRE(it.next());
  BOOST_REQUIRE_LE(30, it.v.size());
  BOOST_REQUIRE_LE(it.v.size(), 50);
  BOOST_REQUIRE_EQUAL(it.v[0], 0);
  BOOST_REQUIRE_EQUAL(it.v[1], 1);
  for (unsigned i = 2; i < it.v.size(); i++) BOOST_REQUIRE_EQUAL(it.v[i], it.v[i - 1] + it.v[i - 2]);
}

struct Item1 : public rand_obj {
  Item1() : v(this) {}

  __rand_vec<unsigned int> u;
  rand_vec<unsigned int> v;
};

BOOST_AUTO_TEST_CASE(free_vector_test) {
  Item1 it;
  BOOST_REQUIRE(it.next());
  BOOST_REQUIRE(it.u.size() == 0);
  BOOST_REQUIRE(it.v.size() > 0);
  for (unsigned i = 0; i < it.v.size(); i++) std::cout << it.v[i] << " ";
  std::cout << std::endl;
}

struct Item2 : public rand_obj {
  Item2() : idx(), v(this) { constraint(foreach (v(), 100 <= v()[idx] && v()[idx] <= 200)); }

  placeholder idx;
  rand_vec<unsigned int> v;
};

BOOST_AUTO_TEST_CASE(default_size_test) {
  Item2 it;
  BOOST_REQUIRE(it.next());
  BOOST_REQUIRE(it.v.size() == default_rand_vec_size());
  for (unsigned i = 0; i < it.v.size(); i++) {
    std::cout << it.v[i] << " ";
    BOOST_REQUIRE(100 <= it.v[i] && it.v[i] <= 200);
  }
  std::cout << std::endl;
}

struct Item3 : public rand_obj {
  Item3() : i(), v(this) {
    constraint(v().size() == 100);
    constraint(foreach (v(), v()[i] < 100));
    constraint(foreach (v(), v()[i] >= 0));
    constraint(unique(v()));
  }

  placeholder i;
  rand_vec<int> v;
};

BOOST_AUTO_TEST_CASE(unique_test_1) {
  Item3 it;
  BOOST_REQUIRE(it.next());
  BOOST_REQUIRE(it.v.size() == 100);
  for (unsigned i = 0; i < it.v.size(); i++) {
    BOOST_REQUIRE(0 <= it.v[i] && it.v[i] < 100);
    for (unsigned j = 0; j < i; j++) BOOST_REQUIRE(it.v[i] != it.v[j]);
  }
}

BOOST_AUTO_TEST_CASE(unique_test_2) {
  rand_vec<unsigned int> v(NULL);
  placeholder idx;
  Generator gen;
  gen(v().size() == 7);
  gen(foreach (v(), v()[idx] < 6));

  gen("unique", unique(v()));
  BOOST_REQUIRE(!gen.next());

  gen.disableConstraint("unique");
  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE(v.size() == 7);
  for (unsigned i = 0; i < v.size(); i++) {
    std::cout << v[i] << " ";
    BOOST_REQUIRE(v[i] < 6);
  }
  std::cout << std::endl;
}

BOOST_AUTO_TEST_CASE(soft_unique_test) {
  rand_vec<unsigned int> v(NULL);
  placeholder idx;
  Generator gen;
  gen(v().size() == 7);
  gen(foreach (v(), v()[idx] < 6));
  gen.soft(unique(v()));
  BOOST_REQUIRE(gen.next());
}

struct Item4 : public rand_obj {
  Item4() : _i(), v(this) {
    constraint(v().size() == 10);
    constraint("c1", foreach (v(), v()[_i] <= 100));
    constraint("c2", foreach (v(), v()[_i] > 100));
  }

  placeholder _i;
  rand_vec<unsigned int> v;
};

BOOST_AUTO_TEST_CASE(constraint_management_test) {
  Item4 it;

  BOOST_REQUIRE(!it.next());

  it.disable_constraint("c1");
  BOOST_REQUIRE(it.next());
  for (unsigned i = 0; i < it.v.size(); i++) BOOST_REQUIRE_GT(it.v[i], 100);

  it.enable_constraint("c1");
  it.disable_constraint("c2");
  BOOST_REQUIRE(it.next());
  for (unsigned i = 0; i < it.v.size(); i++) BOOST_REQUIRE(it.v[i] <= 100);

  it.enable_constraint("c2");
  BOOST_REQUIRE(!it.next());
}

struct Item5 : public rand_obj {
  Item5() : x(), v(this) {
    constraint(v().size() == 50);
    constraint(foreach (v(), if_then(x < 25, v()[x] == x)));
    constraint(foreach (v(), if_then(x == 25, v()[x] == 0)));
    constraint(foreach (v(), if_then(x > 25, v()[x] + x == 200)));
  }

  placeholder x;
  rand_vec<unsigned int> v;
};

BOOST_AUTO_TEST_CASE(index_constraint_test) {
  Item5 it;
  it.constraint.printDotGraph(std::cout);
  BOOST_REQUIRE(it.next());
  BOOST_REQUIRE(it.v.size() == 50);
  for (unsigned i = 0; i < it.v.size(); i++) {
    if (i < 25)
      BOOST_REQUIRE(it.v[i] == i);
    else if (i > 25)
      BOOST_REQUIRE(it.v[i] + i == 200);
    else
      BOOST_REQUIRE(it.v[i] == 0);
  }
}

BOOST_AUTO_TEST_CASE(soft_vec_constraint) {
  rand_vec<unsigned int> v(NULL);
  placeholder i;

  Generator gen;
  gen(v().size() == 10);
  gen(foreach (v(), v()[i] >= v()[i - 1]));
  gen.soft(foreach (v(), v()[i] < v()[i - 1]));
  BOOST_REQUIRE(gen.next());

  Generator gen1;
  gen1(v().size() == 4);
  gen1(foreach (v(), v()[i] >= v()[i - 1]));
  gen1(foreach (v(), v()[i] <= 1000));
  gen1.soft(foreach (v(), v()[i] <= v()[i - 1]));
  gen1.soft(foreach (v(), if_then(i == 0, v()[i] % 13 == 3)));
  BOOST_REQUIRE(gen1.next());
  for (int j = 0; j < 10; j++) {
    BOOST_REQUIRE_EQUAL(v.size(), 4);
    std::cout << v[0] << " " << v[1] << " " << v[2] << " " << v[3] << std::endl;
    BOOST_REQUIRE_EQUAL(v[0], v[1]);
    BOOST_REQUIRE_EQUAL(v[1], v[2]);
    BOOST_REQUIRE_EQUAL(v[2], v[3]);
    BOOST_REQUIRE_EQUAL(v[0] % 13, 3);
    if (!gen1.next()) break;
  }
}

BOOST_AUTO_TEST_CASE(mixed_bv_width_1) {
  rand_vec<signed char> a(NULL);
  placeholder idx;
  Generator gen;
  gen(a().size() == 28);
  gen(foreach (a(), a()[idx] < (short)-100));
  gen(unique(a()));

  BOOST_REQUIRE(gen.next());
  for (unsigned i = 0; i < a.size(); i++) {
    BOOST_REQUIRE_LT(a[i], -100);
    std::cout << " " << (int)a[i];
  }
  std::cout << std::endl;
  BOOST_REQUIRE(check_unique(a));
}

BOOST_AUTO_TEST_CASE(mixed_bv_width_2) {
  rand_vec<short> a(NULL);
  placeholder p;
  Generator gen;
  gen(a().size() == 19);
  gen(foreach (a(), a()[p] < 10));
  gen(foreach (a(), a()[p] > -10));
  gen(unique(a()));

  BOOST_REQUIRE(gen.next());
  for (unsigned i = 0; i < a.size(); i++) BOOST_REQUIRE(-10 < a[i] && a[i] < 10);
  BOOST_REQUIRE(check_unique(a));
}

BOOST_AUTO_TEST_CASE(mixed_bv_width_3) {
  rand_vec<int> a(NULL);
  placeholder _i;
  Generator gen;
  gen(a().size() == 19);
  gen(foreach (a(), a()[_i] < (signed char)10));
  gen(foreach (a(), a()[_i] > (short)-10));
  gen(unique(a()));

  BOOST_REQUIRE(gen.next());
  for (unsigned i = 0; i < a.size(); i++) BOOST_REQUIRE(-10 < a[i] && a[i] < 10);
  BOOST_REQUIRE(check_unique(a));
}

BOOST_AUTO_TEST_CASE(mixed_bv_width_4) {
  rand_vec<short> a(NULL);
  placeholder _i;
  Generator gen;
  gen(a().size() == 10);
  gen(foreach (a(), -3 <= a()[_i] && a()[_i] <= 3));
  gen(foreach (a(), a()[_i] != 0));
  gen(foreach (a(), a()[_i] * a()[_i - 1] % 6 == 0));
  gen(foreach (a(), _i != 0 || a()[_i] % 2 == 0));

  BOOST_REQUIRE(gen.next());
  for (unsigned i = 0; i < a.size(); i++) {
    BOOST_REQUIRE(-3 <= a[i] && a[i] <= 3);
    BOOST_REQUIRE(a[i] != 0);
    if (i == 0)
      BOOST_REQUIRE(a[i] % 2 == 0);
    else
      BOOST_REQUIRE(a[i] * a[i - 1] % 6 == 0);
  }
}

BOOST_AUTO_TEST_CASE(bool_rand_vec) {
  rand_vec<bool> a(NULL);
  placeholder _i;
  Generator gen;
  gen(a().size() == 10);
  gen(foreach (a(), a()[_i] != a()[_i - 1]));

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE(a.size() == 10);
  for (unsigned i = 1; i < a.size(); i++) BOOST_REQUIRE(a[i] != a[i - 1]);
}

// The elements of a bool vector are stored as 0 and 1, the values of false and true.
BOOST_AUTO_TEST_CASE(bool_rand_vec_values) {
  rand_vec<bool> a(NULL);
  Generator gen;
  gen(a().size() == 4);
  gen(foreach (a(), a()[_i] == (_i % 2 == 0)));
  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(a.size(), 4u);
  for (unsigned i = 0; i < a.size(); i++)
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(reinterpret_cast<unsigned char const&>(a[i])), (i + 1) % 2);
}

struct Item6 : public rand_obj {
  Item6() : v(this) { constraint(v().size() == default_rand_vec_size() + 7); }
  rand_vec<unsigned int> v;
};

BOOST_AUTO_TEST_CASE(only_size_constrained_vect_test) {
  Item6 it;
  BOOST_REQUIRE(it.next());
  BOOST_REQUIRE_EQUAL(it.v.size(), default_rand_vec_size() + 7);
}

// The sum of the elements of a vector, whose number is the size of the vector
BOOST_AUTO_TEST_CASE(vector_sum) {
  rand_vec<unsigned short> v(NULL);
  unsigned size = 0;
  Generator gen;
  gen(v().size() == reference(size));
  gen(foreach (v(), v()[_i] < 100));
  gen(v().sum() == 150);

  for (size = 0; size <= 6; ++size) {
    // the sum of fewer than 2 elements below 100 cannot be 150
    BOOST_REQUIRE_EQUAL(gen.next(), size >= 2);
    if (size < 2) continue;
    BOOST_REQUIRE_EQUAL(v.size(), size);
    unsigned sum = 0;
    for (unsigned i = 0; i < v.size(); i++) {
      BOOST_REQUIRE_LT(v[i], 100);
      sum += v[i];
    }
    BOOST_REQUIRE_EQUAL(sum, 150u);
  }
}

// As the sum() reduction method of SystemVerilog (IEEE 1800-2017 7.12.3), the sum has the type of the elements.
BOOST_AUTO_TEST_CASE(vector_sum_element_width) {
  rand_vec<unsigned char> v(NULL);
  Generator gen;
  gen(v().size() == 4);
  gen(foreach (v(), v()[_i] >= 100));
  gen(v().sum() == 144);
  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(v.size(), 4u);
  unsigned sum = 0;
  for (unsigned i = 0; i < v.size(); i++) sum += v[i];
  BOOST_REQUIRE_GT(sum, 255u);
  BOOST_REQUIRE_EQUAL(sum % 256, 144u);

  // the sum of unsigned char elements never exceeds 255
  rand_vec<unsigned char> w(NULL);
  Generator gen2;
  gen2(w().size() == 4);
  gen2(w().sum() == 400);
  BOOST_REQUIRE(!gen2.next());

  // the sum of signed char elements is signed
  rand_vec<signed char> s(NULL);
  Generator gen3;
  gen3(s().size() == 2);
  gen3(foreach (s(), s()[_i] > 0));
  gen3(s().sum() < 0);
  BOOST_REQUIRE(gen3.next());
  BOOST_REQUIRE(s[0] > 0 && s[1] > 0);
  BOOST_REQUIRE(s[0] + s[1] > 127);
  BOOST_REQUIRE(static_cast<signed char>(s[0] + s[1]) < 0);
}

BOOST_AUTO_TEST_CASE(vector_product_and_bitwise_reductions) {
  rand_vec<unsigned char> v(NULL);
  Generator gen;
  gen(v().size() == 3);
  gen(foreach (v(), v()[_i] > 1));
  gen(v().product() == 30);
  for (int n = 0; n < 5; ++n) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_EQUAL(static_cast<unsigned char>(v[0] * v[1] * v[2]), 30);
  }

  rand_vec<unsigned int> w(NULL);
  Generator gen2;
  gen2(w().size() == 4);
  gen2(w().and_() == 0x10u);
  gen2(w().or_() == 0xF0u);
  gen2(w().xor_() == 0x20u);
  gen2(foreach (w(), w()[_i] != w()[_i - 1]));
  BOOST_REQUIRE(gen2.next());
  BOOST_REQUIRE_EQUAL(w.size(), 4u);
  BOOST_REQUIRE_EQUAL(w[0] & w[1] & w[2] & w[3], 0x10u);
  BOOST_REQUIRE_EQUAL(w[0] | w[1] | w[2] | w[3], 0xF0u);
  BOOST_REQUIRE_EQUAL(w[0] ^ w[1] ^ w[2] ^ w[3], 0x20u);
}

// The reductions of no elements are the identities of the operators.
BOOST_AUTO_TEST_CASE(vector_reductions_of_empty_vector) {
  rand_vec<unsigned char> v(NULL);
  Generator gen;
  gen(v().size() == 0);
  gen(v().sum() == 0 && v().product() == 1 && v().and_() == 0xFF && v().or_() == 0 && v().xor_() == 0);
  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(v.size(), 0u);

  rand_vec<signed char> s(NULL);
  Generator gen2;
  gen2(s().size() == 0);
  gen2(s().and_() == -1);
  BOOST_REQUIRE(gen2.next());

  rand_vec<unsigned char> u(NULL);
  Generator gen3;
  gen3(u().size() == 0);
  gen3(u().sum() == 1);
  BOOST_REQUIRE(!gen3.next());
}

BOOST_AUTO_TEST_CASE(bool_vector_reductions) {
  rand_vec<bool> a(NULL);
  Generator gen;
  gen(a().size() == 5);
  gen(a().or_());
  gen(!a().product());
  gen(a().xor_());
  for (int n = 0; n < 10; ++n) {
    BOOST_REQUIRE(gen.next());
    unsigned count = 0;
    for (unsigned i = 0; i < a.size(); i++) count += a[i] != 0;
    BOOST_REQUIRE(count > 0 && count < 5);
    BOOST_REQUIRE_EQUAL(count % 2, 1u);
  }
  rand_vec<bool> b(NULL);
  Generator gen2;
  gen2(b().size() == 3);
  gen2(b().and_());
  BOOST_REQUIRE(gen2.next());
  BOOST_REQUIRE(b[0] != 0 && b[1] != 0 && b[2] != 0);
}

// A reduction in a foreach constraint is the reduction of all elements.
BOOST_AUTO_TEST_CASE(vector_reduction_in_foreach) {
  rand_vec<unsigned int> v(NULL);
  Generator gen;
  gen(v().size() == 4);
  gen(foreach (v(), v()[_i] >= 1 && v()[_i] <= 10));
  // no element is less than the mean, so all elements are equal
  gen(foreach (v(), v()[_i] * 4 >= v().sum()));
  BOOST_REQUIRE(gen.next());
  unsigned sum = v[0] + v[1] + v[2] + v[3];
  for (unsigned i = 0; i < v.size(); i++) BOOST_REQUIRE_GE(v[i] * 4, sum);
  BOOST_REQUIRE(v[0] == v[1] && v[1] == v[2] && v[2] == v[3]);
}

// A constraint with a reduction may refer to the elements of its vector only.
BOOST_AUTO_TEST_CASE(vector_reduction_with_other_variables) {
  rand_vec<unsigned int> v(NULL);
  rand_vec<unsigned int> w(NULL);
  Variable<unsigned int> x;
  Generator gen;
  BOOST_CHECK_THROW(gen(v().sum() == x), std::runtime_error);
  BOOST_CHECK_THROW(gen(v().sum() == w().sum()), std::runtime_error);
  BOOST_CHECK_THROW(gen(v().sum() == v().size()), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()  // Context

//  vim: ft=cpp:ts=2:sw=2:expandtab

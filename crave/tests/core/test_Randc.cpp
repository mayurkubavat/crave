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

#include <algorithm>
#include <set>
#include <vector>

using namespace crave;

namespace {
// generates n values of the variable
template <typename T>
std::vector<int> generate(Generator& gen, Variable<T> const& var, unsigned n) {
  std::vector<int> values;
  for (unsigned i = 0; i < n; ++i) {
    BOOST_REQUIRE(gen.next());
    values.push_back(gen[var]);
  }
  return values;
}

// checks that the values are a permutation of the expected values
void require_permutation(std::vector<int> values, std::set<int> const& expected) {
  std::sort(values.begin(), values.end());
  BOOST_REQUIRE_EQUAL(values.size(), expected.size());
  BOOST_REQUIRE(std::equal(values.begin(), values.end(), expected.begin()));
}

std::set<int> values_in(int first, int last) {
  std::set<int> result;
  for (int i = first; i <= last; ++i) result.insert(i);
  return result;
}
}  // namespace

BOOST_FIXTURE_TEST_SUITE(Randc_t, Context_Fixture)

// IEEE 1800-2017 18.4.2: a randc variable takes all values of its range in a random permutation
BOOST_AUTO_TEST_CASE(randc_full_range) {
  Variable<unsigned char> x;
  Generator gen;
  gen.randc(x);

  std::vector<int> first = generate(gen, x, 256);
  std::vector<int> second = generate(gen, x, 256);
  require_permutation(first, values_in(0, 255));
  require_permutation(second, values_in(0, 255));
  BOOST_REQUIRE(first != second);
  BOOST_REQUIRE(!std::is_sorted(first.begin(), first.end()));
}

// only the values that satisfy the constraints are cycled
BOOST_AUTO_TEST_CASE(randc_constrained) {
  Variable<signed char> x;
  Generator gen(x >= -3 && x < 7);
  gen.randc(x);

  std::set<std::vector<int> > orders;
  for (int cycle = 0; cycle < 6; ++cycle) {
    std::vector<int> values = generate(gen, x, 10);
    require_permutation(values, values_in(-3, 6));
    orders.insert(values);
  }
  BOOST_REQUIRE_GT(orders.size(), 1u);
}

// a 16-bit variable, whose few values are found by the solver after the tried values fail
BOOST_AUTO_TEST_CASE(randc_sparse_16_bit) {
  Variable<unsigned short> x;
  Generator gen(x % 1000u == 7u);
  gen.randc(x);

  std::set<int> expected;
  for (int i = 7; i < 65536; i += 1000) expected.insert(i);
  for (int cycle = 0; cycle < 2; ++cycle) require_permutation(generate(gen, x, expected.size()), expected);
}

// the randc variable is chosen first, the other variables are solved for its value
BOOST_AUTO_TEST_CASE(randc_with_random_variable) {
  Variable<unsigned char> x;
  Variable<unsigned> y;
  Generator gen(x < 16u && y > x && y < 20u);
  gen.randc(x);

  for (int cycle = 0; cycle < 3; ++cycle) {
    std::vector<int> values;
    for (int i = 0; i < 16; ++i) {
      BOOST_REQUIRE(gen.next());
      BOOST_REQUIRE(gen[y] > gen[x] && gen[y] < 20u);
      values.push_back(gen[x]);
    }
    require_permutation(values, values_in(0, 15));
  }
}

// the cycle starts again when the constraints change
BOOST_AUTO_TEST_CASE(randc_constraints_change) {
  for (int trial = 0; trial < 10; ++trial) {
    Variable<unsigned char> x;
    Generator gen;
    gen("lt8", x < 8u);
    gen("lt4", x < 4u);
    gen.disableConstraint("lt4");
    gen.randc(x);

    std::vector<int> values = generate(gen, x, 6);
    BOOST_REQUIRE_EQUAL(std::set<int>(values.begin(), values.end()).size(), 6u);
    gen.enableConstraint("lt4");
    require_permutation(generate(gen, x, 4), values_in(0, 3));
    require_permutation(generate(gen, x, 4), values_in(0, 3));
  }
}

// a cycle of two randc variables, whose values differ
BOOST_AUTO_TEST_CASE(randc_two_variables) {
  Variable<unsigned char> x;
  Variable<unsigned char> y;
  Generator gen(x < 4u && y < 4u && x != y);
  gen.randc(x).randc(y);

  for (int cycle = 0; cycle < 5; ++cycle) {
    std::vector<int> values;
    for (int i = 0; i < 4; ++i) {
      BOOST_REQUIRE(gen.next());
      BOOST_REQUIRE_NE(gen[x], gen[y]);
      values.push_back(gen[x]);
    }
    require_permutation(values, values_in(0, 3));
  }
}

BOOST_AUTO_TEST_CASE(randc_bool) {
  Variable<bool> b;
  Generator gen;
  gen.randc(b);

  for (int cycle = 0; cycle < 10; ++cycle) {
    BOOST_REQUIRE(gen.next());
    bool const first = gen[b];
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_NE(gen[b], first);
  }
}

// the cycle is kept for the variable in another generator with the same constraints
BOOST_AUTO_TEST_CASE(randc_other_generator) {
  Variable<unsigned char> x;
  std::vector<int> values;
  for (int i = 0; i < 10; ++i) {
    Generator gen(x < 10u);
    gen.randc(x);
    values.push_back(generate(gen, x, 1).front());
  }
  require_permutation(values, values_in(0, 9));
}

BOOST_AUTO_TEST_CASE(randc_unsatisfiable) {
  Variable<unsigned char> x;
  Generator gen(x > 3u && x < 2u);
  gen.randc(x);
  BOOST_REQUIRE(!gen.next());
}

BOOST_AUTO_TEST_CASE(randc_off) {
  Variable<unsigned char> x;
  Generator gen(x < 2u);
  gen.randc(x);
  gen.randc(x, false);
  for (int i = 0; i < 4; ++i) BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE(!randc_state(x.id()));
}

// the width of a randc variable is limited
BOOST_AUTO_TEST_CASE(randc_width_limit) {
  Variable<unsigned> u;
  Variable<unsigned short> w;
  Generator gen;
  BOOST_REQUIRE_THROW(gen.randc(u), std::runtime_error);
  BOOST_REQUIRE_NO_THROW(gen.randc(w));
}

// a randv of a rand_obj
struct randc_obj : public rand_obj {
  randv<unsigned char> a;
  explicit randc_obj(rand_obj* parent = 0) : rand_obj(parent), a(this) {
    constraint(a() < 5u);
    constraint.randc(a());
  }
};

BOOST_AUTO_TEST_CASE(randc_randv) {
  randc_obj obj;
  for (int cycle = 0; cycle < 3; ++cycle) {
    std::vector<int> values;
    for (int i = 0; i < 5; ++i) {
      BOOST_REQUIRE(obj.next());
      values.push_back(obj.a);
    }
    require_permutation(values, values_in(0, 4));
  }
}

BOOST_AUTO_TEST_SUITE_END()  // Randc_t

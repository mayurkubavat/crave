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

#include <map>
#include <set>
#include <stdexcept>

using namespace crave;

namespace {
// Pearson's chi-square statistic of the counts against equal probabilities
double scope_chi_square(std::map<int, unsigned> const& counts, unsigned bins, unsigned total) {
  double const expected = double(total) / bins;
  double result = 0;
  for (std::pair<const int, unsigned> const& c : counts) result += (c.second - expected) * (c.second - expected) / expected;
  return result + (bins - counts.size()) * expected;
}

struct scope_item : public crv_sequence_item {
  crv_variable<unsigned> a;
  crv_constraint c{a() < 10u};
  int local = 0;

  scope_item(crv_object_name) {}

  bool randomize_local() { return crave::randomize(local).with(reference(local) > 100, reference(local) < 110); }
};
}  // namespace

BOOST_FIXTURE_TEST_SUITE(ExperimentalScopeRandomize_t, Context_Fixture)

// IEEE 1800-2017 18.12: std::randomize() without constraints gives random values
BOOST_AUTO_TEST_CASE(scope_randomize_without_constraints) {
  set_global_seed(31);
  int x = 0;
  bool b = false;
  unsigned char c = 0;
  std::set<int> xs;
  std::set<bool> bs;
  std::map<int, unsigned> cs;
  unsigned const total = 25600;
  for (unsigned i = 0; i < total; ++i) {
    BOOST_REQUIRE(crave::randomize(x, b, c));
    xs.insert(x);
    bs.insert(b);
    ++cs[c];
  }
  BOOST_REQUIRE_GT(xs.size(), total - 10);
  BOOST_REQUIRE_EQUAL(bs.size(), 2u);
  // 255 degrees of freedom, 330.5 is the critical value for p = 0.001
  BOOST_REQUIRE_EQUAL(cs.size(), 256u);
  BOOST_REQUIRE_LT(scope_chi_square(cs, 256, total), 330.5);
}

// IEEE 1800-2017 18.12: std::randomize() with {...}, the variables are random in the constraints
BOOST_AUTO_TEST_CASE(scope_randomize_with) {
  set_global_seed(32);
  int x = 0;
  unsigned short y = 0;
  std::set<int> xs;
  for (unsigned i = 0; i < 200; ++i) {
    BOOST_REQUIRE(crave::randomize(x, y).with(reference(x) < reference(y), reference(x) >= -5, reference(x) < 5,
                                              reference(y) <= 10));
    BOOST_REQUIRE(x >= -5 && x < 5 && x < y && y <= 10);
    xs.insert(x);
  }
  BOOST_REQUIRE_EQUAL(xs.size(), 10u);
}

// the values of a variable with constraints on it alone have equal probability
BOOST_AUTO_TEST_CASE(scope_randomize_uniform) {
  set_global_seed(33);
  unsigned char c = 0;
  std::map<int, unsigned> counts;
  unsigned const total = 1600;
  for (unsigned i = 0; i < total; ++i) {
    BOOST_REQUIRE(crave::randomize(c).with(reference(c) < 16));
    BOOST_REQUIRE_LT(c, 16);
    ++counts[c];
  }
  // 15 degrees of freedom, 37.70 is the critical value for p = 0.001
  BOOST_REQUIRE_EQUAL(counts.size(), 16u);
  BOOST_REQUIRE_LT(scope_chi_square(counts, 16, total), 37.70);
}

// IEEE 1800-2017 18.12: if the constraints have no solution, the variables keep their values
BOOST_AUTO_TEST_CASE(scope_randomize_failure) {
  int x = 7;
  long long y = 9;
  BOOST_REQUIRE(!crave::randomize(x, y).with(reference(x) > 3, reference(x) < 2));
  BOOST_REQUIRE_EQUAL(x, 7);
  BOOST_REQUIRE_EQUAL(y, 9);
  // also if the constraints on one of the variables have a solution
  for (unsigned i = 0; i < 20; ++i) {
    BOOST_REQUIRE(!crave::randomize(x, y).with(reference(x) == 1, reference(y) > 5, reference(y) < 3));
    BOOST_REQUIRE_EQUAL(x, 7);
    BOOST_REQUIRE_EQUAL(y, 9);
  }
}

// other variables in the constraints are state variables
BOOST_AUTO_TEST_CASE(scope_randomize_state_variables) {
  int x = 0;
  int z = 4;
  BOOST_REQUIRE(crave::randomize(x).with(reference(x) == reference(z) + 1));
  BOOST_REQUIRE_EQUAL(x, 5);
  BOOST_REQUIRE_EQUAL(z, 4);
  z = 10;
  BOOST_REQUIRE(crave::randomize(x).with(reference(x) == reference(z) + 1));
  BOOST_REQUIRE_EQUAL(x, 11);
  BOOST_REQUIRE_EQUAL(z, 10);
  BOOST_REQUIRE(!crave::randomize(x).with(reference(z) == 0));
  BOOST_REQUIRE_EQUAL(x, 11);
}

// a variable, which is not constrained, takes random values
BOOST_AUTO_TEST_CASE(scope_randomize_unconstrained_variable) {
  set_global_seed(34);
  short x = 0;
  bool b = false;
  unsigned ones = 0;
  for (unsigned i = 0; i < 1000; ++i) {
    BOOST_REQUIRE(crave::randomize(x, b).with(reference(x) == 3));
    BOOST_REQUIRE_EQUAL(x, 3);
    ones += b;
  }
  BOOST_REQUIRE_GT(ones, 430u);
  BOOST_REQUIRE_LT(ones, 570u);
}

// variables passed by reference, and the hidden name inside a crv_sequence_item
BOOST_AUTO_TEST_CASE(scope_randomize_references) {
  scope_item item("item");
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_LT(item.a, 10u);
  BOOST_REQUIRE(item.randomize_local());
  BOOST_REQUIRE(item.local > 100 && item.local < 110);

  int values[2] = {0, 0};
  int& first = values[0];
  BOOST_REQUIRE(crave::randomize(first, values[1]).with(reference(values[1]) == reference(first) + 1, reference(first) > 0,
                                                       reference(first) < 5));
  BOOST_REQUIRE(values[0] > 0 && values[0] < 5);
  BOOST_REQUIRE_EQUAL(values[1], values[0] + 1);
}

// a variable passed twice is randomized once, and keeps its value on failure
BOOST_AUTO_TEST_CASE(scope_randomize_same_variable) {
  int x = 7;
  BOOST_REQUIRE(!crave::randomize(x, x).with(reference(x) > 5, reference(x) < 3));
  BOOST_REQUIRE_EQUAL(x, 7);
  for (unsigned i = 0; i < 50; ++i) {
    BOOST_REQUIRE(crave::randomize(x, x).with(reference(x) >= 10, reference(x) <= 12));
    BOOST_REQUIRE(x >= 10 && x <= 12);
  }
}

// the references to the variables, which may not live longer, are removed from the variable container
BOOST_AUTO_TEST_CASE(scope_randomize_no_dangling_references) {
  VariableContainer* vars = variable_container();
  int z = 3;
  int x = 0;
  BOOST_REQUIRE(crave::randomize(x).with(reference(x) > reference(z)));
  std::size_t const reads = vars->read_references.size();
  std::size_t const writes = vars->write_references.size();
  std::size_t const dists = vars->dist_references.size();
  std::size_t const variables = vars->variables.size();
  for (unsigned i = 0; i < 100; ++i) {
    int y = 0;
    BOOST_REQUIRE(crave::randomize(y).with(reference(y) > reference(z), reference(y) < 6));
    BOOST_REQUIRE(y == 4 || y == 5);
    BOOST_REQUIRE(!crave::randomize(y).with(reference(y) < reference(z), reference(y) > 7));
  }
  BOOST_REQUIRE_EQUAL(vars->read_references.size(), reads);
  BOOST_REQUIRE_EQUAL(vars->write_references.size(), writes);
  BOOST_REQUIRE_EQUAL(vars->dist_references.size(), dists);
  BOOST_REQUIRE_EQUAL(vars->variables.size(), variables);
}

BOOST_AUTO_TEST_SUITE_END()  // ExperimentalScopeRandomize_t

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
#include <vector>

using namespace crave;

namespace {
// Pearson's chi-square statistic of the counts against equal probabilities
double chi_square(std::map<int, unsigned> const& counts, unsigned bins, unsigned total) {
  double const expected = double(total) / bins;
  double result = 0;
  unsigned seen = 0;
  for (std::pair<const int, unsigned> const& c : counts) {
    result += (c.second - expected) * (c.second - expected) / expected;
    seen += 1;
  }
  result += (bins - seen) * expected;  // the bins without counts
  return result;
}
}  // namespace

BOOST_FIXTURE_TEST_SUITE(SolveBefore_t, Context_Fixture)

// IEEE 1800-2017 18.5.10: s is true with probability 1/2 instead of 2^-32
BOOST_AUTO_TEST_CASE(solve_before_implication) {
  set_global_seed(11);
  Variable<bool> s;
  Variable<unsigned> d;
  Generator gen(if_then(s, d == 0u));
  gen.solve_before(s, d);

  unsigned const total = 2000;
  unsigned ones = 0;
  for (unsigned i = 0; i < total; ++i) {
    BOOST_REQUIRE(gen.next());
    if (gen[s]) {
      ++ones;
      BOOST_REQUIRE_EQUAL(gen[d], 0u);
    }
  }
  // 1/2 with a standard deviation of about 22, the bounds are more than 4 of them away
  BOOST_REQUIRE_GT(ones, 900u);
  BOOST_REQUIRE_LT(ones, 1100u);
}

// the earlier variable is uniform over the values, for which the constraints have a solution,
// although these values have very different numbers of solutions
BOOST_AUTO_TEST_CASE(solve_before_uniform_projection) {
  set_global_seed(12);
  Variable<unsigned char> x;
  Variable<unsigned short> y;
  Generator gen(x < 8 && y < (x + 1) * 1000);
  gen.solve_before(x, y);

  unsigned const total = 4000;
  std::map<int, unsigned> counts;
  for (unsigned i = 0; i < total; ++i) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_LT(gen[x], 8);
    BOOST_REQUIRE_LT(gen[y], (gen[x] + 1) * 1000);
    ++counts[gen[x]];
  }
  BOOST_REQUIRE_EQUAL(counts.size(), 8u);
  // 7 degrees of freedom, 24.32 is the critical value for p = 0.001
  BOOST_REQUIRE_LT(chi_square(counts, 8, total), 24.32);
}

// few values of a 16-bit variable have a solution, they are found by listing them
BOOST_AUTO_TEST_CASE(solve_before_sparse_values) {
  set_global_seed(13);
  Variable<unsigned short> x;
  Variable<unsigned> y;
  Generator gen(x % 1000 == 7 && y > x && y < 70000u);
  gen.solve_before(x, y);

  unsigned const total = 66 * 50;
  std::map<int, unsigned> counts;
  for (unsigned i = 0; i < total; ++i) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_EQUAL(gen[x] % 1000, 7);
    BOOST_REQUIRE(gen[y] > gen[x] && gen[y] < 70000u);
    ++counts[gen[x]];
  }
  BOOST_REQUIRE_EQUAL(counts.size(), 66u);
  // 65 degrees of freedom, 107.0 is the critical value for p = 0.001
  BOOST_REQUIRE_LT(chi_square(counts, 66, total), 107.0);
}

// with too many values to list, the values are taken from a solution, the solutions stay the same
BOOST_AUTO_TEST_CASE(solve_before_many_sparse_values) {
  set_global_seed(14);
  Variable<unsigned> x;
  Variable<unsigned> y;
  Generator gen(x % 4096u == 0u && y == x + 1u);
  gen.solve_before(x, y);

  std::set<unsigned> values;
  for (unsigned i = 0; i < 8; ++i) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_EQUAL(gen[x] % 4096u, 0u);
    BOOST_REQUIRE_EQUAL(gen[y], gen[x] + 1u);
    values.insert(gen[x]);
  }
  BOOST_REQUIRE_GT(values.size(), 1u);
}

// the ordering does not change the set of solutions
BOOST_AUTO_TEST_CASE(solve_before_same_solutions) {
  set_global_seed(15);
  Variable<bool> s;
  Variable<int> d;
  Generator gen(if_then(s, d == 0) && d > 5);
  gen.solve_before(s, d);
  for (unsigned i = 0; i < 100; ++i) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE(!gen[s]);
    BOOST_REQUIRE_GT(gen[d], 5);
  }

  Generator unsat(s && d == 0 && d > 5);
  unsat.solve_before(s, d);
  BOOST_REQUIRE(!unsat.next());
}

// variables of a level are chosen together, a chain of orderings is solved level by level
BOOST_AUTO_TEST_CASE(solve_before_levels) {
  set_global_seed(16);
  Variable<bool> a;
  Variable<bool> b;
  Variable<signed char> c;
  Variable<unsigned> d;
  Generator gen(if_then(a || b, c == -1) && if_then(c < 0, d == 0u));
  gen.solve_before(a, c).solve_before(b, c).solve_before(c, d);

  unsigned const total = 2000;
  std::map<int, unsigned> counts;
  unsigned negative = 0;
  for (unsigned i = 0; i < total; ++i) {
    BOOST_REQUIRE(gen.next());
    if (gen[a] || gen[b]) BOOST_REQUIRE_EQUAL(gen[c], -1);
    if (gen[c] < 0) BOOST_REQUIRE_EQUAL(gen[d], 0u);
    ++counts[gen[a] * 2 + gen[b]];
    if (!gen[a] && !gen[b] && gen[c] < 0) ++negative;
  }
  // (a, b) is uniform over its 4 values, 3 degrees of freedom, 16.27 is the critical value for p = 0.001
  BOOST_REQUIRE_EQUAL(counts.size(), 4u);
  BOOST_REQUIRE_LT(chi_square(counts, 4, total), 16.27);
  // without a and b, c is uniform over its 256 values, half of them are negative
  BOOST_REQUIRE_GT(negative, counts[0] / 2 - 4 * 16);
  BOOST_REQUIRE_LT(negative, counts[0] / 2 + 4 * 16);
}

// a variable with a distribution takes the values of its distribution, which have a solution
BOOST_AUTO_TEST_CASE(solve_before_distribution) {
  set_global_seed(17);
  Variable<unsigned char> x;
  Variable<unsigned> y;
  Generator gen(dist(x, distribution<unsigned char>::create(weighted_range<unsigned char>(0, 0, 3))(
                            weighted_range<unsigned char>(1, 3, 3))));
  gen(if_then(x == 3, y == 0u));
  gen.solve_before(x, y);

  unsigned const total = 2000;
  std::map<int, unsigned> counts;
  for (unsigned i = 0; i < total; ++i) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_LE(gen[x], 3);
    ++counts[gen[x]];
  }
  // x is 0 with probability 1/2, 1 to 3 with 1/6 each
  BOOST_REQUIRE_GT(counts[0], 900u);
  BOOST_REQUIRE_LT(counts[0], 1100u);
  BOOST_REQUIRE_GT(counts[3], 333u - 4 * 17);
  BOOST_REQUIRE_LT(counts[3], 333u + 4 * 17);
}

// the ordering of variables in different partitions has no effect
BOOST_AUTO_TEST_CASE(solve_before_other_partition) {
  set_global_seed(18);
  Variable<unsigned char> x;
  Variable<unsigned char> y;
  Generator gen(x < 10 && y > 200);
  gen.solve_before(x, y);
  for (unsigned i = 0; i < 20; ++i) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE(gen[x] < 10 && gen[y] > 200);
  }
}

// the BDD backend chooses the earlier variable in the same way
BOOST_AUTO_TEST_CASE(solve_before_bdd_backend) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;
  set_solver_backend("Cudd");
  set_global_seed(20);
  Variable<bool> s;
  Variable<unsigned> d;
  Variable<unsigned short> x;
  Variable<unsigned> y;
  Generator gen(if_then(s, d == 0u));
  gen(x % 1000 == 7 && y > x && y < 70000u);
  gen.solve_before(s, d).solve_before(x, y);

  unsigned const total = 66 * 50;
  unsigned ones = 0;
  std::map<int, unsigned> counts;
  for (unsigned i = 0; i < total; ++i) {
    BOOST_REQUIRE(gen.next());
    if (gen[s]) {
      ++ones;
      BOOST_REQUIRE_EQUAL(gen[d], 0u);
    }
    BOOST_REQUIRE_EQUAL(gen[x] % 1000, 7);
    BOOST_REQUIRE(gen[y] > gen[x] && gen[y] < 70000u);
    ++counts[gen[x]];
  }
  // 1/2 with a standard deviation of about 29, the bounds are more than 4 of them away
  BOOST_REQUIRE_GT(ones, 1530u);
  BOOST_REQUIRE_LT(ones, 1770u);
  BOOST_REQUIRE_EQUAL(counts.size(), 66u);
  BOOST_REQUIRE_LT(chi_square(counts, 66, total), 107.0);
}

// IEEE 1800-2017 18.5.10: the ordering must not be cyclic
BOOST_AUTO_TEST_CASE(solve_before_cyclic) {
  Variable<unsigned char> x;
  Variable<unsigned char> y;
  Variable<unsigned char> z;
  Generator gen(x < y && y < z);
  BOOST_REQUIRE_THROW(gen.solve_before(x, x), std::invalid_argument);
  gen.solve_before(x, y).solve_before(y, z).solve_before(z, x);
  BOOST_REQUIRE_THROW(gen.next(), std::runtime_error);
  BOOST_REQUIRE_THROW(gen.next(), std::runtime_error);  // also the next time
}

// IEEE 1800-2017 18.5.10: randc variables are not allowed in the ordering
BOOST_AUTO_TEST_CASE(solve_before_randc) {
  Variable<unsigned char> x;
  Variable<unsigned char> y;
  Generator gen(x < y);
  gen.randc(x);
  gen.solve_before(x, y);
  BOOST_REQUIRE_THROW(gen.next(), std::runtime_error);
  set_randc(x.id(), false);
}

// the ordering of a rand_obj is combined with the ones of its children
BOOST_AUTO_TEST_CASE(solve_before_rand_obj) {
  set_global_seed(19);
  struct item : rand_obj {
    randv<bool> s;
    randv<unsigned> d;
    explicit item(rand_obj* parent = 0) : rand_obj(parent), s(this), d(this) {
      constraint(if_then(s(), d() == 0u));
      constraint.solve_before(s(), d());
    }
  };
  item obj;
  unsigned ones = 0;
  for (unsigned i = 0; i < 1000; ++i) {
    BOOST_REQUIRE(obj.next());
    if (obj.s) ones++;
  }
  BOOST_REQUIRE_GT(ones, 400u);
  BOOST_REQUIRE_LT(ones, 600u);
}

BOOST_AUTO_TEST_SUITE_END()  // SolveBefore_t

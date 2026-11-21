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

#include <crave/ConstrainedRandom.hpp>
#include <lib/BddSampler.hpp>

#include <cstdint>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace crave;

BOOST_FIXTURE_TEST_SUITE(BddSampler_t, Context_Fixture)

namespace {
// id <= bound for an unsigned variable
NodePtr less_equal(unsigned id, unsigned bitsize, uint64_t bound) {
  NodePtr var(new VariableExpr(id, bitsize, false));
  NodePtr c(new Constant(bound, bitsize, false));
  return new LessEqualOpr(var, c);
}

// A constraint without sampler, if the limits are set by no_complexity_limit: (id * id) * id != value
NodePtr cube_not_equal(unsigned id, uint64_t value) {
  NodePtr var(new VariableExpr(id, 16, false));
  NodePtr c(new Constant(value, 16, false));
  return new NotEqualOpr(new MultipliesOpr(new MultipliesOpr(var, var), var), c);
}

// raises the complexity limit, so that only the (lowered) limit for the size of a BDD is left
struct no_complexity_limit {
  no_complexity_limit() : limit(VariableDefaultSolver::complexity_limit_for_bdd), max_nodes(BddSampler::max_nodes) {
    VariableDefaultSolver::complexity_limit_for_bdd = 1000000;
    BddSampler::max_nodes = 2000;
  }
  ~no_complexity_limit() {
    VariableDefaultSolver::complexity_limit_for_bdd = limit;
    BddSampler::max_nodes = max_nodes;
  }
  unsigned limit;
  unsigned max_nodes;
};

// A bit which does not matter for the constraints is not '0' or '1', the caller has to choose.
uint64_t to_value(std::string const& bits) {
  static uint64_t state = 1;
  uint64_t value = 0;
  for (char c : bits) {
    state = state * 6364136223846793005ull + 1442695040888963407ull;
    bool bit = (c == '0' || c == '1') ? (c == '1') : (state >> 63);
    value = (value << 1) | bit;
  }
  return value;
}
}

BOOST_AUTO_TEST_CASE(samples_all_solutions) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  std::shared_ptr<BddSampler> sampler = BddSampler::get({less_equal(1001, 32, 10)});
  BOOST_REQUIRE(sampler);
  std::set<uint64_t> values;
  for (int i = 0; i < 200; i++) {
    std::string bits;
    BOOST_REQUIRE(sampler->sample(&bits));
    BOOST_REQUIRE_EQUAL(bits.size(), 32);
    BOOST_REQUIRE_LE(to_value(bits), 10);
    values.insert(to_value(bits));
  }
  BOOST_REQUIRE_EQUAL(values.size(), 11);
}

// the number of solutions does not fit into 32 or 64 bits
BOOST_AUTO_TEST_CASE(samples_wide_variable) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  NodePtr var(new VariableExpr(1002, 64, false));
  NodePtr c(new Constant(100, 64, false));
  std::shared_ptr<BddSampler> sampler = BddSampler::get({new GreaterEqualOpr(var, c)});
  BOOST_REQUIRE(sampler);
  std::set<uint64_t> low_words;
  std::vector<int> ones(64, 0);
  const int total = 1000;
  for (int i = 0; i < total; i++) {
    std::string bits;
    BOOST_REQUIRE(sampler->sample(&bits));
    BOOST_REQUIRE_EQUAL(bits.size(), 64);
    uint64_t value = to_value(bits);
    BOOST_REQUIRE_GE(value, 100);
    for (int b = 0; b < 64; b++)
      if ((value >> b) & 1) ones[b]++;
    low_words.insert(value & 0xffffffffu);
  }
  BOOST_REQUIRE_GE(low_words.size(), total * 99 / 100);
  // every bit is 1 for about half of the solutions
  for (int b = 0; b < 64; b++) {
    BOOST_REQUIRE_GT(ones[b], total * 4 / 10);
    BOOST_REQUIRE_LT(ones[b], total * 6 / 10);
  }
}

BOOST_AUTO_TEST_CASE(shared_by_constraints_of_the_same_shape) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  std::shared_ptr<BddSampler> sampler = BddSampler::get({less_equal(1003, 32, 1000)});
  BOOST_REQUIRE(sampler);
  std::size_t count = BddSampler::count();

  // other variables, same constraint
  std::vector<std::shared_ptr<BddSampler> > samplers;
  for (unsigned id = 2000; id < 2300; id++) {
    samplers.push_back(BddSampler::get({less_equal(id, 32, 1000)}));
    BOOST_REQUIRE_EQUAL(samplers.back(), sampler);
  }
  BOOST_REQUIRE_EQUAL(BddSampler::count(), count);

  // another bitsize or constant is another shape
  BOOST_REQUIRE_NE(BddSampler::get({less_equal(1003, 16, 1000)}), sampler);
  BOOST_REQUIRE_NE(BddSampler::get({less_equal(1003, 32, 1001)}), sampler);
}

// samplers which are not used anymore do not pile up
BOOST_AUTO_TEST_CASE(unused_samplers_are_released) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  for (unsigned bound = 5000; bound < 5100; bound++) BOOST_REQUIRE(BddSampler::get({less_equal(1004, 32, bound)}));
  BOOST_REQUIRE_LE(BddSampler::count(), BddSampler::max_recent + 1);
}

BOOST_AUTO_TEST_CASE(no_sampler_for_complex_constraints) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  NodePtr var(new VariableExpr(1005, 32, false));
  NodePtr factor(new Constant(12345, 32, false));
  NodePtr product(new Constant(0x12340000, 32, false));
  NodePtr forty_nine(new Constant(49, 32, false));

  // over the complexity limit
  NodePtr mul_const(new EqualOpr(new MultipliesOpr(var, factor), product));
  NodePtr mul_var(new NotEqualOpr(new MultipliesOpr(var, var), forty_nine));
  BOOST_REQUIRE(!BddSampler::get({mul_const}));
  BOOST_REQUIRE(!BddSampler::get({mul_var}));

  // complex constraints are left out, if there are simple ones as well
  BOOST_REQUIRE_EQUAL(BddSampler::get({mul_var, less_equal(1005, 32, 77)}), BddSampler::get({less_equal(1005, 32, 77)}));

  // over the limit for the size of a BDD, if the estimation of the complexity is wrong
  no_complexity_limit guard;
  NodePtr other_product(new Constant(0x12350000, 32, false));
  BOOST_REQUIRE(!BddSampler::get({new EqualOpr(new MultipliesOpr(var, factor), other_product)}));
}


// The values generated after getting a sampler must not depend on whether it had to be built.
BOOST_AUTO_TEST_CASE(get_leaves_random_numbers_unchanged) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  std::mt19937 before = *crave::rng.get();

  // a new sampler
  BOOST_REQUIRE(!BddSampler::get({less_equal(1006, 32, 424242)}, false));
  std::shared_ptr<BddSampler> sampler = BddSampler::get({less_equal(1006, 32, 424242)});
  BOOST_REQUIRE(sampler);
  BOOST_REQUIRE(before == *crave::rng.get());

  // a shape without sampler
  {
    no_complexity_limit guard;
    BOOST_REQUIRE(!BddSampler::get({cube_not_equal(1006, 4242)}));
  }
  BOOST_REQUIRE(before == *crave::rng.get());

  // an existing sampler
  BOOST_REQUIRE_EQUAL(BddSampler::get({less_equal(1007, 32, 424242)}), sampler);
  BOOST_REQUIRE(before == *crave::rng.get());

  // sampling does
  std::string bits;
  BOOST_REQUIRE(sampler->sample(&bits));
  BOOST_REQUIRE(!(before == *crave::rng.get()));
}

// Shapes without sampler are remembered and do not displace the recently used samplers.
BOOST_AUTO_TEST_CASE(failed_shapes_do_not_displace_samplers) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  BOOST_REQUIRE(BddSampler::get({less_equal(1008, 32, 515151)}));  // only kept as recently used
  {
    no_complexity_limit guard;
    for (unsigned value = 100; value < 100 + 2 * BddSampler::max_recent; value++)
      BOOST_REQUIRE(!BddSampler::get({cube_not_equal(1008, value)}));
  }
  BOOST_REQUIRE(BddSampler::get({less_equal(1008, 32, 515151)}, false));
}

// A shape which failed with a lower limit for the size of a BDD is tried again with a higher one.
BOOST_AUTO_TEST_CASE(failed_shapes_depend_on_max_nodes) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  NodePtr var(new VariableExpr(1009, 16, false));
  NodePtr factor(new Constant(12345, 16, false));
  NodePtr product(new Constant(0x1234, 16, false));
  std::vector<NodePtr> constraints = {new EqualOpr(new MultipliesOpr(var, factor), product)};

  unsigned max_nodes = BddSampler::max_nodes;
  BddSampler::max_nodes = 0;
  BOOST_REQUIRE(!BddSampler::get(constraints));
  BOOST_REQUIRE(!BddSampler::get(constraints));
  BddSampler::max_nodes = max_nodes;
  BOOST_REQUIRE(BddSampler::get(constraints));
}

BOOST_AUTO_TEST_SUITE_END()  // BddSampler_t

//  vim: ft=cpp:ts=2:sw=2:expandtab

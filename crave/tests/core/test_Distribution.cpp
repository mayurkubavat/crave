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

#include <boost/format.hpp>

#include <memory>
#include <set>
#include <vector>
#include <iostream>

// using namespace std;
using namespace crave;

using boost::format;

BOOST_FIXTURE_TEST_SUITE(Distribution_t, Context_Fixture)

// the weight of a range is the number of its values, also if it does not fit into the type of the values
BOOST_AUTO_TEST_CASE(range_weight_t) {
  BOOST_REQUIRE_EQUAL(range<int>(-5, 5).weight_, 11u);
  BOOST_REQUIRE_EQUAL(range<int>(-2147483647 - 1, 2147483646).weight_, 4294967295ull);
  BOOST_REQUIRE_EQUAL(range<int>(-1, 2147483647).weight_, 2147483649ull);
  BOOST_REQUIRE_EQUAL(range<unsigned char>(0, 255).weight_, 256u);
  BOOST_REQUIRE_EQUAL(range<short>(-32768, 32767).weight_, 65536u);
  BOOST_REQUIRE_EQUAL(range<long long>(-9223372036854775807LL - 1, -1).weight_, 9223372036854775808ull);
  BOOST_REQUIRE_EQUAL(range<unsigned>(1, 4294967295u).weight_, 4294967295ull);
}

BOOST_AUTO_TEST_CASE(variable_dist_t1) {
  Variable<int> v;
  Generator gen;
  gen(dist(v, distribution<int>::create(range<int>(0, 5))(range<int>(50, 65))(range<int>(100, 125))));

  std::map<int, int> s;
  int total = 10000;
  for (int i = 0; i < total; i++) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE((0 <= gen[v] && gen[v] <= 5) || (50 <= gen[v] && gen[v] <= 65) || (100 <= gen[v] && gen[v] <= 125));
    ++s[gen[v]];
  }
  for (int i = 0; i <= 200; i++)
    if ((0 <= i && i <= 5) || (50 <= i && i <= 65) || (100 <= i && i <= 125))
      BOOST_REQUIRE(s.find(i) != s.end());
}

BOOST_AUTO_TEST_CASE(variable_dist_t2) {
  Variable<int> v;
  Generator gen;
  gen(dist(v, distribution<int>::create(range<int>(0, 10))(range<int>(50, 75))(range<int>(30, 51))));
  std::map<int, int> counts;
  int const total = 20000;
  int overlap_hits = 0;
  int non_overlap_high_hits = 0;
  for (int i = 0; i < total; ++i) {
    BOOST_REQUIRE(gen.next());
    int const val = gen[v];
    BOOST_REQUIRE((0 <= val && val <= 10) || (30 <= val && val <= 75));
    ++counts[val];
    if (val == 50 || val == 51) ++overlap_hits;
    if (52 <= val && val <= 75) ++non_overlap_high_hits;
  }
  double const overlap_avg = overlap_hits / 2.0;
  double const non_overlap_avg = non_overlap_high_hits / 24.0;
  BOOST_REQUIRE_GT(overlap_avg, non_overlap_avg * 1.4);
}

BOOST_AUTO_TEST_CASE(variable_dist_t3) {
  Variable<char> v;
  Generator gen;
  gen(dist(v, distribution<char>::create(weighted_range<char>(1, 5, 50))(weighted_range<char>(10, 20, 20))(
                  weighted_range<char>(-50, -50, 30))));
  int cnt1 = 0, cnt2 = 0, cnt3 = 0;
  int total = 50000;
  for (int i = 0; i < total; i++) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE((1 <= gen[v] && gen[v] <= 5) || (10 <= gen[v] && gen[v] <= 20) || (gen[v] == -50));
    if (1 <= gen[v] && gen[v] <= 5) cnt1++;
    if (10 <= gen[v] && gen[v] <= 20) cnt2++;
    if (gen[v] == -50) cnt3++;
  }
  BOOST_REQUIRE_LT(cnt2, cnt3);
  BOOST_REQUIRE_LT(cnt3, cnt1);
}

BOOST_AUTO_TEST_CASE(variable_dist_t4) {
  Variable<int> v;
  Generator gen;
  gen("x", dist(v, distribution<int>::create(range<int>(0, 10))(range<int>(50, 75))(range<int>(100, 200))));
  gen("y", dist(v, distribution<int>::simple_range(5000, 6000)));
  gen.disableConstraint("x");
  int total = 10000;
  for (int i = 0; i < total; i++) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE(5000 <= gen[v] && gen[v] <= 6000);
  }
}

BOOST_AUTO_TEST_CASE(dist_of_boolean25) {
  Variable<bool> a;
  int counter = 0;

  Generator gen;
  gen(dist(a, distribution<bool>::create(0.25)));
  for (unsigned i = 0; i < 1000; i++) {
    BOOST_REQUIRE(gen.next());
    if (gen[a]) {
      ++counter;
    } else {
      --counter;
    }
  }

  BOOST_REQUIRE_LT(counter, 0);
}

BOOST_AUTO_TEST_CASE(dist_of_boolean50) {
  Variable<bool> a;
  int counter = 0;

  Generator gen;
  gen(dist(a, distribution<bool>::create(0.5)));
  for (unsigned i = 0; i < 1000; i++) {
    BOOST_REQUIRE(gen.next());
    if (gen[a]) {
      ++counter;
    } else {
      --counter;
    }
  }

  BOOST_REQUIRE_LT(counter, 280);
  BOOST_REQUIRE_GT(counter, -280);
}

BOOST_AUTO_TEST_CASE(dist_of_boolean75) {
  Variable<bool> a;
  int counter = 0;

  Generator gen;
  gen(dist(a, distribution<bool>::create(0.75)));
  for (unsigned i = 0; i < 1000; i++) {
    BOOST_REQUIRE(gen.next());
    if (gen[a]) {
      ++counter;
    } else {
      --counter;
    }
  }

  BOOST_REQUIRE_GT(counter, 0);
}

BOOST_AUTO_TEST_CASE(single_variable_constraint_spread) {
  // The solutions of constraints on a single variable without a distribution
  // are sampled with a BDD, if CUDD is available.
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  randv<unsigned> x(NULL);
  Generator gen;
  gen(x() <= 10);

  std::set<unsigned> values;
  for (int i = 0; i < 200; i++) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_LE(x, 10);
    values.insert(x);
  }
  BOOST_REQUIRE_GE(values.size(), 9);
}

BOOST_AUTO_TEST_CASE(single_variable_constraint_spread_64_bits) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  randv<uint64_t> x(NULL);
  Generator gen;
  gen(x() >= 100);

  std::set<uint32_t> low_words;
  for (int i = 0; i < 200; i++) {
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_GE(x, 100);
    low_words.insert(static_cast<uint32_t>(x));
  }
  BOOST_REQUIRE_GE(low_words.size(), 190);
}

// The samplers are shared, so many objects with the same constraints do not need more of them.
struct s_many_objects : public rand_obj {
  randv<unsigned> addr, len;
  s_many_objects() : addr(this), len(this) {
    constraint(addr() <= 0xffff);
    constraint(1 <= len() && len() <= 64);
  }
};

BOOST_AUTO_TEST_CASE(many_objects_share_samplers) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  std::vector<std::unique_ptr<s_many_objects> > objects;
  objects.emplace_back(new s_many_objects());
  BOOST_REQUIRE(objects.back()->next());
  std::size_t count = BddSampler::count();
  for (int i = 0; i < 100; i++) {
    objects.emplace_back(new s_many_objects());
    BOOST_REQUIRE(objects.back()->next());
    BOOST_REQUIRE_LE(objects.back()->addr, 0xffff);
  }
  BOOST_REQUIRE_EQUAL(BddSampler::count(), count);
}

BOOST_AUTO_TEST_CASE(weighted_range_overlap_and_weight_fixes) {
  BOOST_REQUIRE(weighted_range<int>(1, 5, 10).overlap(weighted_range<int>(1, 5, 20)));
  BOOST_REQUIRE(weighted_range<int>(1, 5, 10).overlap(weighted_range<int>(1, 1, 20)));
  BOOST_REQUIRE(weighted_range<int>(1, 5, 10).overlap(weighted_range<int>(5, 8, 20)));
  BOOST_REQUIRE(!weighted_range<int>(1, 5, 10).overlap(weighted_range<int>(6, 8, 20)));
  BOOST_REQUIRE(!weighted_range<int>(10, 5, 10).overlap(weighted_range<int>(6, 8, 20)));

  BOOST_REQUIRE_EQUAL(range<unsigned>(0u, 4294967295u).weight_, 4294967296ULL);
  BOOST_REQUIRE_EQUAL(range<int>(-2147483647 - 1, 2147483647).weight_, 4294967296ULL);
  BOOST_REQUIRE_EQUAL(range<int>(10, 5).weight_, 0ULL);

  distribution<int64_t> d64(range<int64_t>(std::numeric_limits<int64_t>::min(),
                                           std::numeric_limits<int64_t>::max()));
  d64(weighted_value<int64_t>(0, 1));
  BOOST_REQUIRE(!d64.isAllZero());
  BOOST_REQUIRE(!d64.ranges().empty());
  (void)d64.nextValue();
}

BOOST_AUTO_TEST_CASE(dist_per_value_vs_per_range_and_truncation) {
  struct BackendGuard {
    BackendGuard() : old_sv(sv_signedness()) {}
    ~BackendGuard() {
      set_solver_backend("Z3");
      set_sv_signedness(old_sv);
    }
    bool old_sv;
  } guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);
    for (bool sv : {false, true}) {
      set_sv_signedness(sv);

      // [0,3] := 10 (total 40) vs [10,13] :/ 10 (total 10) -> ~4:1 ratio
      Variable<int> v;
      Generator gen;
      gen(dist(v, make_distribution(dist_per_value(0, 3, 10), dist_per_range(10, 13, 10))));
      int low_cnt = 0, high_cnt = 0;
      for (int i = 0; i < 5000; ++i) {
        BOOST_REQUIRE(gen.next());
        int val = gen[v];
        BOOST_REQUIRE((0 <= val && val <= 3) || (10 <= val && val <= 13));
        if (val <= 3) ++low_cnt;
        else ++high_cnt;
      }
      BOOST_REQUIRE_GT(low_cnt, high_cnt * 2);

      // [0,9] :/ 1 must not truncate 1/10 to 0
      Variable<int> v2;
      Generator gen2;
      gen2(dist(v2, make_distribution(range<int>(0, 9).per_range(1), dist_per_value(20, 1))));
      std::set<int> low_seen;
      for (int i = 0; i < 400; ++i) {
        BOOST_REQUIRE(gen2.next());
        int val = gen2[v2];
        BOOST_REQUIRE((0 <= val && val <= 9) || val == 20);
        if (val <= 9) low_seen.insert(val);
      }
      BOOST_REQUIRE_EQUAL(low_seen.size(), 10u);
    }
  }
}

BOOST_AUTO_TEST_CASE(dist_overlapping_weight_summation) {
  // Mixed := and :/ with overlap: [0,5] := 2, [4,7] :/ 4 -> per-value 0..3:2, 4..5:3, 6..7:1
  Variable<int> v;
  Generator gen;
  gen(dist(v, make_distribution(dist_per_value(0, 5, 2), dist_per_range(4, 7, 4))));
  std::map<int, int> counts;
  for (int i = 0; i < 12000; ++i) {
    BOOST_REQUIRE(gen.next());
    int val = gen[v];
    BOOST_REQUIRE(0 <= val && val <= 7);
    ++counts[val];
  }
  double avg_mid = (counts[4] + counts[5]) / 2.0;
  double avg_high = (counts[6] + counts[7]) / 2.0;
  BOOST_REQUIRE_GT(avg_mid, avg_high * 2.0);

  // Non-integer-divisible overlap + duplicate + containment + abutting
  distribution<int> d_mix(dist_per_range(0, 2, 1));
  d_mix(dist_per_range(1, 3, 2));
  d_mix(dist_per_value(0, 3, 2));
  d_mix(dist_per_value(0, 3, 4));
  d_mix(range<int>(5, 9));
  BOOST_REQUIRE_EQUAL(d_mix.ranges().size(), 2u);
  BOOST_REQUIRE_EQUAL(d_mix.ranges()[0].left_, 0);
  BOOST_REQUIRE_EQUAL(d_mix.ranges()[0].right_, 3);
  BOOST_REQUIRE_EQUAL(d_mix.ranges()[1].left_, 5);
  BOOST_REQUIRE_EQUAL(d_mix.ranges()[1].right_, 9);
}

BOOST_AUTO_TEST_CASE(dist_zero_weight_exclusion_and_all_zero) {
  // Weight-0 singleton and range excluded from both sampling and hard constraints
  {
    Variable<int> v;
    Generator gen;
    gen(dist(v, make_distribution(dist_per_value(0, 0), dist_per_value(1, 1), dist_per_value(2, 1))))(v == 0);
    BOOST_REQUIRE(!gen.next());
  }
  {
    Variable<int> v;
    Generator gen;
    gen(dist(v, make_distribution(dist_per_value(0, 0), dist_per_value(1, 1), dist_per_value(2, 10))))(v != 2);
    for (int i = 0; i < 20; ++i) {
      BOOST_REQUIRE(gen.next());
      BOOST_REQUIRE_EQUAL(gen[v], 1);
    }
  }
  // Overlapping weight-0 + positive weight
  {
    Variable<int> v;
    Generator gen;
    gen(dist(v, make_distribution(dist_per_value(0, 10, 0), dist_per_value(4, 6, 5))));
    for (int i = 0; i < 50; ++i) {
      BOOST_REQUIRE(gen.next());
      BOOST_REQUIRE(4 <= gen[v] && gen[v] <= 6);
    }
  }
  // All-zero distributions make gen.next() return false without throwing; direct nextValue() throws
  for (auto const& tag : {make_distribution(weighted_value<int>(5, 0)),
                          make_distribution(dist_per_value(1, 5, 0)),
                          make_distribution(dist_per_range(1, 5, 0)),
                          make_distribution(range<int>(10, 5))}) {
    distribution<int> raw = boost::proto::value(tag);
    BOOST_REQUIRE(raw.isAllZero());
    BOOST_REQUIRE_THROW(raw.nextValue(), std::runtime_error);

    Variable<int> v;
    Generator gen;
    gen(dist(v, tag));
    BOOST_REQUIRE(!gen.next());
  }
}

BOOST_AUTO_TEST_SUITE_END()  // Context

//  vim: ft=cpp:ts=2:sw=2:expandtab

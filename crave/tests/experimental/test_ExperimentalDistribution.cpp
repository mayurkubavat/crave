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

struct s_crv_variable_dist_t1 : public crv_sequence_item {
  s_crv_variable_dist_t1(crv_object_name) {}
  crv_variable<int> v;
  crv_constraint con = {
      dist(v(), make_distribution(range<int>(0, 5), range<int>(50, 65), range<int>(100, 125)))};
};

BOOST_AUTO_TEST_CASE(crv_variable_dist_t1) {
  s_crv_variable_dist_t1 item("item");
  std::map<int, int> s;
  int total = 100000;
  for (int i = 0; i < total; i++) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE((0 <= item.v && item.v <= 5) || (50 <= item.v && item.v <= 65) || (100 <= item.v && item.v <= 125));
    ++s[item.v];
  }
  for (int i = 0; i <= 200; i++)
    if ((0 <= i && i <= 5) || (50 <= i && i <= 65) || (100 <= i && i <= 125))
      BOOST_REQUIRE(s.find(i) != s.end());
}

struct s_crv_variable_dist_t2 : public crv_sequence_item {
  s_crv_variable_dist_t2(crv_object_name) {}
  crv_variable<int> v;
  crv_constraint con;
};

BOOST_AUTO_TEST_CASE(crv_variable_dist_t2) {
  s_crv_variable_dist_t2 item("item");
  BOOST_REQUIRE_THROW(
      item.con = {dist(item.v(), make_distribution(range<int>(0, 10), range<int>(50, 75), range<int>(30, 51)))},
      std::runtime_error);
}

struct s_crv_variable_dist_t3 : public crv_sequence_item {
  s_crv_variable_dist_t3(crv_object_name) {}
  crv_variable<char> v;
  crv_constraint con = {dist(v(), make_distribution(weighted_range<char>(1, 5, 50), weighted_range<char>(10, 20, 20),
                        weighted_range<char>(-50, -50, 30)))};
};

BOOST_AUTO_TEST_CASE(crv_variable_dist_t3) {
  s_crv_variable_dist_t3 item("item");
  int cnt1 = 0, cnt2 = 0, cnt3 = 0;
  int total = 50000;
  for (int i = 0; i < total; i++) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE((1 <= item.v && item.v <= 5) || (10 <= item.v && item.v <= 20) || (item.v == -50));
    if (1 <= item.v && item.v <= 5) cnt1++;
    if (10 <= item.v && item.v <= 20) cnt2++;
    if (item.v == -50) cnt3++;
  }
  BOOST_REQUIRE_LT(cnt2, cnt3);
  BOOST_REQUIRE_LT(cnt3, cnt1);
}

struct s_crv_variable_dist_t4 : public crv_sequence_item {
  s_crv_variable_dist_t4(crv_object_name) {}
  crv_variable<int> v;
  crv_constraint x = {dist(v(), make_distribution(range<int>(0, 10), range<int>(50, 75), range<int>(100, 200)))};
  crv_constraint y = {dist(v(), make_distribution(range<int>(5000, 6000)))};
};

BOOST_AUTO_TEST_CASE(crv_variable_dist_t4) {
  s_crv_variable_dist_t4 item("item");
  item.x.deactivate();
  int total = 10000;
  for (int i = 0; i < total; i++) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE(5000 <= item.v && item.v <= 6000);
  }
}

struct s_dist_of_boolean25 : crv_sequence_item {
  s_dist_of_boolean25(crv_object_name) {}
  crv_variable<bool> a;
  crv_constraint con = {dist(a(), distribution<bool>::create(0.25))};
};

BOOST_AUTO_TEST_CASE(dist_of_boolean25) {
  s_dist_of_boolean25 item("item");
  int counter = 0;
  for (unsigned i = 0; i < 1000; i++) {
    BOOST_REQUIRE(item.randomize());
    if (item.a) {
      ++counter;
    } else {
      --counter;
    }
  }

  BOOST_REQUIRE_LT(counter, 0);
}

struct s_dist_of_boolean50 : crv_sequence_item {
  s_dist_of_boolean50(crv_object_name) {}
  crv_variable<bool> a;
  crv_constraint con = {dist(a(), distribution<bool>::create(0.5))};
};

BOOST_AUTO_TEST_CASE(dist_of_boolean50) {
  s_dist_of_boolean50 item("item");
  int counter = 0;
  for (unsigned i = 0; i < 1000; i++) {
    BOOST_REQUIRE(item.randomize());
    if (item.a) {
      ++counter;
    } else {
      --counter;
    }
  }

  BOOST_REQUIRE_LT(counter, 280);
  BOOST_REQUIRE_GT(counter, -280);
}

struct s_dist_of_boolean75 : public crv_sequence_item {
  s_dist_of_boolean75(crv_object_name) {}
  crv_variable<bool> a;
  crv_constraint con = {dist(a(), distribution<bool>::create(0.75))};
};

BOOST_AUTO_TEST_CASE(dist_of_boolean75) {
  s_dist_of_boolean75 item("item");
  int counter = 0;
  for (unsigned i = 0; i < 1000; i++) {
    BOOST_REQUIRE(item.randomize());
    if (item.a) {
      ++counter;
    } else {
      --counter;
    }
  }

  BOOST_REQUIRE_GT(counter, 0);
}

// The default distribution of a crv_variable must not keep the solver from
// spreading the values over the solutions of the constraints.

struct s_constrained_variable_spread : public crv_sequence_item {
  s_constrained_variable_spread(crv_object_name) {}
  crv_variable<unsigned> v;
  crv_constraint con = {v() <= 10};
};

BOOST_AUTO_TEST_CASE(constrained_variable_spread) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  s_constrained_variable_spread item("item");
  std::set<unsigned> values;
  for (int i = 0; i < 200; i++) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_LE(item.v, 10);
    values.insert(item.v);
  }
  BOOST_REQUIRE_GE(values.size(), 9);
}

BOOST_AUTO_TEST_CASE(constrained_variable_spread_randomize_with) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  s_constrained_variable_spread item("item");
  std::set<unsigned> values;
  for (int i = 0; i < 200; i++) {
    BOOST_REQUIRE(item.randomize_with(item.v() > 5));
    BOOST_REQUIRE(5 < item.v && item.v <= 10);
    values.insert(item.v);
  }
  BOOST_REQUIRE_EQUAL(values.size(), 5);
}

struct s_constrained_variable_spread_64_bits : public crv_sequence_item {
  s_constrained_variable_spread_64_bits(crv_object_name) {}
  crv_variable<uint64_t> v;
  crv_constraint con = {v() >= 100};
};

BOOST_AUTO_TEST_CASE(constrained_variable_spread_64_bits) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  s_constrained_variable_spread_64_bits item("item");
  std::set<uint32_t> low_words;
  for (int i = 0; i < 200; i++) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_GE(item.v, 100);
    low_words.insert(static_cast<uint32_t>(item.v));
  }
  BOOST_REQUIRE_GE(low_words.size(), 190);
}

// Constraints which are too complex for a BDD leave the default distribution in effect.

struct s_complex_constraint_spread : public crv_sequence_item {
  s_complex_constraint_spread(crv_object_name) {}
  crv_variable<unsigned> v;
  crv_constraint con = {v() * v() != 49};
};

BOOST_AUTO_TEST_CASE(complex_constraint_spread) {
  s_complex_constraint_spread item("item");
  std::set<unsigned> values;
  for (int i = 0; i < 200; i++) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_NE(item.v * item.v, 49);
    values.insert(item.v);
  }
  BOOST_REQUIRE_GE(values.size(), 190);
}

struct s_complex_constraint_by_constant : public crv_sequence_item {
  s_complex_constraint_by_constant(crv_object_name) {}
  crv_variable<uint64_t> v;
  crv_constraint con = {v() * 12345 == 0x12340000};
};

// building a BDD for this constraint would take half a minute
BOOST_AUTO_TEST_CASE(complex_constraint_by_constant) {
  s_complex_constraint_by_constant item("item");
  for (int i = 0; i < 20; i++) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_EQUAL(item.v * 12345, 0x12340000);
  }
}

// Items with the same constraints share their BDD samplers.

struct s_many_items : public crv_sequence_item {
  s_many_items(crv_object_name) {}
  crv_variable<unsigned> addr, len, delay;
  crv_constraint con = {addr() <= 0xffff, 1 <= len() && len() <= 64, delay() <= 10};
};

BOOST_AUTO_TEST_CASE(many_items_share_samplers) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return;

  std::vector<std::unique_ptr<s_many_items> > items;
  items.emplace_back(new s_many_items("item"));
  BOOST_REQUIRE(items.back()->randomize());
  std::size_t count = BddSampler::count();
  for (int i = 0; i < 100; i++) {
    items.emplace_back(new s_many_items("item"));
    BOOST_REQUIRE(items.back()->randomize());
    BOOST_REQUIRE_LE(items.back()->delay, 10);
  }
  BOOST_REQUIRE_EQUAL(BddSampler::count(), count);
}

// A distribution of the user still takes precedence.

struct s_constrained_variable_user_dist : public crv_sequence_item {
  s_constrained_variable_user_dist(crv_object_name) {}
  crv_variable<unsigned> v;
  crv_constraint con = {v() <= 10};
  crv_constraint con_dist = {
      dist(v(), make_distribution(weighted_range<unsigned>(0, 4, 90), weighted_range<unsigned>(5, 10, 10)))};
};

BOOST_AUTO_TEST_CASE(constrained_variable_user_dist) {
  s_constrained_variable_user_dist item("item");
  int low = 0;
  int total = 1000;
  for (int i = 0; i < total; i++) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_LE(item.v, 10);
    if (item.v <= 4) low++;
  }
  // 90% are expected, sampling the solutions uniformly would give 45%
  BOOST_REQUIRE_GT(low, total * 8 / 10);
}

BOOST_AUTO_TEST_CASE(constrained_variable_user_dist_randomize_with) {
  s_constrained_variable_spread item("item");
  std::map<unsigned, int> values;
  int total = 1000;
  for (int i = 0; i < total; i++) {
    BOOST_REQUIRE(item.randomize_with(dist(item.v(), make_distribution(range<unsigned>(6, 10)))));
    BOOST_REQUIRE(6 <= item.v && item.v <= 10);
    ++values[item.v];
  }
  // 20% are expected for each value
  for (unsigned v = 6; v <= 10; v++) {
    BOOST_REQUIRE_GT(values[v], total / 10);
    BOOST_REQUIRE_LT(values[v], total * 3 / 10);
  }
}

BOOST_AUTO_TEST_SUITE_END()  // Context

//  vim: ft=cpp:ts=2:sw=2:expandtab

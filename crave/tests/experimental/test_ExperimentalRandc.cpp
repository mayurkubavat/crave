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
void require_cycle(std::vector<int> values, std::set<int> const& expected) {
  std::sort(values.begin(), values.end());
  BOOST_REQUIRE_EQUAL(values.size(), expected.size());
  BOOST_REQUIRE(std::equal(values.begin(), values.end(), expected.begin()));
}

std::set<int> values_up_to(int last) {
  std::set<int> result;
  for (int i = 0; i <= last; ++i) result.insert(i);
  return result;
}

struct randc_item : public crv_sequence_item {
  crv_variable<unsigned char> x;
  crv_variable<unsigned short> y;
  crv_constraint c{x() < 10u, y() == x() + 100u};
  crv_constraint small{x() < 3u};
  std::vector<int> post;
  unsigned pre = 0;

  randc_item(crv_object_name) {
    x.randc(true);
    small.deactivate();
  }

  void pre_randomize() override { ++pre; }
  void post_randomize() override { post.push_back(x); }
};
}  // namespace

BOOST_FIXTURE_TEST_SUITE(ExperimentalRandc_t, Context_Fixture)

BOOST_AUTO_TEST_CASE(randc_variable) {
  randc_item item("item");
  BOOST_REQUIRE(item.x.randc());
  BOOST_REQUIRE(!item.y.randc());

  std::set<std::vector<int> > orders;
  for (int cycle = 0; cycle < 5; ++cycle) {
    std::vector<int> values;
    for (int i = 0; i < 10; ++i) {
      BOOST_REQUIRE(item.randomize());
      BOOST_REQUIRE_EQUAL(item.y, item.x + 100);
      values.push_back(item.x);
    }
    require_cycle(values, values_up_to(9));
    orders.insert(values);
  }
  BOOST_REQUIRE_GT(orders.size(), 1u);
  // the hooks are called, post_randomize() sees the values of the cycle
  BOOST_REQUIRE_EQUAL(item.pre, 50u);
  BOOST_REQUIRE_EQUAL(item.post.size(), 50u);
  require_cycle(std::vector<int>(item.post.begin(), item.post.begin() + 10), values_up_to(9));
}

// the cycle continues over calls of randomize_with() with the same constraints, and starts again when they change
BOOST_AUTO_TEST_CASE(randc_randomize_with) {
  randc_item item("item");
  for (int cycle = 0; cycle < 3; ++cycle) {
    std::vector<int> values;
    for (int i = 0; i < 5; ++i) {
      BOOST_REQUIRE(item.randomize_with(item.x() >= 5u));
      values.push_back(item.x - 5);
    }
    require_cycle(values, values_up_to(4));
  }

  for (int trial = 0; trial < 5; ++trial) {
    std::vector<int> values;
    for (int i = 0; i < 2; ++i) {
      BOOST_REQUIRE(item.randomize_with(item.x() >= 5u));
      values.push_back(item.x);
    }
    // other constraints start a new cycle
    values.clear();
    for (int i = 0; i < 6; ++i) {
      BOOST_REQUIRE(item.randomize_with(item.x() >= 4u));
      values.push_back(item.x - 4);
    }
    require_cycle(values, values_up_to(5));
    // so do the constraints of randomize()
    values.clear();
    for (int i = 0; i < 10; ++i) {
      BOOST_REQUIRE(item.randomize());
      values.push_back(item.x);
    }
    require_cycle(values, values_up_to(9));
  }
}

// the cycle starts again when a constraint is enabled
BOOST_AUTO_TEST_CASE(randc_constraint_mode) {
  randc_item item("item");
  for (int trial = 0; trial < 5; ++trial) {
    for (int i = 0; i < 4; ++i) BOOST_REQUIRE(item.randomize());
    item.small.activate();
    std::vector<int> values;
    for (int i = 0; i < 6; ++i) {
      BOOST_REQUIRE(item.randomize());
      values.push_back(item.x);
    }
    std::vector<int> second(values.begin() + 3, values.end());
    values.resize(3);
    require_cycle(values, values_up_to(2));
    require_cycle(second, values_up_to(2));
    item.small.deactivate();
  }
}

// a variable whose randomization is disabled keeps its value, the cycle starts again when it is enabled
BOOST_AUTO_TEST_CASE(randc_rand_mode) {
  randc_item item("item");
  for (int i = 0; i < 3; ++i) BOOST_REQUIRE(item.randomize());
  int const kept = item.x;
  item.x.rand_mode(false);
  BOOST_REQUIRE(item.x.randc());
  for (int i = 0; i < 5; ++i) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_EQUAL(item.x, kept);
    BOOST_REQUIRE_EQUAL(item.y, kept + 100);
  }
  item.x.rand_mode(true);
  for (int cycle = 0; cycle < 2; ++cycle) {
    std::vector<int> values;
    for (int i = 0; i < 10; ++i) {
      BOOST_REQUIRE(item.randomize());
      values.push_back(item.x);
    }
    require_cycle(values, values_up_to(9));
  }
}

BOOST_AUTO_TEST_CASE(randc_off_and_width) {
  randc_item item("item");
  item.x.randc(false);
  BOOST_REQUIRE(!item.x.randc());
  for (int i = 0; i < 20; ++i) BOOST_REQUIRE(item.randomize());

  crv_variable<unsigned> wide("wide");
  BOOST_REQUIRE_THROW(wide.randc(true), std::runtime_error);
  BOOST_REQUIRE(!wide.randc());
}

BOOST_AUTO_TEST_SUITE_END()  // ExperimentalRandc_t

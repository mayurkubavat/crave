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

#include <stdexcept>

using namespace crave;

namespace {
struct solve_before_item : public crv_sequence_item {
  crv_variable<bool> s;
  crv_variable<unsigned> d;
  crv_constraint c{if_then(s(), d() == 0u)};
  crv_solve_before order{"order", s, d};

  solve_before_item(crv_object_name) {}
};

struct cyclic_item : public crv_sequence_item {
  crv_variable<unsigned char> x;
  crv_variable<unsigned char> y;
  crv_constraint c{x() < y()};
  crv_solve_before xy{x, y};
  crv_solve_before yx{y, x};

  cyclic_item(crv_object_name) { yx.deactivate(); }
};

// the number of randomizations of 1000, in which s is true
template <typename Randomize>
unsigned count_true(solve_before_item& item, Randomize randomize) {
  unsigned ones = 0;
  for (unsigned i = 0; i < 1000; ++i) {
    BOOST_REQUIRE(randomize());
    if (item.s) {
      ++ones;
      BOOST_REQUIRE_EQUAL(item.d, 0u);
    }
  }
  return ones;
}
}  // namespace

BOOST_FIXTURE_TEST_SUITE(ExperimentalSolveBefore_t, Context_Fixture)

// IEEE 1800-2017 18.5.10: s is true with probability 1/2 instead of 2^-32
BOOST_AUTO_TEST_CASE(solve_before_member) {
  set_global_seed(21);
  solve_before_item item("item");
  BOOST_REQUIRE_EQUAL(item.order.obj_kind(), "crv_constraint");
  BOOST_REQUIRE(item.order.active());

  // 1/2 with a standard deviation of about 16, the bounds are more than 4 of them away
  unsigned ones = count_true(item, [&] { return item.randomize(); });
  BOOST_REQUIRE_GT(ones, 430u);
  BOOST_REQUIRE_LT(ones, 570u);

  ones = count_true(item, [&] { return item.randomize_with(item.d() != 1u); });
  BOOST_REQUIRE_GT(ones, 430u);
  BOOST_REQUIRE_LT(ones, 570u);

  // Without the ordering, s is true less often: the random values suggested for s and d contradict each other
  // if s is true, unless d is 0.
  item.order.deactivate();
  ones = count_true(item, [&] { return item.randomize(); });
  BOOST_REQUIRE_LT(ones, 400u);
}

// IEEE 1800-2017 18.5.10: the ordering must not be cyclic
BOOST_AUTO_TEST_CASE(solve_before_cyclic_members) {
  cyclic_item item("item");
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_LT(item.x, item.y);
  item.yx.activate();
  BOOST_REQUIRE_THROW(item.randomize(), std::runtime_error);
  item.yx.deactivate();
  BOOST_REQUIRE(item.randomize());
}

// IEEE 1800-2017 18.5.10: randc variables are not allowed in the ordering
BOOST_AUTO_TEST_CASE(solve_before_randc_member) {
  cyclic_item item("item");
  item.x.randc(true);
  BOOST_REQUIRE_THROW(item.randomize(), std::runtime_error);
  item.x.randc(false);
  BOOST_REQUIRE(item.randomize());
}

BOOST_AUTO_TEST_SUITE_END()  // ExperimentalSolveBefore_t

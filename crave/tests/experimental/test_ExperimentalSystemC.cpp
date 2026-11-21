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


#define BOOST_TEST_MODULE experimental_SystemC
#include <boost/test/unit_test.hpp>

#include <systemc>
#include <crave/ConstrainedRandom.hpp>
#include <crave/experimental/Experimental.hpp>
#include <crave/experimental/SystemC.hpp>

#include <cstdint>
#include <limits>
#include <set>

using namespace crave;
using namespace sc_dt;

struct Ex_SystemC_Fixture {};

BOOST_FIXTURE_TEST_SUITE(Ex_SystemC_t, Ex_SystemC_Fixture)

template <typename T>
struct sc_item : public crv_sequence_item {
  sc_item(crv_object_name) {}
  crv_variable<T> x;
  crv_constraint con{"con"};
};

template <typename T>
void check_64_bit_unsigned() {
  sc_item<T> item("item");
  item.con = {item.x() == (uint64_t)0x8000000000000005ull};
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(T(item.x).to_uint64(), 0x8000000000000005ull);

  item.con = {item.x() >= (uint64_t)0xfffffffffffffff0ull};
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_GE(T(item.x).to_uint64(), 0xfffffffffffffff0ull);

  // without constraints, the values are spread over the full range
  item.con = {item.x() != 0u};
  std::set<uint64_t> values;
  uint64_t all = 0;
  for (unsigned i = 0; i < 20; ++i) {
    BOOST_REQUIRE(item.randomize());
    values.insert(T(item.x).to_uint64());
    all |= T(item.x).to_uint64();
  }
  BOOST_REQUIRE_GT(values.size(), 10u);
  BOOST_REQUIRE_GE(all, 1ull << 63);
}

template <typename T>
void check_64_bit_signed() {
  sc_item<T> item("item");
  item.con = {item.x() == std::numeric_limits<int64_t>::min()};
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(T(item.x).to_int64(), std::numeric_limits<int64_t>::min());

  item.con = {item.x() == (int64_t)0x4000000000000003ll};
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(T(item.x).to_int64(), 0x4000000000000003ll);

  // without constraints, the values are spread over the full range
  item.con = {item.x() != 0};
  std::set<int64_t> values;
  bool negative = false, positive = false;
  for (unsigned i = 0; i < 20; ++i) {
    BOOST_REQUIRE(item.randomize());
    int64_t v = T(item.x).to_int64();
    values.insert(v);
    negative |= v < -(1ll << 40);
    positive |= v > (1ll << 40);
  }
  BOOST_REQUIRE_GT(values.size(), 10u);
  BOOST_REQUIRE(negative);
  BOOST_REQUIRE(positive);
}

BOOST_AUTO_TEST_CASE(sc_uint_64_t) {
  check_64_bit_unsigned<sc_uint<64> >();
}

BOOST_AUTO_TEST_CASE(sc_biguint_64_t) {
  check_64_bit_unsigned<sc_biguint<64> >();
}

BOOST_AUTO_TEST_CASE(sc_int_64_t) {
  check_64_bit_signed<sc_int<64> >();
}

BOOST_AUTO_TEST_CASE(sc_bigint_64_t) {
  check_64_bit_signed<sc_bigint<64> >();
}

// IEEE 1800-2017 18.12: std::randomize() of variables of SystemC data types
BOOST_AUTO_TEST_CASE(scope_randomize_systemc) {
  set_global_seed(35);
  sc_uint<4> u = 0;
  sc_int<6> i = 0;
  sc_bv<3> v = 0;
  std::set<int> us, is, vs;
  for (unsigned n = 0; n < 1000; ++n) {
    BOOST_REQUIRE(crave::randomize(u, i, v));
    us.insert(u.to_int());
    is.insert(i.to_int());
    vs.insert(v.to_int());
  }
  BOOST_REQUIRE_EQUAL(us.size(), 16u);
  BOOST_REQUIRE_EQUAL(is.size(), 64u);
  BOOST_REQUIRE_EQUAL(vs.size(), 8u);

  us.clear();
  for (unsigned n = 0; n < 200; ++n) {
    BOOST_REQUIRE(crave::randomize(u, i).with(reference(u) > 10, reference(i) < -20, reference(i) > -32));
    BOOST_REQUIRE(u.to_uint() > 10);
    BOOST_REQUIRE(i.to_int() < -20 && i.to_int() > -32);
    us.insert(u.to_int());
  }
  BOOST_REQUIRE_EQUAL(us.size(), 5u);

  u = 3;
  i = -1;
  BOOST_REQUIRE(!crave::randomize(u, i).with(reference(u) > 10, reference(u) < 5));
  BOOST_REQUIRE_EQUAL(u.to_uint(), 3u);
  BOOST_REQUIRE_EQUAL(i.to_int(), -1);
}

struct inside_sc_exp_item : public crv_sequence_item {
  inside_sc_exp_item(crv_object_name) {}
  crv_variable<sc_int<8> > a;
  crv_variable<sc_bigint<40> > b;
  crv_variable<bool> flag;
  std::set<sc_int<8> > sa{-10, -1, 5, 10};
  std::vector<long long> sb{-5, 7, 1LL << 35};
  crv_constraint con_a{"con_a"};
  crv_constraint con_b{"con_b"};
};

BOOST_AUTO_TEST_CASE(inside_sc_experimental) {
  inside_sc_exp_item item("item");
  item.con_a = {item.a() == -1, !inside(item.a(), item.sa)};
  item.con_b = {inside(item.b(), item.sb)};
  BOOST_REQUIRE(!item.randomize());

  item.con_a = {item.a() == -1, if_then(inside(item.a(), item.sa), item.flag() == true), item.flag() == false};
  BOOST_REQUIRE(!item.randomize());

  item.con_a = {!inside(item.a(), item.sa), item.a() >= -2, item.a() <= 2};
  item.con_b = {item.b() == (1LL << 35), inside(item.b(), item.sb)};
  for (int n = 0; n < 10; ++n) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_NE(static_cast<sc_int<8> >(item.a), -1);
    BOOST_REQUIRE_EQUAL(static_cast<sc_bigint<40> >(item.b).to_int64(), 1LL << 35);
  }
}

BOOST_AUTO_TEST_SUITE_END()  // Ex_SystemC_t

//  vim: ft=cpp:ts=2:sw=2:expandtab

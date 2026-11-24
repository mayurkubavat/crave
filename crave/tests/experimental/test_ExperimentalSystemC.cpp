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

struct sc_bv_exp_item : public crv_sequence_item {
  sc_bv_exp_item(crv_object_name) {}
  crv_variable<sc_bv<8> > bv;
  crv_array<sc_bv<8>, 3> arr;
  crv_vector<sc_bv<8> > vec;
  crv_constraint c_arr{arr[0]() == sc_bv<8>(0x11), arr[1]() == sc_bv<8>(0x22), arr[2]() == sc_bv<8>(0x33)};
  crv_constraint c_vec{vec().size() == 2,
                       foreach (vec(), if_then(_i == 0u, vec()[_i] == sc_bv<8>(0x44))),
                       foreach (vec(), if_then(_i == 1u, vec()[_i] == sc_bv<8>(0x55)))};
  crv_constraint con{"con"};
};

struct sc_bv16_vec_item : public crv_sequence_item {
  sc_bv16_vec_item(crv_object_name) {}
  crv_variable<sc_bv<16> > bv;
  crv_vector<sc_bv<16> > vec;
  crv_constraint c_vec{vec().size() == 2,
                       foreach (vec(), if_then(_i == 0u, vec()[_i] == sc_bv<16>(0x1357))),
                       foreach (vec(), if_then(_i == 1u, vec()[_i] == sc_bv<16>(0x8ACE)))};
  crv_constraint con{inside(bv(), vec)};
};

struct sc_bv64_vec_item : public crv_sequence_item {
  sc_bv64_vec_item(crv_object_name) {}
  crv_variable<sc_bv<64> > bv;
  crv_vector<sc_bv<64> > vec;
  crv_constraint c_vec{vec().size() == 3,
                       foreach (vec(), if_then(_i == 0u, vec()[_i] == sc_bv<64>(0x8000000000000000ULL))),
                       foreach (vec(), if_then(_i == 1u, vec()[_i] == sc_bv<64>(0xFFFFFFFFFFFFFFFFULL))),
                       foreach (vec(), if_then(_i == 2u, vec()[_i] == sc_bv<64>(0x123456789ABCDEF0ULL)))};
  crv_constraint con{inside(bv(), vec)};
};

BOOST_AUTO_TEST_CASE(sc_bv_experimental_inside_dist_eval) {
  struct Guard {
    ~Guard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    {
      sc_bv_exp_item item("item");
      item.con = {inside(item.bv(), item.arr)};
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(item.randomize());
        uint64_t v = static_cast<sc_bv<8> >(item.bv).to_uint64();
        BOOST_REQUIRE(v == 0x11u || v == 0x22u || v == 0x33u);
      }

      item.con = {inside(item.bv(), item.vec)};
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(item.randomize());
        uint64_t v = static_cast<sc_bv<8> >(item.bv).to_uint64();
        BOOST_REQUIRE(v == 0x44u || v == 0x55u);
      }

      item.con = {dist(item.bv(), make_distribution(range(sc_bv<8>(100), sc_bv<8>(105))))};
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(item.randomize());
        uint64_t v = static_cast<sc_bv<8> >(item.bv).to_uint64();
        BOOST_REQUIRE(v >= 100u && v <= 105u);
      }
    }

    {
      sc_bv16_vec_item item16("item16");
      for (int i = 0; i < 8; ++i) {
        BOOST_REQUIRE(item16.randomize());
        uint64_t v = static_cast<sc_bv<16> >(item16.bv).to_uint64();
        BOOST_REQUIRE(v == 0x1357u || v == 0x8ACEu);
      }
    }

    if (std::string(backend) == "Z3") {
      sc_bv64_vec_item item64("item64");
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(item64.randomize());
        uint64_t v = static_cast<sc_bv<64> >(item64.bv).to_uint64();
        BOOST_REQUIRE(v == 0x8000000000000000ULL || v == 0xFFFFFFFFFFFFFFFFULL || v == 0x123456789ABCDEF0ULL);
      }
    }

    {
      sc_bv<8> sbv(0);
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(crave::randomize(sbv).with(inside(reference(sbv), range(sc_bv<8>(10), sc_bv<8>(20)))));
        BOOST_REQUIRE(sbv.to_uint64() >= 10u && sbv.to_uint64() <= 20u);
      }
    }

    {
      Variable<sc_bv<8> > v;
      Evaluator eval;
      eval.assign(v, sc_bv<8>(0xA5));
      BOOST_REQUIRE(eval.evaluate(inside(v, inside_mask("8'hA?"))));
      BOOST_REQUIRE(eval.result<bool>());
      BOOST_REQUIRE(eval.evaluate(inside(v, range(sc_bv<8>(0xA0), sc_bv<8>(0xAF)))));
      BOOST_REQUIRE(eval.result<bool>());
      eval.assign(v, sc_bv<8>(0x10));
      BOOST_REQUIRE(eval.evaluate(!inside(v, range(sc_bv<8>(0xA0), sc_bv<8>(0xAF)))));
      BOOST_REQUIRE(eval.result<bool>());
    }
  }
}

struct wide_sc_bv_item : public crv_sequence_item {
  wide_sc_bv_item(crv_object_name) {}
  crv_variable<sc_bv<128> > w;
  crv_constraint con{bitslice(127, 64, w()) == 0x123456789ABCDEF0ULL,
                     bitslice(63, 0, w()) == 0x0FEDCBA987654321ULL};
};

struct wide_sc_bv_mask_item : public crv_sequence_item {
  wide_sc_bv_mask_item(crv_object_name) {}
  crv_variable<sc_bv<128> > w;
  crv_constraint con{bitslice(127, 64, w()) == 0xCAFEBABE00000001ULL,
                     inside(bitslice(7, 0, w()), inside_mask("8'hA?"))};
};

BOOST_AUTO_TEST_CASE(crv_variable_wide_sc_bv_128) {
  struct Guard {
    ~Guard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    if (std::string(backend) == "Z3") {
      wide_sc_bv_item item("wide");
      BOOST_REQUIRE(item.randomize());
      sc_bv<128> val = item.w;
      BOOST_REQUIRE_EQUAL(val.range(127, 64).to_uint64(), 0x123456789ABCDEF0ULL);
      BOOST_REQUIRE_EQUAL(val.range(63, 0).to_uint64(), 0x0FEDCBA987654321ULL);

      wide_sc_bv_mask_item mitem("wide_mask");
      for (int i = 0; i < 4; ++i) {
        BOOST_REQUIRE(mitem.randomize());
        sc_bv<128> mval = mitem.w;
        BOOST_REQUIRE_EQUAL(mval.range(127, 64).to_uint64(), 0xCAFEBABE00000001ULL);
        uint64_t lo8 = mval.range(7, 0).to_uint64();
        BOOST_REQUIRE(lo8 >= 0xA0ULL && lo8 <= 0xAFULL);
      }
    } else {
      // On CUDD, constrain top bit and bottom byte of 128-bit crv_variable to keep BDD tiny
      sc_item<sc_bv<128> > item128("cudd128");
      item128.con = {bitslice(127, 120, item128.x()) == 0x5Au,
                     inside(bitslice(7, 0, item128.x()), 0x11u, 0x22u)};
      for (int i = 0; i < 4; ++i) {
        BOOST_REQUIRE(item128.randomize());
        sc_bv<128> val = item128.x;
        BOOST_REQUIRE_EQUAL(val.range(127, 120).to_uint64(), 0x5Au);
        uint64_t lo8 = val.range(7, 0).to_uint64();
        BOOST_REQUIRE(lo8 == 0x11u || lo8 == 0x22u);
      }
    }

    // Standalone crv_variable<sc_bv<128>>::randomize() produces nonzero upper bits
    crv_variable<sc_bv<128> > standalone("standalone128");
    bool saw_nonzero_upper = false;
    for (int i = 0; i < 4; ++i) {
      BOOST_REQUIRE(standalone.randomize());
      sc_bv<128> val = standalone;
      if (val.range(127, 64).to_uint64() != 0ULL) saw_nonzero_upper = true;
    }
    BOOST_REQUIRE(saw_nonzero_upper);
  }
}

struct sc_bv_m2_item : public crv_sequence_item {
  sc_bv_m2_item(crv_object_name) {}
  crv_variable<sc_bv<8> > bv8;
  crv_variable<sc_bv<128> > bv128;
  crv_array<sc_bv<129>, 3> arr129;
  crv_constraint con{"con"};
};

BOOST_AUTO_TEST_CASE(sc_bv_unique_onehot_countones_clog2) {
  struct Guard {
    ~Guard() {
      set_solver_backend("Z3");
      set_sv_signedness(false);
    }
  } guard;

  for (bool sv : {false, true}) {
    set_sv_signedness(sv);
    for (const char* backend : {"Z3", "Cudd"}) {
      if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
      set_solver_backend(backend);

      sc_bv_m2_item item("item");
      item.con = {onehot(item.bv8()), countones(item.bv8()) == 1u, clog2(item.bv8()) == 7u};
      BOOST_REQUIRE(item.randomize());
      BOOST_REQUIRE_EQUAL(static_cast<sc_bv<8> >(item.bv8).to_uint64(), 0x80ULL);

      if (std::string(backend) == "Z3") {
        item.con = {onehot(item.bv128()), clog2(item.bv128()) == 127u, unique(item.arr129)};
        BOOST_REQUIRE(item.randomize());
        sc_bv<128> v128 = item.bv128;
        BOOST_REQUIRE_EQUAL(v128.range(127, 64).to_uint64(), 0x8000000000000000ULL);
        BOOST_REQUIRE_EQUAL(v128.range(63, 0).to_uint64(), 0ULL);
        BOOST_REQUIRE(static_cast<sc_bv<129> >(item.arr129[0]) != static_cast<sc_bv<129> >(item.arr129[1]));
        BOOST_REQUIRE(static_cast<sc_bv<129> >(item.arr129[0]) != static_cast<sc_bv<129> >(item.arr129[2]));
        BOOST_REQUIRE(static_cast<sc_bv<129> >(item.arr129[1]) != static_cast<sc_bv<129> >(item.arr129[2]));
      }
    }
  }
}

struct sc_dist_cm_item : public crv_sequence_item {
  sc_dist_cm_item(crv_object_name) {}
  crv_variable<sc_int<8> > s8;
  crv_variable<sc_uint<8> > u8;
  crv_constraint c_s8{"c_s8", dist(s8(), make_distribution(range<sc_int<8> >(sc_int<8>(-10), sc_int<8>(-8))))};
  crv_constraint c_u8{"c_u8", dist(u8(), make_distribution(range<sc_uint<8> >(sc_uint<8>(200), sc_uint<8>(202))))};
};

BOOST_AUTO_TEST_CASE(sc_int_uint_dist_constraint_mode) {
  struct Guard {
    ~Guard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    sc_dist_cm_item item("sc_dist");
    item.c_s8.constraint_mode(false);
    item.c_u8.constraint_mode(false);
    std::set<int> s_vals;
    std::set<unsigned> u_vals;
    for (int i = 0; i < 16; ++i) {
      BOOST_REQUIRE(item.randomize());
      s_vals.insert(static_cast<sc_int<8> >(item.s8).to_int());
      u_vals.insert(static_cast<sc_uint<8> >(item.u8).to_uint());
    }
    BOOST_REQUIRE_GT(s_vals.size(), 1u);
    BOOST_REQUIRE_GT(u_vals.size(), 1u);

    item.constraint_mode(true);
    for (int i = 0; i < 5; ++i) {
      BOOST_REQUIRE(item.randomize());
      int sv = static_cast<sc_int<8> >(item.s8).to_int();
      unsigned uv = static_cast<sc_uint<8> >(item.u8).to_uint();
      BOOST_REQUIRE(sv >= -10 && sv <= -8);
      BOOST_REQUIRE(uv >= 200u && uv <= 202u);
    }

    item.constraint_mode(false);
    s_vals.clear();
    u_vals.clear();
    for (int i = 0; i < 16; ++i) {
      BOOST_REQUIRE(item.randomize());
      s_vals.insert(static_cast<sc_int<8> >(item.s8).to_int());
      u_vals.insert(static_cast<sc_uint<8> >(item.u8).to_uint());
    }
    BOOST_REQUIRE_GT(s_vals.size(), 1u);
    BOOST_REQUIRE_GT(u_vals.size(), 1u);
  }
}

struct sc_uint3_dist_cm_item : public crv_sequence_item {
  sc_uint3_dist_cm_item(crv_object_name) {}
  crv_variable<sc_uint<3> > x;
  crv_constraint c_dist{"c_dist", dist(x(), make_distribution(range<sc_uint<3> >(sc_uint<3>(0), sc_uint<3>(1))))};
};

struct sc_uint3_plain_item : public crv_sequence_item {
  sc_uint3_plain_item(crv_object_name) {}
  crv_variable<sc_uint<3> > x;
};

BOOST_AUTO_TEST_CASE(sc_uint3_dist_constraint_mode_randc_and_uniform) {
  struct Guard {
    ~Guard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    // 1) Pure c_var_uniform recovery (no explicit bound constraint) after c_dist.constraint_mode(false)
    {
      sc_uint3_dist_cm_item item("sc_u3_rec");
      for (int i = 0; i < 6; ++i) {
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_LE(static_cast<sc_uint<3> >(item.x).to_uint(), 1u);
      }

      item.c_dist.constraint_mode(false);
      std::set<unsigned> seen;
      for (int i = 0; i < 96; ++i) {
        BOOST_REQUIRE(item.randomize());
        seen.insert(static_cast<sc_uint<3> >(item.x).to_uint());
      }
      BOOST_REQUIRE_EQUAL(seen.size(), 8u);

      // 2) randc(true) after c_dist.constraint_mode(false) cycles all 8 values without throwing
      item.x.randc(true);
      std::set<unsigned> cycle_vals;
      for (int i = 0; i < 8; ++i) {
        BOOST_REQUIRE_NO_THROW(BOOST_REQUIRE(item.randomize()));
        BOOST_REQUIRE(cycle_vals.insert(static_cast<sc_uint<3> >(item.x).to_uint()).second);
      }
      BOOST_REQUIRE_EQUAL(cycle_vals.size(), 8u);

      // 3) Re-enable c_dist after randc(false)
      item.x.randc(false);
      item.c_dist.constraint_mode(true);
      for (int i = 0; i < 6; ++i) {
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_LE(static_cast<sc_uint<3> >(item.x).to_uint(), 1u);
      }
    }

    // 4) randomize_with(dist(...)) on unconstrained sc_uint<3> recovers full 8-value c_var_uniform domain
    {
      sc_uint3_plain_item plain("sc_u3_plain");
      BOOST_REQUIRE(
          plain.randomize_with(dist(plain.x(), make_distribution(range<sc_uint<3> >(sc_uint<3>(0), sc_uint<3>(0))))));
      BOOST_REQUIRE_EQUAL(static_cast<sc_uint<3> >(plain.x).to_uint(), 0u);

      std::set<unsigned> seen;
      for (int i = 0; i < 96; ++i) {
        BOOST_REQUIRE(plain.randomize());
        seen.insert(static_cast<sc_uint<3> >(plain.x).to_uint());
      }
      BOOST_REQUIRE_EQUAL(seen.size(), 8u);
    }
  }
}

template <typename ScT>
void verify_sysc_dist_d2() {
  sc_item<ScT> item("sc_d2");
  item.con = {dist(item.x(), make_distribution(dist_per_value(ScT(1), ScT(4), 3),
                                               dist_per_range(ScT(3), ScT(6), 4),
                                               dist_per_value(ScT(10), 0)))};
  for (int i = 0; i < 40; ++i) {
    BOOST_REQUIRE(item.randomize());
    auto v = distribution_value_type<ScT>::value(item.x.get());
    BOOST_REQUIRE(v >= 1 && v <= 6);
  }
  BOOST_REQUIRE(!item.randomize_with(item.x() == ScT(10)));

  sc_item<ScT> az("sc_az");
  az.con = {dist(az.x(), make_distribution(dist_per_value(ScT(1), ScT(4), 0)))};
  BOOST_REQUIRE_NO_THROW(BOOST_REQUIRE(!az.randomize()));
}

BOOST_AUTO_TEST_CASE(sc_dist_per_value_overlap_zero_weight_and_srandom) {
  verify_sysc_dist_d2<sc_int<8> >();
  verify_sysc_dist_d2<sc_uint<8> >();
  verify_sysc_dist_d2<sc_bigint<16> >();
  verify_sysc_dist_d2<sc_biguint<16> >();
  verify_sysc_dist_d2<sc_bv<8> >();

  // D1 srandom repeatability for SystemC crv_variable<sc_uint<8>> and crv_sequence_item
  sc_item<sc_uint<8> > a("a"), b("b");
  a.con = {a.x() <= sc_uint<8>(200)};
  b.con = {b.x() <= sc_uint<8>(200)};
  a.srandom(4321u);
  b.srandom(4321u);
  for (int i = 0; i < 10; ++i) {
    BOOST_REQUIRE(a.randomize());
    BOOST_REQUIRE(b.randomize());
    BOOST_REQUIRE_EQUAL(a.x.get().to_uint(), b.x.get().to_uint());
  }

  crv_variable<sc_uint<8> > va("va"), vb("vb");
  va.srandom(987u);
  vb.srandom(987u);
  for (int i = 0; i < 10; ++i) {
    BOOST_REQUIRE(va.randomize());
    BOOST_REQUIRE(vb.randomize());
    BOOST_REQUIRE_EQUAL(va.get().to_uint(), vb.get().to_uint());
  }
}

BOOST_AUTO_TEST_SUITE_END()

//  vim: ft=cpp:ts=2:sw=2:expandtab

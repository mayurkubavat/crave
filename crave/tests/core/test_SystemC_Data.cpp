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

#define BOOST_TEST_MODULE SystemC_Data
#include <boost/test/unit_test.hpp>

#include <crave/ConstrainedRandom.hpp>
#include <crave/frontend/SystemC.hpp>
#include <systemc>

#include <boost/format.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <vector>
using boost::format;

using namespace crave;

struct SystemC_Data_Fixture {
  SystemC_Data_Fixture() { crave::set_solver_backend("Boolector"); }
};

BOOST_FIXTURE_TEST_SUITE(SystemC_Data, SystemC_Data_Fixture)

BOOST_AUTO_TEST_CASE(sc_bv_t) {
  using namespace sc_dt;
  const unsigned w = 4;

  Variable<sc_bv<w> > i, j;

  Generator gen(i != j);
  gen();

  sc_bv<w> iv = gen[i];
  sc_bv<w> jv = gen[j];

  BOOST_REQUIRE_NE(iv, jv);
}

BOOST_AUTO_TEST_CASE(sc_uint_t) {
  using namespace sc_dt;
  const unsigned w = 2;

  Variable<sc_uint<w> > i, j, k;

  Generator gen(i != j && i + j == k);

  unsigned count = 0;

  while (gen.next()) {
    ++count;
    sc_uint<w> iv = gen[i];
    sc_uint<w> jv = gen[j];
    sc_uint<w> kv = gen[k];

    sc_uint<w> rv = iv + jv;

    BOOST_REQUIRE_NE(iv, jv);
    BOOST_REQUIRE_EQUAL(rv, kv);

    gen(i != iv || j != jv || k != kv);
  }

  BOOST_REQUIRE_EQUAL(count, 12);
}

BOOST_AUTO_TEST_CASE(sc_int_t) {
  using namespace sc_dt;
  const unsigned w = 4;

  Variable<sc_int<w> > i, j, k;

  Generator gen(i < j && i - j == k);
  gen();

  sc_int<w> iv = gen[i];
  sc_int<w> jv = gen[j];
  sc_int<w> kv = gen[k];

  sc_int<w> rv = iv - jv;

  // std::cout << format("iv: %d, jv: %d, kv: %d\n") %iv%jv%kv;

  BOOST_REQUIRE_LT(iv, jv);
  BOOST_REQUIRE_EQUAL(rv, kv);
}

BOOST_AUTO_TEST_CASE(randv_sc_dt_t) {
  using namespace sc_dt;

  randv<sc_uint<4> > a, b, c;
  randv<sc_uint<8> > d;

  Generator gen;
  gen(a() == 15);
  gen(b() == 15);
  gen(c() == a() + b());
  gen(d() == a() + b());

  BOOST_REQUIRE(gen.next());
  BOOST_REQUIRE_EQUAL(++a, 0);  // wrap around
  BOOST_REQUIRE_EQUAL(b + 1, 16);
  BOOST_REQUIRE_EQUAL(c, 14);
  BOOST_REQUIRE_EQUAL(d, 14);  // deviation to sc_uint semantics (no overflow, thus d == 30)
}

BOOST_AUTO_TEST_CASE(inside_sc_uint_t) {
  using namespace sc_dt;

  std::set<sc_uint<8> > s{0, 64, 128, 192};
  Variable<sc_uint<8> > v;
  Generator gen(inside(v, s));

  std::set<sc_uint<8> > seen;
  for (unsigned i = 0; i < 100; ++i) {
    BOOST_REQUIRE(gen.nextCov());
    sc_uint<8> r = gen[v];
    BOOST_REQUIRE(s.find(r) != s.end());
    seen.insert(r);
  }
  BOOST_REQUIRE_EQUAL(seen.size(), s.size());
}

BOOST_AUTO_TEST_CASE(inside_sc_int_t) {
  using namespace sc_dt;

  std::set<sc_int<8> > s{-128, -1, 5, 127};
  randv<sc_int<8> > v;
  Generator gen(inside(v(), s));

  std::set<sc_int<8> > seen;
  for (unsigned i = 0; i < 100; ++i) {
    BOOST_REQUIRE(gen.nextCov());
    BOOST_REQUIRE(s.find(v) != s.end());
    seen.insert(v);
  }
  BOOST_REQUIRE_EQUAL(seen.size(), s.size());
}

BOOST_AUTO_TEST_CASE(randv_sc_dt_op_support_t) {
  using namespace sc_dt;

  randv<sc_uint<4> > a;
  randv<sc_int<4> > b;
  randv<sc_bv<4> > c;

  a = 1;
  b = a;

  1 + a;
  1 - a;
  1 * a;
  1 / a;
  1 % a;
  BOOST_CHECK(1 == a);
  BOOST_CHECK(!(1 != a));
  BOOST_CHECK(!(1 < a));
  BOOST_CHECK(1 <= a);
  BOOST_CHECK(!(1 > a));
  BOOST_CHECK(1 >= a);
  1 & a;
  1 | a;
  1 ^ a;
  ~a;
  a << 1;
  a >> 1;
  a << b;
  a >> b;

  1 + b;
  1 - b;
  1 * b;
  1 / b;
  1 % b;
  BOOST_CHECK(1 == b);
  BOOST_CHECK(!(1 != b));
  BOOST_CHECK(!(1 < b));
  BOOST_CHECK(1 <= b);
  BOOST_CHECK(!(1 > b));
  BOOST_CHECK(1 >= b);
  1 & b;
  1 | b;
  1 ^ b;
  ~b;
  b << 1;
  b >> 1;
  b << a;
  b >> a;

  BOOST_CHECK(b == a);
  BOOST_CHECK(!(b != a));
  BOOST_CHECK(!(b < a));
  BOOST_CHECK(b <= a);
  BOOST_CHECK(!(b > a));
  BOOST_CHECK(b >= a);
  a + b;
  a - b;
  a* b;
  a / b;
  a % b;

  BOOST_CHECK(!(1 == c));
  BOOST_CHECK(1 != c);
  BOOST_CHECK(!(c == 1));
  BOOST_CHECK(c != 1);
}

BOOST_AUTO_TEST_CASE(dist_sc_int_t) {
  using namespace sc_dt;

  Variable<sc_int<8> > v;
  Generator gen(dist(v, distribution<sc_int<8> >::create(range<sc_int<8> >(-4, -2))(weighted_value<sc_int<8> >(100, 3))));

  std::set<int> seen;
  for (unsigned i = 0; i < 100; ++i) {
    BOOST_REQUIRE(gen.next());
    int r = gen[v].to_int();
    BOOST_REQUIRE((-4 <= r && r <= -2) || r == 100);
    seen.insert(r);
  }
  BOOST_REQUIRE_EQUAL(seen.size(), 4);
}

BOOST_AUTO_TEST_CASE(dist_sc_uint_t) {
  using namespace sc_dt;

  randv<sc_uint<8> > v;
  Generator gen(dist(v(), distribution<sc_uint<8> >::simple_range(250, 255)));

  std::set<unsigned> seen;
  for (unsigned i = 0; i < 100; ++i) {
    BOOST_REQUIRE(gen.next());
    unsigned r = static_cast<sc_uint<8> >(v).to_uint();
    BOOST_REQUIRE(250 <= r && r <= 255);
    seen.insert(r);
  }
  BOOST_REQUIRE_EQUAL(seen.size(), 6);
}

BOOST_AUTO_TEST_CASE(sc_biguint_t) {
  using namespace sc_dt;

  Variable<sc_biguint<2> > i, j;
  Variable<sc_bigint<3> > k;
  Generator gen(i != j && k == i - j);

  for (unsigned n = 0; n < 20; ++n) {
    BOOST_REQUIRE(gen.next());
    sc_biguint<2> iv = gen[i];
    sc_biguint<2> jv = gen[j];
    sc_bigint<3> kv = gen[k];
    BOOST_REQUIRE_NE(iv, jv);
    BOOST_REQUIRE_EQUAL(kv.to_int(), iv.to_int() - jv.to_int());
  }
}

BOOST_AUTO_TEST_CASE(inside_sc_biguint_t) {
  using namespace sc_dt;

  std::set<sc_biguint<8> > s{0, 3, 200, 255};
  Variable<sc_biguint<8> > v;
  Generator gen(inside(v, s));

  std::set<sc_biguint<8> > seen;
  for (unsigned i = 0; i < 100; ++i) {
    BOOST_REQUIRE(gen.nextCov());
    sc_biguint<8> r = gen[v];
    BOOST_REQUIRE(s.find(r) != s.end());
    seen.insert(r);
  }
  BOOST_REQUIRE_EQUAL(seen.size(), s.size());
}

BOOST_AUTO_TEST_CASE(inside_sc_bigint_t) {
  using namespace sc_dt;

  std::vector<long long> s{-5, 7, 1LL << 35};
  Variable<sc_bigint<40> > v;
  Generator gen(inside(v, s));

  std::set<long long> seen;
  for (unsigned i = 0; i < 100; ++i) {
    BOOST_REQUIRE(gen.nextCov());
    long long r = gen[v].to_int64();
    BOOST_REQUIRE(std::find(s.begin(), s.end(), r) != s.end());
    seen.insert(r);
  }
  BOOST_REQUIRE_EQUAL(seen.size(), s.size());
}

BOOST_AUTO_TEST_CASE(dist_sc_bigint_t) {
  using namespace sc_dt;

  Variable<sc_bigint<16> > v;
  Generator gen(dist(v, distribution<sc_bigint<16> >::create(range<sc_bigint<16> >(-1000, -998))(
                            weighted_value<sc_bigint<16> >(20000, 3))));

  std::set<long long> seen;
  for (unsigned i = 0; i < 100; ++i) {
    BOOST_REQUIRE(gen.next());
    long long r = gen[v].to_int64();
    BOOST_REQUIRE((-1000 <= r && r <= -998) || r == 20000);
    seen.insert(r);
  }
  BOOST_REQUIRE_EQUAL(seen.size(), 4);
}

BOOST_AUTO_TEST_CASE(randv_sc_bigint_t) {
  using namespace sc_dt;

  randv<sc_bigint<40> > a;
  randv<sc_biguint<12> > b;
  Generator gen;
  gen(a() > -(1LL << 38))(a() < -(1LL << 36));
  gen(b() > 4000u);

  for (unsigned n = 0; n < 20; ++n) {
    BOOST_REQUIRE(gen.next());
    long long const av = static_cast<sc_bigint<40> >(a).to_int64();
    unsigned long long const bv = static_cast<sc_biguint<12> >(b).to_uint64();
    BOOST_REQUIRE(av > -(1LL << 38) && av < -(1LL << 36));
    BOOST_REQUIRE(bv > 4000u && bv < 4096u);
    BOOST_REQUIRE(a < 0);
    BOOST_REQUIRE(0 > a);
    BOOST_REQUIRE_EQUAL(a + 1, av + 1);
    BOOST_REQUIRE_EQUAL(1 - a, 1 - av);
    BOOST_REQUIRE_EQUAL(b * 2, static_cast<long long>(bv * 2));
    BOOST_REQUIRE_EQUAL(b % 7, static_cast<long long>(bv % 7));
    BOOST_REQUIRE_EQUAL(~b & 0xfff, static_cast<long long>(~bv & 0xfff));
    BOOST_REQUIRE(b != 0);
  }

  a = -3;
  b = a;
  BOOST_REQUIRE_EQUAL(a, -3);
  BOOST_REQUIRE_EQUAL(b, 4093);
  ++b;
  BOOST_REQUIRE_EQUAL(b, 4094);
}

BOOST_AUTO_TEST_CASE(inside_negation_sc_t) {
  using namespace sc_dt;

  {
    std::set<sc_uint<8> > s{0, 64, 128, 192};
    Variable<sc_uint<8> > v;
    Generator gen(v == 64 && !inside(v, s));
    BOOST_REQUIRE(!gen.next());

    Variable<sc_uint<8> > v2;
    Generator gen2(!inside(v2, s) && v2 <= 2);
    for (int i = 0; i < 15; ++i) {
      BOOST_REQUIRE(gen2.next());
      BOOST_REQUIRE(gen2[v2] == 1 || gen2[v2] == 2);
    }
  }
  {
    std::set<sc_int<8> > s{-128, -1, 5, 127};
    randv<sc_int<8> > v;
    Variable<bool> flag;
    Generator gen(v() == -1 && if_then(inside(v(), s), flag == true) && flag == false);
    BOOST_REQUIRE(!gen.next());

    randv<sc_int<8> > v2;
    Generator gen2(!inside(v2(), s) && v2() >= -2 && v2() <= 2);
    for (int i = 0; i < 15; ++i) {
      BOOST_REQUIRE(gen2.next());
      BOOST_REQUIRE_NE(v2, -1);
    }
  }
  {
    std::set<sc_biguint<8> > s{0, 3, 200, 255};
    Variable<sc_biguint<8> > v;
    Generator gen(v == 200 && !inside(v, s));
    BOOST_REQUIRE(!gen.next());
  }
  {
    std::vector<long long> s{-5, 7, 1LL << 35};
    Variable<sc_bigint<40> > v;
    Generator gen(v == (1LL << 35) && !inside(v, s));
    BOOST_REQUIRE(!gen.next());
  }
}

BOOST_AUTO_TEST_CASE(sc_bv_inside_and_dist_t) {
  using namespace sc_dt;
  struct Guard {
    ~Guard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    // inside with std::vector<sc_bv<8>>, std::array, initializer_list, range, tol_plusminus, inside_mask
    {
      Variable<sc_bv<8> > bv;
      std::vector<sc_bv<8> > vec{sc_bv<8>(3), sc_bv<8>(7), sc_bv<8>(0xA5)};
      Generator g_vec(inside(bv, vec));
      std::set<uint64_t> seen;
      for (int i = 0; i < 40; ++i) {
        BOOST_REQUIRE(g_vec.nextCov());
        uint64_t val = g_vec[bv].to_uint64();
        BOOST_REQUIRE(val == 3u || val == 7u || val == 0xA5u);
        seen.insert(val);
      }
      BOOST_REQUIRE_EQUAL(seen.size(), 3u);

      std::array<sc_bv<8>, 2> arr{{sc_bv<8>(11), sc_bv<8>(22)}};
      Generator g_arr(inside(bv, arr));
      BOOST_REQUIRE(g_arr.next());
      BOOST_REQUIRE(g_arr[bv].to_uint64() == 11u || g_arr[bv].to_uint64() == 22u);

      Generator g_init(inside(bv, {sc_bv<8>(0x10), sc_bv<8>(0x20)}));
      BOOST_REQUIRE(g_init.next());
      BOOST_REQUIRE(g_init[bv].to_uint64() == 0x10u || g_init[bv].to_uint64() == 0x20u);

      Generator g_rng(inside(bv, range(sc_bv<8>(5), sc_bv<8>(9))));
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(g_rng.next());
        BOOST_REQUIRE(g_rng[bv].to_uint64() >= 5u && g_rng[bv].to_uint64() <= 9u);
      }

      Generator g_tol(inside(bv, tol_plusminus(sc_bv<8>(2), sc_bv<8>(5))));
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(g_tol.next());
        BOOST_REQUIRE_LE(g_tol[bv].to_uint64(), 7u);
      }

      Generator g_mask(inside(bv, inside_mask("8'hB?")) && !inside(bv, {sc_bv<8>(0xB0)}));
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(g_mask.next());
        uint64_t v = g_mask[bv].to_uint64();
        BOOST_REQUIRE((v & 0xF0u) == 0xB0u && v != 0xB0u);
      }
    }

    // dist with sc_bv<8>
    {
      randv<sc_bv<8> > bv;
      Generator g_dist(dist(bv(), distribution<sc_bv<8> >::create(range<sc_bv<8> >(10, 14))));
      std::set<uint64_t> seen;
      for (int i = 0; i < 60; ++i) {
        BOOST_REQUIRE(g_dist.next());
        uint64_t v = static_cast<sc_bv<8> >(bv).to_uint64();
        BOOST_REQUIRE(v >= 10u && v <= 14u);
        seen.insert(v);
      }
      BOOST_REQUIRE_EQUAL(seen.size(), 5u);

      Variable<sc_bv<8> > bv2;
      Generator g_simple(dist(bv2, distribution<sc_bv<8> >::simple_range(20, 22)));
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(g_simple.next());
        uint64_t v = g_simple[bv2].to_uint64();
        BOOST_REQUIRE(v >= 20u && v <= 22u);
      }
    }

    // sc_bv<64> MSB 0x8000000000000000ULL
    {
      Variable<sc_bv<64> > bv64;
      sc_bv<64> msb_val;
      msb_val = 0x8000000000000000ULL;
      std::vector<sc_bv<64> > s64{msb_val};
      Generator g64(inside(bv64, s64));
      BOOST_REQUIRE(g64.next());
      BOOST_REQUIRE_EQUAL(g64[bv64].to_uint64(), 0x8000000000000000ULL);
    }

    // 4-bit sc_uint<4> unpinned symbolic tol_pluspct (both Z3 and Cudd, sv in {false, true})
    {
      bool const old_sv = sv_signedness();
      for (bool sv : {false, true}) {
        set_sv_signedness(sv);
        Variable<sc_uint<4> > x4, val4, pct4;
        Generator g4(inside(x4, tol_pluspct(val4, pct4)));
        for (int i = 0; i < 15; ++i) {
          BOOST_REQUIRE(g4.next());
          uint64_t xv = g4[x4].to_uint64();
          uint64_t vv = g4[val4].to_uint64();
          uint64_t pv = g4[pct4].to_uint64();
          uint64_t delta = (vv * pv) / 100ULL;
          BOOST_REQUIRE(xv + delta >= vv && xv <= vv + delta);
        }
      }
      set_sv_signedness(old_sv);
    }

    // Variable<sc_bv<16>> + rand_vec<sc_bv<16>> (both Z3 and Cudd)
    {
      Variable<sc_bv<16> > bv16;
      rand_vec<sc_bv<16> > v16(NULL);
      Generator g_vec16;
      g_vec16(v16().size() == 3)(foreach (v16(), v16()[_i] == sc_bv<16>(0x1234) || v16()[_i] == sc_bv<16>(0xABCD)))(
          inside(bv16, v16));
      for (int i = 0; i < 8; ++i) {
        BOOST_REQUIRE(g_vec16.next());
        uint64_t xval = g_vec16[bv16].to_uint64();
        bool found = false;
        for (unsigned j = 0; j < v16.size(); ++j)
          if (v16[j].to_uint64() == xval) found = true;
        BOOST_REQUIRE(found);
      }
    }

    // Variable<sc_bv<64>> + rand_vec<sc_bv<64>> (Z3)
    if (std::string(backend) == "Z3") {
      Variable<sc_bv<64> > bv64;
      rand_vec<sc_bv<64> > v64(NULL);
      sc_bv<64> c0, c1, c2;
      c0 = 0x8000000000000000ULL;
      c1 = 0xFFFFFFFFFFFFFFFFULL;
      c2 = 0x123456789ABCDEF0ULL;
      Generator g_vec64;
      g_vec64(v64().size() == 3)(foreach (v64(), v64()[_i] == c0 || v64()[_i] == c1 || v64()[_i] == c2))(
          inside(bv64, v64));
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(g_vec64.next());
        uint64_t xval = g_vec64[bv64].to_uint64();
        BOOST_REQUIRE(xval == 0x8000000000000000ULL || xval == 0xFFFFFFFFFFFFFFFFULL ||
                      xval == 0x123456789ABCDEF0ULL);
        bool found = false;
        for (unsigned j = 0; j < v64.size(); ++j)
          if (v64[j].to_uint64() == xval) found = true;
        BOOST_REQUIRE(found);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(sc_bv_wide_readback_t) {
  using namespace sc_dt;
  struct Guard {
    ~Guard() { set_solver_backend("Boolector"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    // 1. Variable<sc_bv<128>> >= 2^64 (Z3 #x hex literal read-back + CUDD parity)
    {
      Variable<sc_bv<128> > x;
      Generator gen(bitslice(127, 64, x) == uint64_t(0xFEDCBA9876543210ULL) &&
                    bitslice(63, 0, x) == uint64_t(0x0123456789ABCDEFULL));
      BOOST_REQUIRE(gen.next());
      sc_bv<128> xv = gen[x];
      BOOST_REQUIRE_EQUAL(xv.range(127, 64).to_uint64(), 0xFEDCBA9876543210ULL);
      BOOST_REQUIRE_EQUAL(xv.range(63, 0).to_uint64(), 0x0123456789ABCDEFULL);
    }

    // 2. Variable<sc_bv<128>> and WriteReference<sc_bv<128>> with >= 2^64, inside_mask, inside, and dist
    {
      Variable<sc_bv<128> > x;
      Variable<sc_bv<64> > lo64;
      Generator gen(bitslice(127, 64, x) == uint64_t(0xFEDCBA9876543210ULL) && inside(x, inside_mask("'h?1")) &&
                    bitslice(63, 0, x) == lo64 &&
                    dist(lo64, distribution<sc_bv<64> >::create(range<sc_bv<64> >(0x10ULL, 0x3FULL))));
      for (int i = 0; i < 5; ++i) {
        BOOST_REQUIRE(gen.next());
        sc_bv<128> xv = gen[x];
        BOOST_REQUIRE_EQUAL(xv.range(127, 64).to_uint64(), 0xFEDCBA9876543210ULL);
        uint64_t lo = xv.range(63, 0).to_uint64();
        BOOST_REQUIRE(lo >= 0x10ULL && lo <= 0x3FULL);
        BOOST_REQUIRE_EQUAL(lo & 0xFULL, 1ULL);
      }

      sc_bv<128> ref_val(0);
      WriteReference<sc_bv<128> > xr(&ref_val);
      Generator gen_ref(bitslice(127, 64, xr) == uint64_t(0xFEDCBA9876543210ULL) &&
                        inside(xr, inside_mask("'h?1")) &&
                        inside(bitslice(7, 0, xr), range(uint64_t(0x10), uint64_t(0x5F))));
      for (int i = 0; i < 5; ++i) {
        BOOST_REQUIRE(gen_ref.next());
        BOOST_REQUIRE_EQUAL(ref_val.range(127, 64).to_uint64(), 0xFEDCBA9876543210ULL);
        uint64_t lo8 = ref_val.range(7, 0).to_uint64();
        BOOST_REQUIRE(lo8 >= 0x10ULL && lo8 <= 0x5FULL);
        BOOST_REQUIRE_EQUAL(lo8 & 0xFULL, 1ULL);
      }
    }

    // 3. Variable<sc_bv<128>> < 2^64 (Z3_get_numeral_uint64 path with bv_size > 64)
    {
      Variable<sc_bv<128> > x;
      Generator gen(bitslice(127, 64, x) == uint64_t(0) &&
                    bitslice(63, 0, x) == uint64_t(0x0123456789ABCDEFULL));
      BOOST_REQUIRE(gen.next());
      sc_bv<128> xv = gen[x];
      BOOST_REQUIRE_EQUAL(xv.range(127, 64).to_uint64(), 0ULL);
      BOOST_REQUIRE_EQUAL(xv.range(63, 0).to_uint64(), 0x0123456789ABCDEFULL);
    }

    // 4. Non-4-multiple wide widths: sc_bv<65> and sc_bv<129> (Z3 #b path + CUDD)
    {
      Variable<sc_bv<65> > x65;
      Generator g65(bitslice(64, 64, x65) == uint64_t(1) &&
                    bitslice(63, 0, x65) == uint64_t(0x123456789ABCDEF0ULL));
      BOOST_REQUIRE(g65.next());
      sc_bv<65> v65 = g65[x65];
      BOOST_REQUIRE_EQUAL(v65.range(64, 64).to_uint64(), 1ULL);
      BOOST_REQUIRE_EQUAL(v65.range(63, 0).to_uint64(), 0x123456789ABCDEF0ULL);

      sc_bv<129> ref129(0);
      WriteReference<sc_bv<129> > r129(&ref129);
      Generator g129(bitslice(128, 128, r129) == uint64_t(1) &&
                     bitslice(127, 64, r129) == uint64_t(0xCAFEBABE12345678ULL) &&
                     bitslice(63, 0, r129) == uint64_t(0x89ABCDEF01234567ULL));
      BOOST_REQUIRE(g129.next());
      BOOST_REQUIRE_EQUAL(ref129.range(128, 128).to_uint64(), 1ULL);
      BOOST_REQUIRE_EQUAL(ref129.range(127, 64).to_uint64(), 0xCAFEBABE12345678ULL);
      BOOST_REQUIRE_EQUAL(ref129.range(63, 0).to_uint64(), 0x89ABCDEF01234567ULL);
    }

    // 5. Non-power-of-2 4-multiple width sc_bv<68>
    {
      Variable<sc_bv<68> > x68;
      Generator g68(bitslice(67, 64, x68) == uint64_t(0xA) &&
                    bitslice(63, 0, x68) == uint64_t(0x123456789ABCDEF0ULL));
      BOOST_REQUIRE(g68.next());
      sc_bv<68> v68 = g68[x68];
      BOOST_REQUIRE_EQUAL(v68.range(67, 64).to_uint64(), 0xAULL);
      BOOST_REQUIRE_EQUAL(v68.range(63, 0).to_uint64(), 0x123456789ABCDEF0ULL);
    }

    // 6. Very wide 4-multiple width sc_bv<256> on Z3 (avoids 256-bit CUDD BDD overhead)
    if (std::string(backend) == "Z3") {
      Variable<sc_bv<256> > x256;
      Generator g256(bitslice(255, 192, x256) == uint64_t(0x0123456789ABCDEFULL) &&
                     bitslice(191, 128, x256) == uint64_t(0xFEDCBA9876543210ULL) &&
                     bitslice(127, 64, x256) == uint64_t(0xA5A5A5A55A5A5A5AULL) &&
                     inside(x256, inside_mask("'h?3")));
      BOOST_REQUIRE(g256.next());
      sc_bv<256> v256 = g256[x256];
      BOOST_REQUIRE_EQUAL(v256.range(255, 192).to_uint64(), 0x0123456789ABCDEFULL);
      BOOST_REQUIRE_EQUAL(v256.range(191, 128).to_uint64(), 0xFEDCBA9876543210ULL);
      BOOST_REQUIRE_EQUAL(v256.range(127, 64).to_uint64(), 0xA5A5A5A55A5A5A5AULL);
      BOOST_REQUIRE_EQUAL(v256.range(3, 0).to_uint64(), 3ULL);

      sc_bv<256> ref256(0);
      WriteReference<sc_bv<256> > r256(&ref256);
      Generator g256_ref(bitslice(255, 192, r256) == uint64_t(0x1122334455667788ULL) &&
                         bitslice(191, 128, r256) == uint64_t(0x99AABBCCDDEEFF00ULL) &&
                         bitslice(127, 64, r256) == uint64_t(0x13579BDF02468ACEULL) &&
                         bitslice(63, 0, r256) == uint64_t(0x0F1E2D3C4B5A6978ULL));
      BOOST_REQUIRE(g256_ref.next());
      BOOST_REQUIRE_EQUAL(ref256.range(255, 192).to_uint64(), 0x1122334455667788ULL);
      BOOST_REQUIRE_EQUAL(ref256.range(191, 128).to_uint64(), 0x99AABBCCDDEEFF00ULL);
      BOOST_REQUIRE_EQUAL(ref256.range(127, 64).to_uint64(), 0x13579BDF02468ACEULL);
      BOOST_REQUIRE_EQUAL(ref256.range(63, 0).to_uint64(), 0x0F1E2D3C4B5A6978ULL);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()  // SystemC_Data

//  vim: ft=cpp:ts=2:sw=2:expandtab

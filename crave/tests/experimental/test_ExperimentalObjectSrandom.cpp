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
#include <crave/experimental/Experimental.hpp>
#include <crave/backend/FactoryMetaSMT.hpp>

#include <cstdint>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

using namespace crave;

namespace {

struct SrandomBackendGuard {
  SrandomBackendGuard() : old_sv_(sv_signedness()) {}
  ~SrandomBackendGuard() {
    set_solver_backend("auto");
    set_sv_signedness(old_sv_);
  }
  bool old_sv_;
};

struct SrandomItem : public crv_sequence_item {
  SrandomItem(crv_object_name = "item") {}
  crv_variable<int> a{"a"};
  crv_variable<unsigned> b{"b"};
  crv_variable<unsigned char> rc{"rc"};
  crv_variable<bool> flag{"flag"};
  crv_vector<unsigned> vec{"vec"};
  crv_constraint c_a{"c_a", a() >= -20 && a() <= 50};
  crv_constraint c_b{"c_b", dist(b(), make_distribution(dist_per_value(1u, 4u, 3), dist_per_range(10u, 15u, 6)))};
  crv_constraint c_rc{"c_rc", rc() < 16};
  crv_constraint c_vec{"c_vec", vec().size() == 3, foreach (vec(), vec()[_i] < 100u)};
  crv_solve_before sb{"sb", flag, a};

  void init_randc() { rc.randc(true); }
};

struct SrandomSnapshot {
  int a;
  unsigned b;
  unsigned char rc;
  bool flag;
  std::vector<unsigned> vec;

  bool operator==(SrandomSnapshot const& o) const {
    return a == o.a && b == o.b && rc == o.rc && flag == o.flag && vec == o.vec;
  }
};

SrandomSnapshot capture_snapshot(SrandomItem const& item) {
  SrandomSnapshot s{item.a, item.b, item.rc, item.flag, {}};
  for (std::size_t i = 0; i < const_cast<crv_vector<unsigned>&>(item.vec).size(); ++i) {
    s.vec.push_back(item.vec[static_cast<int>(i)]);
  }
  return s;
}

struct ArrayParentItem : public crv_sequence_item {
  ArrayParentItem(crv_object_name = "parent") {}
  struct Leaf : public crv_sequence_item {
    Leaf(crv_object_name = "leaf") {}
    crv_variable<unsigned> v{"v"};
    crv_constraint c{"c", v() < 1000u};
  };
  crv_object_array<Leaf, 3> arr{"arr"};
};

}  // namespace

BOOST_FIXTURE_TEST_SUITE(ExperimentalObjectSrandom_t, Context_Fixture)

BOOST_AUTO_TEST_CASE(srandom_determinism_and_isolation_backends) {
  SrandomBackendGuard guard;
  for (char const* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);
    for (bool sv : {false, true}) {
      set_sv_signedness(sv);

      SrandomItem itemA("itemA");
      SrandomItem itemB("itemB");
      SrandomItem itemC("itemC");
      itemA.init_randc();
      itemB.init_randc();
      itemC.init_randc();

      itemA.srandom(12345u);
      itemB.srandom(12345u);
      itemC.srandom(99999u);

      std::vector<SrandomSnapshot> seqA, seqB, seqC;
      for (int i = 0; i < 12; ++i) {
        if (i % 3 == 0) {
          BOOST_REQUIRE(itemA.randomize_with(itemA.a() != 0));
          // Advance itemC heavily in the middle to verify non-interference
          for (int j = 0; j < 7; ++j) BOOST_REQUIRE(itemC.randomize());
          BOOST_REQUIRE(itemB.randomize_with(itemB.a() != 0));
        } else {
          BOOST_REQUIRE(itemA.randomize());
          BOOST_REQUIRE(itemB.randomize());
          BOOST_REQUIRE(itemC.randomize());
        }
        seqA.push_back(capture_snapshot(itemA));
        seqB.push_back(capture_snapshot(itemB));
        seqC.push_back(capture_snapshot(itemC));
      }

      BOOST_REQUIRE(seqA == seqB);
      bool all_equal_c = (seqA == seqC);
      BOOST_REQUIRE(!all_equal_c);
    }
  }
}

BOOST_AUTO_TEST_CASE(srandom_zero_and_standalone_primitives) {
  crv_variable<unsigned> v1("v1"), v2("v2"), v3("v3");
  v1.srandom(0u);
  v2.srandom(0u);
  v3.srandom(1u);

  std::vector<unsigned> s1, s2, s3;
  for (int i = 0; i < 16; ++i) {
    BOOST_REQUIRE(v1.randomize());
    BOOST_REQUIRE(v2.randomize());
    BOOST_REQUIRE(v3.randomize());
    s1.push_back(v1);
    s2.push_back(v2);
    s3.push_back(v3);
  }
  BOOST_REQUIRE(s1 == s2);
  BOOST_REQUIRE(s1 != s3);

  crv_vector<int> vec1("vec1"), vec2("vec2");
  vec1.srandom(777u);
  vec2.srandom(777u);
  BOOST_REQUIRE(vec1.randomize());
  BOOST_REQUIRE(vec2.randomize());
  BOOST_REQUIRE_EQUAL(vec1.size(), vec2.size());
  for (std::size_t i = 0; i < vec1.size(); ++i) {
    BOOST_REQUIRE_EQUAL(vec1[static_cast<int>(i)], vec2[static_cast<int>(i)]);
  }
}

BOOST_AUTO_TEST_CASE(get_and_set_randstate_pre_and_mid_solve) {
  SrandomBackendGuard guard;
  for (char const* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    // Pre-solve capture and replay (with randc enabled)
    SrandomItem item("item");
    item.init_randc();
    item.srandom(2025u);
    std::string const pre_state = item.get_randstate();

    std::vector<SrandomSnapshot> run1;
    for (int i = 0; i < 6; ++i) {
      if (i % 2 == 0)
        BOOST_REQUIRE(item.randomize());
      else
        BOOST_REQUIRE(item.randomize_with(item.a() >= -10));
      run1.push_back(capture_snapshot(item));
    }

    item.set_randstate(pre_state);
    std::vector<SrandomSnapshot> run2;
    for (int i = 0; i < 6; ++i) {
      if (i % 2 == 0)
        BOOST_REQUIRE(item.randomize());
      else
        BOOST_REQUIRE(item.randomize_with(item.a() >= -10));
      run2.push_back(capture_snapshot(item));
    }
    BOOST_REQUIRE(run1 == run2);

    // Mid-solve capture and replay (with randc enabled)
    SrandomItem mid("mid");
    mid.init_randc();
    mid.srandom(4242u);
    for (int i = 0; i < 3; ++i) BOOST_REQUIRE(mid.randomize());
    std::string const mid_state = mid.get_randstate();

    std::vector<SrandomSnapshot> mid_run1;
    for (int i = 0; i < 5; ++i) {
      BOOST_REQUIRE(mid.randomize());
      mid_run1.push_back(capture_snapshot(mid));
    }

    mid.set_randstate(mid_state);
    std::vector<SrandomSnapshot> mid_run2;
    for (int i = 0; i < 5; ++i) {
      BOOST_REQUIRE(mid.randomize());
      mid_run2.push_back(capture_snapshot(mid));
    }
    BOOST_REQUIRE(mid_run1 == mid_run2);

    // Transfer randstate to another object of the same type (both with and without prior init_randc)
    SrandomItem peer("peer");
    peer.init_randc();
    peer.set_randstate(mid_state);
    std::vector<SrandomSnapshot> peer_run;
    for (int i = 0; i < 5; ++i) {
      BOOST_REQUIRE(peer.randomize());
      peer_run.push_back(capture_snapshot(peer));
    }
    BOOST_REQUIRE(mid_run1 == peer_run);

    SrandomItem peer_fresh("peer_fresh");
    peer_fresh.set_randstate(mid_state);
    BOOST_REQUIRE(peer_fresh.rc.randc());
    std::vector<SrandomSnapshot> peer_fresh_run;
    for (int i = 0; i < 5; ++i) {
      BOOST_REQUIRE(peer_fresh.randomize());
      peer_fresh_run.push_back(capture_snapshot(peer_fresh));
    }
    BOOST_REQUIRE(mid_run1 == peer_fresh_run);
  }
}

BOOST_AUTO_TEST_CASE(randstate_validation_and_child_reset) {
  ArrayParentItem p("p");
  p.srandom(314u);
  // Capture state before children randomize, then randomize child[0] and verify restore
  ArrayParentItem q("q");
  std::string const q_initial_state = q.get_randstate();

  std::string const p_state = p.get_randstate();
  BOOST_REQUIRE(p.arr[0].randomize());
  unsigned first_val = p.arr[0].v;
  p.set_randstate(p_state);
  BOOST_REQUIRE(p.arr[0].randomize());
  BOOST_REQUIRE_EQUAL(static_cast<unsigned>(p.arr[0].v), first_val);

  // Verify q_initial_state still works and resets children initialized later
  BOOST_REQUIRE(q.arr[0].randomize());
  q.set_randstate(q_initial_state);

  // Malformed states must throw std::invalid_argument without mutating p's RNG state
  p.set_randstate(p_state);
  BOOST_REQUIRE(p.arr[0].randomize());
  unsigned expected_next = 0;
  {
    ArrayParentItem p_ref("p_ref");
    p_ref.set_randstate(p_state);
    BOOST_REQUIRE(p_ref.arr[0].randomize());
    BOOST_REQUIRE(p_ref.arr[0].randomize());
    expected_next = p_ref.arr[0].v;
  }

  BOOST_REQUIRE_THROW(p.set_randstate(""), std::invalid_argument);
  BOOST_REQUIRE_THROW(p.set_randstate("garbage"), std::invalid_argument);
  BOOST_REQUIRE_THROW(p.set_randstate("CRV_RNG_V1 1 2 3"), std::invalid_argument);
  BOOST_REQUIRE_THROW(p.set_randstate(p_state + " trailing_token"), std::invalid_argument);

  // State of p must remain uncorrupted after failed set_randstate calls
  BOOST_REQUIRE(p.arr[0].randomize());
  BOOST_REQUIRE_EQUAL(static_cast<unsigned>(p.arr[0].v), expected_next);
}

BOOST_AUTO_TEST_CASE(hierarchical_srandom_distinct_child_seeds) {
  ArrayParentItem p1("p1"), p2("p2");
  p1.srandom(888u);
  p2.srandom(888u);

  std::vector<unsigned> v1, v2;
  for (int i = 0; i < 3; ++i) {
    BOOST_REQUIRE(p1.arr[static_cast<unsigned>(i)].randomize());
    BOOST_REQUIRE(p2.arr[static_cast<unsigned>(i)].randomize());
    v1.push_back(p1.arr[static_cast<unsigned>(i)].v);
    v2.push_back(p2.arr[static_cast<unsigned>(i)].v);
  }
  BOOST_REQUIRE(v1 == v2);
  // Distinct derived seeds mean elements should not all produce identical clones
  BOOST_REQUIRE(!(v1[0] == v1[1] && v1[1] == v1[2]));
}

BOOST_AUTO_TEST_CASE(multithreaded_separate_objects_determinism) {
  constexpr int kThreads = 4;
  constexpr int kIters = 10;

  std::vector<std::vector<SrandomSnapshot>> single_thread(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    SrandomItem item("st_item");
    item.srandom(static_cast<unsigned>(1000 + t));
    for (int i = 0; i < kIters; ++i) {
      if (i % 2 == 0)
        BOOST_REQUIRE(item.randomize());
      else
        BOOST_REQUIRE(item.randomize_with(item.a() >= -5));
      single_thread[static_cast<std::size_t>(t)].push_back(capture_snapshot(item));
    }
  }

  std::vector<std::vector<SrandomSnapshot>> multi_thread(kThreads);
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([t, kIters, &multi_thread]() {
      SrandomItem item("mt_item");
      item.srandom(static_cast<unsigned>(1000 + t));
      for (int i = 0; i < kIters; ++i) {
        bool ok = (i % 2 == 0) ? item.randomize() : item.randomize_with(item.a() >= -5);
        if (!ok) return;
        multi_thread[static_cast<std::size_t>(t)].push_back(capture_snapshot(item));
      }
    });
  }
  for (std::thread& th : threads) th.join();

  for (int t = 0; t < kThreads; ++t) {
    BOOST_REQUIRE_EQUAL(multi_thread[static_cast<std::size_t>(t)].size(), static_cast<std::size_t>(kIters));
    BOOST_REQUIRE(multi_thread[static_cast<std::size_t>(t)] == single_thread[static_cast<std::size_t>(t)]);
  }
}

BOOST_AUTO_TEST_CASE(generator_mt_per_object_determinism) {
  struct MtItem : public crv_sequence_item {
    MtItem(crv_object_name = "mt") {}
    crv_variable<unsigned> x1{"x1"}, x2{"x2"}, y1{"y1"}, y2{"y2"};
    crv_constraint c1{"c1", x1() < 100u && x2() < 100u && x1() != x2()};
    crv_constraint c2{"c2", y1() < 100u && y2() < 100u && y1() + y2() == 50u};
  };

  auto run_mt = [](unsigned seed) {
    MtItem item("mt_item");
    item.srandom(seed);
    std::vector<std::tuple<unsigned, unsigned, unsigned, unsigned>> seq;
    for (int i = 0; i < 8; ++i) {
      BOOST_REQUIRE(item.randomize());
      seq.emplace_back(item.x1, item.x2, item.y1, item.y2);
    }
    return seq;
  };
  BOOST_REQUIRE(run_mt(555u) == run_mt(555u));

  // Direct VariableGeneratorMT test with ScopedRngGuard
  auto run_vgen_mt = [](unsigned seed) {
    Variable<unsigned> a, b, c, d;
    Generator g;
    g(a < 50u && b < 50u && a != b)(c < 50u && d < 50u && c + d == 40u);
    g.enable_multithreading();
    std::mt19937 obj_rng(seed);
    ScopedRngGuard guard(&obj_rng);
    std::vector<std::tuple<unsigned, unsigned, unsigned, unsigned>> seq;
    for (int i = 0; i < 8; ++i) {
      BOOST_REQUIRE(g.next());
      seq.emplace_back(g[a], g[b], g[c], g[d]);
    }
    return seq;
  };
  BOOST_REQUIRE(run_vgen_mt(777u) == run_vgen_mt(777u));
}

BOOST_AUTO_TEST_CASE(mid_solve_z3_cudd_sv_signedness_solve_before_dist_and_randc_replay) {
  struct FullD1Item : public crv_sequence_item {
    FullD1Item(crv_object_name = "full_d1") {}
    crv_variable<int> a{"a"};
    crv_variable<unsigned> b{"b"};
    crv_variable<unsigned char> rc{"rc"};
    crv_variable<bool> flag{"flag"};
    crv_variable<unsigned> ins{"ins"};
    crv_vector<unsigned> vec{"vec"};
    crv_constraint c_a{"c_a", if_then(flag(), a() >= -15 && a() <= 40),
                       if_then(!flag(), a() >= -5 && a() <= 5)};
    crv_constraint c_b{"c_b", dist(b(), make_distribution(dist_per_value(1u, 4u, 3), dist_per_range(10u, 15u, 6)))};
    crv_constraint c_rc{"c_rc", rc() < 16};
    crv_constraint c_ins{"c_ins", inside(ins(), range(10u, 30u))};
    crv_constraint c_vec{"c_vec", vec().size() == 3, foreach (vec(), vec()[_i] < 100u)};
    crv_solve_before sb{"sb", flag, a};

    void enable_randc() { rc.randc(true); }
  };

  auto snap = [](FullD1Item const& it) {
    return std::make_tuple(static_cast<int>(it.a), static_cast<unsigned>(it.b),
                           static_cast<unsigned char>(it.rc), static_cast<bool>(it.flag),
                           static_cast<unsigned>(it.ins),
                           static_cast<unsigned>(const_cast<crv_vector<unsigned>&>(it.vec)[0]));
  };

  SrandomBackendGuard guard;
  for (char const* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);
    for (bool sv : {false, true}) {
      set_sv_signedness(sv);
      for (bool with_randc : {false, true}) {
        FullD1Item it("it"), it_twin("it_twin");
        if (with_randc) {
          it.enable_randc();
          it_twin.enable_randc();
        }
        it.srandom(31337u);
        it_twin.srandom(31337u);
        for (int i = 0; i < 4; ++i) {
          BOOST_REQUIRE(it.randomize());
          BOOST_REQUIRE(it_twin.randomize());
        }
        std::string const mid = it.get_randstate();

        std::vector<decltype(snap(it))> m1, m_twin, m2, m_peer;
        for (int i = 0; i < 6; ++i) {
          bool ok = (i % 2 == 0) ? it.randomize_with(it.a() != 0) : it.randomize();
          bool ok_t = (i % 2 == 0) ? it_twin.randomize_with(it_twin.a() != 0) : it_twin.randomize();
          BOOST_REQUIRE(ok && ok_t);
          m1.push_back(snap(it));
          m_twin.push_back(snap(it_twin));
        }
        BOOST_REQUIRE(m1 == m_twin);

        it.set_randstate(mid);
        for (int i = 0; i < 6; ++i) {
          bool ok = (i % 2 == 0) ? it.randomize_with(it.a() != 0) : it.randomize();
          BOOST_REQUIRE(ok);
          m2.push_back(snap(it));
        }

        FullD1Item peer("peer");
        if (with_randc) peer.enable_randc();
        peer.set_randstate(mid);
        for (int i = 0; i < 6; ++i) {
          bool ok = (i % 2 == 0) ? peer.randomize_with(peer.a() != 0) : peer.randomize();
          BOOST_REQUIRE(ok);
          m_peer.push_back(snap(peer));
        }
        BOOST_REQUIRE(m2 == m_peer);
        if (std::string(backend) == "Cudd") {
          BOOST_REQUIRE(m1 == m2);
        } else {
          for (std::size_t i = 0; i < m1.size(); ++i) {
            int const a1 = std::get<0>(m1[i]);
            bool const f1 = std::get<3>(m1[i]);
            if (f1) {
              BOOST_REQUIRE(a1 >= -15 && a1 <= 40);
            } else {
              BOOST_REQUIRE(a1 >= -5 && a1 <= 5);
            }
            if (i % 2 == 0) BOOST_REQUIRE_NE(a1, 0);
            BOOST_REQUIRE_EQUAL(std::get<1>(m1[i]), std::get<1>(m2[i]));
            BOOST_REQUIRE_EQUAL(std::get<2>(m1[i]), std::get<2>(m2[i]));
            BOOST_REQUIRE_EQUAL(std::get<3>(m1[i]), std::get<3>(m2[i]));
            BOOST_REQUIRE_EQUAL(std::get<4>(m1[i]), std::get<4>(m2[i]));
            BOOST_REQUIRE_EQUAL(std::get<5>(m1[i]), std::get<5>(m2[i]));
          }
        }
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(descendant_array_reset_clears_child_solver_and_randc) {
  struct ChildWithRandc : public crv_sequence_item {
    ChildWithRandc(crv_object_name = "child") {}
    crv_variable<unsigned char> rc{"rc"};
    crv_variable<int> x{"x"};
    crv_constraint c{"c", rc() < 8, x() >= -4 && x() <= 4};
  };
  struct Holder : public crv_sequence_item {
    Holder(crv_object_name = "holder") {}
    crv_object_array<ChildWithRandc, 2> arr{"arr"};
  };

  SrandomBackendGuard guard;
  set_solver_backend("Z3");
  Holder h("h");
  // Capture before arr[0] is ever initialized (flag == 0 for arr and children)
  std::string const initial = h.get_randstate();

  // Warm Z3 solver + enable randc on descendant after initial capture
  h.arr[0].rc.randc(true);
  for (int i = 0; i < 4; ++i) BOOST_REQUIRE(h.arr[0].randomize());

  // Restore initial snapshot: must reset descendant RNG + randc state + clear descendant Z3 solver
  h.set_randstate(initial);
  h.arr[0].srandom(999u);
  std::vector<std::pair<unsigned char, int>> run1;
  for (int i = 0; i < 6; ++i) {
    BOOST_REQUIRE(h.arr[0].randomize());
    run1.emplace_back(h.arr[0].rc, h.arr[0].x);
  }
  h.set_randstate(initial);
  h.arr[0].srandom(999u);
  std::vector<std::pair<unsigned char, int>> run2;
  for (int i = 0; i < 6; ++i) {
    BOOST_REQUIRE(h.arr[0].randomize());
    run2.emplace_back(h.arr[0].rc, h.arr[0].x);
  }
  BOOST_REQUIRE(run1 == run2);
}

BOOST_AUTO_TEST_CASE(get_and_set_randstate_non_c_locale_isolation) {
  struct CommaNumpunct : std::numpunct<char> {
   protected:
    char do_thousands_sep() const override { return ','; }
    std::string do_grouping() const override { return "\3"; }
  };
  struct LocaleGuard {
    LocaleGuard() : old_(std::locale::global(std::locale(std::locale::classic(), new CommaNumpunct()))) {}
    ~LocaleGuard() { std::locale::global(old_); }
    std::locale old_;
  };

  LocaleGuard lguard;
  SrandomItem item("loc_item");
  item.init_randc();
  item.srandom(13579u);
  for (int i = 0; i < 3; ++i) BOOST_REQUIRE(item.randomize());
  std::string const saved = item.get_randstate();
  BOOST_REQUIRE_EQUAL(saved.find(','), std::string::npos);

  std::vector<SrandomSnapshot> run1, run2;
  for (int i = 0; i < 5; ++i) {
    BOOST_REQUIRE(item.randomize());
    run1.push_back(capture_snapshot(item));
  }
  item.set_randstate(saved);
  for (int i = 0; i < 5; ++i) {
    BOOST_REQUIRE(item.randomize());
    run2.push_back(capture_snapshot(item));
  }
  BOOST_REQUIRE(run1 == run2);
}

BOOST_AUTO_TEST_CASE(concurrent_randomize_with_cache_misses) {
  constexpr int kThreads = 6;
  constexpr int kIters = 8;
  std::vector<int> ok_counts(kThreads, 0);
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([t, kIters, &ok_counts]() {
      SrandomItem item("conc_item");
      item.init_randc();
      item.srandom(static_cast<unsigned>(7000 + t));
      for (int i = 0; i < kIters; ++i) {
        if (!item.randomize_with(item.a() >= i - 10 && item.a() <= 30 + i)) return;
        ++ok_counts[static_cast<std::size_t>(t)];
      }
    });
  }
  for (std::thread& th : threads) th.join();
  for (int t = 0; t < kThreads; ++t) {
    BOOST_REQUIRE_EQUAL(ok_counts[static_cast<std::size_t>(t)], kIters);
  }
}

BOOST_AUTO_TEST_CASE(coupled_randc_cross_object_randstate_and_canonical_signature) {
  struct CoupledRandcItem : public crv_sequence_item {
    CoupledRandcItem(crv_object_name = "coupled") {}
    crv_variable<unsigned char> v0{"v0"}, v1{"v1"}, v2{"v2"}, rc{"rc"};
    crv_constraint c{
        "c", v0() < 10u && v1() < 10u && v2() < 10u && rc() < 8 && v0() + v1() + v2() + rc() == 15u};
    void enable_randc() { rc.randc(true); }
  };

  auto rewrite_header_ids_to_canonical_range = [](std::string const& state) {
    std::istringstream iss(state);
    std::string magic;
    std::size_t var_count = 0;
    BOOST_REQUIRE(iss >> magic >> var_count);
    BOOST_REQUIRE_EQUAL(magic, "CRV_RNG_V1");
    BOOST_REQUIRE_EQUAL(var_count, 4u);
    for (std::size_t i = 0; i < var_count; ++i) {
      unsigned dummy = 0;
      BOOST_REQUIRE(iss >> dummy);
    }
    std::string rest;
    std::getline(iss, rest, '\0');
    return std::string("CRV_RNG_V1 4 0 1 2 3") + rest;
  };

  SrandomBackendGuard guard;
  for (char const* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);
    for (bool sv : {false, true}) {
      set_sv_signedness(sv);

      CoupledRandcItem src("src");
      src.enable_randc();
      src.srandom(424242u + static_cast<unsigned>(sv));

      std::set<unsigned char> cycle_vals;
      for (int i = 0; i < 4; ++i) {
        BOOST_REQUIRE(src.randomize());
        BOOST_REQUIRE(cycle_vals.insert(static_cast<unsigned char>(src.rc)).second);
      }

      RandcState src_rc;
      BOOST_REQUIRE(snapshot_randc_state(src.rc.id(), src_rc));
      BOOST_REQUIRE_NE(src_rc.signature.find("#0"), std::string::npos);
      BOOST_REQUIRE_NE(src_rc.signature.find("#1"), std::string::npos);
      BOOST_REQUIRE_NE(src_rc.signature.find("#2"), std::string::npos);
      BOOST_REQUIRE_NE(src_rc.signature.find("#3"), std::string::npos);

      std::string const mid = src.get_randstate();
      std::string const mid_low_ids = rewrite_header_ids_to_canonical_range(mid);

      std::vector<unsigned char> m1;
      for (int i = 0; i < 4; ++i) {
        BOOST_REQUIRE(src.randomize());
        unsigned char val = src.rc;
        BOOST_REQUIRE(cycle_vals.insert(val).second);
        m1.push_back(val);
      }
      BOOST_REQUIRE_EQUAL(cycle_vals.size(), 8u);

      CoupledRandcItem peer("peer");
      peer.set_randstate(mid);
      RandcState peer_rc;
      BOOST_REQUIRE(snapshot_randc_state(peer.rc.id(), peer_rc));
      BOOST_REQUIRE_EQUAL(peer_rc.signature, src_rc.signature);
      std::vector<unsigned char> m_peer;
      for (int i = 0; i < 4; ++i) {
        BOOST_REQUIRE(peer.randomize());
        m_peer.push_back(static_cast<unsigned char>(peer.rc));
      }
      BOOST_REQUIRE(m_peer == m1);

      CoupledRandcItem peer_low("peer_low");
      peer_low.set_randstate(mid_low_ids);
      RandcState peer_low_rc;
      BOOST_REQUIRE(snapshot_randc_state(peer_low.rc.id(), peer_low_rc));
      BOOST_REQUIRE_EQUAL(peer_low_rc.signature, src_rc.signature);
      std::vector<unsigned char> m_peer_low;
      for (int i = 0; i < 4; ++i) {
        BOOST_REQUIRE(peer_low.randomize());
        m_peer_low.push_back(static_cast<unsigned char>(peer_low.rc));
      }
      BOOST_REQUIRE(m_peer_low == m1);
    }
  }
}

BOOST_AUTO_TEST_CASE(copied_crv_variable_randc_set_randstate) {
  crv_variable<unsigned char> v1("v1");
  v1.randc(true);
  v1.srandom(707u);
  std::string const st = v1.get_randstate();

  crv_variable<unsigned char> v2(v1);
  BOOST_REQUIRE(v2.randc());
  v2.set_randstate(st);

  for (int i = 0; i < 6; ++i) {
    BOOST_REQUIRE(v1.randomize());
    BOOST_REQUIRE(v2.randomize());
    BOOST_REQUIRE_EQUAL(static_cast<unsigned char>(v1), static_cast<unsigned char>(v2));
  }

  v1.rand_mode(false);
  crv_variable<unsigned char> v3(v1);
  BOOST_REQUIRE(v3.randc());
  BOOST_REQUIRE(!v3.rand_mode());
  v3.set_randstate(v1.get_randstate());
}

BOOST_AUTO_TEST_CASE(get_randstate_non_perturbing_twin_randc_across_cycle_wrap) {
  struct TwinRandcItem : public crv_sequence_item {
    TwinRandcItem(crv_object_name = "twin") {}
    crv_variable<unsigned char> rc{"rc"};
    crv_variable<unsigned char> x{"x"};
    crv_constraint c{"c", rc() < 4, x() < 4};
    void enable_randc() { rc.randc(true); }
  };

  SrandomBackendGuard guard;
  for (char const* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);
    for (bool sv : {false, true}) {
      set_sv_signedness(sv);

      TwinRandcItem p1("p1"), p2("p2"), p3("p3");
      p1.enable_randc();
      p2.enable_randc();
      p3.enable_randc();

      p1.srandom(777u);
      p2.srandom(777u);

      // Warm randomize_with cache on both twins before mid-stream capture
      BOOST_REQUIRE(p1.randomize_with(p1.x() <= 3));
      BOOST_REQUIRE(p2.randomize_with(p2.x() <= 3));
      BOOST_REQUIRE_EQUAL(p1.randomize_with_cache_size(), 1u);

      // Step 1 and 2 in lockstep
      for (int i = 0; i < 2; ++i) {
        BOOST_REQUIRE(p1.randomize());
        BOOST_REQUIRE(p2.randomize());
        BOOST_REQUIRE_EQUAL(static_cast<unsigned>(p1.rc), static_cast<unsigned>(p2.rc));
        BOOST_REQUIRE_EQUAL(static_cast<unsigned>(p1.x), static_cast<unsigned>(p2.x));
      }

      // Inspect ONLY p1 mid-stream: must not invalidate cache or perturb Z3/Cudd randc trajectory
      std::string const mid_state = p1.get_randstate();
      BOOST_REQUIRE_EQUAL(p1.randomize_with_cache_size(), 1u);

      // Continue across multiple full 4-value randc cycle wraps (10 more steps = 12 total)
      std::vector<std::pair<unsigned, unsigned>> seq1, seq2, seq3;
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(p1.randomize());
        BOOST_REQUIRE(p2.randomize());
        seq1.emplace_back(static_cast<unsigned>(p1.rc), static_cast<unsigned>(p1.x));
        seq2.emplace_back(static_cast<unsigned>(p2.rc), static_cast<unsigned>(p2.x));
      }
      BOOST_REQUIRE(seq1 == seq2);

      // Replay fresh twin p3 from captured mid_state across the same cycle wraps
      p3.set_randstate(mid_state);
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(p3.randomize());
        seq3.emplace_back(static_cast<unsigned>(p3.rc), static_cast<unsigned>(p3.x));
      }
      BOOST_REQUIRE(seq3 == seq1);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()

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
#include <crave/experimental/Experimental.hpp>

#if defined(__cplusplus) && (__cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace crave;

BOOST_FIXTURE_TEST_SUITE(ExperimentalRandSequence_t, Context_Fixture)

BOOST_AUTO_TEST_CASE(named_value_returning_and_parameterized_productions) {
  crv_randsequence rs("rs");
  BOOST_REQUIRE_EQUAL(rs.obj_kind(), "crv_randsequence");

  auto main_p = rs.production("main");
  auto add_p = rs.production<int(int, int)>("add");
  auto tag_p = rs.production<std::string()>("tag");

  BOOST_REQUIRE(rs.has_production("main"));
  BOOST_REQUIRE(rs.has_production("add"));
  BOOST_REQUIRE(!rs.has_production("missing"));
  BOOST_REQUIRE_THROW(rs.production<int()>("main"), std::logic_error);

  add_p.rule([&](int a, int b) { add_p.do_return(a + b); });
  tag_p.rule([&]() { tag_p.do_return("TAG_OK"); });

  int sum_out = 0;
  std::string tag_out;
  main_p.rule([&]() {
    sum_out = add_p(7, 15);
    tag_out = tag_p();
  });

  rs.srandom(42u);
  BOOST_REQUIRE(rs.execute());
  BOOST_REQUIRE_EQUAL(sum_out, 22);
  BOOST_REQUIRE_EQUAL(tag_out, "TAG_OK");

  // Direct typed run() and set_entry()
  BOOST_REQUIRE_EQUAL(rs.run(add_p, 10, 32), 42);
  int alt_ran = 0;
  auto alt_p = rs.production("alt");
  alt_p.rule([&]() { ++alt_ran; });
  rs.set_entry(alt_p);
  rs.run();
  BOOST_REQUIRE_EQUAL(alt_ran, 1);
}

BOOST_AUTO_TEST_CASE(weighted_rules_zero_exclusion_and_all_zero) {
  crv_randsequence rs("w_rs");
  rs.srandom(101u);

  int c0 = 0, c1 = 0, c2 = 0, dyn_w = 5;
  auto main_p = rs.production("main");
  main_p.rule(0, [&]() { ++c0; });
  main_p.rule(1, [&]() { ++c1; });
  main_p.rule(rs_weight([&]() { return dyn_w; }), [&]() { ++c2; });

  for (int i = 0; i < 600; ++i) rs.run();
  BOOST_REQUIRE_EQUAL(c0, 0);
  BOOST_REQUIRE_GT(c1, 40);
  BOOST_REQUIRE_GT(c2, c1 * 3);

  dyn_w = 0;
  int prev_c2 = c2;
  for (int i = 0; i < 50; ++i) rs.run();
  BOOST_REQUIRE_EQUAL(c2, prev_c2);

  // All-zero weights: safe no-op without crash or division by zero
  crv_randsequence zero_rs("z_rs");
  int zhit = 0;
  auto zp = zero_rs.production("zp");
  auto zret = zero_rs.production<int()>("zret");
  zp.rule(0, [&]() { ++zhit; }).rule(0, [&]() { ++zhit; });
  zret.rule(0, [&]() { zret.do_return(99); });
  BOOST_REQUIRE(zero_rs.execute());
  BOOST_REQUIRE_EQUAL(zhit, 0);
  BOOST_REQUIRE_EQUAL(zero_rs.run(zret), 0);
}

BOOST_AUTO_TEST_CASE(repeat_if_and_case_combinators) {
  crv_randsequence rs("ctrl_rs");
  rs.srandom(202u);

  int rep_fixed = 0;
  std::vector<int> rep_bounds;
  int cond_val = 0;
  int if_then_cnt = 0, if_else_cnt = 0;
  int case_key = 0;
  std::vector<std::string> case_hits;

  auto main_p = rs.production("main");
  main_p.rule(
      rs_repeat(3, [&]() { ++rep_fixed; }),
      [&]() {
        int local_cnt = 0;
        rs_repeat(2, 5, [&]() { ++local_cnt; }).execute(*detail::active_rs_exec_state());
        rep_bounds.push_back(local_cnt);
      },
      rs_if([&]() { return cond_val > 0; }, [&]() { ++if_then_cnt; }, [&]() { ++if_else_cnt; }),
      rs_case([&]() { return case_key; })
          .when(1, [&]() { case_hits.push_back("one"); })
          .when({2, 3}, [&]() { case_hits.push_back("two_three"); })
          .otherwise([&]() { case_hits.push_back("default"); }));

  cond_val = 0;
  case_key = 1;
  rs.run();
  cond_val = 1;
  case_key = 3;
  rs.run();
  case_key = 99;
  rs.run();

  BOOST_REQUIRE_EQUAL(rep_fixed, 9);
  BOOST_REQUIRE_EQUAL(rep_bounds.size(), 3u);
  for (int c : rep_bounds) {
    BOOST_REQUIRE(c >= 2 && c <= 5);
  }
  BOOST_REQUIRE_EQUAL(if_then_cnt, 2);
  BOOST_REQUIRE_EQUAL(if_else_cnt, 1);
  BOOST_REQUIRE_EQUAL(case_hits.size(), 3u);
  BOOST_REQUIRE_EQUAL(case_hits[0], "one");
  BOOST_REQUIRE_EQUAL(case_hits[1], "two_three");
  BOOST_REQUIRE_EQUAL(case_hits[2], "default");

  // Functional rs_case + rs_repeat(min, max)
  crv_randsequence rs2("ctrl_rs2");
  rs2.srandom(303u);
  int rmin_max_cnt = 0;
  int k2 = 2;
  std::string k2_res;
  auto p2 = rs2.production("p2");
  p2.rule(rs_repeat(2, 4, [&]() { ++rmin_max_cnt; }),
          rs_case([&]() { return k2; }, rs_when(1, [&]() { k2_res = "A"; }),
                  rs_when({2, 3}, [&]() { k2_res = "BC"; }), rs_default([&]() { k2_res = "DEF"; })));
  rs2.run();
  BOOST_REQUIRE(rmin_max_cnt >= 2 && rmin_max_cnt <= 4);
  BOOST_REQUIRE_EQUAL(k2_res, "BC");
}

BOOST_AUTO_TEST_CASE(rand_join_interleaving_and_weights) {
  BOOST_REQUIRE_THROW(rs_join(-0.1, []() {}), std::invalid_argument);
  BOOST_REQUIRE_THROW(rs_join(1.1, []() {}), std::invalid_argument);

  auto run_join_bias = [](double w, unsigned seed) {
    crv_randsequence rs("join_rs");
    rs.srandom(seed);
    std::string trace;
    auto a = rs.production("a");
    auto b = rs.production("b");
    auto c = rs.production("c");
    a.rule([&]() { trace += "A1,"; }, [&]() { trace += "A2,"; }, [&]() { trace += "A3,"; });
    b.rule([&]() { trace += "B1,"; }, [&]() { trace += "B2,"; }, [&]() { trace += "B3,"; });
    c.rule([&]() { trace += "C1,"; }, [&]() { trace += "C2,"; });

    auto main_p = rs.production("main");
    main_p.rule(rs_join(w, a, b, c));
    rs.set_entry(main_p);

    int contiguous_runs = 0;
    int interleaved_runs = 0;
    std::map<std::string, int> two_way_patterns;
    for (int i = 0; i < 600; ++i) {
      trace.clear();
      rs.run();
      // Strictly verify relative intra-branch order
      auto pos_a1 = trace.find("A1"), pos_a2 = trace.find("A2"), pos_a3 = trace.find("A3");
      auto pos_b1 = trace.find("B1"), pos_b2 = trace.find("B2"), pos_b3 = trace.find("B3");
      auto pos_c1 = trace.find("C1"), pos_c2 = trace.find("C2");
      BOOST_TEST_INFO("w=" << w << " i=" << i << " trace=" << trace);
      BOOST_REQUIRE(pos_a1 != std::string::npos && pos_a1 < pos_a2 && pos_a2 < pos_a3);
      BOOST_TEST_INFO("w=" << w << " i=" << i << " trace=" << trace);
      BOOST_REQUIRE(pos_b1 != std::string::npos && pos_b1 < pos_b2 && pos_b2 < pos_b3);
      BOOST_TEST_INFO("w=" << w << " i=" << i << " trace=" << trace);
      BOOST_REQUIRE(pos_c1 != std::string::npos && pos_c1 < pos_c2);
      if (trace.find("C1,C2,") != std::string::npos) ++contiguous_runs;
      if (pos_a1 < pos_b1 && pos_b1 < pos_a2) ++interleaved_runs;
      ++two_way_patterns[trace];
    }
    return std::make_tuple(contiguous_runs, interleaved_runs, two_way_patterns.size());
  };

  auto [contig_0, inter_0, uniq_0] = run_join_bias(0.0, 11u);
  auto [contig_half, inter_half, uniq_half] = run_join_bias(0.5, 11u);
  auto [contig_1, inter_1, uniq_1] = run_join_bias(1.0, 11u);
  (void)uniq_0;
  (void)uniq_1;

  // w=0.0 favors contiguous shortest-remaining (C1,C2 together)
  BOOST_REQUIRE_GT(contig_0, contig_1);
  // w=0.5 explores diverse interleavings
  BOOST_REQUIRE_GT(uniq_half, 20u);
  // w=1.0 maximizes interleaving
  BOOST_REQUIRE_GT(inter_1, inter_0);
  BOOST_REQUIRE_GE(inter_half, 10);
}

BOOST_AUTO_TEST_CASE(break_and_return_semantics) {
  crv_randsequence rs("abort_rs");
  rs.srandom(404u);

  std::vector<std::string> log;
  auto inner_ret = rs.production("inner_ret");
  inner_ret.rule(
      [&]() { log.push_back("ir1"); },
      [&]() { rs.do_return(); },
      [&]() { log.push_back("ir_unreachable"); });

  auto main_ret = rs.production("main_ret");
  main_ret.rule(
      [&]() { log.push_back("before"); },
      inner_ret,
      [&]() { log.push_back("after"); });

  log.clear();
  rs.set_entry(main_ret);
  BOOST_REQUIRE(rs.execute());
  BOOST_REQUIRE((log == std::vector<std::string>{"before", "ir1", "after"}));

  // do_return inside one branch of rs_join stops only that branch
  std::vector<std::string> join_log;
  auto jb1 = rs.production("jb1");
  auto jb2 = rs.production("jb2");
  jb1.rule([&]() { join_log.push_back("jb1_1"); }, [&]() { rs.do_return(); },
           [&]() { join_log.push_back("jb1_unreach"); });
  jb2.rule([&]() { join_log.push_back("jb2_1"); }, [&]() { join_log.push_back("jb2_2"); });
  auto join_parent = rs.production("join_parent");
  join_parent.rule(rs_join(jb1, jb2), [&]() { join_log.push_back("parent_after_join"); });
  rs.set_entry(join_parent);
  BOOST_REQUIRE(rs.execute());
  BOOST_REQUIRE(std::find(join_log.begin(), join_log.end(), "jb1_1") != join_log.end());
  BOOST_REQUIRE(std::find(join_log.begin(), join_log.end(), "jb1_unreach") == join_log.end());
  BOOST_REQUIRE(std::find(join_log.begin(), join_log.end(), "jb2_1") != join_log.end());
  BOOST_REQUIRE(std::find(join_log.begin(), join_log.end(), "jb2_2") != join_log.end());
  BOOST_REQUIRE_EQUAL(join_log.back(), "parent_after_join");

  // do_break aborts the entire enclosing randsequence
  std::vector<std::string> brk_log;
  auto inner_brk = rs.production("inner_brk");
  inner_brk.rule([&]() { brk_log.push_back("ib1"); }, [&]() { rs.do_break(); },
                 [&]() { brk_log.push_back("ib_unreach"); });
  auto outer_brk = rs.production("outer_brk");
  outer_brk.rule([&]() { brk_log.push_back("outer_start"); }, inner_brk,
                 [&]() { brk_log.push_back("outer_unreach"); });
  rs.set_entry(outer_brk);
  BOOST_REQUIRE(!rs.execute());
  BOOST_REQUIRE((brk_log == std::vector<std::string>{"outer_start", "ib1"}));
}

BOOST_AUTO_TEST_CASE(recursive_productions_and_per_object_rng) {
  // Recursive binary expression tree generation with bounded depth
  crv_randsequence tree_rs("tree");
  auto expr_p = tree_rs.production<std::string(int)>("expr");
  expr_p.rule(2, [&](int depth) {
    if (depth >= 3) {
      expr_p.do_return("x");
      return;
    }
    std::string lhs = expr_p(depth + 1);
    std::string rhs = expr_p(depth + 1);
    expr_p.do_return("(" + lhs + "+" + rhs + ")");
  });
  expr_p.rule(1, [&](int) { expr_p.do_return("n"); });

  tree_rs.srandom(909u);
  std::string const t1 = tree_rs.run(expr_p, 0);
  tree_rs.srandom(909u);
  std::string const t2 = tree_rs.run(expr_p, 0);
  BOOST_REQUIRE(!t1.empty());
  BOOST_REQUIRE_EQUAL(t1, t2);

  // Per-object srandom, randstate, and crv_sequence_item hierarchy integration
  struct ParentSeqItem : public crv_sequence_item {
    ParentSeqItem(crv_object_name = "pseq") {}
    crv_variable<unsigned> len{"len"};
    crv_constraint c{"c", len() >= 1u && len() <= 10u};
    crv_randsequence seq{"seq"};
    std::vector<int> trace;

    void setup() {
      auto root_p = seq.production("root");
      root_p.rule(2, [&]() { trace.push_back(1); })
          .rule(3, [&]() { trace.push_back(2); })
          .rule(1, [&]() { trace.push_back(3); });
    }
  };

  ParentSeqItem p1("p1"), p2("p2"), distractor("distractor");
  p1.setup();
  p2.setup();
  distractor.setup();

  p1.srandom(5555u);
  p2.srandom(5555u);
  distractor.srandom(9999u);

  std::string const p1_st = p1.get_randstate();
  std::vector<unsigned> lens1, lens2;
  for (int i = 0; i < 15; ++i) {
    BOOST_REQUIRE(p1.randomize());
    p1.seq.run();
    lens1.push_back(p1.len);

    // Interleave distractor calls
    BOOST_REQUIRE(distractor.randomize());
    distractor.seq.run();

    BOOST_REQUIRE(p2.randomize());
    p2.seq.run();
    lens2.push_back(p2.len);
  }
  BOOST_REQUIRE(lens1 == lens2);
  BOOST_REQUIRE(p1.trace == p2.trace);

  // Replay p1 from saved state
  std::vector<int> saved_trace = p1.trace;
  p1.trace.clear();
  p1.set_randstate(p1_st);
  std::vector<unsigned> lens_replay;
  for (int i = 0; i < 15; ++i) {
    BOOST_REQUIRE(p1.randomize());
    p1.seq.run();
    lens_replay.push_back(p1.len);
  }
  BOOST_REQUIRE(lens_replay == lens1);
  BOOST_REQUIRE(p1.trace == saved_trace);
}

BOOST_AUTO_TEST_CASE(do_return_conversions_nan_join_and_large_weights) {
  enum class RsColor { Red, Green, Blue };

  crv_randsequence rs("conv_rs");
  rs.srandom(2468u);

  auto p_i64 = rs.production<std::int64_t()>("p_i64");
  p_i64.rule([&]() { rs.do_return(42); });
  BOOST_REQUIRE_EQUAL(rs.run(p_i64), static_cast<std::int64_t>(42));

  auto p_u = rs.production<unsigned()>("p_u");
  p_u.rule([&]() { rs.do_return(42); });
  BOOST_REQUIRE_EQUAL(rs.run(p_u), 42u);

  auto p_dbl = rs.production<double()>("p_dbl");
  p_dbl.rule([&]() { rs.do_return(42); });
  BOOST_REQUIRE_CLOSE(rs.run(p_dbl), 42.0, 1e-9);

  auto p_str = rs.production<std::string()>("p_str");
  p_str.rule([&]() { rs.do_return("hello"); });
  BOOST_REQUIRE_EQUAL(rs.run(p_str), "hello");

  auto p_bool = rs.production<bool()>("p_bool");
  p_bool.rule([&]() { rs.do_return(true); });
  BOOST_REQUIRE_EQUAL(rs.run(p_bool), true);

  auto p_enum = rs.production<RsColor()>("p_enum");
  p_enum.rule([&]() { rs.do_return(RsColor::Green); });
  BOOST_REQUIRE(rs.run(p_enum) == RsColor::Green);

  // Incompatible do_return types must throw std::logic_error
  auto p_bad_int = rs.production<int()>("p_bad_int");
  p_bad_int.rule([&]() { rs.do_return("bad"); });
  BOOST_REQUIRE_THROW(rs.run(p_bad_int), std::logic_error);

  auto p_bad_str = rs.production<std::string()>("p_bad_str");
  p_bad_str.rule([&]() { rs.do_return(42); });
  BOOST_REQUIRE_THROW(rs.run(p_bad_str), std::logic_error);

  auto p_bad_void = rs.production<void()>("p_bad_void");
  p_bad_void.rule([&]() { rs.do_return(42); });
  BOOST_REQUIRE_THROW(rs.run(p_bad_void), std::logic_error);

  // NaN rs_join weight must throw std::invalid_argument
  BOOST_REQUIRE_THROW(rs_join(std::numeric_limits<double>::quiet_NaN(), []() {}), std::invalid_argument);

  // Large weights near UINT64_MAX must not wrap around
  int hit_a = 0, hit_b = 0;
  auto p_huge = rs.production("p_huge");
  p_huge.rule(std::numeric_limits<std::uint64_t>::max(), [&]() { ++hit_a; })
      .rule(std::numeric_limits<std::uint64_t>::max(), [&]() { ++hit_b; });
  rs.set_entry(p_huge);
  for (int i = 0; i < 200; ++i) rs.run();
  BOOST_REQUIRE_GT(hit_a, 50);
  BOOST_REQUIRE_GT(hit_b, 50);
}

BOOST_AUTO_TEST_CASE(production_handle_do_return_frame_mismatch_and_null_cstr) {
  crv_randsequence rs("mismatch_rs");
  rs.srandom(1234u);

  auto p_int = rs.production<int()>("p_int");
  auto p_str = rs.production<std::string()>("p_str");
  auto p_void = rs.production<void()>("p_void");

  // Calling p_int.do_return(42) while executing inside p_void must throw std::logic_error
  // and must NOT mark p_void's frame as returned.
  bool after_mismatch_ran = false;
  bool void_caught = false;
  p_void.rule([&]() {
    try {
      p_int.do_return(42);
    } catch (std::logic_error const&) {
      void_caught = true;
    }
    after_mismatch_ran = true;
  });
  rs.set_entry(p_void);
  BOOST_REQUIRE(rs.execute());
  BOOST_REQUIRE(void_caught);
  BOOST_REQUIRE(after_mismatch_ran);

  // Calling p_int.do_return(42) while executing inside p_str must throw std::logic_error
  // and allow p_str to continue returning its own string normally.
  bool str_caught = false;
  p_str.rule([&]() {
    try {
      p_int.do_return(42);
    } catch (std::logic_error const&) {
      str_caught = true;
    }
    p_str.do_return("ok_after_mismatch");
  });
  BOOST_REQUIRE_EQUAL(rs.run(p_str), "ok_after_mismatch");
  BOOST_REQUIRE(str_caught);

  // Calling p_void.do_return() inside p_int must throw std::logic_error.
  bool int_caught = false;
  p_int.rule([&]() {
    try {
      p_void.do_return();
    } catch (std::logic_error const&) {
      int_caught = true;
    }
    p_int.do_return(77);
  });
  BOOST_REQUIRE_EQUAL(rs.run(p_int), 77);
  BOOST_REQUIRE(int_caught);

  // Null char const* passed to production_handle<std::string()>::do_return must throw std::logic_error.
  auto p_null_str = rs.production<std::string()>("p_null_str");
  p_null_str.rule([&]() {
    char const* null_p = nullptr;
    p_null_str.do_return(null_p);
  });
  BOOST_REQUIRE_THROW(rs.run(p_null_str), std::logic_error);
}

BOOST_AUTO_TEST_SUITE_END()

#endif  // C++17

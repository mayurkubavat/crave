#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace crave;

namespace {

struct CacheTestGuard {
  SolverTypes prev_solver;
  bool prev_sv_signed;
  bool prev_bypass;
  CacheTestGuard()
      : prev_solver(FactoryMetaSMT::solver_type_),
        prev_sv_signed(sv_signedness()),
        prev_bypass(VariableDefaultSolver::bypass_constraint_analysis) {}
  ~CacheTestGuard() {
    FactoryMetaSMT::solver_type_ = prev_solver;
    set_sv_signedness(prev_sv_signed);
    VariableDefaultSolver::bypass_constraint_analysis = prev_bypass;
  }
};

struct BasicCacheItem : public crv_sequence_item {
  BasicCacheItem(crv_object_name) {}
  crv_variable<unsigned> x;
  crv_variable<unsigned> y;
  crv_variable<int> sx;
  crv_constraint c_bound{"c_bound", x() < 100u, y() < 100u, sx() >= -50 && sx() <= 50};
};

struct PreRandMutateItem : public crv_sequence_item {
  PreRandMutateItem(crv_object_name) {}
  crv_variable<unsigned> x;
  crv_constraint c_bound{"c_bound", x() < 100u};
  unsigned pre_target = 1u;
  unsigned step = 0u;
  void pre_randomize() override {
    ++step;
    pre_target = (step * 7u) % 50u;
  }
};

struct SoftCacheItem : public crv_sequence_item {
  SoftCacheItem(crv_object_name) {}
  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_constraint c_bound{"c_bound", a() < 50u, b() < 50u};
  crv_soft_constraint s_a{"s_a", a() == 10u};
  crv_soft_constraint s_b{"s_b", b() == 20u};
};

struct ComplexCacheItem : public crv_sequence_item {
  ComplexCacheItem(crv_object_name) {}
  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_variable<unsigned> c;
  crv_vector<unsigned> vec;
  crv_array<unsigned, 4> arr;

  crv_constraint c_vars{"c_vars", a() < 32u, b() < 32u, c() < 32u};
  crv_constraint c_vec_sz{"c_vec_sz", vec().size() == 4u};
  crv_constraint c_arr{"c_arr", foreach(arr(), arr()[_i] < 16u)};
  crv_solve_before sb{"sb", a, b};
};

struct RandcCacheItem : public crv_sequence_item {
  RandcCacheItem(crv_object_name) {}
  crv_variable<uint8_t> x;
  crv_variable<unsigned> y;
  crv_constraint c_range{"c_range", x() < 8, y() < 16u};
};

struct GoalItem : public crv_sequence_item {
  GoalItem(crv_object_name) {}
  crv_variable<unsigned> x;
  crv_variable<unsigned> y;
  crv_constraint c_range{"c_range", x() < 16u, y() < 16u};
};

struct GoalToggleItem : public crv_sequence_item {
  GoalToggleItem(crv_object_name) {}
  crv_variable<unsigned> x;
  crv_variable<unsigned> y;
  crv_variable<unsigned> z;
  crv_constraint c_range{"c_range", x() < 16u, y() < 16u, z() < 16u};
  crv_constraint c_extra{"c_extra", y() >= 1u};
};

struct GoalCG : public crv_covergroup {
  crv_variable<unsigned> v;
  crv_coverpoint cp{"cp"};
  GoalCG(crv_object_name) {
    cp.simple_bins() = {crv_bin(v() == 2u), crv_bin(v() == 7u), crv_bin(v() == 13u)};
  }
};

struct GoalCGTwo : public crv_covergroup {
  crv_variable<unsigned> v;
  crv_coverpoint cp{"cp"};
  GoalCGTwo(crv_object_name) {
    cp.simple_bins() = {crv_bin(v() == 3u), crv_bin(v() == 11u)};
  }
};

struct SoftVectorCacheItem : public crv_sequence_item {
  SoftVectorCacheItem(crv_object_name) {}
  crv_variable<unsigned> x;
  crv_variable<unsigned> s0;
  crv_variable<unsigned> sz;
  crv_vector<unsigned> v0;
  crv_constraint c_range{"c_range", x() < 16u, s0() == 5u, sz() >= 2u && sz() <= 3u};
  crv_constraint c_v_sz{"c_v_sz", v0().size() == sz()};
  crv_constraint c_v_elem{"c_v_elem", foreach(v0(), v0()[_i] >= 1u && v0()[_i] <= 2u)};
  crv_constraint c_v_sum{"c_v_sum", v0().sum() == s0()};
  crv_soft_constraint s_sz{"s_sz", sz() == 2u};
  crv_soft_constraint s_x{"s_x", x() == 3u};
};

struct SoftVectorCG : public crv_covergroup {
  crv_variable<unsigned> v;
  crv_coverpoint cp{"cp"};
  SoftVectorCG(crv_object_name) { cp.simple_bins() = {crv_bin(v() == 7u)}; }
};

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void leave_dead_read_refs_in_global_container() {
  SoftCacheItem tmp("dead_rm");
  tmp.a.rand_mode(false);
  tmp.a = 10u;
  BOOST_REQUIRE(tmp.randomize());

  Variable<unsigned> lv;
  unsigned local_bound = 5u;
  Generator lgen;
  lgen(lv <= reference(local_bound));
  BOOST_REQUIRE(lgen.next());
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void stack_poison() {
  volatile unsigned char buf[4096];
  for (std::size_t i = 0; i < sizeof(buf); ++i) {
    buf[i] = static_cast<unsigned char>(0xA5u ^ (i & 0xFFu));
  }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void run_scoped_cg_randomize(GoalItem& item) {
  GoalCG cg("scoped_r_cg");
  cg.v.bind(item.x);
  item.goal(cg);
  for (int i = 0; i < 3; ++i) {
    BOOST_REQUIRE(item.randomize());
    cg.sample();
  }
  BOOST_REQUIRE(cg.covered());
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void run_scoped_cg_randomize_with(GoalItem& item) {
  GoalCG cg("scoped_rw_cg");
  cg.v.bind(item.x);
  item.goal(cg);
  std::size_t const hits_before = item.randomize_with_cache_hits();
  std::size_t const misses_before = item.randomize_with_cache_misses();
  for (int i = 0; i < 3; ++i) {
    BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
    cg.sample();
  }
  BOOST_REQUIRE(cg.covered());
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), misses_before + 1u);
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_hits(), hits_before + 2u);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void run_scoped_cg_mixed(GoalItem& item) {
  GoalCGTwo cg("scoped_mix_cg");
  cg.v.bind(item.x);
  item.goal(cg);
  BOOST_REQUIRE(item.randomize());
  cg.sample();
  BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
  cg.sample();
  BOOST_REQUIRE(cg.covered());
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void run_scoped_cg_two_bin_rw(GoalItem& item) {
  GoalCGTwo cg1("scoped_cg1");
  cg1.v.bind(item.x);
  item.goal(cg1);
  for (int i = 0; i < 2; ++i) {
    BOOST_REQUIRE(item.randomize_with(item.x() == 3u || item.x() == 11u));
    cg1.sample();
  }
  BOOST_REQUIRE(cg1.covered());
}

struct OverlapCG : public crv_covergroup {
  crv_variable<unsigned> v;
  crv_coverpoint cp{"cp"};
  OverlapCG(crv_object_name) {
    cp.simple_bins() = {crv_bin(v() == 2u), crv_bin(v() < 4u)};
  }
};

struct InnerSharedCG : public crv_covergroup {
  crv_variable<unsigned> v;
  crv_coverpoint cp{"cp"};
  InnerSharedCG(crv_object_name) {
    cp.simple_bins() = {crv_bin(v() == 1u), crv_bin(v() == 2u)};
  }
};

struct SoftGoalItem : public crv_sequence_item {
  SoftGoalItem(crv_object_name) {}
  crv_variable<unsigned> x;
  crv_variable<unsigned> y;
  crv_constraint c_range{"c_range", x() < 16u, y() < 16u};
  crv_soft_constraint s_x{"s_x", x() == 5u};
};

struct SubItemCgAfter : public crv_sequence_item {
  crv_variable<unsigned> x;
  GoalCG cg{"sub_cg_after"};
  crv_constraint c_range{"c_range", x() < 16u};
  SubItemCgAfter(crv_object_name) {
    cg.v.bind(x);
    goal(cg);
  }
};

struct SubItemCgBefore : public crv_sequence_item {
  GoalCG cg{"sub_cg_before"};
  crv_variable<unsigned> x;
  crv_constraint c_range{"c_range", x() < 16u};
  SubItemCgBefore(crv_object_name) {
    cg.v.bind(x);
    goal(cg);
  }
};

struct WrapperItemFirst {
  GoalItem item{"wrap_item_first"};
  GoalCG cg{"wrap_cg_second"};
  WrapperItemFirst() {
    cg.v.bind(item.x);
    item.goal(cg);
  }
};

struct WrapperCgFirst {
  GoalCG cg{"wrap_cg_first"};
  GoalItem item{"wrap_item_second"};
  WrapperCgFirst() {
    cg.v.bind(item.x);
    item.goal(cg);
  }
};

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void run_overlap_cg(SoftGoalItem& item) {
  OverlapCG cg("overlap_cg");
  cg.v.bind(item.x);
  item.goal(cg);
  for (int i = 0; i < 2 && !cg.covered(); ++i) {
    BOOST_REQUIRE(item.randomize());
    cg.sample();
  }
  BOOST_REQUIRE(cg.covered());
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void run_inner_shared_cg(GoalItem& item) {
  InnerSharedCG cg_inner("inner_shared_cg");
  cg_inner.v.bind(item.x);
  item.goal(cg_inner);
  for (int i = 0; i < 3 && !cg_inner.covered(); ++i) {
    BOOST_REQUIRE(item.randomize_with(item.x() == 1u || item.x() == 2u));
    cg_inner.sample();
  }
  BOOST_REQUIRE(cg_inner.covered());
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void run_unsampled_cg(SoftGoalItem& item, bool run_solves) {
  GoalCG cg("unsampled_cg");
  cg.v.bind(item.x);
  item.goal(cg);
  if (run_solves) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
  }
  BOOST_REQUIRE(!cg.covered());
}

struct WideItem : public crv_sequence_item {
  WideItem(crv_object_name) {}
  crv_variable<unsigned> x;
  crv_variable<unsigned> y;
  crv_constraint c_range{"c_range", x() < 1000000u, y() < 16u};
};

struct ThreeBinCG : public crv_covergroup {
  crv_variable<unsigned> v;
  crv_coverpoint cp{"cp"};
  ThreeBinCG(crv_object_name) {
    cp.simple_bins() = {crv_bin(v() == 10u), crv_bin(v() == 20u), crv_bin(v() == 30u)};
  }
};

struct WideInnerSharedCG : public crv_covergroup {
  crv_variable<unsigned> v;
  crv_coverpoint cp{"cp"};
  WideInnerSharedCG(crv_object_name) {
    cp.simple_bins() = {crv_bin(v() == 10u), crv_bin(v() == 20u)};
  }
};

struct MultiHitCG : public crv_covergroup {
  crv_variable<unsigned> v;
  crv_coverpoint cp{"cp"};
  MultiHitCG(crv_object_name) {
    cp.simple_bins() = {crv_bin(v() == 2u, 2)};
  }
};

struct TwoBinOuterCG : public crv_covergroup {
  crv_variable<unsigned> v;
  crv_coverpoint cp{"cp"};
  TwoBinOuterCG(crv_object_name) {
    cp.simple_bins() = {crv_bin(v() == 2u), crv_bin(v() == 9u)};
  }
};

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void run_wide_inner_shared_rw(WideItem& item) {
  WideInnerSharedCG cg_inner("wide_inner_rw");
  cg_inner.v.bind(item.x);
  item.goal(cg_inner);
  for (int i = 0; i < 6 && !cg_inner.covered(); ++i) {
    BOOST_REQUIRE(item.randomize_with(item.x() == 10u || item.x() == 20u));
    cg_inner.sample();
  }
  BOOST_REQUIRE(cg_inner.covered());
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void run_wide_inner_shared_randomize(WideItem& item) {
  WideInnerSharedCG cg_inner("wide_inner_r");
  cg_inner.v.bind(item.x);
  item.goal(cg_inner);
  for (int i = 0; i < 8 && !cg_inner.covered(); ++i) {
    BOOST_REQUIRE(item.randomize());
    cg_inner.sample();
  }
  BOOST_REQUIRE(cg_inner.covered());
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void run_soft_inner_shared_randomize(SoftGoalItem& item) {
  InnerSharedCG cg_inner("soft_inner_r");
  cg_inner.v.bind(item.x);
  item.goal(cg_inner);
  for (int i = 0; i < 8 && !cg_inner.covered(); ++i) {
    BOOST_REQUIRE(item.randomize());
    cg_inner.sample();
  }
  BOOST_REQUIRE(cg_inner.covered());
}

}  // namespace

BOOST_FIXTURE_TEST_SUITE(RandomizeWithSolverCache_t, Context_Fixture)

BOOST_AUTO_TEST_CASE(zero_container_growth_and_cache_hits_tight_loop) {
  CacheTestGuard guard;
  set_solver_backend("Z3");
  BasicCacheItem item("item");

  // Warm up permanent member variables once
  BOOST_REQUIRE(item.randomize());

  std::size_t const r0 = variable_container()->read_references.size();
  std::size_t const v0 = variable_container()->variables.size();
  std::size_t const d0 = variable_container()->dist_references.size();
  std::size_t const ud0 = variable_container()->user_dist_references.size();
  std::size_t const id0 = variable_container()->implicit_dist_references.size();
  std::size_t const dm0 = variable_container()->dist_ref_to_var_map.size();

  VariableDefaultSolver::reset_solver_build_count();
  VariableDefaultSolver::reset_analyse_hards_count();

  constexpr unsigned kIters = 2500;
  for (unsigned i = 0; i < kIters; ++i) {
    unsigned local_target = i % 100u;
    BOOST_REQUIRE(item.randomize_with(item.x() == reference(local_target)));
    BOOST_REQUIRE_EQUAL(item.x, local_target);
  }

  BOOST_REQUIRE_EQUAL(variable_container()->read_references.size(), r0);
  BOOST_REQUIRE_EQUAL(variable_container()->variables.size(), v0);
  BOOST_REQUIRE_EQUAL(variable_container()->dist_references.size(), d0);
  BOOST_REQUIRE_EQUAL(variable_container()->user_dist_references.size(), ud0);
  BOOST_REQUIRE_EQUAL(variable_container()->implicit_dist_references.size(), id0);
  BOOST_REQUIRE_EQUAL(variable_container()->dist_ref_to_var_map.size(), dm0);

  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 1u);
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_hits(), kIters - 1u);
  BOOST_REQUIRE_EQUAL(VariableDefaultSolver::solver_build_count(), 3u);
  BOOST_REQUIRE_EQUAL(VariableDefaultSolver::analyse_hards_count(), 0u);

  // Also verify inline dist loop does not grow global variable_container()
  for (unsigned i = 0; i < 200; ++i) {
    BOOST_REQUIRE(item.randomize_with(dist(item.y(), make_distribution(weighted_range<unsigned>(10u, 12u, 5)))));
    BOOST_REQUIRE(item.y >= 10u && item.y <= 12u);
  }
  BOOST_REQUIRE_EQUAL(variable_container()->read_references.size(), r0);
  BOOST_REQUIRE_EQUAL(variable_container()->variables.size(), v0);
  BOOST_REQUIRE_EQUAL(variable_container()->dist_references.size(), d0);
  BOOST_REQUIRE_EQUAL(variable_container()->user_dist_references.size(), ud0);
  BOOST_REQUIRE_EQUAL(variable_container()->implicit_dist_references.size(), id0);
  BOOST_REQUIRE_EQUAL(variable_container()->dist_ref_to_var_map.size(), dm0);
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 2u);
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_hits(), (kIters - 1u) + 199u);
}

BOOST_AUTO_TEST_CASE(cache_key_separation_and_lru_eviction) {
  CacheTestGuard guard;
  BasicCacheItem item("item");

  set_solver_backend("Z3");
  set_sv_signedness(false);

  // Distinct shapes / literals / types / soft / dist weights
  BOOST_REQUIRE(item.randomize_with(item.x() < 10u));
  BOOST_REQUIRE(item.randomize_with(item.x() < 10u));
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 1u);
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_hits(), 1u);

  // Different constant -> miss
  BOOST_REQUIRE(item.randomize_with(item.x() < 11u));
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 2u);

  // Soft vs hard -> miss
  BOOST_REQUIRE(item.randomize_with(soft(item.x() < 10u)));
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 3u);

  // Different reference width/signedness -> miss
  unsigned uval = 5u;
  int ival = 5;
  short sval = 5;
  BOOST_REQUIRE(item.randomize_with(item.sx() == reference(uval)));
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 4u);
  BOOST_REQUIRE(item.randomize_with(item.sx() == reference(ival)));
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 5u);
  BOOST_REQUIRE(item.randomize_with(item.sx() == reference(sval)));
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 6u);

  // Same reference type -> hit
  int ival2 = -7;
  BOOST_REQUIRE(item.randomize_with(item.sx() == reference(ival2)));
  BOOST_REQUIRE_EQUAL(item.sx, -7);
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_hits(), 2u);

  // Different dist weights -> miss
  BOOST_REQUIRE(item.randomize_with(dist(item.x(), make_distribution(weighted_range<unsigned>(1u, 2u, 10)))));
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 7u);
  BOOST_REQUIRE(item.randomize_with(dist(item.x(), make_distribution(weighted_range<unsigned>(1u, 2u, 99)))));
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 8u);

  // sv_signedness switch -> miss, then hit
  set_sv_signedness(true);
  BOOST_REQUIRE(item.randomize_with(item.x() < 10u));
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 9u);
  BOOST_REQUIRE(item.randomize_with(item.x() < 10u));
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_hits(), 3u);

  // bypass_constraint_analysis toggle -> miss
  VariableDefaultSolver::bypass_constraint_analysis = true;
  BOOST_REQUIRE(item.randomize_with(item.x() < 10u));
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 10u);
  VariableDefaultSolver::bypass_constraint_analysis = false;

  // Backend switch Z3 -> Cudd -> Z3
  set_solver_backend("Cudd");
  BOOST_REQUIRE(item.randomize_with(item.x() < 10u));
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 11u);
  BOOST_REQUIRE(item.randomize_with(item.x() < 10u));
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_hits(), 4u);
  set_solver_backend("Z3");
  BOOST_REQUIRE(item.randomize_with(item.x() < 10u));
  BOOST_REQUIRE_EQUAL(item.randomize_with_cache_hits(), 5u);

  // Bounded LRU eviction (kMaxEntries = 64)
  BasicCacheItem lru_item("lru_item");
  for (unsigned k = 0; k < 65; ++k) {
    BOOST_REQUIRE(lru_item.randomize_with(lru_item.x() == k));
  }
  BOOST_REQUIRE_EQUAL(lru_item.randomize_with_cache_misses(), 65u);
  BOOST_REQUIRE_EQUAL(lru_item.randomize_with_cache_size(), 64u);

  // Most recent entry (64) hits
  BOOST_REQUIRE(lru_item.randomize_with(lru_item.x() == 64u));
  BOOST_REQUIRE_EQUAL(lru_item.randomize_with_cache_hits(), 1u);

  // Least recent entry (0) was evicted -> misses
  BOOST_REQUIRE(lru_item.randomize_with(lru_item.x() == 0u));
  BOOST_REQUIRE_EQUAL(lru_item.randomize_with_cache_misses(), 66u);
  BOOST_REQUIRE_EQUAL(lru_item.randomize_with_cache_size(), 64u);
}

BOOST_AUTO_TEST_CASE(slot_updates_pre_randomize_duplicate_ref_and_soft_across_backends_and_signedness) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      // 1. pre_randomize() mutation of referenced member
      PreRandMutateItem pitem("pitem");
      for (unsigned i = 0; i < 6; ++i) {
        BOOST_REQUIRE(pitem.randomize_with(pitem.x() == reference(pitem.pre_target)));
        BOOST_REQUIRE_EQUAL(pitem.x, pitem.pre_target);
      }
      BOOST_REQUIRE_EQUAL(pitem.randomize_with_cache_misses(), 1u);
      BOOST_REQUIRE_EQUAL(pitem.randomize_with_cache_hits(), 5u);

      // 2. Duplicate uses of the same ReadReference object
      BasicCacheItem ditem("ditem");
      for (unsigned v = 1; v <= 5; ++v) {
        auto r = reference(v);
        BOOST_REQUIRE(ditem.randomize_with(ditem.x() == r, ditem.y() == r + 10u));
        BOOST_REQUIRE_EQUAL(ditem.x, v);
        BOOST_REQUIRE_EQUAL(ditem.y, v + 10u);
      }
      // Note: each iteration allocates a new `r` with a fresh id, yet all 5 hit the SAME $R0 slot!
      BOOST_REQUIRE_EQUAL(ditem.randomize_with_cache_misses(), 1u);
      BOOST_REQUIRE_EQUAL(ditem.randomize_with_cache_hits(), 4u);

      // 3. Soft constraint drop and re-activation via ReadReference slot
      SoftCacheItem sitem("sitem");
      unsigned req = 25u;
      BOOST_REQUIRE(sitem.randomize_with(sitem.a() == reference(req), soft(sitem.b() == 33u)));
      BOOST_REQUIRE_EQUAL(sitem.a, 25u);
      BOOST_REQUIRE_EQUAL(sitem.b, 33u);
      {
        auto inact = sitem.inactive_soft_constraints();
        std::sort(inact.begin(), inact.end());
        BOOST_REQUIRE((inact == std::vector<std::string>{"sitem.s_a#0", "sitem.s_b#0"}));
      }
      req = 10u;
      BOOST_REQUIRE(sitem.randomize_with(sitem.a() == reference(req), soft(sitem.b() == 33u)));
      BOOST_REQUIRE_EQUAL(sitem.a, 10u);
      BOOST_REQUIRE_EQUAL(sitem.b, 33u);
      BOOST_REQUIRE((sitem.inactive_soft_constraints() == std::vector<std::string>{"sitem.s_b#0"}));
      BOOST_REQUIRE_EQUAL(sitem.randomize_with_cache_misses(), 1u);
      BOOST_REQUIRE_EQUAL(sitem.randomize_with_cache_hits(), 1u);
    }
  }
}

BOOST_AUTO_TEST_CASE(multiple_items_modes_randc_solve_before_vectors_and_arrays) {
  CacheTestGuard guard;
  set_solver_backend("Z3");

  // Multiple independent items
  BasicCacheItem i1("i1"), i2("i2");
  for (unsigned k = 0; k < 5; ++k) {
    unsigned v1 = k + 1u, v2 = k + 20u;
    BOOST_REQUIRE(i1.randomize_with(i1.x() == reference(v1)));
    BOOST_REQUIRE(i2.randomize_with(i2.x() == reference(v2)));
    BOOST_REQUIRE_EQUAL(i1.x, v1);
    BOOST_REQUIRE_EQUAL(i2.x, v2);
  }
  BOOST_REQUIRE_EQUAL(i1.randomize_with_cache_misses(), 1u);
  BOOST_REQUIRE_EQUAL(i1.randomize_with_cache_hits(), 4u);
  BOOST_REQUIRE_EQUAL(i2.randomize_with_cache_misses(), 1u);
  BOOST_REQUIRE_EQUAL(i2.randomize_with_cache_hits(), 4u);

  // constraint_mode / deactivate / rand_mode invalidate cache
  i1.c_bound.deactivate();
  BOOST_REQUIRE_EQUAL(i1.randomize_with_cache_size(), 0u);
  unsigned big = 200u;
  BOOST_REQUIRE(i1.randomize_with(i1.x() == reference(big)));
  BOOST_REQUIRE_EQUAL(i1.x, 200u);
  i1.constraint_mode(true);
  BOOST_REQUIRE_EQUAL(i1.randomize_with_cache_size(), 0u);
  BOOST_REQUIRE(!i1.randomize_with(i1.x() == reference(big)));

  i1.x.rand_mode(false);
  i1.x = 17u;
  BOOST_REQUIRE(i1.randomize_with(i1.y() == i1.x() + 2u));
  BOOST_REQUIRE_EQUAL(i1.x, 17u);
  BOOST_REQUIRE_EQUAL(i1.y, 19u);
  i1.x.rand_mode(true);

  // randc with cached randomize_with
  RandcCacheItem rc("rc");
  rc.x.randc(true);
  std::set<uint8_t> seen;
  for (int i = 0; i < 4; ++i) {
    unsigned dummy = static_cast<unsigned>(i);
    BOOST_REQUIRE(rc.randomize_with(rc.x() >= 4, rc.y() == reference(dummy)));
    seen.insert(rc.x);
  }
  BOOST_REQUIRE_EQUAL(seen.size(), 4u);
  BOOST_REQUIRE_EQUAL(rc.randomize_with_cache_misses(), 1u);
  BOOST_REQUIRE_EQUAL(rc.randomize_with_cache_hits(), 3u);

  // Complex constructs: inside, unique, crv_vector (foreach, sum_with), crv_array (foreach, sum_with), solve_before
  ComplexCacheItem cx("cx");
  for (unsigned k = 0; k < 4; ++k) {
    unsigned target_sum = 12u + k;
    unsigned min_elem = 1u + k;
    BOOST_REQUIRE(cx.randomize_with(
        inside(cx.a(), 1u, 2u, 3u, 4u, 5u),
        unique(cx.a(), cx.b(), cx.c()),
        foreach(cx.vec(), cx.vec()[_i] >= reference(min_elem)),
        cx.vec().sum() == reference(target_sum),
        cx.vec().sum_with(cx.vec()[_i] >= reference(min_elem)) == 4u,
        foreach(cx.arr(), cx.arr()[_i] < 12u),
        cx.arr().sum_with(cx.arr()[_i] < 10u) == 4u));
    BOOST_REQUIRE(cx.a != cx.b && cx.b != cx.c && cx.a != cx.c);
    BOOST_REQUIRE_EQUAL(cx.vec.size(), 4u);
    unsigned sum = 0;
    for (unsigned idx = 0; idx < cx.vec.size(); ++idx) {
      unsigned e = cx.vec[idx];
      BOOST_REQUIRE_GE(e, min_elem);
      sum += e;
    }
    BOOST_REQUIRE_EQUAL(sum, target_sum);
  }
  // VectorReduceWith alpha-normalized placeholder hits the cache!
  BOOST_REQUIRE_EQUAL(cx.randomize_with_cache_misses(), 1u);
  BOOST_REQUIRE_EQUAL(cx.randomize_with_cache_hits(), 3u);

  // Zero-argument randomize_with and direct expression_list
  BOOST_REQUIRE(i1.randomize_with());
  struct SubItem : public BasicCacheItem {
    using BasicCacheItem::BasicCacheItem;
    bool run_list(expression_list const& l) { return randomize_with_expr_list(l); }
  } sub_item("sub_item");
  expression_list direct_list(sub_item.x() == 42u);
  BOOST_REQUIRE(sub_item.run_list(direct_list));
  BOOST_REQUIRE_EQUAL(sub_item.x, 42u);
}

BOOST_AUTO_TEST_CASE(lazy_analyse_hards_and_cached_sat_unsat_transitions) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    set_solver_backend(backend);
    VariableDefaultSolver::reset_analyse_hards_count();

    BasicCacheItem item("item");
    for (unsigned i = 0; i < 10; ++i) {
      unsigned v = i + 1u;
      BOOST_REQUIRE(item.randomize_with(item.x() == reference(v)));
    }
    // Unqueried UNSAT also does NOT trigger analyseHards()
    {
      unsigned bad = 500u;
      BOOST_REQUIRE(!item.randomize_with(item.x() == reference(bad)));
    }
    BOOST_REQUIRE_EQUAL(VariableDefaultSolver::analyse_hards_count(), 0u);

    // Now query analyse_contradiction() AFTER the stack local `bad` went out of scope!
    auto mus1 = item.analyse_contradiction();
    BOOST_REQUIRE_EQUAL(VariableDefaultSolver::analyse_hards_count(), 3u);
    BOOST_REQUIRE_EQUAL(mus1.size(), 1u);
    std::set<std::string> s1(mus1[0].begin(), mus1[0].end());
    BOOST_REQUIRE((s1 == std::set<std::string>{"item.c_bound#0", "item.randomize_with#0"}));

    // Cached transition back to SAT -> analyse_contradiction() updates to empty
    unsigned good = 15u;
    BOOST_REQUIRE(item.randomize_with(item.x() == reference(good)));
    BOOST_REQUIRE_EQUAL(item.x, 15u);
    BOOST_REQUIRE(item.analyse_contradiction().empty());

    // Cached transition back to UNSAT -> analyse_contradiction() reports conflict again
    unsigned bad2 = 300u;
    BOOST_REQUIRE(!item.randomize_with(item.x() == reference(bad2)));
    auto mus2 = item.analyse_contradiction();
    BOOST_REQUIRE_EQUAL(mus2.size(), 1u);
    std::set<std::string> s2(mus2[0].begin(), mus2[0].end());
    BOOST_REQUIRE((s2 == std::set<std::string>{"item.c_bound#0", "item.randomize_with#0"}));
    BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 1u);
    BOOST_REQUIRE_EQUAL(item.randomize_with_cache_hits(), 12u);

    // Pre-solve getInactiveSofts() & rand_mode(false) contradiction transition (without rebuild)
    SoftCacheItem sit("sit");
    sit.a.rand_mode(false);
    sit.a = 10u;
    BOOST_REQUIRE(sit.analyse_contradiction().empty());
    BOOST_REQUIRE(sit.inactive_soft_constraints().empty());
    sit.a = 999u;
    BOOST_REQUIRE(!sit.analyse_contradiction().empty());
    sit.a = 10u;
    BOOST_REQUIRE(sit.analyse_contradiction().empty());
  }
}

BOOST_AUTO_TEST_CASE(next_cov_unified_generator_and_sequence_item_goal) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    set_solver_backend(backend);

    // 1. Generator::nextCov(): gen[v], multi-partition, soft priority, unreachable skip
    {
      Variable<unsigned> a, b;
      Generator gen;
      gen(a < 10u)(b == 42u);
      gen.soft("s_a", a == 1u);
      gen.cover("cov_unreach", a == 99u);
      gen.cover("cov_5", a == 5u);
      gen.cover("cov_8", a == 8u);

      std::set<unsigned> hit_a;
      for (int i = 0; i < 4; ++i) {
        BOOST_REQUIRE(gen.nextCov());
        BOOST_REQUIRE_EQUAL(gen[b], 42u);  // Partition 1 MUST be solved!
        hit_a.insert(gen[a]);              // gen[a] MUST NOT throw!
        if (gen[a] == 5u || gen[a] == 8u) {
          BOOST_REQUIRE(gen.getInactiveSofts() == std::vector<std::string>{"s_a"});
        }
      }
      BOOST_REQUIRE_EQUAL(hit_a.count(5u), 1u);
      BOOST_REQUIRE_EQUAL(hit_a.count(8u), 1u);
      BOOST_REQUIRE(gen.isCovered());
    }

    // 2. Multi-partition nextCov() with UNSAT non-cover partition returns false
    {
      Variable<unsigned> a, b;
      Generator gen;
      gen(a < 10u)(b == 1u)(b == 2u);
      gen.cover("cov_5", a == 5u);
      BOOST_REQUIRE(!gen.nextCov());
    }

    // 3. nextCov() with VectorSolver (solveJoint + size retry + unreachable cover skip)
    {
      Variable<unsigned> total, sz;
      rand_vec<unsigned> vec(nullptr);
      Generator gen;
      gen.addVecId(vec.id());
      gen(total < 30u);
      gen(sz >= 1u && sz <= 3u);
      gen(vec().size() == sz);
      gen(foreach(vec(), vec()[_i] >= 1u && vec()[_i] <= 4u));
      gen(vec().sum() == total);
      gen.cover("cov_impossible", total == 20u);  // max sum is 3 * 4 = 12
      gen.cover("cov_twelve", total == 12u);      // requires sz == 3 (solveJoint / size retry!)
      gen.cover("cov_two", total == 2u);

      std::set<unsigned> hit_totals;
      for (int i = 0; i < 3; ++i) {
        BOOST_REQUIRE(gen.nextCov());
        hit_totals.insert(gen[total]);
      }
      BOOST_REQUIRE_EQUAL(hit_totals.count(12u), 1u);
      BOOST_REQUIRE_EQUAL(hit_totals.count(2u), 1u);
      BOOST_REQUIRE(gen.isCovered());
    }

    // 4. nextCov() unblocks cover when read_references change
    {
      Variable<unsigned> a;
      unsigned limit = 3u;
      Generator gen;
      gen(a <= reference(limit));
      gen.cover("cov_8", a == 8u);
      BOOST_REQUIRE(gen.nextCov());
      BOOST_REQUIRE_LE(gen[a], 3u);
      BOOST_REQUIRE(gen.isCovered());  // unreachable while limit == 3

      limit = 10u;
      BOOST_REQUIRE(gen.nextCov());
      BOOST_REQUIRE_EQUAL(gen[a], 8u);  // unblocked!
    }

    // 5. crv_sequence_item::goal(cg) BEFORE randomize(), across rebuilds, and in randomize_with()
    {
      GoalItem item("gitem");
      GoalCG cg("cg");
      cg.v.bind(item.x);

      // Must NOT crash before first randomize()
      item.goal(cg);

      for (int i = 0; i < 3; ++i) {
        BOOST_REQUIRE(item.randomize());
        cg.sample();
      }
      BOOST_REQUIRE(cg.covered());

      // Now test goal(cg) inside randomize_with() (including ReadReference unblock!)
      GoalItem item2("gitem2");
      GoalCG cg2("cg2");
      cg2.v.bind(item2.x);
      item2.goal(cg2);

      unsigned max_x = 10u;
      // Bins 2 and 7 are reachable; bin 13 is blocked while max_x == 10
      for (int i = 0; i < 4; ++i) {
        BOOST_REQUIRE(item2.randomize_with(item2.x() <= reference(max_x), item2.y() < 16u));
        cg2.sample();
      }
      BOOST_REQUIRE(!cg2.covered());

      // Rebuild via constraint_mode keeps saved goal, and max_x = 15 unblocks bin 13!
      item2.c_range.deactivate();
      item2.c_range.activate();
      max_x = 15u;
      BOOST_REQUIRE(item2.randomize_with(item2.x() <= reference(max_x), item2.y() < 16u));
      cg2.sample();
      // With rebuild or cache hit, goal covers bin 13 (one of the uncovered bins)
      for (int i = 0; i < 3 && !cg2.covered(); ++i) {
        BOOST_REQUIRE(item2.randomize_with(item2.x() <= reference(max_x), item2.y() < 16u));
        cg2.sample();
      }
      BOOST_REQUIRE(cg2.covered());
    }
  }
}

BOOST_AUTO_TEST_CASE(scoped_dead_read_refs_do_not_dangle_in_next_cov_or_goal) {
  CacheTestGuard guard;
  set_solver_backend("Z3");

  leave_dead_read_refs_in_global_container();

  // Standalone Generator::nextCov() must not dereference dead global read_references
  Variable<unsigned> v;
  Generator gen;
  gen(v < 10u);
  gen.cover("c1", v == 1u);
  BOOST_REQUIRE(gen.nextCov());
  BOOST_REQUIRE_EQUAL(gen[v], 1u);
  BOOST_REQUIRE(gen.nextCov());
  BOOST_REQUIRE(gen.isCovered());
  BOOST_REQUIRE(gen.nextCov());

  // crv_sequence_item::goal() + randomize() must likewise remain safe
  GoalItem gitem("gitem_uaf");
  GoalCGTwo cg("cg_uaf");
  cg.v.bind(gitem.x);
  gitem.goal(cg);
  for (int i = 0; i < 3; ++i) {
    BOOST_REQUIRE(gitem.randomize());
    cg.sample();
  }
  BOOST_REQUIRE(cg.covered());
}

BOOST_AUTO_TEST_CASE(goal_updates_covergroup_switching_and_external_covers) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      // 1. Repeated item.goal(cg) before each randomize(); cg.sample(); covers all bins
      {
        GoalItem item("rep_item");
        GoalCG cg("rep_cg");
        cg.v.bind(item.x);

        std::set<unsigned> seen;
        for (int i = 0; i < 3; ++i) {
          item.goal(cg);
          BOOST_REQUIRE(item.randomize());
          cg.sample();
          seen.insert(item.x);
        }
        BOOST_REQUIRE(cg.covered());
        BOOST_REQUIRE((seen == std::set<unsigned>{2u, 7u, 13u}));

        // Calling goal(cg) after full coverage clears the finished goal cleanly
        item.goal(cg);
        BOOST_REQUIRE(item.randomize());
      }

      // 2. Switching goal(cg1) -> goal(cg2) covers cg2 without re-targeting cg1's covered bins
      {
        GoalItem item("sw_item");
        GoalCG cg1("sw_cg1");
        GoalCGTwo cg2("sw_cg2");
        cg1.v.bind(item.x);
        cg2.v.bind(item.x);

        item.goal(cg1);
        for (int i = 0; i < 3; ++i) {
          BOOST_REQUIRE(item.randomize());
          cg1.sample();
        }
        BOOST_REQUIRE(cg1.covered());

        item.goal(cg2);
        for (int i = 0; i < 2; ++i) {
          BOOST_REQUIRE(item.randomize());
          cg2.sample();
        }
        BOOST_REQUIRE(cg2.covered());
      }

      // 3. External randomize_with covers one bin, then goal(cg) update hits only remaining uncovered bin
      {
        GoalItem item("ext_item");
        GoalCGTwo cg("ext_cg");
        cg.v.bind(item.x);

        item.goal(cg);
        BOOST_REQUIRE(item.randomize_with(item.x() == 3u));
        cg.sample();
        BOOST_REQUIRE(!cg.covered());

        item.goal(cg);
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_EQUAL(item.x, 11u);
        cg.sample();
        BOOST_REQUIRE(cg.covered());
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(multi_partition_next_cov_does_not_poison_unrelated_partition_on_vector_failure) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      Variable<unsigned> total, sz, other;
      rand_vec<unsigned> vec(nullptr);
      Generator gen;
      gen.addVecId(vec.id());

      // Partition 0 (coupled with vec)
      gen(total < 30u);
      gen(sz == 2u);
      gen(vec().size() == sz);
      gen(foreach(vec(), vec()[_i] >= 1u && vec()[_i] <= 4u));
      gen(vec().sum() == total);
      gen.cover("c0_bad_vec", total == 12u);  // SAT in scalar solver, UNSAT in VectorSolver (max sum is 8)
      gen.cover("c0_ok_vec", total == 4u);

      // Partition 1 (unrelated to vec)
      gen(other < 10u);
      gen.cover("c1_ok", other == 7u);

      std::set<unsigned> hit_totals;
      std::set<unsigned> hit_others;
      for (int i = 0; i < 2; ++i) {
        BOOST_REQUIRE(gen.nextCov());
        hit_totals.insert(gen[total]);
        hit_others.insert(gen[other]);
      }
      BOOST_REQUIRE_EQUAL(hit_others.count(7u), 1u);
      BOOST_REQUIRE_EQUAL(hit_totals.count(4u), 1u);
      BOOST_REQUIRE(gen.nextCov());
      BOOST_REQUIRE(gen.isCovered());
    }
  }
}

BOOST_AUTO_TEST_CASE(fast_fail_clears_inactive_softs_and_hard_only_pre_solve_skips_analyse_hards) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    set_solver_backend(backend);

    // 1. Fast-fail after UNSAT analyseContradiction() must clear stale inactive_softs_
    {
      Variable<unsigned> a, b;
      unsigned ref_a = 20u;
      Generator gen;
      gen(a < 50u)(b < 50u)(a == reference(ref_a));
      gen.soft("s_a", a == 10u);
      gen.soft("s_b", b == 20u);

      BOOST_REQUIRE(gen.next());
      BOOST_REQUIRE((gen.getInactiveSofts() == std::vector<std::string>{"s_a"}));

      ref_a = 999u;
      BOOST_REQUIRE(!gen.analyseContradiction().empty());
      BOOST_REQUIRE(!gen.next());  // hits cached-UNSAT fast-fail
      BOOST_REQUIRE(gen.getInactiveSofts().empty());
    }

    // 2. Pre-solve inactive_soft_constraints() on a hard-only item must NOT trigger analyseHards()
    {
      VariableDefaultSolver::reset_analyse_hards_count();
      BasicCacheItem hard_only("hard_only");
      BOOST_REQUIRE(hard_only.inactive_soft_constraints().empty());
      BOOST_REQUIRE_EQUAL(VariableDefaultSolver::analyse_hards_count(), 0u);
    }
  }
}

BOOST_AUTO_TEST_CASE(multithreaded_generator_next_and_next_cov_z3_and_cudd) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      // 1. Multi-partition next() and nextCov() with enable_multithreading()
      {
        Variable<unsigned> a, b, c;
        Generator gen;
        gen(a < 16u)(b < 16u)(c < 16u);
        gen.soft("s_a", a == 1u);
        gen.soft("s_b", b == 2u);
        gen.cover("cov_a_5", a == 5u);
        gen.cover("cov_b_6", b == 6u);
        gen.cover("cov_c_7", c == 7u);
        gen.cover("cov_unreach", a == 99u);
        gen.enable_multithreading();

        std::set<unsigned> hit_a, hit_b, hit_c;
        for (int i = 0; i < 4 && !gen.isCovered(); ++i) {
          BOOST_REQUIRE(gen.nextCov());
          hit_a.insert(gen[a]);
          hit_b.insert(gen[b]);
          hit_c.insert(gen[c]);
        }
        BOOST_REQUIRE(gen.isCovered());
        BOOST_REQUIRE_EQUAL(hit_a.count(5u), 1u);
        BOOST_REQUIRE_EQUAL(hit_b.count(6u), 1u);
        BOOST_REQUIRE_EQUAL(hit_c.count(7u), 1u);

        for (int i = 0; i < 4; ++i) {
          BOOST_REQUIRE(gen.next());
          BOOST_REQUIRE_LT(gen[a], 16u);
          BOOST_REQUIRE_LT(gen[b], 16u);
          BOOST_REQUIRE_LT(gen[c], 16u);
        }
      }

      // 2. Repeated MT rebuilds (rand_mode / reference + analyseContradiction fast-fail)
      {
        Variable<unsigned> a, b;
        unsigned ref_a = 10u;
        Generator gen;
        gen(a < 50u && a == reference(ref_a))(b < 50u);
        gen.soft("s_a", a == 10u);
        gen.soft("s_b", b == 20u);
        gen.cover("c_b", b == 30u);
        gen.enable_multithreading();

        BOOST_REQUIRE(gen.nextCov());
        BOOST_REQUIRE_EQUAL(gen[b], 30u);
        BOOST_REQUIRE((gen.getInactiveSofts() == std::vector<std::string>{"s_b"}));

        ref_a = 999u;
        BOOST_REQUIRE(!gen.analyseContradiction().empty());
        BOOST_REQUIRE(!gen.nextCov());
        BOOST_REQUIRE(gen.getInactiveSofts().empty());
        BOOST_REQUIRE(!gen.next());
        BOOST_REQUIRE(gen.getInactiveSofts().empty());
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(goal_pruning_cache_invalidation_and_last_gen_reset) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      // 1. goal(cg1) -> randomize_with() -> cover 100% via randomize() -> goal(cg1) clears cache & gen_
      {
        GoalItem item("g_clr");
        GoalCG cg("cg_clr");
        cg.v.bind(item.x);

        item.goal(cg);
        BOOST_REQUIRE(item.randomize_with(item.y() < 10u));
        cg.sample();
        BOOST_REQUIRE_EQUAL(item.randomize_with_cache_size(), 1u);
        BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 1u);

        while (!cg.covered()) {
          BOOST_REQUIRE(item.randomize());
          cg.sample();
        }

        item.goal(cg);
        BOOST_REQUIRE_EQUAL(item.randomize_with_cache_size(), 0u);
        BOOST_REQUIRE(item.randomize_with(item.y() < 10u));
        BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 2u);
        BOOST_REQUIRE_EQUAL(item.randomize_with_cache_hits(), 0u);
      }

      // 2. goal(cg1) -> cover cg1 via randomize_with() -> goal(cg2) -> randomize() immediately targets cg2
      {
        GoalItem item("g_sw");
        GoalCG cg1("cg_sw1");
        GoalCGTwo cg2("cg_sw2");
        cg1.v.bind(item.x);
        cg2.v.bind(item.x);

        item.goal(cg1);
        for (int i = 0; i < 3; ++i) {
          BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
          cg1.sample();
        }
        BOOST_REQUIRE(cg1.covered());

        item.goal(cg2);
        std::set<unsigned> hits;
        for (int i = 0; i < 2; ++i) {
          BOOST_REQUIRE(item.randomize());
          cg2.sample();
          hits.insert(item.x);
        }
        BOOST_REQUIRE(cg2.covered());
        BOOST_REQUIRE((hits == std::set<unsigned>{3u, 11u}));
      }

      // 3. goal() after randomize_with() resets last_gen_ so diagnostics don't read stale rand_with_gen_
      {
        SoftCacheItem sit("g_last_gen");
        BOOST_REQUIRE(sit.randomize_with(sit.a() == 30u));
        BOOST_REQUIRE((sit.inactive_soft_constraints() == std::vector<std::string>{"g_last_gen.s_a#0"}));

        GoalCG cg("cg_lg");
        cg.v.bind(sit.b);
        sit.goal(cg);
        BOOST_REQUIRE(sit.inactive_soft_constraints().empty());
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(next_cov_hits_empty_during_failed_size_retry_still_hits_reachable_same_size_cover) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      Variable<unsigned> total, sz;
      rand_vec<unsigned> vec(nullptr);
      Generator gen;
      gen.addVecId(vec.id());
      gen(total < 30u);
      gen(sz >= 2u && sz <= 3u);
      gen(vec().size() == sz);
      gen(foreach(vec(), vec()[_i] >= 1u && vec()[_i] <= 4u));
      gen(vec().sum() == total);
      gen.cover("c_bad_sz2", total == 9u && sz == 2u);   // max sum at sz == 2 is 8
      gen.cover("c_good_sz2", total == 4u && sz == 2u);  // reachable at sz == 2

      std::set<unsigned> hit_totals;
      for (int i = 0; i < 4 && !gen.isCovered(); ++i) {
        BOOST_REQUIRE(gen.nextCov());
        hit_totals.insert(gen[total]);
      }
      BOOST_REQUIRE(gen.isCovered());
      BOOST_REQUIRE_EQUAL(hit_totals.count(4u), 1u);
    }
  }
}

BOOST_AUTO_TEST_CASE(transitive_multi_partition_vector_coupling_does_not_poison_reachable_covers) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      // P0 -- V0 -- P1 -- V1 -- P2, plus independent P3
      {
        Variable<unsigned> s0, bridge, s2, indep;
        rand_vec<unsigned> v0(nullptr), v1(nullptr);
        Generator gen;
        gen.addVecId(v0.id());
        gen.addVecId(v1.id());

        // P0
        gen(s0 < 30u);
        gen.cover("c0_bad", s0 == 20u);
        gen.cover("c0_ok", s0 == 4u);

        // V0 coupling P0 (s0) and P1 (bridge)
        gen(v0().size() == 2u);
        gen(foreach(v0(), v0()[_i] >= 1u && v0()[_i] <= 4u && v0()[_i] >= bridge));
        gen(v0().sum() == s0);

        // P1
        gen(bridge >= 1u && bridge <= 4u);
        gen.cover("c1_ok", bridge == 2u);

        // V1 coupling P1 (bridge) and P2 (s2)
        gen(v1().size() == 2u);
        gen(foreach(v1(), v1()[_i] >= 1u && v1()[_i] <= 4u && v1()[_i] >= bridge));
        gen(v1().sum() == s2);

        // P2
        gen(s2 < 30u);
        gen.cover("c2_ok", s2 == 6u);

        // P3 (independent)
        gen(indep < 10u);
        gen.cover("c3_indep", indep == 7u);

        std::set<unsigned> hit_s0, hit_bridge, hit_s2, hit_indep;
        for (int i = 0; i < 6 && !gen.isCovered(); ++i) {
          BOOST_REQUIRE(gen.nextCov());
          hit_s0.insert(gen[s0]);
          hit_bridge.insert(gen[bridge]);
          hit_s2.insert(gen[s2]);
          hit_indep.insert(gen[indep]);
        }
        BOOST_REQUIRE(gen.isCovered());
        BOOST_REQUIRE_EQUAL(hit_s0.count(4u), 1u);
        BOOST_REQUIRE_EQUAL(hit_bridge.count(2u), 1u);
        BOOST_REQUIRE_EQUAL(hit_s2.count(6u), 1u);
        BOOST_REQUIRE_EQUAL(hit_indep.count(7u), 1u);
      }

      // Same transitive chain with solved size variable triggering failedSize() unwind
      {
        Variable<unsigned> s0, sz0, bridge, s2, indep;
        rand_vec<unsigned> v0(nullptr), v1(nullptr);
        Generator gen;
        gen.addVecId(v0.id());
        gen.addVecId(v1.id());

        gen(s0 < 30u);
        gen(sz0 == 2u);
        gen.cover("c0_bad", s0 == 20u);
        gen.cover("c0_ok", s0 == 4u);

        gen(v0().size() == sz0);
        gen(foreach(v0(), v0()[_i] >= 1u && v0()[_i] <= 4u && v0()[_i] >= bridge));
        gen(v0().sum() == s0);

        gen(bridge >= 1u && bridge <= 4u);
        gen.cover("c1_ok", bridge == 2u);

        gen(v1().size() == 2u);
        gen(foreach(v1(), v1()[_i] >= 1u && v1()[_i] <= 4u && v1()[_i] >= bridge));
        gen(v1().sum() == s2);

        gen(s2 < 30u);
        gen.cover("c2_ok", s2 == 6u);

        gen(indep < 10u);
        gen.cover("c3_indep", indep == 7u);

        std::set<unsigned> hit_s0, hit_bridge, hit_s2, hit_indep;
        for (int i = 0; i < 6 && !gen.isCovered(); ++i) {
          BOOST_REQUIRE(gen.nextCov());
          hit_s0.insert(gen[s0]);
          hit_bridge.insert(gen[bridge]);
          hit_s2.insert(gen[s2]);
          hit_indep.insert(gen[indep]);
        }
        BOOST_REQUIRE(gen.isCovered());
        BOOST_REQUIRE_EQUAL(hit_s0.count(4u), 1u);
        BOOST_REQUIRE_EQUAL(hit_bridge.count(2u), 1u);
        BOOST_REQUIRE_EQUAL(hit_s2.count(6u), 1u);
        BOOST_REQUIRE_EQUAL(hit_indep.count(7u), 1u);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(multi_partition_fast_fail_clears_unvisited_partition_inactive_softs) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      // nextCov() fast-fail
      {
        Variable<unsigned> a, b;
        unsigned ref_a = 10u;
        Generator gen;
        gen(a < 50u && a == reference(ref_a))(b < 50u);
        gen.soft("s_a", a == 10u);
        gen.soft("s_b", b == 20u);
        gen.cover("c_b", b == 30u);

        BOOST_REQUIRE(gen.nextCov());
        BOOST_REQUIRE_EQUAL(gen[b], 30u);
        BOOST_REQUIRE((gen.getInactiveSofts() == std::vector<std::string>{"s_b"}));

        ref_a = 999u;
        BOOST_REQUIRE(!gen.analyseContradiction().empty());
        BOOST_REQUIRE(!gen.nextCov());
        BOOST_REQUIRE(gen.getInactiveSofts().empty());
      }

      // next() fast-fail
      {
        Variable<unsigned> a, b;
        unsigned ref_a = 10u;
        Generator gen;
        gen(a < 50u && a == reference(ref_a))(b < 50u);
        gen.soft("s_a", a == 10u);
        gen.soft("s_b", b == 20u);
        gen.cover("c_b", b == 30u);

        BOOST_REQUIRE(gen.nextCov());
        BOOST_REQUIRE_EQUAL(gen[b], 30u);
        BOOST_REQUIRE((gen.getInactiveSofts() == std::vector<std::string>{"s_b"}));

        ref_a = 999u;
        BOOST_REQUIRE(!gen.analyseContradiction().empty());
        BOOST_REQUIRE(!gen.next());
        BOOST_REQUIRE(gen.getInactiveSofts().empty());
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(multithreaded_multi_var_dist_bool_and_enable_mt_before_cover) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      // 1. Multi-variable partitions + explicit dist() + distribution<bool> under enable_multithreading()
      {
        Variable<unsigned> a0, b0, a1, b1, a2, b2, a3, b3;
        Variable<bool> f0, f1, f2, f3;
        Generator gen;
        gen(a0 + b0 == 12u)(a0 >= 1u && a0 <= 11u)(b0 >= 1u && b0 <= 11u);
        gen(a1 + b1 == 14u)(a1 >= 1u && a1 <= 13u)(b1 >= 1u && b1 <= 13u);
        gen(a2 + b2 == 16u)(a2 >= 1u && a2 <= 15u)(b2 >= 1u && b2 <= 15u);
        gen(a3 + b3 == 18u)(a3 >= 1u && a3 <= 17u)(b3 >= 1u && b3 <= 17u);
        gen(dist(a0, distribution<unsigned>::create(range<unsigned>(1u, 5u))(range<unsigned>(6u, 11u))));
        gen(dist(a1, distribution<unsigned>::create(range<unsigned>(1u, 6u))(range<unsigned>(7u, 13u))));
        gen(dist(a2, distribution<unsigned>::create(range<unsigned>(1u, 7u))(range<unsigned>(8u, 15u))));
        gen(dist(a3, distribution<unsigned>::create(range<unsigned>(1u, 8u))(range<unsigned>(9u, 17u))));
        gen(dist(f0, distribution<bool>::create(0.75)));
        gen(dist(f1, distribution<bool>::create(0.25)));
        gen(dist(f2, distribution<bool>::create(0.5)));
        gen(dist(f3, distribution<bool>::create(0.8)));
        gen.cover("c0", a0 == 5u);
        gen.cover("c1", a1 == 7u);
        gen.cover("c2", a2 == 9u);
        gen.cover("c3", a3 == 11u);
        gen.cover("cf0", f0 == true);
        gen.cover("cf1", f1 == false);
        gen.enable_multithreading();

        std::set<unsigned> hit0, hit1, hit2, hit3;
        for (int i = 0; i < 8 && !gen.isCovered(); ++i) {
          BOOST_REQUIRE(gen.nextCov());
          BOOST_REQUIRE_EQUAL(gen[a0] + gen[b0], 12u);
          BOOST_REQUIRE_EQUAL(gen[a1] + gen[b1], 14u);
          BOOST_REQUIRE_EQUAL(gen[a2] + gen[b2], 16u);
          BOOST_REQUIRE_EQUAL(gen[a3] + gen[b3], 18u);
          hit0.insert(gen[a0]);
          hit1.insert(gen[a1]);
          hit2.insert(gen[a2]);
          hit3.insert(gen[a3]);
        }
        BOOST_REQUIRE(gen.isCovered());
        BOOST_REQUIRE_EQUAL(hit0.count(5u), 1u);
        BOOST_REQUIRE_EQUAL(hit1.count(7u), 1u);
        BOOST_REQUIRE_EQUAL(hit2.count(9u), 1u);
        BOOST_REQUIRE_EQUAL(hit3.count(11u), 1u);

        for (int i = 0; i < 8; ++i) {
          BOOST_REQUIRE(gen.next());
          BOOST_REQUIRE_EQUAL(gen[a0] + gen[b0], 12u);
          BOOST_REQUIRE_EQUAL(gen[a1] + gen[b1], 14u);
          BOOST_REQUIRE_EQUAL(gen[a2] + gen[b2], 16u);
          BOOST_REQUIRE_EQUAL(gen[a3] + gen[b3], 18u);
        }
      }

      // 2. enable_multithreading() before constraints/covers -> !isCovered() and hits cover
      {
        Variable<unsigned> a;
        Generator gen;
        gen.enable_multithreading();
        gen(a < 16u);
        gen.cover("c", a == 5u);
        BOOST_REQUIRE(!gen.isCovered());

        bool hit_5 = false;
        for (int i = 0; i < 4 && !gen.isCovered(); ++i) {
          BOOST_REQUIRE(gen.nextCov());
          if (gen[a] == 5u) hit_5 = true;
        }
        BOOST_REQUIRE(gen.isCovered());
        BOOST_REQUIRE(hit_5);
      }

      // 3. enable_multithreading() after addVecId() preserves to_be_generated_vec_ids_
      {
        rand_vec<unsigned> vec(nullptr);
        Generator gen;
        gen.addVecId(vec.id());
        gen(vec().size() == 3u);
        gen(foreach(vec(), vec()[_i] >= 2u && vec()[_i] <= 5u));
        gen.enable_multithreading();

        BOOST_REQUIRE(gen.next());
        BOOST_REQUIRE_EQUAL(vec.size(), 3u);
        for (unsigned i = 0; i < 3u; ++i) {
          BOOST_REQUIRE(vec[i] >= 2u && vec[i] <= 5u);
        }
      }

      // 4. Adding cover() after next() invalidates isCovered()
      {
        Variable<unsigned> a;
        Generator gen;
        gen(a < 16u);
        BOOST_REQUIRE(gen.next());
        gen.cover("c", a == 7u);
        BOOST_REQUIRE(!gen.isCovered());
        bool hit_7 = false;
        for (int i = 0; i < 4 && !gen.isCovered(); ++i) {
          BOOST_REQUIRE(gen.nextCov());
          if (gen[a] == 7u) hit_7 = true;
        }
        BOOST_REQUIRE(gen.isCovered());
        BOOST_REQUIRE(hit_7);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(next_cov_multi_step_failed_size_does_not_poison_coupled_partition) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      Variable<unsigned> s0, sz, bridge;
      rand_vec<unsigned> v0(nullptr);
      Generator gen;
      gen.addVecId(v0.id());

      // P0
      gen(sz >= 2u && sz <= 3u);
      gen(s0 <= 5u);
      gen.cover("c0_bad", s0 == 5u && sz == 2u);
      gen.cover("c0_ok", s0 == 4u && sz == 2u);

      // V0 coupling P0 (s0, sz) and P1 (bridge)
      gen(v0().size() == sz);
      gen(foreach(v0(), v0()[_i] >= 1u && v0()[_i] <= 2u && v0()[_i] >= bridge));
      gen(v0().sum() == s0);

      // P1
      gen(bridge >= 1u && bridge <= 2u);
      gen.cover("c1_ok", bridge == 2u);

      std::set<unsigned> hit_s0, hit_bridge;
      for (int i = 0; i < 6 && !gen.isCovered(); ++i) {
        BOOST_REQUIRE(gen.nextCov());
        hit_s0.insert(gen[s0]);
        hit_bridge.insert(gen[bridge]);
      }
      BOOST_REQUIRE(gen.isCovered());
      BOOST_REQUIRE_EQUAL(hit_s0.count(4u), 1u);
      BOOST_REQUIRE_EQUAL(hit_bridge.count(2u), 1u);
    }
  }
}

BOOST_AUTO_TEST_CASE(request_rebuild_prunes_covered_goals_before_reset) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      // 1. goal(cg1) -> cover cg1 via randomize_with() -> constraint_mode(false) -> goal(cg2) -> cg2 covered in 2 steps
      {
        GoalToggleItem item("gt_cm");
        GoalCG cg1("cg1_cm");
        GoalCGTwo cg2("cg2_cm");
        cg1.v.bind(item.x);
        cg2.v.bind(item.x);

        item.goal(cg1);
        for (int i = 0; i < 3; ++i) {
          BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
          cg1.sample();
        }
        BOOST_REQUIRE(cg1.covered());

        item.c_extra.constraint_mode(false);
        item.goal(cg2);

        std::set<unsigned> hits;
        for (int i = 0; i < 2; ++i) {
          BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
          cg2.sample();
          hits.insert(item.x);
        }
        BOOST_REQUIRE(cg2.covered());
        BOOST_REQUIRE((hits == std::set<unsigned>{3u, 11u}));
      }

      // 2. goal(cg1) -> cover cg1 via randomize() -> rand_mode(false) -> goal(cg2) -> cg2 covered in 2 steps via randomize()
      {
        GoalToggleItem item("gt_rm");
        GoalCG cg1("cg1_rm");
        GoalCGTwo cg2("cg2_rm");
        cg1.v.bind(item.x);
        cg2.v.bind(item.x);

        item.goal(cg1);
        for (int i = 0; i < 3; ++i) {
          BOOST_REQUIRE(item.randomize());
          cg1.sample();
        }
        BOOST_REQUIRE(cg1.covered());

        item.z.rand_mode(false);
        item.z = 5u;
        item.goal(cg2);

        std::set<unsigned> hits;
        for (int i = 0; i < 2; ++i) {
          BOOST_REQUIRE(item.randomize());
          cg2.sample();
          hits.insert(item.x);
        }
        BOOST_REQUIRE(cg2.covered());
        BOOST_REQUIRE((hits == std::set<unsigned>{3u, 11u}));
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(next_cov_disjoint_coverless_vector_and_independent_coupled_cover) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      // 1. DISJOINT_NON_FS: coverless V0 fails failedSize at sz0==2 (passes at sz0==3),
      //    independent (P1, V1) fails non-failedSize for c1_bad and hits c1_ok.
      {
        Variable<unsigned> s0, sz0, x1, sz1;
        rand_vec<unsigned> v0(nullptr), v1(nullptr);
        Generator gen;
        gen.addVecId(v0.id());
        gen.addVecId(v1.id());

        gen(sz0 >= 2u && sz0 <= 3u)(s0 == 10u);
        gen.soft("prefer_sz0_2", sz0 == 2u);
        gen(v0().size() == sz0)(foreach(v0(), v0()[_i] >= 1u && v0()[_i] <= 4u))(v0().sum() == s0);

        gen(x1 <= 10u)(sz1 == 2u);
        gen.cover("c1_bad", x1 == 10u);
        gen.cover("c1_ok", x1 == 4u);
        gen(v1().size() == sz1)(foreach(v1(), v1()[_i] == x1 && v1()[_i] <= 4u));

        std::set<unsigned> hit_x1;
        for (int i = 0; i < 6 && !gen.isCovered(); ++i) {
          BOOST_REQUIRE(gen.nextCov());
          hit_x1.insert(gen[x1]);
          BOOST_REQUIRE_EQUAL(v0.size(), 3u);
        }
        BOOST_REQUIRE(gen.isCovered());
        BOOST_REQUIRE_EQUAL(hit_x1.count(4u), 1u);
      }

      // 2. DISJOINT_FS: coverless V0 fails failedSize at sz0==2 (passes at sz0==3),
      //    independent (P1, V1) fails failedSize for c1_bad at sz1 in [2,3] and hits c1_ok.
      {
        Variable<unsigned> s0, sz0, x1, sz1;
        rand_vec<unsigned> v0(nullptr), v1(nullptr);
        Generator gen;
        gen.addVecId(v0.id());
        gen.addVecId(v1.id());

        gen(sz0 >= 2u && sz0 <= 3u)(s0 == 10u);
        gen.soft("prefer_sz0_2", sz0 == 2u);
        gen(v0().size() == sz0)(foreach(v0(), v0()[_i] >= 1u && v0()[_i] <= 4u))(v0().sum() == s0);

        gen(x1 <= 10u)(sz1 >= 2u && sz1 <= 3u);
        gen.soft("prefer_sz1_2", sz1 == 2u);
        gen.cover("c1_bad", x1 == 10u);
        gen.cover("c1_ok", x1 == 4u);
        gen(v1().size() == sz1)(foreach(v1(), v1()[_i] >= 1u && v1()[_i] <= 2u))(v1().sum() == x1);

        std::set<unsigned> hit_x1;
        for (int i = 0; i < 6 && !gen.isCovered(); ++i) {
          BOOST_REQUIRE(gen.nextCov());
          hit_x1.insert(gen[x1]);
          BOOST_REQUIRE_EQUAL(v0.size(), 3u);
        }
        BOOST_REQUIRE(gen.isCovered());
        BOOST_REQUIRE_EQUAL(hit_x1.count(4u), 1u);
      }

      // 3. Disjoint coupled (P0, V0) where c0_ok fails at sz0==2 and succeeds at sz0==3,
      //    plus independent (P1, V1) with c1_bad / c1_ok: c0_ok must not be poisoned by V1's failure.
      {
        Variable<unsigned> s0, sz0, x1, sz1;
        rand_vec<unsigned> v0(nullptr), v1(nullptr);
        Generator gen;
        gen.addVecId(v0.id());
        gen.addVecId(v1.id());

        gen(sz0 >= 2u && sz0 <= 3u)(s0 <= 10u);
        gen.soft("prefer_sz0_2", sz0 == 2u);
        gen.cover("c0_ok", s0 == 10u);
        gen(v0().size() == sz0)(foreach(v0(), v0()[_i] >= 1u && v0()[_i] <= 4u))(v0().sum() == s0);

        gen(x1 <= 10u)(sz1 == 2u);
        gen.cover("c1_bad", x1 == 10u);
        gen.cover("c1_ok", x1 == 4u);
        gen(v1().size() == sz1)(foreach(v1(), v1()[_i] == x1 && v1()[_i] <= 4u));

        std::set<unsigned> hit_s0, hit_x1;
        for (int i = 0; i < 6 && !gen.isCovered(); ++i) {
          BOOST_REQUIRE(gen.nextCov());
          hit_s0.insert(gen[s0]);
          hit_x1.insert(gen[x1]);
        }
        BOOST_REQUIRE(gen.isCovered());
        BOOST_REQUIRE_EQUAL(hit_s0.count(10u), 1u);
        BOOST_REQUIRE_EQUAL(hit_x1.count(4u), 1u);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(get_inactive_softs_and_analyse_contradiction_preserve_coverage_after_size_exclusion) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      // 1. Generator::nextCov() loop calling getInactiveSofts() after VectorConstraint failedSize() unwind
      {
        Variable<unsigned> x, s0, sz;
        rand_vec<unsigned> v0(nullptr);
        Generator gen;
        gen.addVecId(v0.id());

        gen(x < 16u)(s0 == 5u)(sz >= 2u && sz <= 3u);
        gen(v0().size() == sz)(foreach(v0(), v0()[_i] >= 1u && v0()[_i] <= 2u))(v0().sum() == s0);
        gen.soft("s_sz", sz == 2u);
        gen.soft("s_x", x == 3u);
        gen.cover("c_x7", x == 7u);
        gen.cover("c_x9", x == 9u);

        std::set<unsigned> hit_x;
        bool saw_inactive_sx = false;
        std::size_t prev_cov = 0;
        for (int i = 0; i < 6 && !gen.isCovered(); ++i) {
          BOOST_REQUIRE(gen.nextCov());
          BOOST_REQUIRE_EQUAL(v0.size(), 3u);
          hit_x.insert(gen[x]);

          std::vector<std::string> inact = gen.getInactiveSofts();
          BOOST_REQUIRE(gen.coveredSet().size() >= prev_cov);
          prev_cov = gen.coveredSet().size();

          if (gen[x] == 7u || gen[x] == 9u) {
            BOOST_REQUIRE(std::find(inact.begin(), inact.end(), "s_x") != inact.end());
            BOOST_REQUIRE(std::find(inact.begin(), inact.end(), "s_sz") != inact.end());
            saw_inactive_sx = true;
          }
        }
        BOOST_REQUIRE(gen.isCovered());
        BOOST_REQUIRE(saw_inactive_sx);
        BOOST_REQUIRE_EQUAL(hit_x.count(7u), 1u);
        BOOST_REQUIRE_EQUAL(hit_x.count(9u), 1u);
      }

      // 2. Generator::nextCov() + analyseContradiction() after failedSize() unwind preserves coverage
      //    and clears internal size exclusions before contradiction analysis
      {
        Variable<unsigned> x, s0, sz;
        unsigned ref_x_cap = 15u;
        rand_vec<unsigned> v0(nullptr);
        Generator gen;
        gen.addVecId(v0.id());

        gen(x < 16u && x <= reference(ref_x_cap))(s0 == 5u)(sz >= 2u && sz <= 3u);
        gen(v0().size() == sz)(foreach(v0(), v0()[_i] >= 1u && v0()[_i] <= 2u))(v0().sum() == s0);
        gen.soft("s_sz", sz == 2u);
        gen.cover("c_x7", x == 7u);
        gen.cover("c_x9", x == 9u);

        BOOST_REQUIRE(gen.nextCov());
        BOOST_REQUIRE_EQUAL(v0.size(), 3u);
        std::size_t const cov_before = gen.coveredSet().size();
        BOOST_REQUIRE(cov_before >= 1u);

        auto conflicts_ok = gen.analyseContradiction();
        BOOST_REQUIRE(conflicts_ok.empty());
        BOOST_REQUIRE_EQUAL(gen.coveredSet().size(), cov_before);

        while (!gen.isCovered()) {
          BOOST_REQUIRE(gen.nextCov());
        }
        BOOST_REQUIRE(gen.isCovered());
        BOOST_REQUIRE_EQUAL(gen.coveredSet().size(), 2u);

        // Hard contradiction via ReadReference after a size-excluded next() is still detected accurately
        // while preserving coveredSet().
        ref_x_cap = 20u;
        BOOST_REQUIRE(gen.next());
        gen("c_min_x", x >= 100u);
        BOOST_REQUIRE(!gen.analyseContradiction().empty());
      }

      // 3. crv_sequence_item::randomize() / randomize_with() + inactive_soft_constraints() + analyse_contradiction()
      //    after VectorConstraint failedSize() unwind
      {
        SoftVectorCacheItem item("sv_cache_item");
        SoftVectorCG cg("sv_cg");
        cg.v.bind(item.x);
        item.goal(cg);

        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_EQUAL(item.v0.size(), 3u);
        BOOST_REQUIRE_EQUAL(item.x, 7u);
        cg.sample();
        BOOST_REQUIRE(cg.covered());

        std::vector<std::string> inact = item.inactive_soft_constraints();
        BOOST_REQUIRE(std::find(inact.begin(), inact.end(), "sv_cache_item.s_x#0") != inact.end());
        BOOST_REQUIRE(std::find(inact.begin(), inact.end(), "sv_cache_item.s_sz#0") != inact.end());
        BOOST_REQUIRE(item.analyse_contradiction().empty());

        BOOST_REQUIRE(item.randomize_with(item.x() == 9u));
        BOOST_REQUIRE_EQUAL(item.v0.size(), 3u);
        BOOST_REQUIRE_EQUAL(item.x, 9u);
        std::vector<std::string> inact_rw = item.inactive_soft_constraints();
        BOOST_REQUIRE(std::find(inact_rw.begin(), inact_rw.end(), "sv_cache_item.s_x#0") != inact_rw.end());
        BOOST_REQUIRE(std::find(inact_rw.begin(), inact_rw.end(), "sv_cache_item.s_sz#0") != inact_rw.end());
        BOOST_REQUIRE(item.analyse_contradiction().empty());
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(covered_stack_scoped_covergroup_destruction_has_no_uar_or_cache_poisoning) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      // 1. Covered via randomize() -> destroy cg + stack_poison() -> randomize() & randomize_with()
      {
        GoalItem item("uar_r_item");
        run_scoped_cg_randomize(item);
        stack_poison();

        for (int i = 0; i < 4; ++i) {
          BOOST_REQUIRE(item.randomize());
        }
        BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
        BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
        BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 1u);
        BOOST_REQUIRE_EQUAL(item.randomize_with_cache_hits(), 1u);
      }

      // 2. Covered via randomize_with() (cache hit preserved during partial coverage) -> destroy cg + stack_poison()
      //    -> cached randomize_with() & randomize()
      {
        GoalItem item("uar_rw_item");
        run_scoped_cg_randomize_with(item);
        stack_poison();

        BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
        BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
        BOOST_REQUIRE_EQUAL(item.randomize_with_cache_misses(), 2u);
        BOOST_REQUIRE_EQUAL(item.randomize_with_cache_hits(), 3u);

        for (int i = 0; i < 4; ++i) {
          BOOST_REQUIRE(item.randomize());
        }
      }

      // 3. Mixed partial randomize() + finish randomize_with() -> destroy cg + stack_poison() -> randomize() & randomize_with()
      {
        GoalItem item("uar_mix_item");
        run_scoped_cg_mixed(item);
        stack_poison();

        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
        BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
      }

      // 4. Two registered goals: outer cg2 remains uncovered while inner stack-scoped cg1 completes & is destroyed
      {
        GoalItem item("uar_two_goals");
        GoalCG cg2("outer_cg2");
        cg2.v.bind(item.x);
        item.goal(cg2);

        run_scoped_cg_two_bin_rw(item);
        stack_poison();

        for (int i = 0; i < 3 && !cg2.covered(); ++i) {
          BOOST_REQUIRE(item.randomize());
          cg2.sample();
        }
        BOOST_REQUIRE(cg2.covered());
        BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(enable_multithreading_and_get_inactive_softs_after_failed_size_unwind) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      // 1. nextCov() failedSize() unwind -> enable_multithreading() -> immediate getInactiveSofts() + finish covers
      {
        Variable<unsigned> x, y, s0, sz;
        rand_vec<unsigned> v0(nullptr);
        Generator gen;
        gen.addVecId(v0.id());

        gen(x < 16u)(x == 7u)(y < 16u)(s0 == 5u)(sz >= 2u && sz <= 3u);
        gen(v0().size() == sz)(foreach(v0(), v0()[_i] >= 1u && v0()[_i] <= 2u))(v0().sum() == s0);
        gen.soft("s_x", x == 3u);
        gen.soft("s_sz", sz == 2u);
        gen.cover("c_v0", v0().size() == 3u);
        gen.cover("c_y", y == 11u);

        BOOST_REQUIRE(gen.nextCov());
        BOOST_REQUIRE_EQUAL(v0.size(), 3u);
        BOOST_REQUIRE_EQUAL(gen[x], 7u);
        std::size_t const cov_before = gen.coveredSet().size();
        BOOST_REQUIRE(cov_before >= 1u);

        gen.enable_multithreading();
        std::vector<std::string> inact = gen.getInactiveSofts();
        BOOST_REQUIRE(std::find(inact.begin(), inact.end(), "s_x") != inact.end());
        BOOST_REQUIRE_EQUAL(gen.coveredSet().size(), cov_before);

        for (int i = 0; i < 4 && !gen.isCovered(); ++i) {
          BOOST_REQUIRE(gen.nextCov());
          BOOST_REQUIRE_EQUAL(v0.size(), 3u);
          BOOST_REQUIRE_EQUAL(gen[x], 7u);
        }
        BOOST_REQUIRE(gen.isCovered());
        BOOST_REQUIRE(gen.next());
      }

      // 2. next() failedSize() unwind -> enable_multithreading() -> immediate getInactiveSofts()
      {
        Variable<unsigned> x, s0, sz;
        rand_vec<unsigned> v0(nullptr);
        Generator gen;
        gen.addVecId(v0.id());

        gen(x < 16u)(x == 7u)(s0 == 5u)(sz >= 2u && sz <= 3u);
        gen(v0().size() == sz)(foreach(v0(), v0()[_i] >= 1u && v0()[_i] <= 2u))(v0().sum() == s0);
        gen.soft("s_x", x == 3u);
        gen.soft("s_sz", sz == 2u);

        BOOST_REQUIRE(gen.next());
        BOOST_REQUIRE_EQUAL(v0.size(), 3u);
        BOOST_REQUIRE_EQUAL(gen[x], 7u);

        gen.enable_multithreading();
        std::vector<std::string> inact = gen.getInactiveSofts();
        BOOST_REQUIRE(std::find(inact.begin(), inact.end(), "s_x") != inact.end());
        BOOST_REQUIRE(gen.next());
      }

      // 3. External constraint change before enable_multithreading() -> getInactiveSofts() rebuilds cleanly
      {
        Variable<unsigned> x, s0, sz;
        rand_vec<unsigned> v0(nullptr);
        Generator gen;
        gen.addVecId(v0.id());

        gen(x < 16u)(s0 == 5u)(sz >= 2u && sz <= 3u);
        gen(v0().size() == sz)(foreach(v0(), v0()[_i] >= 1u && v0()[_i] <= 2u))(v0().sum() == s0);
        gen.soft("s_x", x == 3u);
        gen.cover("c_v0", v0().size() == 3u);

        BOOST_REQUIRE(gen.nextCov());
        gen("c_x7", x == 7u);
        gen.enable_multithreading();
        std::vector<std::string> inact = gen.getInactiveSofts();
        BOOST_REQUIRE(std::find(inact.begin(), inact.end(), "s_x") != inact.end());
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(symmetric_covergroup_detachment_overlap_shared_unsampled_and_lifetimes) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      // 1. OverlapCG (v == 2u, v < 4u): covered via randomize() + sample() with unrecorded cover name,
      //    destroyed + stack_poison() -> randomize(), randomize_with(), inactive_soft_constraints(), analyse_contradiction()
      {
        SoftGoalItem item("overlap_item");
        run_overlap_cg(item);
        stack_poison();

        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_EQUAL(item.x, 5u);
        BOOST_REQUIRE(item.inactive_soft_constraints().empty());
        BOOST_REQUIRE(item.analyse_contradiction().empty());

        BOOST_REQUIRE(item.randomize_with(item.x() == 9u));
        BOOST_REQUIRE_EQUAL(item.x, 9u);
        std::vector<std::string> inact = item.inactive_soft_constraints();
        BOOST_REQUIRE((inact == std::vector<std::string>{"overlap_item.s_x#0"}));
        BOOST_REQUIRE(item.analyse_contradiction().empty());
      }

      // 2. Multi-goal cg_outer + stack-scoped cg_inner sharing bin v == 2u:
      //    cg_inner covered via randomize_with() + sample(), destroyed + stack_poison() -> item.randomize() finishes cg_outer
      {
        GoalItem item("shared_outer_item");
        GoalCG cg_outer("shared_cg_outer");
        cg_outer.v.bind(item.x);
        item.goal(cg_outer);

        BOOST_REQUIRE(item.randomize());
        cg_outer.sample();

        run_inner_shared_cg(item);
        stack_poison();

        for (int i = 0; i < 4 && !cg_outer.covered(); ++i) {
          BOOST_REQUIRE(item.randomize());
          cg_outer.sample();
        }
        BOOST_REQUIRE(cg_outer.covered());
        BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
      }

      // 3. Unsampled / uncovered stack-scoped crv_covergroup registered via goal(cg) and destroyed + stack_poison()
      for (bool run_solves : {false, true}) {
        SoftGoalItem item(run_solves ? "unsampled_item_1" : "unsampled_item_0");
        run_unsampled_cg(item, run_solves);
        stack_poison();

        BOOST_REQUIRE(item.inactive_soft_constraints().empty());
        BOOST_REQUIRE(item.analyse_contradiction().empty());
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_EQUAL(item.x, 5u);
        BOOST_REQUIRE(item.randomize_with(item.x() == 11u));
        BOOST_REQUIRE_EQUAL(item.x, 11u);
        std::string const expected_inact =
            std::string(run_solves ? "unsampled_item_1" : "unsampled_item_0") + ".s_x#0";
        BOOST_REQUIRE((item.inactive_soft_constraints() == std::vector<std::string>{expected_inact}));
        BOOST_REQUIRE(item.analyse_contradiction().empty());
      }

      // 4. Subclass member order (cg after/before crv_variable) and wrapper struct destruction orders
      {
        SubItemCgAfter sub_after("sub_after");
        BOOST_REQUIRE(sub_after.randomize());
        sub_after.cg.sample();
      }
      {
        SubItemCgBefore sub_before("sub_before");
        BOOST_REQUIRE(sub_before.randomize());
        sub_before.cg.sample();
      }
      {
        WrapperItemFirst w_item_first;
        BOOST_REQUIRE(w_item_first.item.randomize());
        w_item_first.cg.sample();
      }
      {
        WrapperCgFirst w_cg_first;
        BOOST_REQUIRE(w_cg_first.item.randomize());
        w_cg_first.cg.sample();
      }

      // 5. Repeated item.goal(cg) (uncovered + covered) & cg shared/rebound across two items (item1 dies before cg, item2 outlives cg)
      {
        GoalCG shared_cg("shared_rebound_cg");
        GoalItem item2("rebound_item2");
        {
          GoalItem item1("rebound_item1");
          shared_cg.v.bind(item1.x);
          item1.goal(shared_cg);
          item1.goal(shared_cg);  // repeated registration while uncovered
          BOOST_REQUIRE(item1.randomize());
          shared_cg.sample();

          shared_cg.v.bind(item2.x);
          item2.goal(shared_cg);  // two items registered simultaneously on shared_cg
          BOOST_REQUIRE(item2.randomize());
          shared_cg.sample();
          // item1 dies first while still registered with shared_cg
        }
        while (!shared_cg.covered()) {
          BOOST_REQUIRE(item2.randomize());
          shared_cg.sample();
        }
        item2.goal(shared_cg);  // repeated goal(cg) after 100% covered detaches cleanly
        // shared_cg dies before item2
      }
      {
        GoalItem item1("rebound_live_item1");
        GoalItem item2("rebound_live_item2");
        {
          GoalCG cg("shared_cg_dies_first");
          cg.v.bind(item1.x);
          item1.goal(cg);
          cg.v.bind(item2.x);
          item2.goal(cg);
          BOOST_REQUIRE(item1.randomize());
          BOOST_REQUIRE(item2.randomize());
          // cg dies while attached to both item1 and item2
        }
        stack_poison();
        BOOST_REQUIRE(item1.randomize());
        BOOST_REQUIRE(item2.randomize_with(item2.y() < 16u));
      }

      // 6. Cloned copies (copy-constructed) destroyed both before and after cg dies,
      //    item destroyed after prune_covered_goals_() while cg lives, and copy-assignment detachment
      {
        GoalItem item("clone_src");
        GoalCG* raw_cg = new GoalCG("clone_cg");
        raw_cg->v.bind(item.x);
        item.goal(*raw_cg);
        BOOST_REQUIRE(item.randomize());

        GoalItem copy_dies_after(item);
        // Destroy copy_dies_before while cg is alive -> must NOT detach item's goal!
        {
          GoalItem copy_dies_before(item);
          (void)copy_dies_before;
          BOOST_REQUIRE(!raw_cg->covered());
        }
        BOOST_REQUIRE(item.randomize());
        raw_cg->sample();

        delete raw_cg;
        stack_poison();
        BOOST_REQUIRE(item.randomize());
        (void)copy_dies_after;
        // copy_dies_after goes out of scope after raw_cg is already destroyed
      }
      {
        OverlapCG live_cg("pruned_live_cg");
        {
          SoftGoalItem item("pruned_item");
          live_cg.v.bind(item.x);
          item.goal(live_cg);
          BOOST_REQUIRE(item.randomize());
          live_cg.sample();
          BOOST_REQUIRE(live_cg.covered());
          BOOST_REQUIRE(item.randomize());  // prune_covered_goals_() prunes live_cg
          // item dies here while live_cg still lives
        }
        // live_cg dies cleanly without UAF on pruned item
      }
      {
        GoalCG cg("assign_cg_rhs_first");
        GoalItem lhs("assign_lhs_1");
        cg.v.bind(lhs.x);
        lhs.goal(cg);
        BOOST_REQUIRE(lhs.randomize());
        {
          GoalItem rhs("assign_rhs_1");
          BOOST_REQUIRE(rhs.randomize());
          lhs = rhs;  // must detach lhs from cg cleanly and mark lhs cloned_
          // rhs destroyed before lhs
        }
        stack_poison();
        BOOST_REQUIRE(!cg.covered());
      }
      {
        GoalItem rhs("assign_rhs_2");
        GoalCG cg_rhs("assign_cg_rhs_2");
        cg_rhs.v.bind(rhs.x);
        rhs.goal(cg_rhs);
        rhs = rhs;  // self-assignment must preserve goal attachment and randomizability
        {
          GoalItem lhs("assign_lhs_2");
          {
            GoalCG cg_lhs("assign_cg_lhs_2");
            cg_lhs.v.bind(lhs.x);
            lhs.goal(cg_lhs);
            BOOST_REQUIRE(lhs.randomize());
            lhs = rhs;  // must detach lhs from cg_lhs cleanly
            // cg_lhs destroyed while lhs and rhs live
          }
          stack_poison();
          // lhs destroyed before rhs
        }
        stack_poison();
        for (int i = 0; i < 3 && !cg_rhs.covered(); ++i) {
          BOOST_REQUIRE(rhs.randomize());
          cg_rhs.sample();
        }
        BOOST_REQUIRE(cg_rhs.covered());
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(goal_refresh_preserves_unsampled_interleaved_and_multi_hit_bins) {
  CacheTestGuard guard;

  for (char const* backend : {"Z3", "Cudd"}) {
    for (bool sv_sign : {false, true}) {
      set_solver_backend(backend);
      set_sv_signedness(sv_sign);

      // 1. Wide-domain (x < 1000000u) Case A: single ThreeBinCG, 1 unsampled randomize(), then 4 sampled cycles
      {
        WideItem item("wide_case_a_r");
        ThreeBinCG cg("wide_cg_a_r");
        cg.v.bind(item.x);
        item.goal(cg);

        BOOST_REQUIRE(item.randomize());  // unsampled
        BOOST_REQUIRE(!cg.covered());

        for (int i = 0; i < 4; ++i) {
          BOOST_REQUIRE(item.randomize());
          cg.sample();
        }
        BOOST_REQUIRE(cg.covered());
      }
      {
        WideItem item("wide_case_a_rw");
        ThreeBinCG cg("wide_cg_a_rw");
        cg.v.bind(item.x);
        item.goal(cg);

        BOOST_REQUIRE(item.randomize_with(item.y() < 16u));  // unsampled
        BOOST_REQUIRE(!cg.covered());

        for (int i = 0; i < 4; ++i) {
          BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
          cg.sample();
        }
        BOOST_REQUIRE(cg.covered());
      }

      // 2. Wide-domain (x < 1000000u) Case B: 1D ThreeBinCG cg_outer + stack-scoped InnerSharedCG cg_inner
      //    without cg_outer.sample() inside run_wide_inner_shared_rw
      {
        WideItem item("wide_case_b");
        ThreeBinCG cg_outer("wide_cg_outer_b");
        cg_outer.v.bind(item.x);
        item.goal(cg_outer);

        BOOST_REQUIRE(item.randomize());
        cg_outer.sample();

        run_wide_inner_shared_rw(item);
        stack_poison();

        for (int i = 0; i < 4 && !cg_outer.covered(); ++i) {
          BOOST_REQUIRE(item.randomize());
          cg_outer.sample();
        }
        BOOST_REQUIRE(cg_outer.covered());
      }

      // 3. cg_outer + cg_inner interleaved using item.randomize() (wide-domain and soft-constrained)
      {
        WideItem item("wide_interleave_r");
        ThreeBinCG cg_outer("wide_outer_r");
        cg_outer.v.bind(item.x);
        item.goal(cg_outer);

        BOOST_REQUIRE(item.randomize());
        cg_outer.sample();

        run_wide_inner_shared_randomize(item);
        stack_poison();

        for (int i = 0; i < 4 && !cg_outer.covered(); ++i) {
          BOOST_REQUIRE(item.randomize());
          cg_outer.sample();
        }
        BOOST_REQUIRE(cg_outer.covered());
      }
      {
        SoftGoalItem item("soft_interleave_r");
        TwoBinOuterCG cg_outer("soft_outer_r");
        cg_outer.v.bind(item.x);
        item.goal(cg_outer);

        BOOST_REQUIRE(item.randomize());
        cg_outer.sample();

        run_soft_inner_shared_randomize(item);
        stack_poison();

        for (int i = 0; i < 4 && !cg_outer.covered(); ++i) {
          BOOST_REQUIRE(item.randomize());
          cg_outer.sample();
        }
        BOOST_REQUIRE(cg_outer.covered());
      }

      // 4. hit_minimum == 2 (crv_bin(v() == 2u, 2)) on wide domain via both randomize() and randomize_with()
      {
        WideItem item("multi_hit_r");
        MultiHitCG cg("multi_hit_cg_r");
        cg.v.bind(item.x);
        item.goal(cg);

        BOOST_REQUIRE(item.randomize());
        cg.sample();
        BOOST_REQUIRE(!cg.covered());

        BOOST_REQUIRE(item.randomize());
        cg.sample();
        BOOST_REQUIRE(cg.covered());
      }
      {
        WideItem item("multi_hit_rw");
        MultiHitCG cg("multi_hit_cg_rw");
        cg.v.bind(item.x);
        item.goal(cg);

        BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
        cg.sample();
        BOOST_REQUIRE(!cg.covered());

        BOOST_REQUIRE(item.randomize_with(item.y() < 16u));
        cg.sample();
        BOOST_REQUIRE(cg.covered());
      }
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()

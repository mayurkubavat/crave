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

#include <crave/experimental/Variable.hpp>
#include <crave/experimental/Coverage.hpp>
#include <crave/experimental/Constraint.hpp>
#include <crave/experimental/Experimental.hpp>

#include <algorithm>
#include <ostream>
#include <set>
using std::ostream;

using namespace crave;

CRAVE_BETTER_ENUM(cm_color_enum, CM_RED = 10, CM_GREEN = 20, CM_BLUE = 30);

BOOST_FIXTURE_TEST_SUITE(ConstraintManagement, Context_Fixture)

struct Item : public crv_sequence_item {
  crv_variable<unsigned int> a;
  crv_variable<unsigned int> b;

  crv_constraint sum{a() + b() == 4};
  crv_constraint product{a() * b() == 4};
  crv_constraint range{a() < 10, b() < 10};
  crv_constraint x{a() != 2};

  Item(crv_object_name) {}
  friend ostream& operator<<(ostream& os, const Item& it) {
    os << it.a << " " << it.b;
    return os;
  }
};

struct Item1 : public crv_sequence_item {
  Item item = {"Item"};

  crv_variable<unsigned int> c;
  crv_variable<unsigned int> d;

  crv_constraint eq = {c() == item.a() + 1, d() == item.b() + 1};

  Item1(crv_object_name) {}
  friend ostream& operator<<(ostream& os, const Item1& it) {
    os << it.c << " " << it.c << " -- " << it.item;
    return os;
  }
};

BOOST_AUTO_TEST_CASE(test1) {
  Item it("Item");

  BOOST_REQUIRE(!it.randomize());
  BOOST_REQUIRE(it.sum.active());
  BOOST_REQUIRE(it.product.active());
  BOOST_REQUIRE(it.x.active());

  it.sum.deactivate();
  BOOST_REQUIRE(it.randomize());
  BOOST_REQUIRE(!it.sum.active());
  BOOST_REQUIRE(it.product.active());
  BOOST_REQUIRE(it.x.active());
  std::cout << it << std::endl;

  it.product.deactivate();
  BOOST_REQUIRE(it.randomize());
  BOOST_REQUIRE(!it.sum.active());
  BOOST_REQUIRE(!it.product.active());
  BOOST_REQUIRE(it.x.active());
  std::cout << it << std::endl;

  it.sum.activate();
  BOOST_REQUIRE(it.randomize());
  BOOST_REQUIRE(it.sum.active());
  BOOST_REQUIRE(!it.product.active());
  BOOST_REQUIRE(it.x.active());
  std::cout << it << std::endl;

  it.product.activate();
  BOOST_REQUIRE(!it.randomize());
  BOOST_REQUIRE(it.sum.active());
  BOOST_REQUIRE(it.product.active());
  BOOST_REQUIRE(it.x.active());

  it.x.deactivate();
  BOOST_REQUIRE(it.randomize());
  BOOST_REQUIRE(it.sum.active());
  BOOST_REQUIRE(it.product.active());
  BOOST_REQUIRE(!it.x.active());

  std::cout << it << std::endl;
  BOOST_REQUIRE(it.a == 2 && it.b == 2);
}

BOOST_AUTO_TEST_CASE(test2) {
  Item1 it("Item1");

  BOOST_REQUIRE(!it.item.randomize());
  BOOST_REQUIRE(!it.randomize());

  it.item.x.deactivate();

  BOOST_REQUIRE(it.item.randomize());
  std::cout << it.item << std::endl;
  BOOST_REQUIRE(it.item.a == 2 && it.item.b == 2);

  BOOST_REQUIRE(it.randomize());
  std::cout << it << std::endl;
  BOOST_REQUIRE(it.c == 3 && it.d == 3);

  it.item.x.activate();

  BOOST_REQUIRE(!it.randomize());
  BOOST_REQUIRE(!it.item.randomize());
}

struct rand_mode_inner : public crv_sequence_item {
  rand_mode_inner(crv_object_name) {}
  crv_variable<unsigned> y;
  crv_array<unsigned char, 2> arr;
  crv_constraint con{y() < 1000u, arr[0]() != arr[1]()};
};

struct rand_mode_item : public crv_sequence_item {
  rand_mode_item(crv_object_name) : inner("inner") {}
  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  rand_mode_inner inner;
  crv_constraint con{a() < 100u, b() == a() + 1u};
};

BOOST_AUTO_TEST_CASE(rand_mode_variable) {
  rand_mode_item item("item");
  BOOST_REQUIRE(item.a.rand_mode());

  // a variable with rand_mode off keeps its value and is a state variable in the constraints
  item.a = 41;
  item.a.rand_mode(false);
  BOOST_REQUIRE(!item.a.rand_mode());
  for (unsigned i = 0; i < 5; ++i) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_EQUAL(item.a, 41u);
    BOOST_REQUIRE_EQUAL(item.b, 42u);
  }

  // the constraints use its current value
  item.a = 7;
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.a, 7u);
  BOOST_REQUIRE_EQUAL(item.b, 8u);
  BOOST_REQUIRE(item.randomize_with(item.b() > 5u));
  BOOST_REQUIRE_EQUAL(item.a, 7u);
  BOOST_REQUIRE_EQUAL(item.b, 8u);
  BOOST_REQUIRE(!item.randomize_with(item.b() > 8u));
  BOOST_REQUIRE_EQUAL(item.a, 7u);

  // randomize() fails if its value violates a constraint
  item.a = 200;
  BOOST_REQUIRE(!item.randomize());
  BOOST_REQUIRE_EQUAL(item.a, 200u);

  item.a.rand_mode(true);
  BOOST_REQUIRE(item.a.rand_mode());
  std::set<unsigned> values;
  for (unsigned i = 0; i < 10; ++i) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_LT(item.a, 100u);
    BOOST_REQUIRE_EQUAL(item.b, item.a + 1);
    values.insert(item.a);
  }
  BOOST_REQUIRE_GT(values.size(), 1u);
}

BOOST_AUTO_TEST_CASE(rand_mode_object) {
  rand_mode_item item("item");
  item.a = 3;
  item.b = 4;
  item.inner.y = 2000;
  item.inner.arr[0] = 5;
  item.inner.arr[1] = 5;

  // rand_mode of an object applies to all variables in its hierarchy, so the constraints fail
  item.rand_mode(false);
  BOOST_REQUIRE(!item.a.rand_mode());
  BOOST_REQUIRE(!item.inner.y.rand_mode());
  BOOST_REQUIRE(!item.inner.arr[1].rand_mode());
  BOOST_REQUIRE(!item.randomize());

  item.inner.y = 999;
  item.inner.arr.rand_mode(true);
  for (unsigned i = 0; i < 5; ++i) {
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_EQUAL(item.a, 3u);
    BOOST_REQUIRE_EQUAL(item.b, 4u);
    BOOST_REQUIRE_EQUAL(item.inner.y, 999u);
    BOOST_REQUIRE_NE(item.inner.arr[0], item.inner.arr[1]);
  }

  item.rand_mode(true);
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.b, item.a + 1);
  BOOST_REQUIRE_LT(item.inner.y, 1000u);
}

struct diag_inner_item : public crv_sequence_item {
  diag_inner_item(crv_object_name) {}
  crv_variable<unsigned> v;
  crv_constraint bound{"bound", v() < 10u};
};

struct diag_item : public crv_sequence_item {
  diag_item(crv_object_name) : sub("sub") {}
  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  diag_inner_item sub;

  crv_constraint range{"range", a() < 10u, b() < 10u};
  crv_constraint sum{"sum", a() + b() == 4u};
  crv_constraint product{"product", a() * b() == 4u};
  crv_constraint not_two{"not_two", a() != 2u};
  crv_soft_constraint prefer_one{"prefer_one", a() == 1u};
};

BOOST_AUTO_TEST_CASE(analyse_contradiction_hard_and_with) {
  diag_item item("it");

  // Can inspect contradictions even before randomize() is called.
  auto pre_conflicts = item.analyse_contradiction();
  BOOST_REQUIRE_GE(pre_conflicts.size(), 1u);

  BOOST_REQUIRE(!item.randomize());
  auto conflicts = item.analyse_contradiction();
  BOOST_REQUIRE_GE(conflicts.size(), 1u);
  std::set<std::string> mus(conflicts[0].begin(), conflicts[0].end());
  BOOST_REQUIRE_EQUAL(mus.count("it.sum#0"), 1u);
  BOOST_REQUIRE_EQUAL(mus.count("it.product#0"), 1u);
  BOOST_REQUIRE_EQUAL(mus.count("it.not_two#0"), 1u);
  // In 32-bit bitvector arithmetic, (2^31 + 2) satisfies sum, product and not_two
  // unless bounded by range#0 (a < 10) or range#1 (b < 10).
  BOOST_REQUIRE(mus.count("it.range#0") == 1u || mus.count("it.range#1") == 1u);

  // Deactivate not_two: now a==2, b==2 is solvable, and soft constraint prefer_one (a==1) is dropped.
  item.not_two.deactivate();
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.a, 2u);
  BOOST_REQUIRE_EQUAL(item.b, 2u);
  BOOST_REQUIRE(item.analyse_contradiction().empty());
  auto dropped_soft = item.inactive_soft_constraints();
  BOOST_REQUIRE_EQUAL(dropped_soft.size(), 1u);
  BOOST_REQUIRE_EQUAL(dropped_soft[0], "it.prefer_one#0");

  // Inline randomize_with() contradiction reports the inline clause alongside conflicting member constraints.
  BOOST_REQUIRE(!item.randomize_with(item.a() > 5u));
  auto with_conflicts = item.analyse_contradiction();
  BOOST_REQUIRE_GE(with_conflicts.size(), 1u);
  bool saw_inline = false;
  for (auto const& group : with_conflicts) {
    for (auto const& cname : group) {
      if (cname == "it.randomize_with#0") saw_inline = true;
    }
  }
  BOOST_REQUIRE(saw_inline);
}

BOOST_AUTO_TEST_CASE(analyse_contradiction_hierarchy) {
  diag_item item("top");
  item.not_two.deactivate();
  BOOST_REQUIRE(item.randomize());

  // Force a cross-hierarchy conflict between parent inline constraint and child sub.bound.
  BOOST_REQUIRE(!item.randomize_with(item.sub.v() >= 20u));
  auto conflicts = item.analyse_contradiction();
  BOOST_REQUIRE_EQUAL(conflicts.size(), 1u);
  std::set<std::string> mus(conflicts[0].begin(), conflicts[0].end());
  std::set<std::string> expected{"top.sub.bound#0", "top.randomize_with#0"};
  BOOST_REQUIRE(mus == expected);
}

struct dyn_soft_item : public crv_sequence_item {
  dyn_soft_item(crv_object_name) {}
  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_constraint bound{"bound", a() < 100u};
  crv_soft_constraint s1{"s1", a() == 10u};
  crv_soft_constraint s2{"s2", a() == 20u};
  crv_soft_constraint s3{"s3", b() == 55u};
};

BOOST_AUTO_TEST_CASE(dynamic_rand_mode_soft_experimental) {
  struct BackendGuard {
    ~BackendGuard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    dyn_soft_item item("it");
    item.a = 10;
    item.a.rand_mode(false);

    // 1. rand_mode(false) value 10 conflicts with higher-priority s2 (a==20): s2 drops, s1 & s3 hold
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_EQUAL(item.a, 10u);
    BOOST_REQUIRE_EQUAL(item.b, 55u);
    {
      auto inact = item.inactive_soft_constraints();
      std::sort(inact.begin(), inact.end());
      BOOST_REQUIRE(inact == std::vector<std::string>({"it.s2#0"}));
    }

    // 2. Change disabled value to 30 without rebuilding: both s1 & s2 drop, s3 remains active
    item.a = 30;
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_EQUAL(item.a, 30u);
    BOOST_REQUIRE_EQUAL(item.b, 55u);
    {
      auto inact = item.inactive_soft_constraints();
      std::sort(inact.begin(), inact.end());
      BOOST_REQUIRE(inact == std::vector<std::string>({"it.s1#0", "it.s2#0"}));
    }

    // 3. Change disabled value back to 20: s2 re-activates!
    item.a = 20;
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_EQUAL(item.a, 20u);
    BOOST_REQUIRE_EQUAL(item.b, 55u);
    {
      auto inact = item.inactive_soft_constraints();
      std::sort(inact.begin(), inact.end());
      BOOST_REQUIRE(inact == std::vector<std::string>({"it.s1#0"}));
    }

    // 4. Re-enable rand_mode: s2 wins over s1
    item.a.rand_mode(true);
    BOOST_REQUIRE(item.randomize());
    BOOST_REQUIRE_EQUAL(item.a, 20u);
    BOOST_REQUIRE_EQUAL(item.b, 55u);
    {
      auto inact = item.inactive_soft_constraints();
      std::sort(inact.begin(), inact.end());
      BOOST_REQUIRE(inact == std::vector<std::string>({"it.s1#0"}));
    }
  }
}

BOOST_AUTO_TEST_CASE(bypass_constraint_analysis_experimental) {
  struct Guard {
    bool prev;
    Guard() : prev(VariableDefaultSolver::bypass_constraint_analysis) {
      VariableDefaultSolver::bypass_constraint_analysis = true;
    }
    ~Guard() { VariableDefaultSolver::bypass_constraint_analysis = prev; }
  } guard;

  dyn_soft_item item("it");
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.a, 20u);
  BOOST_REQUIRE_EQUAL(item.b, 55u);
}

struct inline_soft_item : public crv_sequence_item {
  inline_soft_item(crv_object_name) {}
  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_constraint bound{"bound", a() < 100u};
  crv_soft_constraint s_mem{"s_mem", a() == 10u};
};

BOOST_AUTO_TEST_CASE(inline_soft_randomize_with_priority_and_diagnostics) {
  inline_soft_item item("it");

  // Single inline soft overrides member crv_soft_constraint
  BOOST_REQUIRE(item.randomize_with(soft(item.a() == 20u)));
  BOOST_REQUIRE_EQUAL(item.a, 20u);
  BOOST_REQUIRE(item.inactive_soft_constraints() == std::vector<std::string>({"it.s_mem#0"}));

  // Multi-clause comma-separated inline softs: last-wins (randomize_with#1 overrides #0 & s_mem)
  BOOST_REQUIRE(item.randomize_with(crave::soft(item.a() == 20u), crave::soft(item.a() == 30u), soft(item.b() == 77u)));
  BOOST_REQUIRE_EQUAL(item.a, 30u);
  BOOST_REQUIRE_EQUAL(item.b, 77u);
  {
    auto inact = item.inactive_soft_constraints();
    std::sort(inact.begin(), inact.end());
    BOOST_REQUIRE(inact == std::vector<std::string>({"it.randomize_with#0", "it.s_mem#0"}));
  }

  // Mixed hard + soft: hard randomize_with#0 blocks soft randomize_with#1, so s_mem (a==10) stays active
  BOOST_REQUIRE(item.randomize_with(item.a() < 25u, soft(item.a() == 30u), soft(item.b() == 88u)));
  BOOST_REQUIRE_EQUAL(item.a, 10u);
  BOOST_REQUIRE_EQUAL(item.b, 88u);
  BOOST_REQUIRE(item.inactive_soft_constraints() == std::vector<std::string>({"it.randomize_with#1"}));

  // Hard contradiction with inline soft present: analyse_contradiction() excludes inline soft and preserves indices
  BOOST_REQUIRE(!item.randomize_with(item.a() > 200u, soft(item.a() == 50u), item.b() == 5u));
  auto conflicts = item.analyse_contradiction();
  BOOST_REQUIRE_GE(conflicts.size(), 1u);
  std::set<std::string> mus(conflicts[0].begin(), conflicts[0].end());
  BOOST_REQUIRE_EQUAL(mus.count("it.bound#0"), 1u);
  BOOST_REQUIRE_EQUAL(mus.count("it.randomize_with#0"), 1u);
  BOOST_REQUIRE_EQUAL(mus.count("it.randomize_with#1"), 0u);
}

struct crv_constraint_inline_soft_item : public crv_sequence_item {
  crv_constraint_inline_soft_item(crv_object_name) {}
  crv_variable<unsigned> a;
  crv_variable<unsigned> b;
  crv_constraint c{"c", a() < 10u, soft(a() == 5u)};
};

BOOST_AUTO_TEST_CASE(inline_soft_crv_constraint_and_solve) {
  crv_constraint_inline_soft_item item("it");
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.a, 5u);
  BOOST_REQUIRE(item.inactive_soft_constraints().empty());

  // Assignment (=) and conjunction (&=) with inline soft
  item.c = {item.a() < 10u, crave::soft(item.a() == 20u)};
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_LT(item.a, 10u);
  BOOST_REQUIRE(item.inactive_soft_constraints() == std::vector<std::string>({"it.c#1"}));

  item.c &= {crave::soft(item.a() == 7u), crave::soft(item.b() == 9u)};
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.a, 7u);
  BOOST_REQUIRE_EQUAL(item.b, 9u);

  // crave::solve(...) with inline soft
  crv_variable<unsigned> x("x");
  crv_variable<unsigned> y("y");
  BOOST_REQUIRE(crave::solve(x() < 10u, crave::soft(x() == 20u), crave::soft(y() == 33u)));
  BOOST_REQUIRE_LT(x, 10u);
  BOOST_REQUIRE_EQUAL(y, 33u);

  BOOST_REQUIRE(crave::solve(x() < 10u, crave::soft(x() == 4u), crave::soft(x() == 8u)));
  BOOST_REQUIRE_EQUAL(x, 8u);

  // crave::soft() is rejected as a coverage bin
  crv_coverpoint cp("cp");
  BOOST_REQUIRE_THROW(cp.add_simple_bins(expression_list(crave::soft(x() == 1u))), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(inline_soft_rand_mode_and_reference) {
  struct BackendGuard {
    ~BackendGuard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    inline_soft_item item("it");
    unsigned ext = 44;

    // Inline soft with crave::reference across changing ext
    BOOST_REQUIRE(item.randomize_with(crave::soft(item.a() == crave::reference(ext))));
    BOOST_REQUIRE_EQUAL(item.a, 44u);

    ext = 250;  // conflicts with bound (a < 100) -> drops inline soft, s_mem (a == 10) wins
    BOOST_REQUIRE(item.randomize_with(crave::soft(item.a() == crave::reference(ext))));
    BOOST_REQUIRE_EQUAL(item.a, 10u);
    BOOST_REQUIRE(item.inactive_soft_constraints() == std::vector<std::string>({"it.randomize_with#0"}));

    // rand_mode(false) + inline soft
    item.a = 42;
    item.a.rand_mode(false);
    BOOST_REQUIRE(item.randomize_with(crave::soft(item.a() == 5u), crave::soft(item.b() == 99u)));
    BOOST_REQUIRE_EQUAL(item.a, 42u);
    BOOST_REQUIRE_EQUAL(item.b, 99u);
    {
      auto inact = item.inactive_soft_constraints();
      std::sort(inact.begin(), inact.end());
      BOOST_REQUIRE(inact == std::vector<std::string>({"it.randomize_with#0", "it.s_mem#0"}));
    }
  }
}

struct inline_soft_vec_item : public crv_sequence_item {
  inline_soft_vec_item(crv_object_name) {}
  crv_vector<unsigned> v;
  crv_constraint c_size{"c_size", v().size() == 3u};
  crv_constraint c_elem{"c_elem", foreach (v(), v()[_i] < 10u)};
};

BOOST_AUTO_TEST_CASE(inline_soft_vector_constraints) {
  inline_soft_vec_item item("it");

  // unique + sum inside soft(...)
  BOOST_REQUIRE(item.randomize_with(soft(unique(item.v())), soft(item.v().sum() == 6u)));
  BOOST_REQUIRE_EQUAL(item.v.size(), 3u);
  BOOST_REQUIRE_NE(item.v[0], item.v[1]);
  BOOST_REQUIRE_NE(item.v[0], item.v[2]);
  BOOST_REQUIRE_NE(item.v[1], item.v[2]);
  BOOST_REQUIRE_EQUAL(item.v[0] + item.v[1] + item.v[2], 6u);

  // Conflicting vector softs: sum == 50 impossible with elem < 10 -> drops; foreach == 2 overrides unique
  BOOST_REQUIRE(item.randomize_with(soft(unique(item.v())), soft(foreach (item.v(), item.v()[_i] == 2u)),
                                    soft(item.v().sum() == 50u)));
  BOOST_REQUIRE_EQUAL(item.v.size(), 3u);
  BOOST_REQUIRE_EQUAL(item.v[0], 2u);
  BOOST_REQUIRE_EQUAL(item.v[1], 2u);
  BOOST_REQUIRE_EQUAL(item.v[2], 2u);
}

struct cm_child_item : public crv_sequence_item {
  cm_child_item(crv_object_name) {}
  crv_variable<unsigned> y;
  crv_variable<cm_color_enum> col;
  crv_constraint c_y{"c_y", y() == 77u};
  unsigned pre_count = 0;
  unsigned post_count = 0;
  void pre_randomize() override { ++pre_count; }
  void post_randomize() override { ++post_count; }
};

struct cm_parent_item : public crv_sequence_item {
  cm_parent_item(crv_object_name) : sub("sub") {}
  crv_variable<unsigned> x;
  crv_variable<bool> sel;
  cm_child_item sub;
  crv_constraint c_x{"c_x", x() == 11u};
  crv_constraint c_x2{"c_x2", x() < 100u};
  crv_soft_constraint s_x{"s_x", x() == 55u};
  crv_solve_before order{sel, x};
};

BOOST_AUTO_TEST_CASE(constraint_mode_hierarchy_and_enum) {
  cm_parent_item item("top");
  BOOST_REQUIRE(item.constraint_mode());
  BOOST_REQUIRE(item.c_x.constraint_mode());
  BOOST_REQUIRE(item.c_x2.constraint_mode());
  BOOST_REQUIRE(item.s_x.constraint_mode());
  BOOST_REQUIRE(item.order.constraint_mode());
  BOOST_REQUIRE(item.sub.constraint_mode());
  BOOST_REQUIRE(item.sub.c_y.constraint_mode());

  // Initially c_x forces x == 11, c_y forces y == 77, col must be legal enum
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.x, 11u);
  BOOST_REQUIRE_EQUAL(item.sub.y, 77u);
  BOOST_REQUIRE(item.sub.col == cm_color_enum::CM_RED || item.sub.col == cm_color_enum::CM_GREEN ||
                item.sub.col == cm_color_enum::CM_BLUE);

  // Disable single constraint via constraint_mode(false): parent stays true while c_x2/s_x/c_y remain active
  item.c_x.constraint_mode(false);
  BOOST_REQUIRE(!item.c_x.constraint_mode());
  BOOST_REQUIRE(!item.c_x.active());
  BOOST_REQUIRE(item.constraint_mode());
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.x, 55u);  // soft s_x now wins

  // Disable all constraints recursively via item.constraint_mode(false):
  // - pre_randomize/post_randomize STILL execute
  // - enum_internal & c_var_uniform remain intact!
  unsigned pre_before = item.sub.pre_count;
  unsigned post_before = item.sub.post_count;
  item.constraint_mode(false);
  BOOST_REQUIRE(!item.constraint_mode());
  BOOST_REQUIRE(!item.c_x2.constraint_mode());
  BOOST_REQUIRE(!item.s_x.constraint_mode());
  BOOST_REQUIRE(!item.order.constraint_mode());
  BOOST_REQUIRE(!item.sub.constraint_mode());
  BOOST_REQUIRE(!item.sub.c_y.constraint_mode());

  std::set<unsigned> x_vals;
  std::set<unsigned> y_vals;
  for (int i = 0; i < 12; ++i) {
    BOOST_REQUIRE(item.randomize());
    x_vals.insert(item.x);
    y_vals.insert(item.sub.y);
    // Must never fail or produce illegal enum value when constraint_mode(false)
    BOOST_REQUIRE(item.sub.col == cm_color_enum::CM_RED || item.sub.col == cm_color_enum::CM_GREEN ||
                  item.sub.col == cm_color_enum::CM_BLUE);
  }
  BOOST_REQUIRE_GT(x_vals.size(), 1u);
  BOOST_REQUIRE_GT(y_vals.size(), 1u);
  BOOST_REQUIRE_EQUAL(item.sub.pre_count, pre_before + 12u);
  BOOST_REQUIRE_EQUAL(item.sub.post_count, post_before + 12u);

  // Re-enabling a child constraint updates parent constraint_mode() and triggers rebuild
  item.sub.c_y.constraint_mode(true);
  BOOST_REQUIRE(item.sub.constraint_mode());
  BOOST_REQUIRE(item.constraint_mode());
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.sub.y, 77u);

  // Re-enable all constraints
  item.constraint_mode(true);
  BOOST_REQUIRE(item.c_x.constraint_mode());
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.x, 11u);
  BOOST_REQUIRE_EQUAL(item.sub.y, 77u);

  // Rebuild invalidation on contradiction analysis
  item.c_x2 = {item.x() > 100u};
  BOOST_REQUIRE(!item.randomize());
  BOOST_REQUIRE(!item.analyse_contradiction().empty());
  item.c_x2.constraint_mode(false);
  BOOST_REQUIRE(item.analyse_contradiction().empty());
  BOOST_REQUIRE(item.randomize());
  BOOST_REQUIRE_EQUAL(item.x, 11u);
}

struct cm_dist_sub_item : public crv_sequence_item {
  cm_dist_sub_item(crv_object_name) {}
  crv_variable<int> z;
  crv_constraint c_z_dist{"c_z_dist", dist(z(), make_distribution(range<int>(30, 32)))};
};

struct cm_dist_item : public crv_sequence_item {
  cm_dist_item(crv_object_name) : sub("sub") {}
  crv_variable<int> x;
  crv_variable<bool> b;
  crv_array<int, 2> arr;
  cm_dist_sub_item sub;
  crv_constraint c_dist{"c_dist", dist(x(), make_distribution(range<int>(10, 12)))};
  crv_constraint c_dist2{"c_dist2", dist(x(), make_distribution(range<int>(20, 22)))};
  crv_constraint c_b_dist{"c_b_dist", dist(b(), distribution<bool>::create(1.0))};
  crv_constraint c_arr_dist{"c_arr_dist", dist(arr[0](), make_distribution(range<int>(100, 102)))};
};

struct cm_plain_item : public crv_sequence_item {
  cm_plain_item(crv_object_name) {}
  crv_variable<int> x;
};

BOOST_AUTO_TEST_CASE(constraint_mode_with_dist_and_randomize_with) {
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

      // 1) Disable dist before first randomize() (both constraint_mode(false) and deactivate())
      {
        cm_dist_item item("d1");
        item.x = 42;
        item.c_dist.constraint_mode(false);
        item.c_dist2.constraint_mode(false);
        std::set<int> seen;
        for (int i = 0; i < 16; ++i) {
          BOOST_REQUIRE(item.randomize());
          seen.insert(item.x);
        }
        BOOST_REQUIRE_GT(seen.size(), 1u);

        cm_dist_item item_deact("d1_deact");
        item_deact.x = 77;
        item_deact.c_dist.deactivate();
        item_deact.c_dist2.deactivate();
        std::set<int> seen_deact;
        for (int i = 0; i < 16; ++i) {
          BOOST_REQUIRE(item_deact.randomize());
          seen_deact.insert(item_deact.x);
        }
        BOOST_REQUIRE_GT(seen_deact.size(), 1u);
      }

      // 2) Active -> disable -> re-enable -> disable again
      {
        cm_dist_item item("d2");
        item.c_dist2.constraint_mode(false);
        for (int i = 0; i < 5; ++i) {
          BOOST_REQUIRE(item.randomize());
          BOOST_REQUIRE_GE(item.x, 10);
          BOOST_REQUIRE_LE(item.x, 12);
        }

        item.c_dist.constraint_mode(false);
        std::set<int> seen;
        for (int i = 0; i < 16; ++i) {
          BOOST_REQUIRE(item.randomize());
          seen.insert(item.x);
        }
        BOOST_REQUIRE_GT(seen.size(), 1u);

        item.c_dist.constraint_mode(true);
        for (int i = 0; i < 5; ++i) {
          BOOST_REQUIRE(item.randomize());
          BOOST_REQUIRE_GE(item.x, 10);
          BOOST_REQUIRE_LE(item.x, 12);
        }

        item.c_dist.constraint_mode(false);
        seen.clear();
        for (int i = 0; i < 16; ++i) {
          BOOST_REQUIRE(item.randomize());
          seen.insert(item.x);
        }
        BOOST_REQUIRE_GT(seen.size(), 1u);
      }

      // 3) item.constraint_mode(false) / (true) across scalar, bool, crv_array, and nested crv_sequence_item
      {
        cm_dist_item item("d3");
        item.c_dist2.constraint_mode(false);
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_GE(item.x, 10);
        BOOST_REQUIRE_LE(item.x, 12);
        BOOST_REQUIRE(item.b);
        BOOST_REQUIRE_GE(item.arr[0], 100);
        BOOST_REQUIRE_LE(item.arr[0], 102);
        BOOST_REQUIRE_GE(item.sub.z, 30);
        BOOST_REQUIRE_LE(item.sub.z, 32);

        item.constraint_mode(false);
        BOOST_REQUIRE(!item.constraint_mode());
        std::set<int> x_vals, arr0_vals, z_vals;
        std::set<bool> b_vals;
        for (int i = 0; i < 20; ++i) {
          BOOST_REQUIRE(item.randomize());
          x_vals.insert(item.x);
          b_vals.insert(item.b);
          arr0_vals.insert(item.arr[0]);
          z_vals.insert(item.sub.z);
        }
        BOOST_REQUIRE_GT(x_vals.size(), 1u);
        BOOST_REQUIRE_GT(b_vals.size(), 1u);
        BOOST_REQUIRE_GT(arr0_vals.size(), 1u);
        BOOST_REQUIRE_GT(z_vals.size(), 1u);

        item.c_dist.constraint_mode(true);
        item.c_b_dist.constraint_mode(true);
        item.c_arr_dist.constraint_mode(true);
        item.sub.c_z_dist.constraint_mode(true);
        for (int i = 0; i < 5; ++i) {
          BOOST_REQUIRE(item.randomize());
          BOOST_REQUIRE_GE(item.x, 10);
          BOOST_REQUIRE_LE(item.x, 12);
          BOOST_REQUIRE(item.b);
          BOOST_REQUIRE_GE(item.arr[0], 100);
          BOOST_REQUIRE_LE(item.arr[0], 102);
          BOOST_REQUIRE_GE(item.sub.z, 30);
          BOOST_REQUIRE_LE(item.sub.z, 32);
        }
      }

      // 4) randomize_with(dist(...)) while c_dist is off, and on plain item with no dist
      {
        cm_dist_item item("d4");
        item.c_dist.constraint_mode(false);
        item.c_dist2.constraint_mode(false);
        BOOST_REQUIRE(item.randomize_with(dist(item.x(), make_distribution(range<int>(50, 52)))));
        BOOST_REQUIRE_GE(item.x, 50);
        BOOST_REQUIRE_LE(item.x, 52);
        std::set<int> seen;
        for (int i = 0; i < 16; ++i) {
          BOOST_REQUIRE(item.randomize());
          seen.insert(item.x);
        }
        BOOST_REQUIRE_GT(seen.size(), 1u);

        cm_plain_item plain("plain");
        BOOST_REQUIRE(plain.randomize_with(dist(plain.x(), make_distribution(range<int>(60, 62)))));
        BOOST_REQUIRE_GE(plain.x, 60);
        BOOST_REQUIRE_LE(plain.x, 62);
        std::set<int> plain_seen;
        for (int i = 0; i < 16; ++i) {
          BOOST_REQUIRE(plain.randomize());
          plain_seen.insert(plain.x);
        }
        BOOST_REQUIRE_GT(plain_seen.size(), 1u);
      }

      // 5) Two dist constraints on same variable
      {
        cm_dist_item item("d5");
        item.c_dist.constraint_mode(false);
        for (int i = 0; i < 5; ++i) {
          BOOST_REQUIRE(item.randomize());
          BOOST_REQUIRE_GE(item.x, 20);
          BOOST_REQUIRE_LE(item.x, 22);
        }
        item.c_dist2.constraint_mode(false);
        std::set<int> seen;
        for (int i = 0; i < 16; ++i) {
          BOOST_REQUIRE(item.randomize());
          seen.insert(item.x);
        }
        BOOST_REQUIRE_GT(seen.size(), 1u);
        item.c_dist.constraint_mode(true);
        for (int i = 0; i < 5; ++i) {
          BOOST_REQUIRE(item.randomize());
          BOOST_REQUIRE_GE(item.x, 10);
          BOOST_REQUIRE_LE(item.x, 12);
        }
      }
    }
  }
}

struct cm_dist_randc_item : public crv_sequence_item {
  cm_dist_randc_item(crv_object_name) {}
  crv_variable<unsigned char> x;
  crv_variable<unsigned> y;
  crv_constraint c_bound{"c_bound", x() < static_cast<unsigned char>(8u)};
  crv_constraint c_dist{"c_dist", dist(x(), make_distribution(range<unsigned char>(0, 2)))};
  crv_constraint c_rel{"c_rel", if_then(x() == static_cast<unsigned char>(0u), y() == 77u)};
  crv_solve_before order{x, y};
};

struct cm_dist_plain_u8_item : public crv_sequence_item {
  cm_dist_plain_u8_item(crv_object_name) {}
  crv_variable<unsigned char> x;
  crv_constraint c_bound{"c_bound", x() < static_cast<unsigned char>(8u)};
};

struct cm_dist_inside_item : public crv_sequence_item {
  cm_dist_inside_item(crv_object_name) {}
  crv_variable<unsigned char> x;
  crv_constraint c_inside{"c_inside", inside(x(), static_cast<unsigned char>(10u), static_cast<unsigned char>(11u),
                                              static_cast<unsigned char>(12u), static_cast<unsigned char>(13u))};
  crv_constraint c_dist{"c_dist", dist(x(), make_distribution(range<unsigned char>(10, 10)))};
};

BOOST_AUTO_TEST_CASE(constraint_mode_dist_then_randc) {
  struct Guard {
    ~Guard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    // 1) c_dist active -> c_dist.constraint_mode(false) -> x.randc(true) -> full 8-value cycle without repeats ->
    //    x.randc(false) -> c_dist.constraint_mode(true)
    {
      cm_dist_randc_item item("dr1");
      item.order.constraint_mode(false);
      for (int i = 0; i < 5; ++i) {
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_LE(static_cast<unsigned>(item.x), 2u);
      }

      item.c_dist.constraint_mode(false);
      item.x.randc(true);
      std::set<unsigned char> cycle_vals;
      for (int i = 0; i < 8; ++i) {
        BOOST_REQUIRE_NO_THROW(BOOST_REQUIRE(item.randomize()));
        BOOST_REQUIRE_LT(static_cast<unsigned>(item.x), 8u);
        if (item.x == 0u) BOOST_REQUIRE_EQUAL(item.y, 77u);
        BOOST_REQUIRE(cycle_vals.insert(item.x).second);
      }
      BOOST_REQUIRE_EQUAL(cycle_vals.size(), 8u);

      // Re-enable c_dist after randc(false)
      item.x.randc(false);
      item.c_dist.constraint_mode(true);
      for (int i = 0; i < 5; ++i) {
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_LE(static_cast<unsigned>(item.x), 2u);
      }
    }

    // 2) randomize_with(dist(x(), ...)) -> x.randc(true) must not throw and must cycle all 8 values
    {
      cm_dist_plain_u8_item item("dr2");
      BOOST_REQUIRE(item.randomize_with(dist(item.x(), make_distribution(range<unsigned char>(1, 1)))));
      BOOST_REQUIRE_EQUAL(static_cast<unsigned>(item.x), 1u);

      item.x.randc(true);
      std::set<unsigned char> cycle_vals;
      for (int i = 0; i < 8; ++i) {
        BOOST_REQUIRE_NO_THROW(BOOST_REQUIRE(item.randomize()));
        BOOST_REQUIRE_LT(static_cast<unsigned>(item.x), 8u);
        BOOST_REQUIRE(cycle_vals.insert(item.x).second);
      }
      BOOST_REQUIRE_EQUAL(cycle_vals.size(), 8u);
    }

    // 3) solve_before + c_dist interaction: disabling c_dist releases solve_before dist override,
    //    and solve_before + randc throws while solve_before is active and succeeds once disabled
    {
      cm_dist_randc_item item("dr3");
      for (int i = 0; i < 5; ++i) {
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_LE(static_cast<unsigned>(item.x), 2u);
      }
      item.c_dist.constraint_mode(false);
      std::set<unsigned char> seen;
      for (int i = 0; i < 96; ++i) {
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_LT(static_cast<unsigned>(item.x), 8u);
        if (item.x == 0u) BOOST_REQUIRE_EQUAL(item.y, 77u);
        seen.insert(item.x);
      }
      BOOST_REQUIRE_EQUAL(seen.size(), 8u);

      item.x.randc(true);
      BOOST_REQUIRE_THROW(item.randomize(), std::runtime_error);
      item.order.constraint_mode(false);
      std::set<unsigned char> cycle_vals;
      for (int i = 0; i < 8; ++i) {
        BOOST_REQUIRE_NO_THROW(BOOST_REQUIRE(item.randomize()));
        BOOST_REQUIRE(cycle_vals.insert(item.x).second);
      }
      BOOST_REQUIRE_EQUAL(cycle_vals.size(), 8u);
    }
  }
}

BOOST_AUTO_TEST_CASE(constraint_mode_dist_uniform_bdd_recovery) {
  struct Guard {
    ~Guard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    // 1) c_bound (x < 8): BDD sampler / uniform domain recovery after c_dist.constraint_mode(false)
    {
      cm_dist_randc_item item("bdd_rec");
      item.order.constraint_mode(false);
      for (int i = 0; i < 6; ++i) {
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_LE(static_cast<unsigned>(item.x), 2u);
      }

      item.c_dist.constraint_mode(false);
      std::set<unsigned char> seen;
      for (int i = 0; i < 96; ++i) {
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_LT(static_cast<unsigned>(item.x), 8u);
        seen.insert(item.x);
      }
      BOOST_REQUIRE_EQUAL(seen.size(), 8u);
    }

    // 2) c_inside (inside(x, 10..13)): inside_samplers_ / bdd_samplers_ recovery after c_dist.constraint_mode(false)
    {
      cm_dist_inside_item item("inside_rec");
      for (int i = 0; i < 4; ++i) {
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_EQUAL(static_cast<unsigned>(item.x), 10u);
      }

      item.c_dist.constraint_mode(false);
      std::set<unsigned char> seen;
      for (int i = 0; i < 64; ++i) {
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE(item.x >= 10u && item.x <= 13u);
        seen.insert(item.x);
      }
      BOOST_REQUIRE_EQUAL(seen.size(), 4u);
    }

    // 3) randomize_with(dist(x(), ...)) followed by plain randomize() recovers full 8-value domain
    {
      cm_dist_plain_u8_item item("rw_rec");
      BOOST_REQUIRE(item.randomize_with(dist(item.x(), make_distribution(range<unsigned char>(0, 0)))));
      BOOST_REQUIRE_EQUAL(static_cast<unsigned>(item.x), 0u);

      std::set<unsigned char> seen;
      for (int i = 0; i < 96; ++i) {
        BOOST_REQUIRE(item.randomize());
        BOOST_REQUIRE_LT(static_cast<unsigned>(item.x), 8u);
        seen.insert(item.x);
      }
      BOOST_REQUIRE_EQUAL(seen.size(), 8u);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()

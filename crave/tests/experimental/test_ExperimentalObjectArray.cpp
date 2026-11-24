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

#include <stdexcept>
#include <string>
#include <vector>

using namespace crave;

namespace {

struct ObjArrBackendGuard {
  ObjArrBackendGuard() : old_sv_(sv_signedness()) {}
  ~ObjArrBackendGuard() {
    set_solver_backend("auto");
    set_sv_signedness(old_sv_);
  }
  bool old_sv_;
};

struct NamedPkt : public crv_sequence_item {
  NamedPkt(crv_object_name = "pkt") {}
  crv_variable<unsigned> x{"x"};
  crv_variable<int> y{"y"};
  crv_constraint c_bounds{"c_bounds", x() >= 1u && x() <= 20u && y() >= -10 && y() <= 10};

  void pre_randomize() override { ++pre_calls; }
  void post_randomize() override { ++post_calls; }

  int pre_calls = 0;
  int post_calls = 0;
};

struct DefaultOnlyPkt : public crv_sequence_item {
  DefaultOnlyPkt() {}
  crv_variable<unsigned> addr{"addr"};
  crv_constraint c{"c", addr() < 16u};
};

struct TopWithObjArray : public crv_sequence_item {
  TopWithObjArray(crv_object_name = "top") {}
  crv_object_array<NamedPkt, 4> pkts{"pkts"};
  crv_constraint c_cross{"c_cross",
                         pkts[0].x() < pkts[1].x(),
                         unique(pkts[0].x(), pkts[1].x(), pkts[2].x(), pkts[3].x()),
                         dist(pkts[0].x(), make_distribution(dist_per_value(1u, 5u, 4)))};
};

struct OuterObj : public crv_sequence_item {
  OuterObj(crv_object_name = "outer") {}
  crv_object_array<NamedPkt, 2> inner{"inner"};
  crv_constraint c{"c", inner[0].x() + inner[1].x() == 10u};
};

struct NestedObjArrayTop : public crv_sequence_item {
  NestedObjArrayTop(crv_object_name = "nested_top") {}
  crv_object_array<OuterObj, 2> outers{"outers"};
  crv_constraint c{"c", outers[0].inner[0].x() < outers[1].inner[0].x()};
};

struct CustomRandItem : public crv_object {
  CustomRandItem(crv_object_name = "custom") {}
  bool randomize() override {
    ++calls;
    val = calls * 10;
    return true;
  }
  int calls = 0;
  int val = 0;
};

}  // namespace

BOOST_FIXTURE_TEST_SUITE(ExperimentalObjectArray_t, Context_Fixture)

BOOST_AUTO_TEST_CASE(hierarchy_naming_and_accessors) {
  TopWithObjArray top("my_top");
  BOOST_REQUIRE_EQUAL(top.pkts.obj_kind(), "crv_object_array");
  BOOST_REQUIRE_EQUAL(top.pkts.size(), 4u);
  BOOST_REQUIRE(!top.pkts.empty());
  BOOST_REQUIRE(crv_object::find("my_top.pkts") == &top.pkts);
  BOOST_REQUIRE(crv_object::find("my_top.pkts.element[0]") == &top.pkts[0]);
  BOOST_REQUIRE(crv_object::find("my_top.pkts.element[3].x") == &top.pkts[3].x);
  BOOST_REQUIRE_EQUAL(top.pkts.at(2).fullname(), "my_top.pkts.element[2]");
  BOOST_REQUIRE_THROW(top.pkts.at(4), std::out_of_range);

  // Iterators and range-for
  int idx = 0;
  for (NamedPkt& p : top.pkts) {
    BOOST_REQUIRE_EQUAL(p.name(), "element[" + std::to_string(idx++) + "]");
  }
  BOOST_REQUIRE_EQUAL(top.pkts.end() - top.pkts.begin(), 4);
  BOOST_REQUIRE_EQUAL(top.pkts.cend() - top.pkts.cbegin(), 4);

  // Default-constructible Item() support
  crv_object_array<DefaultOnlyPkt, 2> darr("darr");
  BOOST_REQUIRE(crv_object::find("darr.element[1].addr") == &darr[1].addr);
  BOOST_REQUIRE(darr.randomize());
  BOOST_REQUIRE_LT(static_cast<unsigned>(darr[0].addr), 16u);
  BOOST_REQUIRE_LT(static_cast<unsigned>(darr[1].addr), 16u);

  // Zero-length array
  crv_object_array<NamedPkt, 0> zarr("zarr");
  BOOST_REQUIRE_EQUAL(zarr.size(), 0u);
  BOOST_REQUIRE(zarr.empty());
  BOOST_REQUIRE(zarr.begin() == zarr.end());
  BOOST_REQUIRE(zarr.randomize());
}

BOOST_AUTO_TEST_CASE(nested_arrays_and_joint_solving_backends) {
  ObjArrBackendGuard guard;
  for (char const* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);
    for (bool sv : {false, true}) {
      set_sv_signedness(sv);

      TopWithObjArray top("top");
      for (int i = 0; i < 10; ++i) {
        BOOST_REQUIRE(top.randomize());
        BOOST_REQUIRE_LT(static_cast<unsigned>(top.pkts[0].x), static_cast<unsigned>(top.pkts[1].x));
        BOOST_REQUIRE_LE(static_cast<unsigned>(top.pkts[0].x), 5u);
        std::vector<unsigned> vals = {top.pkts[0].x, top.pkts[1].x, top.pkts[2].x, top.pkts[3].x};
        for (std::size_t a = 0; a < vals.size(); ++a) {
          for (std::size_t b = a + 1; b < vals.size(); ++b) {
            BOOST_REQUIRE_NE(vals[a], vals[b]);
          }
        }
      }

      NestedObjArrayTop ntop("ntop");
      for (int i = 0; i < 5; ++i) {
        BOOST_REQUIRE(ntop.randomize());
        BOOST_REQUIRE_EQUAL(static_cast<unsigned>(ntop.outers[0].inner[0].x) +
                                static_cast<unsigned>(ntop.outers[0].inner[1].x),
                            10u);
        BOOST_REQUIRE_EQUAL(static_cast<unsigned>(ntop.outers[1].inner[0].x) +
                                static_cast<unsigned>(ntop.outers[1].inner[1].x),
                            10u);
        BOOST_REQUIRE_LT(static_cast<unsigned>(ntop.outers[0].inner[0].x),
                         static_cast<unsigned>(ntop.outers[1].inner[0].x));
      }
    }
  }
}

#if defined(__cplusplus) && (__cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))
BOOST_AUTO_TEST_CASE(cpp17_foreach_reductions_and_cache) {
  ObjArrBackendGuard guard;
  struct RedItem : public crv_sequence_item {
    RedItem(crv_object_name = "red") {}
    crv_object_array<NamedPkt, 4> pkts{"pkts"};
    crv_constraint c_fe{"c_fe",
                        crave::foreach (pkts, [](auto const& p) { return p.x() >= 2u && p.x() <= 8u; }),
                        crave::foreach (pkts, [](unsigned idx, auto const& p) { return p.y() >= static_cast<int>(idx); }),
                        pkts.sum_with([](auto const& p) { return p.x(); }) == 20u,
                        pkts.sum_with([](auto const& p) { return p.x() >= 6u; }) == 2u,
                        pkts.with([](auto const& p) { return p.y() > 0; }).and_()};
  };

  for (char const* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);
    RedItem item("ritem");
    for (int i = 0; i < 6; ++i) {
      BOOST_REQUIRE(item.randomize());
      unsigned sum_x = 0;
      unsigned ge6_cnt = 0;
      for (unsigned k = 0; k < 4; ++k) {
        unsigned xv = item.pkts[k].x;
        int yv = item.pkts[k].y;
        BOOST_REQUIRE(xv >= 2u && xv <= 8u);
        BOOST_REQUIRE_GE(yv, static_cast<int>(k));
        BOOST_REQUIRE_GT(yv, 0);
        sum_x += xv;
        if (xv >= 6u) ++ge6_cnt;
      }
      BOOST_REQUIRE_EQUAL(sum_x, 20u);
      BOOST_REQUIRE_EQUAL(ge6_cnt, 2u);
    }

    // randomize_with + !foreach + soft(foreach) + cache hits
    unsigned lim = 3u;
    BOOST_REQUIRE(item.randomize_with(crave::foreach (item.pkts, [&](auto const& p) { return p.x() >= crave::reference(lim); })));
    lim = 4u;
    BOOST_REQUIRE(item.randomize_with(crave::foreach (item.pkts, [&](auto const& p) { return p.x() >= crave::reference(lim); })));
    BOOST_REQUIRE_EQUAL(item.randomize_with_cache_hits(), 1u);
    for (auto const& p : item.pkts) {
      BOOST_REQUIRE_GE(static_cast<unsigned>(p.x), 4u);
    }
  }
}
#endif

BOOST_AUTO_TEST_CASE(rand_mode_constraint_mode_standalone_and_srandom) {
  TopWithObjArray top("top_ctrl");
  BOOST_REQUIRE(top.randomize());
  BOOST_REQUIRE_EQUAL(top.pkts[0].pre_calls, 1);
  BOOST_REQUIRE_EQUAL(top.pkts[0].post_calls, 1);

  unsigned const saved_x1 = top.pkts[1].x;
  top.pkts[1].rand_mode(false);
  BOOST_REQUIRE(top.randomize());
  BOOST_REQUIRE_EQUAL(static_cast<unsigned>(top.pkts[1].x), saved_x1);
  BOOST_REQUIRE_EQUAL(top.pkts[1].pre_calls, 1);  // skipped while disabled
  top.pkts[1].rand_mode(true);
  BOOST_REQUIRE(top.randomize());
  BOOST_REQUIRE_EQUAL(top.pkts[1].pre_calls, 2);

  // constraint_mode(false) disables array element constraints
  top.c_cross.deactivate();
  top.pkts.constraint_mode(false);
  BOOST_REQUIRE(!top.pkts.constraint_mode());
  BOOST_REQUIRE(!top.pkts[0].c_bounds.constraint_mode());
  bool saw_gt_20 = false;
  for (int i = 0; i < 20; ++i) {
    BOOST_REQUIRE(top.randomize());
    if (static_cast<unsigned>(top.pkts[0].x) > 20u) {
      saw_gt_20 = true;
      break;
    }
  }
  BOOST_REQUIRE(saw_gt_20);
  top.pkts.constraint_mode(true);
  BOOST_REQUIRE(top.pkts.constraint_mode());

  // Standalone arr.randomize() including custom crv_object subclass
  crv_object_array<CustomRandItem, 3> custom_arr("carr");
  BOOST_REQUIRE(custom_arr.randomize());
  BOOST_REQUIRE_EQUAL(custom_arr[0].val, 10);
  BOOST_REQUIRE_EQUAL(custom_arr[2].val, 10);

  // Per-object srandom & randstate on crv_object_array
  crv_object_array<NamedPkt, 3> a1("a1"), a2("a2");
  a1.srandom(9001u);
  a2.srandom(9001u);
  std::string const st = a1.get_randstate();
  BOOST_REQUIRE(a1.randomize());
  BOOST_REQUIRE(a2.randomize());
  for (unsigned i = 0; i < 3; ++i) {
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(a1[i].x), static_cast<unsigned>(a2[i].x));
    BOOST_REQUIRE_EQUAL(static_cast<int>(a1[i].y), static_cast<int>(a2[i].y));
  }
  std::vector<unsigned> first_pass = {a1[0].x, a1[1].x, a1[2].x};
  BOOST_REQUIRE(a1.randomize());
  a1.set_randstate(st);
  BOOST_REQUIRE(a1.randomize());
  for (unsigned i = 0; i < 3; ++i) {
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(a1[i].x), first_pass[i]);
  }

  // Array-level rand_mode(false) + direct arr.randomize() skips all element pre/post hooks and randomize()
  crv_object_array<NamedPkt, 3> rm_arr("rm_arr");
  BOOST_REQUIRE(rm_arr.randomize());
  for (unsigned i = 0; i < 3; ++i) {
    BOOST_REQUIRE_EQUAL(rm_arr[i].pre_calls, 1);
    BOOST_REQUIRE_EQUAL(rm_arr[i].post_calls, 1);
  }
  unsigned const saved0 = rm_arr[0].x;
  unsigned const saved1 = rm_arr[1].x;
  rm_arr.rand_mode(false);
  BOOST_REQUIRE(rm_arr.randomize());
  BOOST_REQUIRE_EQUAL(static_cast<unsigned>(rm_arr[0].x), saved0);
  BOOST_REQUIRE_EQUAL(static_cast<unsigned>(rm_arr[1].x), saved1);
  for (unsigned i = 0; i < 3; ++i) {
    BOOST_REQUIRE_EQUAL(rm_arr[i].pre_calls, 1);
    BOOST_REQUIRE_EQUAL(rm_arr[i].post_calls, 1);
  }

  // Element-level rand_mode(false) skips only the disabled element in direct arr.randomize()
  rm_arr.rand_mode(true);
  rm_arr[1].rand_mode(false);
  unsigned const kept1 = rm_arr[1].x;
  BOOST_REQUIRE(rm_arr.randomize());
  BOOST_REQUIRE_EQUAL(rm_arr[0].pre_calls, 2);
  BOOST_REQUIRE_EQUAL(rm_arr[0].post_calls, 2);
  BOOST_REQUIRE_EQUAL(rm_arr[1].pre_calls, 1);
  BOOST_REQUIRE_EQUAL(rm_arr[1].post_calls, 1);
  BOOST_REQUIRE_EQUAL(static_cast<unsigned>(rm_arr[1].x), kept1);
  BOOST_REQUIRE_EQUAL(rm_arr[2].pre_calls, 2);
  BOOST_REQUIRE_EQUAL(rm_arr[2].post_calls, 2);

  // Also verify CustomRandItem array skips disabled array/element
  custom_arr.rand_mode(false);
  BOOST_REQUIRE(custom_arr.randomize());
  BOOST_REQUIRE_EQUAL(custom_arr[0].calls, 1);
  BOOST_REQUIRE_EQUAL(custom_arr[1].calls, 1);
  custom_arr.rand_mode(true);
  custom_arr[1].rand_mode(false);
  BOOST_REQUIRE(custom_arr.randomize());
  BOOST_REQUIRE_EQUAL(custom_arr[0].calls, 2);
  BOOST_REQUIRE_EQUAL(custom_arr[1].calls, 1);
  BOOST_REQUIRE_EQUAL(custom_arr[2].calls, 2);
}

#if defined(__cplusplus) && (__cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))
BOOST_AUTO_TEST_CASE(zero_length_array_boolean_reductions) {
  struct ZeroArrHolder : public crv_sequence_item {
    ZeroArrHolder(crv_object_name = "zh") {}
    crv_object_array<NamedPkt, 0> arr0{"arr0"};
    crv_object_array<OuterObj, 0> outer0{"outer0"};
    crv_variable<unsigned> sum_out{"sum_out"};
    crv_variable<unsigned> sum_fe{"sum_fe"};
    crv_variable<unsigned> sum_it{"sum_it"};
    crv_variable<unsigned> sum_ite_bool{"sum_ite_bool"};
    crv_variable<unsigned> sum_ite_u{"sum_ite_u"};
    crv_variable<unsigned> prod_ite_u{"prod_ite_u"};
    crv_constraint c{
        "c",
        arr0.with([](auto const& p) { return p.x() > 0u; }).and_() == true,
        arr0.with([](auto const& p) { return p.x() > 0u; }).or_() == false,
        sum_out() == arr0.sum_with([](auto const& p) { return p.x() > 0u; }),
        outer0.with([](auto const& o) {
          return crave::foreach (o.inner, [](auto const& p) { return p.x() > 0u; });
        }).and_() == true,
        outer0.with([](auto const& o) {
          return crave::foreach (o.inner, [](auto const& p) { return p.x() > 0u; });
        }).or_() == false,
        sum_fe() == outer0.sum_with([](auto const& o) {
          return crave::foreach (o.inner, [](auto const& p) { return p.x() > 0u; });
        }),
        arr0.with([](auto const& p) { return crave::if_then(p.x() > 5u, p.y() > 0); }).and_() == true,
        arr0.with([](auto const& p) { return crave::if_then(p.x() > 5u, p.y() > 0); }).or_() == false,
        sum_it() == arr0.sum_with([](auto const& p) { return crave::if_then(p.x() > 5u, p.y() > 0); }),
        arr0.with([](auto const& p) { return crave::if_then_else(p.x() > 5u, p.y() > 0, p.y() < 0); }).and_() == true,
        arr0.with([](auto const& p) { return crave::if_then_else(p.x() > 5u, p.y() > 0, p.y() < 0); }).or_() == false,
        sum_ite_bool() ==
            arr0.sum_with([](auto const& p) { return crave::if_then_else(p.x() > 5u, p.y() > 0, p.y() < 0); }),
        sum_ite_u() == arr0.sum_with([](auto const& p) { return crave::if_then_else(p.x() > 5u, 1u, 2u); }),
        prod_ite_u() == arr0.with([](auto const& p) { return crave::if_then_else(p.x() > 5u, 1u, 2u); }).product(),
    };
  };

  ObjArrBackendGuard guard;
  for (char const* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);
    ZeroArrHolder zh("zh");
    BOOST_REQUIRE(zh.randomize());
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(zh.sum_out), 0u);
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(zh.sum_fe), 0u);
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(zh.sum_it), 0u);
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(zh.sum_ite_bool), 0u);
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(zh.sum_ite_u), 0u);
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(zh.prod_ite_u), 1u);
    BOOST_REQUIRE(zh.randomize_with(zh.arr0.with([](auto const& p) { return p.y() > 0; }).and_()));
    BOOST_REQUIRE(zh.randomize_with(
        zh.outer0.with([](auto const& o) {
          return crave::foreach (o.inner, [](auto const& p) { return p.x() > 0u; });
        }).and_()));
    BOOST_REQUIRE(zh.randomize_with(
        zh.arr0.with([](auto const& p) { return crave::if_then(p.x() > 5u, p.y() > 0); }).and_()));
  }
}

BOOST_AUTO_TEST_CASE(zero_vs_nonzero_terminal_boolean_tags_and_width_parity) {
  struct RichElem : public crv_sequence_item {
    RichElem(crv_object_name = "re") {}
    crv_variable<unsigned> x{"x"};
    crv_variable<unsigned> z{"z"};
    crv_variable<uint64_t> u64{"u64"};
    crv_variable<int8_t> s8{"s8"};
    crv_array<unsigned, 2> uarr{"uarr"};
    crv_array<bool, 2> barr{"barr"};
    crv_vector<unsigned> uvec{"uvec"};
    crv_vector<bool> bvec{"bvec"};
    crv_variable<unsigned> idx{"idx"};
    crv_object_array<NamedPkt, 2> inner{"inner"};
    crv_constraint c{
        "c",
        x() >= 1u && x() <= 8u,
        z() == 9u,
        uarr[0]() >= 1u && uarr[0]() <= 4u && uarr[1]() >= 1u && uarr[1]() <= 4u,
        uvec().size() == 2u,
        bvec().size() == 2u,
        idx() < 2u,
        inner[0].x() >= 1u && inner[1].x() >= 1u,
    };
  };

  auto run_parity = []() {
    struct ZeroRichHolder : public crv_sequence_item {
      ZeroRichHolder(crv_object_name = "zrh") {}
      crv_object_array<RichElem, 0> arr0{"arr0"};
      crv_variable<unsigned> sum_in{"sum_in"};
      crv_variable<unsigned> sum_uniq{"sum_uniq"};
      crv_variable<unsigned> sum_oh{"sum_oh"};
      crv_variable<unsigned> sum_nested{"sum_nested"};
      crv_variable<uint64_t> and_u64{"and_u64"};
      crv_variable<uint64_t> prod_u64{"prod_u64"};
      crv_variable<uint64_t> sum_u64{"sum_u64"};
      crv_variable<int8_t> and_s8{"and_s8"};
      crv_constraint c{
          "c",
          arr0.with([](auto const& p) { return crave::inside(p.x(), 1u, 2u, 4u, 8u); }).and_() == true,
          arr0.with([](auto const& p) { return crave::inside(p.x(), 1u, 2u, 4u, 8u); }).or_() == false,
          arr0.with([](auto const& p) { return crave::inside(p.x(), 1u, 2u, 4u, 8u); }).xor_() == false,
          arr0.with([](auto const& p) { return crave::inside(p.x(), 1u, 2u, 4u, 8u); }).product() == true,
          sum_in() == arr0.sum_with([](auto const& p) { return crave::inside(p.x(), 1u, 2u, 4u, 8u); }),
          arr0.with([](auto const& p) { return crave::unique(p.x(), p.z()); }).and_() == true,
          sum_uniq() == arr0.sum_with([](auto const& p) { return crave::unique(p.x(), p.z()); }),
          arr0.with([](auto const& p) { return crave::onehot(p.x()); }).and_() == true,
          sum_oh() == arr0.sum_with([](auto const& p) { return crave::onehot(p.x()); }),
          arr0.with([](auto const& p) { return crave::onehot0(p.x()); }).and_() == true,
          arr0.with([](auto const& p) { return crave::foreach (p.uarr, _item >= 1u); }).and_() == true,
          arr0.with([](auto const& p) { return crave::foreach (p.uvec, p.uvec()[_i] >= 1u); }).and_() == true,
          arr0.with([](auto const& p) { return p.barr[p.idx()]; }).and_() == true,
          arr0.with([](auto const& p) { return p.barr[p.idx()]; }).or_() == false,
          arr0.with([](auto const& p) { return p.barr.with(p.barr[_i]).and_(); }).and_() == true,
          arr0.with([](auto const& p) { return crave::foreach (p.barr, p.barr[_i]); }).and_() == true,
          arr0.with([](auto const& p) { return p.bvec.with(_item).and_(); }).and_() == true,
          arr0.with([](auto const& p) { return p.bvec()[0u]; }).and_() == true,
          arr0.with([](auto const& p) {
            return p.inner.with([](auto const& q) { return q.x() >= 1u; }).and_();
          }).and_() == true,
          arr0.with([](auto const& p) {
            return p.inner.with([](auto const& q) { return q.x() >= 1u; }).or_();
          }).or_() == false,
          arr0.with([](auto const& p) {
            return p.inner.with([](auto const& q) { return q.x() >= 1u; }).xor_();
          }).xor_() == false,
          arr0.with([](auto const& p) {
            return p.inner.with([](auto const& q) { return q.x() >= 1u; }).product();
          }).product() == true,
          sum_nested() == arr0.sum_with([](auto const& p) {
            return p.inner.sum_with([](auto const& q) { return q.x() >= 1u; });
          }),
          arr0.with([](auto const& p) { return p.uarr.with(_item >= 1u).and_(); }).and_() == true,
          arr0.with([](auto const& p) { return p.uarr.with([](auto const& v) { return v >= 1u; }).and_(); }).and_() == true,
          arr0.with([](auto const& p) { return p.barr.and_(); }).and_() == true,
          arr0.with([](auto const& p) {
            return crave::if_then_else(p.x() > 4u, crave::inside(p.x(), 5u, 8u), crave::onehot(p.x()));
          }).and_() == true,
          and_u64() == arr0.with([](auto const& p) { return p.u64(); }).and_(),
          prod_u64() == arr0.with([](auto const& p) { return p.u64(); }).product(),
          sum_u64() == arr0.with([](auto const& p) { return p.u64(); }).sum(),
          and_s8() == arr0.with([](auto const& p) { return p.s8(); }).and_(),
      };
    };

    struct TwoRichHolder : public crv_sequence_item {
      TwoRichHolder(crv_object_name = "trh") {}
      crv_object_array<RichElem, 2> arr2{"arr2"};
      crv_variable<unsigned> sum_in{"sum_in"};
      crv_variable<unsigned> sum_nested{"sum_nested"};
      crv_constraint c{
          "c",
          arr2.with([](auto const& p) { return crave::inside(p.x(), 1u, 2u, 4u, 8u); }).and_() == true,
          arr2.with([](auto const& p) { return crave::unique(p.x(), p.z()); }).and_() == true,
          arr2.with([](auto const& p) { return crave::onehot(p.x()); }).and_() == true,
          arr2.with([](auto const& p) { return crave::onehot0(p.x()); }).and_() == true,
          arr2.with([](auto const& p) { return crave::foreach (p.uarr, _item >= 1u); }).and_() == true,
          arr2.with([](auto const& p) { return p.barr.with(p.barr[_i]).and_(); }).and_() == true,
          arr2.with([](auto const& p) { return crave::foreach (p.barr, p.barr[_i]); }).and_() == true,
          arr2.with([](auto const& p) {
            return p.inner.with([](auto const& q) { return q.x() >= 1u; }).and_();
          }).and_() == true,
          arr2.with([](auto const& p) { return p.uarr.with(_item >= 1u).and_(); }).and_() == true,
          arr2.with([](auto const& p) { return p.uarr.with([](auto const& v) { return v >= 1u; }).and_(); }).and_() == true,
          arr2.with([](auto const& p) { return p.barr.and_(); }).and_() == true,
          arr2.with([](auto const& p) {
            return crave::if_then_else(p.x() > 4u, crave::inside(p.x(), 5u, 8u), crave::onehot(p.x()));
          }).and_() == true,
          sum_in() == arr2.sum_with([](auto const& p) { return crave::inside(p.x(), 1u, 2u, 4u, 8u); }),
          sum_nested() == arr2.sum_with([](auto const& p) {
            return p.inner.sum_with([](auto const& q) { return q.x() >= 1u; });
          }),
      };
    };

    struct OneVecHolder : public crv_sequence_item {
      OneVecHolder(crv_object_name = "ovh") {}
      crv_object_array<RichElem, 1> arr1{"arr1"};
      crv_constraint c{
          "c",
          arr1.with([](auto const& p) { return crave::foreach (p.uvec, p.uvec()[_i] >= 1u); }).and_(),
          arr1.with([](auto const& p) { return p.bvec.with(_item).and_(); }).and_() == true,
      };
    };

    ZeroRichHolder zh("zh");
    BOOST_REQUIRE(zh.randomize());
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(zh.sum_in), 0u);
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(zh.sum_uniq), 0u);
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(zh.sum_oh), 0u);
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(zh.sum_nested), 0u);
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.and_u64), ~uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.prod_u64), uint64_t(1));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.sum_u64), uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<int>(zh.and_s8), -1);

    TwoRichHolder th("th");
    BOOST_REQUIRE(th.randomize());
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(th.sum_in), 2u);
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(th.sum_nested), 4u);
    for (unsigned i = 0; i < 2; ++i) {
      BOOST_REQUIRE(th.arr2[i].barr[0] && th.arr2[i].barr[1]);
    }

    OneVecHolder ovh("ovh");
    BOOST_REQUIRE(ovh.randomize());
    for (unsigned i = 0; i < 2; ++i) {
      BOOST_REQUIRE_GE(ovh.arr1[0].uvec[static_cast<int>(i)], 1u);
      BOOST_REQUIRE(ovh.arr1[0].bvec[static_cast<int>(i)]);
    }
  };

  ObjArrBackendGuard guard;
  for (char const* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);
    for (bool sv : {false, true}) {
      set_sv_signedness(sv);
      run_parity();
    }
  }
}

BOOST_AUTO_TEST_CASE(zero_vs_nonzero_widening_lambda_proxy_and_self_determined_parity) {
  struct WideElem : public crv_sequence_item {
    WideElem(crv_object_name = "we") {}
    crv_variable<bool> b{"b"};
    crv_array<bool, 2> barr{"barr"};
    crv_array<uint8_t, 2> u8arr{"u8arr"};
    crv_array<int8_t, 2> s8arr{"s8arr"};
    crv_vector<uint8_t> u8vec{"u8vec"};
    crv_constraint c{
        "c",
        u8vec().size() == 1u,
    };
  };

  auto run_widening_parity = [](bool sv) {
    struct WideEmptyVecElem : public crv_sequence_item {
      WideEmptyVecElem(crv_object_name = "weve") {}
      crv_vector<uint8_t> u8vec{"u8vec"};
      crv_constraint c{"c", u8vec().size() == 0u};
    };

    struct ZeroWideHolder : public crv_sequence_item {
      ZeroWideHolder(crv_object_name = "zwh") {}
      crv_object_array<WideElem, 0> arr0{"arr0"};
      crv_variable<uint64_t> arr_or{"arr_or"};
      crv_variable<uint64_t> arr_xor{"arr_xor"};
      crv_variable<uint64_t> arr_prod{"arr_prod"};
      crv_variable<uint64_t> arr_sum{"arr_sum"};
      crv_variable<uint64_t> vec_or{"vec_or"};
      crv_variable<uint64_t> vec_xor{"vec_xor"};
      crv_variable<uint64_t> vec_prod{"vec_prod"};
      crv_variable<uint64_t> vec_sum{"vec_sum"};
      crv_variable<uint64_t> idx_arr_and{"idx_arr_and"};
      crv_variable<uint64_t> idx_arr_prod{"idx_arr_prod"};
      crv_variable<uint64_t> idx_vec_and{"idx_vec_and"};
      crv_variable<uint64_t> idx_sub_and{"idx_sub_and"};
      crv_variable<uint64_t> comp_wide_and{"comp_wide_and"};
      crv_variable<uint64_t> neg_wide_and{"neg_wide_and"};
      crv_variable<uint64_t> shift_wide_and{"shift_wide_and"};
      crv_variable<uint64_t> mixed_s8_and{"mixed_s8_and"};
      crv_variable<uint64_t> ite_and{"ite_and"};
      crv_constraint c{
          "c",
          arr0.with([](auto const& p) {
            return p.u8arr.with([](auto& x) { return x | 0xffffffffffffff00ULL; }).and_();
          }).and_() == ~uint64_t(0),
          arr0.with([](auto const& p) {
            return p.u8vec.with([](auto& x) { return x | 0xffffffffffffff00ULL; }).and_();
          }).and_() == ~uint64_t(0),
          arr_or() == arr0.with([](auto const& p) {
            return p.u8arr.with([](auto& x) { return x | 0xffffffffffffff00ULL; }).or_();
          }).or_(),
          arr_xor() == arr0.with([](auto const& p) {
            return p.u8arr.with([](auto& x) { return x | 0xffffffffffffff00ULL; }).xor_();
          }).xor_(),
          arr_prod() == arr0.with([](auto const& p) {
            return p.u8arr.with([](auto& x) { return x | 0xffffffffffffff00ULL; }).product();
          }).product(),
          arr_sum() == arr0.with([](auto const& p) {
            return p.u8arr.with([](auto& x) { return x | 0xffffffffffffff00ULL; }).sum();
          }).sum(),
          vec_or() == arr0.with([](auto const& p) {
            return p.u8vec.with([](auto& x) { return x | 0xffffffffffffff00ULL; }).or_();
          }).or_(),
          vec_xor() == arr0.with([](auto const& p) {
            return p.u8vec.with([](auto& x) { return x | 0xffffffffffffff00ULL; }).xor_();
          }).xor_(),
          vec_prod() == arr0.with([](auto const& p) {
            return p.u8vec.with([](auto& x) { return x | 0xffffffffffffff00ULL; }).product();
          }).product(),
          vec_sum() == arr0.with([](auto const& p) {
            return p.u8vec.with([](auto& x) { return x | 0xffffffffffffff00ULL; }).sum();
          }).sum(),
          idx_arr_and() == arr0.with([](auto const& p) {
            return p.u8arr.with([](auto const& idx, auto const& x) { return x + uint64_t(0x100); }).and_();
          }).and_(),
          idx_arr_prod() == arr0.with([](auto const& p) {
            return p.u8arr.with([](auto const& idx, auto const& x) { return x + uint64_t(0x100); }).product();
          }).product(),
          idx_vec_and() == arr0.with([](auto const& p) {
            return p.u8vec.with([](auto const& idx, auto const& x) { return x + uint64_t(0x100); }).and_();
          }).and_(),
          idx_sub_and() == arr0.with([](auto const& p) {
            return p.u8arr.with([&](auto const& idx, auto const&) { return p.u8arr[idx] | 0xffffffffffffff00ULL; })
                .and_();
          }).and_(),
          comp_wide_and() == arr0.with([](auto const& p) {
            return p.u8arr.with([](auto const& x) { return ~(x | 0xffffffffffffff00ULL); }).and_();
          }).and_(),
          neg_wide_and() == arr0.with([](auto const& p) {
            return p.u8arr.with([](auto const& x) { return -(x + uint64_t(1)); }).and_();
          }).and_(),
          shift_wide_and() == arr0.with([](auto const& p) {
            return p.u8arr.with([](auto const& x) { return x << uint64_t(0); }).and_();
          }).and_(),
          mixed_s8_and() == arr0.with([](auto const& p) {
            return p.s8arr.with([](auto const& x) { return x + uint64_t(1); }).and_();
          }).and_(),
          ite_and() == arr0.with([](auto const& p) {
            return p.u8arr.with([](auto const& x) {
              return crave::if_then_else(x > 0u, x | 0xffffffffffffff00ULL, uint64_t(0));
            }).and_();
          }).and_(),
          arr0.with([](auto const& p) {
            return p.u8arr.with([](auto const& idx, auto const& x) { return idx + x; }).and_();
          }).and_() == ~0u,
      };
    };

    struct TwoWideHolder : public crv_sequence_item {
      TwoWideHolder(crv_object_name = "twh") {}
      crv_object_array<WideElem, 2> arr2{"arr2"};
      crv_constraint c{
          "c",
          arr2.with([](auto const& p) {
            return p.u8arr.with([](auto& x) { return x | 0xffffffffffffff00ULL; }).and_();
          }).and_() == ~uint64_t(0),
          arr2.with([](auto const& p) {
            return p.u8arr.with([](auto const& idx, auto const& x) { return x + uint64_t(0x100); }).and_();
          }).and_() == uint64_t(0x1ff),
          arr2.with([](auto const& p) {
            return p.u8arr.with([&](auto const& idx, auto const&) { return p.u8arr[idx] | 0xffffffffffffff00ULL; })
                .and_();
          }).and_() == ~uint64_t(0),
      };
    };

    struct OneWideVecHolder : public crv_sequence_item {
      OneWideVecHolder(crv_object_name = "owvh") {}
      crv_object_array<WideElem, 1> arr1{"arr1"};
      crv_constraint c{
          "c",
          arr1.with([](auto const& p) {
            return p.u8vec.with([](auto& x) { return x | 0xffffffffffffff00ULL; }).and_();
          }).and_() == ~uint64_t(0),
          arr1.with([](auto const& p) {
            return p.u8vec.with([](auto const& idx, auto const& x) { return x + uint64_t(0x100); }).and_();
          }).and_() == uint64_t(0x1ff),
      };
    };

    struct OneEmptyVecHolder : public crv_sequence_item {
      OneEmptyVecHolder(crv_object_name = "oevh") {}
      crv_object_array<WideEmptyVecElem, 1> arr1{"arr1"};
      crv_constraint c{
          "c",
          arr1.with([](auto const& p) {
            return p.u8vec.with([](auto& x) { return x | 0xffffffffffffff00ULL; }).and_();
          }).and_() == ~uint64_t(0),
      };
    };

    ZeroWideHolder zh("zh");
    BOOST_REQUIRE(zh.randomize());
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.arr_or), uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.arr_xor), uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.arr_prod), uint64_t(1));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.arr_sum), uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.vec_or), uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.vec_xor), uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.vec_prod), uint64_t(1));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.vec_sum), uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.idx_arr_and), ~uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.idx_arr_prod), uint64_t(1));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.idx_vec_and), ~uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.idx_sub_and), ~uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.comp_wide_and), ~uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.neg_wide_and), ~uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.shift_wide_and), sv ? uint64_t(0xff) : ~uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.mixed_s8_and), ~uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint64_t>(zh.ite_and), ~uint64_t(0));

    auto comp_wide_fn = [](auto const& x) { return ~(x + uint64_t(0x100)); };
    using comp_wide_t = decltype(std::declval<WideElem const&>().u8arr.with(comp_wide_fn).and_());
    BOOST_REQUIRE_EQUAL(detail::proto_expr_width<comp_wide_t>(sv), 64u);
    BOOST_REQUIRE(!detail::proto_expr_signed<comp_wide_t>(sv));

    auto bool_comp_fn = [](auto const& x) { return ~x; };
    using bool_comp_t = decltype(std::declval<WideElem const&>().barr.with(bool_comp_fn).and_());
    BOOST_REQUIRE_EQUAL(detail::proto_expr_width<bool_comp_t>(sv), sv ? 1u : 32u);
    BOOST_REQUIRE_EQUAL(detail::proto_expr_signed<bool_comp_t>(sv), !sv);

    TwoWideHolder th("th");
    BOOST_REQUIRE(th.randomize());
    for (unsigned i = 0; i < 2; ++i) {
      BOOST_REQUIRE_EQUAL(th.arr2[i].u8arr[0] | 0xffffffffffffff00ULL, ~uint64_t(0));
      BOOST_REQUIRE_EQUAL(th.arr2[i].u8arr[1] | 0xffffffffffffff00ULL, ~uint64_t(0));
      BOOST_REQUIRE_EQUAL(static_cast<uint8_t>(th.arr2[i].u8arr[0]), uint8_t(0xff));
      BOOST_REQUIRE_EQUAL(static_cast<uint8_t>(th.arr2[i].u8arr[1]), uint8_t(0xff));
    }

    OneWideVecHolder ovh("ovh");
    BOOST_REQUIRE(ovh.randomize());
    BOOST_REQUIRE_EQUAL(ovh.arr1[0].u8vec[0] | 0xffffffffffffff00ULL, ~uint64_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint8_t>(ovh.arr1[0].u8vec[0]), uint8_t(0xff));

    OneEmptyVecHolder oevh("oevh");
    BOOST_REQUIRE(oevh.randomize());
    BOOST_REQUIRE_EQUAL(oevh.arr1[0].u8vec.size(), 0u);

    // 2-arg product() widening parity (kept at N=1 for fast BDD construction on Cudd)
    struct OneProdHolder : public crv_sequence_item {
      OneProdHolder(crv_object_name = "oph", bool sv_mode = false) : expected(sv_mode ? uint64_t(0x10000) : uint64_t(0)) {}
      uint64_t expected;
      crv_object_array<WideElem, 1> arr1{"arr1"};
      crv_constraint c{
          "c",
          arr1[0].u8arr[0]() == uint8_t(0),
          arr1[0].u8arr[1]() == uint8_t(0),
          arr1.with([](auto const& p) {
            return p.u8arr.with([](auto const& idx, auto const& x) { return x + uint64_t(0x100); }).product();
          }).product() == reference(expected),
      };
    };
    OneProdHolder oph("oph", sv);
    BOOST_REQUIRE(oph.randomize());
    BOOST_REQUIRE_EQUAL(static_cast<uint8_t>(oph.arr1[0].u8arr[0]), uint8_t(0));
    BOOST_REQUIRE_EQUAL(static_cast<uint8_t>(oph.arr1[0].u8arr[1]), uint8_t(0));

    // Also verify 64-bit non-shrinking literal product() parity across all 4 cells
    struct OneProd64Holder : public crv_sequence_item {
      OneProd64Holder(crv_object_name = "op64") {}
      crv_object_array<WideElem, 1> arr1{"arr1"};
      crv_constraint c{
          "c",
          arr1[0].u8arr[0]() == uint8_t(1),
          arr1[0].u8arr[1]() == uint8_t(1),
          arr1.with([](auto const& p) {
            return p.u8arr
                .with([](auto const& idx, auto const& x) { return x + uint64_t(0x8000000000000000ULL); })
                .product();
          }).product() == uint64_t(1),
      };
    };
    OneProd64Holder op64("op64");
    BOOST_REQUIRE(op64.randomize());

    // Bug 3: ~p.b() and ~x (bool array) self-determined parity across sv_signedness(true/false)
    struct ZeroBoolCompHolder : public crv_sequence_item {
      ZeroBoolCompHolder(crv_object_name = "zbch") {}
      crv_object_array<WideElem, 0> arr0{"arr0"};
    };
    struct TwoBoolCompHolder : public crv_sequence_item {
      TwoBoolCompHolder(crv_object_name = "tbch") {}
      crv_object_array<WideElem, 2> arr2{"arr2"};
    };

    ZeroBoolCompHolder zbch("zbch");
    TwoBoolCompHolder tbch("tbch");
    if (sv) {
      BOOST_REQUIRE(zbch.randomize_with(zbch.arr0.with([](auto const& p) { return ~p.b(); }).and_() == 1u));
      BOOST_REQUIRE(tbch.randomize_with(tbch.arr2.with([](auto const& p) { return ~p.b(); }).and_() == 1u));
      BOOST_REQUIRE(!tbch.arr2[0].b && !tbch.arr2[1].b);
      BOOST_REQUIRE(zbch.randomize_with(
          zbch.arr0.with([](auto const& p) { return p.barr.with([](auto const& x) { return ~x; }).and_(); }).and_() ==
          1u));
      BOOST_REQUIRE(tbch.randomize_with(
          tbch.arr2.with([](auto const& p) { return p.barr.with([](auto const& x) { return ~x; }).and_(); }).and_() ==
          1u));
    } else {
      BOOST_REQUIRE(!zbch.randomize_with(zbch.arr0.with([](auto const& p) { return ~p.b(); }).and_() == 1u));
      BOOST_REQUIRE(!tbch.randomize_with(tbch.arr2.with([](auto const& p) { return ~p.b(); }).and_() == 1u));
      BOOST_REQUIRE(zbch.randomize_with(zbch.arr0.with([](auto const& p) { return ~p.b(); }).and_() == ~0u));
      BOOST_REQUIRE(tbch.randomize_with(tbch.arr2.with([](auto const& p) { return ~p.b(); }).and_() == ~0u));
      BOOST_REQUIRE(zbch.randomize_with(
          zbch.arr0.with([](auto const& p) { return p.barr.with([](auto const& x) { return ~x; }).and_(); }).and_() ==
          ~0u));
      BOOST_REQUIRE(tbch.randomize_with(
          tbch.arr2.with([](auto const& p) { return p.barr.with([](auto const& x) { return ~x; }).and_(); }).and_() ==
          ~0u));
    }
  };

  ObjArrBackendGuard guard;
  for (char const* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);
    for (bool sv : {false, true}) {
      set_sv_signedness(sv);
      run_widening_parity(sv);
    }
  }
}
#endif

BOOST_AUTO_TEST_SUITE_END()

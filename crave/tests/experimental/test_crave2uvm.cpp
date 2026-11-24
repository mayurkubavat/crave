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

#define BOOST_TEST_MODULE crave2uvm_tests
#include <boost/test/unit_test.hpp>

#include <crave2uvm.h>

#include <string>
#include <vector>

struct crave2uvm_item : public uvm_randomized_sequence_item {
  crave::crv_variable<unsigned> addr;
  crave::crv_variable<unsigned> data;

  crave::crv_constraint c_addr{"c_addr", addr() < 100u};
  crave::crv_soft_constraint s_addr{"s_addr", addr() == 10u};

  UVM_OBJECT_UTILS(crave2uvm_item);

  crave2uvm_item(crave::crv_object_name name = "crave2uvm_item") : uvm_randomized_sequence_item(name) {}
};

namespace {
unsigned g_seq_addr_1 = 0;
unsigned g_seq_data_1 = 0;
unsigned g_seq_addr_2 = 0;
unsigned g_seq_data_2 = 0;
bool g_seq_ran = false;

class crave2uvm_soft_seq : public uvm::uvm_sequence<crave2uvm_item> {
 public:
  crave2uvm_soft_seq(const std::string& name = "crave2uvm_soft_seq") : uvm::uvm_sequence<crave2uvm_item>(name) {}
  UVM_OBJECT_UTILS(crave2uvm_soft_seq);

  crave2uvm_item* req{nullptr};

  void body() override {
    UVM_DO_WITH(req, (req->addr() < 100u, crave::soft(req->addr() == 42u), crave::soft(req->data() == 7u)));
    g_seq_addr_1 = req->addr;
    g_seq_data_1 = req->data;

    UVM_DO_WITH(req, (req->addr() < 100u, crave::soft(req->addr() == 200u), crave::soft(req->data() == 9u)));
    g_seq_addr_2 = req->addr;
    g_seq_data_2 = req->data;
    g_seq_ran = true;
  }
};

class crave2uvm_driver : public uvm::uvm_driver<crave2uvm_item> {
 public:
  crave2uvm_driver(uvm::uvm_component_name name) : uvm::uvm_driver<crave2uvm_item>(name) {}
  UVM_COMPONENT_UTILS(crave2uvm_driver);

  void run_phase(uvm::uvm_phase& phase) override {
    (void)phase;
    while (true) {
      crave2uvm_item item;
      seq_item_port.get_next_item(item);
      seq_item_port.item_done();
    }
  }
};

class crave2uvm_soft_test : public uvm::uvm_test {
 public:
  crave2uvm_soft_test(uvm::uvm_component_name name) : uvm::uvm_test(name) {}
  UVM_COMPONENT_UTILS(crave2uvm_soft_test);

  uvm::uvm_sequencer<crave2uvm_item>* seqr{nullptr};
  crave2uvm_driver* drv{nullptr};

  void build_phase(uvm::uvm_phase& phase) override {
    uvm::uvm_test::build_phase(phase);
    seqr = new uvm::uvm_sequencer<crave2uvm_item>("seqr");
    drv = crave2uvm_driver::type_id::create("drv", this);
  }

  void connect_phase(uvm::uvm_phase& phase) override {
    uvm::uvm_test::connect_phase(phase);
    drv->seq_item_port.connect(seqr->seq_item_export);
  }

  void run_phase(uvm::uvm_phase& phase) override {
    phase.raise_objection(this);
    crave2uvm_soft_seq* seq = crave2uvm_soft_seq::type_id::create("seq");
    seq->start(seqr);
    phase.drop_objection(this);
  }
};
}  // namespace

int sc_main(int, char* []) {
  uvm::run_test("crave2uvm_soft_test");
  return 0;
}

struct Crave2Uvm_Fixture {};

BOOST_FIXTURE_TEST_SUITE(Crave2Uvm_t, Crave2Uvm_Fixture)

BOOST_AUTO_TEST_CASE(uvm_item_and_do_with_inline_soft) {
  crave2uvm_item item("item");
  BOOST_REQUIRE(item.randomize_with(crave::soft(item.addr() == 55u), crave::soft(item.data() == 11u)));
  BOOST_REQUIRE_EQUAL(item.addr, 55u);
  BOOST_REQUIRE_EQUAL(item.data, 11u);
  BOOST_REQUIRE(item.inactive_soft_constraints() == std::vector<std::string>({"item.s_addr#0"}));

  char prog[] = "test_crave2uvm";
  char* argv[] = {prog, nullptr};
  int rc = sc_core::sc_elab_and_sim(1, argv);
  BOOST_REQUIRE_EQUAL(rc, 0);
  BOOST_REQUIRE(g_seq_ran);
  BOOST_REQUIRE_EQUAL(g_seq_addr_1, 42u);
  BOOST_REQUIRE_EQUAL(g_seq_data_1, 7u);
  BOOST_REQUIRE_EQUAL(g_seq_addr_2, 10u);
  BOOST_REQUIRE_EQUAL(g_seq_data_2, 9u);
}

BOOST_AUTO_TEST_SUITE_END()

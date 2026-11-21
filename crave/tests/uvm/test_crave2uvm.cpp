//----------------------------------------------------------------------
//  Licensed to Accellera Systems Initiative Inc. (Accellera)
//  under one or more contributor license agreements.
//  See the NOTICE file distributed with this work for
//  additional information regarding copyright ownership.
//
//  Accellera licenses this file to you under the Apache
//  License, Version 2.0 (the "License"); you may not use
//  this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in
//  writing, software distributed under the License is
//  distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//  CONDITIONS OF ANY KIND, either express or implied.  See
//  the License for the specific language governing
//  permissions and limitations under the License.
//----------------------------------------------------------------------

#include <systemc>
#include <uvm>
#include <crave2uvm.h>

#include <sstream>
#include <utility>
#include <vector>

class bus_item : public uvm_randomized_sequence_item {
 public:
  crave::crv_variable<unsigned int> addr;
  crave::crv_variable<unsigned int> data;

  crave::crv_constraint c_addr { addr() >= 0x10u, addr() <= 0xffu };
  crave::crv_constraint c_data { data() >= 1u, data() <= 0xffu };

  UVM_OBJECT_UTILS(bus_item);

  bus_item(crave::crv_object_name name = "bus_item")
      : uvm_randomized_sequence_item(name) {
    addr = 0;
    data = 0;
  }

  bus_item(const bus_item& rhs)
      : uvm_randomized_sequence_item(rhs.get_name()) {
    *this = rhs;
  }

  bus_item& operator=(const bus_item& rhs) {
    if (this != &rhs) {
      uvm::uvm_sequence_item::operator=(rhs);
      addr = static_cast<unsigned int>(rhs.addr);
      data = static_cast<unsigned int>(rhs.data);
    }
    return *this;
  }
};

class plain_child_seq : public uvm::uvm_sequence<bus_item> {
 public:
  UVM_OBJECT_UTILS(plain_child_seq);

  plain_child_seq(const std::string& name = "plain_child_seq")
      : uvm::uvm_sequence<bus_item>(name) {}

  void body() override {
    bus_item* req = nullptr;
    UVM_DO_WITH(req, (req->addr() == 0x90u, req->data() == 0x99u));
  }
};

class burst_sub_seq : public uvm_randomized_sequence<bus_item> {
 public:
  crave::crv_variable<unsigned int> count;
  crave::crv_variable<unsigned int> base_addr;

  crave::crv_constraint c_burst {
    count() >= 2u, count() <= 4u,
    base_addr() >= 0x20u, base_addr() <= 0x80u
  };

  UVM_OBJECT_UTILS(burst_sub_seq);

  burst_sub_seq(crave::crv_object_name name = "burst_sub_seq")
      : uvm_randomized_sequence<bus_item>(name) {
    count = 0;
    base_addr = 0;
  }

  void body() override {
    unsigned int n = static_cast<unsigned int>(count);
    unsigned int base = static_cast<unsigned int>(base_addr);
    for (unsigned int i = 0; i < n; ++i) {
      bus_item* req = nullptr;
      UVM_DO_WITH(req, (req->addr() == base + i, req->data() == 0x10u + i));
    }
  }
};

class top_seq : public uvm_randomized_sequence<bus_item> {
 public:
  UVM_OBJECT_UTILS(top_seq);

  top_seq(crave::crv_object_name name = "top_seq")
      : uvm_randomized_sequence<bus_item>(name) {}

  void body() override {
    burst_sub_seq* s1 = nullptr;
    UVM_DO(s1);

    burst_sub_seq* s2 = nullptr;
    UVM_DO_WITH(s2, (s2->count() == 3u, s2->base_addr() == 0x40u));

    bus_item* pri_item = nullptr;
    UVM_DO_PRI_WITH(pri_item, 100, (pri_item->addr() == 0x64u, pri_item->data() == 0x77u));

    bus_item* on_item = nullptr;
    UVM_DO_ON_WITH(on_item, this->m_sequencer, (on_item->addr() == 0x68u, on_item->data() == 0x88u));

    plain_child_seq* plain_seq = nullptr;
    UVM_DO(plain_seq);
  }
};

class sink_driver : public uvm::uvm_driver<bus_item> {
 public:
  std::vector<std::pair<unsigned int, unsigned int>> seen;

  UVM_COMPONENT_UTILS(sink_driver);

  sink_driver(uvm::uvm_component_name name) : uvm::uvm_driver<bus_item>(name) {}

  void run_phase(uvm::uvm_phase&) override {
    while (true) {
      bus_item req;
      this->seq_item_port->get_next_item(req);
      seen.push_back(std::make_pair(
          static_cast<unsigned int>(req.addr),
          static_cast<unsigned int>(req.data)));
      sc_core::wait(1, sc_core::SC_NS);
      this->seq_item_port->item_done();
    }
  }
};

class crave2uvm_test_comp : public uvm::uvm_test {
 public:
  uvm::uvm_sequencer<bus_item>* seqr;
  sink_driver* drv;

  UVM_COMPONENT_UTILS(crave2uvm_test_comp);

  crave2uvm_test_comp(uvm::uvm_component_name name)
      : uvm::uvm_test(name), seqr(nullptr), drv(nullptr) {}

  void build_phase(uvm::uvm_phase& phase) override {
    uvm::uvm_test::build_phase(phase);
    seqr = new uvm::uvm_sequencer<bus_item>("seqr");
    drv = new sink_driver("drv");
  }

  void connect_phase(uvm::uvm_phase& phase) override {
    uvm::uvm_test::connect_phase(phase);
    drv->seq_item_port.connect(seqr->seq_item_export);
  }

  void run_phase(uvm::uvm_phase& phase) override {
    phase.raise_objection(this);
    top_seq* seq = top_seq::type_id::create("seq");
    seq->start(seqr);
    phase.drop_objection(this);
  }

  void check_phase(uvm::uvm_phase&) override {
    // s1: 2..4 items, s2: 3 items (0x40..0x42), pri_item: (0x64,0x77), on_item: (0x68,0x88), plain_seq: (0x90,0x99)
    if (drv->seen.size() < 8 || drv->seen.size() > 10) {
      std::ostringstream os;
      os << "Expected 8..10 items, got " << drv->seen.size();
      UVM_ERROR("SUBSEQ_RND", os.str());
      return;
    }
    size_t s1_len = drv->seen.size() - 6;
    unsigned int s1_base = drv->seen[0].first;
    if (s1_base < 0x20u || s1_base > 0x80u) {
      UVM_ERROR("SUBSEQ_DO", "UVM_DO sub-sequence base_addr out of constraint bounds");
    }
    for (size_t i = 0; i < s1_len; ++i) {
      if (drv->seen[i].first != s1_base + i || drv->seen[i].second != 0x10u + i) {
        UVM_ERROR("SUBSEQ_DO", "UVM_DO sub-sequence item mismatch");
      }
    }
    for (size_t i = 0; i < 3; ++i) {
      auto s2_it = drv->seen[s1_len + i];
      if (s2_it.first != 0x40u + i || s2_it.second != 0x10u + i) {
        UVM_ERROR("SUBSEQ_WITH", "UVM_DO_WITH sub-sequence item mismatch");
      }
    }
    auto pri_it = drv->seen[s1_len + 3];
    auto on_it = drv->seen[s1_len + 4];
    auto plain_it = drv->seen[s1_len + 5];
    if (pri_it.first != 0x64u || pri_it.second != 0x77u) {
      UVM_ERROR("PRI_WITH", "UVM_DO_PRI_WITH constraint mismatch");
    }
    if (on_it.first != 0x68u || on_it.second != 0x88u) {
      UVM_ERROR("ON_WITH", "UVM_DO_ON_WITH constraint mismatch");
    }
    if (plain_it.first != 0x90u || plain_it.second != 0x99u) {
      UVM_ERROR("PLAIN_SEQ", "Plain uvm_sequence UVM_DO mismatch");
    }
  }
};

int sc_main(int, char*[]) {
  crave::set_global_seed(42);
  uvm::run_test("crave2uvm_test_comp");
  return uvm::uvm_report_server::get_server()->get_severity_count(uvm::UVM_ERROR) > 0 ? 1 : 0;
}

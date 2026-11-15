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

// The values generated in a SystemC process must only depend on the seed and on what this process
// does, not on the other processes. The program prints the values of the process "observed", while
// the process "other" randomizes nothing (mode 0), objects of the same class (mode 1) or objects of
// another class (mode 2) before. The output has to be the same for all modes.

#include <crave/ConstrainedRandom.hpp>
#include <crave/experimental/Experimental.hpp>

#include <systemc>

#include <cstdlib>
#include <iostream>
#include <sstream>

using namespace crave;

struct packet : public crv_sequence_item {
  packet(crv_object_name) {}
  crv_variable<unsigned> addr, len;
  crv_vector<unsigned> data;
  crv_constraint con = {addr() <= 0xffff, 1 <= len() && len() <= 64, data().size() == 2,
                        foreach (data(), data()[_i] <= 0xff)};
};

struct other_item : public crv_sequence_item {
  other_item(crv_object_name) {}
  crv_variable<unsigned> a;
  crv_constraint con = {a() <= 77};
};

int mode = 0;

SC_MODULE(top) {
  SC_CTOR(top) {
    SC_THREAD(other);
    SC_THREAD(observed);
  }

  void other() {
    if (mode == 1) {
      packet p("p");
      for (int i = 0; i < 3; i++) p.randomize();
    }
    if (mode == 2) {
      other_item o("o");
      for (int i = 0; i < 3; i++) o.randomize();
    }
    wait(10, sc_core::SC_NS);
  }

  void observed() {
    wait(1, sc_core::SC_NS);
    packet p("p");
    std::ostringstream os;
    for (int i = 0; i < 10; i++) {
      bool ok = (i % 2) ? p.randomize() : p.randomize_with(p.len() > 2);
      os << " " << ok << "/" << p.addr << "/" << p.len << "/" << p.data[0] << "/" << p.data[1];
    }
    std::cout << "sequence:" << os.str() << std::endl;
  }
};

int sc_main(int argc, char* argv[]) {
  crave::set_global_seed(1234);
  if (argc > 1) mode = std::atoi(argv[1]);
  top t("top");
  sc_core::sc_start();
  return 0;
}

int main(int argc, char* argv[]) { return sc_core::sc_elab_and_sim(argc, argv); }

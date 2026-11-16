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


#include "../crave/backend/Randc.hpp"

#include <map>
#include <mutex>

namespace crave {

namespace {
std::mutex randc_mutex;

std::map<int, std::shared_ptr<RandcState> >& randc_states() {
  static std::map<int, std::shared_ptr<RandcState> > states;
  return states;
}
}  // namespace

void set_randc(int id, bool on) {
  std::lock_guard<std::mutex> lock(randc_mutex);
  if (!on) {
    randc_states().erase(id);
    return;
  }
  std::shared_ptr<RandcState>& state = randc_states()[id];
  if (!state) state = std::make_shared<RandcState>();
}

std::shared_ptr<RandcState> randc_state(int id) {
  std::lock_guard<std::mutex> lock(randc_mutex);
  std::map<int, std::shared_ptr<RandcState> >::const_iterator ite = randc_states().find(id);
  return ite == randc_states().end() ? std::shared_ptr<RandcState>() : ite->second;
}

}  // namespace crave

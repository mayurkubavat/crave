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


#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace crave {

/**
 * \ingroup setting
 * \brief The maximal width in bits of a cyclic random variable (randc).
 *
 * As IEEE 1800-2017 18.4.2 permits, the width of a randc variable is limited. The limit is 16 bits, more than
 * the 8 bits that SystemVerilog requires at least.
 */
unsigned const randc_max_bitsize = 16;

/**
 * \brief The number of values tried at random for a randc variable, before the solver lists the values.
 *
 * Up to this number of values, which have not been generated in the current cycle, are tried at random for a
 * randc variable. If none of them satisfies the constraints, the solver lists up to crave::randc_max_listed
 * unused values, which satisfy them, and one of them is chosen at random.
 */
unsigned const randc_max_tries = 8;

/**
 * \brief The maximal number of values of a cycle, which the solver lists for a randc variable at a time.
 *
 * If at most this number of values of a cycle satisfies the constraints, which is the case for a variable of up
 * to 8 bits, the cycle is a uniformly random permutation of them. Otherwise, the values, which are not found
 * by the tries at random, are chosen among the listed ones, the solver lists the next ones when they are used.
 */
unsigned const randc_max_listed = 256;

/**
 * \brief The number of lower bits of the values of a window, among which the solver lists the values of a randc
 * variable, see crave::randc_max_listed.
 *
 * Once more than crave::randc_max_listed values of a cycle have been used, the solver lists the values of a
 * variable of more bits window by window, among the values with the same upper bits, so that the cost of listing
 * them does not grow with the number of used values.
 */
unsigned const randc_window_bits = 10;

/**
 * \internal
 * The state of a cyclic random variable.
 */
struct RandcState {
  RandcState() : signature(), unused(), used(), listed(), complete(false), started(false) {}
  std::string signature;         // the constraints of the current cycle
  std::vector<uint64_t> unused;  // the values, which have not been generated in the current cycle
  std::vector<uint64_t> used;    // the values, which have been generated in the current cycle
  std::vector<uint64_t> listed;  // unused values, which the solver has found to satisfy the constraints
  bool complete;                 // whether all unused values, which satisfy the constraints, are listed
  bool started;
};

/**
 * \internal
 * Makes the variable with the given id a cyclic random variable or a random variable.
 */
void set_randc(int id, bool on);

/**
 * \internal
 * Gets the state of the cyclic random variable with the given id, or a null pointer if it is not cyclic.
 */
std::shared_ptr<RandcState> randc_state(int id);

}  // namespace crave

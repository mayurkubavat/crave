/****************************************************************************************
//	MIT License
//***************************************************************************************
//	Copyright (c) 2011-2016 University of Bremen, Germany.
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
****************************************************************************************/

#define BOOST_TEST_MODULE direct_Z3
#include <metaSMT/DirectSolver_Context.hpp>
#include <metaSMT/backend/Z3_Backend.hpp>

using namespace metaSMT::solver;
using namespace metaSMT;
struct Solver_Fixture {
  typedef DirectSolver_Context<Z3_Backend> ContextType;
  ContextType ctx;
};

#include "test_solver.cpp"
#include "test_QF_BV.cpp"
#include "test_QF_UF.cpp"
#include "test_Array.cpp"
#include "test_unsat.cpp"
#include "test_cardinality.cpp"
#include "test_annotate.cpp"
#include "test_evaluator.cpp"

BOOST_FIXTURE_TEST_SUITE(Z3_Specific, Solver_Fixture)

BOOST_AUTO_TEST_CASE( read_value_wide_bv_hex_and_bin )
{
  BOOST_CHECK_EQUAL(
      metaSMT::solver::detail::bv_literal_to_binary("#xFEDCBA98765432100123456789abcdef", 128),
      "1111111011011100101110101001100001110110010101000011001000010000"
      "0000000100100011010001010110011110001001101010111100110111101111");
  BOOST_CHECK_EQUAL(metaSMT::solver::detail::bv_literal_to_binary("#X5", 8), "00000101");
  BOOST_CHECK_EQUAL(metaSMT::solver::detail::bv_literal_to_binary("#x1f", 5), "11111");
  BOOST_CHECK_EQUAL(metaSMT::solver::detail::bv_literal_to_binary("#B101", 6), "000101");
  BOOST_CHECK_EQUAL(metaSMT::solver::detail::bv_literal_to_binary("(_ bv255 12)", 12), "000011111111");
  BOOST_CHECK_THROW(metaSMT::solver::detail::bv_literal_to_binary("#x", 8), std::runtime_error);
  BOOST_CHECK_THROW(metaSMT::solver::detail::bv_literal_to_binary("#x1G", 8), std::runtime_error);
  BOOST_CHECK_THROW(metaSMT::solver::detail::bv_literal_to_binary("#b102", 3), std::runtime_error);
  BOOST_CHECK_THROW(metaSMT::solver::detail::bv_literal_to_binary("(_ bv", 8), std::runtime_error);
  BOOST_CHECK_THROW(metaSMT::solver::detail::bv_literal_to_binary("(_ bvabc 12)", 12), std::runtime_error);
  BOOST_CHECK_THROW(metaSMT::solver::detail::bv_literal_to_binary("(_ bv255)", 12), std::runtime_error);
  BOOST_CHECK_THROW(metaSMT::solver::detail::bv_literal_to_binary("(_ bv255foo 12)", 12), std::runtime_error);
  BOOST_CHECK_THROW(metaSMT::solver::detail::bv_literal_to_binary("(_ bv255 )", 12), std::runtime_error);
  BOOST_CHECK_THROW(metaSMT::solver::detail::bv_literal_to_binary("(_ bv255 12", 12), std::runtime_error);
  BOOST_CHECK_THROW(metaSMT::solver::detail::bv_literal_to_binary("(_ bv255 12)x", 12), std::runtime_error);

  using namespace metaSMT::logic::QF_BV;

  bitvector bv128_hi = new_bitvector(128);
  bitvector bv128_lo = new_bitvector(128);
  bitvector bv65 = new_bitvector(65);
  bitvector bv68 = new_bitvector(68);
  bitvector bv129 = new_bitvector(129);
  bitvector bv256 = new_bitvector(256);

  assertion(ctx, equal(bv128_hi, bvhex("FEDCBA98765432100123456789ABCDEF")));
  assertion(ctx, equal(bv128_lo, bvuint(0x0123456789ABCDEFULL, 128)));
  assertion(ctx, equal(bv65, concat(bit1, bvuint(0x123456789ABCDEF0ULL, 64))));
  assertion(ctx, equal(bv68, bvhex("A123456789ABCDEF0")));
  assertion(ctx, equal(bv129, concat(bit1, bvhex("CAFEBABE1234567889ABCDEF01234567"))));
  assertion(ctx, equal(bv256, bvhex("0123456789ABCDEFFEDCBA9876543210A5A5A5A55A5A5A5A1122334455667788")));

  BOOST_REQUIRE(solve(ctx));

  std::string s128_hi = read_value(ctx, bv128_hi);
  BOOST_REQUIRE_EQUAL(s128_hi.size(), 128u);
  BOOST_REQUIRE_EQUAL(
      s128_hi,
      "1111111011011100101110101001100001110110010101000011001000010000"
      "0000000100100011010001010110011110001001101010111100110111101111");

  std::string s128_lo = read_value(ctx, bv128_lo);
  BOOST_REQUIRE_EQUAL(s128_lo.size(), 128u);
  BOOST_REQUIRE_EQUAL(
      s128_lo,
      std::string(64, '0') + "0000000100100011010001010110011110001001101010111100110111101111");

  std::string s65 = read_value(ctx, bv65);
  BOOST_REQUIRE_EQUAL(s65.size(), 65u);
  BOOST_REQUIRE_EQUAL(s65[0], '1');
  BOOST_REQUIRE_EQUAL(s65.substr(1), "0001001000110100010101100111100010011010101111001101111011110000");

  std::string s68 = read_value(ctx, bv68);
  BOOST_REQUIRE_EQUAL(s68.size(), 68u);
  BOOST_REQUIRE_EQUAL(
      s68,
      "10100001001000110100010101100111100010011010101111001101111011110000");

  std::string s129 = read_value(ctx, bv129);
  BOOST_REQUIRE_EQUAL(s129.size(), 129u);
  BOOST_REQUIRE_EQUAL(
      s129,
      "1"
      "1100101011111110101110101011111000010010001101000101011001111000"
      "1000100110101011110011011110111100000001001000110100010101100111");

  std::string s256 = read_value(ctx, bv256);
  BOOST_REQUIRE_EQUAL(s256.size(), 256u);
  BOOST_REQUIRE_EQUAL(
      s256,
      "0000000100100011010001010110011110001001101010111100110111101111"
      "1111111011011100101110101001100001110110010101000011001000010000"
      "1010010110100101101001011010010101011010010110100101101001011010"
      "0001000100100010001100110100010001010101011001100111011110001000");
}

BOOST_AUTO_TEST_SUITE_END() // Z3_Specific

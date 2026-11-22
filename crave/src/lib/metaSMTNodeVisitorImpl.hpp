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
#include <metaSMT/frontend/QF_BV.hpp>
#include <metaSMT/DirectSolver_Context.hpp>
#include <metaSMT/support/cardinality.hpp>
#include <metaSMT/support/contradiction_analysis.hpp>

#include <map>
#include <stack>
#include <utility>
#include <vector>
#include <string>
#include <algorithm>
#include <functional>

#include "../crave/ir/visitor/metaSMTNodeVisitor.hpp"
#include "../crave/frontend/AssignResult.hpp"
#include "../crave/frontend/RandomBase.hpp"
#include "RandomShuffle.hpp"

namespace crave {
namespace preds = metaSMT::logic;
namespace qf_bv = metaSMT::logic::QF_BV;
using metaSMT::evaluate;

template <typename SolverType>
class metaSMTVisitorImpl : public metaSMTVisitor {
 public:
  metaSMTVisitorImpl()
      : metaSMTVisitor(),
        solver_(),
        exprStack_(),
        terminals_(),
        softs_(),
        assumptions_(),
        suggestions_(),
        last_inactive_softs_() {}

  virtual void visitNode(Node const &);
  virtual void visitTerminal(Terminal const &);
  virtual void visitUnaryExpr(UnaryExpression const &);
  virtual void visitUnaryOpr(UnaryOperator const &);
  virtual void visitBinaryExpr(BinaryExpression const &);
  virtual void visitBinaryOpr(BinaryOperator const &);
  virtual void visitTernaryExpr(TernaryExpression const &);
  virtual void visitPlaceholder(Placeholder const &);
  virtual void visitVariableExpr(VariableExpr const &);
  virtual void visitConstant(Constant const &);
  virtual void visitVectorExpr(VectorExpr const &);
  virtual void visitVectorReduction(VectorReduction const &);
  virtual void visitNotOpr(NotOpr const &);
  virtual void visitNegOpr(NegOpr const &);
  virtual void visitComplementOpr(ComplementOpr const &);
  virtual void visitInside(Inside const &);
  virtual void visitExtendExpr(ExtendExpression const &);
  virtual void visitAndOpr(AndOpr const &);
  virtual void visitOrOpr(OrOpr const &);
  virtual void visitLogicalAndOpr(LogicalAndOpr const &);
  virtual void visitLogicalOrOpr(LogicalOrOpr const &);
  virtual void visitXorOpr(XorOpr const &);
  virtual void visitEqualOpr(EqualOpr const &);
  virtual void visitNotEqualOpr(NotEqualOpr const &);
  virtual void visitLessOpr(LessOpr const &);
  virtual void visitLessEqualOpr(LessEqualOpr const &);
  virtual void visitGreaterOpr(GreaterOpr const &);
  virtual void visitGreaterEqualOpr(GreaterEqualOpr const &);
  virtual void visitPlusOpr(PlusOpr const &);
  virtual void visitMinusOpr(MinusOpr const &);
  virtual void visitMultipliesOpr(MultipliesOpr const &);
  virtual void visitDevideOpr(DevideOpr const &);
  virtual void visitModuloOpr(ModuloOpr const &);
  virtual void visitShiftLeftOpr(ShiftLeftOpr const &);
  virtual void visitShiftRightOpr(ShiftRightOpr const &);
  virtual void visitVectorAccess(VectorAccess const &);
  virtual void visitIfThenElse(IfThenElse const &);
  virtual void visitForEach(ForEach const &);
  virtual void visitUnique(Unique const &);
  virtual void visitBitslice(Bitslice const &);
  virtual void visitRangeExpr(RangeExpr const &);
  virtual void visitTolPlusMinusExpr(TolPlusMinusExpr const &);
  virtual void visitTolPlusPctExpr(TolPlusPctExpr const &);
  virtual void visitVectorContains(VectorContains const &);

  virtual void makeAssertion(Node const &);
  virtual void makeSoftAssertion(Node const &);
  virtual void makeSuggestion(Node const &);
  virtual void makeAssumption(Node const &);
  virtual std::vector<unsigned int> analyseSofts();
  virtual std::vector<unsigned int> resolveSofts();
  virtual std::vector<unsigned int> const& getInactiveSofts() const;
  virtual std::vector<std::vector<unsigned int> > analyseContradiction(std::map<unsigned int, NodePtr> const &);
  virtual bool solve(bool ignoreSofts);
  virtual bool read(Node const &var, AssignResult &);
  virtual bool read(Node const &var, std::string &);
  virtual bool readVector(const std::vector<VariablePtr> &vec, __rand_vec_base *rand_vec);

 private:  // typedefs
  typedef typename SolverType::result_type result_type;
  typedef std::pair<result_type, bool> stack_entry;
  typedef std::map<int, result_type> result_map;

 private:  // methods
  inline void pop(stack_entry &fst);
  inline void pop2(stack_entry &fst, stack_entry &snd);
  inline void pop3(stack_entry &fst, stack_entry &snd, stack_entry &trd);
  void evalBinExpr(BinaryExpression const &expr, stack_entry &fst, stack_entry &snd);
  void evalTernExpr(TernaryExpression const &expr, stack_entry &fst, stack_entry &snd, stack_entry &trd);
  unsigned normalizeBv(stack_entry &se);
  result_type extendToWideSigned(result_type const &bv, unsigned w, bool s, unsigned target_w);
  static unsigned signedWidth(unsigned w, bool s) { return s ? w : w + 1; }
  static __int128 constantToInt128(Constant const &c);
  static unsigned unsignedBitsNeeded(unsigned __int128 v);
  static unsigned signedBitsNeeded(__int128 v);
  static std::string int128ToBvBin(__int128 v, unsigned width);

 private:  // data
  SolverType solver_;
  std::stack<stack_entry> exprStack_;
  result_map terminals_;
  std::vector<result_type> softs_;
  std::vector<result_type> assumptions_;
  std::vector<result_type> suggestions_;
  std::vector<unsigned int> last_inactive_softs_;
};

template <typename SolverType>
inline void metaSMTVisitorImpl<SolverType>::pop(stack_entry &fst) {
  assert(exprStack_.size() >= 1);
  fst = exprStack_.top();
  exprStack_.pop();
}

template <typename SolverType>
inline void metaSMTVisitorImpl<SolverType>::pop2(stack_entry &fst, stack_entry &snd) {
  assert(exprStack_.size() >= 2);
  fst = exprStack_.top();
  exprStack_.pop();
  snd = exprStack_.top();
  exprStack_.pop();
}

template <typename SolverType>
inline void metaSMTVisitorImpl<SolverType>::pop3(stack_entry &fst, stack_entry &snd, stack_entry &trd) {
  assert(exprStack_.size() >= 3);
  fst = exprStack_.top();
  exprStack_.pop();
  snd = exprStack_.top();
  exprStack_.pop();
  trd = exprStack_.top();
  exprStack_.pop();
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::evalBinExpr(BinaryExpression const &expr, stack_entry &fst, stack_entry &snd) {
  visitBinaryExpr(expr);
  pop2(snd, fst);
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::evalTernExpr(TernaryExpression const &expr, stack_entry &fst, stack_entry &snd,
                                                  stack_entry &trd) {
  visitTernaryExpr(expr);
  pop3(trd, snd, fst);
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitNode(Node const &) {}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitTerminal(Terminal const &) {}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitUnaryExpr(UnaryExpression const &ue) {
  ue.child()->visit(this);
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitUnaryOpr(UnaryOperator const &) {}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitBinaryExpr(BinaryExpression const &be) {
  be.lhs()->visit(this);
  be.rhs()->visit(this);
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitBinaryOpr(BinaryOperator const &) {}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitTernaryExpr(TernaryExpression const &te) {
  te.a()->visit(this);
  te.b()->visit(this);
  te.c()->visit(this);
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitPlaceholder(Placeholder const &) {}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitVariableExpr(VariableExpr const &ve) {
  typename result_map::iterator ite(terminals_.lower_bound(ve.id()));

  result_type result;
  if (ite != terminals_.end() && !(terminals_.key_comp()(ve.id(), ite->first))) {
    // &t already exists
    result = ite->second;
  } else {
    if (ve.isBool())
      result = evaluate(solver_, preds::new_variable());
    else
      result = evaluate(solver_, qf_bv::new_bitvector(ve.bitsize()));
    terminals_.insert(ite, std::make_pair(ve.id(), result));
  }

  exprStack_.push(std::make_pair(result, ve.sign()));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitConstant(Constant const &c) {
  result_type result = c.isBool() ? (c.value() ? evaluate(solver_, preds::True) : evaluate(solver_, preds::False))
                                  : (c.sign() ? evaluate(solver_, qf_bv::bvsint(c.value(), c.bitsize()))
                                              : evaluate(solver_, qf_bv::bvuint(c.value(), c.bitsize())));
  exprStack_.push(std::make_pair(result, c.sign()));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitNotOpr(NotOpr const &o) {
  visitUnaryExpr(o);

  stack_entry entry;
  pop(entry);

  result_type result = evaluate(solver_, preds::Not(entry.first));
  exprStack_.push(std::make_pair(result, entry.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitNegOpr(NegOpr const &o) {
  visitUnaryExpr(o);

  stack_entry entry;
  pop(entry);

  result_type result = evaluate(solver_, qf_bv::bvneg(entry.first));
  exprStack_.push(std::make_pair(result, entry.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitComplementOpr(ComplementOpr const &o) {
  visitUnaryExpr(o);

  stack_entry entry;
  pop(entry);

  result_type result = evaluate(solver_, qf_bv::bvnot(entry.first));
  exprStack_.push(std::make_pair(result, entry.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitInside(Inside const &in) {
  visitUnaryExpr(in);

  stack_entry entry;
  pop(entry);

  result_type result = evaluate(solver_, preds::False);
  for(Constant c : in.collection()) {
    stack_entry constExpr;
    c.visit(this);
    pop(constExpr);
    result = evaluate(solver_, preds::Or(result, preds::equal(entry.first, constExpr.first)));
  }

  exprStack_.push(std::make_pair(result, false));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitExtendExpr(ExtendExpression const &e) {
  visitUnaryExpr(e);

  stack_entry entry;
  pop(entry);

  result_type result;
  if (entry.second)
    result = evaluate(solver_, qf_bv::sign_extend(e.value(), entry.first));
  else
    result = evaluate(solver_, qf_bv::zero_extend(e.value(), entry.first));

  exprStack_.push(std::make_pair(result, entry.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitAndOpr(AndOpr const &o) {
  stack_entry fst, snd;
  evalBinExpr(o, fst, snd);

  result_type result = evaluate(solver_, qf_bv::bvand(fst.first, snd.first));
  exprStack_.push(std::make_pair(result, fst.second || snd.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitOrOpr(OrOpr const &o) {
  stack_entry fst, snd;
  evalBinExpr(o, fst, snd);

  result_type result = evaluate(solver_, qf_bv::bvor(fst.first, snd.first));
  exprStack_.push(std::make_pair(result, fst.second || snd.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitLogicalAndOpr(LogicalAndOpr const &o) {
  stack_entry fst, snd;
  evalBinExpr(o, fst, snd);

  result_type result = evaluate(solver_, preds::And(fst.first, snd.first));
  exprStack_.push(std::make_pair(result, fst.second || snd.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitLogicalOrOpr(LogicalOrOpr const &o) {
  stack_entry fst, snd;
  evalBinExpr(o, fst, snd);

  result_type result = evaluate(solver_, preds::Or(fst.first, snd.first));
  exprStack_.push(std::make_pair(result, fst.second || snd.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitXorOpr(XorOpr const &o) {
  stack_entry fst, snd;
  evalBinExpr(o, fst, snd);

  result_type result = evaluate(solver_, qf_bv::bvxor(fst.first, snd.first));
  exprStack_.push(std::make_pair(result, fst.second || snd.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitEqualOpr(EqualOpr const &o) {
  stack_entry fst, snd;
  evalBinExpr(o, fst, snd);

  result_type result = evaluate(solver_, preds::equal(fst.first, snd.first));
  exprStack_.push(std::make_pair(result, false));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitNotEqualOpr(NotEqualOpr const &o) {
  stack_entry fst, snd;
  evalBinExpr(o, fst, snd);

  result_type result = evaluate(solver_, preds::nequal(fst.first, snd.first));
  exprStack_.push(std::make_pair(result, false));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitLessOpr(LessOpr const &o) {
  stack_entry rhs_p, lhs_p;
  evalBinExpr(o, lhs_p, rhs_p);

  result_type lhs = lhs_p.first;
  result_type rhs = rhs_p.first;
  bool lhs_signed = lhs_p.second;
  bool rhs_signed = rhs_p.second;

  result_type result;
  if (lhs_signed && rhs_signed) {
    result = evaluate(solver_, qf_bv::bvslt(lhs, rhs));
  } else if (!(lhs_signed || rhs_signed)) {
    result = evaluate(solver_, qf_bv::bvult(lhs, rhs));
  } else if (lhs_signed) {
    result = evaluate(solver_, qf_bv::bvslt(qf_bv::sign_extend(1, lhs), qf_bv::zero_extend(1, rhs)));
  } else {
    result = evaluate(solver_, qf_bv::bvslt(qf_bv::zero_extend(1, lhs), qf_bv::sign_extend(1, rhs)));
  }
  exprStack_.push(std::make_pair(result, false));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitLessEqualOpr(LessEqualOpr const &o) {
  stack_entry rhs_p, lhs_p;
  evalBinExpr(o, lhs_p, rhs_p);

  result_type lhs = lhs_p.first;
  result_type rhs = rhs_p.first;
  bool lhs_signed = lhs_p.second;
  bool rhs_signed = rhs_p.second;

  result_type result;
  if (lhs_signed && rhs_signed) {
    result = evaluate(solver_, qf_bv::bvsle(lhs, rhs));
  } else if (!(lhs_signed || rhs_signed)) {
    result = evaluate(solver_, qf_bv::bvule(lhs, rhs));
  } else if (lhs_signed) {
    result = evaluate(solver_, qf_bv::bvsle(qf_bv::sign_extend(1, lhs), qf_bv::zero_extend(1, rhs)));
  } else {
    result = evaluate(solver_, qf_bv::bvsle(qf_bv::zero_extend(1, lhs), qf_bv::sign_extend(1, rhs)));
  }
  exprStack_.push(std::make_pair(result, false));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitGreaterOpr(GreaterOpr const &o) {
  stack_entry rhs_p, lhs_p;
  evalBinExpr(o, lhs_p, rhs_p);

  result_type lhs = lhs_p.first;
  result_type rhs = rhs_p.first;
  bool lhs_signed = lhs_p.second;
  bool rhs_signed = rhs_p.second;

  result_type result;
  if (lhs_signed && rhs_signed) {
    result = evaluate(solver_, qf_bv::bvsgt(lhs, rhs));
  } else if (!(lhs_signed || rhs_signed)) {
    result = evaluate(solver_, qf_bv::bvugt(lhs, rhs));
  } else if (lhs_signed) {
    result = evaluate(solver_, qf_bv::bvsgt(qf_bv::sign_extend(1, lhs), qf_bv::zero_extend(1, rhs)));
  } else {
    result = evaluate(solver_, qf_bv::bvsgt(qf_bv::zero_extend(1, lhs), qf_bv::sign_extend(1, rhs)));
  }
  exprStack_.push(std::make_pair(result, false));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitGreaterEqualOpr(GreaterEqualOpr const &o) {
  stack_entry rhs_p, lhs_p;
  evalBinExpr(o, lhs_p, rhs_p);

  result_type lhs = lhs_p.first;
  result_type rhs = rhs_p.first;
  bool lhs_signed = lhs_p.second;
  bool rhs_signed = rhs_p.second;

  result_type result;
  if (lhs_signed && rhs_signed) {
    result = evaluate(solver_, qf_bv::bvsge(lhs, rhs));
  } else if (!(lhs_signed || rhs_signed)) {
    result = evaluate(solver_, qf_bv::bvuge(lhs, rhs));
  } else if (lhs_signed) {
    result = evaluate(solver_, qf_bv::bvsge(qf_bv::sign_extend(1, lhs), qf_bv::zero_extend(1, rhs)));
  } else {
    result = evaluate(solver_, qf_bv::bvsge(qf_bv::zero_extend(1, lhs), qf_bv::sign_extend(1, rhs)));
  }
  exprStack_.push(std::make_pair(result, false));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitPlusOpr(PlusOpr const &o) {
  stack_entry fst, snd;
  evalBinExpr(o, fst, snd);

  result_type result = evaluate(solver_, qf_bv::bvadd(fst.first, snd.first));
  exprStack_.push(std::make_pair(result, fst.second || snd.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitMinusOpr(MinusOpr const &o) {
  stack_entry fst, snd;
  evalBinExpr(o, fst, snd);

  result_type result = evaluate(solver_, qf_bv::bvsub(fst.first, snd.first));
  exprStack_.push(std::make_pair(result, fst.second || snd.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitMultipliesOpr(MultipliesOpr const &o) {
  stack_entry fst, snd;
  evalBinExpr(o, fst, snd);

  result_type result = evaluate(solver_, qf_bv::bvmul(fst.first, snd.first));
  exprStack_.push(std::make_pair(result, fst.second || snd.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitDevideOpr(DevideOpr const &o) {
  stack_entry rhs_p, lhs_p;
  evalBinExpr(o, lhs_p, rhs_p);

  result_type lhs = lhs_p.first;
  result_type rhs = rhs_p.first;
  bool lhs_signed = lhs_p.second;
  bool rhs_signed = rhs_p.second;

  result_type result;
  if (lhs_signed && rhs_signed) {
    result = evaluate(solver_, qf_bv::bvsdiv(lhs, rhs));
  } else if (!(lhs_signed || rhs_signed)) {
    result = evaluate(solver_, qf_bv::bvudiv(lhs, rhs));
  } else {
    // FixWidthVisitor has extended the operands of a signed and an unsigned operand by a bit, the signed one by
    // its sign and the unsigned one by zero, so both values are the ones of the operands as signed values.
    // A division by zero gives what the backend gives for a signed division by zero, as before the extension:
    // with Z3, 1 for a negative dividend and -1 otherwise; with CUDD, -1 for a negative dividend and 1 otherwise.
    result = evaluate(solver_, qf_bv::bvsdiv(lhs, rhs));
  }
  exprStack_.push(std::make_pair(result, lhs_signed || rhs_signed));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitModuloOpr(ModuloOpr const &o) {
  stack_entry rhs_p, lhs_p;
  evalBinExpr(o, lhs_p, rhs_p);

  result_type lhs = lhs_p.first;
  result_type rhs = rhs_p.first;
  bool lhs_signed = lhs_p.second;

  result_type result;
  if (lhs_signed)
    result = evaluate(solver_, qf_bv::bvsrem(lhs, rhs));
  else
    result = evaluate(solver_, qf_bv::bvurem(lhs, rhs));

  exprStack_.push(std::make_pair(result, lhs_signed || rhs_p.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitShiftLeftOpr(ShiftLeftOpr const &o) {
  stack_entry fst, snd;
  evalBinExpr(o, fst, snd);

  result_type result = evaluate(solver_, qf_bv::bvshl(fst.first, snd.first));
  exprStack_.push(std::make_pair(result, fst.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitShiftRightOpr(ShiftRightOpr const &o) {
  stack_entry fst, snd;
  evalBinExpr(o, fst, snd);

  result_type result = evaluate(solver_, qf_bv::bvshr(fst.first, snd.first));
  exprStack_.push(std::make_pair(result, fst.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitIfThenElse(IfThenElse const &ite) {
  stack_entry fst, snd, trd;
  evalTernExpr(ite, fst, snd, trd);
  result_type result = evaluate(solver_, preds::Ite(preds::equal(fst.first, preds::True), snd.first, trd.first));
  exprStack_.push(std::make_pair(result, snd.second || trd.second));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitBitslice(Bitslice const &b) {
  visitUnaryExpr(b);

  stack_entry entry;
  pop(entry);

  result_type result = evaluate(solver_, qf_bv::extract(b.r(), b.l(), entry.first));

  exprStack_.push(std::make_pair(result, false));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitVectorAccess(VectorAccess const &) {
  throw std::runtime_error("VectorAccess is not allowed in metaSMTNodeVisitor.");
}
template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitVectorExpr(VectorExpr const &) {
  throw std::runtime_error("VectorExpr is not allowed in metaSMTNodeVisitor.");
}
template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitVectorReduction(VectorReduction const &) {
  throw std::runtime_error("VectorReduction is not allowed in metaSMTNodeVisitor.");
}
template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitForEach(ForEach const &) {
  throw std::runtime_error("ForEach is not allowed in metaSMTNodeVisitor.");
}
template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitUnique(Unique const &) {
  throw std::runtime_error("Unique is not allowed in metaSMTNodeVisitor.");
}
template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitVectorContains(VectorContains const &) {
  throw std::runtime_error("VectorContains is not allowed in metaSMTNodeVisitor.");
}

template <typename SolverType>
unsigned metaSMTVisitorImpl<SolverType>::normalizeBv(stack_entry &se) {
  unsigned w = solver_.get_bv_width(se.first);
  if (w == 0) {
    se.first = evaluate(solver_, preds::Ite(preds::equal(se.first, preds::True), qf_bv::bvuint(1, 1),
                                            qf_bv::bvuint(0, 1)));
    se.second = false;
    w = 1;
  }
  return w;
}

template <typename SolverType>
typename metaSMTVisitorImpl<SolverType>::result_type metaSMTVisitorImpl<SolverType>::extendToWideSigned(
    result_type const &bv, unsigned w, bool s, unsigned target_w) {
  if (w >= target_w) return bv;
  if (s) return evaluate(solver_, qf_bv::sign_extend(target_w - w, bv));
  return evaluate(solver_, qf_bv::zero_extend(target_w - w, bv));
}

template <typename SolverType>
__int128 metaSMTVisitorImpl<SolverType>::constantToInt128(Constant const &c) {
  if (c.isBool()) return c.value() ? 1 : 0;
  unsigned w = c.bitsize();
  unsigned __int128 raw = static_cast<unsigned __int128>(c.value());
  if (w > 0 && w < 64) raw &= ((static_cast<unsigned __int128>(1) << w) - 1);
  if (c.sign() && w > 0 && w <= 64) {
    unsigned __int128 sign_bit = static_cast<unsigned __int128>(1) << (w - 1);
    if (raw & sign_bit) raw |= (~static_cast<unsigned __int128>(0)) << w;
  }
  return static_cast<__int128>(raw);
}

template <typename SolverType>
unsigned metaSMTVisitorImpl<SolverType>::unsignedBitsNeeded(unsigned __int128 v) {
  unsigned b = 0;
  while (v > 0) {
    v >>= 1;
    ++b;
  }
  return b;
}

template <typename SolverType>
unsigned metaSMTVisitorImpl<SolverType>::signedBitsNeeded(__int128 v) {
  unsigned __int128 mag = v < 0 ? ~static_cast<unsigned __int128>(v) : static_cast<unsigned __int128>(v);
  return 1u + unsignedBitsNeeded(mag);
}

template <typename SolverType>
std::string metaSMTVisitorImpl<SolverType>::int128ToBvBin(__int128 v, unsigned width) {
  std::string bits(width, v < 0 ? '1' : '0');
  unsigned __int128 u = static_cast<unsigned __int128>(v);
  unsigned limit = std::min(width, 128u);
  for (unsigned i = 0; i < limit; ++i) {
    bits[width - 1 - i] = ((u >> i) & 1) ? '1' : '0';
  }
  return bits;
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitRangeExpr(RangeExpr const &r) {
  stack_entry e_p, lo_p, hi_p;
  evalTernExpr(r, e_p, lo_p, hi_p);
  unsigned const w_e = normalizeBv(e_p);
  unsigned const w_l = normalizeBv(lo_p);
  unsigned const w_h = normalizeBv(hi_p);
  unsigned const w_wide = std::max(signedWidth(w_e, e_p.second),
                                   std::max(signedWidth(w_l, lo_p.second), signedWidth(w_h, hi_p.second)));
  result_type e_w = extendToWideSigned(e_p.first, w_e, e_p.second, w_wide);
  result_type lo_w = extendToWideSigned(lo_p.first, w_l, lo_p.second, w_wide);
  result_type hi_w = extendToWideSigned(hi_p.first, w_h, hi_p.second, w_wide);
  result_type res = evaluate(solver_, preds::And(qf_bv::bvsle(lo_w, e_w), qf_bv::bvsle(e_w, hi_w)));
  exprStack_.push(std::make_pair(res, false));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitTolPlusMinusExpr(TolPlusMinusExpr const &t) {
  stack_entry e_p, val_p, tol_p;
  evalTernExpr(t, e_p, val_p, tol_p);
  unsigned const w_e = normalizeBv(e_p);
  unsigned const w_v = normalizeBv(val_p);
  unsigned const w_t = normalizeBv(tol_p);
  unsigned const w_vt = std::max(signedWidth(w_v, val_p.second), signedWidth(w_t, tol_p.second)) + 1;
  unsigned const w_wide = std::max(signedWidth(w_e, e_p.second), w_vt);
  result_type e_w = extendToWideSigned(e_p.first, w_e, e_p.second, w_wide);
  result_type val_w = extendToWideSigned(val_p.first, w_v, val_p.second, w_wide);
  result_type tol_w = extendToWideSigned(tol_p.first, w_t, tol_p.second, w_wide);
  result_type lo_w = evaluate(solver_, qf_bv::bvsub(val_w, tol_w));
  result_type hi_w = evaluate(solver_, qf_bv::bvadd(val_w, tol_w));
  result_type res = evaluate(solver_, preds::And(qf_bv::bvsle(lo_w, e_w), qf_bv::bvsle(e_w, hi_w)));
  exprStack_.push(std::make_pair(res, false));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::visitTolPlusPctExpr(TolPlusPctExpr const &t) {
  Constant const *val_c = dynamic_cast<Constant const *>(t.b().get());
  Constant const *pct_c = dynamic_cast<Constant const *>(t.c().get());

  if (val_c && pct_c) {
    __int128 const pct_i = constantToInt128(*pct_c);
    if (pct_i < 0) {
      exprStack_.push(std::make_pair(evaluate(solver_, preds::False), false));
      return;
    }
    t.a()->visit(this);
    stack_entry e_p;
    pop(e_p);
    unsigned const w_e = normalizeBv(e_p);
    __int128 const val_i = constantToInt128(*val_c);
    if (pct_i == 0) {
      unsigned const w_wide = std::max(signedWidth(w_e, e_p.second), signedBitsNeeded(val_i));
      result_type e_w = extendToWideSigned(e_p.first, w_e, e_p.second, w_wide);
      result_type val_bv = evaluate(solver_, qf_bv::bvbin(int128ToBvBin(val_i, w_wide)));
      exprStack_.push(std::make_pair(evaluate(solver_, preds::equal(e_w, val_bv)), false));
      return;
    }
    unsigned __int128 const mag =
        val_i < 0 ? static_cast<unsigned __int128>(-val_i) : static_cast<unsigned __int128>(val_i);
    __int128 const delta = static_cast<__int128>((mag * static_cast<unsigned __int128>(pct_i)) / 100);
    __int128 const lo_i = val_i - delta;
    __int128 const hi_i = val_i + delta;
    unsigned const w_wide =
        std::max(signedWidth(w_e, e_p.second), std::max(signedBitsNeeded(lo_i), signedBitsNeeded(hi_i)));
    result_type e_w = extendToWideSigned(e_p.first, w_e, e_p.second, w_wide);
    result_type lo_bv = evaluate(solver_, qf_bv::bvbin(int128ToBvBin(lo_i, w_wide)));
    result_type hi_bv = evaluate(solver_, qf_bv::bvbin(int128ToBvBin(hi_i, w_wide)));
    result_type res = evaluate(solver_, preds::And(qf_bv::bvsle(lo_bv, e_w), qf_bv::bvsle(e_w, hi_bv)));
    exprStack_.push(std::make_pair(res, false));
    return;
  }

  if (pct_c) {
    __int128 const pct_i = constantToInt128(*pct_c);
    if (pct_i < 0) {
      exprStack_.push(std::make_pair(evaluate(solver_, preds::False), false));
      return;
    }
    t.a()->visit(this);
    t.b()->visit(this);
    stack_entry val_p, e_p;
    pop2(val_p, e_p);
    unsigned const w_e = normalizeBv(e_p);
    unsigned const w_v = normalizeBv(val_p);
    if (pct_i == 0) {
      unsigned const w_wide = std::max(signedWidth(w_e, e_p.second), signedWidth(w_v, val_p.second));
      result_type e_w = extendToWideSigned(e_p.first, w_e, e_p.second, w_wide);
      result_type val_w = extendToWideSigned(val_p.first, w_v, val_p.second, w_wide);
      exprStack_.push(std::make_pair(evaluate(solver_, preds::equal(e_w, val_w)), false));
      return;
    }
    result_type mag = val_p.second
                          ? evaluate(solver_, preds::Ite(qf_bv::bvslt(val_p.first, qf_bv::bvsint(0, w_v)),
                                                         qf_bv::bvneg(val_p.first), val_p.first))
                          : val_p.first;
    if (pct_i == 100 || pct_i == 50 || pct_i == 25) {
      unsigned const w_wide =
          std::max(signedWidth(w_e, e_p.second), std::max(signedWidth(w_v, val_p.second), w_v + 1) + 1);
      result_type e_w = extendToWideSigned(e_p.first, w_e, e_p.second, w_wide);
      result_type val_w = extendToWideSigned(val_p.first, w_v, val_p.second, w_wide);
      result_type mag_w = extendToWideSigned(mag, w_v, false, w_wide);
      result_type delta_w = mag_w;
      if (pct_i == 50)
        delta_w = evaluate(solver_, qf_bv::bvshr(mag_w, qf_bv::bvuint(1, w_wide)));
      else if (pct_i == 25)
        delta_w = evaluate(solver_, qf_bv::bvshr(mag_w, qf_bv::bvuint(2, w_wide)));
      result_type lo_w = evaluate(solver_, qf_bv::bvsub(val_w, delta_w));
      result_type hi_w = evaluate(solver_, qf_bv::bvadd(val_w, delta_w));
      result_type res = evaluate(solver_, preds::And(qf_bv::bvsle(lo_w, e_w), qf_bv::bvsle(e_w, hi_w)));
      exprStack_.push(std::make_pair(res, false));
      return;
    }
    unsigned const w_p_eff = std::max(1u, unsignedBitsNeeded(static_cast<unsigned __int128>(pct_i)));
    unsigned const w_mul = std::max(w_v + w_p_eff, 7u);
    result_type mag_m = extendToWideSigned(mag, w_v, false, w_mul);
    result_type pct_m = evaluate(solver_, qf_bv::bvbin(int128ToBvBin(pct_i, w_mul)));
    result_type div_100 = evaluate(solver_, qf_bv::bvuint(100, w_mul));
    result_type delta = evaluate(solver_, qf_bv::bvudiv(qf_bv::bvmul(mag_m, pct_m), div_100));
    unsigned const w_wide =
        std::max(signedWidth(w_e, e_p.second), std::max(signedWidth(w_v, val_p.second), w_mul + 1) + 1);
    result_type e_w = extendToWideSigned(e_p.first, w_e, e_p.second, w_wide);
    result_type val_w = extendToWideSigned(val_p.first, w_v, val_p.second, w_wide);
    result_type delta_w = extendToWideSigned(delta, w_mul, false, w_wide);
    result_type lo_w = evaluate(solver_, qf_bv::bvsub(val_w, delta_w));
    result_type hi_w = evaluate(solver_, qf_bv::bvadd(val_w, delta_w));
    result_type res = evaluate(solver_, preds::And(qf_bv::bvsle(lo_w, e_w), qf_bv::bvsle(e_w, hi_w)));
    exprStack_.push(std::make_pair(res, false));
    return;
  }

  stack_entry e_p, val_p, pct_p;
  evalTernExpr(t, e_p, val_p, pct_p);
  unsigned const w_e = normalizeBv(e_p);
  unsigned const w_v = normalizeBv(val_p);
  unsigned const w_p = normalizeBv(pct_p);
  unsigned const w_mul = std::max(w_v + w_p, 7u);
  result_type mag = val_p.second
                        ? evaluate(solver_, preds::Ite(qf_bv::bvslt(val_p.first, qf_bv::bvsint(0, w_v)),
                                                       qf_bv::bvneg(val_p.first), val_p.first))
                        : val_p.first;
  result_type mag_m = extendToWideSigned(mag, w_v, false, w_mul);
  result_type pct_m = extendToWideSigned(pct_p.first, w_p, false, w_mul);
  result_type div_100 = evaluate(solver_, qf_bv::bvuint(100, w_mul));
  result_type delta = evaluate(solver_, qf_bv::bvudiv(qf_bv::bvmul(mag_m, pct_m), div_100));
  unsigned const w_wide =
      std::max(signedWidth(w_e, e_p.second), std::max(signedWidth(w_v, val_p.second), w_mul + 1) + 1);
  result_type e_w = extendToWideSigned(e_p.first, w_e, e_p.second, w_wide);
  result_type val_w = extendToWideSigned(val_p.first, w_v, val_p.second, w_wide);
  result_type delta_w = extendToWideSigned(delta, w_mul, false, w_wide);
  result_type lo_w = evaluate(solver_, qf_bv::bvsub(val_w, delta_w));
  result_type hi_w = evaluate(solver_, qf_bv::bvadd(val_w, delta_w));
  result_type in_bounds = evaluate(solver_, preds::And(qf_bv::bvsle(lo_w, e_w), qf_bv::bvsle(e_w, hi_w)));
  result_type res =
      pct_p.second ? evaluate(solver_, preds::And(qf_bv::bvsge(pct_p.first, qf_bv::bvsint(0, w_p)), in_bounds))
                   : in_bounds;
  exprStack_.push(std::make_pair(res, false));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::makeAssertion(Node const &expr) {
  expr.visit(this);

  stack_entry entry;
  pop(entry);

  metaSMT::assertion(solver_, preds::equal(entry.first, preds::True));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::makeSoftAssertion(Node const &expr) {
  expr.visit(this);

  stack_entry entry;
  pop(entry);

  softs_.push_back(evaluate(solver_, preds::equal(entry.first, preds::True)));
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::makeSuggestion(Node const &expr) {
  expr.visit(this);

  stack_entry entry;
  pop(entry);

  suggestions_.push_back(entry.first);
}

template <typename SolverType>
void metaSMTVisitorImpl<SolverType>::makeAssumption(Node const &expr) {
  expr.visit(this);

  stack_entry entry;
  pop(entry);

  assumptions_.push_back(evaluate(solver_, preds::equal(entry.first, preds::True)));
}

template <typename SolverType>
std::vector<unsigned int> metaSMTVisitorImpl<SolverType>::analyseSofts() {
  std::vector<unsigned int> result;
  // IEEE 1800-2023 18.5.14: last-added soft constraint has highest priority
  for (std::size_t idx = softs_.size(); idx > 0; --idx) {
    unsigned int const i = static_cast<unsigned int>(idx - 1);
    metaSMT::assumption(solver_, softs_[i]);
    if (metaSMT::solve(solver_)) {
      // accept
      metaSMT::assertion(solver_, softs_[i]);
    } else {
      // reject
      result.push_back(i);
    }
  }
  std::reverse(result.begin(), result.end());
  last_inactive_softs_ = result;
  softs_.clear();
  return result;
}

template <typename SolverType>
std::vector<unsigned int> metaSMTVisitorImpl<SolverType>::resolveSofts() {
  last_inactive_softs_.clear();
  if (softs_.empty()) {
    assumptions_.clear();
    return last_inactive_softs_;
  }

  for (result_type const & item : assumptions_) { metaSMT::assumption(solver_, item); }
  for (result_type const & item : softs_) { metaSMT::assumption(solver_, item); }
  if (metaSMT::solve(solver_)) {
    assumptions_.clear();
    return last_inactive_softs_;
  }

  if (!assumptions_.empty()) {
    for (result_type const & item : assumptions_) { metaSMT::assumption(solver_, item); }
    if (!metaSMT::solve(solver_)) {
      assumptions_.clear();
      return last_inactive_softs_;
    }
  }

  std::vector<result_type> active_softs;
  active_softs.reserve(softs_.size());
  for (std::size_t idx = softs_.size(); idx > 0; --idx) {
    unsigned int const i = static_cast<unsigned int>(idx - 1);
    for (result_type const & item : assumptions_) { metaSMT::assumption(solver_, item); }
    for (result_type const & item : active_softs) { metaSMT::assumption(solver_, item); }
    metaSMT::assumption(solver_, softs_[i]);
    if (metaSMT::solve(solver_)) {
      active_softs.push_back(softs_[i]);
    } else {
      last_inactive_softs_.push_back(i);
    }
  }
  std::reverse(last_inactive_softs_.begin(), last_inactive_softs_.end());
  assumptions_.clear();
  return last_inactive_softs_;
}

template <typename SolverType>
std::vector<unsigned int> const& metaSMTVisitorImpl<SolverType>::getInactiveSofts() const {
  return last_inactive_softs_;
}

template <typename SolverType>
std::vector<std::vector<unsigned int> > metaSMTVisitorImpl<SolverType>::analyseContradiction(
    std::map<unsigned int, NodePtr> const &s) {
  std::vector<std::vector<unsigned int> > results;
  std::map<unsigned int, result_type> result_map;

  typedef std::pair<unsigned int, NodePtr> NodePair;

  for(NodePair entry : s) {
    entry.second->visit(this);
    stack_entry st_entry;
    pop(st_entry);

    result_type var(evaluate(solver_, preds::new_variable()));
    metaSMT::assertion(solver_, preds::implies(var, st_entry.first));
    result_map.insert(std::make_pair(entry.first, var));
  }

  results = metaSMT::analyze_conflicts(solver_, result_map, metaSMT::evaluate(solver_, preds::True), results);

  return results;
}

template <typename SolverType>
bool metaSMTVisitorImpl<SolverType>::solve(bool ignoreSofts) {
  bool result = false;
  unsigned failures = 0;

  crave::random_shuffle(assumptions_.begin(), assumptions_.end());
  crave::random_shuffle(suggestions_.begin(), suggestions_.end());

  if (ignoreSofts || softs_.empty()) {
    while (true) {
      for(result_type const & item : assumptions_) { metaSMT::assumption(solver_, item); }

      for(result_type const & item : suggestions_) { metaSMT::assumption(solver_, item); }

      if (metaSMT::solve(solver_)) {
        result = true;
        break;
      }

      if (suggestions_.empty()) break;

      // Suggestions are dropped until the rest holds. To bound the number of calls of the solver, only
      // the first failures drop a single suggestion, after that half of the remaining ones are dropped.
      unsigned drop = ++failures <= 4 ? 1 : (suggestions_.size() + 1) / 2;
      for (unsigned i = 0; i < drop; i++) {
        unsigned select = crave::random_unsigned(suggestions_.size());
        std::swap(suggestions_[select], suggestions_.back());
        suggestions_.pop_back();
      }
    }

    assumptions_.clear();
    suggestions_.clear();

    return result;
  }

  last_inactive_softs_.clear();

  // Fast path: try assumptions_ + suggestions_ + all softs_
  for (result_type const & item : assumptions_) { metaSMT::assumption(solver_, item); }
  for (result_type const & item : suggestions_) { metaSMT::assumption(solver_, item); }
  for (result_type const & item : softs_) { metaSMT::assumption(solver_, item); }
  if (metaSMT::solve(solver_)) {
    assumptions_.clear();
    suggestions_.clear();
    return true;
  }

  // Slow path: resolve soft constraints against assumptions_ alone so suggestions_ are preserved
  std::vector<result_type> active_softs;
  bool all_softs_sat = false;
  if (!suggestions_.empty()) {
    for (result_type const & item : assumptions_) { metaSMT::assumption(solver_, item); }
    for (result_type const & item : softs_) { metaSMT::assumption(solver_, item); }
    if (metaSMT::solve(solver_)) {
      active_softs = softs_;
      all_softs_sat = true;
    }
  }

  if (!all_softs_sat) {
    if (!assumptions_.empty()) {
      for (result_type const & item : assumptions_) { metaSMT::assumption(solver_, item); }
      if (!metaSMT::solve(solver_)) {
        assumptions_.clear();
        suggestions_.clear();
        return false;
      }
    }

    active_softs.reserve(softs_.size());
    for (std::size_t idx = softs_.size(); idx > 0; --idx) {
      unsigned int const i = static_cast<unsigned int>(idx - 1);
      for (result_type const & item : assumptions_) { metaSMT::assumption(solver_, item); }
      for (result_type const & item : active_softs) { metaSMT::assumption(solver_, item); }
      metaSMT::assumption(solver_, softs_[i]);
      if (metaSMT::solve(solver_)) {
        active_softs.push_back(softs_[i]);
      } else {
        last_inactive_softs_.push_back(i);
      }
    }
    std::reverse(last_inactive_softs_.begin(), last_inactive_softs_.end());
  }

  // Run suggestion pruning loop (or single solve when suggestions_.empty()) so the solver stores a model
  // consistent with assumptions_ + all active_softs.
  while (true) {
    for (result_type const & item : assumptions_) { metaSMT::assumption(solver_, item); }
    for (result_type const & item : active_softs) { metaSMT::assumption(solver_, item); }
    for (result_type const & item : suggestions_) { metaSMT::assumption(solver_, item); }

    if (metaSMT::solve(solver_)) {
      result = true;
      break;
    }

    if (suggestions_.empty()) break;

    unsigned drop = ++failures <= 4 ? 1 : (suggestions_.size() + 1) / 2;
    for (unsigned i = 0; i < drop; i++) {
      unsigned select = crave::random_unsigned(suggestions_.size());
      std::swap(suggestions_[select], suggestions_.back());
      suggestions_.pop_back();
    }
  }

  assumptions_.clear();
  suggestions_.clear();

  return result;
}

template <typename SolverType>
bool metaSMTVisitorImpl<SolverType>::read(Node const &v, AssignResult &assign) {
  std::string res;
  if (read(v, res)) {
    assign.set_value(res);
    return true;
  }
  else
    return false;
}

template <typename SolverType>
bool metaSMTVisitorImpl<SolverType>::read(Node const &v, std::string &str) {
  // assert(static_cast<VariableExpr const *>(&var) != 0);
  VariableExpr const &var = *static_cast<VariableExpr const *>(&v);
  typename result_map::const_iterator ite(terminals_.find(var.id()));
  if (ite == terminals_.end()) return false;
  result_type var_expr = ite->second;
  std::string tmp = metaSMT::read_value(solver_, var_expr);
  str = tmp;
  return true;
}


template <typename SolverType>
bool metaSMTVisitorImpl<SolverType>::readVector(const std::vector<VariablePtr> &vec, __rand_vec_base *rand_vec) {
  std::vector<std::string> sv;
  for(VariablePtr var : vec) {
    typename result_map::const_iterator ite(terminals_.find(var->id()));
    if (ite == terminals_.end()) return false;

    result_type var_expr = ite->second;
    sv.push_back(metaSMT::read_value(solver_, var_expr));
  }

  rand_vec->set_values(sv);

  return true;
}

}  // namespace crave

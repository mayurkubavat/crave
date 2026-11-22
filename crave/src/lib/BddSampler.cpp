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

#include "BddSampler.hpp"

#include "../crave/backend/FactoryMetaSMT.hpp"
#include "../crave/backend/VariableDefaultSolver.hpp"
#include "../crave/ir/visitor/ComplexityEstimationVisitor.hpp"
#include "../crave/ir/visitor/FixWidthVisitor.hpp"
#include "../crave/ir/visitor/NodeVisitor.hpp"
#include "../crave/RandomSeedManager.hpp"
#include "../crave/utils/Logging.hpp"

#include <algorithm>
#include <exception>
#include <random>
#include <set>
#include <sstream>
#include <typeinfo>

namespace crave {

extern RandomSeedManager rng;

namespace {

// Writes an expression as a string, which does not depend on the ids of its variables.
class ShapeVisitor : public NodeVisitor {
 public:
  std::string shape(Node const& n) {
    os_.str("");
    n.visit(this);
    return os_.str();
  }

  std::set<int> const& vars() const { return vars_; }
  NodePtr var() const { return var_; }
  bool okay() const { return okay_; }
  bool hasBoundsCheck() const { return has_bounds_check_; }

  void visitNode(Node const& n) override;
  void visitTerminal(Terminal const& n) override;
  void visitUnaryExpr(UnaryExpression const& n) override;
  void visitUnaryOpr(UnaryOperator const& n) override;
  void visitBinaryExpr(BinaryExpression const& n) override;
  void visitBinaryOpr(BinaryOperator const& n) override;
  void visitTernaryExpr(TernaryExpression const& n) override;
  void visitPlaceholder(Placeholder const& n) override;
  void visitVariableExpr(VariableExpr const& n) override;
  void visitConstant(Constant const& n) override;
  void visitVectorExpr(VectorExpr const& n) override;
  void visitNotOpr(NotOpr const& n) override;
  void visitNegOpr(NegOpr const& n) override;
  void visitComplementOpr(ComplementOpr const& n) override;
  void visitInside(Inside const& n) override;
  void visitExtendExpr(ExtendExpression const& n) override;
  void visitAndOpr(AndOpr const& n) override;
  void visitOrOpr(OrOpr const& n) override;
  void visitLogicalAndOpr(LogicalAndOpr const& n) override;
  void visitLogicalOrOpr(LogicalOrOpr const& n) override;
  void visitLogicalIffOpr(LogicalIffOpr const& n) override;
  void visitXorOpr(XorOpr const& n) override;
  void visitEqualOpr(EqualOpr const& n) override;
  void visitNotEqualOpr(NotEqualOpr const& n) override;
  void visitLessOpr(LessOpr const& n) override;
  void visitLessEqualOpr(LessEqualOpr const& n) override;
  void visitGreaterOpr(GreaterOpr const& n) override;
  void visitGreaterEqualOpr(GreaterEqualOpr const& n) override;
  void visitPlusOpr(PlusOpr const& n) override;
  void visitMinusOpr(MinusOpr const& n) override;
  void visitMultipliesOpr(MultipliesOpr const& n) override;
  void visitDevideOpr(DevideOpr const& n) override;
  void visitModuloOpr(ModuloOpr const& n) override;
  void visitShiftLeftOpr(ShiftLeftOpr const& n) override;
  void visitShiftRightOpr(ShiftRightOpr const& n) override;
  void visitVectorAccess(VectorAccess const& n) override;
  void visitIfThenElse(IfThenElse const& n) override;
  void visitForEach(ForEach const& n) override;
  void visitUnique(Unique const& n) override;
  void visitBitslice(Bitslice const& n) override;
  void visitRangeExpr(RangeExpr const& n) override;
  void visitTolPlusMinusExpr(TolPlusMinusExpr const& n) override;
  void visitTolPlusPctExpr(TolPlusPctExpr const& n) override;
  void visitVectorContains(VectorContains const& n) override;
  void visitOneHotExpr(OneHotExpr const& n) override;
  void visitOneHot0Expr(OneHot0Expr const& n) override;
  void visitCountOnesExpr(CountOnesExpr const& n) override;
  void visitClog2Expr(Clog2Expr const& n) override;
  void visitSelfDeterminedExpr(SelfDeterminedExpr const& n) override;
  void visitBoundsCheckExpr(BoundsCheckExpr const& n) override;
  void visitVectorReduceWith(VectorReduceWith const& n) override;

 private:
  template <typename T>
  void unary(T const& n) {
    os_ << "(" << typeid(n).name() << " ";
    n.child()->visit(this);
    os_ << ")";
  }

  template <typename T>
  void binary(T const& n) {
    os_ << "(" << typeid(n).name() << " ";
    n.lhs()->visit(this);
    os_ << " ";
    n.rhs()->visit(this);
    os_ << ")";
  }

  template <typename T>
  void ternary(T const& n) {
    os_ << "(" << typeid(n).name() << " ";
    n.a()->visit(this);
    os_ << " ";
    n.b()->visit(this);
    os_ << " ";
    n.c()->visit(this);
    os_ << ")";
  }

  void constant(Constant const& c) { os_ << "c" << c.value() << ":" << c.bitsize() << (c.sign() ? "s" : "u"); }

  std::ostringstream os_;
  std::set<int> vars_;
  NodePtr var_;
  bool okay_ = true;
  bool has_bounds_check_ = false;
};

void ShapeVisitor::visitNode(Node const&) { okay_ = false; }
void ShapeVisitor::visitTerminal(Terminal const&) { okay_ = false; }
void ShapeVisitor::visitUnaryExpr(UnaryExpression const& n) { unary(n); }
void ShapeVisitor::visitUnaryOpr(UnaryOperator const& n) { unary(n); }
void ShapeVisitor::visitBinaryExpr(BinaryExpression const& n) { binary(n); }
void ShapeVisitor::visitBinaryOpr(BinaryOperator const& n) { binary(n); }
void ShapeVisitor::visitTernaryExpr(TernaryExpression const& n) { ternary(n); }
void ShapeVisitor::visitPlaceholder(Placeholder const&) { okay_ = false; }
void ShapeVisitor::visitVectorExpr(VectorExpr const&) { okay_ = false; }
void ShapeVisitor::visitVariableExpr(VariableExpr const& v) {
  if (vars_.insert(v.id()).second) var_ = new VariableExpr(v);
  os_ << "v:" << v.bitsize() << (v.sign() ? "s" : "u");
}
void ShapeVisitor::visitConstant(Constant const& c) { constant(c); }
void ShapeVisitor::visitInside(Inside const& n) {
  os_ << "(inside ";
  n.child()->visit(this);
  for (Constant const& c : n.collection()) {
    os_ << " ";
    constant(c);
  }
  os_ << ")";
}
void ShapeVisitor::visitExtendExpr(ExtendExpression const& n) {
  os_ << "(extend" << n.value() << " ";
  n.child()->visit(this);
  os_ << ")";
}
void ShapeVisitor::visitBitslice(Bitslice const& n) {
  os_ << "(slice" << n.r() << ":" << n.l() << " ";
  n.child()->visit(this);
  os_ << ")";
}
void ShapeVisitor::visitNotOpr(NotOpr const& n) { unary(n); }
void ShapeVisitor::visitNegOpr(NegOpr const& n) { unary(n); }
void ShapeVisitor::visitComplementOpr(ComplementOpr const& n) { unary(n); }
void ShapeVisitor::visitAndOpr(AndOpr const& n) { binary(n); }
void ShapeVisitor::visitOrOpr(OrOpr const& n) { binary(n); }
void ShapeVisitor::visitLogicalAndOpr(LogicalAndOpr const& n) { binary(n); }
void ShapeVisitor::visitLogicalOrOpr(LogicalOrOpr const& n) { binary(n); }
void ShapeVisitor::visitLogicalIffOpr(LogicalIffOpr const& n) { binary(n); }
void ShapeVisitor::visitXorOpr(XorOpr const& n) { binary(n); }
void ShapeVisitor::visitEqualOpr(EqualOpr const& n) { binary(n); }
void ShapeVisitor::visitNotEqualOpr(NotEqualOpr const& n) { binary(n); }
void ShapeVisitor::visitLessOpr(LessOpr const& n) { binary(n); }
void ShapeVisitor::visitLessEqualOpr(LessEqualOpr const& n) { binary(n); }
void ShapeVisitor::visitGreaterOpr(GreaterOpr const& n) { binary(n); }
void ShapeVisitor::visitGreaterEqualOpr(GreaterEqualOpr const& n) { binary(n); }
void ShapeVisitor::visitPlusOpr(PlusOpr const& n) { binary(n); }
void ShapeVisitor::visitMinusOpr(MinusOpr const& n) { binary(n); }
void ShapeVisitor::visitMultipliesOpr(MultipliesOpr const& n) { binary(n); }
void ShapeVisitor::visitDevideOpr(DevideOpr const& n) { binary(n); }
void ShapeVisitor::visitModuloOpr(ModuloOpr const& n) { binary(n); }
void ShapeVisitor::visitShiftLeftOpr(ShiftLeftOpr const& n) { binary(n); }
void ShapeVisitor::visitShiftRightOpr(ShiftRightOpr const& n) { binary(n); }
void ShapeVisitor::visitVectorAccess(VectorAccess const& n) { binary(n); }
void ShapeVisitor::visitIfThenElse(IfThenElse const& n) { ternary(n); }
void ShapeVisitor::visitForEach(ForEach const& n) { binary(n); }
void ShapeVisitor::visitUnique(Unique const& n) { unary(n); }
void ShapeVisitor::visitRangeExpr(RangeExpr const& n) { ternary(n); }
void ShapeVisitor::visitTolPlusMinusExpr(TolPlusMinusExpr const& n) { ternary(n); }
void ShapeVisitor::visitTolPlusPctExpr(TolPlusPctExpr const& n) { ternary(n); }
void ShapeVisitor::visitVectorContains(VectorContains const&) { okay_ = false; }
void ShapeVisitor::visitOneHotExpr(OneHotExpr const& n) { unary(n); }
void ShapeVisitor::visitOneHot0Expr(OneHot0Expr const& n) { unary(n); }
void ShapeVisitor::visitCountOnesExpr(CountOnesExpr const& n) { unary(n); }
void ShapeVisitor::visitClog2Expr(Clog2Expr const& n) { unary(n); }
void ShapeVisitor::visitSelfDeterminedExpr(SelfDeterminedExpr const& n) { unary(n); }
void ShapeVisitor::visitBoundsCheckExpr(BoundsCheckExpr const& n) {
  has_bounds_check_ = true;
  binary(n);
}
void ShapeVisitor::visitVectorReduceWith(VectorReduceWith const&) { okay_ = false; }

// restores the state of a random number generator, also if an exception is thrown
class GeneratorGuard {
 public:
  explicit GeneratorGuard(std::mt19937* generator) : generator_(generator), saved_(*generator) {}
  ~GeneratorGuard() { *generator_ = saved_; }

  GeneratorGuard(GeneratorGuard const&) = delete;
  GeneratorGuard& operator=(GeneratorGuard const&) = delete;

 private:
  std::mt19937* generator_;
  std::mt19937 saved_;
};

}  // namespace

unsigned BddSampler::max_nodes = 100000;
unsigned BddSampler::max_recent = 16;

std::mutex BddSampler::mutex_;
std::map<std::string, std::weak_ptr<BddSampler> > BddSampler::samplers_;
std::list<std::shared_ptr<BddSampler> > BddSampler::recent_;
std::set<std::string> BddSampler::failed_;
unsigned BddSampler::failed_max_nodes_ = 0;

std::shared_ptr<BddSampler> BddSampler::get(std::vector<NodePtr> const& constraints, bool create) {
  if (!FactoryMetaSMT::isDefined(CUDD)) return nullptr;

  ShapeVisitor visitor;
  std::vector<std::string> shapes;
  std::vector<NodePtr> simple_constraints;
  for (NodePtr c : constraints) {
    unsigned complexity = ComplexityEstimationVisitor().getComplexityEstimation(*c);
    if (complexity == 0 || complexity >= VariableDefaultSolver::complexity_limit_for_bdd) continue;
    shapes.push_back(visitor.shape(*c));
    simple_constraints.push_back(c);
  }
  if (simple_constraints.empty() || !visitor.okay() || visitor.vars().size() != 1) return nullptr;
  NodePtr target_var = visitor.var();
  if (visitor.hasBoundsCheck()) {
    ShapeVisitor lowered_visitor;
    std::vector<std::string> lowered_shapes;
    std::vector<NodePtr> lowered_constraints;
    for (NodePtr c : simple_constraints) {
      FixWidthVisitor fwv;
      NodePtr lowered = fwv.fixWidth(*c);
      unsigned complexity = ComplexityEstimationVisitor().getComplexityEstimation(*lowered);
      if (complexity == 0 || complexity >= VariableDefaultSolver::complexity_limit_for_bdd) continue;
      lowered_shapes.push_back(lowered_visitor.shape(*lowered));
      lowered_constraints.push_back(lowered);
    }
    if (lowered_constraints.empty() || !lowered_visitor.okay() || lowered_visitor.vars().size() != 1) return nullptr;
    shapes.swap(lowered_shapes);
    simple_constraints.swap(lowered_constraints);
    target_var = lowered_visitor.var();
  }

  std::sort(shapes.begin(), shapes.end());
  std::string key;
  for (std::string const& s : shapes) key += s + ";";

  std::lock_guard<std::mutex> lock(mutex_);

  // whether a BDD can be built depends on the limit for its size
  if (failed_max_nodes_ != max_nodes) {
    failed_.clear();
    failed_max_nodes_ = max_nodes;
  }
  if (failed_.count(key)) return nullptr;

  std::shared_ptr<BddSampler> sampler;
  auto entry = samplers_.find(key);
  if (entry != samplers_.end()) sampler = entry->second.lock();
  if (!sampler) {
    if (!create) return nullptr;

    SolverPtr solver;
    {
      // Checking for a solution picks one at random. The caller must get the same random numbers
      // afterwards, whether the sampler has to be built or exists already.
      GeneratorGuard guard(rng.get());
      try {
        solver.reset(FactoryMetaSMT::getNewBddSampler(max_nodes));
        for (NodePtr c : simple_constraints) solver->makeAssertion(*c);
        if (!solver->solve()) solver.reset();
      }
      catch (std::exception const& e) {
        LOG(INFO) << "No BDD for " << key << " (" << e.what() << ")";
        solver.reset();
      }
    }

    if (!solver) {
      if (failed_.size() >= max_failed_) failed_.clear();
      failed_.insert(key);
      return nullptr;
    }

    sampler.reset(new BddSampler());
    sampler->solver_ = solver;
    sampler->var_ = target_var;

    // forget the samplers, which are not in use anymore
    for (auto ite = samplers_.begin(); ite != samplers_.end();) {
      if (ite->second.expired())
        ite = samplers_.erase(ite);
      else
        ++ite;
    }
    samplers_[key] = sampler;
  }

  // Keep the sampler for a while, since the same constraints are likely to be requested again,
  // e.g. by the next call of randomize_with().
  recent_.remove(sampler);
  recent_.push_front(sampler);
  while (recent_.size() > max_recent) recent_.pop_back();

  return sampler;
}

std::size_t BddSampler::count() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::size_t n = 0;
  for (auto const& entry : samplers_)
    if (!entry.second.expired()) n++;
  return n;
}

bool BddSampler::sample(std::string* bits) {
  std::lock_guard<std::mutex> lock(mutex_);
  return solver_->solve() && solver_->read(*var_, *bits);
}

}  // namespace crave

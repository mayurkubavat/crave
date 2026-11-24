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

#include "../crave/ir/ConstraintManager.hpp"
#include "../crave/ir/visitor/GetSupportSetVisitor.hpp"
#include "../crave/ir/visitor/ToDotNodeVisitor.hpp"
#include "../crave/ir/visitor/ComplexityEstimationVisitor.hpp"
#include "../crave/frontend/RandomBase.hpp"
#include "../crave/utils/Logging.hpp"

#include <atomic>
#include <mutex>

namespace crave {

template <typename ostream>
ostream& operator<<(ostream& os, const ConstraintManager& set) {
  os << "Set " << set.id_ << " has " << set.constraints_.size() << " constraint(s) and has "
     << (set.changed_ ? "" : "not ") << "changed" << std::endl;
  for(ConstraintPtr item : set.constraints_) { os << item << std::endl; }
  os << std::flush;
  return os;
}
template std::ostream& operator<<<std::ostream>(std::ostream& os, const ConstraintManager& set);

ConstraintManager::ConstraintManager() : changed_(false) {
  static std::atomic<unsigned> ID{0};
  id_ = ++ID;
}

bool ConstraintManager::enableConstraint(std::string const& key) {
  LOG(INFO) << "Enable constraint " << key << " in set " << id_ << ": ";
  ConstraintMap::iterator ite = constr_map_.find(key);
  if (ite != constr_map_.end()) {
    if (!ite->second->isEnabled()) {
      LOG(INFO) << "  ok";
      ite->second->enable();
      changed_ = true;
    } else {
      LOG(INFO) << "  already enabled";
    }
    return true;
  }
  LOG(INFO) << "not found";
  return false;
}

bool ConstraintManager::disableConstraint(std::string const& key) {
  LOG(INFO) << "Disable constraint " << key << " in set " << id_ << ": ";
  ConstraintMap::iterator ite = constr_map_.find(key);
  if (ite != constr_map_.end()) {
    if (ite->second->isEnabled()) {
      LOG(INFO) << "  ok";
      ite->second->disable();
      changed_ = true;
    } else {
      LOG(INFO) << "  already enabled";
    }
    return true;
  }
  LOG(INFO) << "  not found";
  return false;
}

bool ConstraintManager::isConstraintEnabled(std::string const& key) {
  ConstraintMap::iterator ite = constr_map_.find(key);
  return ite != constr_map_.end() && ite->second->isEnabled();
}

bool ConstraintManager::isChanged() const { return changed_; }

void ConstraintManager::resetChanged() { changed_ = false; }

ConstraintPtr ConstraintManager::makeConstraint(std::string const& name, int c_id, NodePtr n, Context* ctx,
                                                bool const soft, bool const cover) {
  LOG(INFO) << "New " << (soft ? "soft " : "") << (cover ? "cover " : "") << "constraint " << name << " in set " << id_;

  if (constr_map_.find(name) != constr_map_.end()) {
    throw std::runtime_error(name + "Constraint already exists.");
  }

  GetSupportSetVisitor gssv;
  n->visit(&gssv);

  ConstraintPtr c;

  // A reduction of the elements of a vector is solved by the solver of the vector, after the size of the
  // vector has been solved. So a constraint with a reduction may refer to a single vector and scalar variables/references.
  bool const reduction = !gssv.getReducedVectors().empty();
  if (auto fe = boost::dynamic_pointer_cast<ForEach>(n)) {
    auto vec = boost::dynamic_pointer_cast<VectorExpr>(fe->lhs());
    if (!vec || gssv.getVectorVars().size() != 1 || *gssv.getVectorVars().begin() != static_cast<int>(vec->id()))
      throw std::runtime_error("Constraint " + name + ": foreach may refer to the elements of one vector only.");
    c = std::make_shared<UserVectorConstraint>(c_id, n, name, gssv.getSupportVars(), vec->id(), false, soft, cover);
  } else if (auto u = boost::dynamic_pointer_cast<Unique>(n)) {
    auto vec = boost::dynamic_pointer_cast<VectorExpr>(u->child());
    if (!vec || gssv.getVectorVars().size() != 1 || *gssv.getVectorVars().begin() != static_cast<int>(vec->id()))
      throw std::runtime_error("Constraint " + name + ": unique may refer to one vector only.");
    c = std::make_shared<UserVectorConstraint>(c_id, n, name, gssv.getSupportVars(), vec->id(), true, soft, cover);
  } else if (reduction) {
    if (gssv.getReducedVectors().size() != 1 || gssv.getVectorVars().size() != 1)
      throw std::runtime_error("Constraint " + name +
                               ": a constraint with a reduction of a vector may refer to the elements of that vector only.");
    int const vector_id = *gssv.getReducedVectors().begin();
    bool has_size_var = false;
    {
      std::lock_guard<std::recursive_mutex> vbm_lock(vector_base_map_mutex());
      auto vec_ite = vectorBaseMap.find(vector_id);
      has_size_var = (vec_ite != vectorBaseMap.end() && gssv.getSupportVars().count(vec_ite->second->size_var().id()));
    }
    if (has_size_var)
      throw std::runtime_error("Constraint " + name +
                               ": a constraint with a reduction of a vector may not refer to the size of that vector.");
    c = std::make_shared<UserVectorConstraint>(c_id, n, name, gssv.getSupportVars(), vector_id, false, soft, cover,
                                               true, true);
  } else {
    c = std::make_shared<UserConstraint>(c_id, n, name, gssv.getSupportVars(), soft, cover);
  }

  assert(!c->isSoft() || !c->isCover());              // soft cover constraint not defined/supported yet
  assert(!c->isVectorConstraint() || !c->isCover());  // cover vector constraint not defined/supported yet

  if (!c->isVectorConstraint()) {
    ComplexityEstimationVisitor cev;
    c->complexity_ = cev.getComplexityEstimation(*n);
  }

  changed_ = true;
  constraints_.push_back(c);

  return constr_map_[name] = c;
}

std::ostream& ConstraintManager::printDotGraph(std::ostream& os) {
  ToDotVisitor visitor(os);
  for(ConstraintPtr c : constraints_) {
    size_t a = reinterpret_cast<size_t>(&*c);
    size_t b = reinterpret_cast<size_t>(&(*c->expr()));
    os << "\t" << a << " [label=\"" << c->name() << (c->isSoft() ? " soft" : "") << (c->isCover() ? " cover" : "")
       << (!c->isEnabled() ? " disabled" : "") << "\"]" << std::endl;
    os << "\t" << a << " -> " << b << std::endl;
    c->expr()->visit(&visitor);
  }
  return os;
}
}

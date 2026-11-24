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


#include "../crave/ir/visitor/metaSMTNodeVisitor.hpp"
#include "../crave/backend/FactoryMetaSMT.hpp"
#include "../crave/utils/Logging.hpp"
#include "metaSMTNodeVisitorImpl.hpp"

#include <metaSMT/BitBlast.hpp>
#include <metaSMT/DirectSolver_Context.hpp>

#include <mutex>
#include <random>
#include <string>

#define DEFINE_SOLVER(SOLVER_ENUM, SOLVER_T)                                                     \
  namespace crave {                                                                              \
  template <>                                                                                    \
  struct FactorySolver<SOLVER_ENUM> {                                                            \
    typedef metaSMT::DirectSolver_Context<SOLVER_T> SolverType;                                  \
    static bool isDefined() { return true; }                                                     \
    static metaSMTVisitor* getNewInstance() { return new metaSMTVisitorImpl<SolverType>(); }     \
  };                                                                                             \
  }  // namespace crave

#ifdef metaSMT_USE_Boolector
#include <metaSMT/backend/Boolector.hpp>
namespace crave {
class BoolectorSolver : public metaSMT::solver::Boolector {
 public:
  BoolectorSolver() {
#ifndef metaSMT_BOOLECTOR_1_API
    boolector_set_opt(_btor, BTOR_OPT_REWRITE_LEVEL, 1);
//    assert(boolector_set_sat_solver_minisat(_btor));
#endif
  }
};
}
DEFINE_SOLVER(crave::BOOLECTOR, crave::BoolectorSolver);
#endif

#ifdef metaSMT_USE_CVC4
#include <metaSMT/backend/CVC4.hpp>
DEFINE_SOLVER(crave::CVC4, metaSMT::solver::CVC4);
#endif

#ifdef metaSMT_USE_STP
#include <metaSMT/backend/STP.hpp>
DEFINE_SOLVER(crave::STP, metaSMT::solver::STP);
#endif

#ifdef metaSMT_USE_SWORD
#include <metaSMT/backend/SWORD_Backend.hpp>
DEFINE_SOLVER(crave::SWORD, metaSMT::solver::SWORD_Backend);
#endif

#ifdef metaSMT_USE_Z3
#include <metaSMT/backend/Z3_Backend.hpp>
namespace crave {
template <>
struct SolverMutexTraits<metaSMT::DirectSolver_Context<metaSMT::solver::Z3_Backend> > {
  static std::recursive_mutex& mutex() {
    static std::recursive_mutex* m = new std::recursive_mutex();
    return *m;
  }
  struct Guard {
    std::lock_guard<std::recursive_mutex> lock;
    Guard() : lock(mutex()) {}
  };
};
}
DEFINE_SOLVER(crave::Z3, metaSMT::solver::Z3_Backend);
#endif

#ifdef metaSMT_USE_CUDD
#include <metaSMT/backend/CUDD_Distributed.hpp>
#include "../crave/RandomSeedManager.hpp"
namespace crave {
extern RandomSeedManager rng;
class CUDD_Solver : public metaSMT::solver::CUDD_Distributed {
 public:
  explicit CUDD_Solver(unsigned cache_slots = CUDD_CACHE_SLOTS) : metaSMT::solver::CUDD_Distributed(cache_slots) {}

 protected:
  // use the random number generator of CRAVE, so that the solutions only depend on the global seed
  double uniform01() override {
    return with_active_or_global_rng([](std::mt19937& g) { return std::uniform_real_distribution<double>(0, 1)(g); });
  }
};

// CUDD_Solver with a small computed table and a limit for the number of nodes
class CUDD_Sampler : public CUDD_Solver {
 public:
  CUDD_Sampler() : CUDD_Solver(1u << 12), max_nodes_(max_nodes) {
    // CUDD checks this whenever it needs memory for more nodes, the operation then fails with an exception.
    // Cudd_SetMaxLive() cannot be used instead: the C++ interface of CUDD does not report that this limit
    // is exceeded, it returns a BDD without node.
    _manager.RegisterTerminationCallback(&CUDD_Sampler::tooManyNodes, this);
  }

  static thread_local unsigned max_nodes;

 private:
  static int tooManyNodes(const void* arg) {
    const CUDD_Sampler* self = static_cast<const CUDD_Sampler*>(arg);
    DdManager* manager = self->_manager.getManager();
    return Cudd_ReadKeys(manager) - Cudd_ReadDead(manager) > self->max_nodes_;
  }

  unsigned max_nodes_;
};
thread_local unsigned CUDD_Sampler::max_nodes = ~0u;
}
DEFINE_SOLVER(crave::CUDD, metaSMT::BitBlast<crave::CUDD_Solver>);
#endif

#undef DEFINE_SOLVER

namespace crave {

namespace {
std::recursive_mutex& factoryMutex() {
  static std::recursive_mutex* m = new std::recursive_mutex();
  return *m;
}
}  // namespace

SolverTypes FactoryMetaSMT::solver_type_ = UNDEFINED_SOLVER;  // default solver

void FactoryMetaSMT::setSolverType(std::string const& type) {
  std::lock_guard<std::recursive_mutex> lock(factoryMutex());
  if (type == "Boolector")
    solver_type_ = BOOLECTOR;
  else if (type == "CVC4")
    solver_type_ = CVC4;
  else if (type == "STP")
    solver_type_ = STP;
  else if (type == "SWORD")
    solver_type_ = SWORD;
  else if (type == "Yices2")
    solver_type_ = YICES2;
  else if (type == "Z3")
    solver_type_ = Z3;
  else if (type == "Cudd")
    solver_type_ = CUDD;
}

bool FactoryMetaSMT::isDefined(SolverTypes type) {
  switch (type) {
    case BOOLECTOR:
      return FactorySolver<BOOLECTOR>::isDefined();
    case SWORD:
      return FactorySolver<SWORD>::isDefined();
    case STP:
      return FactorySolver<STP>::isDefined();
    case YICES2:
      return FactorySolver<YICES2>::isDefined();
    case Z3:
      return FactorySolver<Z3>::isDefined();
    case CVC4:
      return FactorySolver<CVC4>::isDefined();
    case CUDD:
      return FactorySolver<CUDD>::isDefined();
    default:
      return false;
  }
}

metaSMTVisitor* FactoryMetaSMT::getNewBddSampler(unsigned max_nodes) {
  std::lock_guard<std::recursive_mutex> lock(factoryMutex());
#ifdef metaSMT_USE_CUDD
  CUDD_Sampler::max_nodes = max_nodes;
  return new metaSMTVisitorImpl<metaSMT::DirectSolver_Context<metaSMT::BitBlast<CUDD_Sampler> > >();
#else
  return NULL;
#endif
}

#define TRY_GET_SOLVER(solver)                                                        \
  if (!FactorySolver<solver>::isDefined()) {                                          \
    solver_type_ = UNDEFINED_SOLVER;                                                  \
    LOG(INFO) << #solver << " has not been defined, another solver will be selected"; \
    return getNewInstance();                                                          \
  } else {                                                                            \
    return FactorySolver<solver>::getNewInstance();                                   \
  }

#define TRY_GET_SOLVER_WHEN_UNDEFINED(solver)                   \
  if (FactorySolver<solver>::isDefined()) {                     \
    solver_type_ = solver;                                      \
    LOG(INFO) << #solver << " has been automatically selected"; \
    return getNewInstance();                                    \
  }

metaSMTVisitor* FactoryMetaSMT::getNewInstance(SolverTypes type) {
  std::lock_guard<std::recursive_mutex> lock(factoryMutex());
  switch (type) {
    case BOOLECTOR:
      TRY_GET_SOLVER(BOOLECTOR);
    case SWORD:
      TRY_GET_SOLVER(SWORD);
    case STP:
      TRY_GET_SOLVER(STP);
    case YICES2:
      TRY_GET_SOLVER(YICES2);
    case Z3:
      TRY_GET_SOLVER(Z3);
    case CVC4:
      TRY_GET_SOLVER(CVC4);
    case CUDD:
      TRY_GET_SOLVER(CUDD);
    default:  // UNDEFINED_SOLVER
      TRY_GET_SOLVER_WHEN_UNDEFINED(BOOLECTOR);
      TRY_GET_SOLVER_WHEN_UNDEFINED(SWORD);
      TRY_GET_SOLVER_WHEN_UNDEFINED(STP);
      TRY_GET_SOLVER_WHEN_UNDEFINED(YICES2);
      TRY_GET_SOLVER_WHEN_UNDEFINED(Z3);
      TRY_GET_SOLVER_WHEN_UNDEFINED(CVC4);
      TRY_GET_SOLVER_WHEN_UNDEFINED(CUDD);
      throw std::runtime_error("No solver has been defined.");  
  }
}

SolverTypes FactoryMetaSMT::effectiveSolverType() {
  std::lock_guard<std::recursive_mutex> lock(factoryMutex());
  if (solver_type_ != UNDEFINED_SOLVER && isDefined(solver_type_)) return solver_type_;
  SolverTypes const candidates[] = {BOOLECTOR, SWORD, STP, YICES2, Z3, CVC4, CUDD};
  for (SolverTypes s : candidates) {
    if (isDefined(s)) return s;
  }
  return UNDEFINED_SOLVER;
}

}  // namespace crave

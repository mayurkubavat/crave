//****************************************************************************************
//	MIT License
//****************************************************************************************
//	Copyright (c) 2012-2020 University of Bremen, Germany. 
//  	Copyright (c) 2015-2020 DFKI GmbH Bremen, Germany.
//  	Copyright (c) 2020 Johannes Kepler University Linz, Austria.
//   Copyright (c) 2022 - 2023 Coseda Technologies GmbH.
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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Coverage.hpp"
#include "Expression.hpp"
#include "Object.hpp"
#include "../ir/VariableContainer.hpp"

namespace crave {
struct Generator;
struct RandWithCache;

namespace detail {
struct InlineReadCapture {
  int id;
  std::shared_ptr<ReferenceExpression> ref_expr;
};

struct InlineDistCapture {
  int id;
  std::shared_ptr<ReferenceExpression> ref_expr;
  int target_var_id;
  bool is_implicit;
};

class InlineContainerGuard {
 public:
  InlineContainerGuard()
      : vcon_(variable_container()),
        reads_0_(vcon_->read_references.size()),
        dists_0_(vcon_->dist_references.size()),
        rolled_back_(false) {}

  InlineContainerGuard(InlineContainerGuard const&) = delete;
  InlineContainerGuard& operator=(InlineContainerGuard const&) = delete;

  ~InlineContainerGuard() { rollback(); }

  void capture_and_rollback(std::vector<InlineReadCapture>& out_reads,
                            std::vector<InlineDistCapture>& out_dists) {
    out_reads.reserve(vcon_->read_references.size() - reads_0_);
    for (std::size_t i = reads_0_; i < vcon_->read_references.size(); ++i) {
      out_reads.push_back(InlineReadCapture{vcon_->read_references[i].first, vcon_->read_references[i].second});
    }
    out_dists.reserve(vcon_->dist_references.size() - dists_0_);
    for (std::size_t i = dists_0_; i < vcon_->dist_references.size(); ++i) {
      int id = vcon_->dist_references[i].first;
      auto map_it = vcon_->dist_ref_to_var_map.find(id);
      int target_var_id = (map_it != vcon_->dist_ref_to_var_map.end()) ? map_it->second : -1;
      bool is_implicit = vcon_->implicit_dist_references.count(id) != 0;
      out_dists.push_back(
          InlineDistCapture{id, vcon_->dist_references[i].second, target_var_id, is_implicit});
    }
    rollback();
  }

  void rollback() {
    if (rolled_back_) return;
    rolled_back_ = true;
    for (std::size_t i = reads_0_; i < vcon_->read_references.size(); ++i) {
      vcon_->variables.erase(vcon_->read_references[i].first);
    }
    vcon_->read_references.resize(reads_0_);
    for (std::size_t i = dists_0_; i < vcon_->dist_references.size(); ++i) {
      int id = vcon_->dist_references[i].first;
      vcon_->variables.erase(id);
      vcon_->dist_ref_to_var_map.erase(id);
      vcon_->implicit_dist_references.erase(id);
      vcon_->user_dist_references.erase(id);
    }
    vcon_->dist_references.resize(dists_0_);
  }

 private:
  VariableContainer* vcon_;
  std::size_t reads_0_;
  std::size_t dists_0_;
  bool rolled_back_;
};
}  // namespace detail

/**
 * \ingroup newAPI
 * \brief Base class for all randomizable objects of CRAVE.
 * 
 * All classes whose objects should be randomized with CRAVE must extends this class. 
 * It contains basic functionality and data necessary for randomzation.
 * You can define as members crv_variable to be randomized and crv_constraint for constraints to be randomized.
 */
class crv_sequence_item : public crv_object {
 public:
   /*!
    * \brief Empty constructor.
    */
  crv_sequence_item();

  /**
   * \brief Copy constructor.
   * \param other The object to be copied from.
   */
  crv_sequence_item(crv_sequence_item const& other);
  crv_sequence_item& operator=(crv_sequence_item const& other);
  ~crv_sequence_item();

  std::string obj_kind() const override final;

  bool randomize() override;

  /**
   * \brief Randomize variables with extra inline constraints.
   * 
   * Adds more constraints that should be used to find the next solution for the random variables.
   * This methods works like randomize() except it takes further inline constraints into consideration.
   * 
   * \param exprs extra constraints to use for constraint solving
   * \return true if a solution is found, false otherwise.
   */
  template <typename... Exprs>
  bool randomize_with(Exprs... exprs) {
    detail::InlineContainerGuard guard;
    expression_list list(exprs...);
    std::vector<detail::InlineReadCapture> inline_reads;
    std::vector<detail::InlineDistCapture> inline_dists;
    guard.capture_and_rollback(inline_reads, inline_dists);
    return randomize_with_expr_list_impl_(list, inline_reads, inline_dists);
  }

  /**
   * \brief Hints CRAVE to cover a given covergroup.
   * 
   * This method can be used to hint CRAVE which coverage goals exist.
   * CRAVE tries to find a solution for all unhit crv_coverpoint that are inside the given crv_covergroup.
   * 
   * \param group The crv_covergroup that should be fully covered by CRAVE.
   */
  void goal(crv_covergroup& group);

  void crosscheck_constraints ();

  /**
   * \brief Returns minimal unsatisfiable subsets of hard constraint expressions.
   *
   * Inspects the active constraint graph (or the most recent randomize() /
   * randomize_with() attempt) and returns each minimal contradicting set of
   * constraint expression names (e.g. "item.cstr#0", "item.randomize_with#0").
   * Returns an empty vector when the hard constraints are satisfiable.
   */
  std::vector<std::vector<std::string>> analyse_contradiction();

  /**
   * \brief Returns the names of soft constraint expressions deactivated to
   * make the constraint system satisfiable.
   */
  std::vector<std::string> inactive_soft_constraints();

  std::size_t randomize_with_cache_hits() const { return rand_with_cache_hits_; }
  std::size_t randomize_with_cache_misses() const { return rand_with_cache_misses_; }
  std::size_t randomize_with_cache_size() const;

 protected:
  friend class crv_covergroup;

  struct SavedCover {
    std::string name;
    expression expr;
  };

  struct SavedGoal {
    crv_covergroup* group;
    expression_list bindings;
    std::vector<SavedCover> covers;
  };

  void detach_goal_(crv_covergroup const* group);
  void detach_all_goals_();

  void request_rebuild() override;
  bool prune_covered_goals_();
  void prune_completed_goals_();
  void refresh_goals_();
  Generator& ensure_generator_();
  void apply_goals_(Generator& target);
  void clear_rand_with_cache_();

  bool randomize_with_expr_list(expression_list const&);
  bool randomize_with_expr_list_impl_(expression_list const& list,
                                      std::vector<detail::InlineReadCapture> const& inline_reads,
                                      std::vector<detail::InlineDistCapture> const& inline_dists);

  std::shared_ptr<Generator> gen_;
  std::shared_ptr<Generator> rand_with_gen_;
  std::shared_ptr<Generator> last_gen_;
  bool built_;
  bool cloned_;
  std::vector<SavedGoal> goals_;
  std::size_t goal_epoch_;
  std::uint64_t next_goal_uid_;
  std::shared_ptr<RandWithCache> rand_with_cache_;
  std::size_t rand_with_cache_hits_;
  std::size_t rand_with_cache_misses_;
};
}

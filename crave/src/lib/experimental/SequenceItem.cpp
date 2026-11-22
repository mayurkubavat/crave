//****************************************************************************************
//	MIT License
//****************************************************************************************
//	Copyright (c) 2012-2020 University of Bremen, Germany. 
//  	Copyright (c) 2015-2020 DFKI GmbH Bremen, Germany.
//  	Copyright (c) 2020 Johannes Kepler University Linz, Austria.
//      Copyright (c) 2022 - 2023 Coseda Technologies GmbH.
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

#include "../../crave/experimental/SequenceItem.hpp"
#include "../../crave/backend/Generator.hpp"

namespace crave
{
  crv_sequence_item::crv_sequence_item() : gen_(), rand_with_gen_(), last_gen_(), built_(false), cloned_(false) {}

  crv_sequence_item::crv_sequence_item(crv_sequence_item const & other)
      : crv_object(other), gen_(), rand_with_gen_(), last_gen_(), built_(false), cloned_(true) {}

  std::string crv_sequence_item::obj_kind() const { return "crv_sequence_item"; }

  void crv_sequence_item::crosscheck_constraints() {
    // Implicit default distributions (c_var_uniform) are suppressed per-Generator
    // during partitioning whenever an active user dist() constraint targets the
    // same variable, so c_var_uniform must never be permanently deactivated.
  }

  Generator& crv_sequence_item::ensure_generator_() {
    assert(!cloned_ && "cloned crv_sequence_item cannot be randomized");
    if (!last_gen_) {
      crosscheck_constraints();
      if (!built_) {
        gen_ = std::make_shared<Generator>();
        recursive_build(*gen_);
        built_ = true;
      }
      last_gen_ = gen_;
    }
    return *last_gen_;
  }

  std::vector<std::vector<std::string>> crv_sequence_item::analyse_contradiction() {
    return ensure_generator_().analyseContradiction();
  }

  std::vector<std::string> crv_sequence_item::inactive_soft_constraints() {
    return ensure_generator_().getInactiveSofts();
  }

  bool crv_sequence_item::randomize() {
    assert(!cloned_ && "cloned crv_sequence_item cannot be randomized");
    recursive_pre_randomize();
    crosscheck_constraints() ;

    if (!built_) {
      gen_ = std::make_shared<Generator>();
      recursive_build(*gen_);
      built_ = true;
    }
    last_gen_ = gen_;
    if (!gen_->nextCov()) return false;
    recursive_post_randomize();
    return true;
  }
  
  void crv_sequence_item::goal(crv_covergroup& group) {
    for (auto e : group.bound_var_expr_list()) (*gen_)(e);
    for (auto e : group.uncovered_as_list()) gen_->cover(e);
  }
    
  void crv_sequence_item::request_rebuild() {
    built_ = false;
    gen_.reset();
    rand_with_gen_.reset();
    last_gen_.reset();
    crv_object::request_rebuild();
  }

  bool crv_sequence_item::randomize_with_expr_list(const expression_list & list) {
    assert(!cloned_ && "cloned crv_sequence_item cannot be randomized");
    recursive_pre_randomize();
    crosscheck_constraints();
    // TODO Generator caching
    rand_with_gen_ = std::make_shared<Generator>();
    recursive_build(*rand_with_gen_);
    for (std::size_t i = 0; i < list.size(); ++i) {
      std::string const cname = fullname() + ".randomize_with#" + std::to_string(i);
      if (list.is_soft(i))
        rand_with_gen_->soft(cname, list[i]);
      else
        (*rand_with_gen_)(cname, list[i]);
    }
    last_gen_ = rand_with_gen_;
    if (!rand_with_gen_->next()) return false;
    recursive_post_randomize();
    return true;
  }

}

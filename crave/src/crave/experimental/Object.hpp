//****************************************************************************************
//	MIT License
//****************************************************************************************
//	Copyright (c) 2012-2020 University of Bremen, Germany. 
//  	Copyright (c) 2015-2020 DFKI GmbH Bremen, Germany.
//  	Copyright (c) 2020 Johannes Kepler University Linz, Austria.
//    Copyright (c) 2022 - 2023 Coseda Technologies GmbH.
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

#include <iostream>
#include <list>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>


namespace crave {

struct Generator;
struct RandcState;

struct crv_object_name;
class crv_object;
struct VariableContainer;

/**
 * \ingroup newAPI
 * \brief Class for names of CRAVE objects.
 *
 * This class is a type safe class to wrap the name of an object in CRAVE.
 */
struct crv_object_name {
  /**
   * \brief Constructor using a char array.
   * \param ca character array to create the name from.
   */
  crv_object_name(const char*);
  /**
   * \brief Constructor using a string.
   * \param s string to create the name from.
   */
  crv_object_name(const std::string& s) : crv_object_name(s.c_str()) {}
  /**
   * \brief Copy constructor.
   * \param other Other object to copy from.
   */
  crv_object_name(const crv_object_name&) = default;
  /**
   * \brief Destructor.
   */
  ~crv_object_name();

  /**
   * \brief Operator() to get the name as a string.
   * \return wrapped name as a string
   */
  std::string operator()() { return name_; }

  std::string name_;   /** < name of object*/
  crv_object* object_; /** < internal crave object*/
};

/**
 * \ingroup newAPI
 * \brief Base class for all CRAVE objects within the new API.
 */
class crv_object {
  template <typename T, typename Enable>
  friend class crv_variable;

 public:
  /**
  * \brief Empty Constructor.
  */
  crv_object();
  /**
   * \brief Destructor.
   */
  ~crv_object();

  /**
   * \brief Return the type of the object as a string.
   *
   * The type is most likely the name of the class the object is made from.
   *
   * \return Type of the object as a string.
   */
  virtual std::string obj_kind() const { return "crv_object"; }

  /**
   * \brief Randomizes the variables in this object.
   *
   * The variables must be of type crv_variable in order to be randomized.
   * While randomizing, all constraints that are part of the object will be used to get a solution.
   * These constraints may produce contradicts and cannot be solved.
   * In that case, this method returns false.
   *
   * \return true if a solution is found, false otherwise.
   */
  virtual bool randomize() { return false; }

  /**
   * \brief Called by randomize() before new random values are computed.
   *
   * As pre_randomize() in SystemVerilog, randomize() calls it on the randomized object and on all
   * objects in its hierarchy, a parent before its children. Override it to set up state that the
   * constraints use. Objects whose randomization is disabled with rand_mode(false) and their
   * children are skipped, until a variable of them is enabled again. The default implementation
   * does nothing.
   */
  virtual void pre_randomize() {}

  /**
   * \brief Called by randomize() after new random values have been computed.
   *
   * As post_randomize() in SystemVerilog, randomize() calls it on the randomized object and on all
   * objects in its hierarchy, a parent before its children, but only if a solution has been found.
   * Objects whose randomization is disabled with rand_mode(false) and their children are skipped,
   * until a variable of them is enabled again. The default implementation does nothing.
   */
  virtual void post_randomize() {}

  /**
   * \brief Get the name of the object.
   * \return Name of the object as a string.
   */
  std::string name() const { return name_; }

  /**
   * \brief Get the fullname of the object.
   * \return Fullname of the object as a string.
   */
  std::string fullname() const { return fullname_; }

  /**
   * \brief Enables or disables the randomization of all variables in this object.
   *
   * As rand_mode() of an object in SystemVerilog, it sets the rand_mode of all random variables
   * of this object and of the objects in its hierarchy, see crv_variable_base_::rand_mode(bool).
   * The variables of a crv_vector are not affected.
   *
   * \param on true to enable, false to disable the randomization.
   */
  virtual void rand_mode(bool on) {
    object_rand_mode_ = on;
    for (auto obj : children_) obj->rand_mode(on);
  }

  /**
   * \brief Get whether randomization of this object is enabled.
   * \return true if enabled, false otherwise.
   */
  virtual bool rand_mode() const { return object_rand_mode_; }

  /**
   * \brief Enables or disables all constraints in this object and its child objects.
   * \param on true to enable, false to disable constraints.
   */
  virtual void constraint_mode(bool on) {
    for (auto obj : children_) obj->constraint_mode(on);
    object_constraint_mode_ = on;
    notify_constraint_mode_changed_();
  }

  /**
   * \brief Get whether constraints of this object are enabled.
   * \return true if enabled, false otherwise.
   */
  virtual bool constraint_mode() const { return object_constraint_mode_; }

  /**
   * \brief Seeds this object's per-object RNG (and deterministically seeds child objects).
   */
  virtual void srandom(unsigned int seed);

  /**
   * \brief Captures this object's per-object RNG state (and any initialized child states).
   */
  std::string get_randstate() const;

  /**
   * \brief Restores this object's per-object RNG state from a string produced by get_randstate().
   */
  void set_randstate(std::string const& state);

  /**
   * outputs the object hierarchy (i.e. children and their children, etc.) to std::cout
   */
  void print_object_hierarchy(int level = 0) const;

  /**
   * get the root object of the hierarchy
   */
  static crv_object* root();
  /**
   * find the object with a given name
   */
  static crv_object* find(std::string);

  /**
   * get the numer of objects in the hierarchy
   */
  static unsigned count();

 public:
  /**
   * copy constructor
   */
  crv_object(const crv_object& other)
      : name_(other.name_),
        orig_name_(other.orig_name_),
        parent_(),
        children_(),
        fullname_(other.fullname_),
        object_rand_mode_(other.object_rand_mode_),
        object_constraint_mode_(other.object_constraint_mode_),
        rng_(other.rng_ ? new std::mt19937(*other.rng_) : nullptr) {}

  void remove_child(crv_object*);

  virtual void request_rebuild() {
    if (parent_) parent_->request_rebuild();
  }

  virtual void recursive_build(Generator& gen) const;

  /**
   * calls pre_randomize() on this object and its children
   */
  void recursive_pre_randomize();

  /**
   * calls post_randomize() on this object and its children
   */
  void recursive_post_randomize();

  virtual VariableContainer* check_default_constraints(Generator& gen);

  virtual bool has_active_constraint_() const {
    for (auto obj : children_) {
      if (obj->has_active_constraint_()) return true;
    }
    return false;
  }

  virtual void on_child_constraint_mode_changed_() {
    bool any = has_active_constraint_();
    if (object_constraint_mode_ != any) {
      object_constraint_mode_ = any;
      notify_constraint_mode_changed_();
    }
  }

  void notify_constraint_mode_changed_() {
    if (parent_) parent_->on_child_constraint_mode_changed_();
  }

  std::string name_;                                         /** < name of the object*/
  std::string orig_name_;                                    /** < original name of the object*/
  crv_object* parent_;                                       /** < parent node*/
  std::list<crv_object*> children_;                          /** < children nodes*/
  std::string fullname_;                                     /** < fullname*/
  std::unordered_map<std::string, unsigned> local_name_map_; /** < names of children in a map*/
  bool object_rand_mode_ = true;                             /** < set by rand_mode(bool) of this object*/
  bool object_constraint_mode_ = true;                       /** < set by constraint_mode(bool) of this object*/

 protected:
  friend struct RandstateCodec;
  virtual void on_rng_state_changed_() const {}
  virtual bool has_randc_state_for_randstate_() const { return false; }
  virtual bool supports_randc_for_randstate_() const { return false; }
  virtual void collect_var_ids_for_randstate_(std::vector<unsigned>&) const {}
  virtual bool capture_randc_state_(RandcState&) const { return false; }
  virtual void restore_randc_state_(bool, RandcState const&) {}
  virtual void reset_randc_state_() {}
  std::mt19937& ensure_rng_() const;
  bool has_any_initialized_rng_() const;
  void attach_copied_child_(crv_object* child);
  void collect_subtree_var_ids_(std::vector<unsigned>& out) const;
  void append_randstate_(std::ostringstream& os) const;
  void reset_rng_recursive_();

  mutable std::unique_ptr<std::mt19937> rng_;

 private:
  /**
   * internal constructor, used to construct the root object
   */
  crv_object(int) : name_("root"), orig_name_(name_), parent_(), children_(), fullname_(name_) {}
};

};  // namespace crave

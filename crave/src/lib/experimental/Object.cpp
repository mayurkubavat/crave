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


#include "../../crave/experimental/Object.hpp"
#include "../../crave/RandomSeedManager.hpp"
#include "../../crave/backend/Randc.hpp"

#include <cassert>
#include <cstdint>
#include <list>
#include <locale>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace crave {

static thread_local std::list<crv_object_name*> crv_name_stack;
static thread_local std::list<crv_object*> crv_obj_stack;
static std::unordered_map<std::string, crv_object*> crv_obj_map;

static std::recursive_mutex& obj_hierarchy_mutex() {
  static std::recursive_mutex* m = new std::recursive_mutex();
  return *m;
}

static unsigned int derive_child_seed(unsigned int seed, unsigned int idx) {
  std::uint64_t z = (static_cast<std::uint64_t>(seed) << 32) | static_cast<std::uint64_t>(idx + 1u);
  z += 0x9e3779b97f4a7c15ULL;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  z = z ^ (z >> 31);
  return static_cast<unsigned int>(z ^ (z >> 32));
}

crv_object_name::crv_object_name(const char* c) : name_(c), object_(0) { crv_name_stack.push_back(this); }

crv_object_name::~crv_object_name() {
  if (object_ == nullptr) return;
  assert(!crv_obj_stack.empty());
  assert(crv_obj_stack.back() == object_);
  crv_obj_stack.pop_back();
  object_ = nullptr;
}

crv_object::crv_object() {
  assert(!crv_name_stack.empty());
  crv_object_name* top = crv_name_stack.back();
  top->object_ = this;
  parent_ = !crv_obj_stack.empty() ? crv_obj_stack.back() : root();
  {
    std::lock_guard<std::recursive_mutex> lock(obj_hierarchy_mutex());
    parent_->children_.push_back(this);
    name_ = top->name_;
    orig_name_ = name_;
    // check name conflict
    auto& name_map = parent_->local_name_map_;
    if (name_map.find(name_) != name_map.end()) {
      name_ += std::to_string(name_map[name_]++);
    } else {
      name_map[name_] = 1;
    }
    // calculate fullname
    for (auto obj : crv_obj_stack) fullname_ += obj->name_ + ".";
    fullname_ += name_;
    crv_obj_map[fullname_] = this;
  }
  crv_obj_stack.push_back(this);
  crv_name_stack.pop_back();
}

crv_object::~crv_object() {
  std::lock_guard<std::recursive_mutex> lock(obj_hierarchy_mutex());
  if (parent_) {
    parent_->remove_child(this);
    parent_ = nullptr;
  }
  auto it = crv_obj_map.find(fullname_);
  if (it != crv_obj_map.end() && it->second == this) {
    crv_obj_map.erase(it);
  }
}

void crv_object::attach_copied_child_(crv_object* child) {
  std::lock_guard<std::recursive_mutex> lock(obj_hierarchy_mutex());
  child->parent_ = this;
  children_.push_back(child);
  ++local_name_map_[child->orig_name_];
}

std::mt19937& crv_object::ensure_rng_() const {
  if (!rng_) {
    unsigned int seed = with_active_or_global_rng(
        [](std::mt19937& g) { return static_cast<unsigned int>(g()); });
    rng_.reset(new std::mt19937(seed));
  }
  return *rng_;
}

void crv_object::srandom(unsigned int seed) {
  std::lock_guard<std::recursive_mutex> lock(obj_hierarchy_mutex());
  if (!rng_) {
    rng_.reset(new std::mt19937(seed));
  } else {
    rng_->seed(seed);
  }
  on_rng_state_changed_();
  unsigned int idx = 0;
  for (crv_object* child : children_) {
    child->srandom(derive_child_seed(seed, idx++));
  }
}

bool crv_object::has_any_initialized_rng_() const {
  std::lock_guard<std::recursive_mutex> lock(obj_hierarchy_mutex());
  if (rng_ || has_randc_state_for_randstate_()) return true;
  for (crv_object const* child : children_) {
    if (child->has_any_initialized_rng_()) return true;
  }
  return false;
}

void crv_object::collect_subtree_var_ids_(std::vector<unsigned>& out) const {
  std::lock_guard<std::recursive_mutex> lock(obj_hierarchy_mutex());
  collect_var_ids_for_randstate_(out);
  for (crv_object const* child : children_) {
    child->collect_subtree_var_ids_(out);
  }
}

namespace {
std::string encode_hex_str(std::string const& s) {
  if (s.empty()) return "-";
  static char const kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(s.size() * 2);
  for (unsigned char c : s) {
    out.push_back(kHex[(c >> 4) & 0xf]);
    out.push_back(kHex[c & 0xf]);
  }
  return out;
}

int hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

bool decode_hex_str(std::string const& tok, std::string& out) {
  if (tok == "-") {
    out.clear();
    return true;
  }
  if (tok.size() % 2 != 0) return false;
  out.clear();
  out.reserve(tok.size() / 2);
  for (std::size_t i = 0; i < tok.size(); i += 2) {
    int hi = hex_nibble(tok[i]);
    int lo = hex_nibble(tok[i + 1]);
    if (hi < 0 || lo < 0) return false;
    out.push_back(static_cast<char>((hi << 4) | lo));
  }
  return true;
}

void write_u64_vec(std::ostringstream& os, std::vector<std::uint64_t> const& v) {
  os << ' ' << v.size();
  for (std::uint64_t x : v) os << ' ' << x;
}

void read_u64_vec(std::istringstream& iss, std::vector<std::uint64_t>& out, std::size_t max_len) {
  std::size_t n = 0;
  if (!(iss >> n) || n > max_len) {
    throw std::invalid_argument("Invalid randstate: malformed randc vector length");
  }
  out.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    if (!(iss >> out[i])) {
      throw std::invalid_argument("Invalid randstate: malformed randc value");
    }
  }
}

struct ParsedRngNode {
  std::mt19937 rng;
  bool present = false;
  bool has_randc = false;
  RandcState randc_state;
  std::vector<ParsedRngNode> children;
};
}  // namespace

struct RandstateCodec {
  static void parse_node(std::istringstream& iss, crv_object const& obj, ParsedRngNode& out) {
    std::mt19937 tmp_rng;
    if (!(iss >> tmp_rng)) {
      throw std::invalid_argument("Invalid randstate: malformed RNG state");
    }
    int has_rc = -1;
    if (!(iss >> has_rc) || (has_rc != 0 && has_rc != 1)) {
      throw std::invalid_argument("Invalid randstate: malformed randc flag");
    }
    out.has_randc = (has_rc == 1);
    if (out.has_randc) {
      if (!obj.supports_randc_for_randstate_()) {
        throw std::invalid_argument("Invalid randstate: randc incompatible with target object");
      }
      std::string sig_hex;
      if (!(iss >> sig_hex) || !decode_hex_str(sig_hex, out.randc_state.signature)) {
        throw std::invalid_argument("Invalid randstate: malformed randc signature");
      }
      int started_flag = -1;
      int complete_flag = -1;
      if (!(iss >> started_flag >> complete_flag) ||
          (started_flag != 0 && started_flag != 1) ||
          (complete_flag != 0 && complete_flag != 1)) {
        throw std::invalid_argument("Invalid randstate: malformed randc flags");
      }
      out.randc_state.started = (started_flag == 1);
      out.randc_state.complete = (complete_flag == 1);
      std::size_t const kMaxRandcEntries = static_cast<std::size_t>(1) << randc_max_bitsize;
      read_u64_vec(iss, out.randc_state.unused, kMaxRandcEntries);
      read_u64_vec(iss, out.randc_state.used, kMaxRandcEntries);
      read_u64_vec(iss, out.randc_state.listed, kMaxRandcEntries);
    }
    std::size_t child_count = 0;
    if (!(iss >> child_count) || child_count != obj.children_.size()) {
      throw std::invalid_argument("Invalid randstate: child count mismatch");
    }
    out.rng = tmp_rng;
    out.present = true;
    out.children.resize(child_count);
    auto child_it = obj.children_.begin();
    for (std::size_t i = 0; i < child_count; ++i, ++child_it) {
      int flag = -1;
      if (!(iss >> flag) || (flag != 0 && flag != 1)) {
        throw std::invalid_argument("Invalid randstate: malformed child flag");
      }
      if (flag == 1) {
        parse_node(iss, **child_it, out.children[i]);
      } else {
        out.children[i].present = false;
      }
    }
  }

  static void apply_node(crv_object& obj, ParsedRngNode const& node) {
    if (!obj.rng_) {
      obj.rng_.reset(new std::mt19937(node.rng));
    } else {
      *obj.rng_ = node.rng;
    }
    obj.restore_randc_state_(node.has_randc, node.randc_state);
    obj.on_rng_state_changed_();
    auto child_it = obj.children_.begin();
    for (std::size_t i = 0; i < node.children.size(); ++i, ++child_it) {
      if (node.children[i].present) {
        apply_node(**child_it, node.children[i]);
      } else {
        (*child_it)->reset_rng_recursive_();
      }
    }
  }
};

void crv_object::append_randstate_(std::ostringstream& os) const {
  std::lock_guard<std::recursive_mutex> lock(obj_hierarchy_mutex());
  std::mt19937& r = ensure_rng_();
  os << ' ' << r;
  RandcState rc;
  if (capture_randc_state_(rc)) {
    os << " 1 " << encode_hex_str(rc.signature) << ' ' << (rc.started ? 1 : 0) << ' ' << (rc.complete ? 1 : 0);
    write_u64_vec(os, rc.unused);
    write_u64_vec(os, rc.used);
    write_u64_vec(os, rc.listed);
  } else {
    os << " 0";
  }
  os << ' ' << children_.size();
  for (crv_object const* child : children_) {
    if (child->has_any_initialized_rng_()) {
      os << " 1";
      child->append_randstate_(os);
    } else {
      os << " 0";
    }
  }
}

void crv_object::reset_rng_recursive_() {
  std::lock_guard<std::recursive_mutex> lock(obj_hierarchy_mutex());
  rng_.reset();
  reset_randc_state_();
  on_rng_state_changed_();
  for (crv_object* child : children_) {
    child->reset_rng_recursive_();
  }
}

std::string crv_object::get_randstate() const {
  std::lock_guard<std::recursive_mutex> lock(obj_hierarchy_mutex());
  std::ostringstream os;
  os.imbue(std::locale::classic());
  os << "CRV_RNG_V1";
  std::vector<unsigned> var_ids;
  collect_subtree_var_ids_(var_ids);
  os << ' ' << var_ids.size();
  for (unsigned vid : var_ids) os << ' ' << vid;
  append_randstate_(os);
  return os.str();
}

void crv_object::set_randstate(std::string const& state) {
  std::lock_guard<std::recursive_mutex> lock(obj_hierarchy_mutex());
  std::istringstream iss(state);
  iss.imbue(std::locale::classic());
  std::string magic;
  if (!(iss >> magic) || magic != "CRV_RNG_V1") {
    throw std::invalid_argument("Invalid randstate: bad header");
  }
  std::size_t var_count = 0;
  std::vector<unsigned> dst_var_ids;
  collect_subtree_var_ids_(dst_var_ids);
  if (!(iss >> var_count) || var_count != dst_var_ids.size()) {
    throw std::invalid_argument("Invalid randstate: variable count mismatch");
  }
  for (std::size_t i = 0; i < var_count; ++i) {
    unsigned src_id = 0;
    if (!(iss >> src_id)) {
      throw std::invalid_argument("Invalid randstate: malformed variable id");
    }
  }
  ParsedRngNode root_node;
  RandstateCodec::parse_node(iss, *this, root_node);
  if (!(iss >> std::ws).eof()) {
    throw std::invalid_argument("Invalid randstate: trailing data");
  }
  RandstateCodec::apply_node(*this, root_node);
}

void crv_object::print_object_hierarchy(int lvl) const {
  std::lock_guard<std::recursive_mutex> lock(obj_hierarchy_mutex());
  std::cout << std::string(2 * lvl, ' ');
  std::cout << name() << " # " << fullname() << " @ " << this << ", parent @ " << parent_ << std::endl;
  for (auto obj : children_) obj->print_object_hierarchy(lvl + 1);
}

void crv_object::remove_child(crv_object* child) {
  children_.remove(child);
  if (--local_name_map_[child->orig_name_] == 0) local_name_map_.erase(child->name_);
}

// As in SystemVerilog, the hooks of an object whose randomization is disabled with rand_mode() are not called.
void crv_object::recursive_pre_randomize() {
  pre_randomize();
  for (auto obj : children_)
    if (obj->object_rand_mode_) obj->recursive_pre_randomize();
}

void crv_object::recursive_post_randomize() {
  post_randomize();
  for (auto obj : children_)
    if (obj->object_rand_mode_) obj->recursive_post_randomize();
}

crv_object* crv_object::root() {
  static crv_object root(0);
  return &root;
}

unsigned crv_object::count() {
  std::lock_guard<std::recursive_mutex> lock(obj_hierarchy_mutex());
  return crv_obj_map.size();
}

crv_object* crv_object::find(std::string name) {
  std::lock_guard<std::recursive_mutex> lock(obj_hierarchy_mutex());
  auto tmp = crv_obj_map.find(name);
  return tmp != crv_obj_map.end() ? tmp->second : nullptr;
}

};  // namespace crave

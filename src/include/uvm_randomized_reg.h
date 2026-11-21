//----------------------------------------------------------------------
//  Licensed to Accellera Systems Initiative Inc. (Accellera)
//  under one or more contributor license agreements.
//  See the NOTICE file distributed with this work for
//  additional information regarding copyright ownership.
//
//  Accellera licenses this file to you under the Apache
//  License, Version 2.0 (the "License"); you may not use
//  this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in
//  writing, software distributed under the License is
//  distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//  CONDITIONS OF ANY KIND, either express or implied.  See
//  the License for the specific language governing
//  permissions and limitations under the License.
//----------------------------------------------------------------------

#pragma once

#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

#include <crave/ConstrainedRandom.hpp>
#include <crave/experimental/Experimental.hpp>

#include <uvm>

namespace crave2uvm_detail {

template <typename F>
auto field_rand_enabled_impl(const F* f, int)
    -> decltype(static_cast<bool>(f->get_rand_mode())) {
  return f != nullptr && static_cast<bool>(f->get_rand_mode());
}

template <typename F>
bool field_rand_enabled_impl(const F* f, long) {
  if (f == nullptr) return false;
  std::string acc = f->get_access();
  std::transform(acc.begin(), acc.end(), acc.begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return acc != "RO" && acc != "RC" && acc != "RS" && acc != "NOACCESS";
}

inline bool field_rand_enabled(const ::uvm::uvm_reg_field* f) {
  return field_rand_enabled_impl(f, 0);
}

template <typename Var>
auto set_crv_var_rand_mode_impl(Var& v, bool on, int)
    -> decltype((void)v.rand_mode(on)) {
  v.rand_mode(on);
}

template <typename Var>
void set_crv_var_rand_mode_impl(Var&, bool, long) {}

template <typename Var>
void set_crv_var_rand_mode(Var& v, bool on) {
  set_crv_var_rand_mode_impl(v, on, 0);
}

template <typename Obj>
auto has_crv_callbacks_test(int)
    -> decltype((void)&Obj::pre_randomize, (void)&Obj::post_randomize, std::true_type());

template <typename Obj>
std::false_type has_crv_callbacks_test(...);

template <typename Obj>
struct has_crv_callbacks : decltype(has_crv_callbacks_test<Obj>(0)) {};

template <typename T>
typename std::enable_if<std::is_signed<T>::value>::type
add_signed_lower_bound(crave::crv_constraint& cstr, crave::crv_variable<T>& var) {
  cstr &= crave::crv_constraint(var() >= static_cast<T>(0));
}

template <typename T>
typename std::enable_if<!std::is_signed<T>::value>::type
add_signed_lower_bound(crave::crv_constraint&, crave::crv_variable<T>&) {}

}  // namespace crave2uvm_detail

/**
 * \brief Adapter merging uvm::uvm_reg and crave::crv_sequence_item.
 *
 * Derive a register class from uvm_randomized_reg, declare CRAVE variables
 * and constraints as members, and call bind_field(field, var) inside build()
 * to automatically constrain each variable to its field's bit-width and write
 * solved values into the register's desired field state upon randomize() or
 * randomize_with().
 */
class uvm_randomized_reg : public uvm::uvm_reg, public crave::crv_sequence_item {
 public:
  uvm_randomized_reg(crave::crv_object_name name = "uvm_randomized_reg",
                     unsigned int n_bits = 32,
                     int has_coverage = uvm::UVM_NO_COVERAGE)
      : uvm::uvm_reg(name(), n_bits, has_coverage),
        crave::crv_sequence_item(),
        m_field_bounds_cstr("field_bounds"),
        m_synced(false) {}

  virtual ~uvm_randomized_reg() {}

  /**
   * \brief Bind a uvm_reg_field to a crave::crv_variable<T>.
   *
   * Automatically constrains \p var to fit within [0, 2^field->get_n_bits() - 1]
   * when the field is narrower than T, respects read-only / non-rand fields,
   * and updates field->set(...) whenever randomize() or randomize_with() succeeds.
   */
  template <typename T>
  void bind_field(uvm::uvm_reg_field* field, crave::crv_variable<T>& var) {
    if (field == nullptr) return;

    const unsigned int n_bits = field->get_n_bits();
    const unsigned int type_bits = static_cast<unsigned int>(sizeof(T) * 8u);
    const unsigned int max_usable_bits =
        std::is_signed<T>::value ? (type_bits - 1u) : type_bits;

    crave2uvm_detail::add_signed_lower_bound(m_field_bounds_cstr, var);
    if (n_bits > 0u && n_bits < max_usable_bits && n_bits < 64u) {
      const T max_val = static_cast<T>((1ULL << n_bits) - 1ULL);
      m_field_bounds_cstr &= crave::crv_constraint(var() <= max_val);
    }

    bound_field_entry entry;
    entry.field = field;
    entry.prepare = [field, &var](bool rand_en) {
      if (!rand_en) {
        var = static_cast<T>(field->get().to_uint64());
      }
      crave2uvm_detail::set_crv_var_rand_mode(var, rand_en);
    };
    entry.read_var = [n_bits, &var]() -> sc_dt::uint64 {
      sc_dt::uint64 raw = static_cast<sc_dt::uint64>(static_cast<T>(var));
      if (n_bits > 0u && n_bits < 64u) {
        raw &= ((static_cast<sc_dt::uint64>(1) << n_bits) - 1ULL);
      }
      return raw;
    };
    entry.pre_desired = 0;
    m_bound_fields.push_back(entry);
  }

  virtual bool randomize() {
    prepare_bound_fields();
    if (!crave2uvm_detail::has_crv_callbacks<crave::crv_object>::value) {
      this->pre_randomize();
    }
    if (!crave::crv_sequence_item::randomize()) {
      return false;
    }
    finalize_bound_fields();
    return true;
  }

  template <typename... Exprs>
  bool randomize_with(Exprs... exprs) {
    prepare_bound_fields();
    if (!crave2uvm_detail::has_crv_callbacks<crave::crv_object>::value) {
      this->pre_randomize();
    }
    if (!crave::crv_sequence_item::randomize_with(exprs...)) {
      return false;
    }
    finalize_bound_fields();
    return true;
  }

  virtual void pre_randomize() {}

  virtual void post_randomize() {
    sync_bound_fields();
    m_synced = true;
  }

 protected:
  void sync_bound_fields() {
    for (std::size_t i = 0; i < m_bound_fields.size(); ++i) {
      bound_field_entry& e = m_bound_fields[i];
      if (!crave2uvm_detail::field_rand_enabled(e.field)) continue;
      if (e.field->get() != e.pre_desired) continue;
      e.field->set(uvm::uvm_reg_data_t(e.read_var()));
    }
  }

 private:
  struct bound_field_entry {
    uvm::uvm_reg_field* field;
    std::function<void(bool)> prepare;
    std::function<sc_dt::uint64()> read_var;
    uvm::uvm_reg_data_t pre_desired;
  };

  void prepare_bound_fields() {
    m_synced = false;
    for (std::size_t i = 0; i < m_bound_fields.size(); ++i) {
      bound_field_entry& e = m_bound_fields[i];
      e.pre_desired = e.field->get();
      e.prepare(crave2uvm_detail::field_rand_enabled(e.field));
    }
  }

  void finalize_bound_fields() {
    if (!m_synced) {
      sync_bound_fields();
      m_synced = true;
    }
    if (!crave2uvm_detail::has_crv_callbacks<crave::crv_object>::value) {
      this->post_randomize();
    }
  }

  crave::crv_constraint m_field_bounds_cstr;
  std::vector<bound_field_entry> m_bound_fields;
  bool m_synced;
};

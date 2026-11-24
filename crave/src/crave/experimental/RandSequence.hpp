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

#if defined(__cplusplus) && (__cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L))

#include <algorithm>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Object.hpp"
#include "../../crave/RandomSeedManager.hpp"

namespace crave {

class crv_randsequence;

template <typename Sig>
class production_handle;

namespace detail {

struct RsFrameBase {
  virtual ~RsFrameBase() = default;
  virtual bool is_void() const { return false; }
  virtual bool try_set_exact(std::type_info const& ti, void const* ptr) {
    (void)ti;
    (void)ptr;
    return false;
  }
  virtual bool try_set_bool(bool) { return false; }
  virtual bool try_set_signed(long long) { return false; }
  virtual bool try_set_unsigned(unsigned long long) { return false; }
  virtual bool try_set_float(long double) { return false; }
  virtual bool try_set_cstr(char const*) { return false; }
  virtual bool try_set_string(std::string const&) { return false; }
  bool returned = false;
  RsFrameBase* prev = nullptr;
};

template <typename R>
struct RsTypedFrame : RsFrameBase {
  bool try_set_exact(std::type_info const& ti, void const* ptr) override {
    if (ti == typeid(R)) {
      if constexpr (std::is_copy_constructible_v<R>) {
        ret_val = *static_cast<R const*>(ptr);
        return true;
      }
    }
    return false;
  }

  bool try_set_bool(bool v) override {
    if constexpr (std::is_arithmetic_v<R>) {
      ret_val = static_cast<R>(v);
      return true;
    }
    return false;
  }

  bool try_set_signed(long long v) override {
    if constexpr (std::is_arithmetic_v<R>) {
      ret_val = static_cast<R>(v);
      return true;
    }
    return false;
  }

  bool try_set_unsigned(unsigned long long v) override {
    if constexpr (std::is_arithmetic_v<R>) {
      ret_val = static_cast<R>(v);
      return true;
    }
    return false;
  }

  bool try_set_float(long double v) override {
    if constexpr (std::is_arithmetic_v<R>) {
      ret_val = static_cast<R>(v);
      return true;
    }
    return false;
  }

  bool try_set_cstr(char const* v) override {
    if constexpr (std::is_constructible_v<R, char const*> && !std::is_arithmetic_v<std::decay_t<R>>) {
      if (v != nullptr) {
        ret_val = R(v);
        return true;
      }
    }
    return false;
  }

  bool try_set_string(std::string const& v) override {
    if constexpr (std::is_constructible_v<R, std::string const&> && !std::is_arithmetic_v<std::decay_t<R>>) {
      ret_val = R(v);
      return true;
    }
    return false;
  }

  std::optional<R> ret_val;
};

template <>
struct RsTypedFrame<void> : RsFrameBase {
  bool is_void() const override { return true; }
};

struct RsExecState {
  crv_randsequence* seq = nullptr;
  bool aborted = false;
  RsFrameBase* current_frame = nullptr;
};

inline RsExecState*& active_rs_exec_state() {
  static thread_local RsExecState* tls_state = nullptr;
  return tls_state;
}

class ScopedRsExecGuard {
 public:
  explicit ScopedRsExecGuard(RsExecState* state) : prev_(active_rs_exec_state()) {
    active_rs_exec_state() = state;
  }
  ~ScopedRsExecGuard() { active_rs_exec_state() = prev_; }

  ScopedRsExecGuard(ScopedRsExecGuard const&) = delete;
  ScopedRsExecGuard& operator=(ScopedRsExecGuard const&) = delete;

 private:
  RsExecState* prev_;
};

class ScopedRsFrameGuard {
 public:
  ScopedRsFrameGuard(RsExecState& exec, RsFrameBase* frame) : exec_(exec), prev_(exec.current_frame) {
    frame->prev = prev_;
    exec_.current_frame = frame;
  }
  ~ScopedRsFrameGuard() { exec_.current_frame = prev_; }

  ScopedRsFrameGuard(ScopedRsFrameGuard const&) = delete;
  ScopedRsFrameGuard& operator=(ScopedRsFrameGuard const&) = delete;

 private:
  RsExecState& exec_;
  RsFrameBase* prev_;
};

template <typename Fn>
struct RsWeightWrapper {
  Fn fn;
};

template <typename T>
struct is_rs_weight_wrapper : std::false_type {};
template <typename Fn>
struct is_rs_weight_wrapper<RsWeightWrapper<Fn>> : std::true_type {};

template <typename T>
struct is_production_handle : std::false_type {};
template <typename Sig>
struct is_production_handle<production_handle<Sig>> : std::true_type {};

struct RsStepTag {};

template <typename T>
struct is_rs_combinator : std::is_base_of<RsStepTag, std::decay_t<T>> {};

template <typename T>
struct is_integral_weight
    : std::bool_constant<std::is_integral_v<std::decay_t<T>> && !std::is_same_v<std::decay_t<T>, bool>> {};

template <typename Fn, typename = void>
struct is_callable_weight : std::false_type {};

template <typename Fn>
struct is_callable_weight<
    Fn, std::enable_if_t<!is_production_handle<std::decay_t<Fn>>::value &&
                         !is_rs_combinator<std::decay_t<Fn>>::value &&
                         !is_rs_weight_wrapper<std::decay_t<Fn>>::value &&
                         !is_integral_weight<Fn>::value &&
                         std::is_invocable_v<std::decay_t<Fn>&>>>
    : std::bool_constant<
          std::is_integral_v<std::decay_t<std::invoke_result_t<std::decay_t<Fn>&>>> &&
          !std::is_same_v<std::decay_t<std::invoke_result_t<std::decay_t<Fn>&>>, bool>> {};

template <typename T>
struct is_weight_arg
    : std::bool_constant<is_integral_weight<T>::value || is_rs_weight_wrapper<std::decay_t<T>>::value ||
                         is_callable_weight<T>::value> {};

using RsVoidStep = std::function<void(RsExecState&)>;

template <typename... Args>
using RsBoundStep = std::function<void(RsExecState&, Args...)>;

struct RsProductionBase {
  virtual ~RsProductionBase() = default;
  virtual std::type_index signature_type() const = 0;
  virtual bool invoke_void(RsExecState& exec) = 0;
  virtual std::vector<RsVoidStep> sample_join_steps(RsExecState& exec) = 0;
};

template <typename Sig>
struct RsProductionImpl;

template <typename Step, typename... Args>
inline void invoke_step_with_args(Step& step, RsExecState& exec, Args&... args) {
  if constexpr (is_rs_combinator<Step>::value) {
    step.execute(exec, args...);
  } else if constexpr (std::is_invocable_v<Step&, Args&...>) {
    (void)exec;
    step(args...);
  } else if constexpr (std::is_invocable_v<Step&>) {
    (void)exec;
    step();
  } else {
    static_assert(std::is_invocable_v<Step&>, "Invalid step type in crv_randsequence rule");
  }
}

template <typename V>
inline std::uint64_t to_nonneg_weight(V v) {
  if constexpr (std::is_signed_v<std::decay_t<V>>) {
    return v > 0 ? static_cast<std::uint64_t>(v) : 0ULL;
  } else {
    return static_cast<std::uint64_t>(v);
  }
}

template <typename Weight>
inline std::uint64_t eval_weight(Weight& w) {
  if constexpr (is_rs_weight_wrapper<std::decay_t<Weight>>::value) {
    return to_nonneg_weight(w.fn());
  } else if constexpr (std::is_integral_v<std::decay_t<Weight>>) {
    return to_nonneg_weight(w);
  } else {
    return to_nonneg_weight(w());
  }
}

template <typename CountOrFn>
inline std::size_t eval_count(CountOrFn& c) {
  if constexpr (std::is_integral_v<std::decay_t<CountOrFn>>) {
    auto v = static_cast<std::int64_t>(c);
    return v > 0 ? static_cast<std::size_t>(v) : 0u;
  } else {
    auto v = static_cast<std::int64_t>(c());
    return v > 0 ? static_cast<std::size_t>(v) : 0u;
  }
}

template <typename CondOrFn>
inline bool eval_cond(CondOrFn& c) {
  if constexpr (std::is_invocable_v<CondOrFn&>) {
    return static_cast<bool>(c());
  } else {
    return static_cast<bool>(c);
  }
}

template <typename KeyOrFn>
inline auto eval_key(KeyOrFn& k) {
  if constexpr (std::is_invocable_v<KeyOrFn&>) {
    return k();
  } else {
    return k;
  }
}

template <typename R, typename... Args>
struct RsProductionImpl<R(Args...)> : RsProductionBase {
  struct Rule {
    std::function<std::uint64_t()> weight_fn;
    std::vector<RsBoundStep<Args...>> steps;
  };

  std::type_index signature_type() const override { return std::type_index(typeid(R(Args...))); }

  bool invoke_void(RsExecState& exec) override {
    if constexpr (sizeof...(Args) == 0) {
      execute(exec);
      return true;
    } else {
      (void)exec;
      throw std::logic_error("crv_randsequence: cannot invoke parameterized production as void()");
    }
  }

  std::vector<RsVoidStep> sample_join_steps(RsExecState& exec) override {
    if constexpr (sizeof...(Args) == 0 && std::is_void_v<R>) {
      Rule const* chosen = choose_rule(exec);
      if (!chosen) return {};
      std::vector<RsVoidStep> out;
      out.reserve(chosen->steps.size());
      for (auto const& s : chosen->steps) {
        out.push_back([s](RsExecState& ex) { s(ex); });
      }
      return out;
    } else {
      (void)exec;
      throw std::logic_error("crv_randsequence: rs_join requires void() productions");
    }
  }

  Rule const* choose_rule(RsExecState& exec);

  R execute(RsExecState& exec, Args... args);

  std::vector<Rule> rules;
};

}  // namespace detail

template <typename Fn>
inline detail::RsWeightWrapper<std::decay_t<Fn>> rs_weight(Fn&& fn) {
  return detail::RsWeightWrapper<std::decay_t<Fn>>{std::forward<Fn>(fn)};
}

template <typename... Steps>
struct RsSeqExpr : detail::RsStepTag {
  explicit RsSeqExpr(Steps... s) : steps(std::move(s)...) {}

  template <typename... Args>
  void execute(detail::RsExecState& exec, Args&... args) const {
    execute_impl(exec, std::index_sequence_for<Steps...>{}, args...);
  }

  void collect_void_steps(std::vector<detail::RsVoidStep>& out) const {
    collect_impl(out, std::index_sequence_for<Steps...>{});
  }

  std::tuple<Steps...> steps;

 private:
  template <std::size_t... Is, typename... Args>
  void execute_impl(detail::RsExecState& exec, std::index_sequence<Is...>, Args&... args) const {
    (void)exec;
    (void)sizeof...(args);
    auto run_one = [&](auto const& step) {
      if (exec.aborted || (exec.current_frame && exec.current_frame->returned)) return;
      auto copy = step;
      detail::invoke_step_with_args(copy, exec, args...);
    };
    (run_one(std::get<Is>(steps)), ...);
  }

  template <std::size_t... Is>
  void collect_impl(std::vector<detail::RsVoidStep>& out, std::index_sequence<Is...>) const {
    auto add_one = [&](auto const& step) {
      if constexpr (std::is_base_of_v<detail::RsStepTag, std::decay_t<decltype(step)>>) {
        step.collect_void_steps(out);
      } else {
        out.push_back([step](detail::RsExecState& exec) {
          auto copy = step;
          detail::invoke_step_with_args(copy, exec);
        });
      }
    };
    (add_one(std::get<Is>(steps)), ...);
  }
};

template <typename... Steps>
inline RsSeqExpr<std::decay_t<Steps>...> rs_seq(Steps&&... steps) {
  return RsSeqExpr<std::decay_t<Steps>...>(std::forward<Steps>(steps)...);
}

template <typename CountOrFn, typename... Steps>
struct RsRepeatExpr : detail::RsStepTag {
  RsRepeatExpr(CountOrFn c, Steps... s) : count(std::move(c)), body(std::move(s)...) {}

  template <typename... Args>
  void execute(detail::RsExecState& exec, Args&... args) const {
    auto c = count;
    std::size_t const n = detail::eval_count(c);
    for (std::size_t i = 0; i < n; ++i) {
      if (exec.aborted || (exec.current_frame && exec.current_frame->returned)) break;
      body.execute(exec, args...);
    }
  }

  void collect_void_steps(std::vector<detail::RsVoidStep>& out) const {
    auto self = *this;
    out.push_back([self](detail::RsExecState& exec) { self.execute(exec); });
  }

  CountOrFn count;
  RsSeqExpr<Steps...> body;
};

template <typename MinOrFn, typename MaxOrFn, typename... Steps>
struct RsRepeatRangeExpr : detail::RsStepTag {
  RsRepeatRangeExpr(MinOrFn lo, MaxOrFn hi, Steps... s)
      : min_count(std::move(lo)), max_count(std::move(hi)), body(std::move(s)...) {}

  template <typename... Args>
  void execute(detail::RsExecState& exec, Args&... args) const;

  void collect_void_steps(std::vector<detail::RsVoidStep>& out) const {
    auto self = *this;
    out.push_back([self](detail::RsExecState& exec) { self.execute(exec); });
  }

  MinOrFn min_count;
  MaxOrFn max_count;
  RsSeqExpr<Steps...> body;
};

template <typename CountOrFn, typename... Steps>
inline RsRepeatExpr<std::decay_t<CountOrFn>, std::decay_t<Steps>...> rs_repeat(CountOrFn&& count, Steps&&... steps) {
  return RsRepeatExpr<std::decay_t<CountOrFn>, std::decay_t<Steps>...>(std::forward<CountOrFn>(count),
                                                                       std::forward<Steps>(steps)...);
}

template <typename MinOrFn, typename MaxOrFn, typename... Steps,
          typename = std::enable_if_t<
              (std::is_integral_v<std::decay_t<MinOrFn>> && std::is_integral_v<std::decay_t<MaxOrFn>>)>>
inline RsRepeatRangeExpr<std::decay_t<MinOrFn>, std::decay_t<MaxOrFn>, std::decay_t<Steps>...>
rs_repeat(MinOrFn&& min_c, MaxOrFn&& max_c, Steps&&... steps) {
  return RsRepeatRangeExpr<std::decay_t<MinOrFn>, std::decay_t<MaxOrFn>, std::decay_t<Steps>...>(
      std::forward<MinOrFn>(min_c), std::forward<MaxOrFn>(max_c), std::forward<Steps>(steps)...);
}

template <typename CondOrFn, typename ThenStep, typename ElseStep = void>
struct RsIfExpr : detail::RsStepTag {
  RsIfExpr(CondOrFn c, ThenStep t, ElseStep e)
      : cond(std::move(c)), then_step(std::move(t)), else_step(std::move(e)) {}

  template <typename... Args>
  void execute(detail::RsExecState& exec, Args&... args) const {
    if (exec.aborted || (exec.current_frame && exec.current_frame->returned)) return;
    auto c = cond;
    if (detail::eval_cond(c)) {
      auto t = then_step;
      detail::invoke_step_with_args(t, exec, args...);
    } else {
      auto e = else_step;
      detail::invoke_step_with_args(e, exec, args...);
    }
  }

  void collect_void_steps(std::vector<detail::RsVoidStep>& out) const {
    auto self = *this;
    out.push_back([self](detail::RsExecState& exec) { self.execute(exec); });
  }

  CondOrFn cond;
  ThenStep then_step;
  ElseStep else_step;
};

template <typename CondOrFn, typename ThenStep>
struct RsIfExpr<CondOrFn, ThenStep, void> : detail::RsStepTag {
  RsIfExpr(CondOrFn c, ThenStep t) : cond(std::move(c)), then_step(std::move(t)) {}

  template <typename... Args>
  void execute(detail::RsExecState& exec, Args&... args) const {
    if (exec.aborted || (exec.current_frame && exec.current_frame->returned)) return;
    auto c = cond;
    if (detail::eval_cond(c)) {
      auto t = then_step;
      detail::invoke_step_with_args(t, exec, args...);
    }
  }

  void collect_void_steps(std::vector<detail::RsVoidStep>& out) const {
    auto self = *this;
    out.push_back([self](detail::RsExecState& exec) { self.execute(exec); });
  }

  CondOrFn cond;
  ThenStep then_step;
};

template <typename CondOrFn, typename ThenStep>
inline RsIfExpr<std::decay_t<CondOrFn>, std::decay_t<ThenStep>> rs_if(CondOrFn&& cond, ThenStep&& then_s) {
  return RsIfExpr<std::decay_t<CondOrFn>, std::decay_t<ThenStep>>(std::forward<CondOrFn>(cond),
                                                                  std::forward<ThenStep>(then_s));
}

template <typename CondOrFn, typename ThenStep, typename ElseStep>
inline RsIfExpr<std::decay_t<CondOrFn>, std::decay_t<ThenStep>, std::decay_t<ElseStep>>
rs_if(CondOrFn&& cond, ThenStep&& then_s, ElseStep&& else_s) {
  return RsIfExpr<std::decay_t<CondOrFn>, std::decay_t<ThenStep>, std::decay_t<ElseStep>>(
      std::forward<CondOrFn>(cond), std::forward<ThenStep>(then_s), std::forward<ElseStep>(else_s));
}

template <typename KeyT, typename Step>
struct RsWhenClause {
  std::vector<KeyT> values;
  Step step;
};

template <typename Step>
struct RsDefaultClause {
  Step step;
};

template <typename KeyT, typename Step>
inline RsWhenClause<std::decay_t<KeyT>, std::decay_t<Step>> rs_when(KeyT const& val, Step&& step) {
  return RsWhenClause<std::decay_t<KeyT>, std::decay_t<Step>>{{val}, std::forward<Step>(step)};
}

template <typename KeyT, typename Step>
inline RsWhenClause<std::decay_t<KeyT>, std::decay_t<Step>> rs_when(std::initializer_list<KeyT> vals, Step&& step) {
  return RsWhenClause<std::decay_t<KeyT>, std::decay_t<Step>>{std::vector<std::decay_t<KeyT>>(vals),
                                                              std::forward<Step>(step)};
}

template <typename Step>
inline RsDefaultClause<std::decay_t<Step>> rs_default(Step&& step) {
  return RsDefaultClause<std::decay_t<Step>>{std::forward<Step>(step)};
}

template <typename KeyOrFn>
class RsCaseBuilder : public detail::RsStepTag {
 public:
  using KeyT = std::decay_t<decltype(detail::eval_key(std::declval<KeyOrFn&>()))>;

  explicit RsCaseBuilder(KeyOrFn key) : key_fn_(std::move(key)) {}

  template <typename Step>
  RsCaseBuilder& when(KeyT const& val, Step&& step) {
    branches_.push_back(Branch{{val}, wrap_step(std::forward<Step>(step))});
    return *this;
  }

  template <typename Step>
  RsCaseBuilder& when(std::initializer_list<KeyT> vals, Step&& step) {
    branches_.push_back(Branch{std::vector<KeyT>(vals), wrap_step(std::forward<Step>(step))});
    return *this;
  }

  template <typename Step>
  RsCaseBuilder& otherwise(Step&& step) {
    default_fn_ = wrap_step(std::forward<Step>(step));
    return *this;
  }

  template <typename... Args>
  void execute(detail::RsExecState& exec, Args&...) const {
    if (exec.aborted || (exec.current_frame && exec.current_frame->returned)) return;
    auto k = key_fn_;
    KeyT const key = detail::eval_key(k);
    for (Branch const& b : branches_) {
      if (std::find(b.values.begin(), b.values.end(), key) != b.values.end()) {
        b.action(exec);
        return;
      }
    }
    if (default_fn_) default_fn_(exec);
  }

  void collect_void_steps(std::vector<detail::RsVoidStep>& out) const {
    auto self = *this;
    out.push_back([self](detail::RsExecState& exec) { self.execute(exec); });
  }

 private:
  template <typename Step>
  static detail::RsVoidStep wrap_step(Step&& step) {
    auto s = std::forward<Step>(step);
    return [s](detail::RsExecState& exec) {
      auto copy = s;
      detail::invoke_step_with_args(copy, exec);
    };
  }

  struct Branch {
    std::vector<KeyT> values;
    detail::RsVoidStep action;
  };

  KeyOrFn key_fn_;
  std::vector<Branch> branches_;
  detail::RsVoidStep default_fn_;
};

template <typename KeyOrFn>
inline RsCaseBuilder<std::decay_t<KeyOrFn>> rs_case(KeyOrFn&& key) {
  return RsCaseBuilder<std::decay_t<KeyOrFn>>(std::forward<KeyOrFn>(key));
}

template <typename KeyOrFn, typename Clause0, typename... Clauses>
inline RsCaseBuilder<std::decay_t<KeyOrFn>> rs_case(KeyOrFn&& key, Clause0&& c0, Clauses&&... rest) {
  auto b = RsCaseBuilder<std::decay_t<KeyOrFn>>(std::forward<KeyOrFn>(key));
  auto add_clause = [&](auto&& cl) {
    using CT = std::decay_t<decltype(cl)>;
    if constexpr (std::is_same_v<CT, RsDefaultClause<decltype(cl.step)>>) {
      b.otherwise(cl.step);
    } else {
      for (auto const& v : cl.values) {
        b.when(v, cl.step);
      }
    }
  };
  add_clause(std::forward<Clause0>(c0));
  (add_clause(std::forward<Clauses>(rest)), ...);
  return b;
}

template <typename... Branches>
struct RsJoinExpr : detail::RsStepTag {
  RsJoinExpr(double w, Branches... b) : weight(w), branches(std::move(b)...) {
    if (!(weight >= 0.0 && weight <= 1.0)) {
      throw std::invalid_argument("crv_randsequence rs_join weight must be in [0.0, 1.0]");
    }
  }

  template <typename... Args>
  void execute(detail::RsExecState& exec, Args&...) const;

  void collect_void_steps(std::vector<detail::RsVoidStep>& out) const {
    auto self = *this;
    out.push_back([self](detail::RsExecState& exec) { self.execute(exec); });
  }

  double weight;
  std::tuple<Branches...> branches;
};

template <typename First, typename... Rest>
inline auto rs_join(First&& first, Rest&&... rest) {
  if constexpr (std::is_arithmetic_v<std::decay_t<First>> &&
                !std::is_same_v<std::decay_t<First>, bool>) {
    static_assert(sizeof...(Rest) >= 1, "rs_join(weight, branches...) requires at least one branch");
    return RsJoinExpr<std::decay_t<Rest>...>(static_cast<double>(first), std::forward<Rest>(rest)...);
  } else {
    return RsJoinExpr<std::decay_t<First>, std::decay_t<Rest>...>(0.5, std::forward<First>(first),
                                                                   std::forward<Rest>(rest)...);
  }
}

template <typename R, typename... Args>
class production_handle<R(Args...)> {
 public:
  production_handle() = default;
  production_handle(crv_randsequence* owner, detail::RsProductionImpl<R(Args...)>* impl)
      : owner_(owner), impl_(impl) {}

  template <typename First, typename... Rest>
  production_handle& rule(First&& first, Rest&&... rest) {
    ensure_valid_();
    if constexpr (sizeof...(Rest) >= 1 && detail::is_weight_arg<First>::value) {
      add_weighted_rule_(std::forward<First>(first), std::forward<Rest>(rest)...);
    } else {
      add_weighted_rule_(1ULL, std::forward<First>(first), std::forward<Rest>(rest)...);
    }
    return *this;
  }

  R operator()(Args... args) const;

  template <typename Dummy = R, typename = std::enable_if_t<std::is_void_v<Dummy>>>
  void do_return() const {
    detail::RsExecState* exec = detail::active_rs_exec_state();
    if (exec && exec->current_frame) {
      if (!exec->current_frame->is_void()) {
        throw std::logic_error("crv_randsequence: production_handle::do_return frame type mismatch");
      }
      exec->current_frame->returned = true;
    }
  }

  template <typename T, typename Dummy = R, typename = std::enable_if_t<!std::is_void_v<Dummy>>>
  void do_return(T&& val) const {
    detail::RsExecState* exec = detail::active_rs_exec_state();
    if (exec && exec->current_frame) {
      auto* typed = dynamic_cast<detail::RsTypedFrame<R>*>(exec->current_frame);
      if (!typed) {
        throw std::logic_error("crv_randsequence: production_handle::do_return frame type mismatch");
      }
      using ValT = std::decay_t<T>;
      if constexpr ((std::is_same_v<ValT, char const*> || std::is_same_v<ValT, char*>) &&
                    !std::is_pointer_v<R>) {
        if (val == nullptr) {
          throw std::logic_error("crv_randsequence: null pointer in production_handle::do_return");
        }
      }
      typed->ret_val = static_cast<R>(std::forward<T>(val));
      exec->current_frame->returned = true;
    }
  }

  bool valid() const noexcept { return owner_ != nullptr && impl_ != nullptr; }

  detail::RsProductionImpl<R(Args...)>* impl() const noexcept { return impl_; }
  crv_randsequence* owner() const noexcept { return owner_; }

 private:
  void ensure_valid_() const {
    if (!valid()) throw std::logic_error("crv_randsequence: null production_handle");
  }

  template <typename Weight, typename... Steps>
  void add_weighted_rule_(Weight&& w, Steps&&... steps) {
    typename detail::RsProductionImpl<R(Args...)>::Rule r;
    auto w_copy = std::forward<Weight>(w);
    r.weight_fn = [w_copy]() mutable -> std::uint64_t { return detail::eval_weight(w_copy); };
    r.steps.reserve(sizeof...(Steps));
    (append_step_(r.steps, std::forward<Steps>(steps)), ...);
    impl_->rules.push_back(std::move(r));
  }

  template <typename Step>
  static void append_step_(std::vector<detail::RsBoundStep<Args...>>& out, Step&& step) {
    auto s_copy = std::forward<Step>(step);
    out.push_back([s_copy](detail::RsExecState& exec, Args... args) mutable {
      detail::invoke_step_with_args(s_copy, exec, args...);
    });
  }

  crv_randsequence* owner_ = nullptr;
  detail::RsProductionImpl<R(Args...)>* impl_ = nullptr;
};

class crv_randsequence : public crv_object {
 public:
  explicit crv_randsequence(crv_object_name = "randseq") {}

  crv_randsequence(crv_randsequence const&) = delete;
  crv_randsequence& operator=(crv_randsequence const&) = delete;

  std::string obj_kind() const override { return "crv_randsequence"; }

  std::mt19937& rng() { return ensure_rng_(); }

  template <typename Sig = void()>
  production_handle<Sig> production(std::string const& name) {
    auto it = name_to_index_.find(name);
    if (it != name_to_index_.end()) {
      detail::RsProductionBase* base = productions_[it->second].get();
      if (base->signature_type() != std::type_index(typeid(Sig))) {
        throw std::logic_error("crv_randsequence: production '" + name + "' signature mismatch");
      }
      return production_handle<Sig>(this, static_cast<detail::RsProductionImpl<Sig>*>(base));
    }
    auto impl = std::make_unique<detail::RsProductionImpl<Sig>>();
    auto* raw = impl.get();
    std::size_t idx = productions_.size();
    productions_.push_back(std::move(impl));
    name_to_index_[name] = idx;
    if (!has_entry_) {
      if constexpr (std::is_same_v<Sig, void()>) {
        entry_ = raw;
        has_entry_ = true;
      }
    }
    return production_handle<Sig>(this, raw);
  }

  void set_entry(production_handle<void()> entry) {
    if (!entry.valid() || entry.owner() != this) {
      throw std::logic_error("crv_randsequence: invalid entry production");
    }
    entry_ = entry.impl();
    has_entry_ = true;
  }

  bool has_production(std::string const& name) const {
    return name_to_index_.find(name) != name_to_index_.end();
  }

  void run() { (void)execute(); }

  bool execute() {
    if (!has_entry_ || !entry_) {
      throw std::logic_error("crv_randsequence: no entry production configured");
    }
    ScopedRngGuard rng_guard(&ensure_rng_());
    if (detail::active_rs_exec_state() && detail::active_rs_exec_state()->seq == this) {
      return entry_->invoke_void(*detail::active_rs_exec_state()) && !detail::active_rs_exec_state()->aborted;
    }
    detail::RsExecState exec;
    exec.seq = this;
    detail::ScopedRsExecGuard exec_guard(&exec);
    entry_->invoke_void(exec);
    return !exec.aborted;
  }

  template <typename R, typename... Args>
  R run(production_handle<R(Args...)> prod, Args... args) {
    if (!prod.valid() || prod.owner() != this) {
      throw std::logic_error("crv_randsequence: invalid production handle for run()");
    }
    return prod(std::forward<Args>(args)...);
  }

  void do_break() {
    detail::RsExecState* exec = detail::active_rs_exec_state();
    if (exec) exec->aborted = true;
  }

  void do_return() {
    detail::RsExecState* exec = detail::active_rs_exec_state();
    if (exec && exec->current_frame) exec->current_frame->returned = true;
  }

  template <typename T>
  void do_return(T&& val) {
    detail::RsExecState* exec = detail::active_rs_exec_state();
    if (!exec || !exec->current_frame) return;
    if (exec->current_frame->is_void()) {
      throw std::logic_error("crv_randsequence: cannot return a value from a void production");
    }
    using ValT = std::decay_t<T>;
    if (auto* f = dynamic_cast<detail::RsTypedFrame<ValT>*>(exec->current_frame)) {
      f->ret_val = std::forward<T>(val);
      exec->current_frame->returned = true;
      return;
    }
    bool ok = false;
    if constexpr (std::is_same_v<ValT, bool>) {
      ok = exec->current_frame->try_set_bool(val);
    } else if constexpr (std::is_integral_v<ValT>) {
      if constexpr (std::is_signed_v<ValT>) {
        ok = exec->current_frame->try_set_signed(static_cast<long long>(val));
      } else {
        ok = exec->current_frame->try_set_unsigned(static_cast<unsigned long long>(val));
      }
    } else if constexpr (std::is_floating_point_v<ValT>) {
      ok = exec->current_frame->try_set_float(static_cast<long double>(val));
    } else if constexpr (std::is_same_v<ValT, char const*> || std::is_same_v<ValT, char*>) {
      ok = exec->current_frame->try_set_cstr(val);
    } else if constexpr (std::is_same_v<ValT, std::string>) {
      ok = exec->current_frame->try_set_string(val);
    } else {
      ok = exec->current_frame->try_set_exact(typeid(ValT), &val);
    }
    if (!ok) {
      throw std::logic_error("crv_randsequence: incompatible return value type in do_return");
    }
    exec->current_frame->returned = true;
  }

 private:
  std::vector<std::unique_ptr<detail::RsProductionBase>> productions_;
  std::unordered_map<std::string, std::size_t> name_to_index_;
  detail::RsProductionBase* entry_ = nullptr;
  bool has_entry_ = false;
};

namespace detail {

template <typename R, typename... Args>
inline typename RsProductionImpl<R(Args...)>::Rule const*
RsProductionImpl<R(Args...)>::choose_rule(RsExecState& exec) {
  if (rules.empty()) return nullptr;
  std::vector<std::uint64_t> weights;
  weights.reserve(rules.size());
  unsigned __int128 total = 0;
  for (Rule const& r : rules) {
    std::uint64_t w = r.weight_fn ? r.weight_fn() : 0ULL;
    weights.push_back(w);
    total += static_cast<unsigned __int128>(w);
  }
  if (total == 0) return nullptr;
  unsigned __int128 pick = 0;
  if (total <= static_cast<unsigned __int128>(std::numeric_limits<std::uint64_t>::max())) {
    std::uniform_int_distribution<std::uint64_t> dist(0, static_cast<std::uint64_t>(total) - 1);
    pick = static_cast<unsigned __int128>(dist(exec.seq->rng()));
  } else {
    unsigned __int128 const limit = total - 1;
    unsigned bits = 0;
    for (unsigned __int128 tmp = limit; tmp != 0; tmp >>= 1) ++bits;
    unsigned __int128 const mask =
        (bits == 128) ? ~static_cast<unsigned __int128>(0) : ((static_cast<unsigned __int128>(1) << bits) - 1);
    for (;;) {
      unsigned __int128 cand = 0;
      for (unsigned b = 0; b < bits; b += 32) {
        cand = (cand << 32) | static_cast<std::uint32_t>(exec.seq->rng()());
      }
      cand &= mask;
      if (cand < total) {
        pick = cand;
        break;
      }
    }
  }
  unsigned __int128 accum = 0;
  for (std::size_t i = 0; i < rules.size(); ++i) {
    if (weights[i] == 0ULL) continue;
    accum += static_cast<unsigned __int128>(weights[i]);
    if (pick < accum) return &rules[i];
  }
  return nullptr;
}

template <typename R, typename... Args>
inline R RsProductionImpl<R(Args...)>::execute(RsExecState& exec, Args... args) {
  RsTypedFrame<R> frame;
  ScopedRsFrameGuard frame_guard(exec, &frame);
  Rule const* chosen = choose_rule(exec);
  if (chosen) {
    for (auto const& step : chosen->steps) {
      if (exec.aborted || frame.returned) break;
      step(exec, args...);
    }
  }
  if constexpr (!std::is_void_v<R>) {
    if (frame.ret_val.has_value()) return *frame.ret_val;
    return R{};
  }
}

}  // namespace detail

template <typename MinOrFn, typename MaxOrFn, typename... Steps>
template <typename... Args>
inline void RsRepeatRangeExpr<MinOrFn, MaxOrFn, Steps...>::execute(detail::RsExecState& exec, Args&... args) const {
  auto lo_copy = min_count;
  auto hi_copy = max_count;
  std::int64_t lo = static_cast<std::int64_t>(detail::eval_count(lo_copy));
  std::int64_t hi = static_cast<std::int64_t>(detail::eval_count(hi_copy));
  if (hi < lo) return;
  std::uniform_int_distribution<std::int64_t> dist(lo, hi);
  std::size_t const n = static_cast<std::size_t>(dist(exec.seq->rng()));
  for (std::size_t i = 0; i < n; ++i) {
    if (exec.aborted || (exec.current_frame && exec.current_frame->returned)) break;
    body.execute(exec, args...);
  }
}

template <typename... Branches>
template <typename... Args>
inline void RsJoinExpr<Branches...>::execute(detail::RsExecState& exec, Args&...) const {
  if (exec.aborted || (exec.current_frame && exec.current_frame->returned)) return;
  struct BranchStream {
    std::vector<detail::RsVoidStep> steps;
    std::size_t pos = 0;
    detail::RsTypedFrame<void> frame;
    bool from_production = false;
  };

  std::vector<BranchStream> streams(sizeof...(Branches));
  std::size_t b_idx = 0;
  auto init_branch = [&](auto const& br) {
    BranchStream& st = streams[b_idx++];
    using BType = std::decay_t<decltype(br)>;
    if constexpr (detail::is_production_handle<BType>::value) {
      if (!br.valid()) throw std::logic_error("crv_randsequence: null production_handle in rs_join");
      st.steps = br.impl()->sample_join_steps(exec);
      st.from_production = true;
    } else if constexpr (std::is_base_of_v<detail::RsStepTag, BType>) {
      br.collect_void_steps(st.steps);
    } else {
      st.steps.push_back([br](detail::RsExecState& ex) {
        auto copy = br;
        detail::invoke_step_with_args(copy, ex);
      });
    }
  };
  std::apply([&](auto const&... brs) { (init_branch(brs), ...); }, branches);

  for (;;) {
    if (exec.aborted || (exec.current_frame && exec.current_frame->returned)) break;
    std::vector<std::size_t> active;
    active.reserve(streams.size());
    std::size_t min_rem = std::numeric_limits<std::size_t>::max();
    std::size_t max_rem = 0;
    double sum_rem = 0.0;
    for (std::size_t i = 0; i < streams.size(); ++i) {
      if (streams[i].pos < streams[i].steps.size() && !streams[i].frame.returned) {
        std::size_t rem = streams[i].steps.size() - streams[i].pos;
        active.push_back(i);
        min_rem = std::min(min_rem, rem);
        max_rem = std::max(max_rem, rem);
        sum_rem += static_cast<double>(rem);
      }
    }
    if (active.empty()) break;

    std::size_t chosen_idx = active[0];
    if (active.size() > 1) {
      std::size_t min_cnt = 0;
      std::size_t max_cnt = 0;
      for (std::size_t idx : active) {
        std::size_t rem = streams[idx].steps.size() - streams[idx].pos;
        if (rem == min_rem) ++min_cnt;
        if (rem == max_rem) ++max_cnt;
      }
      std::vector<double> probs(active.size(), 0.0);
      for (std::size_t k = 0; k < active.size(); ++k) {
        std::size_t rem = streams[active[k]].steps.size() - streams[active[k]].pos;
        double const whalf = static_cast<double>(rem) / sum_rem;
        if (weight <= 0.5) {
          double const w0 = (rem == min_rem) ? (1.0 / static_cast<double>(min_cnt)) : 0.0;
          double const alpha = weight * 2.0;
          probs[k] = (1.0 - alpha) * w0 + alpha * whalf;
        } else {
          double const w1 = (rem == max_rem) ? (1.0 / static_cast<double>(max_cnt)) : 0.0;
          double const alpha = (weight - 0.5) * 2.0;
          probs[k] = (1.0 - alpha) * whalf + alpha * w1;
        }
      }
      std::discrete_distribution<std::size_t> dist(probs.begin(), probs.end());
      chosen_idx = active[dist(exec.seq->rng())];
    }

    BranchStream& chosen = streams[chosen_idx];
    auto& step = chosen.steps[chosen.pos++];
    if (chosen.from_production) {
      detail::ScopedRsFrameGuard branch_frame(exec, &chosen.frame);
      step(exec);
    } else {
      step(exec);
    }
  }
}

template <typename R, typename... Args>
inline R production_handle<R(Args...)>::operator()(Args... args) const {
  ensure_valid_();
  detail::RsExecState* cur = detail::active_rs_exec_state();
  if (cur && cur->seq == owner_) {
    return impl_->execute(*cur, std::forward<Args>(args)...);
  }
  ScopedRngGuard rng_guard(&owner_->rng());
  detail::RsExecState exec;
  exec.seq = owner_;
  detail::ScopedRsExecGuard exec_guard(&exec);
  return impl_->execute(exec, std::forward<Args>(args)...);
}

}  // namespace crave

#endif  // C++17

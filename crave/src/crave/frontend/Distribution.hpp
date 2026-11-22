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

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <mutex>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
	
#include "ConstraintType.hpp"
#include "WeightedRange.hpp"
#include "../RandomSeedManager.hpp"

namespace crave {

extern RandomSeedManager rng;

template <typename T>
struct distribution_tag;

namespace detail {

template <typename T>
inline typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value, T>::type
from_distribution_value(typename distribution_value_type<T>::type v) {
  return static_cast<T>(v);
}

template <typename T>
inline typename std::enable_if<!std::is_integral<T>::value && !std::is_enum<T>::value, T>::type
from_distribution_value(typename distribution_value_type<T>::type v) {
  T out;
  out = v;
  return out;
}

struct uint256_t {
  std::array<uint64_t, 4> w{{0, 0, 0, 0}};

  static uint256_t zero() { return uint256_t{}; }
  static uint256_t from_u64(uint64_t v) { return uint256_t{{{v, 0, 0, 0}}}; }
  static uint256_t from_u128(unsigned __int128 v) {
    return uint256_t{{{static_cast<uint64_t>(v), static_cast<uint64_t>(v >> 64), 0, 0}}};
  }

  bool is_zero() const { return w[0] == 0 && w[1] == 0 && w[2] == 0 && w[3] == 0; }
  bool fits_u64() const { return w[1] == 0 && w[2] == 0 && w[3] == 0; }
  uint64_t lo64() const { return w[0]; }

  unsigned bit_length() const {
    for (int i = 3; i >= 0; --i) {
      if (w[static_cast<std::size_t>(i)] != 0) {
        unsigned bits = 0;
        uint64_t v = w[static_cast<std::size_t>(i)];
        while (v != 0) {
          ++bits;
          v >>= 1;
        }
        return static_cast<unsigned>(i) * 64u + bits;
      }
    }
    return 0;
  }

  bool operator<(uint256_t const& o) const {
    for (int i = 3; i >= 0; --i) {
      std::size_t idx = static_cast<std::size_t>(i);
      if (w[idx] != o.w[idx]) return w[idx] < o.w[idx];
    }
    return false;
  }

  bool operator<=(uint256_t const& o) const { return !(o < *this); }

  uint256_t operator+(uint256_t const& o) const {
    uint256_t r;
    unsigned __int128 carry = 0;
    for (std::size_t i = 0; i < 4; ++i) {
      unsigned __int128 sum = static_cast<unsigned __int128>(w[i]) + o.w[i] + carry;
      r.w[i] = static_cast<uint64_t>(sum);
      carry = sum >> 64;
    }
    return r;
  }

  uint256_t operator-(uint256_t const& o) const {
    uint256_t r;
    uint64_t borrow = 0;
    for (std::size_t i = 0; i < 4; ++i) {
      unsigned __int128 lhs = w[i];
      unsigned __int128 rhs = static_cast<unsigned __int128>(o.w[i]) + borrow;
      if (lhs >= rhs) {
        r.w[i] = static_cast<uint64_t>(lhs - rhs);
        borrow = 0;
      } else {
        r.w[i] = static_cast<uint64_t>((static_cast<unsigned __int128>(1) << 64) + lhs - rhs);
        borrow = 1;
      }
    }
    return r;
  }
};

template <typename Generator>
inline uint256_t sample_uniform_u256(uint256_t const& total, Generator& gen) {
  if (total.fits_u64()) {
    return uint256_t::from_u64(std::uniform_int_distribution<uint64_t>(0, total.lo64() - 1)(gen));
  }
  uint256_t const max_val = total - uint256_t::from_u64(1);
  unsigned const bits = max_val.bit_length();
  std::size_t const top_word = static_cast<std::size_t>((bits - 1) / 64);
  unsigned const top_bits = ((bits - 1) % 64) + 1;
  uint64_t const top_mask = (top_bits == 64) ? ~uint64_t(0) : ((uint64_t(1) << top_bits) - 1);
  for (;;) {
    uint256_t r;
    for (std::size_t i = 0; i <= top_word; ++i) {
      uint64_t hi = static_cast<uint64_t>(static_cast<uint32_t>(gen()));
      uint64_t lo = static_cast<uint64_t>(static_cast<uint32_t>(gen()));
      r.w[i] = (hi << 32) | lo;
    }
    r.w[top_word] &= top_mask;
    if (r < total) return r;
  }
}

template <typename T, typename ValT, typename Generator>
inline typename std::enable_if<(sizeof(ValT) < sizeof(int)), T>::type
uniformly_distributed_impl(ValT lv, ValT rv, Generator& gen) {
  int sampled = std::uniform_int_distribution<int>(static_cast<int>(lv), static_cast<int>(rv))(gen);
  return from_distribution_value<T>(static_cast<ValT>(sampled));
}

template <typename T, typename ValT, typename Generator>
inline typename std::enable_if<(sizeof(ValT) >= sizeof(int)), T>::type
uniformly_distributed_impl(ValT lv, ValT rv, Generator& gen) {
  ValT sampled = std::uniform_int_distribution<ValT>(lv, rv)(gen);
  return from_distribution_value<T>(sampled);
}

}  // namespace detail

template <typename T, typename Generator>
T uniformly_distributed_value(T const& left, T const& right, Generator& gen) {
  typedef typename distribution_value_type<T>::type val_t;
  val_t const lv = distribution_value_type<T>::value(left);
  val_t const rv = distribution_value_type<T>::value(right);
  if (lv >= rv) return left;
  return detail::uniformly_distributed_impl<T, val_t, Generator>(lv, rv, gen);
}

/*!
 * \ingroup oldAPI
 * \ingroup newAPI
 * \brief Distribution of values to used in constraints.
 * 
 * This class allows you to create a distribution.
 * You can use crave::dist operator to force a variable to be inside a distribution.
 * A distribution is defined by ranges.
 * For details see crave::range and crave::weighted_range.
 */
template <typename T>
struct distribution {
  /*!
   * \brief Defines an empty distribution.
   * 
   * An empty distribution with no ranges corresponds to the whole value range of the base type T.
   */
  distribution() : items_(), ranges_(), item_accum_weights_(), total_weight_(detail::uint256_t::zero()) {}

  /*!
   * \brief Defines a distribution with a weighted range
   * 
   * A weighted range uses weights to define which values should occur more often.
   * See crave::weighted_range for details.
   * 
   * \param range weighted range for distribution
   */
  distribution(const weighted_range<T>& range)
      : items_(), ranges_(), item_accum_weights_(), total_weight_(detail::uint256_t::zero()) {
    addRange(range);
  }

  /**
   * \brief Adds another range to the distribution.
   * 
   * \param range A weighted range to add to this distribution
   * \return reference to this distribution
   */
  distribution& operator()(const weighted_range<T>& range) {
    addRange(range);
    return *this;
  }

  /**
   * \brief Creates a distribution to use in constraints.
   * 
   * This method creates a distribution_tag<T> which is recognized by the constraint frontend.
   * Typically this is an input argument for crave::dist.
   * 
   * \param range A (weighted) range to create a distribution of.
   * \return distribution_tag<T> for constraint definition
   */
  static distribution_tag<T> create(const weighted_range<T>& range) { return distribution_tag<T>(distribution(range)); }

  /**
   * \brief Creates a simple distribution with given left and right bound.
   * 
   * This method creates a distribution_tag<T> which is recognized by the constraint frontend.
   * It internally creates an equally distributed range from left to right bound.
   * 
   * \param left left bound of the range
   * \param right right bound of the range
   * \return distribution_tag<T> for constraint definition
   */
  static distribution_tag<T> simple_range(T left, T right) {
    return distribution_tag<T>(distribution(range<T>(left, right)));
  }

  /**
   * \brief Deletes all ranges of this distribution
   */
  void reset() {
    items_.clear();
    ranges_.clear();
    item_accum_weights_.clear();
    total_weight_ = detail::uint256_t::zero();
    has_items_ = false;
  }

  /**
   * \brief Get all coalesced positive-weight ranges of this distribution.
   * \return weighted ranges of this distribution
   */
  std::vector<weighted_range<T> > const& ranges() const { return ranges_; }

  std::vector<weighted_range<T> > const& items() const { return items_; }

  bool hasItems() const { return has_items_; }

  void markHasItems() { has_items_ = true; }

  bool isAllZero() const { return has_items_ && items_.empty(); }

  /*!
   * \brief Returns true if this is the default distribution of a variable.
   * \see implicit_distribution
   */
  bool isImplicit() const { return implicit_; }

  void setImplicit(bool implicit = true) { implicit_ = implicit; }

  /**
   * \brief Generate next random distribution value.
   * 
   * If no ranges exist for this distribution, an uniform distribution over all possible values of T is used.
   * 
   * \return T Next random value based on specified weights of ranges.
   */
  T nextValue() const {
    if (isAllZero() || (has_items_ && total_weight_.is_zero())) {
      throw std::runtime_error("Distribution has zero total weight.");
    }
    return with_active_or_global_rng([this](std::mt19937& gen) -> T {
      if (items_.empty()) {
        return uniformly_distributed_value(std::numeric_limits<T>::min(), std::numeric_limits<T>::max(), gen);
      }
      if (items_.size() == 1) {
        return uniformly_distributed_value(items_[0].left_, items_[0].right_, gen);
      }
      detail::uint256_t const r = detail::sample_uniform_u256(total_weight_, gen);
      auto it = std::upper_bound(item_accum_weights_.begin(), item_accum_weights_.end(), r);
      std::size_t const idx = static_cast<std::size_t>(it - item_accum_weights_.begin());
      return uniformly_distributed_value(items_[idx].left_, items_[idx].right_, gen);
    });
  }

 protected:
  void addRange(weighted_range<T> wr) {
    has_items_ = true;
    __int128 const lv = static_cast<__int128>(distribution_value_type<T>::value(wr.left_));
    __int128 const rv = static_cast<__int128>(distribution_value_type<T>::value(wr.right_));
    if (lv > rv || wr.raw_weight_ == 0 || (wr.kind_ == dist_weight_kind::per_range && wr.weight_ == 0)) {
      return;
    }
    unsigned __int128 const card = static_cast<unsigned __int128>(rv - lv) + 1u;
    unsigned __int128 const item_w = (wr.kind_ == dist_weight_kind::per_value)
                                         ? card * static_cast<unsigned __int128>(wr.raw_weight_)
                                         : static_cast<unsigned __int128>(wr.raw_weight_);
    items_.push_back(wr);
    total_weight_ = total_weight_ + detail::uint256_t::from_u128(item_w);
    item_accum_weights_.push_back(total_weight_);
    rebuildRanges();
  }

  void rebuildRanges() {
    ranges_.clear();
    if (items_.empty()) return;
    std::vector<std::pair<__int128, __int128> > intervals;
    intervals.reserve(items_.size());
    for (weighted_range<T> const& it : items_) {
      intervals.emplace_back(static_cast<__int128>(distribution_value_type<T>::value(it.left_)),
                             static_cast<__int128>(distribution_value_type<T>::value(it.right_)));
    }
    std::sort(intervals.begin(), intervals.end(),
              [](std::pair<__int128, __int128> const& a, std::pair<__int128, __int128> const& b) {
                return a.first < b.first || (a.first == b.first && a.second < b.second);
              });
    typedef typename distribution_value_type<T>::type val_t;
    std::pair<__int128, __int128> cur = intervals[0];
    uint64_t accum = 0;
    auto push_coalesced = [&](std::pair<__int128, __int128> const& iv) {
      T tl = detail::from_distribution_value<T>(static_cast<val_t>(iv.first));
      T tr = detail::from_distribution_value<T>(static_cast<val_t>(iv.second));
      weighted_range<T> cr(tl, tr, 1, dist_weight_kind::per_value);
      uint64_t const add = cr.weight_;
      accum = (std::numeric_limits<uint64_t>::max() - accum < add) ? std::numeric_limits<uint64_t>::max()
                                                                   : (accum + add);
      cr.accumWeight_ = accum;
      ranges_.push_back(cr);
    };
    for (std::size_t i = 1; i < intervals.size(); ++i) {
      if (intervals[i].first <= cur.second + 1) {
        if (intervals[i].second > cur.second) cur.second = intervals[i].second;
      } else {
        push_coalesced(cur);
        cur = intervals[i];
      }
    }
    push_coalesced(cur);
  }

 protected:
  std::vector<weighted_range<T> > items_;
  std::vector<weighted_range<T> > ranges_;
  std::vector<detail::uint256_t> item_accum_weights_;
  detail::uint256_t total_weight_;
  bool has_items_ = false;
  bool implicit_ = false;
};

template <>
struct distribution<bool> {
  explicit distribution(const double prob = 0.5);

  static distribution_tag<bool> create(const double prob);

  bool nextValue() const;

  std::vector<weighted_range<bool> > const& ranges() const;

  double prob() const { return prob_; }

  bool hasItems() const { return false; }

  bool isAllZero() const { return false; }

  bool isImplicit() const { return implicit_; }

  void setImplicit(bool implicit = true) { implicit_ = implicit; }

 private:
  double prob_;
  bool implicit_ = false;
};

template <typename T>
struct distribution_tag : public Constraint<typename boost::proto::terminal<distribution<T> >::type> {
  typedef Constraint<typename boost::proto::terminal<distribution<T> >::type> base_type;
  explicit distribution_tag(distribution<T> d) : base_type(boost::proto::make_expr<boost::proto::tag::terminal>(d)) {}

  distribution_tag& operator()(const weighted_range<T>& range) {
    boost::proto::value (*this)(range);
    return *this;
  }
};

template <typename T>
distribution_tag<T> make_distribution(weighted_range<T> const& range) {
  return distribution<T>::create(range);
}

template <typename T, typename... Args>
distribution_tag<T> make_distribution(weighted_range<T> const& range, Args... args) {
  return make_distribution(args...)(range);
}

/*!
 * \brief Marks a distribution as the default distribution of a variable.
 *
 * A default distribution is not specified by the user. It only says that the
 * values of the variable should be spread over its complete range. So in
 * contrast to a distribution of the user, it does not keep the solver from
 * sampling the solutions of the constraints on the variable instead.
 */
template <typename T>
distribution_tag<T> implicit_distribution(distribution_tag<T> dist) {
  boost::proto::value(dist).setImplicit();
  return dist;
}

}  // namespace crave

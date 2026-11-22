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

#include <boost/proto/core.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>

#include "bitsize_traits.hpp"

namespace crave {

enum class dist_weight_kind : uint8_t { per_range = 0, per_value = 1 };

/**
 * \ingroup oldAPI
 * \ingroup newAPI
 * \brief A weighted range of values of type T
 * 
 * A weighted range is a range with an annotated weight.
 * The weight indicates the chance to be hit inside a distribution where this range is included.
 * A greater weight means the distribution will hit the range more often. 
 */
template <typename T>
struct weighted_range {
  /**
   * \brief Defines a new weighted range (per-range total weight, IEEE 1800 :/ semantics).
   * 
   * \param l Left value of the range (inclusive)
   * \param r Right value of the range (inclusive)
   * \param w Weight of the range
   */
  weighted_range(T l, T r, uint64_t w)
      : left_(l), right_(r), weight_(w), accumWeight_(0), kind_(dist_weight_kind::per_range), raw_weight_(w) {}

  weighted_range(T l, T r, uint64_t w, dist_weight_kind kind)
      : left_(l),
        right_(r),
        weight_(kind == dist_weight_kind::per_value ? saturated_total_weight(l, r, w) : w),
        accumWeight_(0),
        kind_(kind),
        raw_weight_(w) {}

  static uint64_t saturated_total_weight(T l, T r, uint64_t w) {
    __int128 const lv = static_cast<__int128>(distribution_value_type<T>::value(l));
    __int128 const rv = static_cast<__int128>(distribution_value_type<T>::value(r));
    if (lv > rv || w == 0) return 0;
    unsigned __int128 const card = static_cast<unsigned __int128>(rv - lv) + 1u;
    unsigned __int128 const prod = card * static_cast<unsigned __int128>(w);
    return prod > std::numeric_limits<uint64_t>::max() ? std::numeric_limits<uint64_t>::max()
                                                       : static_cast<uint64_t>(prod);
  }

  /**
   * \brief Compares a weighted range to another range.
   * 
   * A range is less than another range, if its left value is less than the left value of the other range or its left values are equal and the right value of the range is less than the right value of the other range.
   * In all other cases, it is not.
   * 
   * \param other Other range to compare with
   * \return true if this range is less than the other range, false otherwise.
   */
  bool operator<(const weighted_range<T>& other) const {
    auto const l = distribution_value_type<T>::value(left_);
    auto const ol = distribution_value_type<T>::value(other.left_);
    if (l < ol) return true;
    if (l > ol) return false;
    auto const r = distribution_value_type<T>::value(right_);
    auto const or_ = distribution_value_type<T>::value(other.right_);
    if (r < or_) return true;
    return false;
  }

  /**
   * \brief Checks if this range overlaps another range.
   * 
   * \param other Other range to check overlap with
   * \return true if this range overlaps the other range, false otherwise.
   */
  bool overlap(const weighted_range<T>& other) const {
    auto const l = distribution_value_type<T>::value(left_);
    auto const r = distribution_value_type<T>::value(right_);
    auto const ol = distribution_value_type<T>::value(other.left_);
    auto const or_ = distribution_value_type<T>::value(other.right_);
    if (l > r || ol > or_) return false;
    return l <= or_ && ol <= r;
  }

  T left_;
  T right_;
  uint64_t weight_;
  uint64_t accumWeight_;
  dist_weight_kind kind_;
  uint64_t raw_weight_;
};

namespace detail {
template <typename T, bool IsExpr = boost::proto::is_expr<typename std::decay<T>::type>::value>
struct range_capture {
  typedef typename std::decay<T>::type type;
  static type capture(T const& v) { return v; }
};

template <typename T>
struct range_capture<T, true> {
  typedef typename boost::proto::result_of::deep_copy<T const&>::type type;
  static type capture(T const& v) { return boost::proto::deep_copy(v); }
};
}  // namespace detail

template <typename T, typename Hi = void>
struct range;

/**
 * \ingroup oldAPI
 * \ingroup newAPI
 * \brief A range of values of type T.
 * 
 * A range with default weight that is equal to its size.
 * For details about the weight see the base class crave::weighted_range.
 */
template <typename T>
struct range<T, void> : weighted_range<T> {
  /*!
   * \brief Defines a new range from a left value to a right value.
   * 
   * \param l Left value of the range (inclusive)
   * \param r Right value of the range (inclusive)
   */
  range(T l, T r) : weighted_range<T>(l, r, 1, dist_weight_kind::per_value) {}

  T const& lo() const { return this->left_; }
  T const& hi() const { return this->right_; }

  weighted_range<T> per_value(uint64_t w) const {
    return weighted_range<T>(this->left_, this->right_, w, dist_weight_kind::per_value);
  }

  weighted_range<T> per_range(uint64_t w) const {
    return weighted_range<T>(this->left_, this->right_, w, dist_weight_kind::per_range);
  }

 private:
  static uint64_t weight(T l, T r) { return weighted_range<T>::saturated_total_weight(l, r, 1); }
};

/**
 * \ingroup newAPI
 * \brief A closed symbolic or mixed-type range [lo, hi] for use in crave::inside.
 */
template <typename Lo, typename Hi>
struct range {
  template <typename L, typename H>
  range(L const& l, H const& h)
      : lo_(detail::range_capture<L>::capture(l)), hi_(detail::range_capture<H>::capture(h)) {}

  Lo const& lo() const { return lo_; }
  Hi const& hi() const { return hi_; }

  Lo lo_;
  Hi hi_;
};

#if __cplusplus >= 201703L
template <typename T>
range(T const&, T const&) -> range<
    typename std::conditional<!boost::proto::is_expr<typename std::decay<T>::type>::value &&
                                  is_distribution_scalar<typename std::decay<T>::type>::value,
                              typename std::decay<T>::type, typename detail::range_capture<T>::type>::type,
    typename std::conditional<!boost::proto::is_expr<typename std::decay<T>::type>::value &&
                                  is_distribution_scalar<typename std::decay<T>::type>::value,
                              void, typename detail::range_capture<T>::type>::type>;

template <typename Lo, typename Hi>
range(Lo const&, Hi const&) -> range<typename detail::range_capture<Lo>::type,
                                     typename detail::range_capture<Hi>::type>;
#endif

/**
 * \ingroup oldAPI
 * \ingroup newAPI
 * \brief A single value of type T with a weight.
 * 
 * A weighted value is a weighted range containing only one element.
 * For details about the weight see the base class crave::weighted_range.
 */
template <typename T>
struct weighted_value : weighted_range<T> {
  /*!
   * \brief Defines a new weighted value with its weight.
   * 
   * \param v The value.
   * \param w The weight.
   */
  weighted_value(T v, uint64_t w) : weighted_range<T>(v, v, w, dist_weight_kind::per_value) {}
};

template <typename T>
struct weighted_range_per_value : weighted_range<T> {
  weighted_range_per_value(T l, T r, uint64_t w)
      : weighted_range<T>(l, r, w, dist_weight_kind::per_value) {}
};

template <typename T>
struct weighted_range_per_range : weighted_range<T> {
  weighted_range_per_range(T l, T r, uint64_t w)
      : weighted_range<T>(l, r, w, dist_weight_kind::per_range) {}
};

template <typename T>
inline weighted_range<T> dist_per_value(T const& l, T const& r, uint64_t w) {
  return weighted_range<T>(l, r, w, dist_weight_kind::per_value);
}

template <typename T>
inline weighted_range<T> dist_per_value(T const& v, uint64_t w) {
  return weighted_value<T>(v, w);
}

template <typename T>
inline weighted_range<T> dist_per_range(T const& l, T const& r, uint64_t w) {
  return weighted_range<T>(l, r, w, dist_weight_kind::per_range);
}

template <typename T>
inline weighted_range<T> dist_per_range(T const& v, uint64_t w) {
  return weighted_value<T>(v, w);
}
}  // namespace crave

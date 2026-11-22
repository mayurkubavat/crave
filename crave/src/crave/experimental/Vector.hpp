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

#include <string>
#include <vector>

#include "Object.hpp"
#include "Variable.hpp"

#include "../backend/Generator.hpp"
#include "../frontend/RandomBase.hpp"

namespace crave {

/**
* \ingroup newAPI
* \brief Class for a randomizable vector in the new API.
* 
* This class represents a vector of randomizable elements in CRAVE.
*/
template <typename T>
class crv_vector : public __rand_vec<T>, public crv_object {
 public:
  /**
  * \brief Define a crv_vector by a name.
  * \param name Name of the vector. "vec" if no name is given.
  */
  crv_vector(crv_object_name = "vec") : __rand_vec<T>() {}

  /**
   * \brief Deleted copy constructor.
   */
  crv_vector(crv_vector const&) = delete;

  std::string obj_kind() const override { return "crv_vector"; }

  /**
   * \brief Generates random values for this vector.
   * 
   * The size of the vector is fixed to be default_rand_vec_size()
   * It always returns true.
   * 
   * \return true
   */
  bool randomize() override {
    ScopedRngGuard rng_guard(&this->ensure_rng_());
    __rand_vec<T>::gen_values(default_rand_vec_size());
    return true;
  }
  
  void recursive_build(Generator& gen) const override { gen.addVecId(__rand_vec<T>::id()); }

  /**
   * \brief Gets the sum of the elements as a CRAVE expression, see Vector::sum().
   */
  auto sum() const { return (*this)().sum(); }

  /**
   * \brief Gets the product of the elements as a CRAVE expression, see Vector::product().
   */
  auto product() const { return (*this)().product(); }

  /**
   * \brief Gets the bitwise AND of the elements as a CRAVE expression, see Vector::and_().
   */
  auto and_() const { return (*this)().and_(); }

  /**
   * \brief Gets the bitwise OR of the elements as a CRAVE expression, see Vector::or_().
   */
  auto or_() const { return (*this)().or_(); }

  /**
   * \brief Gets the bitwise XOR of the elements as a CRAVE expression, see Vector::xor_().
   */
  auto xor_() const { return (*this)().xor_(); }

  template <typename Pred>
  auto sum_with(Pred&& pred) const {
    return (*this)().sum_with(std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto product_with(Pred&& pred) const {
    return (*this)().product_with(std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto and_with(Pred&& pred) const {
    return (*this)().and_with(std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto or_with(Pred&& pred) const {
    return (*this)().or_with(std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto xor_with(Pred&& pred) const {
    return (*this)().xor_with(std::forward<Pred>(pred));
  }

  template <typename Pred>
  auto with(Pred&& pred) const {
    return (*this)().with(std::forward<Pred>(pred));
  }

 protected:
  void collect_var_ids_for_randstate_(std::vector<unsigned>& out) const override {
    out.push_back(static_cast<unsigned>(__rand_vec<T>::id()));
    out.push_back(static_cast<unsigned>(__rand_vec<T>::size_var().id()));
  }
};

}  // namespace crave

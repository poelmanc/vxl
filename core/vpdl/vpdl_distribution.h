// This is core/vpdl/vpdl_distribution.h
#ifndef vpdl_distribution_h_
#define vpdl_distribution_h_
//:
// \file
// \author Matthew Leotta
// \date February 5, 2009
// \brief The templated base class for all distributions
//
// \verbatim
//  Modifications
//   None
// \endverbatim

#include <vpdl/vpdt/vpdt_field_traits.h>
#include <vpdl/vpdt/vpdt_field_default.h>
#include <vcl_cmath.h>

//: The base class for all probability distributions.
// There is a distinct polymorphic class hierarchy for each choice of
// template parameters.  The vector and matrix data types vary with both \c T and \c n.
// \tparam T is the scalar type use for numerical calculations (generally double or float)
// \tparam n is the fixed dimension of the space with special case 0 (the default)
//           indicating dynamic dimension set at run time.
// - For n > 1 the data types are vnl_vector_fixed<T,n> and vnl_matrix_fixed<T,n,n>
// - For n == 1 the data types are T and T
// - For n == 0 the data types are vnl_vector<T> and vnl_matrix<T>
//
template<class T, unsigned int n=0>
class vpdl_distribution 
{
 public:
  //: the data type used for vectors
  typedef typename vpdt_field_default<T,n>::type vector;
  //: the data type used for matrices
  typedef typename vpdt_field_traits<vector>::matrix_type matrix;
 
  //: Return the run time dimension, which does not equal \c n when \c n==0
  virtual unsigned int dimension() const = 0;

  //: Create a copy on the heap and return base class pointer
  virtual vpdl_distribution<T,n>* clone() const = 0;

  //: Evaluate the unnormalized density at a point
  // \note This is not a probability density. 
  // To make this a probability multiply by norm_const()
  // \sa prob_density
  virtual T density(const vector& pt) const = 0;

  //: Evaluate the probability density at a point
  virtual T prob_density(const vector& pt) const = 0;

  //: Evaluate the log probability density at a point
  virtual T log_prob_density(const vector& pt) const
  {
    return vcl_log(prob_density(pt));
  };

  //: The normalization constant for the density
  // When density() is multiplied by this value it becomes prob_density
  // norm_const() is reciprocal of the integral of density over the entire field
  virtual T norm_const() const = 0;

  //: Evaluate the cumulative distribution function at a point
  // This is the integral of the density function from negative infinity
  // (in all dimensions) to the point in question
  // \note It is not possible to compute this value for all functions in
  //       closed form.  In some cases, numerical integration may be used.
  //       If no good solutions exists the function should return a quiet NaN.
  virtual T cumulative_prob(const vector& pt) const = 0;

  //: Compute the inverse of the cumulative_prob() function
  // The value of x: P(x'<x) = P for x' drawn from the distribution.
  // \note This is only valid for univariate distributions
  //       multivariate distributions will return a quiet NaN
  virtual vector inverse_cdf(const T& p) const;

  //: The probability of being in an axis-aligned box
  // The box is defined by two points, the minimum and maximum.
  // Implemented in terms of \c cumulative_prob() by default.
  virtual T box_prob(const vector& min_pt, const vector& max_pt) const;

  //: Compute the mean of the distribution.
  // This may be trivial for distributions like Gaussians,
  // but actually involves computation for others.
  virtual void compute_mean(vector& mean) const = 0;

  //: Compute the covariance of the distribution.
  // This may be trivial for distributions like Gaussians,
  // but actually involves computation for others.
  virtual void compute_covar(matrix& covar) const = 0;
};


//: Default implementation of numerical CDF inverse computation.
// This function is called by the virtual function inverse_cdf() by default
// in the univariate case.
template <class T>
T vpdl_compute_inverse_cdf(const vpdl_distribution<T,1>& dist, double p);


#endif // vpdl_distribution_h_

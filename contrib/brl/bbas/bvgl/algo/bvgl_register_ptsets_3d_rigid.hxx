#ifndef bvgl_register_ptsets_3d_rigid_hxx_
#define bvgl_register_ptsets_3d_rigid_hxx_

#include "bvgl_register_ptsets_3d_rigid.h"
#include <limits>
#include <fstream>
#include <algorithm>
#include <vgl/vgl_distance.h>
#include <vgl/algo/vgl_fit_xy_paraboloid_3d.h>
#include <vnl/vnl_random.h>
#include <vnl/vnl_matrix.h>
#include <vnl/algo/vnl_svd.h>

template <class T>
T bvgl_register_ptsets_3d_rigid<T>::error(vgl_vector_3d<T> const& t)
{
  size_t n = frac_trans_.npts();
  T error = T(0);
  T cnt = T(0);
  for (size_t i = 0; i<n; ++i) {
    const vgl_point_3d<T>& p = frac_trans_.p(i);
    vgl_point_3d<T> tp(p.x()+t.x(), p.y()+t.y(), p.z()+t.z());
    vgl_point_3d<T> cp;
    if (!knn_fixed_.closest_point(tp, cp)) {
      std::cout << "KNN index failed to find neighbors" << std::endl;
      return std::numeric_limits<T>::max();
    }
    T d = vgl_distance<T>(tp, cp);
    if (d > outlier_thresh_)
      continue;
    cnt += T(1);
    error += d*d;
  }
  error /= cnt;
  return sqrt(error);
}
template <class T>
vgl_vector_3d<T>  bvgl_register_ptsets_3d_rigid<T>::mean_error(vgl_vector_3d<T> const& t)
{
  size_t n = frac_trans_.npts();
  vgl_vector_3d<T> ret(T(0),T(0),T(0));
  T cnt = T(0);
  for (size_t i = 0; i<n; ++i) {
    const vgl_point_3d<T>& p = frac_trans_.p(i);
    vgl_point_3d<T> tp(p.x()+t.x(), p.y()+t.y(), p.z()+t.z());
    vgl_point_3d<T> cp;
    if (!knn_fixed_.closest_point(tp, cp)) {
      std::cout << "KNN index failed to find neighbors" << std::endl;
      return ret;
    }
    T d = vgl_distance<T>(tp, cp);
    if (d > outlier_thresh_)
      continue;
    ret += (cp - tp);
    cnt += T(1);
  }
  ret /= cnt;
  return ret;
}

// distr_error is defined as below. Ideally the distance distribution reaches 100% of the population
// in zero distance to the nearest points in the fixed population
//     |
//     |             /--------------
//     |<--d.75-->  /
//     |           /
//frac |<--d.5--> /     error = (d.75 + d.5 + d.25)/3
// pop.|         /
//     |<-d.25->/
//     |       /
//     |-------
//     ========================
//     distance to nearest point
//
template <class T>
T bvgl_register_ptsets_3d_rigid<T>::distr_error(vgl_vector_3d<T> const& t){
  std::vector<T> dists;
  size_t n = frac_trans_.npts();
  for (size_t i = 0; i<n; ++i) {
    const vgl_point_3d<T>& p = frac_trans_.p(i);
    vgl_point_3d<T> tp(p.x()+t.x(), p.y()+t.y(), p.z()+t.z());
    vgl_point_3d<T> cp;
    if (!knn_fixed_.closest_point(tp, cp)) {
      std::cout << "KNN index failed to find neighbors" << std::endl;
      return std::numeric_limits<T>::max();
    }
    T d = vgl_distance<T>(tp, cp);
    dists.push_back(d);
  }
  size_t nd = dists.size();
  std::sort(dists.begin(), dists.end(), std::less<T>());
  return (dists[nd/2] + dists[nd/4] + dists[(3*nd)/4])/T(3);
}

template <class T>
bool bvgl_register_ptsets_3d_rigid<T>::minimize_exhaustive()
{
  if (fixed_.npts() == 0 || frac_trans_.npts() == 0) {
    std::cerr << "No points to minimize" << std::endl;
    return false;
  }
  T min_z = -t_range_.z(), max_z = t_range_.z();
  T min_y = -t_range_.y(), max_y = t_range_.y();
  T min_x = -t_range_.x(), max_x = t_range_.x();
  min_exhaustive_error_ = std::numeric_limits<T>::max();

  // initialize progress display
  int nx = (max_x-min_x)/t_inc_.x();
  reg_progress rp(nx);

  T z_at_min = T(0), y_at_min = T(0), x_at_min = T(0);
  for (T z = min_z; z <= max_z; z += t_inc_.z()) {
    for (T y = min_y; y <= max_y; y += t_inc_.y()) {
      for (T x = min_x; x <= max_x; x += t_inc_.x()) {
        rp();//update progress display
        T err = distr_error(vgl_vector_3d<T>(x, y, z));
        if (err < min_exhaustive_error_) {
          min_exhaustive_error_ = err;
          z_at_min = z;
          y_at_min = y;
          x_at_min = x;
        }
      }
    }
  }
  std::cout << std::endl;
  if (min_exhaustive_error_ >= outlier_thresh_) {
    exhaustive_t_ = vgl_vector_3d<T>(T(0), T(0), T(0));
    return false;
  }
  exhaustive_t_ = vgl_vector_3d<T>(x_at_min, y_at_min, z_at_min);
  return true;
}

template <class T>
bool bvgl_register_ptsets_3d_rigid<T>::minimize_ransac(vgl_vector_3d<T> const& initial_t){
  //debug
  std::cout << "frac " << transform_fraction_ << " frac_trans size " << frac_trans_.size() << " n hypos " << n_hypos_ << std::endl;
  // select a random point from the test set
  size_t n = frac_trans_.size();
  vnl_random rand;
  T min_error = std::numeric_limits<T>::max();
  best_ransac_t_ = initial_t;
  for(size_t i = 0; i<n_hypos_; ++i){
    size_t k = rand(n);
    const vgl_point_3d<T>& p = frac_trans_.p(k);
    vgl_point_3d<T> tp = p + best_ransac_t_;
    vgl_point_3d<T> cp;
    if (!knn_fixed_.closest_point(tp, cp)) {
      std::cout << "KNN index failed to find neighbors" << std::endl;
      return false;
    }
    vgl_vector_3d<T> t = cp-tp;
    vgl_vector_3d<T> tt = t+best_ransac_t_;
    T er = distr_error(tt);
    if(er < min_error){
      min_error = er;
      best_ransac_t_ = tt;
      std::cout << min_error << ' ' << best_ransac_t_.z() << std::endl;
    }
  }
  min_ransac_error_ = min_error;
  return true;
}
// the grid search is in x-y only. tz = initial_t.z()
template <class T>
bool bvgl_register_ptsets_3d_rigid<T>::minimize_analytic(vgl_vector_3d<T> const& initial_t){
  if (fixed_.npts() == 0 || frac_trans_.npts() == 0) {
    std::cerr << "No points to minimize" << std::endl;
    return false;
  }
  T min_x = initial_t.x()-t_range_.x(), max_x = initial_t.x()+t_range_.x();
  T min_y = initial_t.y()-t_range_.y(), max_y = initial_t.y()+t_range_.y();
  T z = initial_t.z();

  // initialize progress display
  int nx = (max_x-min_x)/t_inc_.x();
  reg_progress rp(nx);

  std::vector<vgl_point_3d<T> > grid_pts;
  for (T y = min_y; y <= max_y; y += t_inc_.y()) {
    for (T x = min_x; x <= max_x; x += t_inc_.x()) {
      rp();//update progress display
      T err = distr_error(vgl_vector_3d<T>(x, y, z));
      grid_pts.emplace_back(x, y, err);
    }
  }
  std::cout << std::endl; // end progress display

  vgl_fit_xy_paraboloid_3d<T> xyp(grid_pts);
  if(!xyp.fit_linear(&std::cout))
    return false;
  std::string qtype = xyp.quadric_type();
  if(qtype != "elliptic_paraboloid"){
    std::cout << "incorrect quadric class - not an elliptic_paraboloid" << std::endl;
    return false;
  }
  vgl_point_2d<T> min_xy = xyp.extremum_point();
  analytic_t_ = vgl_vector_3d<T>(min_xy.x(), min_xy.y(), z);
  min_analytic_error_ = distr_error(analytic_t_);
  return true;
}
template <class T>
bool bvgl_register_ptsets_3d_rigid<T>::minimize_mean_z_error(){
  vgl_vector_3d<T> initial_t = analytic_t_;
  std::vector<std::pair<T, T> > tz_error_vals;
  T min_tz = initial_t.z() - 0.3, max_tz = initial_t.z() + 0.31;
  std::cout << "refine tz plot " << std::endl;
  for(T tz = min_tz; tz<=max_tz; tz += T(0.05)){
    vgl_vector_3d<T> t(initial_t.x(), initial_t.y(), tz);
    T zerr = mean_error(t).z();
    tz_error_vals.emplace_back(tz, zerr);
  }
  //least squares 1d linear fit to mean z error data
  double  Sz2 = 0.0, Sz = 0.0;
  double  Sez = 0.0, Se = 0.0, N = 0.0;
  for(size_t i = 0; i<tz_error_vals.size(); ++i){
    double z = tz_error_vals[i].first, e = tz_error_vals[i].second;
    Sz2 += z*z; Sz += z;
    Sez += e*z; Se += e; N+=1.0;
  }
  Sz2 /= N; Sz /= N;
  Sez /= N; Se /= N;
  
  vnl_matrix<double> M(2,2);
  M[0][0] = Sz2; M[0][1] = Sz;
  M[1][0] = Sz; M[1][1] = 1;
  vnl_vector<double> b(2), coef(2);
   b[0] = Sez; b[1] = Se;
  vnl_svd<double> svd(M);
  size_t rank = svd.rank();
  if(rank != 2){
    std::cout << "Singular min z error solution" << std::endl;
    return false;
  }
  coef = svd.solve(b);
  double tz_min = -coef[1]/coef[0];
  analytic_t_.set(analytic_t_.x(), analytic_t_.y(), tz_min);
  min_analytic_error_ = distr_error(analytic_t_);
  return true;
}

template <class T>
bool bvgl_register_ptsets_3d_rigid<T>::read_fixed_ptset(std::string const& fixed_path)
{
  fixed_.clear();
  std::ifstream istr(fixed_path);
  if (!istr) {
    std::cerr << "Can't load fixed pointset from " << fixed_path << std::endl;
    return false;
  }
  istr >> fixed_;
  knn_fixed_.set_pointset(fixed_);
  return true;
}

template <class T>
bool bvgl_register_ptsets_3d_rigid<T>::read_movable_ptset(std::string const& movable_path)
{
  movable_.clear();
  std::ifstream istr(movable_path);
  if (!istr) {
    std::cerr << "Can't load transformed pointset from " << movable_path << std::endl;
    return false;
  }
  istr >> movable_;
  unsigned n = movable_.npts();
  size_t nf = static_cast<size_t>(transform_fraction_*n);
  if (nf<min_n_pts_)
    nf = min_n_pts_;
  if (nf > n)
    nf = n;
  vnl_random rand;
  for (size_t i = 0; i<nf; ++i) {
    size_t k = static_cast<size_t>(rand(n));
    const vgl_point_3d<T>& p = movable_.p(k);
    frac_trans_.add_point(p);
  }
  return true;
}

template <class T>
bool bvgl_register_ptsets_3d_rigid<T>::save_transformed_ptset(std::string const& path)
{
  std::ofstream ostr(path);
  if (!ostr) {
    std::cerr << "Can't save transformed pointset to " << path << std::endl;
    return false;
  }
  size_t n = movable_.npts();
  vgl_pointset_3d<T> temp;
  for (size_t i = 0; i<n; ++i) {
    const vgl_point_3d<T>& p = movable_.p(i);
    vgl_point_3d<T> pt = p + best_ransac_t_;
    temp.add_point(pt);
  }
  ostr << temp;
  ostr.close();
  return true;
}

#define BVGL_REGISTER_PTSETS_3D_RIGID_INSTANTIATE(T)    \
template class bvgl_register_ptsets_3d_rigid<T>

#endif //bvgl_register_ptsets_3d_rigid_hxx_

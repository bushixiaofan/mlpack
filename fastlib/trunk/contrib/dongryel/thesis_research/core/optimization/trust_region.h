/** @file trust_region.h
 *
 *  @author Dongryeol Lee (dongryel@cc.gatech.edu)
 */

#ifndef CORE_OPTIMIZATION_TRUST_REGION_H
#define CORE_OPTIMIZATION_TRUST_REGION_H

#include <iostream>
#include <algorithm>
#include <armadillo>

namespace core {
namespace optimization {
class TrustRegionSearchMethod {
  public:
    enum SearchType {CAUCHY, DOGLEG, STEIHAUG};
};

template<typename FunctionType>
class TrustRegion {
  private:
    double max_radius_;

    FunctionType *function_;

    core::optimization::TrustRegionSearchMethod::SearchType search_method_;

  private:

    bool GradientNormTooSmall_(const arma::vec &gradient) const;

    double Evaluate_(const arma::vec &iterate) const;

    void ObtainStepDirection_(
      double radius, const arma::vec &gradient,
      const arma::mat &hessian, arma::vec *step_direction,
      double *step_direction_norm);

    void ComputeCauchyPoint_(
      double radius, const arma::vec &gradient,
      const arma::mat &hessian, arma::vec *p);

    void ComputeDoglegDirection_(
      double radius, const arma::vec &gradient, const arma::mat &hessian,
      arma::vec *p);

    void ComputeSteihaugDirection_(
      double radius, const arma::vec &gradient, const arma::mat &hessian,
      arma::vec *p);

    void TrustRadiusUpdate_(
      double rho, double p_norm, double *current_radius);

    double ReductionRatio_(
      const arma::vec &iterate, const arma::vec &step,
      const arma::vec &gradient, const arma::mat &hessian) const;

  public:

    TrustRegion();

    double get_max_radius() const {
      return max_radius_;
    }

    void set_max_radius(double max_radius_in) {
      max_radius_ = max_radius_in;
    }

    void Init(
      FunctionType &function_in,
      core::optimization::TrustRegionSearchMethod::SearchType search_method_in);

    void Optimize(int num_iterations, arma::vec *iterate);
};
};
};

#endif

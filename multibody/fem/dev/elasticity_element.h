#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "drake/common/default_scalars.h"
#include "drake/common/eigen_types.h"
#include "drake/multibody/fem/dev/constitutive_model.h"
#include "drake/multibody/fem/dev/elasticity_element_base.h"
#include "drake/multibody/fem/dev/fem_element.h"
#include "drake/multibody/fem/dev/fem_state.h"
#include "drake/multibody/fem/dev/isoparametric_element.h"
#include "drake/multibody/fem/dev/linear_simplex_element.h"
#include "drake/multibody/fem/dev/quadrature.h"

namespace drake {
namespace multibody {
namespace fem {
/** The FEM element class for static and dynamic 3D elasticity problems.
 Implements the abstract interface of FemElement.

 See ElasticityElementCacheEntry for the corresponding ElementCacheEntry for
 %ElasticityElement.
 @tparam_nonsymbolic_scalar T.
 @tparam IsoparametricElementType The type of IsoparametricElement used in this
 %ElasticityElement. IsoparametricElementType must be a derived class from
 IsoparametricElement.
 @tparam QuadratureType The type of Quadrature used in this %ElasticityElement.
 QuadratureType must be a derived class from Quadrature.
 */
/* TODO(xuchenhan-tri): Consider making num_quadrature_points() and num_nodes()
 available at compile time and thereby eliminating heap allocations in this
 class. */
/* TODO(xuchenhan-tri): Consider abstracting out the IsoparametricElement and
 the Quadrature to a FixedSizeFemElement class, see issue #14302. */
template <typename T, class IsoparametricElementType, class QuadratureType>
class ElasticityElement : public ElasticityElementBase<T> {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(ElasticityElement);

  /** Constructs a new FEM elasticity element.
   @param[in] element_index The global index of the new element.
   @param[in] node_indices The global node indices of the nodes of this
   element.
   @param[in] density The mass density of the element in the reference
   configuration, with unit kg/m³.
   @param[in] constitutive_model The ConstitutiveModel to be used for this
   element.
   @param[in] reference_positions The positions of the nodes of this element in
   the reference configuration.
   @pre element_index must be valid.
   @pre node_indices.size() must be equal to
   IsoparametricElementType::num_nodes().
   @pre node_indices.size() must be equal to reference_positions.cols().
   @warning The input `constitutive_model` must be compatible with the
   DeformationGradientCacheEntry in the ElasticityElementCacheEntry that shares
   the same element index with this %ElasticityElement. More specifically, if
   the DeformationGradientCacheEntry in the corresponding
   ElasticityElementCacheEntry is of type "FooModelCacheEntry", then the input
   `constitutive_model` must be of type "FooModel". */
  ElasticityElement(ElementIndex element_index,
                    const std::vector<NodeIndex>& node_indices,
                    const T& density,
                    std::unique_ptr<ConstitutiveModel<T>> constitutive_model,
                    const Matrix3X<T>& reference_positions);

  virtual ~ElasticityElement() = default;

  /** Creates an ElasticityElementCacheEntry that is compatible with this
   element. */
  std::unique_ptr<ElementCacheEntry<T>> MakeElementCacheEntry() const final;

  /** Number of quadrature points at which element-wise quantities are
   evaluated. */
  int num_quadrature_points() const final { return quadrature_.num_points(); }

  /** Number of nodes associated with this element. */
  int num_nodes() const final { return shape_.num_nodes(); }

  /** Returns the elastic potential energy stored in this element in unit J. */
  T CalcElasticEnergy(const FemState<T>& state) const final;

 private:
  static constexpr int kNaturalDim = IsoparametricElementType::kNaturalDim;
  using MatrixD3 = Eigen::Matrix<T, kNaturalDim, 3>;

  friend class ElasticityElementTest;

  /* Calculates the element residual of this element evaluated at the input
   state.
   @param[in] state The FEM state at which to evaluate the residual.
   @returns a vector of residual of size `3 * num_nodes()`. The vector is
   ordered such that `3*i`-th to `3*i+2`-th entries of the vector stores the
   residual corresponding to the i-th node in this element. */
  void DoCalcResidual(const FemState<T>& state,
                      EigenPtr<VectorX<T>> residual) const final;

  /* Calculates the elastic forces on the nodes in this element. Returns a
   vector of elastic forces of size 3*num_nodes(). */
  void CalcNegativeElasticForce(const FemState<T>& state,
                                EigenPtr<VectorX<T>> force) const;

  /* Calculates the deformation gradient at all quadrature points in the this
   element. */
  void CalcDeformationGradient(const FemState<T>& state,
                               std::vector<Matrix3<T>>* F) const;

  /* Evaluates the DeformationGradientCacheEntry for this element. */
  /* TODO(xuchenhan-tri): This method unconditionally recomputes the
   DeformationGradientCacheEntry. Enable caching when caching is in place. */
  const DeformationGradientCacheEntry<T>& EvalDeformationGradientCacheEntry(
      const FemState<T>& state) const;

  /* Calculates the elastic energy density per unit reference volume, in unit
   J/m³, at each quadrature point in this element. */
  void CalcElasticEnergyDensity(const FemState<T>& state,
                                std::vector<T>* Psi) const;

  /* Calculates the first Piola stress, in unit Pa, at each quadrature point in
   this element. */
  void CalcFirstPiolaStress(const FemState<T>& state,
                            std::vector<Matrix3<T>>* P) const;

  /* The quadrature rule used for this element. */
  QuadratureType quadrature_;
  /* The isoparametric shape function used for this element. */
  IsoparametricElementType shape_{quadrature_.get_points()};
  /* The mass density of the element in the reference configuration with unit
   kg/m³. */
  T density_;
  /* The constitutive model that describes the stress-strain relationship for
   this element. */
  std::unique_ptr<ConstitutiveModel<T>> constitutive_model_;
  /* The inverse element Jacobian evaluated at reference configuration at the
   quadrature points in this element. */
  std::vector<MatrixD3> dxidX_;
  /* The positions of the nodes of this element in the reference configuration.
   */
  Matrix3X<T> reference_positions_;
  /* The volume evaluated at reference configuration occupied by the quadrature
   points in this element. To integrate a function f over the reference domain,
   sum f(q)*reference_volume_[q] over all the quadrature points q in the
   element. */
  std::vector<T> reference_volume_;
};
extern template class ElasticityElement<
    double, LinearSimplexElement<double, 3>,
    SimplexGaussianQuadrature<double, 1, 3>>;
extern template class ElasticityElement<
    AutoDiffXd, LinearSimplexElement<AutoDiffXd, 3>,
    SimplexGaussianQuadrature<AutoDiffXd, 1, 3>>;
extern template class ElasticityElement<
    double, LinearSimplexElement<double, 3>,
    SimplexGaussianQuadrature<double, 2, 3>>;
extern template class ElasticityElement<
    AutoDiffXd, LinearSimplexElement<AutoDiffXd, 3>,
    SimplexGaussianQuadrature<AutoDiffXd, 2, 3>>;
extern template class ElasticityElement<
    double, LinearSimplexElement<double, 3>,
    SimplexGaussianQuadrature<double, 3, 3>>;
extern template class ElasticityElement<
    AutoDiffXd, LinearSimplexElement<AutoDiffXd, 3>,
    SimplexGaussianQuadrature<AutoDiffXd, 3, 3>>;
}  // namespace fem
}  // namespace multibody
}  // namespace drake

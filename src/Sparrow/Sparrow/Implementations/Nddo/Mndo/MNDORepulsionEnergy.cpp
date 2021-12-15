/**
 * @file
 * @copyright This code is licensed under the 3-clause BSD license.\n
 *            Copyright ETH Zurich, Laboratory of Physical Chemistry, Reiher Group.\n
 *            See LICENSE.txt for details.
 */

#include "MNDORepulsionEnergy.h"
#include <Sparrow/Implementations/Nddo/Utils/ParameterUtils/ElementParameters.h>
#include <Utils/Math/AtomicSecondDerivativeCollection.h>

namespace Scine {
namespace Sparrow {

using namespace Utils::AutomaticDifferentiation;

namespace nddo {

MNDORepulsionEnergy::MNDORepulsionEnergy(const Utils::ElementTypeCollection& elements,
                                         const Utils::PositionCollection& positions, const ElementParameters& elementParameters)
  : RepulsionCalculator(elements, positions), elementParameters_(elementParameters) {
}

MNDORepulsionEnergy::~MNDORepulsionEnergy() = default;

void MNDORepulsionEnergy::initialize() {
  nAtoms_ = elements_.size();

  // Create 2D-vector of empty uni
  rep_ = Container(nAtoms_);
  for (int i = 0; i < nAtoms_; ++i)
    rep_[i] = std::vector<pairRepulsion_t>(nAtoms_);

  for (int i = 0; i < nAtoms_; i++) {
    for (int j = i + 1; j < nAtoms_; j++) {
      initializePair(i, j);
    }
  }
}

void MNDORepulsionEnergy::initializePair(int i, int j) {
  Utils::ElementType e1 = elements_[i];
  Utils::ElementType e2 = elements_[j];
  const auto& p1 = elementParameters_.get(e1);
  const auto& p2 = elementParameters_.get(e2);

  rep_[i][j] = std::make_unique<MNDOPairwiseRepulsion>(p1, p2);
}

void MNDORepulsionEnergy::calculateRepulsion(Utils::DerivativeOrder order) {
#pragma omp parallel for
  for (int i = 0; i < nAtoms_; i++) {
    for (int j = i + 1; j < nAtoms_; j++) {
      calculatePairRepulsion(i, j, order);
    }
  }
}

void MNDORepulsionEnergy::calculatePairRepulsion(int i, int j, Utils::DerivativeOrder order) {
  auto pA = positions_.row(i);
  auto pB = positions_.row(j);
  Eigen::Vector3d Rab = pB - pA;

  rep_[i][j]->calculate(Rab, order);
}

double MNDORepulsionEnergy::getRepulsionEnergy() const {
  double repulsionEnergy = 0;
#pragma omp parallel for reduction(+ : repulsionEnergy)
  for (int i = 0; i < nAtoms_; i++) {
    for (int j = i + 1; j < nAtoms_; j++) {
      repulsionEnergy += rep_[i][j]->getRepulsionEnergy();
    }
  }
  return repulsionEnergy;
}

void MNDORepulsionEnergy::addRepulsionDerivatives(DerivativeContainerType<Utils::Derivative::First>& derivatives) const {
  addRepulsionDerivativesImpl<Utils::Derivative::First>(derivatives);
}

void MNDORepulsionEnergy::addRepulsionDerivatives(DerivativeContainerType<Utils::Derivative::SecondAtomic>& derivatives) const {
  addRepulsionDerivativesImpl<Utils::Derivative::SecondAtomic>(derivatives);
}

void MNDORepulsionEnergy::addRepulsionDerivatives(DerivativeContainerType<Utils::Derivative::SecondFull>& derivatives) const {
  addRepulsionDerivativesImpl<Utils::Derivative::SecondFull>(derivatives);
}

template<Utils::Derivative O>
void MNDORepulsionEnergy::addRepulsionDerivativesImpl(DerivativeContainerType<O>& derivatives) const {
  for (int i = 0; i < nAtoms_; ++i) {
    for (int j = i + 1; j < nAtoms_; ++j) {
      auto dRep = rep_[i][j]->getDerivative<O>();
      addDerivativeToContainer<O>(derivatives, i, j, dRep);
    }
  }
}

} // namespace nddo
} // namespace Sparrow
} // namespace Scine

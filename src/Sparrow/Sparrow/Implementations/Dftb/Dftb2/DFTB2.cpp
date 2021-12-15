/**
 * @file
 * @copyright This code is licensed under the 3-clause BSD license.\n
 *            Copyright ETH Zurich, Laboratory of Physical Chemistry, Reiher Group.\n
 *            See LICENSE.txt for details.
 */

#include "DFTB2.h"
#include "Sparrow/Implementations/Dftb/Utils/DFTBCommon.h"
#include "Sparrow/Implementations/Dftb/Utils/DensityGuess.h"
#include "Sparrow/Implementations/Dftb/Utils/Overlap.h"
#include "Sparrow/Implementations/Dftb/Utils/Repulsion.h"
#include "Sparrow/Implementations/Dftb/Utils/SecondOrderFock.h"
#include "Sparrow/Implementations/Dftb/Utils/ZeroOrderMatricesCalculator.h"
#include <Utils/Scf/LcaoUtils/LcaoUtils.h>

namespace Scine {
namespace Sparrow {

using namespace Utils::AutomaticDifferentiation;

namespace dftb {

DFTB2::DFTB2() : ScfMethod(true, Utils::DerivativeOrder::Two), atomParameters(110) {
  dftbBase = std::make_shared<DFTBCommon>(elementTypes_, nElectrons_, molecularCharge_, atomParameters, pairParameters);
  matricesCalculator_ = std::make_unique<dftb::ZeroOrderMatricesCalculator>(elementTypes_, positions_, aoIndexes_,
                                                                            atomParameters, pairParameters, densityMatrix_);
  overlapCalculator_ = std::make_unique<dftb::Overlap>(*matricesCalculator_);
  electronicPart_ = std::make_unique<dftb::SecondOrderFock>(*matricesCalculator_, elementTypes_, positions_,
                                                            atomParameters, pairParameters, densityMatrix_,
                                                            energyWeightedDensityMatrix_, atomicCharges_, coreCharges_,
                                                            aoIndexes_, overlapMatrix_, unrestrictedCalculationRunning_);
  rep_ = std::make_unique<dftb::Repulsion>(elementTypes_, positions_, dftbBase->getPairParameters());
  densityMatrixGuess_ = std::make_unique<dftb::DensityGuess>(aoIndexes_, coreCharges_, nElectrons_);

  initializer_ = dftbBase;
}

DFTB2::~DFTB2() = default;

void DFTB2::initializeFromParameterPath(const std::string& path) {
  dftbBase->setMethodDetails(path, 2);
  ScfMethod::initialize();
}

std::shared_ptr<DFTBCommon> DFTB2::getInitializer() const {
  return dftbBase;
}

Eigen::MatrixXd DFTB2::calculateGammaMatrix() const {
  auto secondOrderFock = std::dynamic_pointer_cast<SecondOrderFock>(electronicPart_);
  Eigen::MatrixXd gammaMatrix(elementTypes_.size(), elementTypes_.size());
  for (int atom = 0; atom < static_cast<int>(elementTypes_.size()); ++atom) {
    for (int atom_b = atom; atom_b < static_cast<int>(elementTypes_.size()); ++atom_b) {
      gammaMatrix(atom, atom_b) = secondOrderFock->gamma<Utils::DerivativeOrder::Zero>(atom, atom_b);
    }
  }
  return gammaMatrix.selfadjointView<Eigen::Upper>();
}

std::shared_ptr<Eigen::VectorXd> DFTB2::calculateSpinConstantVector() const {
  try {
    Eigen::VectorXd spinConstantVector(elementTypes_.size());
    for (int atom = 0; atom < static_cast<int>(elementTypes_.size()); ++atom) {
      spinConstantVector(atom) = atomParameters[Utils::ElementInfo::Z(elementTypes_[atom])]->getAtomicResolvedSpinConstant();
    }
    return std::make_shared<Eigen::VectorXd>(std::move(spinConstantVector));
  }
  catch (...) {
    return nullptr;
  }
}
} // namespace dftb
} // namespace Sparrow
} // namespace Scine

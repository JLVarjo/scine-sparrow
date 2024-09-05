/**
 * @file
 * @copyright This code is licensed under the 3-clause BSD license.\n
 *            Copyright ETH Zurich, Department of Chemistry and Applied Biosciences, Reiher Group.\n
 *            See LICENSE.txt for details.
 */

#include "DFTB0.h"
#include "Sparrow/Implementations/Dftb/Utils/Overlap.h"
#include "Sparrow/Implementations/Dftb/Utils/Repulsion.h"
#include "Sparrow/Implementations/Dftb/Utils/SKAtom.h"
#include "Sparrow/Implementations/Dftb/Utils/SKPair.h"
#include "Sparrow/Implementations/Dftb/Utils/ZeroOrderFock.h"
#include "Sparrow/Implementations/Dftb/Utils/ZeroOrderMatricesCalculator.h"
#include <Utils/Scf/LcaoUtils/LcaoUtils.h>

namespace Scine {
namespace Sparrow {

using namespace Utils::AutomaticDifferentiation;

namespace dftb {

DFTB0::DFTB0() : LcaoMethod(false, Utils::DerivativeOrder::Two), atomParameters(110) {
  dftbBase = std::make_shared<DFTBCommon>(elementTypes_, nElectrons_, molecularCharge_, atomParameters, pairParameters);
  matricesCalculator_ = std::make_unique<dftb::ZeroOrderMatricesCalculator>(elementTypes_, positions_, aoIndexes_,
                                                                            atomParameters, pairParameters, densityMatrix_);
  overlapCalculator_ = std::make_unique<dftb::Overlap>(*matricesCalculator_);
  electronicPart_ = std::make_unique<dftb::ZeroOrderFock>(*matricesCalculator_, singleParticleEnergies_,
                                                          energyWeightedDensityMatrix_, nElectrons_);
  rep_ = std::make_unique<dftb::Repulsion>(elementTypes_, positions_, dftbBase->getPairParameters());

  initializer_ = dftbBase;
}

DFTB0::~DFTB0() = default;

void DFTB0::initializeFromParameterPath(const std::string& path) {
  dftbBase->setMethodDetails(path, 0);
  LcaoMethod::initialize();
}

std::shared_ptr<DFTBCommon> DFTB0::getInitializer() const {
  return dftbBase;
}

Eigen::MatrixXd DFTB0::calculateGammaMatrix() const {
  return Eigen::MatrixXd(0, 0);
}
std::shared_ptr<Eigen::VectorXd> DFTB0::calculateSpinConstantVector() const {
  return nullptr;
}
} // namespace dftb
} // namespace Sparrow
} // namespace Scine

/**
 * @file MNDOMethodWrapper.cpp
 * @copyright This code is licensed under the 3-clause BSD license.\n
 *            Copyright ETH Zurich, Laboratory of Physical Chemistry, Reiher Group.\n
 *            See LICENSE.txt for details.
 */

/* Internal Includes */
#include "MNDOMethodWrapper.h"
#include "MNDOSettings.h"
#include <Sparrow/Implementations/Nddo/Parameters.h>
#include <Sparrow/Implementations/Nddo/TimeDependent/LinearResponse/CISData.h>
#include <Sparrow/Implementations/Nddo/Utils/DipoleUtils/NDDODipoleMatrixCalculator.h>
#include <Sparrow/Implementations/Nddo/Utils/DipoleUtils/NDDODipoleMomentCalculator.h>
#include <Sparrow/Implementations/Nddo/Utils/NDDOInitializer.h>
#include <Sparrow/Implementations/Nddo/Utils/OneElectronMatrix.h>
#include <Sparrow/Implementations/Nddo/Utils/TwoElectronMatrix.h>
/* External Includes */
#include <Core/Exceptions.h>
#include <Utils/CalculatorBasics.h>
#include <Utils/Geometry/AtomCollection.h>
#include <Utils/IO/NativeFilenames.h>
#include <Utils/Scf/MethodExceptions.h>
#include <Utils/UniversalSettings/SettingsNames.h>
#include <memory>

namespace Scine {
namespace Sparrow {

MNDOMethodWrapper::MNDOMethodWrapper() {
  dipoleMatrixCalculator_ = NDDODipoleMatrixCalculator<nddo::MNDOMethod>::create(method_);
  dipoleCalculator_ = NDDODipoleMomentCalculator<nddo::MNDOMethod>::create(method_, *dipoleMatrixCalculator_);
  this->settings_ = std::make_unique<MNDOSettings>();
  requiredProperties_ = Utils::Property::Energy;
  applySettings();
}

MNDOMethodWrapper::MNDOMethodWrapper(const MNDOMethodWrapper& rhs) : MNDOMethodWrapper() {
  copyInto(*this, rhs);
}

MNDOMethodWrapper& MNDOMethodWrapper::operator=(const MNDOMethodWrapper& rhs) {
  copyInto(*this, rhs);
  return *this;
}

MNDOMethodWrapper::~MNDOMethodWrapper() = default;

void MNDOMethodWrapper::applySettings() {
  // Set whether the NDDO dipole approximation is used.
  auto useNDDOApprox = settings_->getBool(Utils::SettingsNames::NDDODipoleApproximation);
  auto& NDDODipoleCalculator = dynamic_cast<NDDODipoleMomentCalculator<nddo::MNDOMethod>&>(*dipoleCalculator_);
  NDDODipoleCalculator.useNDDOApproximation(useNDDOApprox);

  NDDOMethodWrapper::applySettings(settings_, method_);
}

std::string MNDOMethodWrapper::name() const {
  return "MNDO";
}

void MNDOMethodWrapper::initialize() {
  try {
    auto parameterFile = settings_->getString(Utils::SettingsNames::methodParameters);

    if (parameterFile.empty()) {
      method_.getInitializer().getRawParameters() = nddo::mndo();
    }
    else {
      method_.readParameters(parameterFile);
    }
    method_.initialize();
  }
  catch (Utils::Methods::InitializationException& e) {
    throw Core::InitializationException(e.what());
  }
}

Utils::LcaoMethod& MNDOMethodWrapper::getLcaoMethod() {
  return method_;
}

const Utils::LcaoMethod& MNDOMethodWrapper::getLcaoMethod() const {
  return method_;
}

void MNDOMethodWrapper::calculateImpl(Utils::Derivative requiredDerivative) {
  method_.calculate(requiredDerivative, getLog());
}

Eigen::MatrixXd MNDOMethodWrapper::getOneElectronMatrix() const {
  return method_.getOneElectronMatrix().getMatrix();
}

Utils::SpinAdaptedMatrix MNDOMethodWrapper::getTwoElectronMatrix() const {
  Utils::SpinAdaptedMatrix twoElectronMatrix;
  if (!method_.unrestrictedCalculationRunning()) {
    twoElectronMatrix = Utils::SpinAdaptedMatrix::createRestricted(method_.getTwoElectronMatrix().getMatrix());
  }
  else {
    twoElectronMatrix = Utils::SpinAdaptedMatrix::createUnrestricted(method_.getTwoElectronMatrix().getAlpha(),
                                                                     method_.getTwoElectronMatrix().getBeta());
  }
  return twoElectronMatrix;
}

Utils::DensityMatrix MNDOMethodWrapper::getDensityMatrixGuess() const {
  return method_.getDensityMatrixGuess();
}

CISData MNDOMethodWrapper::getCISDataImpl() const {
  return CISData::constructCISDataFromNDDOMethod(method_);
}

void MNDOMethodWrapper::addElectronicContribution(std::shared_ptr<Utils::AdditiveElectronicContribution> contribution) {
  method_.addElectronicContribution(std::move(contribution));
}

bool MNDOMethodWrapper::successfulCalculation() const {
  return method_.hasConverged();
}

} /* namespace Sparrow */
} /* namespace Scine */

/**
 * @file DFTB0MethodWrapper.h
 * @copyright This code is licensed under the 3-clause BSD license.\n
 *            Copyright ETH Zurich, Laboratory of Physical Chemistry, Reiher Group.\n
 *            See LICENSE.txt for details.
 */
#ifndef SPARROW_DFTB0METHODWRAPPER_H
#define SPARROW_DFTB0METHODWRAPPER_H

/* Internal Includes */
#include <Sparrow/Implementations/Dftb/DFTBMethodWrapper.h>
#include <Sparrow/Implementations/Dftb/Dftb0/DFTB0.h>
/* External Includes */
#include <string>

namespace Scine {

namespace Utils {
class AdditiveElectronicContribution;
} // namespace Utils

namespace Sparrow {
/**
 * @class DFTB0MethodWrapper DFTB0MethodWrapper.h
 * @brief A method wrapper handling DFTB0 calculations.
 */
class DFTB0MethodWrapper final : public Utils::CloneInterface<DFTB0MethodWrapper, DFTBMethodWrapper> {
 public:
  static constexpr const char* model = "DFTB0";

  /// @brief Default Constructor.
  DFTB0MethodWrapper();
  // Rule of 5
  DFTB0MethodWrapper(const DFTB0MethodWrapper& rhs);
  DFTB0MethodWrapper& operator=(const DFTB0MethodWrapper& rhs);
  DFTB0MethodWrapper(DFTB0MethodWrapper&& rhs) = delete;
  DFTB0MethodWrapper& operator=(DFTB0MethodWrapper&& rhs) = delete;
  ~DFTB0MethodWrapper() final;
  /**
   * @brief Getter for the name of the underlying method.
   * @returns Returns the name of the underlying method.
   */
  std::string name() const final;
  /**
   * @brief Function to apply the settings to the underlying method.
   */
  void applySettings() final;
  /**
   * @brief Function to add a contribution to the electronic DFTB0 Hamiltonian.
   * @param contribution An Utils::AdditiveElectronicContribution polymorphic class.
   */
  void addElectronicContribution(std::shared_ptr<Utils::AdditiveElectronicContribution> contribution) final;

 private:
  TDDFTBData getTDDFTBDataImpl() const final;
  bool successfulCalculation() const final;
  //! This function hides the templated generic function in @file DFTBMethodWrapper.h.
  void copyInto(DFTB0MethodWrapper& instance, const DFTB0MethodWrapper& classToCopy);
  Utils::DensityMatrix getDensityMatrixGuess() const final;
  //! Initializes a method with the parameter file present in the settings.
  void initialize() final;
  //! Calls the underlying method's calculate() function.
  void calculateImpl(Utils::Derivative requiredDerivative) final;
  //! Get the underlying method as a LCAO method.
  Utils::LcaoMethod& getLcaoMethod() final;
  const Utils::LcaoMethod& getLcaoMethod() const final;
  void loadState(std::shared_ptr<Core::State> state) final;
  dftb::DFTB0 method_;
};

} /* namespace Sparrow */
} /* namespace Scine */

#endif /* SPARROW_DFTB0METHODWRAPPER_H */

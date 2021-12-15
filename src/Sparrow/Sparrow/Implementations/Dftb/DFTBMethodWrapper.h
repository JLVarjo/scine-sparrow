/**
 * @file
 * @copyright This code is licensed under the 3-clause BSD license.\n
 *            Copyright ETH Zurich, Laboratory of Physical Chemistry, Reiher Group.\n
 *            See LICENSE.txt for details.
 */
#ifndef SPARROW_DFTBMETHODWRAPPER_H
#define SPARROW_DFTBMETHODWRAPPER_H

#include <Sparrow/Implementations/GenericMethodWrapper.h>
#include <Utils/UniversalSettings/SettingsNames.h>

namespace Scine {
namespace Sparrow {
class TDDFTBData;

class DFTBMethodWrapper : public Utils::CloneInterface<Utils::Abstract<DFTBMethodWrapper>, GenericMethodWrapper> {
 public:
  /**
   * @brief Constructor.
   * It plays with the befriended states handler by giving it a *this reference.
   */
  DFTBMethodWrapper();
  ~DFTBMethodWrapper() override;
  /**
   * @brief Get the list of the possible properties to calculate analytically.
   * Since it is the same for all DFTB, have it stated here.
   * If they diverge, override this in each method wrapper.
   */
  Utils::PropertyList possibleProperties() const final;
  TDDFTBData getTDDFTBData() const;

 private:
  void assembleResults(const std::string& description) final;

 protected:
  virtual TDDFTBData getTDDFTBDataImpl() const = 0;

  // Extracted method from all copy constructors and copy assignment operators.
  template<class DFTBMethod>
  void copyInto(DFTBMethod& instance, const DFTBMethod& classToCopy) {
    auto results = classToCopy.results();
    instance.settings() = classToCopy.settings();
    // Concurrent calling of the logger introduces race conditions
    // that eventually trigger a segfault
    instance.setStructure(*classToCopy.getStructure());
    instance.results() = std::move(results);
    instance.loadState(classToCopy.getState());
    instance.setLog(classToCopy.getLog());
  }
  bool getZPVEInclusion() const final;
};

} // namespace Sparrow
} // namespace Scine

#endif // SPARROW_DFTBMETHODWRAPPER_H

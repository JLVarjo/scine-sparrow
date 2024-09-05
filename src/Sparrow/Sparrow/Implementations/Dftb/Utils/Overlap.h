/**
 * @file
 * @copyright This code is licensed under the 3-clause BSD license.\n
 *            Copyright ETH Zurich, Department of Chemistry and Applied Biosciences, Reiher Group.\n
 *            See LICENSE.txt for details.
 */

#ifndef SPARROW_DFTB_OVERLAP_H
#define SPARROW_DFTB_OVERLAP_H

#include <Utils/Scf/MethodInterfaces/OverlapCalculator.h>

namespace Scine {

namespace Utils {
enum class DerivativeOrder;
}

namespace Sparrow {

namespace dftb {
class ZeroOrderMatricesCalculator;

class Overlap : public Utils::OverlapCalculator {
 public:
  explicit Overlap(ZeroOrderMatricesCalculator& matricesCalculator);

  void calculateOverlap(Utils::DerivativeOrder highestRequiredOrder) override;
  const Utils::MatrixWithDerivatives& getOverlap() const override;
  void reset() override;

 private:
  ZeroOrderMatricesCalculator& matricesCalculator_;
};

} // namespace dftb

} // namespace Sparrow
} // namespace Scine
#endif // SPARROW_DFTB_OVERLAP_H

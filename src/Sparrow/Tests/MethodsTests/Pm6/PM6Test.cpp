/**
 * @file
 * @copyright This code is licensed under the 3-clause BSD license.\n
 *            Copyright ETH Zurich, Department of Chemistry and Applied Biosciences, Reiher Group.\n
 *            See LICENSE.txt for details.
 */

#include "Utils/Scf/LcaoUtils/SpinMode.h"
#include <Core/Interfaces/Calculator.h>
#include <Core/Log.h>
#include <Core/ModuleManager.h>
#include <Sparrow/Implementations/Nddo/Pm6/PM6Method.h>
#include <Sparrow/Implementations/Nddo/Pm6/Wrapper/PM6MethodWrapper.h>
#include <Sparrow/Implementations/Nddo/Utils/OneElectronMatrix.h>
#include <Sparrow/Implementations/Nddo/Utils/ParameterUtils/ElementParameters.h>
#include <Sparrow/Implementations/OrbitalSteeringCalculator.h>
#include <Sparrow/Implementations/OrbitalSteeringSettings.h>
#include <Utils/Constants.h>
#include <Utils/Geometry/AtomCollection.h>
#include <Utils/IO/ChemicalFileFormats/XyzStreamHandler.h>
#include <Utils/Scf/ConvergenceAccelerators/ConvergenceAcceleratorFactory.h>
#include <Utils/Typenames.h>
#include <Utils/UniversalSettings/SettingsNames.h>
#include <gmock/gmock.h>
#include <omp.h>
#include <Eigen/Core>

namespace Scine {
namespace Sparrow {

using namespace testing;
using namespace nddo;

class APM6Calculation : public Test {
 public:
  PM6Method method;
  std::shared_ptr<PM6MethodWrapper> pm6MethodWrapper;
  std::shared_ptr<Core::Calculator> polymorphicMethodWrapper;
  ElementParameters elementParameters;

  Core::Log log;

  void SetUp() override {
    pm6MethodWrapper = std::make_shared<PM6MethodWrapper>();
    polymorphicMethodWrapper = std::make_shared<PM6MethodWrapper>();

    method.setMaxIterations(10000);
    method.setConvergenceCriteria({1e-7, 1e-8});
    pm6MethodWrapper->settings().modifyInt(Utils::SettingsNames::maxScfIterations, 10000);
    pm6MethodWrapper->settings().modifyDouble(Utils::SettingsNames::densityRmsdCriterion, 1e-8);
    polymorphicMethodWrapper->settings().modifyInt(Utils::SettingsNames::maxScfIterations, 10000);
    polymorphicMethodWrapper->settings().modifyDouble(Utils::SettingsNames::densityRmsdCriterion, 1e-8);
    log = Core::Log::silent();
    pm6MethodWrapper->setLog(Core::Log::silent());
    polymorphicMethodWrapper->setLog(Core::Log::silent());
  }
};
TEST_F(APM6Calculation, HasTheCorrectNumberOfAOsAfterInitialization) {
  std::stringstream ssH("4\n\n"
                        "C      0.0000000000    0.0000000000    0.0000000000\n"
                        "V      0.0529177211   -0.3175063264    0.2645886053\n"
                        "H     -0.5291772107    0.1058354421   -0.1587531632\n"
                        "H     -0.1058354421    0.1058354421   -0.1587531632\n");
  auto as = Utils::XyzStreamHandler::read(ssH);
  method.setMolecularCharge(1);
  method.setStructure(as);
  ASSERT_THAT(method.getNumberAtomicOrbitals(), Eq(15));
}

TEST_F(APM6Calculation, CanReturnAtomicGtos) {
  std::stringstream ssH("4\n\n"
                        "C      0.0000000000    0.0000000000    0.0000000000\n"
                        "P      0.0529177211   -0.3175063264    0.2645886053\n"
                        "H     -0.5291772107    0.1058354421   -0.1587531632\n"
                        "H     -0.1058354421    0.1058354421   -0.1587531632\n");
  auto as = Utils::XyzStreamHandler::read(ssH);
  method.setMolecularCharge(1);
  pm6MethodWrapper->settings().modifyInt(Utils::SettingsNames::molecularCharge, 1);
  method.setStructure(as);
  pm6MethodWrapper->setStructure(as);
  auto gtoMap = pm6MethodWrapper->getAtomicGtosMap();
  ASSERT_TRUE(gtoMap.find(6) != gtoMap.end());
  ASSERT_TRUE(gtoMap.find(15) != gtoMap.end());
  ASSERT_TRUE(gtoMap.find(1) != gtoMap.end());
  ASSERT_TRUE(gtoMap.find(8) == gtoMap.end());
  ASSERT_THAT(gtoMap.at(15).p->angularMomentum, Eq(1));
  ASSERT_THAT(gtoMap.at(6).s->nAOs(), Eq(1));
  ASSERT_THAT(gtoMap.at(6).p->nAOs(), Eq(3));
  ASSERT_THAT(gtoMap.at(1).s->gtfs.size(), Eq(6));
  ASSERT_THAT(gtoMap.at(1).s->gtfs.at(0).exponent, DoubleNear(3.718317372849182e+01, 1e-10));
  ASSERT_THAT(gtoMap.at(15).p->gtfs.at(2).coefficient, DoubleNear(1.584431977558960e-01, 1e-10));
}

TEST_F(APM6Calculation, GetsCorrectOneElectronPartOfFockForH) {
  std::stringstream ssH("1\n\n"
                        "H -1.0 0.2 -0.3\n");
  auto as = Utils::XyzStreamHandler::read(ssH);
  method.setSpinMultiplicity(2);
  method.setUnrestrictedCalculation(true);
  method.setStructure(as);

  method.calculateDensityIndependentQuantities();
  ASSERT_THAT(method.getOneElectronMatrix().getMatrix()(0, 0) * Utils::Constants::ev_per_hartree, DoubleNear(-11.246958, 1e-4));
}

TEST_F(APM6Calculation, GetsCorrectOneElectronPartOfFockForC) {
  std::stringstream ssC("1\n\n"
                        "C 0.0 0.0 0.0\n");
  auto as = Utils::XyzStreamHandler::read(ssC);
  method.setStructure(as);
  method.setScfMixer(Utils::scf_mixer_t::none);

  method.calculateDensityIndependentQuantities();
  Eigen::Matrix<double, 4, 4> expected;
  expected << -51.089653, 0, 0, 0, 0, -39.937920, 0, 0, 0, 0, -39.937920, 0, 0, 0, 0, -39.937920;
  ASSERT_TRUE(method.getOneElectronMatrix().getMatrix().isApprox(expected / Utils::Constants::ev_per_hartree, 1e-4));
}

TEST_F(APM6Calculation, GetsCorrectOneElectronPartOfFockForH2) {
  std::stringstream ssH2("2\n\n"
                         "H -0.529177  0.105835 -0.158753\n"
                         "H -0.105835  0.105835 -0.158753\n");
  auto as = Utils::XyzStreamHandler::read(ssH2);
  method.setStructure(as);

  method.calculateDensityIndependentQuantities();
  Eigen::Matrix<double, 2, 2> expected;
  expected << -24.545567, 0, -7.139417, -24.545567;
  ASSERT_TRUE(method.getOneElectronMatrix().getMatrix().isApprox(expected / Utils::Constants::ev_per_hartree, 1e-4));
}

TEST_F(APM6Calculation, GetsCorrectOneElectronPartOfFockForCC) {
  std::stringstream ssC2("2\n\n"
                         "C 0.0 0.0 0.0\n"
                         "C 0.423342  0.0529177 -0.0529177\n");
  auto as = Utils::XyzStreamHandler::read(ssC2);
  method.setStructure(as);

  method.calculateDensityIndependentQuantities();
  Eigen::Matrix<double, 8, 8> expected;
  expected << -100.648, 0, 0, 0, 0, 0, 0, 0, -3.979998, -85.5516, 0, 0, 0, 0, 0, 0, -0.4975, -0.180171, -84.1327, 0, 0,
      0, 0, 0, 0.4975, 0.180171, 0.0225214, -84.1327, 0, 0, 0, 0, -13.3368, -4.99404, -0.6242550, 0.624255, -100.648, 0,
      0, 0, 4.994039, -4.12903, 0.262602, -0.262602, 3.979998, -85.5516, 0, 0, 0.624255, 0.262602, -6.19702, -0.0328252,
      0.4975, -0.180171, -84.1327, 0, -0.624255, -0.262602, -0.0328252, -6.19702, -0.4975, 0.180171, 0.0225214, -84.1327;

  ASSERT_TRUE(method.getOneElectronMatrix().getMatrix().isApprox(expected / Utils::Constants::ev_per_hartree, 1e-3));
}

TEST_F(APM6Calculation, GetsCorrectOneElectronPartOfFockForCH2O) {
  std::stringstream ss("4\n\n"
                       "C      0.0000000000    0.0000000000    0.0000000000\n"
                       "O      0.0529177211   -0.3175063264    0.2645886053\n"
                       "H     -0.5291772107    0.1058354421   -0.1587531632\n"
                       "H     -0.1058354421    0.1058354421   -0.1587531632\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  method.setStructure(as);

  method.calculateDensityIndependentQuantities();
  Eigen::Matrix<double, 10, 10> expected;
  expected << -146.064, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.912815, -125.847, 0, 0, 0, 0, 0, 0, 0, 0, 3.16799, 0.293816,
      -126.189, 0, 0, 0, 0, 0, 0, 0, -2.27491, -0.339711, 0.835436, -125.976, 0, 0, 0, 0, 0, 0, -18.6187, -2.2623,
      13.5738, -11.3115, -159.169, 0, 0, 0, 0, 0, 0.900141, -10.9564, -0.454979, 0.379149, 1.96835, -131.661, 0, 0, 0, 0,
      -5.40085, -0.454979, -8.30233, -2.2749, -5.87582, 0.775143, -133.039, 0, 0, 0, 4.50071, 0.379149, -2.2749, -9.13645,
      5.17589, -0.740047, 1.80334, -132.653, 0, 0, -9.19958, 3.86377, -0.772754, 1.15913, -7.65641, 4.50843, -3.27886,
      3.27886, -134.632, 0, -11.2585, 1.04621, -1.04621, 1.56931, -11.9153, 1.79134, -4.7769, 4.7769, -7.13942, -145.648;

  ASSERT_TRUE(method.getOneElectronMatrix().getMatrix().isApprox(expected / Utils::Constants::ev_per_hartree, 1e-4));
}

TEST_F(APM6Calculation, GetsCorrectEigenvaluesForC) {
  std::stringstream ss("1\n\n"
                       "C     -0.1058354421    0.1058354421   -0.1587531632\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  method.setStructure(as);
  method.setScfMixer(Utils::scf_mixer_t::none);

  method.convergedCalculation(log);

  auto eigenvalues = method.getSingleParticleEnergies().getRestrictedEnergies();

  std::vector<double> expected = {-15.41519, -6.82065, 0.72739, 0.72739};
  for (unsigned i = 0; i < eigenvalues.size(); i++) {
    SCOPED_TRACE("... for the eigenvalue " + std::to_string(i) + ":");
    EXPECT_THAT(eigenvalues[i] * Utils::Constants::ev_per_hartree, DoubleNear(expected[i], 1e-4));
  }
}

TEST_F(APM6Calculation, GetsCorrectEigenvaluesForFe) {
  std::stringstream ss("1\n\n"
                       "Fe     -0.1058354421    0.1058354421   -0.1587531632\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  method.setStructure(as);
  method.setScfMixer(Utils::scf_mixer_t::none);

  Utils::DensityMatrix P;
  P.setDensity(Eigen::MatrixXd::Ones(9, 9) / 9.0 * 8.0, 8);
  method.setDensityMatrix(P);
  method.convergedCalculation(log);

  auto eigenvalues = method.getSingleParticleEnergies().getRestrictedEnergies();

  std::vector<double> expected = {-11.29641, -11.29509, -11.29456, -7.69782, 2.89602,
                                  2.89818,   4.21603,   4.21629,   4.21641};

  for (unsigned i = 0; i < eigenvalues.size(); i++) {
    SCOPED_TRACE("... for the eigenvalue " + std::to_string(i) + ":");
    EXPECT_THAT(eigenvalues[i] * Utils::Constants::ev_per_hartree, DoubleNear(expected[i], 1e-2));
  }
}

TEST_F(APM6Calculation, GetsCorrectUnrestrictedEigenvaluesForBoron) {
  std::stringstream ss("1\n\n"
                       "B     -0.1058354421    0.1058354421   -0.1587531632\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  method.setUnrestrictedCalculation(true);
  method.setSpinMultiplicity(2);
  method.setStructure(as);

  method.convergedCalculation(log, Utils::Derivative::None);

  auto alphaEigenvalues = method.getSingleParticleEnergies().getAlphaEnergies();
  auto betaEigenvalues = method.getSingleParticleEnergies().getBetaEnergies();

  // From Mopac
  double mopacEnergy = -49.53668; // eV
  std::vector<double> alphaExpected = {-11.74716, -5.78067, -0.09376, -0.09376};
  std::vector<double> betaExpected = {-10.49432, 0.62041, 0.62041, 2.04873};

  ASSERT_THAT(method.getEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(mopacEnergy, 1e-5));
  for (unsigned i = 0; i < alphaEigenvalues.size(); i++) {
    SCOPED_TRACE("... for the eigenvalues " + std::to_string(i) + ":");
    EXPECT_THAT(alphaEigenvalues[i] * Utils::Constants::ev_per_hartree, DoubleNear(alphaExpected[i], 1e-5));
    EXPECT_THAT(betaEigenvalues[i] * Utils::Constants::ev_per_hartree, DoubleNear(betaExpected[i], 1e-5));
  }
}

TEST_F(APM6Calculation, IsIndependentOfOrderOfAtoms) {
  std::stringstream ss("3\n\n"
                       "O      0.0000000000    0.0000000000    0.0000000000\n"
                       "H      0.4005871485    0.3100978455    0.0000000000\n"
                       "H     -0.4005871485    0.3100978455    0.0000000000\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  method.setStructure(as);
  method.convergedCalculation(log);
  double energy1 = method.getElectronicEnergy();

  std::stringstream ss2("3\n\n"
                        "H      0.4005871485    0.3100978455    0.0000000000\n"
                        "H     -0.4005871485    0.3100978455    0.0000000000\n"
                        "O      0.0000000000    0.0000000000    0.0000000000\n");
  auto as2 = Utils::XyzStreamHandler::read(ss2);
  method.setStructure(as2);

  method.convergedCalculation(log);
  double energy2 = method.getElectronicEnergy();

  ASSERT_THAT(energy1, DoubleNear(energy2, 1e-10));
}

TEST_F(APM6Calculation, GetsCorrectElectronicEnergyForDimethylZinc) {
  std::stringstream ss("9\n\n"
                       "C      1.8500000000    0.0000000000   -0.0000000000\n"
                       "Zn    -0.0000000000    0.0000000000   -0.0000000000\n"
                       "C     -1.8500000000    0.0000000000    0.0000000000\n"
                       "H      2.2133000000   -0.0000000000    1.0277000000\n"
                       "H      2.2133000000   -0.8900000000   -0.5138100000\n"
                       "H      2.2133000000    0.8900000000   -0.5138100000\n"
                       "H     -2.2133000000   -0.0000000000   -1.0277000000\n"
                       "H     -2.2133000000   -0.8900000000    0.5138100000\n"
                       "H     -2.2133000000    0.8900000000    0.5138100000\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  method.setStructure(as);

  method.convergedCalculation(log);
  ASSERT_THAT(method.getRepulsionEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(642.08271, 1e-3));
  ASSERT_THAT(method.getElectronicEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(-996.23478, 1e-3));
}

TEST_F(APM6Calculation, GetsCorrectElectronicEnergyForAlH3) {
  std::stringstream ss("4\n\n"
                       "Al     0.0000000000    0.0000000000   -0.0002000000\n"
                       "H     -1.3077000000    0.7550000000    0.0019100000\n"
                       "H      1.3077000000    0.7550000000    0.0019100000\n"
                       "H      0.0000000000   -1.5100000000    0.0019100000\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  method.setStructure(as);
  method.convergedCalculation(log, Utils::Derivative::None);

  ASSERT_THAT(method.getRepulsionEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(93.17841, 1e-3));
  ASSERT_THAT(method.getElectronicEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(-181.33808, 1e-3));
}

TEST_F(APM6Calculation, GetsCorrectElectronicEnergyForCl2) {
  std::stringstream ss("2\n\n"
                       "Cl    -0.9900000000    0.0000000000   -0.0000000000\n"
                       "Cl     0.9900000000    0.0000000000    0.0000000000\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  method.setStructure(as);
  method.convergedCalculation(log);

  ASSERT_THAT(method.getRepulsionEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(303.41784, 1e-3));
  ASSERT_THAT(method.getElectronicEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(-811.76884, 1e-3));
}

TEST_F(APM6Calculation, GetsCorrectTotalEnergyForH2) {
  std::stringstream ssH2("2\n\n"
                         "H -0.529177  0.105835 -0.158753\n"
                         "H -0.105835  0.105835 -0.158753\n");
  auto as = Utils::XyzStreamHandler::read(ssH2);
  method.setStructure(as);
  method.setScfMixer(Utils::scf_mixer_t::none);
  method.convergedCalculation(log);

  auto eigenvalues = method.getSingleParticleEnergies().getRestrictedEnergies();
  std::vector<double> expected = {-17.81134, 9.76611};

  for (unsigned i = 0; i < eigenvalues.size(); i++) {
    SCOPED_TRACE("... for the eigenvalue " + std::to_string(i) + ":");
    EXPECT_THAT(eigenvalues[i] * Utils::Constants::ev_per_hartree, DoubleNear(expected[i], 1e-4));
  }
  ASSERT_THAT(method.getElectronicEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(-49.49632, 1e-4));
  ASSERT_THAT(method.getRepulsionEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(27.86289, 1e-4));
}

TEST_F(APM6Calculation, GetsCorrectTotalEnergyForH2AtOtherDistance) {
  std::stringstream ss("2\n\n"
                       "H     0.0000000000    0.0000000000   -0.0000000000\n"
                       "H     2.0000000000    0.0000000000    0.0000000000\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  method.setStructure(as);
  method.convergedCalculation(log);

  ASSERT_THAT(method.getRepulsionEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(6.48781, 1e-4));
  ASSERT_THAT(method.getElectronicEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(-26.79570, 1e-3));
}

TEST_F(APM6Calculation, GetsCorrectTotalEnergyForC2) {
  std::stringstream ss("2\n\n"
                       "C     0.0000000000    0.0000000000   -0.0000000000\n"
                       "C     2.0000000000    0.0000000000    0.0000000000\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  method.setStructure(as);
  method.convergedCalculation(log);

  ASSERT_THAT(method.getRepulsionEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(102.50659, 1e-4));
  ASSERT_THAT(method.getElectronicEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(-332.24339, 1e-3));
}

TEST_F(APM6Calculation, GetsCorrectRepulsionEnergyForCH) {
  std::stringstream ss("2\n\n"
                       "C     0.0000000000    0.0000000000   -0.0000000000\n"
                       "H     2.0000000000    0.0000000000    0.0000000000\n");
  auto as = Utils::XyzStreamHandler::read(ss);

  method.setUnrestrictedCalculation(true);
  method.setSpinMultiplicity(2);
  method.setStructure(as);
  method.calculate(Utils::Derivative::None, log);

  ASSERT_THAT(method.getRepulsionEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(25.82052, 1e-4));
}

TEST_F(APM6Calculation, GetsCorrectTotalEnergyForFEHV) {
  std::stringstream ss("3\n\n"
                       "Fe     0.0000000000    0.0000000000   -0.0000000000\n"
                       "Cl     2.0000000000    0.0000000000    0.0000000000\n"
                       "H     -2.0000000000    0.0000000000    0.0000000000\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  method.setStructure(as);
  // method.finalizeCalculation();
  method.setScfMixer(Utils::scf_mixer_t::fock_diis);
  method.convergedCalculation(log, Utils::Derivative::None);
  ASSERT_THAT(method.getRepulsionEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(411.98969, 1e-3));
}

TEST_F(APM6Calculation, DISABLED_GetsCorrectTotalEnergyForHe2) { // TODO: find out what is wrong with mopac in this case
  std::stringstream ss("2\n\n"
                       "He    0.0000000000    0.0000000000   -0.0000000000\n"
                       "He    2.0000000000    0.0000000000    0.0000000000\n");
  auto as = Utils::XyzStreamHandler::read(ss);

  /*
   * Alain, 28.11.2014:
   * The one-electron matrix calculated is not the same as the one given by MOPAC.
   * The one in MOPAC has elements equal to zero for the <s|H|p> elements for the
   * same atom, which should be given by Vsp,B, which shouldn't be zero.
   * Basically the one-electon matrix should have the same structure as, for instance
   * C2 or Xe2, as they have the same number of orbitals etc., but it is not the case.
   */
  method.setStructure(as);
  method.convergedCalculation(log);
  /*!
  std::cout << "dens. \n" << method.getDensityMatrix() << std::endl;
  std::cout << "overlap \n" << method.getOverlapMatrix() << std::endl;
  std::cout << "f1. \n" << method.getOneElHMatrix()*Utils::Constants::ev_per_hartree << std::endl;
  std::cout << "f2. \n" << method.getTwoElGMatrix()*Utils::Constants::ev_per_hartree << std::endl;
  std::cout << "fock. \n" << method.getFockMatrix()*Utils::Constants::ev_per_hartree << std::endl;
  auto eigenvalues = method.getRestrictedEigenvalues();
  */

  ASSERT_THAT(method.getRepulsionEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(22.979910, 1e-4));
  ASSERT_THAT(method.getElectronicEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(-131.13180, 1e-4));
}

TEST_F(APM6Calculation, GetsCorrectTotalEnergyForXe2) {
  std::stringstream ss("2\n\n"
                       "Xe    0.0000000000    0.0000000000   -0.0000000000\n"
                       "Xe    2.0000000000    0.0000000000    0.0000000000\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  method.setStructure(as);
  method.convergedCalculation(log);

  ASSERT_THAT(method.getRepulsionEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(257.56561, 1e-3));
  ASSERT_THAT(method.getElectronicEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(-2131.51516, 1e-3));
}

TEST_F(APM6Calculation, GetsCorrectGradientsForMethane) {
  /* Structure optimized with MOPAC 2016
   *
   *
   * PM6 RHF 1SCF PRECISE GRADIENTS
   *
   *
   * C      0.0000000000    0.0000000000    0.0000000000
   * H      0.6287000000    0.6287000000    0.6287000000
   * H     -0.6287000000   -0.6287000000    0.6287000000
   * H     -0.6287000000    0.6287000000   -0.6287000000
   * H      0.6287000000   -0.6287000000   -0.6287000000
   *
   */

  auto stream = std::stringstream("5\n\n"
                                  "C      0.0000000000    0.0000000000    0.0000000000\n"
                                  "H      0.6287000000    0.6287000000    0.6287000000\n"
                                  "H     -0.6287000000   -0.6287000000    0.6287000000\n"
                                  "H     -0.6287000000    0.6287000000   -0.6287000000\n"
                                  "H      0.6287000000   -0.6287000000   -0.6287000000\n");
  auto structure = Utils::XyzStreamHandler::read(stream);
  method.setStructure(structure);
  method.convergedCalculation(log, Utils::Derivative::First);

  Eigen::RowVector3d cOneGradient(0.000009, -0.000017, 0.000005);
  Eigen::RowVector3d hOneGradient(1.725729, 1.725682, 1.725542);
  Eigen::RowVector3d hTwoGradient(-1.742019, -1.742019, 1.756755);
  Eigen::RowVector3d hThreeGradient(-1.742116, 1.757545, -1.741122);
  Eigen::RowVector3d hFourGradient(1.758416, -1.741192, -1.741180);
  cOneGradient *= Utils::Constants::angstrom_per_bohr * Utils::Constants::hartree_per_kCalPerMol;
  hOneGradient *= Utils::Constants::angstrom_per_bohr * Utils::Constants::hartree_per_kCalPerMol;
  hTwoGradient *= Utils::Constants::angstrom_per_bohr * Utils::Constants::hartree_per_kCalPerMol;
  hThreeGradient *= Utils::Constants::angstrom_per_bohr * Utils::Constants::hartree_per_kCalPerMol;
  hFourGradient *= Utils::Constants::angstrom_per_bohr * Utils::Constants::hartree_per_kCalPerMol;

  ASSERT_THAT(method.getEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(-177.169, 1e-3));
  ASSERT_TRUE((method.getGradients().row(0) - cOneGradient).norm() < 1e-4);
  ASSERT_TRUE((method.getGradients().row(1) - hOneGradient).norm() < 1e-4);
  ASSERT_TRUE((method.getGradients().row(2) - hTwoGradient).norm() < 1e-4);
  ASSERT_TRUE((method.getGradients().row(3) - hThreeGradient).norm() < 1e-4);
  ASSERT_TRUE((method.getGradients().row(4) - hFourGradient).norm() < 1e-4);
}

TEST_F(APM6Calculation, GetsZeroGradientForOptimizedMethane) {
  /* Structure optimized with MOPAC 2016
   *
   *
   * PM6 RHF OPT PRECISE GRADIENTS
   *
   *
   * C      0.0000000000    0.0000000000    0.0000000000
   * H      0.6287000000    0.6287000000    0.6287000000
   * H     -0.6287000000   -0.6287000000    0.6287000000
   * H     -0.6287000000    0.6287000000   -0.6287000000
   * H      0.6287000000   -0.6287000000   -0.6287000000
   *
   */
  std::stringstream stream("5\n\n"
                           "C     0.00000000   0.00000001  -0.00000097\n"
                           "H     0.62612502   0.62612484   0.62613824\n"
                           "H    -0.62612503  -0.62612486   0.62613824\n"
                           "H    -0.62612481   0.62612463  -0.62613657\n"
                           "H     0.62612481  -0.62612464  -0.62613657\n");

  auto structure = Utils::XyzStreamHandler::read(stream);
  method.setStructure(structure);
  method.convergedCalculation(log, Utils::Derivative::First);

  ASSERT_THAT(method.getEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(-177.17021, 1e-3));
  ASSERT_TRUE(method.getGradients().row(0).norm() < 1e-3);
  ASSERT_TRUE(method.getGradients().row(1).norm() < 1e-3);
  ASSERT_TRUE(method.getGradients().row(2).norm() < 1e-3);
  ASSERT_TRUE(method.getGradients().row(3).norm() < 1e-3);
  ASSERT_TRUE(method.getGradients().row(4).norm() < 1e-3);
}

TEST_F(APM6Calculation, GetsCorrectTotalEnergyForEthanol) {
  std::stringstream ss("9\n\n"
                       "H      1.9655905060   -0.0263662325    1.0690084915\n"
                       "C      1.3088788172   -0.0403821764    0.1943189946\n"
                       "H      1.5790293586    0.8034866305   -0.4554748131\n"
                       "H      1.5186511399   -0.9518066799   -0.3824432806\n"
                       "C     -0.1561112248    0.0249676675    0.5877379610\n"
                       "H     -0.4682794700   -0.8500294693    1.1854276282\n"
                       "H     -0.4063173598    0.9562730342    1.1264955766\n"
                       "O     -0.8772416674    0.0083263307   -0.6652828084\n"
                       "H     -1.8356000997    0.0539308952   -0.5014877498\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  method.setStructure(as);
  method.convergedCalculation(log);

  ASSERT_THAT(method.getEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(-618.35221, 4e-3));
  ASSERT_THAT(method.getRepulsionEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(1133.72286, 4e-3));
  ASSERT_THAT(method.getElectronicEnergy() * Utils::Constants::ev_per_hartree, DoubleNear(-1752.07507, 4e-3));
}

TEST_F(APM6Calculation, GetsSameTotalEnergyWithAndWithoutWrapper) {
  std::stringstream ss("9\n\n"
                       "H      1.9655905060   -0.0263662325    1.0690084915\n"
                       "C      1.3088788172   -0.0403821764    0.1943189946\n"
                       "H      1.5790293586    0.8034866305   -0.4554748131\n"
                       "H      1.5186511399   -0.9518066799   -0.3824432806\n"
                       "C     -0.1561112248    0.0249676675    0.5877379610\n"
                       "H     -0.4682794700   -0.8500294693    1.1854276282\n"
                       "H     -0.4063173598    0.9562730342    1.1264955766\n"
                       "O     -0.8772416674    0.0083263307   -0.6652828084\n"
                       "H     -1.8356000997    0.0539308952   -0.5014877498\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  pm6MethodWrapper->setStructure(as);
  auto result = pm6MethodWrapper->calculate("");
  method.setStructure(as);
  method.convergedCalculation(log);

  ASSERT_THAT(method.getEnergy(), DoubleNear(result.get<Utils::Property::Energy>(), 1e-5));
}

TEST_F(APM6Calculation, GetsSameTotalEnergyWithPolymorphicMethodAndWithMethod) {
  std::stringstream ss("9\n\n"
                       "H      1.9655905060   -0.0263662325    1.0690084915\n"
                       "C      1.3088788172   -0.0403821764    0.1943189946\n"
                       "H      1.5790293586    0.8034866305   -0.4554748131\n"
                       "H      1.5186511399   -0.9518066799   -0.3824432806\n"
                       "C     -0.1561112248    0.0249676675    0.5877379610\n"
                       "H     -0.4682794700   -0.8500294693    1.1854276282\n"
                       "H     -0.4063173598    0.9562730342    1.1264955766\n"
                       "O     -0.8772416674    0.0083263307   -0.6652828084\n"
                       "H     -1.8356000997    0.0539308952   -0.5014877498\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  polymorphicMethodWrapper->setStructure(as);
  auto result = polymorphicMethodWrapper->calculate("");
  method.setStructure(as);
  method.convergedCalculation(log);

  ASSERT_THAT(method.getEnergy(), DoubleNear(result.get<Utils::Property::Energy>(), 1e-5));
}

TEST_F(APM6Calculation, GetsSameTotalEnergyWithDynamicallyLoadedMethod) {
  auto& moduleManager = Core::ModuleManager::getInstance();
  auto dynamicallyLoadedMethodWrapper = moduleManager.get<Core::Calculator>("PM6");
  dynamicallyLoadedMethodWrapper->setLog(Core::Log::silent());
  dynamicallyLoadedMethodWrapper->settings().modifyInt(Utils::SettingsNames::maxScfIterations, 10000);
  dynamicallyLoadedMethodWrapper->settings().modifyDouble(Utils::SettingsNames::selfConsistenceCriterion, 1e-8);
  std::stringstream ss("9\n\n"
                       "H      1.9655905060   -0.0263662325    1.0690084915\n"
                       "C      1.3088788172   -0.0403821764    0.1943189946\n"
                       "H      1.5790293586    0.8034866305   -0.4554748131\n"
                       "H      1.5186511399   -0.9518066799   -0.3824432806\n"
                       "C     -0.1561112248    0.0249676675    0.5877379610\n"
                       "H     -0.4682794700   -0.8500294693    1.1854276282\n"
                       "H     -0.4063173598    0.9562730342    1.1264955766\n"
                       "O     -0.8772416674    0.0083263307   -0.6652828084\n"
                       "H     -1.8356000997    0.0539308952   -0.5014877498\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  dynamicallyLoadedMethodWrapper->setStructure(as);
  dynamicallyLoadedMethodWrapper->setRequiredProperties(Utils::Property::Energy);
  auto result = dynamicallyLoadedMethodWrapper->calculate("");
  method.setStructure(as);
  method.convergedCalculation(log);

  ASSERT_THAT(result.get<Utils::Property::Energy>() * Utils::Constants::ev_per_hartree, DoubleNear(-618.35221, 4e-3));
  ASSERT_THAT(method.getEnergy(), DoubleNear(result.get<Utils::Property::Energy>(), 1e-5));
}

TEST_F(APM6Calculation, MethodWrapperCanBeCloned) {
  auto& moduleManager = Core::ModuleManager::getInstance();
  auto dynamicallyLoadedMethodWrapper = moduleManager.get<Core::Calculator>("PM6");
  dynamicallyLoadedMethodWrapper->setLog(Core::Log::silent());
  dynamicallyLoadedMethodWrapper->settings().modifyDouble(Utils::SettingsNames::selfConsistenceCriterion, 1e-8);

  std::stringstream ss("9\n\n"
                       "H      1.9655905060   -0.0263662325    1.0690084915\n"
                       "C      1.3088788172   -0.0403821764    0.1943189946\n"
                       "H      1.5790293586    0.8034866305   -0.4554748131\n"
                       "H      1.5186511399   -0.9518066799   -0.3824432806\n"
                       "C     -0.1561112248    0.0249676675    0.5877379610\n"
                       "H     -0.4682794700   -0.8500294693    1.1854276282\n"
                       "H     -0.4063173598    0.9562730342    1.1264955766\n"
                       "O     -0.8772416674    0.0083263307   -0.6652828084\n"
                       "H     -1.8356000997    0.0539308952   -0.5014877498\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  dynamicallyLoadedMethodWrapper->setStructure(as);

  auto cloned = dynamicallyLoadedMethodWrapper->clone();

  auto scc = Utils::SettingsNames::selfConsistenceCriterion;
  ASSERT_EQ(cloned->settings().getDouble(scc), dynamicallyLoadedMethodWrapper->settings().getDouble(scc));
}

TEST_F(APM6Calculation, StructureIsCorrectlyCloned) {
  auto& moduleManager = Core::ModuleManager::getInstance();
  auto dynamicallyLoadedMethodWrapper = moduleManager.get<Core::Calculator>("PM6");
  dynamicallyLoadedMethodWrapper->setLog(Core::Log::silent());

  std::stringstream ss("9\n\n"
                       "H      1.9655905060   -0.0263662325    1.0690084915\n"
                       "C      1.3088788172   -0.0403821764    0.1943189946\n"
                       "H      1.5790293586    0.8034866305   -0.4554748131\n"
                       "H      1.5186511399   -0.9518066799   -0.3824432806\n"
                       "C     -0.1561112248    0.0249676675    0.5877379610\n"
                       "H     -0.4682794700   -0.8500294693    1.1854276282\n"
                       "H     -0.4063173598    0.9562730342    1.1264955766\n"
                       "O     -0.8772416674    0.0083263307   -0.6652828084\n"
                       "H     -1.8356000997    0.0539308952   -0.5014877498\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  dynamicallyLoadedMethodWrapper->setStructure(as);

  auto cloned = dynamicallyLoadedMethodWrapper->clone();

  ASSERT_EQ(cloned->getPositions(), dynamicallyLoadedMethodWrapper->getPositions());
}

TEST_F(APM6Calculation, ClonedMethodCanCalculate) {
  auto& moduleManager = Core::ModuleManager::getInstance();
  auto dynamicallyLoadedMethodWrapper = moduleManager.get<Core::Calculator>("PM6");
  dynamicallyLoadedMethodWrapper->setLog(Core::Log::silent());

  std::stringstream ss("9\n\n"
                       "H      1.9655905060   -0.0263662325    1.0690084915\n"
                       "C      1.3088788172   -0.0403821764    0.1943189946\n"
                       "H      1.5790293586    0.8034866305   -0.4554748131\n"
                       "H      1.5186511399   -0.9518066799   -0.3824432806\n"
                       "C     -0.1561112248    0.0249676675    0.5877379610\n"
                       "H     -0.4682794700   -0.8500294693    1.1854276282\n"
                       "H     -0.4063173598    0.9562730342    1.1264955766\n"
                       "O     -0.8772416674    0.0083263307   -0.6652828084\n"
                       "H     -1.8356000997    0.0539308952   -0.5014877498\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  dynamicallyLoadedMethodWrapper->setStructure(as);

  auto cloned = dynamicallyLoadedMethodWrapper->clone();

  auto resultCloned = cloned->calculate("");
  auto result = dynamicallyLoadedMethodWrapper->calculate("");
  ASSERT_THAT(resultCloned.get<Utils::Property::Energy>(), DoubleNear(result.get<Utils::Property::Energy>(), 1e-9));
}

TEST_F(APM6Calculation, ClonedMethodCanCalculateGradients) {
  auto& moduleManager = Core::ModuleManager::getInstance();
  auto dynamicallyLoadedMethodWrapper = moduleManager.get<Core::Calculator>("PM6");
  dynamicallyLoadedMethodWrapper->setLog(Core::Log::silent());

  std::stringstream ss("9\n\n"
                       "H      1.9655905060   -0.0263662325    1.0690084915\n"
                       "C      1.3088788172   -0.0403821764    0.1943189946\n"
                       "H      1.5790293586    0.8034866305   -0.4554748131\n"
                       "H      1.5186511399   -0.9518066799   -0.3824432806\n"
                       "C     -0.1561112248    0.0249676675    0.5877379610\n"
                       "H     -0.4682794700   -0.8500294693    1.1854276282\n"
                       "H     -0.4063173598    0.9562730342    1.1264955766\n"
                       "O     -0.8772416674    0.0083263307   -0.6652828084\n"
                       "H     -1.8356000997    0.0539308952   -0.5014877498\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  dynamicallyLoadedMethodWrapper->setStructure(as);

  auto cloned = dynamicallyLoadedMethodWrapper->clone();

  dynamicallyLoadedMethodWrapper->setRequiredProperties(Utils::Property::Gradients);
  cloned->setRequiredProperties(Utils::Property::Gradients);

  auto resultCloned = cloned->calculate("");
  auto result = dynamicallyLoadedMethodWrapper->calculate("");

  for (int i = 0; i < resultCloned.get<Utils::Property::Gradients>().rows(); ++i)
    for (int j = 0; j < resultCloned.get<Utils::Property::Gradients>().cols(); ++j)
      ASSERT_THAT(resultCloned.get<Utils::Property::Gradients>()(i, j),
                  DoubleNear(result.get<Utils::Property::Gradients>()(i, j), 1e-12));
}

TEST_F(APM6Calculation, ClonedMethodCanCalculateGradientsWithDifferentNumberCores) {
  auto& moduleManager = Core::ModuleManager::getInstance();
  auto dynamicallyLoadedMethodWrapper = moduleManager.get<Core::Calculator>("PM6");
  dynamicallyLoadedMethodWrapper->setLog(Core::Log::silent());

  std::stringstream ss("9\n\n"
                       "H      1.9655905060   -0.0263662325    1.0690084915\n"
                       "C      1.3088788172   -0.0403821764    0.1943189946\n"
                       "H      1.5790293586    0.8034866305   -0.4554748131\n"
                       "H      1.5186511399   -0.9518066799   -0.3824432806\n"
                       "C     -0.1561112248    0.0249676675    0.5877379610\n"
                       "H     -0.4682794700   -0.8500294693    1.1854276282\n"
                       "H     -0.4063173598    0.9562730342    1.1264955766\n"
                       "O     -0.8772416674    0.0083263307   -0.6652828084\n"
                       "H     -1.8356000997    0.0539308952   -0.5014877498\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  dynamicallyLoadedMethodWrapper->setStructure(as);

  auto cloned = dynamicallyLoadedMethodWrapper->clone();

  dynamicallyLoadedMethodWrapper->setRequiredProperties(Utils::Property::Gradients);
  cloned->setRequiredProperties(Utils::Property::Gradients);

  auto numThreads = omp_get_num_threads();
  omp_set_num_threads(1);
  auto resultCloned = cloned->calculate("");
  omp_set_num_threads(4);
  auto result = dynamicallyLoadedMethodWrapper->calculate("");
  omp_set_num_threads(numThreads);

  for (int i = 0; i < resultCloned.get<Utils::Property::Gradients>().size(); ++i) {
    ASSERT_THAT(resultCloned.get<Utils::Property::Gradients>()(i),
                DoubleNear(result.get<Utils::Property::Gradients>()(i), 1e-7));
  }
  ASSERT_EQ(result.get<Utils::Property::SuccessfulCalculation>(), true);
}

TEST_F(APM6Calculation, NotClonedMethodCanCalculateGradientsWithDifferentNumberCores) {
  auto& moduleManager = Core::ModuleManager::getInstance();
  auto dynamicallyLoadedMethodWrapper = moduleManager.get<Core::Calculator>("PM6");
  auto dynamicallyLoadedMethodWrapper2 = moduleManager.get<Core::Calculator>("PM6");
  dynamicallyLoadedMethodWrapper->setLog(Core::Log::silent());
  dynamicallyLoadedMethodWrapper2->setLog(Core::Log::silent());

  std::stringstream ss("9\n\n"
                       "H      1.9655905060   -0.0263662325    1.0690084915\n"
                       "C      1.3088788172   -0.0403821764    0.1943189946\n"
                       "H      1.5790293586    0.8034866305   -0.4554748131\n"
                       "H      1.5186511399   -0.9518066799   -0.3824432806\n"
                       "C     -0.1561112248    0.0249676675    0.5877379610\n"
                       "H     -0.4682794700   -0.8500294693    1.1854276282\n"
                       "H     -0.4063173598    0.9562730342    1.1264955766\n"
                       "O     -0.8772416674    0.0083263307   -0.6652828084\n"
                       "H     -1.8356000997    0.0539308952   -0.5014877498\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  dynamicallyLoadedMethodWrapper->setStructure(as);
  dynamicallyLoadedMethodWrapper2->setStructure(as);

  dynamicallyLoadedMethodWrapper->setRequiredProperties(Utils::Property::Gradients);
  dynamicallyLoadedMethodWrapper2->setRequiredProperties(Utils::Property::Gradients);

  auto numThreads = omp_get_num_threads();
  omp_set_num_threads(1);
  auto resultCloned = dynamicallyLoadedMethodWrapper2->calculate("");
  omp_set_num_threads(4);
  auto result = dynamicallyLoadedMethodWrapper->calculate("");
  omp_set_num_threads(numThreads);

  for (int i = 0; i < resultCloned.get<Utils::Property::Gradients>().size(); ++i) {
    ASSERT_THAT(resultCloned.get<Utils::Property::Gradients>()(i),
                DoubleNear(result.get<Utils::Property::Gradients>()(i), 1e-7));
  }
}

TEST_F(APM6Calculation, ClonedMethodCopiesResultsCorrectly) {
  auto& moduleManager = Core::ModuleManager::getInstance();
  auto dynamicallyLoadedMethodWrapper = moduleManager.get<Core::Calculator>("PM6");
  dynamicallyLoadedMethodWrapper->setLog(Core::Log::silent());

  std::stringstream ss("9\n\n"
                       "H      1.9655905060   -0.0263662325    1.0690084915\n"
                       "C      1.3088788172   -0.0403821764    0.1943189946\n"
                       "H      1.5790293586    0.8034866305   -0.4554748131\n"
                       "H      1.5186511399   -0.9518066799   -0.3824432806\n"
                       "C     -0.1561112248    0.0249676675    0.5877379610\n"
                       "H     -0.4682794700   -0.8500294693    1.1854276282\n"
                       "H     -0.4063173598    0.9562730342    1.1264955766\n"
                       "O     -0.8772416674    0.0083263307   -0.6652828084\n"
                       "H     -1.8356000997    0.0539308952   -0.5014877498\n");
  auto as = Utils::XyzStreamHandler::read(ss);
  dynamicallyLoadedMethodWrapper->setStructure(as);

  auto result = dynamicallyLoadedMethodWrapper->calculate();
  auto cloned = dynamicallyLoadedMethodWrapper->clone();

  ASSERT_THAT(cloned->results().get<Utils::Property::Energy>(), DoubleNear(result.get<Utils::Property::Energy>(), 1e-9));
}

TEST_F(APM6Calculation, AtomCollectionCanBeReturned) {
  std::stringstream ssH("4\n\n"
                        "C      0.0000000000    0.0000000000    0.0000000000\n"
                        "V      0.0529177211   -0.3175063264    0.2645886053\n"
                        "H     -0.5291772107    0.1058354421   -0.1587531632\n"
                        "H     -0.1058354421    0.1058354421   -0.1587531632\n");
  auto structure = Utils::XyzStreamHandler::read(ssH);
  pm6MethodWrapper->settings().modifyInt(Utils::SettingsNames::molecularCharge, 1);
  pm6MethodWrapper->setStructure(structure);
  ASSERT_EQ(structure.getPositions(), pm6MethodWrapper->getStructure()->getPositions());
  for (unsigned int i = 0; i < structure.getElements().size(); ++i) {
    ASSERT_EQ(structure.getElements()[i], pm6MethodWrapper->getStructure()->getElements()[i]);
  }
}

TEST_F(APM6Calculation, ChargeIsNotSilentlyUnset) {
  auto calculator = std::make_shared<PM6MethodWrapper>();
  calculator->settings().modifyInt(Utils::SettingsNames::molecularCharge, -1);
  calculator->applySettings();
  ASSERT_EQ(calculator->settings().getInt(Utils::SettingsNames::molecularCharge), -1);
}

TEST_F(APM6Calculation, CorrectlyCalculatesThermochemistry) {
  auto calculator = std::make_shared<PM6MethodWrapper>();
  calculator->setLog(Core::Log::silent());
  std::stringstream ssN2{"2\n\n"
                         "N      -0.5585    0.0000    0.0000\n"
                         "N       0.5585    0.0000   -0.0000\n"};
  calculator->settings().modifyInt("symmetry_number", 2);
  calculator->settings().modifyDouble(Utils::SettingsNames::temperature, 300.0);
  calculator->settings().modifyDouble(Utils::SettingsNames::selfConsistenceCriterion, 1e-9);
  calculator->setRequiredProperties(Utils::Property::Thermochemistry);
  calculator->setStructure(Utils::XyzStreamHandler::read(ssN2));
  calculator->calculate("");
  auto thermo = calculator->results().get<Utils::Property::Thermochemistry>();
  EXPECT_THAT(thermo.vibrationalComponent.enthalpy,
              DoubleNear(0.0636 / 1000.0 * Utils::Constants::hartree_per_kCalPerMol, 1e-6));
  EXPECT_THAT(thermo.vibrationalComponent.entropy,
              DoubleNear(0.0002 / 1000.0 * Utils::Constants::hartree_per_kCalPerMol, 1e-6));
  EXPECT_THAT(thermo.rotationalComponent.enthalpy,
              DoubleNear(596.1620 / 1000.0 * Utils::Constants::hartree_per_kCalPerMol, 1e-6));
  EXPECT_THAT(thermo.rotationalComponent.entropy, DoubleNear(9.9162 / 1000.0 * Utils::Constants::hartree_per_kCalPerMol, 1e-6));
  EXPECT_THAT(thermo.translationalComponent.enthalpy,
              DoubleNear(1490.4049 / 1000.0 * Utils::Constants::hartree_per_kCalPerMol, 1e-6));
  EXPECT_THAT(thermo.translationalComponent.entropy,
              DoubleNear(35.9559 / 1000.0 * Utils::Constants::hartree_per_kCalPerMol, 1e-6));
}

TEST_F(APM6Calculation, UpdatesCorrectlySpinMode) {
  std::stringstream ssN2{"2\n\n"
                         "N      -0.5585    0.0000    0.0000\n"
                         "N       0.5585    0.0000   -0.0000\n"};
  auto structure = Utils::XyzStreamHandler::read(ssN2);
  pm6MethodWrapper->setStructure(structure);
  EXPECT_EQ(pm6MethodWrapper->settings().getString(Utils::SettingsNames::spinMode),
            Utils::SpinModeInterpreter::getStringFromSpinMode(Utils::SpinMode::Any));
  pm6MethodWrapper->calculate("");
  EXPECT_EQ(pm6MethodWrapper->settings().getString(Utils::SettingsNames::spinMode),
            Utils::SpinModeInterpreter::getStringFromSpinMode(Utils::SpinMode::Restricted));
}

TEST_F(APM6Calculation, GetsCorrectAtomicHessians) {
  std::stringstream ssH("5\n\n"
                        "C     -4.22875    2.29085   -0.00000\n"
                        "H     -3.42876    2.04248    0.66574\n"
                        "H     -3.90616    3.05518   -0.67574\n"
                        "H     -4.51405    1.42151   -0.55475\n"
                        "H     -5.06606    2.64424    0.56475\n");
  auto structure = Utils::XyzStreamHandler::read(ssH);
  method.setStructure(structure);
  method.convergedCalculation(log, Utils::Derivative::SecondAtomic);

  auto ah = method.getAtomicSecondDerivatives();
  ASSERT_EQ(ah.size(), 5);
  // Reference data has been generated with original re-implementation in Sparrow
  ASSERT_NEAR(ah.getAtomicHessian(0)(0, 0), 0.59720109232151419, 1e-6);
  ASSERT_NEAR(ah.getAtomicHessian(0)(0, 1), 1.5639100396924732e-06, 1e-6);
  ASSERT_NEAR(ah.getAtomicHessian(0)(0, 2), 5.949380209022137e-06, 1e-6);
  ASSERT_NEAR(ah.getAtomicHessian(0)(1, 0), 1.5639100396924732e-06, 1e-6);
  ASSERT_NEAR(ah.getAtomicHessian(0)(1, 1), 0.59720271417414317, 1e-6);
  ASSERT_NEAR(ah.getAtomicHessian(0)(1, 2), -3.3392744368428151e-06, 1e-6);
  ASSERT_NEAR(ah.getAtomicHessian(0)(2, 0), 5.949380209022137e-06, 1e-6);
  ASSERT_NEAR(ah.getAtomicHessian(0)(2, 1), -3.3392744368428151e-06, 1e-6);
  ASSERT_NEAR(ah.getAtomicHessian(0)(2, 2), 0.59720302471686415, 1e-6);
}

TEST_F(APM6Calculation, OrbitalSteeringSavesStretchedMethane) {
  std::stringstream ssStretchedMethane{"5\n\n"
                                       "C      0.00000    0.00000    0.00000\n"
                                       "H      0.00000    0.00000    2.58900\n"
                                       "H      1.02672    0.00000   -0.36300\n"
                                       "H     -0.51336   -0.88916   -0.36300\n"
                                       "H     -0.51336    0.88916   -0.36300"};
  pm6MethodWrapper->setStructure(Utils::XyzStreamHandler::read(ssStretchedMethane));
  pm6MethodWrapper->settings().modifyString(Utils::SettingsNames::spinMode, "unrestricted");
  OrbitalSteeringCalculator steerer;
  steerer.setLog(Core::Log::silent());
  steerer.settings().modifyInt(mixingFrequencyKey, 1);
  steerer.setReferenceCalculator(pm6MethodWrapper);
  steerer.referenceCalculation();
  double energy1 = steerer.results().get<Utils::Property::Energy>();
  double energy2 = 0.0;
  // The steering is undeterministic, so we need to do it a couple of times to be sure
  // that it happens. In the norm it is steered after the first iteration, but sometimes
  // also after the second one or so.
  for (int i = 0; i < 15; ++i) {
    energy2 = steerer.calculate().get<Utils::Property::Energy>();
  }

  ASSERT_LT(energy2, energy1);
}

TEST_F(APM6Calculation, OrbitalSteeringHasAllRequiredProperties) {
  std::stringstream ssStretchedMethane{"5\n\n"
                                       "C      0.00000    0.00000    0.00000\n"
                                       "H      0.00000    0.00000    2.58900\n"
                                       "H      1.02672    0.00000   -0.36300\n"
                                       "H     -0.51336   -0.88916   -0.36300\n"
                                       "H     -0.51336    0.88916   -0.36300"};
  pm6MethodWrapper->setStructure(Utils::XyzStreamHandler::read(ssStretchedMethane));
  pm6MethodWrapper->settings().modifyString(Utils::SettingsNames::spinMode, "unrestricted");
  pm6MethodWrapper->setRequiredProperties(Utils::Property::Energy | Utils::Property::Gradients);
  OrbitalSteeringCalculator steerer;
  steerer.setLog(Core::Log::silent());
  steerer.settings().modifyInt(mixingFrequencyKey, 2);
  steerer.setReferenceCalculator(pm6MethodWrapper);
  auto steeredResults = steerer.calculate();
  // The steering is undeterministic, so we need to do it a couple of times to be sure
  // that it happens. In the norm it is steered after the first iteration, but sometimes
  // also after the second one or so.
  // Here I want to check that independently from when it happens, one always gets energy and
  // gradients.
  for (int i = 0; i < 10; ++i) {
    steeredResults = steerer.calculate();
    ASSERT_TRUE(steeredResults.has<Utils::Property::Energy>());
    ASSERT_TRUE(steeredResults.has<Utils::Property::Gradients>());
  }
}

TEST_F(APM6Calculation, OrbitalSteeringIssuesWarningForRestrictedOrbitalsInFirstIteration) {
  std::stringstream ssStretchedMethane{"5\n\n"
                                       "C      0.00000    0.00000    0.00000\n"
                                       "H      0.00000    0.00000    2.58900\n"
                                       "H      1.02672    0.00000   -0.36300\n"
                                       "H     -0.51336   -0.88916   -0.36300\n"
                                       "H     -0.51336    0.88916   -0.36300"};
  pm6MethodWrapper->setStructure(Utils::XyzStreamHandler::read(ssStretchedMethane));
  pm6MethodWrapper->settings().modifyString(Utils::SettingsNames::spinMode, "restricted");
  pm6MethodWrapper->setRequiredProperties(Utils::Property::Energy);
  OrbitalSteeringCalculator steerer;

  auto testStream = std::make_shared<std::ostringstream>(std::ios_base::out | std::ios_base::app);

  steerer.settings().modifyInt(mixingFrequencyKey, 2);
  steerer.setReferenceCalculator(pm6MethodWrapper);
  steerer.getLog() = Core::Log::silent();
  steerer.getLog().warning.add("testout", testStream);
  auto steeredResults = steerer.calculate();
  EXPECT_FALSE(testStream->str().empty());
  testStream->str("");
  // Here I want to check that the warning that the orbitals are restricted
  // is printed only at the first iteration and not every iteration.
  // The warning log should then be empty. Multiple testing to be sure that
  // also if the steering happens nothing bad happens.
  for (int i = 0; i < 10; ++i) {
    steeredResults = steerer.calculate();
    EXPECT_TRUE(testStream->str().empty());
  }
}

TEST_F(APM6Calculation, AfterSetStructureResultsDestroyed) {
  std::stringstream ssH2("2\n\n"
                         "H -0.529177  0.105835 -0.158753\n"
                         "H -0.105835  0.105835 -0.158753\n");
  auto structure = Utils::XyzStreamHandler::read(ssH2);
  pm6MethodWrapper->setStructure(structure);
  pm6MethodWrapper->settings().modifyString(Utils::SettingsNames::spinMode, "restricted");
  pm6MethodWrapper->setRequiredProperties(Utils::Property::Energy);
  const auto& results = pm6MethodWrapper->calculate("");
  ASSERT_TRUE(results.has<Utils::Property::Energy>());
  pm6MethodWrapper->setStructure(structure);
  ASSERT_FALSE(results.has<Utils::Property::Energy>());
  pm6MethodWrapper->calculate("");
  ASSERT_TRUE(results.has<Utils::Property::Energy>());
  pm6MethodWrapper->modifyPositions(structure.getPositions());
  ASSERT_FALSE(results.has<Utils::Property::Energy>());
}

} // namespace Sparrow
} // namespace Scine

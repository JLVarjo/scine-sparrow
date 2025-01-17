/**
 * @file
 * @copyright This code is licensed under the 3-clause BSD license.\n
 *            Copyright ETH Zurich, Laboratory of Physical Chemistry, Reiher Group.\n
 *            See LICENSE.txt for details.
 */

#include "multipoleTypes.h"
#include "GeneralTypes.h"

namespace Scine {
namespace Sparrow {

namespace nddo {
using namespace GeneralTypes;

namespace multipole {

rotationOrbitalPair getRotPairType(orb_t o1, orb_t o2) {
  if (o1 == s && o2 == s)
    return rotationOrbitalPair::s_s;
  if (o1 == x && o2 == x)
    return rotationOrbitalPair::x_x;
  if (o1 == x && o2 == y)
    return rotationOrbitalPair::x_y;
  if (o1 == x && o2 == z)
    return rotationOrbitalPair::x_z;
  if (o1 == y && o2 == x)
    return rotationOrbitalPair::y_x;
  if (o1 == y && o2 == y)
    return rotationOrbitalPair::y_y;
  if (o1 == y && o2 == z)
    return rotationOrbitalPair::y_z;
  if (o1 == z && o2 == x)
    return rotationOrbitalPair::z_x;
  if (o1 == z && o2 == y)
    return rotationOrbitalPair::z_y;
  if (o1 == z && o2 == z)
    return rotationOrbitalPair::z_z;
  if (o1 == x2y2 && o2 == x2y2)
    return rotationOrbitalPair::x2y2_x2y2;
  if (o1 == x2y2 && o2 == xz)
    return rotationOrbitalPair::x2y2_xz;
  if (o1 == x2y2 && o2 == z2)
    return rotationOrbitalPair::x2y2_z2;
  if (o1 == x2y2 && o2 == yz)
    return rotationOrbitalPair::x2y2_yz;
  if (o1 == x2y2 && o2 == xy)
    return rotationOrbitalPair::x2y2_xy;
  if (o1 == xz && o2 == x2y2)
    return rotationOrbitalPair::xz_x2y2;
  if (o1 == xz && o2 == xz)
    return rotationOrbitalPair::xz_xz;
  if (o1 == xz && o2 == z2)
    return rotationOrbitalPair::xz_z2;
  if (o1 == xz && o2 == yz)
    return rotationOrbitalPair::xz_yz;
  if (o1 == xz && o2 == xy)
    return rotationOrbitalPair::xz_xy;
  if (o1 == z2 && o2 == x2y2)
    return rotationOrbitalPair::z2_x2y2;
  if (o1 == z2 && o2 == xz)
    return rotationOrbitalPair::z2_xz;
  if (o1 == z2 && o2 == z2)
    return rotationOrbitalPair::z2_z2;
  if (o1 == z2 && o2 == yz)
    return rotationOrbitalPair::z2_yz;
  if (o1 == z2 && o2 == xy)
    return rotationOrbitalPair::z2_xy;
  if (o1 == yz && o2 == x2y2)
    return rotationOrbitalPair::yz_x2y2;
  if (o1 == yz && o2 == xz)
    return rotationOrbitalPair::yz_xz;
  if (o1 == yz && o2 == z2)
    return rotationOrbitalPair::yz_z2;
  if (o1 == yz && o2 == yz)
    return rotationOrbitalPair::yz_yz;
  if (o1 == yz && o2 == xy)
    return rotationOrbitalPair::yz_xy;
  if (o1 == xy && o2 == x2y2)
    return rotationOrbitalPair::xy_x2y2;
  if (o1 == xy && o2 == xz)
    return rotationOrbitalPair::xy_xz;
  if (o1 == xy && o2 == z2)
    return rotationOrbitalPair::xy_z2;
  if (o1 == xy && o2 == yz)
    return rotationOrbitalPair::xy_yz;
  if (o1 == xy && o2 == xy)
    return rotationOrbitalPair::xy_xy;

  throw InvalidOrbitalPairException();
}

int MQuantumNumber(Multipole m) {
  int n;
  switch (m) {
    case Multipole::M00:
      n = 0;
      break;
    case Multipole::Qxx:
      n = 0;
      break;
    case Multipole::Qyy:
      n = 0;
      break;
    case Multipole::Qzz:
      n = 0;
      break;
    case Multipole::M1m1:
      n = -1;
      break;
    case Multipole::M10:
      n = 0;
      break;
    case Multipole::M11:
      n = 1;
      break;
    case Multipole::M2m2:
      n = -2;
      break;
    case Multipole::M2m1:
      n = -1;
      break;
    case Multipole::M20:
      n = 0;
      break;
    case Multipole::M21:
      n = 1;
      break;
    case Multipole::M22:
      n = 2;
      break;
    case Multipole::Qzx:
      n = 2;
      break;
    default:
      throw InvalidMultipoleException();
  }
  return n;
}
int LQuantumNumber(Multipole m) {
  int n;
  switch (m) {
    case Multipole::M00:
      n = 0;
      break;
    case Multipole::Qxx:
      n = 2;
      break;
    case Multipole::Qyy:
      n = 2;
      break;
    case Multipole::Qzz:
      n = 2;
      break;
    case Multipole::M1m1:
      n = 1;
      break;
    case Multipole::M10:
      n = 1;
      break;
    case Multipole::M11:
      n = 1;
      break;
    case Multipole::M2m2:
      n = 2;
      break;
    case Multipole::M2m1:
      n = 2;
      break;
    case Multipole::M20:
      n = 2;
      break;
    case Multipole::M21:
      n = 2;
      break;
    case Multipole::M22:
      n = 2;
      break;
    case Multipole::Qzx:
      n = 2;
      break;
    default:
      throw InvalidMultipoleException();
  }
  return n;
}

MultipolePair pairType(int l1, int l2, int l) {
  if (l1 == 0 && l2 == 1 && l == 1)
    return MultipolePair::sp1;
  if (l1 == 1 && l2 == 2 && l == 1)
    return MultipolePair::pd1;
  if (l1 == 1 && l2 == 1 && l == 2)
    return MultipolePair::pp2;
  if (l1 == 0 && l2 == 2 && l == 2)
    return MultipolePair::sd2;
  if (l1 == 2 && l2 == 2 && l == 2)
    return MultipolePair::dd2;
  if (l1 == 0 && l2 == 0 && l == 0)
    return MultipolePair::ss0;
  if (l1 == 1 && l2 == 1 && l == 0)
    return MultipolePair::pp0;
  if (l1 == 2 && l2 == 2 && l == 0)
    return MultipolePair::dd0;

  throw InvalidQuantumNumbersException();
}

} // namespace multipole

} // namespace nddo
} // namespace Sparrow
} // namespace Scine

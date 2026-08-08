#include <greeter/CylinderMagnet.h>
#include <greeter/Quaternion.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

  // Relative accuracy the elliptic integral iterates to. Bulirsch's algorithm
  // converges quadratically, so this costs a handful of iterations.
  constexpr double CEL_TOLERANCE = 1.0e-12;

  // Below this dimensionless radius the general diametral solution loses its
  // accuracy to cancellation and a series around the axis is used instead.
  constexpr double SMALL_RADIUS = 0.05;

}  // namespace


greeter::CylinderMagnet::CylinderMagnet()
    : position({0.0f, 0.0f, 0.0f}),
      dimensions({1.0f, 1.0f}),
      orientation({1.0f, 0.0f, 0.0f, 0.0f}),
      magnetization({0.0f, 0.0f, 1.0f}) {}

greeter::CylinderMagnet::CylinderMagnet(
    std::vector<float> _position, std::vector<float> _orientation,
    std::vector<float> _dimensions, std::vector<float> _magnetization)
    : position(_position),
      dimensions(_dimensions),
      orientation(_orientation),
      magnetization(_magnetization) {}

greeter::CylinderMagnet::CylinderMagnet(const CylinderMagnet& other)
    : position(other.position),
      dimensions(other.dimensions),
      orientation(other.orientation),
      magnetization(other.magnetization) {}

greeter::CylinderMagnet::~CylinderMagnet() {}


std::string greeter::CylinderMagnet::getStaticTypeName() { return "cylinder"; }

u_int16_t greeter::CylinderMagnet::getStaticTypeID() { return 3; }

size_t greeter::CylinderMagnet::numberOfParameters() { return 12; }

uint16_t greeter::CylinderMagnet::getTypeID() const { return getStaticTypeID(); }

size_t greeter::CylinderMagnet::getNumOfParameters() const {
  return numberOfParameters();
}

std::vector<float> greeter::CylinderMagnet::getPosition() const { return position; }

std::vector<float> greeter::CylinderMagnet::getDimensions() const { return dimensions; }

std::vector<float> greeter::CylinderMagnet::getOrientation() const { return orientation; }

std::vector<float> greeter::CylinderMagnet::getMagnetization() const { return magnetization; }

std::unique_ptr<greeter::Magnet> greeter::CylinderMagnet::clone() const {
  return std::make_unique<greeter::CylinderMagnet>(*this);
}

void greeter::CylinderMagnet::setPosition(const float& x, const float& y, const float& z) {
  position = {x, y, z};
}

void greeter::CylinderMagnet::translate(const float& x, const float& y, const float& z) {
  position[0] += x;
  position[1] += y;
  position[2] += z;
}

void greeter::CylinderMagnet::display() const {
  std::cout << "Cylinder magnet, diameter " << dimensions[0]
            << ", height " << dimensions[1]
            << ", at (" << position[0] << ", " << position[1] << ", "
            << position[2] << ")" << std::endl;
}


double greeter::CylinderMagnet::completeEllipticIntegral(
    double kc, double p, double c, double s) {

  if (kc == 0.0) {
    throw std::domain_error("cel is not defined for kc = 0");
  }

  // A negative p is folded onto a positive one by Bulirsch's transformation,
  // which is what lets the same routine return the integral of the third kind.
  if (p > 0.0) {
    p = std::sqrt(p);
    s /= p;
  } else {
    double f = kc * kc;
    double q = 1.0 - f;
    const double g = 1.0 - p;
    f -= p;
    q *= s - c * p;

    p = std::sqrt(f / g);
    c = (c - s) / g;
    s = -q / (g * g * p) + c * p;
  }

  double em = 1.0;
  double qc = std::fabs(kc);
  double kk = qc;

  while (true) {

    double g = kk / p;
    const double c_next = c + s / p;
    s = 2.0 * (s + c * g);
    c = c_next;
    p += g;

    g = em;
    em += qc;

    if (std::fabs(g - qc) <= g * CEL_TOLERANCE) {
      break;
    }

    qc = 2.0 * std::sqrt(kk);
    kk = em * qc;
  }

  return (M_PI / 2.0) * (s + c * em) / (em * (em + p));
}


void greeter::CylinderMagnet::calculateAxialBField(
    double z0, double r, double z, double& b_r, double& b_z) {

  const double zph = z + z0;
  const double zmh = z - z0;
  const double dpr = 1.0 + r;
  const double dmr = 1.0 - r;

  const double sq0 = std::sqrt(zmh * zmh + dpr * dpr);
  const double sq1 = std::sqrt(zph * zph + dpr * dpr);

  const double k1 = std::sqrt((zph * zph + dmr * dmr) / (zph * zph + dpr * dpr));
  const double k0 = std::sqrt((zmh * zmh + dmr * dmr) / (zmh * zmh + dpr * dpr));
  const double gamma = dmr / dpr;

  b_r = (completeEllipticIntegral(k1, 1.0, 1.0, -1.0) / sq1
         - completeEllipticIntegral(k0, 1.0, 1.0, -1.0) / sq0) / M_PI;

  b_z = (zph * completeEllipticIntegral(k1, gamma * gamma, 1.0, gamma) / sq1
         - zmh * completeEllipticIntegral(k0, gamma * gamma, 1.0, gamma) / sq0)
        / dpr / M_PI;
}


void greeter::CylinderMagnet::calculateDiametralHField(
    double z0, double r, double z, double phi,
    double& h_r, double& h_phi, double& h_z) {

  const double zp = z + z0;
  const double zm = z - z0;
  const double zp2 = zp * zp;
  const double zm2 = zm * zm;
  const double r2 = r * r;

  if (r < SMALL_RADIUS) {

    // Series of the general expression below around the axis, where the two
    // nearly equal elliptic integrals of that expression cancel each other.
    const double zpp = zp2 + 1.0;
    const double zmm = zm2 + 1.0;
    const double sqrt_p = std::sqrt(zpp);
    const double sqrt_m = std::sqrt(zmm);

    const double frac1 = zp / sqrt_p;
    const double frac2 = zm / sqrt_m;

    const double r3 = r2 * r;
    const double r4 = r3 * r;
    const double r5 = r4 * r;

    const double term1 = frac1 - frac2;
    const double term2 = (frac1 / (zpp * zpp) - frac2 / (zmm * zmm)) * r2 / 8.0;
    const double term3 = ((3.0 - 4.0 * zp2) * frac1 / std::pow(zpp, 4)
                          - (3.0 - 4.0 * zm2) * frac2 / std::pow(zmm, 4))
                         / 64.0 * r4;

    h_r = -std::cos(phi) / 4.0 * (term1 + 9.0 * term2 + 25.0 * term3);
    h_phi = std::sin(phi) / 4.0 * (term1 + 3.0 * term2 + 5.0 * term3);
    h_z = -std::cos(phi) / 4.0
          * (r * (1.0 / zpp / sqrt_p - 1.0 / zmm / sqrt_m)
             + 3.0 / 8.0 * r3
                 * ((1.0 - 4.0 * zp2) / std::pow(zpp, 3) / sqrt_p
                    - (1.0 - 4.0 * zm2) / std::pow(zmm, 3) / sqrt_m)
             + 15.0 / 64.0 * r5
                 * ((1.0 - 12.0 * zp2 + 8.0 * zp2 * zp2) / std::pow(zpp, 5) / sqrt_p
                    - (1.0 - 12.0 * zm2 + 8.0 * zm2 * zm2) / std::pow(zmm, 5) / sqrt_m));

    return;
  }

  const double rp = r + 1.0;
  const double rm = r - 1.0;
  const double rp2 = rp * rp;
  const double rm2 = rm * rm;

  const double ap2 = zp2 + rm2;
  const double am2 = zm2 + rm2;
  const double ap = std::sqrt(ap2);
  const double am = std::sqrt(am2);

  const double argp = -4.0 * r / ap2;
  const double argm = -4.0 * r / am2;

  // On the hull the characteristic of the integral of the third kind runs to
  // infinity. A large finite stand in keeps the reciprocals below defined and
  // the surrounding values are stable, so only the hull itself is special.
  double argc = 1.0e16;
  double one_over_rm = 0.0;
  if (rm != 0.0) {
    argc = -4.0 * r / rm2;
    one_over_rm = 1.0 / rm;
  }

  const double kcp = std::sqrt(1.0 - argp);
  const double kcm = std::sqrt(1.0 - argm);

  const double elle_p = completeEllipticIntegral(kcp, 1.0, 1.0, kcp * kcp);
  const double elle_m = completeEllipticIntegral(kcm, 1.0, 1.0, kcm * kcm);
  const double ellk_p = completeEllipticIntegral(kcp, 1.0, 1.0, 1.0);
  const double ellk_m = completeEllipticIntegral(kcm, 1.0, 1.0, 1.0);
  const double ellpi_p = completeEllipticIntegral(kcp, 1.0 - argc, 1.0, 1.0);
  const double ellpi_m = completeEllipticIntegral(kcm, 1.0 - argc, 1.0, 1.0);

  h_r = -std::cos(phi) / (4.0 * M_PI * r2)
        * (-zm * am * elle_m
           + zp * ap * elle_p
           + zm / am * (2.0 + zm2) * ellk_m
           - zp / ap * (2.0 + zp2) * ellk_p
           + (zm / am * ellpi_m - zp / ap * ellpi_p) * rp * (r2 + 1.0) * one_over_rm);

  h_phi = std::sin(phi) / (4.0 * M_PI * r2)
          * (zm * am * elle_m
             - zp * ap * elle_p
             - zm / am * (2.0 + zm2 + 2.0 * r2) * ellk_m
             + zp / ap * (2.0 + zp2 + 2.0 * r2) * ellk_p
             + zm / am * rp2 * ellpi_m
             - zp / ap * rp2 * ellpi_p);

  h_z = -std::cos(phi) / (2.0 * M_PI * r)
        * (am * elle_m
           - ap * elle_p
           - (1.0 + zm2 + r2) / am * ellk_m
           + (1.0 + zp2 + r2) / ap * ellk_p);
}


void greeter::CylinderMagnet::calculateMagneticFieldForAxisAlignedCylinder(
    const float diameter, const float height,
    const float* magnetization,
    const float* observation_point,
    float& result_x, float& result_y, float& result_z) {

  result_x = 0.0f;
  result_y = 0.0f;
  result_z = 0.0f;

  const double radius = (double) diameter / 2.0;
  if (radius <= 0.0 || height <= 0.0f) {
    return;
  }

  const double pol_x = (double) magnetization[0];
  const double pol_y = (double) magnetization[1];
  const double pol_z = (double) magnetization[2];

  const double pol_xy = std::sqrt(pol_x * pol_x + pol_y * pol_y);

  if (pol_xy == 0.0 && pol_z == 0.0) {
    return;
  }

  const double x = (double) observation_point[0];
  const double y = (double) observation_point[1];

  const double phi = std::atan2(y, x);

  // Both closed forms are scale invariant, so everything is measured in
  // cylinder radii.
  const double r = std::sqrt(x * x + y * y) / radius;
  const double z = (double) observation_point[2] / radius;
  const double z0 = (double) height / 2.0 / radius;

  // The field diverges on the rim where the hull meets a base.
  const bool on_hull = std::fabs(r - 1.0) <= 1.0e-14;
  const bool on_bases = std::fabs(std::fabs(z) - z0) <= 1.0e-14 * z0;
  if (on_hull && on_bases) {
    return;
  }

  const bool inside = (std::fabs(z) <= z0) && (r <= 1.0);

  double b_r = 0.0;
  double b_phi = 0.0;
  double b_z = 0.0;

  if (pol_xy != 0.0) {
    // The diametral solution is written for a polarization along the local x
    // axis, so the azimuth is measured from wherever the polarization points.
    const double tetta = std::atan2(pol_y, pol_x);
    double h_r, h_phi, h_z;
    calculateDiametralHField(z0, r, z, phi - tetta, h_r, h_phi, h_z);
    b_r += h_r * pol_xy;
    b_phi += h_phi * pol_xy;
    b_z += h_z * pol_xy;
  }

  if (pol_z != 0.0) {
    double axial_r, axial_z;
    calculateAxialBField(z0, r, z, axial_r, axial_z);
    b_r += axial_r * pol_z;
    b_z += axial_z * pol_z;
  }

  double b_x = b_r * std::cos(phi) - b_phi * std::sin(phi);
  double b_y = b_r * std::sin(phi) + b_phi * std::cos(phi);

  // The diametral part returns an H-field, so inside the body the polarization
  // it is missing has to be added back to make it a B-field.
  if (inside && pol_xy != 0.0) {
    b_x += pol_x;
    b_y += pol_y;
  }

  result_x = (float) b_x;
  result_y = (float) b_y;
  result_z = (float) b_z;
}


void greeter::CylinderMagnet::computeMagneticFieldForCylinder(
    const float* parameters,
    const float* observation_point,
    float& result_x, float& result_y, float& result_z) {

  const float* position = &parameters[0];
  const float* orientation = &parameters[3];
  const float diameter = parameters[7];
  const float height = parameters[8];
  const float* magnetization = &parameters[9];

  const float translated_observation_point[3] = {
      observation_point[0] - position[0],
      observation_point[1] - position[1],
      observation_point[2] - position[2]};

  float local_observation_point[3];

  greeter::Quaternion::applyInverseRotationFromQuaternion(
      orientation, translated_observation_point, local_observation_point);

  float local_b[3];

  calculateMagneticFieldForAxisAlignedCylinder(
      diameter, height, magnetization, local_observation_point,
      local_b[0], local_b[1], local_b[2]);

  float rotated_b[3];

  greeter::Quaternion::applyRotationFromQuaternion(
      orientation, local_b, rotated_b);

  result_x = rotated_b[0];
  result_y = rotated_b[1];
  result_z = rotated_b[2];
}


void greeter::CylinderMagnet::computeMagneticField(
    const float* parameters, const float* observation_point,
    float& b_x, float& b_y, float& b_z) const {
  computeMagneticFieldForCylinder(parameters, observation_point, b_x, b_y, b_z);
}


std::vector<float> greeter::CylinderMagnet::computeMagneticField(
    double x, double y, double z) const {

  const float parameters[12] = {
      position[0], position[1], position[2],
      orientation[0], orientation[1], orientation[2], orientation[3],
      dimensions[0], dimensions[1],
      magnetization[0], magnetization[1], magnetization[2]};

  const float observation_point[3] = {(float) x, (float) y, (float) z};

  float b_x, b_y, b_z;
  computeMagneticFieldForCylinder(parameters, observation_point, b_x, b_y, b_z);

  return {b_x, b_y, b_z};
}


namespace {

  /*
    Grow the smallest entry of a triple to `min_val` while keeping the product
    of the three unchanged, so that a very flat shape still gets at least one
    division along its short side. Port of magpylib _apportion_triple.
  */
  void apportionTriple(double triple[3], const double min_val) {

    for (int iteration = 0; iteration < 30; iteration++) {

      if (triple[0] >= min_val && triple[1] >= min_val && triple[2] >= min_val) {
        break;
      }

      int amin = 0;
      int amax = 0;
      for (int i = 1; i < 3; i++) {
        if (triple[i] < triple[amin]) amin = i;
        if (triple[i] > triple[amax]) amax = i;
      }

      const double factor = min_val / triple[amin];

      if (triple[amax] >= factor * min_val) {
        const double root = std::sqrt(factor);
        for (int i = 0; i < 3; i++) triple[i] /= root;
        triple[amin] *= factor * root;
      }
    }
  }

  /*
    Split a target number of cells over the three dimensions of a shape so that
    the cells come out as close to cubes as possible. Port of magpylib
    _cells_from_dimension with its default arguments, kept faithful down to the
    order in which the rounding combinations are tried, because ties are
    resolved by whichever comes last.
  */
  void cellsFromDimension(const double dim[3], const double target_elements,
                          uint32_t result[3]) {

    const double elements = std::max(1.0, target_elements);

    const double x = std::fabs(dim[0]);
    const double y = std::fabs(dim[1]);
    const double z = std::fabs(dim[2]);

    double triple[3] = {
        std::pow(x, 2.0 / 3.0) * std::cbrt(elements / y / z),
        std::pow(y, 2.0 / 3.0) * std::cbrt(elements / x / z),
        std::pow(z, 2.0 / 3.0) * std::cbrt(elements / x / y)};

    apportionTriple(triple, 1.0);

    double epsilon = elements;

    double best[3] = {std::ceil(triple[0]), std::ceil(triple[1]), std::ceil(triple[2])};

    for (int combination = 0; combination < 8; combination++) {

      double candidate[3];
      for (int i = 0; i < 3; i++) {
        const bool use_floor = ((combination >> (2 - i)) & 1) != 0;
        candidate[i] = use_floor ? std::floor(triple[i]) : std::ceil(triple[i]);
      }

      const double difference =
          elements - candidate[0] * candidate[1] * candidate[2];

      if (std::fabs(difference) <= epsilon
          && candidate[0] >= 1.0 && candidate[1] >= 1.0 && candidate[2] >= 1.0) {
        epsilon = std::fabs(difference);
        best[0] = candidate[0];
        best[1] = candidate[1];
        best[2] = candidate[2];
      }
    }

    for (int i = 0; i < 3; i++) {
      result[i] = (uint32_t) std::max(1.0, best[i]);
    }
  }

}  // namespace


greeter::TargetMeshData greeter::CylinderMagnet::generateTargetMesh(
    const float* parameters, const greeter::MeshingSpec& meshing) {

  const double outer_radius = (double) parameters[7] / 2.0;
  const double height = (double) parameters[8];
  const float* polarization = &parameters[9];

  greeter::TargetMeshData mesh;

  if (outer_radius <= 0.0 || height <= 0.0) {
    return mesh;
  }

  // A cylinder has no three axes to split along, so an explicit split is read
  // as the total it asks for, as it is for a tetrahedron.
  double target = (double) meshing.total;
  if (meshing.explicit_split) {
    target = (double) std::max(1u, meshing.n[0])
           * (double) std::max(1u, meshing.n[1])
           * (double) std::max(1u, meshing.n[2]);
  }

  // Unroll the hull and split the target over circumference, radius and
  // height. The 3.14 is magpylib's, and is kept so that the two libraries
  // produce the same number of cells for the same request. Note that the
  // apportioning never gives a dimension fewer than one division, so a
  // cylinder asked for a single cell comes back with more than one, unlike the
  // other shapes.
  const double arc_length = outer_radius * 3.14;
  const double dim[3] = {arc_length, outer_radius, height};

  uint32_t counts[3];
  cellsFromDimension(dim, target, counts);

  const uint32_t n_phi = counts[0];
  const uint32_t n_r = counts[1];
  const uint32_t n_h = counts[2];

  const double radial_step = outer_radius / (double) n_r;
  const double height_step = height / (double) n_h;

  const double moment_scale = 1.0 / (double) greeter::MU0;

  for (uint32_t r_index = 0; r_index < n_r; r_index++) {

    const double inner = (double) r_index * radial_step;
    const double outer = (r_index + 1 == n_r) ? outer_radius
                                              : (double) (r_index + 1) * radial_step;

    // More divisions the further out, so that the cells keep a similar area.
    const uint32_t n_phi_ring =
        std::max(1u, (uint32_t) (outer / (outer_radius / 2.0) * (double) n_phi));

    for (uint32_t h_index = 0; h_index < n_h; h_index++) {

      const double z = height_step * (double) h_index - height / 2.0 + height_step / 2.0;

      if (n_r >= 3 && r_index == 0) {
        // The innermost cells are a full little cylinder rather than a ring of
        // slivers that would all sit almost on the axis.
        greeter::MeshCell cell;
        cell.point[0] = 0.0f;
        cell.point[1] = 0.0f;
        cell.point[2] = (float) z;

        const double volume = M_PI * outer * outer * height_step;

        cell.moment[0] = (float) (volume * moment_scale * polarization[0]);
        cell.moment[1] = (float) (volume * moment_scale * polarization[1]);
        cell.moment[2] = (float) (volume * moment_scale * polarization[2]);

        mesh.push_back(cell);
        continue;
      }

      const double radial_coordinate = (inner + outer) / 2.0;
      const double volume =
          M_PI * (outer * outer - inner * inner) * height_step / (double) n_phi_ring;

      for (uint32_t phi_index = 0; phi_index < n_phi_ring; phi_index++) {

        const double angle =
            ((double) phi_index + 0.5) * 2.0 * M_PI / (double) n_phi_ring;

        greeter::MeshCell cell;
        cell.point[0] = (float) (radial_coordinate * std::cos(angle));
        cell.point[1] = (float) (radial_coordinate * std::sin(angle));
        cell.point[2] = (float) z;

        cell.moment[0] = (float) (volume * moment_scale * polarization[0]);
        cell.moment[1] = (float) (volume * moment_scale * polarization[1]);
        cell.moment[2] = (float) (volume * moment_scale * polarization[2]);

        mesh.push_back(cell);
      }
    }
  }

  return mesh;
}


greeter::view::ShapeDescriptor greeter::CylinderMagnet::describeShape(
    const float* parameters) {

  greeter::view::ShapeDescriptor shape;

  shape.kind = greeter::view::ShapeKind::Cylinder;
  shape.parameters = {parameters[7], parameters[8]};

  return shape;
}


void greeter::CylinderMagnet::computePolarizationForCylinder(
    const float* parameters, const float* observation_point,
    float& j_x, float& j_y, float& j_z) {

  j_x = 0.0f;
  j_y = 0.0f;
  j_z = 0.0f;

  const float* position = &parameters[0];
  const float* orientation = &parameters[3];
  const float diameter = parameters[7];
  const float height = parameters[8];
  const float* magnetization = &parameters[9];

  const float translated[3] = {
    observation_point[0] - position[0],
    observation_point[1] - position[1],
    observation_point[2] - position[2]
  };

  float local[3];

  greeter::Quaternion::applyInverseRotationFromQuaternion(
    orientation, translated, local);

  if (std::fabs(local[2]) > 0.5f * height) {
    return;
  }

  if (std::sqrt(local[0] * local[0] + local[1] * local[1]) > 0.5f * diameter) {
    return;
  }

  float turned[3];

  greeter::Quaternion::applyRotationFromQuaternion(
    orientation, magnetization, turned);

  j_x = turned[0];
  j_y = turned[1];
  j_z = turned[2];
}

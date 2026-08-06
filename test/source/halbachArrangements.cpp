#include <doctest/doctest.h>
#include <greeter/greeter.h>
#include <greeter/version.h>
#include <greeter/MagnetCollection.h>
#include <greeter/io/ArrangementFactoryIO.h>
#include <greeter/io/SceneIO.h>

#include <nlohmann/json.hpp>
#include <cmath>
#include <string>

/*
  The rings and the row.

  What is worth checking about one of these is the placement it works out, and
  then the property the arrangement exists for: a Halbach ring of order one
  makes a field inside it and next to none outside, and a linear Halbach row
  makes a field on one side and next to none on the other. Neither needs a
  reference to check against, because both are statements about the arrangement
  rather than about a number.
*/

namespace {

  const float SQRT_HALF = 0.70710678f;

  const char* FIELD_OF_VIEW = R"({
    "x": { "min": 0, "max": 1, "n": 2 },
    "y": { "min": 0, "max": 1, "n": 2 },
    "z": { "min": 0, "max": 1, "n": 2 }
  })";

  nlohmann::json fileWith(const std::string& type, const nlohmann::json& parameters) {
    nlohmann::json data;
    data["arrangements"] = nlohmann::json::array();
    data["arrangements"].push_back({{"id", 1}, {"type", type}, {"parameters", parameters}});
    data["field_of_view"] = nlohmann::json::parse(FIELD_OF_VIEW);
    return data;
  }

  nlohmann::json cuboidElement(const nlohmann::json& dimensions,
                               const nlohmann::json& magnetization) {
    return {
      {"type", "cuboid"},
      {"parameters", {{"dimensions", dimensions}, {"magnetization", magnetization}}}
    };
  }

  greeter::MagnetCollection build(const std::string& type, const nlohmann::json& parameters) {
    return std::move(greeter::SceneIO::read(fileWith(type, parameters)).collection);
  }

  std::vector<float> positionOf(const greeter::MagnetCollection& collection, size_t index) {
    const std::vector<float> parameters = collection.getMagnetParameters(index);
    return {parameters[0], parameters[1], parameters[2]};
  }

  std::vector<float> orientationOf(const greeter::MagnetCollection& collection, size_t index) {
    const std::vector<float> parameters = collection.getMagnetParameters(index);
    return {parameters[3], parameters[4], parameters[5], parameters[6]};
  }

  void checkVector(const std::vector<float>& given, const std::vector<float>& expected) {
    REQUIRE(given.size() == expected.size());
    for (size_t i = 0; i < expected.size(); i++) {
      CHECK(given[i] == doctest::Approx(expected[i]).epsilon(1e-5).scale(1.0));
    }
  }

  float magnitude(const std::vector<float>& field) {
    return std::sqrt(field[0] * field[0] + field[1] * field[1] + field[2] * field[2]);
  }

  // The strength of the field summed over a line of points, which is what says
  // which side of a Halbach row or ring the field is on.
  float fieldStrengthAlong(const greeter::MagnetCollection& collection,
                           const std::vector<std::vector<float>>& points) {
    float total = 0.0f;
    for (const auto& field : collection.simulate(points)) {
      total += magnitude(field);
    }
    return total;
  }

}  // namespace


TEST_CASE("A Halbach ring turns each member by its angle round the ring") {

  const greeter::MagnetCollection ring = build("halbach_ring", {
    {"radius", 0.3},
    {"count", 4},
    {"element", cuboidElement({0.12, 0.12, 0.12}, {0, 5, 0})}
  });

  REQUIRE(ring.get_num_magnets() == 4);

  SUBCASE("the members sit on the circle, starting on the local x axis") {

    checkVector(positionOf(ring, 0), { 0.3f,  0.0f, 0.0f});
    checkVector(positionOf(ring, 1), { 0.0f,  0.3f, 0.0f});
    checkVector(positionOf(ring, 2), {-0.3f,  0.0f, 0.0f});
    checkVector(positionOf(ring, 3), { 0.0f, -0.3f, 0.0f});
  }

  SUBCASE("order one turns a member by twice the angle at which it sits") {

    // A quarter of the way round the ring is half a turn of the polarization,
    // which is what makes the field of the ring uniform inside it.
    checkVector(orientationOf(ring, 0), { 1.0f, 0.0f, 0.0f,  0.0f});
    checkVector(orientationOf(ring, 1), { 0.0f, 0.0f, 0.0f,  1.0f});
    checkVector(orientationOf(ring, 2), {-1.0f, 0.0f, 0.0f,  0.0f});
    checkVector(orientationOf(ring, 3), { 0.0f, 0.0f, 0.0f, -1.0f});
  }

  SUBCASE("the order has to be a whole number") {

    // The polarization has to come back to itself after a full turn round the
    // ring, which a fractional order would not do.
    CHECK_THROWS_AS(build("halbach_ring", {
      {"radius", 0.3}, {"count", 4}, {"order", 1.5},
      {"element", cuboidElement({0.12, 0.12, 0.12}, {0, 5, 0})}
    }), std::invalid_argument);
  }

  SUBCASE("a ring needs a radius it can sit on") {

    CHECK_THROWS_AS(build("halbach_ring", {
      {"count", 4}, {"element", cuboidElement({0.12, 0.12, 0.12}, {0, 5, 0})}
    }), std::invalid_argument);

    CHECK_THROWS_AS(build("halbach_ring", {
      {"radius", 0.0}, {"count", 4},
      {"element", cuboidElement({0.12, 0.12, 0.12}, {0, 5, 0})}
    }), std::invalid_argument);

    CHECK_THROWS_AS(build("halbach_ring", {
      {"radius", -0.3}, {"count", 4},
      {"element", cuboidElement({0.12, 0.12, 0.12}, {0, 5, 0})}
    }), std::invalid_argument);
  }
}


TEST_CASE("The Halbach ring of the input file is the one the library builds") {

  // MagnetCollection::generateCircularHalbachArray is this arrangement, so the
  // ring the C++ interface makes and the ring the file asks for have to be the
  // same magnets. The numbers on the far side of this are the ones the
  // circular Halbach array test has always checked.
  const greeter::MagnetCollection from_the_library =
    greeter::MagnetCollection::generateCircularHalbachArray(
      0.3f, {0.12f, 0.12f, 0.12f}, 4, {0.0f, 5.0f, 0.0f});

  const greeter::MagnetCollection from_the_file = build("halbach_ring", {
    {"radius", 0.3},
    {"count", 4},
    {"order", 1},
    {"element", cuboidElement({0.12, 0.12, 0.12}, {0, 5, 0})}
  });

  REQUIRE(from_the_library.get_num_magnets() == from_the_file.get_num_magnets());

  for (size_t i = 0; i < 4; i++) {
    checkVector(positionOf(from_the_library, i), positionOf(from_the_file, i));
    checkVector(orientationOf(from_the_library, i), orientationOf(from_the_file, i));
  }

  const std::vector<std::vector<float>> points = {
    {-0.16666667f, -0.16666667f, 0.0f},
    {-0.5f, -0.16666667f, 0.0f}
  };

  const std::vector<std::vector<float>> field = from_the_file.simulate(points);

  CHECK(field[0][0] == doctest::Approx(0.0).scale(0.1));
  CHECK(field[0][1] == doctest::Approx(0.03494169));
  CHECK(field[0][2] == doctest::Approx(0.0).scale(0.1));

  CHECK(field[1][0] == doctest::Approx(0.05888743));
  CHECK(field[1][1] == doctest::Approx(0.0107326));
  CHECK(field[1][2] == doctest::Approx(0.0).scale(0.1));
}


TEST_CASE("A Halbach ring of order one keeps its field inside itself") {

  // Sixteen cuboids on a ring of radius 0.3, polarized along their own y.
  const greeter::MagnetCollection ring = build("halbach_ring", {
    {"radius", 0.3},
    {"count", 16},
    {"order", 1},
    {"element", cuboidElement({0.1, 0.1, 0.1}, {0, 1, 0})}
  });

  REQUIRE(ring.get_num_magnets() == 16);

  // Well inside the bore. A ring of sixteen is uniform to about two percent
  // out to a tenth of its radius and to fifteen percent by a quarter of it,
  // so how uniform it is only means anything together with where it is asked.
  const std::vector<std::vector<float>> inside = {
    { 0.00f,  0.00f, 0.0f},
    { 0.03f,  0.00f, 0.0f},
    {-0.03f,  0.00f, 0.0f},
    { 0.00f,  0.03f, 0.0f},
    { 0.00f, -0.03f, 0.0f}
  };

  const std::vector<std::vector<float>> outside = {
    { 0.90f,  0.00f, 0.0f},
    {-0.90f,  0.00f, 0.0f},
    { 0.00f,  0.90f, 0.0f},
    { 0.00f, -0.90f, 0.0f}
  };

  const std::vector<std::vector<float>> field_inside = ring.simulate(inside);
  const std::vector<std::vector<float>> field_outside = ring.simulate(outside);

  SUBCASE("the field inside is uniform and lies along the axis of the element") {

    const float at_center = field_inside[0][1];

    // The element is polarized along its own y, and the ring turns that into a
    // field along y. Which way along it is decided by the ring rather than by
    // the element: an element polarized along +y gives a field along -y. This
    // agrees with magpylib, which puts -0.070679 T at the middle of this ring.
    CHECK(at_center < 0.0f);
    CHECK(at_center == doctest::Approx(-0.070679).epsilon(1e-4));

    for (const auto& field : field_inside) {
      CHECK(field[1] == doctest::Approx(at_center).epsilon(0.03));
      CHECK(std::fabs(field[0]) < 0.03f * std::fabs(at_center));
      CHECK(std::fabs(field[2]) < 0.03f * std::fabs(at_center));
    }
  }

  SUBCASE("the field outside all but cancels") {

    const float at_center = magnitude(field_inside[0]);

    for (const auto& field : field_outside) {
      CHECK(magnitude(field) < 0.02f * at_center);
    }
  }
}


TEST_CASE("A circular array is a ring that does not turn its members") {

  SUBCASE("facing the axis leaves every member pointing the same way") {

    const greeter::MagnetCollection ring = build("circular_array", {
      {"radius", 0.3}, {"count", 4},
      {"element", cuboidElement({0.1, 0.1, 0.1}, {0, 0, 1})}
    });

    for (size_t i = 0; i < 4; i++) {
      checkVector(orientationOf(ring, i), {1.0f, 0.0f, 0.0f, 0.0f});
    }
  }

  SUBCASE("facing the center carries the frame of a member round with it") {

    const greeter::MagnetCollection ring = build("circular_array", {
      {"radius", 0.3}, {"count", 4}, {"face", "center"},
      {"element", cuboidElement({0.1, 0.1, 0.1}, {1, 0, 0})}
    });

    // Turned by the angle at which it sits, so the local x axis of a member
    // points away from the middle.
    checkVector(orientationOf(ring, 0), {1.0f,       0.0f, 0.0f, 0.0f});
    checkVector(orientationOf(ring, 1), {SQRT_HALF,  0.0f, 0.0f, SQRT_HALF});
    checkVector(orientationOf(ring, 2), {0.0f,       0.0f, 0.0f, 1.0f});
    checkVector(orientationOf(ring, 3), {-SQRT_HALF, 0.0f, 0.0f, SQRT_HALF});
  }

  SUBCASE("a ring of magnets pointing along the axis is symmetric about it") {

    const greeter::MagnetCollection ring = build("circular_array", {
      {"radius", 0.3}, {"count", 6},
      {"element", cuboidElement({0.1, 0.1, 0.1}, {0, 0, 1})}
    });

    const std::vector<std::vector<float>> field =
      ring.simulate(std::vector<std::vector<float>>{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.2f}});

    // On the axis of a ring that is symmetric about it, nothing points sideways.
    for (const auto& point : field) {
      CHECK(std::fabs(point[0]) < 1e-4f * std::fabs(point[2]));
      CHECK(std::fabs(point[1]) < 1e-4f * std::fabs(point[2]));
    }
  }

  SUBCASE("the two rings are the same ring underneath") {

    // A circular array is a Halbach ring that turns its members not at all or
    // exactly once, so asking for it either way has to give the same magnets.
    const greeter::MagnetCollection facing_axis = build("circular_array", {
      {"radius", 0.3}, {"count", 5}, {"face", "axis"},
      {"element", cuboidElement({0.1, 0.1, 0.1}, {0, 1, 0})}
    });

    const greeter::MagnetCollection order_minus_one = build("halbach_ring", {
      {"radius", 0.3}, {"count", 5}, {"order", -1},
      {"element", cuboidElement({0.1, 0.1, 0.1}, {0, 1, 0})}
    });

    const greeter::MagnetCollection facing_center = build("circular_array", {
      {"radius", 0.3}, {"count", 5}, {"face", "center"},
      {"element", cuboidElement({0.1, 0.1, 0.1}, {0, 1, 0})}
    });

    const greeter::MagnetCollection order_zero = build("halbach_ring", {
      {"radius", 0.3}, {"count", 5}, {"order", 0},
      {"element", cuboidElement({0.1, 0.1, 0.1}, {0, 1, 0})}
    });

    for (size_t i = 0; i < 5; i++) {
      checkVector(orientationOf(facing_axis, i), orientationOf(order_minus_one, i));
      checkVector(orientationOf(facing_center, i), orientationOf(order_zero, i));
    }
  }

  SUBCASE("each ring refuses the parameter that belongs to the other") {

    CHECK_THROWS_AS(build("circular_array", {
      {"radius", 0.3}, {"count", 4}, {"order", 2},
      {"element", cuboidElement({0.1, 0.1, 0.1}, {0, 1, 0})}
    }), std::invalid_argument);

    CHECK_THROWS_AS(build("halbach_ring", {
      {"radius", 0.3}, {"count", 4}, {"face", "center"},
      {"element", cuboidElement({0.1, 0.1, 0.1}, {0, 1, 0})}
    }), std::invalid_argument);

    CHECK_THROWS_AS(build("circular_array", {
      {"radius", 0.3}, {"count", 4}, {"face", "sideways"},
      {"element", cuboidElement({0.1, 0.1, 0.1}, {0, 1, 0})}
    }), std::invalid_argument);
  }
}


TEST_CASE("A linear Halbach row sweeps its polarization along itself") {

  const greeter::MagnetCollection row = build("halbach_linear", {
    {"count", 4},
    {"spacing", 0.02},
    {"steps_per_period", 4},
    {"element", cuboidElement({0.02, 0.02, 0.02}, {0, 0, 1})}
  });

  REQUIRE(row.get_num_magnets() == 4);

  SUBCASE("the members lie in a row, centred on the arrangement") {

    checkVector(positionOf(row, 0), {-0.03f, 0.0f, 0.0f});
    checkVector(positionOf(row, 1), {-0.01f, 0.0f, 0.0f});
    checkVector(positionOf(row, 2), { 0.01f, 0.0f, 0.0f});
    checkVector(positionOf(row, 3), { 0.03f, 0.0f, 0.0f});
  }

  SUBCASE("four members to a period is a quarter turn each") {

    // Turned about the local y axis, so a polarization along the local z axis
    // sweeps through the xz plane.
    checkVector(orientationOf(row, 0), { 1.0f,      0.0f, 0.0f,      0.0f});
    checkVector(orientationOf(row, 1), { SQRT_HALF, 0.0f, SQRT_HALF, 0.0f});
    checkVector(orientationOf(row, 2), { 0.0f,      0.0f, 1.0f,      0.0f});
    checkVector(orientationOf(row, 3), {-SQRT_HALF, 0.0f, SQRT_HALF, 0.0f});
  }

  SUBCASE("the first member is never turned, however the row is placed") {

    const greeter::MagnetCollection uncentered = build("halbach_linear", {
      {"count", 4}, {"spacing", 0.02}, {"steps_per_period", 4}, {"centered", false},
      {"element", cuboidElement({0.02, 0.02, 0.02}, {0, 0, 1})}
    });

    checkVector(positionOf(uncentered, 0), {0.0f, 0.0f, 0.0f});

    for (size_t i = 0; i < 4; i++) {
      checkVector(orientationOf(uncentered, i), orientationOf(row, i));
    }
  }

  SUBCASE("a wavelength is the members per period times the spacing") {

    const greeter::MagnetCollection by_wavelength = build("halbach_linear", {
      {"count", 4}, {"spacing", 0.02}, {"wavelength", 0.08},
      {"element", cuboidElement({0.02, 0.02, 0.02}, {0, 0, 1})}
    });

    for (size_t i = 0; i < 4; i++) {
      checkVector(positionOf(by_wavelength, i), positionOf(row, i));
      checkVector(orientationOf(by_wavelength, i), orientationOf(row, i));
    }
  }

  SUBCASE("the period has to be given exactly one way") {

    CHECK_THROWS_AS(build("halbach_linear", {
      {"count", 4}, {"spacing", 0.02},
      {"element", cuboidElement({0.02, 0.02, 0.02}, {0, 0, 1})}
    }), std::invalid_argument);

    CHECK_THROWS_AS(build("halbach_linear", {
      {"count", 4}, {"spacing", 0.02}, {"steps_per_period", 4}, {"wavelength", 0.08},
      {"element", cuboidElement({0.02, 0.02, 0.02}, {0, 0, 1})}
    }), std::invalid_argument);

    CHECK_THROWS_AS(build("halbach_linear", {
      {"count", 4}, {"spacing", 0.02}, {"steps_per_period", 0},
      {"element", cuboidElement({0.02, 0.02, 0.02}, {0, 0, 1})}
    }), std::invalid_argument);
  }
}


TEST_CASE("A linear Halbach row puts its field on one side") {

  // The property the arrangement exists for. Twelve touching cuboids, four to
  // a period, sampled along a line above the row and the same line below it.
  const nlohmann::json parameters = {
    {"count", 12},
    {"spacing", 0.02},
    {"steps_per_period", 4},
    {"element", cuboidElement({0.02, 0.02, 0.02}, {0, 0, 1})}
  };

  // With a positive number of members to a period, the strong side is the one
  // the local z axis points away from. magpylib puts the sum of |B| over these
  // nine points at 1.045674 T below the row and 0.12328 T above it.
  std::vector<std::vector<float>> strong_side;
  std::vector<std::vector<float>> weak_side;

  for (int i = -4; i <= 4; i++) {
    const float x = 0.01f * (float) i;
    strong_side.push_back({x, 0.0f, -0.025f});
    weak_side.push_back({x, 0.0f, 0.025f});
  }

  const greeter::MagnetCollection row = build("halbach_linear", parameters);

  const float strong = fieldStrengthAlong(row, strong_side);
  const float weak = fieldStrengthAlong(row, weak_side);

  SUBCASE("one side is far stronger than the other") {

    CHECK(strong == doctest::Approx(1.045674).epsilon(1e-4));
    CHECK(weak == doctest::Approx(0.12328).epsilon(1e-4));

    CHECK(strong > 5.0f * weak);
  }

  SUBCASE("reversing the period swaps the side the field is on") {

    nlohmann::json reversed = parameters;
    reversed["steps_per_period"] = -4;

    const greeter::MagnetCollection other_way = build("halbach_linear", reversed);

    // The row is the mirror of the first one, so the two sides trade places.
    CHECK(fieldStrengthAlong(other_way, weak_side) == doctest::Approx(strong).epsilon(1e-4));
    CHECK(fieldStrengthAlong(other_way, strong_side) == doctest::Approx(weak).epsilon(1e-4));
  }
}


TEST_CASE("Every arrangement is reachable through the registry") {

  const std::vector<std::string> types = {
    "linear_array", "circular_array", "halbach_ring", "halbach_linear"
  };

  for (const auto& type : types) {
    CAPTURE(type);
    CHECK(greeter::ArrangementFactoryIO::getInstance().isRegistered(type));
  }
}

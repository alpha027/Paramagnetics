#include <doctest/doctest.h>
#include <greeter/FieldOfView.h>
#include <greeter/MagnetCollection.h>
#include <greeter/io/FieldOfViewIO.h>
#include <greeter/io/MagnetIO.h>
#include <cmath>


TEST_CASE("A field of view sampled along three axes lays out a grid") {

  const greeter::FieldOfView fov({0.0f, 1.0f, 0.0f, 2.0f, 0.0f, 4.0f}, {2, 3, 5});

  CHECK(fov.isGrid());
  CHECK(fov.getNumPoints() == 2 * 3 * 5);

  const std::vector<uint32_t> counts = fov.getCounts();
  CHECK(counts[0] == 2);
  CHECK(counts[1] == 3);
  CHECK(counts[2] == 5);

  const std::vector<float> spacing = fov.getSpacing();
  CHECK(spacing[0] == doctest::Approx(1.0f));
  CHECK(spacing[1] == doctest::Approx(1.0f));
  CHECK(spacing[2] == doctest::Approx(1.0f));

  const std::vector<float> bounds = fov.getBounds();
  CHECK(bounds[0] == doctest::Approx(0.0f));
  CHECK(bounds[1] == doctest::Approx(1.0f));
  CHECK(bounds[5] == doctest::Approx(4.0f));
}


TEST_CASE("Grid points come out with x slowest and z fastest") {

  const greeter::FieldOfView fov({0.0f, 1.0f, 0.0f, 2.0f, 0.0f, 4.0f}, {2, 3, 5});

  const std::vector<std::vector<float>> points = fov.getFOV();

  // Every point sits where getIndex says it does, which is the only thing a
  // reader of the result has to go on.
  for (uint32_t i = 0; i < 2; i++) {
    for (uint32_t j = 0; j < 3; j++) {
      for (uint32_t k = 0; k < 5; k++) {

        const size_t index = fov.getIndex(i, j, k);

        CHECK(points[index][0] == doctest::Approx((float) i));
        CHECK(points[index][1] == doctest::Approx((float) j));
        CHECK(points[index][2] == doctest::Approx((float) k));
      }
    }
  }

  // Neighbours along z are next to each other, neighbours along x are a
  // whole plane apart.
  CHECK(fov.getIndex(0, 0, 1) == 1);
  CHECK(fov.getIndex(0, 1, 0) == 5);
  CHECK(fov.getIndex(1, 0, 0) == 15);

  CHECK_THROWS_AS(fov.getIndex(2, 0, 0), std::out_of_range);
  CHECK_THROWS_AS(fov.getIndex(0, 3, 0), std::out_of_range);
  CHECK_THROWS_AS(fov.getIndex(0, 0, 5), std::out_of_range);
}


TEST_CASE("An axis sampled once is a plane, not a division by zero") {

  // The single most common thing to ask a viewer for: the xy plane at z = 0.
  const greeter::FieldOfView fov({-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f}, {3, 3, 1});

  CHECK(fov.getNumPoints() == 9);

  const std::vector<std::vector<float>> points = fov.getFOV();

  for (const auto& point : points) {
    for (const auto& value : point) {
      CHECK_FALSE(std::isnan(value));
      CHECK_FALSE(std::isinf(value));
    }
    CHECK(point[2] == doctest::Approx(0.0f));
  }

  // The sample of an axis taken once sits at its minimum, as linspace does.
  CHECK(fov.getSpacing()[2] == doctest::Approx(0.0f));

  const greeter::FieldOfView offset({0.0f, 0.0f, 0.0f, 0.0f, 2.0f, 7.0f}, {1, 1, 2});

  CHECK(offset.getFOV()[0][2] == doctest::Approx(2.0f));
  CHECK(offset.getFOV()[1][2] == doctest::Approx(7.0f));
}


TEST_CASE("A plane of a field of view still simulates") {

  // Before the counts were allowed to be one, this produced NaN coordinates
  // and so a NaN field, without anything being reported as wrong.
  const greeter::MagnetCollection collection = greeter::MagnetIO::read(
    nlohmann::json::parse(R"({
      "magnets": [
        { "id": 1, "type": "cuboid", "parameters": {
            "dimensions": [0.02, 0.02, 0.02], "magnetization": [0, 0, 1],
            "position": [0, 0, 0], "orientation": [1, 0, 0, 0] } }
      ],
      "field_of_view": {
        "x": {"min": -0.05, "max": 0.05, "n": 5},
        "y": {"min": -0.05, "max": 0.05, "n": 5},
        "z": {"min":  0.05, "max": 0.05, "n": 1}
      }
    })"));

  const greeter::FieldOfView plane(
    {-0.05f, 0.05f, -0.05f, 0.05f, 0.05f, 0.05f}, {5, 5, 1});

  const std::vector<std::vector<float>> field = collection.simulate(plane);

  REQUIRE(field.size() == 25);

  for (const auto& sample : field) {
    for (const auto& component : sample) {
      CHECK_FALSE(std::isnan(component));
    }
  }

  // On the axis of a cube polarized along z, above it, the field is along +z.
  const size_t center = plane.getIndex(2, 2, 0);
  CHECK(field[center][2] > 0.0f);
}


TEST_CASE("A field of view rejects a box it cannot sample") {

  // Nothing to simulate.
  CHECK_THROWS_AS(
    greeter::FieldOfView({0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f}, {0, 1, 1}),
    std::invalid_argument);

  // A maximum below its minimum is an input error, not a request for a grid
  // that runs backwards.
  CHECK_THROWS_AS(
    greeter::FieldOfView({1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, {2, 2, 2}),
    std::invalid_argument);

  CHECK_THROWS_AS(
    greeter::FieldOfView({0.0f, 1.0f}, {2, 2, 2}),
    std::invalid_argument);

  CHECK_THROWS_AS(
    greeter::FieldOfView({0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f}, {2, 2}),
    std::invalid_argument);
}


TEST_CASE("A list of points is not a grid and says so") {

  const greeter::FieldOfView fov(std::vector<std::vector<float>>{
    {0.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 3.0f}});

  CHECK_FALSE(fov.isGrid());
  CHECK(fov.getNumPoints() == 2);

  // Asking for a box that was never given back is a mistake worth hearing
  // about, rather than a box of zeros.
  CHECK_THROWS_AS(fov.getBounds(), std::logic_error);
  CHECK_THROWS_AS(fov.getCounts(), std::logic_error);
  CHECK_THROWS_AS(fov.getSpacing(), std::logic_error);
  CHECK_THROWS_AS(fov.getIndex(0, 0, 0), std::logic_error);
}


TEST_CASE("Replacing the points of a grid stops it being one") {

  greeter::FieldOfView fov({0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f}, {2, 2, 2});

  REQUIRE(fov.isGrid());

  // setFOV used to do nothing at all, so the points and the box could not
  // disagree. Now that it does something, the box has to go with them.
  fov.setFOV({{5.0f, 5.0f, 5.0f}});

  CHECK_FALSE(fov.isGrid());
  CHECK(fov.getNumPoints() == 1);
  CHECK(fov.getFOV()[0][0] == doctest::Approx(5.0f));
}


TEST_CASE("A copied field of view keeps its box") {

  const greeter::FieldOfView fov({0.0f, 1.0f, 0.0f, 2.0f, 0.0f, 3.0f}, {2, 3, 4});

  const greeter::FieldOfView copy(fov);

  CHECK(copy.isGrid());
  CHECK(copy.getCounts()[1] == 3);
  CHECK(copy.getBounds()[5] == doctest::Approx(3.0f));

  const std::unique_ptr<greeter::FieldOfView> cloned = fov.clone();

  REQUIRE(cloned != nullptr);
  CHECK(cloned->isGrid());
  CHECK(cloned->getNumPoints() == fov.getNumPoints());
}


TEST_CASE("The field of view section is read from plain numbers") {

  const nlohmann::json section = nlohmann::json::parse(R"({
    "x": {"min": -0.06, "max": 0.06, "n": 13},
    "y": {"min": -0.06, "max": 0.06, "n": 13},
    "z": {"min":  0.02, "max": 0.10, "n":  9}
  })");

  const greeter::FieldOfView fov = greeter::MagnetIO::readFieldOfView(section);

  CHECK(fov.isGrid());
  CHECK(fov.getNumPoints() == 13 * 13 * 9);
  CHECK(fov.getCounts()[2] == 9);
  CHECK(fov.getBounds()[4] == doctest::Approx(0.02f));

  // Both readers of the section are the same reader.
  const greeter::FieldOfView same = greeter::FieldOfViewIO::read(section);
  CHECK(same.getNumPoints() == fov.getNumPoints());

  const std::unique_ptr<greeter::FieldOfView> created =
    greeter::FieldOfViewIO::createFOV(section);
  REQUIRE(created != nullptr);
  CHECK(created->getNumPoints() == fov.getNumPoints());
}


TEST_CASE("A malformed field of view section names the axis at fault") {

  auto section = [](const std::string& z) {
    return nlohmann::json::parse(
      "{\"x\": {\"min\": 0, \"max\": 1, \"n\": 2},"
      " \"y\": {\"min\": 0, \"max\": 1, \"n\": 2},"
      " \"z\": " + z + "}");
  };

  CHECK_THROWS_AS(greeter::FieldOfViewIO::read(section("{\"min\": 0, \"max\": 1}")),
                  std::invalid_argument);
  CHECK_THROWS_AS(greeter::FieldOfViewIO::read(section("{\"min\": 0, \"n\": 2}")),
                  std::invalid_argument);
  CHECK_THROWS_AS(greeter::FieldOfViewIO::read(section("{\"min\": 0, \"max\": 1, \"n\": 0}")),
                  std::invalid_argument);
  CHECK_THROWS_AS(greeter::FieldOfViewIO::read(section("{\"min\": 0, \"max\": 1, \"n\": 1.5}")),
                  std::invalid_argument);
  CHECK_THROWS_AS(greeter::FieldOfViewIO::read(section("4")), std::invalid_argument);

  const nlohmann::json missing = nlohmann::json::parse(
    "{\"x\": {\"min\": 0, \"max\": 1, \"n\": 2}}");
  CHECK_THROWS_AS(greeter::FieldOfViewIO::read(missing), std::invalid_argument);
}

/*
  Tests for the half of the viewer that is not drawing.

  Turning a scene into triangles, following a field line, working out what a
  colour stands for and deciding what a click landed on are all ordinary
  computations, and none of them needs a window. They are tested here, and
  this target links no Qt at all, which is also a check that they have not
  quietly grown a dependency on it.
*/

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "ColorMap.h"
#include "GeometryBuilder.h"

#include <cmath>


namespace {

  /*
    The field of a dipole at the origin pointing along z, sampled on a grid.
    Analytic, so a line followed through it can be checked against what a
    dipole field actually does.
  */
  greeter::view::FieldGrid dipoleGrid(const uint32_t& count = 41,
                                      const float& reach = 0.1f) {

    greeter::view::FieldGrid field;

    field.grid = true;

    for (int axis = 0; axis < 3; axis++) {
      field.bounds[2 * axis] = -reach;
      field.bounds[2 * axis + 1] = reach;
      field.counts[axis] = count;
    }

    field.field.assign(3 * (size_t) count * count * count, 0.0f);

    for (uint32_t i = 0; i < count; i++) {
      for (uint32_t j = 0; j < count; j++) {
        for (uint32_t k = 0; k < count; k++) {

          const size_t at = field.getIndex(i, j, k);

          float point[3];
          field.getPoint(at, point);

          const float r = std::sqrt(point[0] * point[0] + point[1] * point[1] +
                                    point[2] * point[2]);

          if (r < 1e-4f) {
            continue;
          }

          // B = (3 (m . r^) r^ - m) / r^3, with m along z and the constants
          // dropped: only the shape of the field matters here.
          const float unit[3] = {point[0] / r, point[1] / r, point[2] / r};

          const float projected = unit[2];

          for (int axis = 0; axis < 3; axis++) {
            const float m = axis == 2 ? 1.0f : 0.0f;
            field.field[3 * at + axis] =
              (3.0f * projected * unit[axis] - m) / (r * r * r);
          }
        }
      }
    }

    return field;
  }

  greeter::view::SceneSnapshot oneCube(const float& size = 0.02f) {

    greeter::view::SceneSnapshot scene;

    greeter::view::MagnetView magnet;

    magnet.id = 1;
    magnet.shape.kind = greeter::view::ShapeKind::Box;
    magnet.shape.type_name = "cuboid";
    magnet.shape.parameters = {size, size, size};
    magnet.magnetization[2] = 1.0f;

    scene.magnets.push_back(magnet);

    return scene;
  }

}  // namespace


TEST_CASE("A magnet becomes triangles where it actually is") {

  greeter::view::SceneSnapshot scene = oneCube();

  scene.magnets[0].position[0] = 0.5f;

  viewer::ViewSettings settings;

  viewer::SurfaceBuffer surfaces;
  viewer::LineBuffer outlines;
  viewer::LineBuffer magnetization;
  std::vector<viewer::PickSphere> picks;

  viewer::buildMagnets(scene, settings, surfaces, outlines, magnetization, picks);

  REQUIRE_FALSE(surfaces.empty());
  REQUIRE(picks.size() == 1);

  CHECK(picks[0].id == 1);
  CHECK(picks[0].center[0] == doctest::Approx(0.5f));

  // A cube of side 0.02 reaches 0.01 from its middle along each axis, so
  // every one of its corners is within 0.011 of where it sits.
  for (size_t i = 0; i < surfaces.getVertexCount(); i++) {

    const float* point = &surfaces.data[9 * i];

    CHECK(std::fabs(point[0] - 0.5f) <= 0.011f);
    CHECK(std::fabs(point[1]) <= 0.011f);
    CHECK(std::fabs(point[2]) <= 0.011f);
  }
}


TEST_CASE("A turned magnet is drawn turned") {

  greeter::view::SceneSnapshot scene = oneCube();

  // A long thin magnet, so that a quarter turn about y is unmistakable: what
  // reached along z now reaches along x.
  scene.magnets[0].shape.parameters = {0.01f, 0.01f, 0.10f};

  const float half = std::sqrt(0.5f);

  scene.magnets[0].orientation[0] = half;  // w
  scene.magnets[0].orientation[2] = half;  // y

  viewer::ViewSettings settings;

  viewer::SurfaceBuffer surfaces;
  viewer::LineBuffer outlines;
  viewer::LineBuffer magnetization;
  std::vector<viewer::PickSphere> picks;

  viewer::buildMagnets(scene, settings, surfaces, outlines, magnetization, picks);

  float reach[3] = {0.0f, 0.0f, 0.0f};

  for (size_t i = 0; i < surfaces.getVertexCount(); i++) {
    for (int axis = 0; axis < 3; axis++) {
      reach[axis] = std::max(reach[axis], std::fabs(surfaces.data[9 * i + axis]));
    }
  }

  CHECK(reach[0] == doctest::Approx(0.05f).epsilon(0.01));
  CHECK(reach[2] == doctest::Approx(0.005f).epsilon(0.01));
}


TEST_CASE("A magnet nobody can draw is still marked where it is") {

  greeter::view::SceneSnapshot scene = oneCube();

  scene.magnets[0].shape.kind = greeter::view::ShapeKind::Unknown;
  scene.magnets[0].shape.parameters.clear();
  scene.magnets[0].position[1] = 0.3f;

  viewer::ViewSettings settings;

  viewer::SurfaceBuffer surfaces;
  viewer::LineBuffer outlines;
  viewer::LineBuffer magnetization;
  std::vector<viewer::PickSphere> picks;

  viewer::buildMagnets(scene, settings, surfaces, outlines, magnetization, picks);

  // No triangles, but a cross where it sits and something to click on, so a
  // scene with a magnet type the viewer has never met still shows all of it.
  CHECK(surfaces.empty());
  CHECK_FALSE(outlines.empty());
  REQUIRE(picks.size() == 1);
  CHECK(picks[0].radius > 0.0f);
}


TEST_CASE("Turning magnets off draws none of them") {

  viewer::ViewSettings settings;
  settings.show_magnets = false;

  viewer::SurfaceBuffer surfaces;
  viewer::LineBuffer outlines;
  viewer::LineBuffer magnetization;
  std::vector<viewer::PickSphere> picks;

  viewer::buildMagnets(oneCube(), settings, surfaces, outlines, magnetization, picks);

  CHECK(surfaces.empty());
  CHECK(outlines.empty());

  // And nothing invisible left to click on.
  CHECK(picks.empty());
}


TEST_CASE("A click finds the magnet in front of it") {

  std::vector<viewer::PickSphere> spheres;

  viewer::PickSphere near_one;
  near_one.id = 7;
  near_one.center[2] = 1.0f;
  near_one.radius = 0.2f;

  viewer::PickSphere far_one;
  far_one.id = 9;
  far_one.center[2] = 5.0f;
  far_one.radius = 0.2f;

  spheres.push_back(far_one);
  spheres.push_back(near_one);

  const float origin[3] = {0.0f, 0.0f, 0.0f};
  const float along_z[3] = {0.0f, 0.0f, 1.0f};

  // Both are on the ray, and the near one is what was clicked.
  CHECK(viewer::pick(spheres, origin, along_z) == 7);

  const float away[3] = {0.0f, 0.0f, -1.0f};
  CHECK(viewer::pick(spheres, origin, away) == 0);

  const float sideways[3] = {1.0f, 0.0f, 0.0f};
  CHECK(viewer::pick(spheres, origin, sideways) == 0);

  CHECK(viewer::pick({}, origin, along_z) == 0);
}


TEST_CASE("A slice covers the plane it is asked for") {

  const greeter::view::FieldGrid field = dipoleGrid(21);

  viewer::ViewSettings settings;
  settings.slice_axis = 2;
  settings.slice_position = 0.5f;
  settings.slice_resolution = 8;

  const viewer::ColorRange range = viewer::computeRange(field, settings);

  viewer::SurfaceBuffer surfaces;

  viewer::buildSlice(field, settings, range, surfaces);

  // Two triangles a cell, three vertices each.
  REQUIRE(surfaces.getVertexCount() == 8 * 8 * 6);

  for (size_t i = 0; i < surfaces.getVertexCount(); i++) {

    const float* point = &surfaces.data[9 * i];

    // Halfway along z, and spread across the whole of the other two.
    CHECK(point[2] == doctest::Approx(0.0f));
    CHECK(point[0] >= doctest::Approx(-0.1f));
    CHECK(point[0] <= doctest::Approx(0.1f));
  }

  settings.slice_axis = 0;
  settings.slice_position = 0.0f;

  viewer::buildSlice(field, settings, range, surfaces);

  for (size_t i = 0; i < surfaces.getVertexCount(); i++) {
    CHECK(surfaces.data[9 * i] == doctest::Approx(-0.1f));
  }

  settings.show_slice = false;
  viewer::buildSlice(field, settings, range, surfaces);
  CHECK(surfaces.empty());
}


TEST_CASE("Field lines follow the field") {

  // The point of the whole streamline path: a line through a dipole field
  // has to come back round, and a line that followed the grid instead of the
  // field would not.
  const greeter::view::FieldGrid field = dipoleGrid(61);

  viewer::ViewSettings settings;
  settings.show_streamlines = true;
  settings.slice_axis = 1;      // the xz plane, which the field lies in
  settings.slice_position = 0.5f;
  settings.streamline_seeds = 5;
  settings.streamline_steps = 200;

  viewer::LineBuffer lines;

  viewer::buildStreamlines(field, settings, lines);

  REQUIRE_FALSE(lines.empty());

  // Every segment is a step of the field, so each one lies along the field at
  // the point it starts from.
  //
  // Except near the origin. The field used here is the exact field of a point
  // dipole, which is infinite at the point itself and turns right round
  // within a single cell of the grid near it. A line passing through there
  // has nothing sensible to follow, and that is a property of this made up
  // field rather than of a real magnet, which has a finite field inside it.
  // So the segments within a couple of centimetres of the origin are left
  // out, and the rest are held to a tight tolerance.
  size_t checked = 0;

  for (size_t i = 0; i + 1 < lines.getVertexCount(); i += 2) {

    const float* from = &lines.data[6 * i];
    const float* to = &lines.data[6 * (i + 1)];

    float step[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};

    const float taken = std::sqrt(step[0] * step[0] + step[1] * step[1] +
                                  step[2] * step[2]);

    if (!(taken > 0.0f)) {
      continue;
    }

    const float radius = std::sqrt(from[0] * from[0] + from[1] * from[1] +
                                   from[2] * from[2]);

    if (radius < 0.02f) {
      continue;
    }

    float b[3];

    if (!field.sample(from, b)) {
      continue;
    }

    const float strength = std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);

    if (!(strength > 0.0f)) {
      continue;
    }

    // Along the field or against it, depending which way this half of the
    // line was followed, but never across it.
    const float alignment = (step[0] * b[0] + step[1] * b[1] + step[2] * b[2]) /
                            (taken * strength);

    CHECK(std::fabs(alignment) > 0.999f);

    checked++;
  }

  CHECK(checked > 1000);

  // Seeded on the xz plane, and the field of a dipole along z has no y
  // component there, so no line should wander off it.
  for (size_t i = 0; i < lines.getVertexCount(); i++) {
    CHECK(std::fabs(lines.data[6 * i + 1]) < 1e-3f);
  }
}


TEST_CASE("Field lines stop at the edge of the box") {

  const greeter::view::FieldGrid field = dipoleGrid(21);

  viewer::ViewSettings settings;
  settings.show_streamlines = true;
  settings.streamline_seeds = 3;
  settings.streamline_steps = 5000;

  viewer::LineBuffer lines;

  viewer::buildStreamlines(field, settings, lines);

  REQUIRE_FALSE(lines.empty());

  for (size_t i = 0; i < lines.getVertexCount(); i++) {
    for (int axis = 0; axis < 3; axis++) {
      CHECK(lines.data[6 * i + axis] >= doctest::Approx(-0.1f));
      CHECK(lines.data[6 * i + axis] <= doctest::Approx(0.1f));
    }
  }
}


TEST_CASE("Arrows are all the same length, the strength being in the colour") {

  const greeter::view::FieldGrid field = dipoleGrid(21);

  viewer::ViewSettings settings;
  settings.show_glyphs = true;
  settings.glyph_count = 4;

  const viewer::ColorRange range = viewer::computeRange(field, settings);

  viewer::LineBuffer lines;

  viewer::buildGlyphs(field, settings, range, lines);

  REQUIRE_FALSE(lines.empty());

  // A length that followed the strength would draw one arrow across the whole
  // box and leave every other one too small to see, so they are all alike and
  // the colour carries the strength.
  float shortest = 0.0f;
  float longest = 0.0f;

  bool first = true;

  for (size_t i = 0; i + 1 < lines.getVertexCount(); i += 10) {

    // The shaft of each arrow, the four barbs after it being shorter.
    const float* from = &lines.data[6 * i];
    const float* to = &lines.data[6 * (i + 1)];

    const float taken = std::sqrt(
      (to[0] - from[0]) * (to[0] - from[0]) +
      (to[1] - from[1]) * (to[1] - from[1]) +
      (to[2] - from[2]) * (to[2] - from[2]));

    shortest = first ? taken : std::min(shortest, taken);
    longest = first ? taken : std::max(longest, taken);

    first = false;
  }

  CHECK(longest == doctest::Approx(shortest));
}


TEST_CASE("A colour scale over a field that spans decades") {

  const greeter::view::FieldGrid field = dipoleGrid(21);

  viewer::ViewSettings settings;
  settings.quantity = viewer::FieldQuantity::Magnitude;
  settings.clip_quantile = 0.98f;

  const viewer::ColorRange range = viewer::computeRange(field, settings);

  CHECK_FALSE(range.diverging);
  CHECK(range.maximum > range.minimum);

  // The scale stops short of the largest sample, or the handful of samples
  // nearest the dipole would take all of it.
  float largest = 0.0f;

  for (size_t i = 0; i < field.size(); i++) {
    largest = std::max(largest, field.getMagnitude(i));
  }

  CHECK(range.maximum < largest);

  CHECK(range.normalise(range.minimum) == doctest::Approx(0.0f));
  CHECK(range.normalise(range.maximum) == doctest::Approx(1.0f));

  // Anything above the top of the scale is drawn in the top colour rather
  // than running off it.
  CHECK(range.normalise(largest * 10.0f) == doctest::Approx(1.0f));
}


TEST_CASE("A component of the field is coloured either side of zero") {

  const greeter::view::FieldGrid field = dipoleGrid(21);

  viewer::ViewSettings settings;
  settings.quantity = viewer::FieldQuantity::Bz;

  const viewer::ColorRange range = viewer::computeRange(field, settings);

  CHECK(range.diverging);
  CHECK(range.minimum < 0.0f);
  CHECK(range.maximum > 0.0f);

  // Zero sits in the middle, and the two signs are equally far from it, so
  // that the sign is what the colour says.
  CHECK(range.normalise(0.0f) == doctest::Approx(0.5f));
  CHECK(range.normalise(range.maximum) > 0.9f);
  CHECK(range.normalise(range.minimum) < 0.1f);

  float middle[3];
  viewer::mapDivergingColor(0.5f, middle);

  CHECK(middle[0] == doctest::Approx(middle[1]));
  CHECK(middle[1] == doctest::Approx(middle[2]));
}


TEST_CASE("The colour scale runs from one end to the other") {

  float low[3];
  float high[3];

  viewer::mapColor(0.0f, low);
  viewer::mapColor(1.0f, high);

  // Dark at the bottom, bright at the top, which is the whole reason to use
  // this one rather than a rainbow.
  const float low_sum = low[0] + low[1] + low[2];
  const float high_sum = high[0] + high[1] + high[2];

  CHECK(high_sum > low_sum);

  for (int i = 0; i <= 10; i++) {

    float rgb[3];
    viewer::mapColor((float) i / 10.0f, rgb);

    for (int channel = 0; channel < 3; channel++) {
      CHECK(rgb[channel] >= 0.0f);
      CHECK(rgb[channel] <= 1.0f);
    }
  }

  // Out of range is clamped rather than left to run off the ends.
  float under[3];
  float over[3];

  viewer::mapColor(-5.0f, under);
  viewer::mapColor(5.0f, over);

  CHECK(under[0] == doctest::Approx(low[0]));
  CHECK(over[0] == doctest::Approx(high[0]));
}


TEST_CASE("Force arrows start where the torque turns about") {

  greeter::view::SceneSnapshot scene = oneCube();

  greeter::view::ForceReport forces;

  greeter::view::ForceEntry big;
  big.id = 1;
  big.force[0] = 100.0f;
  big.pivot[0] = 0.02f;

  greeter::view::ForceEntry small;
  small.id = 2;
  small.force[2] = 0.001f;
  small.pivot[2] = -0.02f;

  forces.entries.push_back(big);
  forces.entries.push_back(small);

  viewer::ViewSettings settings;
  settings.show_forces = true;
  settings.show_torques = false;

  viewer::LineBuffer lines;

  viewer::buildForces(scene, forces, settings, lines);

  REQUIRE_FALSE(lines.empty());

  // The first arrow is drawn from the pivot of the first entry.
  CHECK(lines.data[0] == doctest::Approx(0.02f));

  // A force five decades smaller still has an arrow, because the length is
  // taken logarithmically. Drawn to scale it would be invisible.
  const size_t shaft = 10;  // one shaft and four barbs each

  REQUIRE(lines.getVertexCount() > shaft + 1);

  const float* second_from = &lines.data[6 * shaft];
  const float* second_to = &lines.data[6 * (shaft + 1)];

  const float taken = std::sqrt(
    (second_to[0] - second_from[0]) * (second_to[0] - second_from[0]) +
    (second_to[1] - second_from[1]) * (second_to[1] - second_from[1]) +
    (second_to[2] - second_from[2]) * (second_to[2] - second_from[2]));

  CHECK(taken > 0.0f);

  settings.show_forces = false;
  viewer::buildForces(scene, forces, settings, lines);
  CHECK(lines.empty());
}


TEST_CASE("The field box is drawn as twelve edges") {

  const greeter::view::FieldGrid field = dipoleGrid(5);

  viewer::LineBuffer lines;

  viewer::buildFieldBox(field, lines);

  // Twelve edges, two ends each, and no edge drawn twice.
  CHECK(lines.getVertexCount() == 24);

  greeter::view::FieldGrid nothing;

  viewer::buildFieldBox(nothing, lines);

  CHECK(lines.empty());
}


TEST_CASE("Nothing is drawn from a field that was never simulated") {

  const greeter::view::FieldGrid nothing;

  viewer::ViewSettings settings;
  settings.show_slice = true;
  settings.show_glyphs = true;
  settings.show_streamlines = true;

  const viewer::ColorRange range = viewer::computeRange(nothing, settings);

  viewer::SurfaceBuffer surfaces;
  viewer::LineBuffer lines;

  viewer::buildSlice(nothing, settings, range, surfaces);
  CHECK(surfaces.empty());

  viewer::buildGlyphs(nothing, settings, range, lines);
  CHECK(lines.empty());

  viewer::buildStreamlines(nothing, settings, lines);
  CHECK(lines.empty());
}

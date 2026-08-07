#include "GeometryBuilder.h"

#include <greeter/Quaternion.h>
#include <greeter/view/ShapeMesh.h>

#include <algorithm>
#include <cmath>


namespace {

void addSurfaceVertex(viewer::SurfaceBuffer& buffer, const float* point,
                      const float* normal, const float* color) {

  for (int i = 0; i < 3; i++) buffer.data.push_back(point[i]);
  for (int i = 0; i < 3; i++) buffer.data.push_back(normal[i]);
  for (int i = 0; i < 3; i++) buffer.data.push_back(color[i]);
}

void addLineVertex(viewer::LineBuffer& buffer, const float* point,
                   const float* color) {

  for (int i = 0; i < 3; i++) buffer.data.push_back(point[i]);
  for (int i = 0; i < 3; i++) buffer.data.push_back(color[i]);
}

void addSegment(viewer::LineBuffer& buffer, const float* from, const float* to,
                const float* color) {
  addLineVertex(buffer, from, color);
  addLineVertex(buffer, to, color);
}

float lengthOf(const float* vector) {
  return std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] +
                   vector[2] * vector[2]);
}

void cross(const float* a, const float* b, float* result) {
  result[0] = a[1] * b[2] - a[2] * b[1];
  result[1] = a[2] * b[0] - a[0] * b[2];
  result[2] = a[0] * b[1] - a[1] * b[0];
}

bool normalise(float* vector) {

  const float length = lengthOf(vector);

  if (!(length > 0.0f)) {
    return false;
  }

  for (int i = 0; i < 3; i++) {
    vector[i] /= length;
  }

  return true;
}

/*
  An arrow drawn in lines: the shaft, and four barbs laid round it. Lines
  rather than a cone because there may be tens of thousands of them, and at
  the size they are drawn a cone would look no different.
*/
void addArrow(viewer::LineBuffer& buffer, const float* from, const float* to,
              const float* color) {

  addSegment(buffer, from, to, color);

  float along[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};

  const float length = lengthOf(along);

  if (!(length > 0.0f)) {
    return;
  }

  for (int i = 0; i < 3; i++) {
    along[i] /= length;
  }

  // Any direction not along the arrow will do to start the barbs from.
  const float guess[3] = {
    std::fabs(along[2]) < 0.9f ? 0.0f : 1.0f,
    0.0f,
    std::fabs(along[2]) < 0.9f ? 1.0f : 0.0f
  };

  float side[3];
  cross(along, guess, side);

  if (!normalise(side)) {
    return;
  }

  float other[3];
  cross(along, side, other);

  const float head = 0.25f * length;
  const float spread = 0.12f * length;

  const float base[3] = {
    to[0] - head * along[0],
    to[1] - head * along[1],
    to[2] - head * along[2]
  };

  for (int barb = 0; barb < 4; barb++) {

    const float sign = barb % 2 == 0 ? 1.0f : -1.0f;
    const float* axis = barb < 2 ? side : other;

    const float tip[3] = {
      base[0] + sign * spread * axis[0],
      base[1] + sign * spread * axis[1],
      base[2] + sign * spread * axis[2]
    };

    addSegment(buffer, to, tip, color);
  }
}

/* Local point of a magnet to where it actually is. */
void toWorld(const greeter::view::MagnetView& magnet, const float* local,
             float* world) {

  float turned[3];

  greeter::Quaternion::applyRotationFromQuaternion(
    magnet.orientation, local, turned);

  for (int i = 0; i < 3; i++) {
    world[i] = magnet.position[i] + turned[i];
  }
}

/* A direction of a magnet, turned but not moved. */
void toWorldDirection(const greeter::view::MagnetView& magnet,
                      const float* local, float* world) {
  greeter::Quaternion::applyRotationFromQuaternion(
    magnet.orientation, local, world);
}

/* Where a point of the field box sits, for a fraction along each axis. */
void boxPoint(const greeter::view::FieldGrid& field, const float* fraction,
              float* point) {

  for (int axis = 0; axis < 3; axis++) {
    point[axis] = field.bounds[2 * axis] +
                  fraction[axis] * (field.bounds[2 * axis + 1] -
                                    field.bounds[2 * axis]);
  }
}

}  // namespace


viewer::ColorRange viewer::computeRange(const greeter::view::FieldGrid& field,
                                        const viewer::ViewSettings& settings) {

  viewer::ColorRange range;

  range.scale = settings.scale;
  range.diverging = settings.quantity != viewer::FieldQuantity::Magnitude;

  if (field.empty()) {
    return range;
  }

  std::vector<float> values;
  values.reserve(field.size());

  for (size_t i = 0; i < field.size(); i++) {
    float b[3];
    field.getField(i, b);
    values.push_back(viewer::quantityOf(b, settings.quantity));
  }

  if (range.diverging) {

    // Both ends of a diverging scale are set by how far the values reach from
    // zero, so the quantile is taken of the magnitudes.
    std::vector<float> magnitudes;
    magnitudes.reserve(values.size());

    for (const auto& value : values) {
      magnitudes.push_back(std::fabs(value));
    }

    const size_t at = (size_t) (std::max(0.0f, std::min(1.0f, settings.clip_quantile)) *
                                (float) (magnitudes.size() - 1));

    std::nth_element(magnitudes.begin(), magnitudes.begin() + (long) at,
                     magnitudes.end());

    range.maximum = magnitudes[at];
    range.minimum = -range.maximum;

    return range;
  }

  const size_t at = (size_t) (std::max(0.0f, std::min(1.0f, settings.clip_quantile)) *
                              (float) (values.size() - 1));

  std::nth_element(values.begin(), values.begin() + (long) at, values.end());

  range.maximum = values[at];
  range.minimum = *std::min_element(values.begin(), values.end());

  return range;
}


void viewer::buildMagnets(const greeter::view::SceneSnapshot& scene,
                          const viewer::ViewSettings& settings,
                          viewer::SurfaceBuffer& surfaces,
                          viewer::LineBuffer& outlines,
                          viewer::LineBuffer& magnetization,
                          std::vector<viewer::PickSphere>& picks) {

  surfaces.clear();
  outlines.clear();
  magnetization.clear();
  picks.clear();

  if (!settings.show_magnets) {
    return;
  }

  const float body[3] = {0.62f, 0.64f, 0.70f};
  const float chosen[3] = {1.00f, 0.72f, 0.20f};
  const float edge[3] = {0.20f, 0.21f, 0.24f};
  const float polarization[3] = {0.90f, 0.25f, 0.30f};

  for (const auto& magnet : scene.magnets) {

    const bool selected = magnet.id == settings.selected_id;
    const float* color = selected ? chosen : body;

    const greeter::view::ShapeMesh mesh =
      greeter::view::buildMesh(magnet.shape, 28);

    const std::vector<float> half = magnet.shape.getLocalHalfExtent();

    viewer::PickSphere sphere;
    sphere.id = magnet.id;
    sphere.radius = lengthOf(half.data());

    for (int i = 0; i < 3; i++) {
      sphere.center[i] = magnet.position[i];
    }

    // A shape nobody described has no triangles, so it is picked out by a
    // small cross instead of disappearing.
    if (mesh.empty()) {

      const float reach = sphere.radius > 0.0f ? sphere.radius : 0.005f;

      sphere.radius = reach;

      for (int axis = 0; axis < 3; axis++) {

        float from[3] = {magnet.position[0], magnet.position[1], magnet.position[2]};
        float to[3] = {magnet.position[0], magnet.position[1], magnet.position[2]};

        from[axis] -= reach;
        to[axis] += reach;

        addSegment(outlines, from, to, color);
      }

      picks.push_back(sphere);
      continue;
    }

    for (const auto& index : mesh.triangles) {

      float point[3];
      float normal[3];

      toWorld(magnet, &mesh.vertices[3 * index], point);
      toWorldDirection(magnet, &mesh.normals[3 * index], normal);

      addSurfaceVertex(surfaces, point, normal, color);
    }

    if (settings.show_outlines) {
      for (size_t i = 0; i + 1 < mesh.edges.size(); i += 2) {

        float from[3];
        float to[3];

        toWorld(magnet, &mesh.vertices[3 * mesh.edges[i]], from);
        toWorld(magnet, &mesh.vertices[3 * mesh.edges[i + 1]], to);

        addSegment(outlines, from, to, selected ? chosen : edge);
      }
    }

    if (settings.show_magnetization) {

      float direction[3];
      toWorldDirection(magnet, magnet.magnetization, direction);

      if (normalise(direction)) {

        // As long as the magnet is wide, so that it reads as belonging to it
        // rather than as a field arrow.
        const float reach = 0.8f * std::max(sphere.radius, 1e-6f);

        const float from[3] = {
          magnet.position[0] - reach * direction[0],
          magnet.position[1] - reach * direction[1],
          magnet.position[2] - reach * direction[2]
        };

        const float to[3] = {
          magnet.position[0] + reach * direction[0],
          magnet.position[1] + reach * direction[1],
          magnet.position[2] + reach * direction[2]
        };

        addArrow(magnetization, from, to, polarization);
      }
    }

    picks.push_back(sphere);
  }
}


void viewer::buildSlice(const greeter::view::FieldGrid& field,
                        const viewer::ViewSettings& settings,
                        const viewer::ColorRange& range,
                        viewer::SurfaceBuffer& surfaces) {

  surfaces.clear();

  if (field.empty() || !field.grid || !settings.show_slice) {
    return;
  }

  const int across = settings.slice_axis % 3;
  const int first = (across + 1) % 3;
  const int second = (across + 2) % 3;

  const int steps = std::max(2, settings.slice_resolution);

  // The plane faces along the axis it is across, and is drawn unlit, so the
  // normal is only there to fill the vertex out.
  float normal[3] = {0.0f, 0.0f, 0.0f};
  normal[across] = 1.0f;

  std::vector<float> corner(3 * (size_t) (steps + 1) * (size_t) (steps + 1), 0.0f);
  std::vector<float> color(3 * (size_t) (steps + 1) * (size_t) (steps + 1), 0.0f);

  for (int a = 0; a <= steps; a++) {
    for (int b = 0; b <= steps; b++) {

      float fraction[3];

      fraction[across] = std::max(0.0f, std::min(1.0f, settings.slice_position));
      fraction[first] = (float) a / (float) steps;
      fraction[second] = (float) b / (float) steps;

      float point[3];
      boxPoint(field, fraction, point);

      const size_t at = (size_t) a * (size_t) (steps + 1) + (size_t) b;

      for (int i = 0; i < 3; i++) {
        corner[3 * at + i] = point[i];
      }

      float value[3] = {0.0f, 0.0f, 0.0f};

      if (field.sample(point, value)) {
        viewer::mapColor(range, viewer::quantityOf(value, settings.quantity),
                         &color[3 * at]);
      }
    }
  }

  for (int a = 0; a < steps; a++) {
    for (int b = 0; b < steps; b++) {

      const size_t at[4] = {
        (size_t) a * (size_t) (steps + 1) + (size_t) b,
        (size_t) (a + 1) * (size_t) (steps + 1) + (size_t) b,
        (size_t) (a + 1) * (size_t) (steps + 1) + (size_t) (b + 1),
        (size_t) a * (size_t) (steps + 1) + (size_t) (b + 1)
      };

      const int order[6] = {0, 1, 2, 0, 2, 3};

      for (const auto& which : order) {
        addSurfaceVertex(surfaces, &corner[3 * at[which]], normal,
                         &color[3 * at[which]]);
      }
    }
  }
}


void viewer::buildGlyphs(const greeter::view::FieldGrid& field,
                         const viewer::ViewSettings& settings,
                         const viewer::ColorRange& range,
                         viewer::LineBuffer& lines) {

  lines.clear();

  if (field.empty() || !field.grid || !settings.show_glyphs) {
    return;
  }

  const int steps = std::max(1, settings.glyph_count);

  // Long enough to see, short enough not to reach the next one.
  float reach = 0.0f;

  for (int axis = 0; axis < 3; axis++) {
    const float span = field.bounds[2 * axis + 1] - field.bounds[2 * axis];
    reach = std::max(reach, span);
  }

  reach *= 0.6f * settings.glyph_scale / (float) steps;

  for (int i = 0; i < steps; i++) {
    for (int j = 0; j < steps; j++) {
      for (int k = 0; k < steps; k++) {

        // In the middle of each cell, so that no arrow sits on a face of the
        // box where half of it would be outside.
        const float fraction[3] = {
          ((float) i + 0.5f) / (float) steps,
          ((float) j + 0.5f) / (float) steps,
          ((float) k + 0.5f) / (float) steps
        };

        float point[3];
        boxPoint(field, fraction, point);

        float value[3];

        if (!field.sample(point, value)) {
          continue;
        }

        float direction[3] = {value[0], value[1], value[2]};

        if (!normalise(direction)) {
          continue;
        }

        float color[3];
        viewer::mapColor(range, viewer::quantityOf(value, settings.quantity), color);

        // Every arrow the same length, the strength said in the colour. A
        // length proportional to the field would draw one arrow across the
        // whole box and leave the rest invisible.
        const float from[3] = {
          point[0] - 0.5f * reach * direction[0],
          point[1] - 0.5f * reach * direction[1],
          point[2] - 0.5f * reach * direction[2]
        };

        const float to[3] = {
          point[0] + 0.5f * reach * direction[0],
          point[1] + 0.5f * reach * direction[1],
          point[2] + 0.5f * reach * direction[2]
        };

        addArrow(lines, from, to, color);
      }
    }
  }
}


void viewer::buildStreamlines(const greeter::view::FieldGrid& field,
                              const viewer::ViewSettings& settings,
                              viewer::LineBuffer& lines) {

  lines.clear();

  if (field.empty() || !field.grid || !settings.show_streamlines) {
    return;
  }

  float span = 0.0f;

  for (int axis = 0; axis < 3; axis++) {
    span = std::max(span, field.bounds[2 * axis + 1] - field.bounds[2 * axis]);
  }

  if (!(span > 0.0f)) {
    return;
  }

  const float step = span / (float) std::max(1, settings.streamline_steps);

  const int seeds = std::max(1, settings.streamline_seeds);

  const int across = settings.slice_axis % 3;
  const int first = (across + 1) % 3;
  const int second = (across + 2) % 3;

  /*
    One colour, and deliberately not the colour scale.

    Field lines are started on the slice plane and, wherever the field is
    tangent to it, stay on it. Colouring them by the same quantity the slice
    is coloured by therefore paints every line exactly the colour of the
    pixel underneath it, and the lines vanish. A line says which way the
    field points; the slice and the arrows say how strong it is.
  */
  const float color[3] = {0.93f, 0.94f, 0.98f};

  for (int a = 0; a < seeds; a++) {
    for (int b = 0; b < seeds; b++) {

      float fraction[3];

      fraction[across] = std::max(0.0f, std::min(1.0f, settings.slice_position));
      fraction[first] = ((float) a + 0.5f) / (float) seeds;
      fraction[second] = ((float) b + 0.5f) / (float) seeds;

      float seed[3];
      boxPoint(field, fraction, seed);

      // Followed both ways, because a line through a seed has two halves and
      // showing one of them makes the picture lopsided.
      for (int sense = -1; sense <= 1; sense += 2) {

        float point[3] = {seed[0], seed[1], seed[2]};

        for (int taken = 0; taken < settings.streamline_steps; taken++) {

          // Classic fourth order Runge Kutta on the direction of the field,
          // which is what makes a line follow the field rather than the grid.
          float k1[3], k2[3], k3[3], k4[3];

          auto direction = [&](const float* at, float* out) {
            if (!field.sample(at, out)) {
              return false;
            }
            return normalise(out);
          };

          if (!direction(point, k1)) break;

          float probe[3];

          for (int i = 0; i < 3; i++) {
            probe[i] = point[i] + (float) sense * 0.5f * step * k1[i];
          }
          if (!direction(probe, k2)) break;

          for (int i = 0; i < 3; i++) {
            probe[i] = point[i] + (float) sense * 0.5f * step * k2[i];
          }
          if (!direction(probe, k3)) break;

          for (int i = 0; i < 3; i++) {
            probe[i] = point[i] + (float) sense * step * k3[i];
          }
          if (!direction(probe, k4)) break;

          float next[3];

          for (int i = 0; i < 3; i++) {
            next[i] = point[i] + (float) sense * step / 6.0f *
                      (k1[i] + 2.0f * k2[i] + 2.0f * k3[i] + k4[i]);
          }

          float value[3];

          if (!field.sample(next, value)) {
            break;
          }

          addSegment(lines, point, next, color);

          for (int i = 0; i < 3; i++) {
            point[i] = next[i];
          }
        }
      }
    }
  }
}


void viewer::buildForces(const greeter::view::SceneSnapshot& scene,
                         const greeter::view::ForceReport& forces,
                         const viewer::ViewSettings& settings,
                         viewer::LineBuffer& lines) {

  lines.clear();

  if (forces.empty()) {
    return;
  }

  float minimum = 0.0f;
  float maximum = 0.0f;

  float reach = 0.0f;

  float low[3];
  float high[3];

  if (scene.getBounds(low, high)) {
    for (int axis = 0; axis < 3; axis++) {
      reach = std::max(reach, high[axis] - low[axis]);
    }
  }

  if (!(reach > 0.0f)) {
    reach = 0.1f;
  }

  reach *= 0.25f;

  const float force_color[3] = {0.20f, 0.85f, 0.45f};
  const float torque_color[3] = {0.40f, 0.60f, 1.00f};

  struct Which {
    bool wanted;
    bool torque;
    const float* color;
  };

  const Which drawn[2] = {
    {settings.show_forces, false, force_color},
    {settings.show_torques, true, torque_color}
  };

  for (const auto& what : drawn) {

    if (!what.wanted) {
      continue;
    }

    const bool ranged = what.torque
      ? forces.getTorqueMagnitudeRange(minimum, maximum)
      : forces.getForceMagnitudeRange(minimum, maximum);

    if (!ranged || !(maximum > 0.0f)) {
      continue;
    }

    for (const auto& entry : forces.entries) {

      const float* vector = what.torque ? entry.torque : entry.force;

      const float magnitude = what.torque
        ? entry.getTorqueMagnitude() : entry.getForceMagnitude();

      if (!(magnitude > 0.0f)) {
        continue;
      }

      float direction[3] = {vector[0], vector[1], vector[2]};

      if (!normalise(direction)) {
        continue;
      }

      // Forces in one scene run over decades, so the length is taken
      // logarithmically against the largest of them. A linear length would
      // draw one arrow and a row of stubs.
      const float relative = std::log10(magnitude / maximum) / 3.0f + 1.0f;

      const float length = reach * std::max(0.15f, std::min(1.0f, relative));

      const float to[3] = {
        entry.pivot[0] + length * direction[0],
        entry.pivot[1] + length * direction[1],
        entry.pivot[2] + length * direction[2]
      };

      addArrow(lines, entry.pivot, to, what.color);
    }
  }
}


void viewer::buildFieldBox(const greeter::view::FieldGrid& field,
                           viewer::LineBuffer& lines) {

  lines.clear();

  if (field.empty() || !field.grid) {
    return;
  }

  const float color[3] = {0.45f, 0.45f, 0.50f};

  for (int corner = 0; corner < 8; corner++) {
    for (int axis = 0; axis < 3; axis++) {

      // Each edge once: only from the corners that are low along its axis.
      if ((corner >> axis) & 1) {
        continue;
      }

      float from[3];
      float to[3];

      for (int i = 0; i < 3; i++) {
        const int high = (corner >> i) & 1;
        from[i] = field.bounds[2 * i + high];
        to[i] = field.bounds[2 * i + high];
      }

      to[axis] = field.bounds[2 * axis + 1];

      addSegment(lines, from, to, color);
    }
  }
}


void viewer::buildAxes(const float& length, viewer::LineBuffer& lines) {

  lines.clear();

  const float origin[3] = {0.0f, 0.0f, 0.0f};

  const float color[3][3] = {
    {0.90f, 0.30f, 0.30f},
    {0.30f, 0.80f, 0.30f},
    {0.35f, 0.55f, 0.95f}
  };

  for (int axis = 0; axis < 3; axis++) {

    float to[3] = {0.0f, 0.0f, 0.0f};
    to[axis] = length;

    addArrow(lines, origin, to, color[axis]);
  }
}


int64_t viewer::pick(const std::vector<viewer::PickSphere>& spheres,
                     const float* origin, const float* direction) {

  int64_t found = 0;
  float nearest = 0.0f;

  for (const auto& sphere : spheres) {

    const float offset[3] = {
      origin[0] - sphere.center[0],
      origin[1] - sphere.center[1],
      origin[2] - sphere.center[2]
    };

    // |origin + t * direction - centre|^2 = radius^2, with a unit direction.
    const float b = offset[0] * direction[0] + offset[1] * direction[1] +
                    offset[2] * direction[2];

    const float c = offset[0] * offset[0] + offset[1] * offset[1] +
                    offset[2] * offset[2] - sphere.radius * sphere.radius;

    const float discriminant = b * b - c;

    if (discriminant < 0.0f) {
      continue;
    }

    const float root = std::sqrt(discriminant);

    float distance = -b - root;

    if (distance < 0.0f) {
      distance = -b + root;
    }

    if (distance < 0.0f) {
      continue;  // behind the eye
    }

    if (found == 0 || distance < nearest) {
      found = sphere.id;
      nearest = distance;
    }
  }

  return found;
}

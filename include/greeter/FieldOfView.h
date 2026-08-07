#ifndef FIELD_OF_VIEW_H
#define FIELD_OF_VIEW_H

#include <cstdint>
#include <vector>
#include <string>
#include <memory>


namespace greeter {

  /*
    Where the field is sampled.

    A field of view is a list of observation points. It is usually a regular
    grid, and when it is it remembers the box and how many points sit along
    each axis. A bare list of points is enough to run a simulation, but not to
    draw a slice through the result, interpolate between samples or seed a
    streamline, so a grid is asked to keep saying that it is one.

    Points of a grid come out with x slowest and z fastest, so the point
    (i, j, k) sits at index (i * ny + j) * nz + k. Note that this is the
    opposite of the order the members of a linear_array arrangement come out
    in, which is x fastest.

    Along an axis sampled once the single point sits at "min" and "max" is not
    used, which is what numpy.linspace does with num=1. A field of view with
    one point along an axis is the ordinary way to ask for a plane.
  */
  class FieldOfView {

    std::vector<std::vector<float>> fov;

    // Set only when the points were laid out as a grid, see isGrid().
    bool grid;
    std::vector<float> bounds;      // x_min, x_max, y_min, y_max, z_min, z_max
    std::vector<uint32_t> counts;   // points along x, y and z

    public:

        FieldOfView();

        /* An explicit list of observation points, which is not a grid. */
        FieldOfView(std::vector<std::vector<float>> fov);

        /*
          A regular grid.

          xxyyzz is x_min, x_max, y_min, y_max, z_min, z_max and num_points is
          the number of samples along each axis, each of which has to be at
          least one. Throws if a maximum lies below its minimum.
        */
        FieldOfView(
            std::vector<float> xxyyzz,
            std::vector<uint32_t> num_points
        );

        FieldOfView(const FieldOfView& other);

        ~FieldOfView();

        std::vector<std::vector<float>> getFOV() const;

        /* The points themselves, for callers that would rather not copy them. */
        const std::vector<std::vector<float>>& getPoints() const;

        void setFOV(const std::vector<std::vector<float>>& fov);

        size_t getNumPoints() const;

        /*
          Whether the points were laid out as a grid. getBounds() and
          getCounts() only mean anything when they were, and both throw
          otherwise rather than hand back a box that was never asked for.
        */
        bool isGrid() const;

        std::vector<float> getBounds() const;
        std::vector<uint32_t> getCounts() const;

        /*
          Spacing between neighbouring samples along each axis. An axis
          sampled once has a spacing of zero.
        */
        std::vector<float> getSpacing() const;

        /* Index into the point list of the grid point (i, j, k). */
        size_t getIndex(const uint32_t& i, const uint32_t& j, const uint32_t& k) const;

        void display() const;

        std::unique_ptr<FieldOfView> clone() const;

  };
}  // namespace greeter

#endif // FIELD_OF_VIEW_H

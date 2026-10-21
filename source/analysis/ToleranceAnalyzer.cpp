#include <greeter/MagnetGeometryFactory.h>
#include <greeter/analysis/ToleranceAnalyzer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>

namespace greeter {
  namespace analysis {
    namespace {

      using namespace service;
      constexpr double PI = 3.14159265358979323846;
      struct Cancelled {};

      // Defined conversion and Box-Muller sampling avoid standard-library-specific
      // distribution implementations. Sampling is serial and independent of Kokkos.
      class Errors {
        std::mt19937_64 generator;
        bool spare_ready = false;
        double spare = 0.0;
        ToleranceDistribution distribution;
        double unit() { return (double)(generator() >> 11) / 9007199254740992.0; }

      public:
        Errors(uint64_t seed, ToleranceDistribution d) : generator(seed), distribution(d) {}
        double draw(double amount) {
          if (amount == 0.0) return 0.0;
          if (distribution == ToleranceDistribution::Uniform) return amount * (2.0 * unit() - 1.0);
          if (spare_ready) {
            spare_ready = false;
            return amount * spare;
          }
          const double radius = std::sqrt(-2.0 * std::log(1.0 - unit()));
          const double angle = 2.0 * PI * unit();
          spare = radius * std::sin(angle);
          spare_ready = true;
          return amount * radius * std::cos(angle);
        }
      };

      std::array<float, 4> rotation(Errors& errors, double degrees) {
        const double x = errors.draw(degrees) * PI / 180.0;
        const double y = errors.draw(degrees) * PI / 180.0;
        const double z = errors.draw(degrees) * PI / 180.0;
        const double angle = std::hypot(std::hypot(x, y), z);
        const double scale = angle > 0.0 ? std::sin(angle / 2.0) / angle : 0.0;
        return {(float)std::cos(angle / 2.0), (float)(x * scale), (float)(y * scale),
                (float)(z * scale)};
      }

      void compose(const float* a, const float* b, float* result) {
        const double q[4] = {a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3],
                             a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2],
                             a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1],
                             a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0]};
        const double norm = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        for (size_t i = 0; i < 4; ++i) result[i] = (float)(q[i] / norm);
      }

      struct Geometry {
        size_t offset;
        size_t size;
        size_t geometry_start;
        bool has_size;
      };

    }  // namespace

    service::ToleranceReport analyzeTolerance(const MagnetCollection& collection,
                                              const FieldRequest& original,
                                              const ToleranceSettings& settings,
                                              ProgressSink* progress, const RunOptions& options) {
      settings.validate();
      if (collection.get_num_magnets() == 0)
        throw std::invalid_argument("Tolerance analysis needs at least one magnet");
      FieldRequest request = original;
      request.kind = view::FieldKind::B;
      size_t grid_count = 1;
      double center[3], radius = std::numeric_limits<double>::max();
      for (size_t axis = 0; axis < 3; ++axis) {
        const double low = request.bounds[2 * axis], high = request.bounds[2 * axis + 1];
        if (!std::isfinite(low) || !std::isfinite(high) || high < low)
          throw std::invalid_argument("Analysis bounds must be finite and ordered");
        if (settings.points_per_axis > 0)
          request.counts[axis] = high > low ? settings.points_per_axis : 1;
        if (request.counts[axis] < 1 || request.counts[axis] > 2000000 / grid_count)
          throw std::invalid_argument(
              "Analysis grid must contain 1 to 2000000 points; reduce the sampling density");
        grid_count *= request.counts[axis];
        center[axis] = 0.5 * (low + high);
        radius = std::min(radius, 0.5 * (high - low));
      }
      if (settings.region == ToleranceRegion::InscribedSphere && !(radius > 0.0))
        throw std::invalid_argument(
            "A spherical analysis region needs nonzero extent along all three axes");

      std::vector<std::vector<float>> points;
      points.reserve(grid_count);
      for (size_t index = 0; index < grid_count; ++index) {
        const size_t coordinate[3]
            = {index / ((size_t)request.counts[1] * request.counts[2]),
               (index / request.counts[2]) % request.counts[1], index % request.counts[2]};
        std::vector<float> point(3);
        double distance2 = 0.0;
        for (size_t axis = 0; axis < 3; ++axis) {
          const double step
              = request.counts[axis] > 1
                    ? ((double)request.bounds[2 * axis + 1] - request.bounds[2 * axis])
                          / (request.counts[axis] - 1)
                    : 0.0;
          point[axis] = (float)(request.bounds[2 * axis] + coordinate[axis] * step);
          distance2 += ((double)point[axis] - center[axis]) * ((double)point[axis] - center[axis]);
        }
        if (settings.region == ToleranceRegion::Box || distance2 <= radius * radius * (1.0 + 1e-6))
          points.push_back(std::move(point));
      }
      if (points.empty())
        throw std::invalid_argument(
            "The analysis region has no samples; increase the grid density");

      ToleranceReport report;
      report.settings = settings;
      report.backend = ExecSpace::name();
      report.points = points.size();
      report.magnets = collection.get_num_magnets();
      report.sampling = {{"bounds_m",
                          {request.bounds[0], request.bounds[1], request.bounds[2],
                           request.bounds[3], request.bounds[4], request.bounds[5]}},
                         {"counts", {request.counts[0], request.counts[1], request.counts[2]}},
                         {"quantity", "B_magnitude_T"}};

      UInt32VectorView types("tolerance_types", report.magnets);
      UInt32VectorView offsets("tolerance_offsets", report.magnets);
      FloatVectorView parameters("tolerance_parameters", collection.getTotalNumOfParameters());
      collection.fillMagnetParameters(parameters, offsets, types);
      std::vector<float> nominal(parameters.extent(0));
      for (size_t i = 0; i < nominal.size(); ++i) nominal[i] = parameters(i);
      std::vector<Geometry> geometry;
      for (size_t magnet = 0; magnet < report.magnets; ++magnet) {
        const size_t offset = offsets(magnet);
        const size_t end = magnet + 1 < report.magnets ? offsets(magnet + 1) : nominal.size();
        const auto shape = MagnetGeometryFactory::getInstance().describeShape(
            (uint16_t)types(magnet), nominal.data() + offset);
        if (settings.size_percent > 0.0 && shape.kind == view::ShapeKind::Unknown)
          throw std::invalid_argument("Size tolerance is unavailable for an unregistered geometry");
        // Meshes carry a face count before their coordinates. Never scale it.
        const size_t metadata = shape.kind == view::ShapeKind::Mesh
                                        && end - offset - 10 == shape.parameters.size() + 1
                                    ? 1
                                    : 0;
        geometry.push_back(
            {offset, end - offset, offset + 7 + metadata, shape.kind != view::ShapeKind::Point});
      }

      Float3VectorView initial_points("tolerance_initial", 1);
      MagneticFieldSimulator simulator(parameters, offsets, types, initial_points);
      // Small chunks keep Stop responsive even for thousands of magnets.
      const size_t chunk = std::max((size_t)1, std::min(options.chunk, (size_t)512));
      if (points.size() > std::numeric_limits<size_t>::max() / ((size_t)settings.trials + 1))
        throw std::invalid_argument("Analysis work count exceeds this platform's capacity");
      const size_t total = points.size() * ((size_t)settings.trials + 1);
      size_t done = 0;
      auto notify = [&]() {
        if (progress && !progress->onProgress(done, total)) throw Cancelled{};
      };
      auto measure = [&]() {
        ToleranceMetrics measured;
        measured.min_T = std::numeric_limits<double>::max();
        double sum = 0.0;
        for (size_t start = 0; start < points.size(); start += chunk) {
          notify();
          const size_t end = std::min(start + chunk, points.size());
          simulator.fillObservationPoints(
              std::vector<std::vector<float>>(points.begin() + start, points.begin() + end));
          simulator.simulate(false);
          const auto fields = simulator.getMagneticFieldsFlat();
          for (size_t i = 0; i < fields.size(); i += 3) {
            const double value
                = std::hypot(std::hypot((double)fields[i], fields[i + 1]), fields[i + 2]);
            if (!std::isfinite(value))
              throw std::runtime_error(
                  "Non-finite field sample; the region may meet a singularity");
            sum += value;
            measured.min_T = std::min(measured.min_T, value);
            measured.max_T = std::max(measured.max_T, value);
          }
          done += end - start;
          notify();
        }
        measured.mean_T = sum / points.size();
        if (!(measured.mean_T > 0.0))
          throw std::runtime_error("Mean |B| is zero; homogeneity is undefined");
        measured.homogeneity_ppm = 1e6 * (measured.max_T - measured.min_T) / measured.mean_T;
        return measured;
      };

      try {
        report.nominal = measure();
        std::vector<double> means, homogeneities, shifts;
        for (uint32_t trial = 1; trial <= settings.trials; ++trial) {
          notify();
          ToleranceTrial result;
          result.trial = trial;
          const size_t trial_start = done;
          try {
            Errors errors(settings.seed + 0x9e3779b97f4a7c15ULL * trial, settings.distribution);
            for (const auto& g : geometry) {
              std::vector<float> p(nominal.begin() + g.offset, nominal.begin() + g.offset + g.size);
              const double strength = 1.0 + errors.draw(settings.strength_percent / 100.0);
              const double size
                  = g.has_size ? 1.0 + errors.draw(settings.size_percent / 100.0) : 1.0;
              if (!(strength > 0.0) || !(size > 0.0))
                throw std::runtime_error(
                    "Nonpositive strength or size drawn; trial rejected without resampling");
              for (size_t axis = 0; axis < 3; ++axis)
                p[axis] += (float)errors.draw(settings.position_mm / 1000.0);
              if (settings.body_tilt_degrees > 0.0) {
                const auto q = rotation(errors, settings.body_tilt_degrees);
                float placed[4];
                compose(q.data(), p.data() + 3, placed);
                for (size_t i = 0; i < 4; ++i) p[3 + i] = placed[i];
              }
              if (settings.direction_degrees > 0.0) {
                const auto q = rotation(errors, settings.direction_degrees);
                float moment[3];
                Quaternion::applyRotationFromQuaternion(q.data(), p.data() + g.size - 3, moment);
                for (size_t axis = 0; axis < 3; ++axis) p[g.size - 3 + axis] = moment[axis];
              }
              for (size_t axis = 0; axis < 3; ++axis) p[g.size - 3 + axis] *= (float)strength;
              for (size_t i = g.geometry_start - g.offset; i < g.size - 3; ++i) p[i] *= (float)size;
              for (size_t i = 0; i < p.size(); ++i) {
                if (!std::isfinite(p[i]))
                  throw std::runtime_error("A perturbed parameter is non-finite");
                parameters(g.offset + i) = p[i];
              }
            }
            result.metrics = measure();
            result.metrics.field_shift_ppm
                = 1e6 * (result.metrics.mean_T - report.nominal.mean_T) / report.nominal.mean_T;
            result.valid = true;
            result.passed = settings.hasLimits()
                            && (settings.homogeneity_limit_ppm <= 0.0
                                || result.metrics.homogeneity_ppm <= settings.homogeneity_limit_ppm)
                            && result.metrics.mean_T >= settings.minimum_mean_T;
            ++report.valid_trials;
            if (result.passed) ++report.passed_trials;
            means.push_back(result.metrics.mean_T);
            homogeneities.push_back(result.metrics.homogeneity_ppm);
            shifts.push_back(result.metrics.field_shift_ppm);
          } catch (const std::exception& error) {
            result.error = error.what();
            done = trial_start
                   + points.size();  // Invalid draws count as failed assemblies, never retried.
            notify();
          }
          report.trials.push_back(std::move(result));
        }
        report.mean_field = toleranceStatistics(std::move(means));
        report.homogeneity = toleranceStatistics(std::move(homogeneities));
        report.field_shift = toleranceStatistics(std::move(shifts));
      } catch (const Cancelled&) {
        report.cancelled = true;
      }
      return report;
    }

  }  // namespace analysis
}  // namespace greeter

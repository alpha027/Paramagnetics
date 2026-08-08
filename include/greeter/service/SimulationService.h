#ifndef SERVICE_SIMULATION_SERVICE_H
#define SERVICE_SIMULATION_SERVICE_H

#include <greeter/view/Snapshot.h>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <memory>
#include <string>


namespace greeter {
namespace service {

/*
  Told how far along a run is, and asked whether to carry on.

  Returning false asks the run to stop, and it then stops at the end of the
  chunk it is in. A Kokkos parallel region cannot be interrupted once it has
  started, so this is as fine-grained as cancelling gets.
*/
class ProgressSink {

  public:

    virtual ~ProgressSink() = default;

    /* Returns false to ask the run to stop. */
    virtual bool onProgress(const size_t& done, const size_t& total) = 0;
};

/* The box to sample the field in, how finely, and which quantity. */
struct FieldRequest {

  float bounds[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};  // xx, yy, zz
  uint32_t counts[3] = {1, 1, 1};

  /*
    B by default, which is what the field kernels give and what everything
    here used to give. The other three cost a second look at every magnet,
    to ask whether the point is inside it.
  */
  greeter::view::FieldKind kind = greeter::view::FieldKind::B;

  size_t getSampleCount() const;
};

struct RunOptions {

  /*
    Observation points per chunk. A run is cut into chunks so that it can
    report progress and be asked to stop; smaller chunks answer sooner and
    waste a little more time between them.
  */
  size_t chunk = 8192;

  /* Whether the simulator announces itself on the standard output. */
  bool verbose = false;
};

/*
  Everything a viewer is allowed to ask of the simulation.

  This header names no Kokkos type, no Magnet and no MagnetCollection, and
  neither does anything it includes. That is what lets a viewer be compiled
  with none of them on its include path, which is a boundary the build then
  keeps rather than one a reader has to.

  A run is cancellable and reports progress, because a field of any size takes
  long enough that a window which cannot be closed during it is not usable.
*/
class SimulationService {

  public:

    SimulationService();
    ~SimulationService();

    SimulationService(SimulationService&& other) noexcept;
    SimulationService& operator=(SimulationService&& other) noexcept;

    SimulationService(const SimulationService&) = delete;
    SimulationService& operator=(const SimulationService&) = delete;

    /* Reads an input file of the ordinary kind, magnets and arrangements. */
    void loadFile(const std::string& path);

    void loadJSON(const nlohmann::json& data, const std::string& source = std::string());

    bool isLoaded() const;

    /* Where the scene was read from, for a window title. */
    std::string getSource() const;

    /* The input as it was read, which a viewer shows and reloads. */
    const nlohmann::json& getInput() const;

    /* The magnets, as much of them as a viewer needs. */
    greeter::view::SceneSnapshot getScene() const;

    bool hasFieldSection() const;
    bool hasForceSection() const;

    /*
      The field_of_view the input file asks for. Returns false when it asks
      for none, leaving `request` alone.
    */
    bool getFieldRequest(FieldRequest& request) const;

    /*
      Samples the field. Returns an empty grid when the run was stopped, so
      that a cancelled run is told apart from an empty one by asking the sink
      that stopped it.
    */
    greeter::view::FieldGrid simulateField(
      const FieldRequest& request,
      ProgressSink* progress = nullptr,
      const RunOptions& options = RunOptions()) const;

    /*
      Computes the forces the input file asks for. Throws when it asks for
      none. Progress is reported once per run rather than per target, because
      the targets are meshed and run together.
    */
    greeter::view::ForceReport simulateForces(
      ProgressSink* progress = nullptr,
      const RunOptions& options = RunOptions()) const;

    /*
      Everything the input file asks for, in one go: the scene always, the
      field and the forces if it asks for them.
    */
    greeter::view::Snapshot run(
      ProgressSink* progress = nullptr,
      const RunOptions& options = RunOptions()) const;

  private:

    struct Impl;

    std::unique_ptr<Impl> impl;
};

}  // namespace service
}  // namespace greeter

#endif  // SERVICE_SIMULATION_SERVICE_H

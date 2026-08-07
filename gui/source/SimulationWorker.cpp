#include "SimulationWorker.h"

#include <greeter/view/SnapshotIO.h>

#include <QFileInfo>

#include <exception>


namespace viewer {

/*
  Passes progress on as a signal, and answers the run with whatever the
  cancel flag presently says.
*/
class WorkerProgress: public greeter::service::ProgressSink {

  public:

    WorkerProgress(SimulationWorker& owner, const QString& what):
      owner(owner), what(what) {}

    bool onProgress(const size_t& done, const size_t& total) override {

      if (total > 0) {

        const int percent = (int) (100 * done / total);

        // Only when it has moved: a signal a hundred times a second would
        // spend the drawing thread's time on repainting a progress bar.
        if (percent != last_percent) {
          last_percent = percent;
          emit owner.progressed(percent, what);
        }
      }

      return !owner.cancel_requested;
    }

  private:

    SimulationWorker& owner;
    QString what;
    int last_percent = -1;
};

}  // namespace viewer


viewer::SimulationWorker::SimulationWorker(QObject* parent): QObject(parent) {}


viewer::SimulationWorker::~SimulationWorker() = default;


void viewer::SimulationWorker::loadInput(const QString& path) {

  busy = true;
  cancel_requested = false;

  try {

    service.loadFile(path.toStdString());

    latest = greeter::view::Snapshot();
    latest.scene = service.getScene();

    emit message(QString("Read %1 magnets from %2")
                   .arg(latest.scene.magnets.size())
                   .arg(QFileInfo(path).fileName()));

    emit sceneReady(latest, path);

  } catch (const std::exception& error) {
    emit failed(QString::fromStdString(error.what()));
  }

  busy = false;
}


void viewer::SimulationWorker::loadSnapshot(const QString& path) {

  busy = true;
  cancel_requested = false;

  try {

    latest = greeter::view::SnapshotIO::read(path.toStdString());

    emit message(QString("Opened a snapshot of %1 magnets and %2 field samples")
                   .arg(latest.scene.magnets.size())
                   .arg(latest.field.size()));

    emit sceneReady(latest, path);
    emit runFinished(latest);

  } catch (const std::exception& error) {
    emit failed(QString::fromStdString(error.what()));
  }

  busy = false;
}


void viewer::SimulationWorker::run() {

  greeter::service::FieldRequest request;

  if (service.isLoaded() && service.getFieldRequest(request)) {
    runField(request);
    return;
  }

  // No field asked for, but there may still be forces.
  runField(greeter::service::FieldRequest());
}


void viewer::SimulationWorker::runField(
    const greeter::service::FieldRequest& request) {

  if (!service.isLoaded()) {
    emit failed("No input file has been opened");
    return;
  }

  busy = true;
  cancel_requested = false;

  try {

    greeter::view::Snapshot result;

    result.scene = service.getScene();

    greeter::service::RunOptions options;
    options.verbose = false;

    if (request.getSampleCount() > 1) {

      WorkerProgress progress(*this, "field");

      result.field = service.simulateField(request, &progress, options);

      if (cancel_requested) {
        emit cancelled();
        busy = false;
        return;
      }
    }

    if (service.hasForceSection()) {

      emit progressed(0, "forces");

      WorkerProgress progress(*this, "forces");

      result.forces = service.simulateForces(&progress, options);

      if (cancel_requested) {
        emit cancelled();
        busy = false;
        return;
      }
    }

    latest = result;

    emit progressed(100, "done");
    emit runFinished(latest);

  } catch (const std::exception& error) {
    emit failed(QString::fromStdString(error.what()));
  }

  busy = false;
}


void viewer::SimulationWorker::writeSnapshot(const QString& path) {

  try {

    greeter::view::SnapshotIO::write(latest, path.toStdString());

    emit message(QString("Snapshot written to %1").arg(QFileInfo(path).fileName()));

  } catch (const std::exception& error) {
    emit failed(QString::fromStdString(error.what()));
  }
}

#ifndef VIEWER_SIMULATION_WORKER_H
#define VIEWER_SIMULATION_WORKER_H

#include <greeter/service/SimulationService.h>
#include <greeter/view/Snapshot.h>

#include <QObject>
#include <QString>

#include <atomic>


// Both cross a thread boundary in a queued signal, so Qt has to be able to
// copy them by name.
Q_DECLARE_METATYPE(greeter::view::Snapshot)
Q_DECLARE_METATYPE(greeter::service::FieldRequest)


namespace viewer {

/*
  Runs the simulation somewhere other than the thread that draws.

  A field of any size takes long enough that running it on the thread Qt
  delivers events on would leave a window that cannot be moved, resized or
  closed while it went. So this object is moved onto a thread of its own and
  spoken to entirely in signals.

  Cancelling is the awkward part. A Kokkos parallel region cannot be
  interrupted, so the run is cut into chunks and the flag is looked at
  between them; cancel() is therefore safe to call from the drawing thread
  while a run is going, and is deliberately not a slot, since a queued slot
  would not be delivered until the run it is meant to stop had finished.
*/
class SimulationWorker: public QObject {

  Q_OBJECT

  public:

    explicit SimulationWorker(QObject* parent = nullptr);
    ~SimulationWorker() override;

    /* Safe from any thread, at any time. */
    void cancel() { cancel_requested = true; }

    bool isBusy() const { return busy; }

  public slots:

    /* Reads an input file and hands back the scene, without simulating. */
    void loadInput(const QString& path);

    /* Opens a run somebody else did. Nothing is simulated. */
    void loadSnapshot(const QString& path);

    /* Simulates whatever the loaded input asks for. */
    void run();

    /* Simulates a field in a box of the viewer's choosing. */
    void runField(const greeter::service::FieldRequest& request);

    void writeSnapshot(const QString& path);

  signals:

    void sceneReady(const greeter::view::Snapshot& snapshot, const QString& source);

    void runFinished(const greeter::view::Snapshot& snapshot);

    void progressed(int percent, const QString& what);

    void failed(const QString& message);

    void cancelled();

    void message(const QString& text);

  private:

    friend class WorkerProgress;

    greeter::service::SimulationService service;

    greeter::view::Snapshot latest;

    std::atomic<bool> cancel_requested{false};
    std::atomic<bool> busy{false};
};

}  // namespace viewer

#endif  // VIEWER_SIMULATION_WORKER_H

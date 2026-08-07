#ifndef VIEWER_MAIN_WINDOW_H
#define VIEWER_MAIN_WINDOW_H

#include "SimulationWorker.h"
#include "ViewSettings.h"
#include "Viewport.h"

#include <QMainWindow>
#include <QThread>

class QAction;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QSlider;
class QSpinBox;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;


namespace viewer {

/*
  The window.

  Owns the worker and the thread it runs on, and does nothing itself but
  arrange widgets and pass things between them. The simulation is on the far
  side of the worker, and the drawing is on the far side of the viewport.
*/
class MainWindow: public QMainWindow {

  Q_OBJECT

  public:

    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /* Opens a file named on the command line, once the window is up. */
    void openAtStart(const QString& path);

    /*
      Draws the run to a PNG and quits, instead of waiting to be looked at.

      A picture of a scene is worth having from a machine with no screen, in a
      batch of them, or from a test. It is also the only way to check that the
      viewport draws what it is supposed to without a person looking at it.
    */
    void renderToFileAndQuit(const QString& path);

    /*
      Turns things on and off from outside, so that a drawing made without a
      person can be of something other than the defaults. Names are those of
      the checkboxes: magnets, edges, polarization, slice, arrows, lines,
      box, forces, torques.
    */
    void applyShowList(const QStringList& names);

    void setOpacity(const float& opacity);

    void setSliceAxis(const int& axis);

  protected:

    void closeEvent(QCloseEvent* event) override;

  private slots:

    void onOpenInput();
    void onOpenSnapshot();
    void onSaveSnapshot();
    void onReload();
    void onRun();
    void onCancel();

    void onSceneReady(const greeter::view::Snapshot& snapshot, const QString& source);
    void onRunFinished(const greeter::view::Snapshot& snapshot);
    void onProgressed(int percent, const QString& what);
    void onFailed(const QString& text);
    void onCancelled();
    void onMessage(const QString& text);

    void onSettingsChanged();
    void onSceneSelectionChanged();
    void onMagnetPicked(qint64 id);

  private:

    void buildActions();
    void buildDocks();
    void buildSceneTree();
    void buildForceTable();

    void setBusy(const bool& busy);

    void log(const QString& text);

    ViewSettings readSettings() const;

    /* Picks out a magnet in the tree without asking for it back again. */
    void selectInTree(const int64_t& id);

    Viewport* viewport = nullptr;

    QThread worker_thread;
    SimulationWorker* worker = nullptr;

    greeter::view::Snapshot snapshot;

    QString input_path;

    QAction* open_input_action = nullptr;
    QAction* open_snapshot_action = nullptr;
    QAction* save_snapshot_action = nullptr;
    QAction* reload_action = nullptr;
    QAction* run_action = nullptr;
    QAction* cancel_action = nullptr;
    QAction* frame_action = nullptr;

    QTreeWidget* scene_tree = nullptr;
    QTableWidget* force_table = nullptr;
    QPlainTextEdit* log_view = nullptr;

    QProgressBar* progress = nullptr;
    QLabel* status_label = nullptr;

    QCheckBox* magnets_box = nullptr;
    QCheckBox* magnetization_box = nullptr;
    QCheckBox* outlines_box = nullptr;
    QCheckBox* axes_box = nullptr;
    QCheckBox* field_box_box = nullptr;
    QCheckBox* slice_box = nullptr;
    QCheckBox* glyphs_box = nullptr;
    QCheckBox* streamlines_box = nullptr;
    QCheckBox* forces_box = nullptr;
    QCheckBox* torques_box = nullptr;

    QComboBox* quantity_combo = nullptr;
    QComboBox* scale_combo = nullptr;
    QComboBox* slice_axis_combo = nullptr;

    QSlider* slice_slider = nullptr;
    QSlider* opacity_slider = nullptr;

    QSpinBox* glyph_count_spin = nullptr;
    QSpinBox* seed_count_spin = nullptr;
    QSpinBox* slice_resolution_spin = nullptr;

    QDoubleSpinBox* clip_spin = nullptr;

    bool updating_tree = false;

    /* Where to draw to before quitting, when asked to run without a person. */
    QString screenshot_path;

    /*
      Whether the camera has been framed on a scene that had a field in it.
      Opening a file frames on the magnets, which are usually far smaller than
      the box the field is asked for, so the first field to arrive has to
      frame again. Later runs must not, or the camera would jump back every
      time and undo wherever it had been moved to.
    */
    bool framed_with_field = false;
};

}  // namespace viewer

#endif  // VIEWER_MAIN_WINDOW_H

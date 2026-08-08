#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <cmath>
#include <cstdio>


namespace {

QString formatVector(const float* vector, const char* unit) {
  return QString("(%1, %2, %3) %4")
    .arg((double) vector[0], 0, 'g', 4)
    .arg((double) vector[1], 0, 'g', 4)
    .arg((double) vector[2], 0, 'g', 4)
    .arg(unit);
}

}  // namespace


viewer::MainWindow::MainWindow(QWidget* parent): QMainWindow(parent) {

  setWindowTitle("ParaMagneticS viewer");

  viewport = new viewer::Viewport(this);
  setCentralWidget(viewport);

  worker = new viewer::SimulationWorker();
  worker->moveToThread(&worker_thread);

  connect(&worker_thread, &QThread::finished, worker, &QObject::deleteLater);

  connect(worker, &viewer::SimulationWorker::sceneReady,
          this, &viewer::MainWindow::onSceneReady);
  connect(worker, &viewer::SimulationWorker::runFinished,
          this, &viewer::MainWindow::onRunFinished);
  connect(worker, &viewer::SimulationWorker::progressed,
          this, &viewer::MainWindow::onProgressed);
  connect(worker, &viewer::SimulationWorker::failed,
          this, &viewer::MainWindow::onFailed);
  connect(worker, &viewer::SimulationWorker::cancelled,
          this, &viewer::MainWindow::onCancelled);
  connect(worker, &viewer::SimulationWorker::message,
          this, &viewer::MainWindow::onMessage);

  connect(viewport, &viewer::Viewport::magnetPicked,
          this, &viewer::MainWindow::onMagnetPicked);

  worker_thread.start();

  buildActions();
  buildDocks();

  progress = new QProgressBar(this);
  progress->setMaximumWidth(200);
  progress->setVisible(false);

  status_label = new QLabel("Open an input file to begin", this);

  statusBar()->addWidget(status_label, 1);
  statusBar()->addPermanentWidget(progress);

  resize(1360, 860);
}


viewer::MainWindow::~MainWindow() {

  // The worker holds the simulation, and everything it allocated has to be
  // gone before Kokkos is finalized in main.
  worker->cancel();

  worker_thread.quit();
  worker_thread.wait();
}


void viewer::MainWindow::buildActions() {

  open_input_action = new QAction("&Open input...", this);
  open_input_action->setShortcut(QKeySequence::Open);
  connect(open_input_action, &QAction::triggered, this, &MainWindow::onOpenInput);

  open_snapshot_action = new QAction("Open &snapshot...", this);
  connect(open_snapshot_action, &QAction::triggered, this, &MainWindow::onOpenSnapshot);

  save_snapshot_action = new QAction("Sa&ve snapshot...", this);
  save_snapshot_action->setShortcut(QKeySequence::Save);
  save_snapshot_action->setEnabled(false);
  connect(save_snapshot_action, &QAction::triggered, this, &MainWindow::onSaveSnapshot);

  reload_action = new QAction("&Reload", this);
  reload_action->setShortcut(QKeySequence::Refresh);
  reload_action->setEnabled(false);
  connect(reload_action, &QAction::triggered, this, &MainWindow::onReload);

  run_action = new QAction("&Run", this);
  run_action->setShortcut(Qt::Key_F5);
  run_action->setEnabled(false);
  connect(run_action, &QAction::triggered, this, &MainWindow::onRun);

  cancel_action = new QAction("&Stop", this);
  cancel_action->setEnabled(false);
  connect(cancel_action, &QAction::triggered, this, &MainWindow::onCancel);

  frame_action = new QAction("&Frame scene", this);
  frame_action->setShortcut(Qt::Key_F);
  connect(frame_action, &QAction::triggered, this, [this]() {
    viewport->frameScene();
  });

  QAction* quit_action = new QAction("&Quit", this);
  quit_action->setShortcut(QKeySequence::Quit);
  connect(quit_action, &QAction::triggered, this, &QWidget::close);

  QMenu* file_menu = menuBar()->addMenu("&File");
  file_menu->addAction(open_input_action);
  file_menu->addAction(open_snapshot_action);
  file_menu->addAction(reload_action);
  file_menu->addSeparator();
  file_menu->addAction(save_snapshot_action);
  file_menu->addSeparator();
  file_menu->addAction(quit_action);

  QMenu* run_menu = menuBar()->addMenu("&Simulation");
  run_menu->addAction(run_action);
  run_menu->addAction(cancel_action);

  QMenu* view_menu = menuBar()->addMenu("&View");
  view_menu->addAction(frame_action);

  QToolBar* bar = addToolBar("Main");
  bar->setMovable(false);
  bar->addAction(open_input_action);
  bar->addAction(reload_action);
  bar->addSeparator();
  bar->addAction(run_action);
  bar->addAction(cancel_action);
  bar->addSeparator();
  bar->addAction(frame_action);
  bar->addAction(save_snapshot_action);
}


void viewer::MainWindow::buildDocks() {

  // ---- the scene ----

  scene_tree = new QTreeWidget(this);
  scene_tree->setHeaderLabels({"Scene", "Type"});
  scene_tree->setColumnWidth(0, 190);

  connect(scene_tree, &QTreeWidget::itemSelectionChanged,
          this, &viewer::MainWindow::onSceneSelectionChanged);

  QDockWidget* scene_dock = new QDockWidget("Scene", this);
  scene_dock->setWidget(scene_tree);
  addDockWidget(Qt::LeftDockWidgetArea, scene_dock);

  // ---- what is drawn ----

  QWidget* controls = new QWidget(this);
  QVBoxLayout* layout = new QVBoxLayout(controls);

  auto addCheck = [&](QGroupBox* group, QFormLayout* form, const QString& text,
                      const bool& on) {
    (void) group;
    QCheckBox* box = new QCheckBox(text, controls);
    box->setChecked(on);
    connect(box, &QCheckBox::toggled, this, &viewer::MainWindow::onSettingsChanged);
    form->addRow(box);
    return box;
  };

  QGroupBox* magnet_group = new QGroupBox("Magnets", controls);
  QFormLayout* magnet_form = new QFormLayout(magnet_group);

  magnets_box = addCheck(magnet_group, magnet_form, "Show magnets", true);
  outlines_box = addCheck(magnet_group, magnet_form, "Show edges", true);
  magnetization_box = addCheck(magnet_group, magnet_form, "Show polarization", true);

  opacity_slider = new QSlider(Qt::Horizontal, controls);
  opacity_slider->setRange(10, 100);
  opacity_slider->setValue(100);
  connect(opacity_slider, &QSlider::valueChanged, this, &viewer::MainWindow::onSettingsChanged);
  magnet_form->addRow("Opacity", opacity_slider);

  layout->addWidget(magnet_group);

  QGroupBox* field_group = new QGroupBox("Field", controls);
  QFormLayout* field_form = new QFormLayout(field_group);

  slice_box = addCheck(field_group, field_form, "Slice plane", true);
  glyphs_box = addCheck(field_group, field_form, "Arrows", false);
  streamlines_box = addCheck(field_group, field_form, "Field lines", false);
  field_box_box = addCheck(field_group, field_form, "Field box", true);

  quantity_combo = new QComboBox(controls);
  quantity_combo->addItems({"|B|", "Bx", "By", "Bz"});
  connect(quantity_combo, &QComboBox::currentIndexChanged,
          this, &viewer::MainWindow::onSettingsChanged);
  field_form->addRow("Colour by", quantity_combo);

  scale_combo = new QComboBox(controls);
  scale_combo->addItems({"Logarithmic", "Linear"});
  connect(scale_combo, &QComboBox::currentIndexChanged,
          this, &viewer::MainWindow::onSettingsChanged);
  field_form->addRow("Scale", scale_combo);

  clip_spin = new QDoubleSpinBox(controls);
  clip_spin->setRange(0.50, 1.00);
  clip_spin->setSingleStep(0.01);
  clip_spin->setDecimals(2);
  clip_spin->setValue(0.98);
  clip_spin->setToolTip(
    "The colour scale stops at this quantile of the samples, so that a few "
    "samples against the surface of a magnet do not take the whole of it");
  connect(clip_spin, &QDoubleSpinBox::valueChanged,
          this, &viewer::MainWindow::onSettingsChanged);
  field_form->addRow("Clip at", clip_spin);

  slice_axis_combo = new QComboBox(controls);
  slice_axis_combo->addItems({"across x", "across y", "across z"});
  slice_axis_combo->setCurrentIndex(2);
  connect(slice_axis_combo, &QComboBox::currentIndexChanged,
          this, &viewer::MainWindow::onSettingsChanged);
  field_form->addRow("Plane", slice_axis_combo);

  slice_slider = new QSlider(Qt::Horizontal, controls);
  slice_slider->setRange(0, 100);
  slice_slider->setValue(50);
  connect(slice_slider, &QSlider::valueChanged, this, &viewer::MainWindow::onSettingsChanged);
  field_form->addRow("Position", slice_slider);

  slice_resolution_spin = new QSpinBox(controls);
  slice_resolution_spin->setRange(8, 400);
  slice_resolution_spin->setValue(96);
  connect(slice_resolution_spin, &QSpinBox::valueChanged,
          this, &viewer::MainWindow::onSettingsChanged);
  field_form->addRow("Plane detail", slice_resolution_spin);

  glyph_count_spin = new QSpinBox(controls);
  glyph_count_spin->setRange(2, 24);
  glyph_count_spin->setValue(8);
  connect(glyph_count_spin, &QSpinBox::valueChanged,
          this, &viewer::MainWindow::onSettingsChanged);
  field_form->addRow("Arrows across", glyph_count_spin);

  seed_count_spin = new QSpinBox(controls);
  seed_count_spin->setRange(2, 40);
  seed_count_spin->setValue(12);
  connect(seed_count_spin, &QSpinBox::valueChanged,
          this, &viewer::MainWindow::onSettingsChanged);
  field_form->addRow("Lines across", seed_count_spin);

  layout->addWidget(field_group);

  QGroupBox* force_group = new QGroupBox("Forces", controls);
  QFormLayout* force_form = new QFormLayout(force_group);

  forces_box = addCheck(force_group, force_form, "Show forces", true);
  torques_box = addCheck(force_group, force_form, "Show torques", false);

  layout->addWidget(force_group);

  QLabel* note = new QLabel(
    "Arrow length is logarithmic: forces in one scene run over decades.",
    controls);
  note->setWordWrap(true);
  note->setStyleSheet("color: gray;");
  layout->addWidget(note);

  layout->addStretch(1);

  QDockWidget* controls_dock = new QDockWidget("Display", this);
  controls_dock->setWidget(controls);
  addDockWidget(Qt::RightDockWidgetArea, controls_dock);

  // ---- the numbers ----

  force_table = new QTableWidget(this);
  force_table->setColumnCount(6);
  force_table->setHorizontalHeaderLabels(
    {"id", "|F| [N]", "F [N]", "|T| [N m]", "T [N m]", "cells"});
  force_table->horizontalHeader()->setStretchLastSection(true);
  force_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  force_table->setSelectionBehavior(QAbstractItemView::SelectRows);

  QDockWidget* force_dock = new QDockWidget("Forces and torques", this);
  force_dock->setWidget(force_table);
  addDockWidget(Qt::BottomDockWidgetArea, force_dock);

  log_view = new QPlainTextEdit(this);
  log_view->setReadOnly(true);
  log_view->setMaximumBlockCount(500);

  QDockWidget* log_dock = new QDockWidget("Log", this);
  log_dock->setWidget(log_view);
  addDockWidget(Qt::BottomDockWidgetArea, log_dock);

  tabifyDockWidget(force_dock, log_dock);
  force_dock->raise();
}


viewer::ViewSettings viewer::MainWindow::readSettings() const {

  viewer::ViewSettings settings = viewport->getSettings();

  settings.show_magnets = magnets_box->isChecked();
  settings.show_outlines = outlines_box->isChecked();
  settings.show_magnetization = magnetization_box->isChecked();
  settings.magnet_opacity = (float) opacity_slider->value() / 100.0f;

  settings.show_slice = slice_box->isChecked();
  settings.show_glyphs = glyphs_box->isChecked();
  settings.show_streamlines = streamlines_box->isChecked();
  settings.show_field_box = field_box_box->isChecked();

  settings.show_forces = forces_box->isChecked();
  settings.show_torques = torques_box->isChecked();

  settings.quantity = (viewer::FieldQuantity) quantity_combo->currentIndex();
  settings.scale = (viewer::ColorScale) scale_combo->currentIndex();

  settings.slice_axis = slice_axis_combo->currentIndex();
  settings.slice_position = (float) slice_slider->value() / 100.0f;
  settings.slice_resolution = slice_resolution_spin->value();

  settings.glyph_count = glyph_count_spin->value();
  settings.streamline_seeds = seed_count_spin->value();

  settings.clip_quantile = (float) clip_spin->value();

  return settings;
}


void viewer::MainWindow::onSettingsChanged() {
  viewport->setSettings(readSettings());
}


void viewer::MainWindow::onOpenInput() {

  const QString path = QFileDialog::getOpenFileName(
    this, "Open an input file", QString(), "Input files (*.json);;All files (*)");

  if (path.isEmpty()) {
    return;
  }

  openAtStart(path);
}


void viewer::MainWindow::openAtStart(const QString& path) {

  if (path.isEmpty()) {
    return;
  }

  const bool is_snapshot = path.endsWith(".pmsnap", Qt::CaseInsensitive);

  setBusy(true);

  if (is_snapshot) {
    QMetaObject::invokeMethod(worker, "loadSnapshot", Qt::QueuedConnection,
                              Q_ARG(QString, path));
    return;
  }

  input_path = path;

  QMetaObject::invokeMethod(worker, "loadInput", Qt::QueuedConnection,
                            Q_ARG(QString, path));
}


void viewer::MainWindow::onOpenSnapshot() {

  const QString path = QFileDialog::getOpenFileName(
    this, "Open a snapshot", QString(),
    "Snapshots (*.pmsnap *.json);;All files (*)");

  if (path.isEmpty()) {
    return;
  }

  setBusy(true);

  QMetaObject::invokeMethod(worker, "loadSnapshot", Qt::QueuedConnection,
                            Q_ARG(QString, path));
}


void viewer::MainWindow::onSaveSnapshot() {

  const QString path = QFileDialog::getSaveFileName(
    this, "Save a snapshot", "run.pmsnap",
    "Snapshot (*.pmsnap);;Readable snapshot (*.json)");

  if (path.isEmpty()) {
    return;
  }

  QMetaObject::invokeMethod(worker, "writeSnapshot", Qt::QueuedConnection,
                            Q_ARG(QString, path));
}


void viewer::MainWindow::onReload() {

  if (input_path.isEmpty()) {
    return;
  }

  openAtStart(input_path);
}


void viewer::MainWindow::onRun() {

  setBusy(true);

  log("Running");

  QMetaObject::invokeMethod(worker, "run", Qt::QueuedConnection);
}


void viewer::MainWindow::onCancel() {

  // Straight through, not queued: a queued call would sit behind the run it
  // is meant to stop.
  worker->cancel();

  log("Stopping");
}


void viewer::MainWindow::onSceneReady(const greeter::view::Snapshot& given,
                                      const QString& source) {

  snapshot = given;

  setBusy(false);

  const bool from_snapshot = source.endsWith(".pmsnap", Qt::CaseInsensitive);

  run_action->setEnabled(!from_snapshot);
  reload_action->setEnabled(!input_path.isEmpty());
  save_snapshot_action->setEnabled(true);

  setWindowTitle(QString("ParaMagneticS viewer - %1").arg(QFileInfo(source).fileName()));

  viewport->setSnapshot(snapshot);
  viewport->frameScene();

  framed_with_field = snapshot.hasField();

  buildSceneTree();
  buildForceTable();

  status_label->setText(QString("%1 magnets").arg(snapshot.scene.magnets.size()));

  // Asked to draw and quit, there is nobody to press Run. A snapshot already
  // carries its results and goes straight to the drawing.
  if (!screenshot_path.isEmpty() && !from_snapshot) {
    onRun();
  }
}


void viewer::MainWindow::renderToFileAndQuit(const QString& path) {
  screenshot_path = path;
}


void viewer::MainWindow::applyShowList(const QStringList& names) {

  const QMap<QString, QCheckBox*> boxes = {
    {"magnets", magnets_box},
    {"edges", outlines_box},
    {"polarization", magnetization_box},
    {"slice", slice_box},
    {"arrows", glyphs_box},
    {"lines", streamlines_box},
    {"box", field_box_box},
    {"forces", forces_box},
    {"torques", torques_box}
  };

  // Naming any of them at all means that the ones not named are off, so that
  // "--show lines" gives a picture of field lines and nothing else.
  for (auto it = boxes.begin(); it != boxes.end(); ++it) {
    it.value()->setChecked(false);
  }

  for (const auto& name : names) {

    const QString trimmed = name.trimmed().toLower();

    if (boxes.contains(trimmed)) {
      boxes[trimmed]->setChecked(true);
    } else {
      log(QString("There is nothing called \"%1\" to show").arg(trimmed));
    }
  }

  onSettingsChanged();
}


void viewer::MainWindow::setOpacity(const float& opacity) {

  opacity_slider->setValue((int) (100.0f * std::max(0.1f, std::min(1.0f, opacity))));

  onSettingsChanged();
}


void viewer::MainWindow::setSliceAxis(const int& axis) {

  slice_axis_combo->setCurrentIndex(std::max(0, std::min(2, axis)));

  onSettingsChanged();
}


void viewer::MainWindow::onRunFinished(const greeter::view::Snapshot& given) {

  snapshot = given;

  setBusy(false);

  viewport->setSnapshot(snapshot);

  // The box a field was asked for is usually far larger than the magnets, so
  // a camera framed on the magnets alone is left inside it. Only the first
  // time, so that a second run does not undo wherever the camera was moved.
  if (!framed_with_field && snapshot.hasField()) {
    viewport->frameScene();
    framed_with_field = true;
  }

  buildForceTable();

  if (!screenshot_path.isEmpty()) {

    // After the event loop has been round once more, so that the viewport has
    // drawn the snapshot it was just handed rather than the one before it.
    QTimer::singleShot(0, this, [this]() {

      viewport->repaint();

      const QImage image = viewport->grabFramebuffer();

      if (image.save(screenshot_path)) {
        std::fprintf(stderr, "Drawn to %s\n", qPrintable(screenshot_path));
      } else {
        std::fprintf(stderr, "Could not write %s\n", qPrintable(screenshot_path));
      }

      QApplication::quit();
    });
  }

  QString said = QString("%1 magnets").arg(snapshot.scene.magnets.size());

  if (snapshot.hasField()) {
    said += QString(", %1 field samples").arg(snapshot.field.size());
  }

  if (snapshot.hasForces()) {
    said += QString(", %1 force targets").arg(snapshot.forces.entries.size());
  }

  status_label->setText(said);

  log(said);
}


void viewer::MainWindow::onProgressed(int percent, const QString& what) {

  progress->setVisible(true);
  progress->setValue(percent);

  status_label->setText(QString("%1: %2%").arg(what).arg(percent));
}


void viewer::MainWindow::onFailed(const QString& text) {

  setBusy(false);

  log("Error: " + text);

  QMessageBox::warning(this, "ParaMagneticS", text);

  status_label->setText("Failed");
}


void viewer::MainWindow::onCancelled() {

  setBusy(false);

  log("Stopped");

  status_label->setText("Stopped");
}


void viewer::MainWindow::onMessage(const QString& text) {
  log(text);
}


void viewer::MainWindow::setBusy(const bool& busy) {

  progress->setVisible(busy);

  if (!busy) {
    progress->setValue(0);
  }

  cancel_action->setEnabled(busy);

  open_input_action->setEnabled(!busy);
  open_snapshot_action->setEnabled(!busy);
  reload_action->setEnabled(!busy && !input_path.isEmpty());
  run_action->setEnabled(!busy && !input_path.isEmpty());
}


void viewer::MainWindow::log(const QString& text) {
  log_view->appendPlainText(text);
}


void viewer::MainWindow::buildSceneTree() {

  updating_tree = true;

  scene_tree->clear();

  // Magnets an arrangement generated hang under it, so that a ring of
  // sixteen reads as one thing rather than as sixteen.
  QMap<qint64, QTreeWidgetItem*> parents;

  for (const auto& arrangement : snapshot.scene.arrangements) {

    QTreeWidgetItem* item = new QTreeWidgetItem(scene_tree);

    item->setText(0, QString("arrangement %1 (%2 magnets)")
                       .arg(arrangement.id)
                       .arg(arrangement.members.size()));
    item->setText(1, QString::fromStdString(arrangement.type));
    item->setData(0, Qt::UserRole, (qint64) 0);

    parents.insert((qint64) arrangement.id, item);
  }

  for (const auto& magnet : snapshot.scene.magnets) {

    QTreeWidgetItem* item = parents.contains((qint64) magnet.arrangement_id)
      ? new QTreeWidgetItem(parents[(qint64) magnet.arrangement_id])
      : new QTreeWidgetItem(scene_tree);

    item->setText(0, QString("magnet %1").arg(magnet.id));
    item->setText(1, QString::fromStdString(magnet.shape.type_name));
    item->setData(0, Qt::UserRole, (qint64) magnet.id);

    // A dipole carries a moment in ampere metre squared, everything else a
    // polarization in Tesla. The source says which, so this does not have to
    // guess from the type name.
    item->setToolTip(0, QString("at %1\n%2 %3")
                          .arg(formatVector(magnet.position, "m"))
                          .arg(QString::fromStdString(
                                 greeter::view::getName(magnet.moment_kind)))
                          .arg(formatVector(
                                 magnet.magnetization,
                                 greeter::view::getUnit(magnet.moment_kind).c_str())));
  }

  scene_tree->expandAll();

  updating_tree = false;
}


void viewer::MainWindow::buildForceTable() {

  force_table->setRowCount((int) snapshot.forces.entries.size());

  int row = 0;

  for (const auto& entry : snapshot.forces.entries) {

    force_table->setItem(row, 0, new QTableWidgetItem(QString::number(entry.id)));

    force_table->setItem(row, 1, new QTableWidgetItem(
      QString::number((double) entry.getForceMagnitude(), 'g', 5)));

    force_table->setItem(row, 2, new QTableWidgetItem(
      formatVector(entry.force, "")));

    force_table->setItem(row, 3, new QTableWidgetItem(
      QString::number((double) entry.getTorqueMagnitude(), 'g', 5)));

    force_table->setItem(row, 4, new QTableWidgetItem(
      formatVector(entry.torque, "")));

    force_table->setItem(row, 5, new QTableWidgetItem(QString::number(entry.cells)));

    row++;
  }

  force_table->resizeColumnsToContents();
}


void viewer::MainWindow::onSceneSelectionChanged() {

  if (updating_tree) {
    return;
  }

  const QList<QTreeWidgetItem*> chosen = scene_tree->selectedItems();

  viewer::ViewSettings settings = readSettings();

  settings.selected_id = chosen.isEmpty()
    ? 0 : chosen.first()->data(0, Qt::UserRole).toLongLong();

  viewport->setSettings(settings);
}


void viewer::MainWindow::onMagnetPicked(qint64 id) {

  viewer::ViewSettings settings = readSettings();
  settings.selected_id = id;

  viewport->setSettings(settings);

  selectInTree(id);
}


void viewer::MainWindow::selectInTree(const int64_t& id) {

  updating_tree = true;

  scene_tree->clearSelection();

  if (id != 0) {

    const QList<QTreeWidgetItem*> found = scene_tree->findItems(
      QString("magnet %1").arg(id), Qt::MatchExactly | Qt::MatchRecursive, 0);

    if (!found.isEmpty()) {
      found.first()->setSelected(true);
      scene_tree->scrollToItem(found.first());
    }
  }

  updating_tree = false;
}


void viewer::MainWindow::closeEvent(QCloseEvent* event) {

  worker->cancel();

  event->accept();
}

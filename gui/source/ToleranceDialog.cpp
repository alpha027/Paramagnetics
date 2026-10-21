#include "ToleranceDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
  using namespace greeter::service;

  class Histogram : public QWidget {
  public:
    std::vector<double> values;
    double limit = 0.0;
    explicit Histogram(QWidget* parent) : QWidget(parent) { setMinimumHeight(125); }

  protected:
    void paintEvent(QPaintEvent*) override {
      QPainter painter(this);
      painter.fillRect(rect(), palette().base());
      painter.setPen(palette().text().color());
      if (values.empty()) {
        painter.drawText(rect(), Qt::AlignCenter, "No valid trials to plot");
        return;
      }
      double low = *std::min_element(values.begin(), values.end());
      double high = *std::max_element(values.begin(), values.end());
      if (high <= low) {
        painter.drawText(
            rect(), Qt::AlignCenter,
            QString("All %1 valid trials: %2 ppm").arg(values.size()).arg(low, 0, 'g', 8));
        return;
      }
      const QRectF plot(45, 12, std::max(1, width() - 65), std::max(1, height() - 48));
      int bins[20] = {};
      for (double value : values) ++bins[std::min(19, (int)(20 * (value - low) / (high - low)))];
      const int peak = *std::max_element(bins, bins + 20);
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(48, 139, 185));
      for (int i = 0; i < 20; ++i) {
        const double h = plot.height() * bins[i] / peak;
        painter.drawRect(QRectF(plot.left() + i * plot.width() / 20, plot.bottom() - h,
                                plot.width() / 20 - 1, h));
      }
      if (limit > 0 && limit >= low && limit <= high) {
        painter.setPen(QPen(QColor(205, 65, 65), 2, Qt::DashLine));
        const double x = plot.left() + (limit - low) / (high - low) * plot.width();
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
      }
      painter.setPen(palette().text().color());
      painter.drawText(QRectF(0, plot.top(), 40, 20), Qt::AlignRight, QString::number(peak));
      painter.drawText(QRectF(plot.left(), plot.bottom() + 3, plot.width(), 20), Qt::AlignLeft,
                       QString::number(low, 'g', 5));
      painter.drawText(QRectF(plot.left(), plot.bottom() + 3, plot.width(), 20), Qt::AlignRight,
                       QString::number(high, 'g', 5));
      painter.drawText(QRectF(plot.left(), height() - 18, plot.width(), 18), Qt::AlignCenter,
                       "Homogeneity [ppm] — trial counts");
    }
  };

  QDoubleSpinBox* toleranceSpin(QWidget* parent, const char* name, double maximum, double value) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setObjectName(name);
    spin->setRange(0, maximum);
    spin->setDecimals(4);
    spin->setValue(value);
    return spin;
  }

  void cell(QTableWidget* table, int row, int column, const QString& text) {
    table->setItem(row, column, new QTableWidgetItem(text));
  }
  QString number(double value) { return QString::number(value, 'g', 8); }
}  // namespace

viewer::ToleranceDialog::ToleranceDialog(QWidget* parent) : QDialog(parent) {
  setObjectName("toleranceDialog");
  setWindowTitle("Tolerance and robustness analysis");
  resize(1060, 760);
  auto* layout = new QVBoxLayout(this);
  tabs = new QTabWidget(this);
  layout->addWidget(tabs);
  controls = new QWidget(tabs);
  auto* form = new QFormLayout(controls);
  domain = new QLabel(controls);
  domain->setWordWrap(true);
  form->addRow(domain);
  trials = new QSpinBox(controls);
  trials->setObjectName("tolTrials");
  trials->setRange(1, 10000);
  trials->setValue(100);
  seed = new QLineEdit("42", controls);
  seed->setObjectName("tolSeed");
  distribution = new QComboBox(controls);
  distribution->setObjectName("tolDistribution");
  distribution->addItems({"Uniform (± tolerance)", "Normal (tolerance = one standard deviation)"});
  region = new QComboBox(controls);
  region->setObjectName("tolRegion");
  region->addItems({"Input field-of-view box", "Sphere inscribed in the input box"});
  density = new QSpinBox(controls);
  density->setObjectName("tolDensity");
  density->setRange(2, 101);
  density->setSingleStep(2);
  density->setValue(11);
  strength = toleranceSpin(controls, "tolStrength", 100, 1);
  position = toleranceSpin(controls, "tolPosition", 100000, 0.1);
  direction = toleranceSpin(controls, "tolDirection", 180, 1);
  tilt = toleranceSpin(controls, "tolTilt", 180, 0);
  size = toleranceSpin(controls, "tolSize", 100, 0);
  ppm_limit = toleranceSpin(controls, "tolPpmLimit", 1e12, 0);
  minimum_field = toleranceSpin(controls, "tolMinimumField", 1e9, 0);
  ppm_limit->setSpecialValueText("Disabled");
  minimum_field->setSpecialValueText("Disabled");
  form->addRow("Random assemblies", trials);
  form->addRow("Random seed", seed);
  form->addRow("Distribution", distribution);
  form->addRow("Measurement region", region);
  form->addRow("Grid samples per axis", density);
  form->addRow("Strength / remanence error [%]", strength);
  form->addRow("Position error per global axis [mm]", position);
  form->addRow("Magnetization direction error per local axis [°]", direction);
  form->addRow("Body tilt error per global axis [°]", tilt);
  form->addRow("Isotropic size error [%]", size);
  form->addRow("Accept homogeneity up to [ppm]", ppm_limit);
  form->addRow("Require mean |B| of at least [mT]", minimum_field);
  auto* note = new QLabel(
      "Errors are independent for each magnet, including arrangement members. "
      "Angles describe rotation-vector components. Size scales each body about its local origin; "
      "point dipoles have no size. Nonpositive normal draws are counted as invalid assemblies, not "
      "resampled. "
      "The study evaluates |B| on the selected grid; it does not check collisions, temperature or "
      "material response.",
      controls);
  note->setWordWrap(true);
  form->addRow(note);
  tabs->addTab(controls, "Setup");

  auto* results = new QWidget(tabs);
  auto* results_layout = new QVBoxLayout(results);
  summary = new QLabel("Run a study to compare manufactured assemblies with the nominal design.",
                       results);
  summary->setObjectName("tolSummary");
  summary->setWordWrap(true);
  results_layout->addWidget(summary);
  statistics = new QTableWidget(0, 9, results);
  statistics->setObjectName("tolStatistics");
  statistics->setHorizontalHeaderLabels(
      {"Metric", "Nominal", "Mean", "Std dev", "P5", "Median", "P95", "Min", "Max"});
  statistics->setEditTriggers(QAbstractItemView::NoEditTriggers);
  statistics->setMaximumHeight(150);
  statistics->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  results_layout->addWidget(statistics);
  histogram = new Histogram(results);
  results_layout->addWidget(histogram);
  trial_table = new QTableWidget(0, 6, results);
  trial_table->setObjectName("tolTrialsTable");
  trial_table->setHorizontalHeaderLabels({"Assembly", "Mean |B| [mT]", "Homogeneity [ppm]",
                                          "Field shift [ppm]", "Acceptance", "Error"});
  trial_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  trial_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  trial_table->horizontalHeader()->setStretchLastSection(true);
  results_layout->addWidget(trial_table);
  tabs->addTab(results, "Results");
  status = new QLabel(this);
  status->setObjectName("tolStatus");
  status->setWordWrap(true);
  layout->addWidget(status);
  progress = new QProgressBar(this);
  progress->setObjectName("tolProgress");
  progress->hide();
  layout->addWidget(progress);
  auto* buttons = new QDialogButtonBox(this);
  run = buttons->addButton("Run analysis", QDialogButtonBox::ActionRole);
  run->setObjectName("tolRun");
  stop = buttons->addButton("Stop", QDialogButtonBox::ActionRole);
  stop->setObjectName("tolStop");
  csv = buttons->addButton("Export CSV…", QDialogButtonBox::ActionRole);
  csv->setObjectName("tolExportCsv");
  json = buttons->addButton("Export JSON…", QDialogButtonBox::ActionRole);
  json->setObjectName("tolExportJson");
  buttons->addButton(QDialogButtonBox::Close);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, this, &ToleranceDialog::reject);
  connect(run, &QPushButton::clicked, this, [this]() {
    try {
      const auto request = settings();
      status->clear();
      emit analysisRequested(request);
    } catch (const std::exception& error) {
      status->setText(QString::fromUtf8(error.what()));
    }
  });
  connect(stop, &QPushButton::clicked, this, &ToleranceDialog::cancelRequested);
  connect(csv, &QPushButton::clicked, this, [this]() { exportReport(false); });
  connect(json, &QPushButton::clicked, this, [this]() { exportReport(true); });
  setDomain(false, FieldRequest());
}

greeter::service::ToleranceSettings viewer::ToleranceDialog::settings() const {
  ToleranceSettings s;
  bool ok = false;
  s.seed = seed->text().toULongLong(&ok);
  if (!ok || seed->text().trimmed().startsWith('-'))
    throw std::invalid_argument("Seed must be an unsigned 64-bit integer");
  s.trials = (uint32_t)trials->value();
  s.points_per_axis = (uint32_t)density->value();
  s.distribution = distribution->currentIndex() == 0 ? ToleranceDistribution::Uniform
                                                     : ToleranceDistribution::Normal;
  s.region = region->currentIndex() == 0 ? ToleranceRegion::Box : ToleranceRegion::InscribedSphere;
  s.strength_percent = strength->value();
  s.position_mm = position->value();
  s.direction_degrees = direction->value();
  s.body_tilt_degrees = tilt->value();
  s.size_percent = size->value();
  s.homogeneity_limit_ppm = ppm_limit->value();
  s.minimum_mean_T = minimum_field->value() / 1000.0;
  s.validate();
  return s;
}

void viewer::ToleranceDialog::setDomain(bool enabled, const FieldRequest& request) {
  available = enabled;
  has_report = false;
  report = ToleranceReport();
  statistics->setRowCount(0);
  trial_table->setRowCount(0);
  static_cast<Histogram*>(histogram)->values.clear();
  histogram->update();
  summary->setText("Run a study to compare manufactured assemblies with the nominal design.");
  tabs->setCurrentIndex(0);
  if (enabled) {
    domain->setText(QString("Input bounds [mm]: x %1 … %2, y %3 … %4, z %5 … %6. "
                            "The analysis grid is separate from the displayed field; check results "
                            "at finer densities.")
                        .arg(1000.0 * request.bounds[0])
                        .arg(1000.0 * request.bounds[1])
                        .arg(1000.0 * request.bounds[2])
                        .arg(1000.0 * request.bounds[3])
                        .arg(1000.0 * request.bounds[4])
                        .arg(1000.0 * request.bounds[5]));
    status->clear();
  } else {
    domain->setText(
        "Open an input JSON scene containing a field_of_view to run tolerance analysis. "
        "Saved snapshots are read-only.");
  }
  updateButtons();
}

void viewer::ToleranceDialog::updateButtons() {
  controls->setEnabled(available && !running);
  run->setEnabled(available && !running);
  stop->setEnabled(running);
  csv->setEnabled(has_report && !running);
  json->setEnabled(has_report && !running);
}

void viewer::ToleranceDialog::setRunning(bool value) {
  running = value;
  progress->setVisible(value);
  if (value) {
    progress->setValue(0);
    status->setText("Working; Stop cancels between field chunks.");
  } else
    status->clear();
  updateButtons();
}

void viewer::ToleranceDialog::setProgress(int percent) { progress->setValue(percent); }

void viewer::ToleranceDialog::setReport(const ToleranceReport& given) {
  if (given.cancelled || given.trials.size() != given.settings.trials) return;
  report = given;
  has_report = true;
  const auto& r = report;
  QString text = QString(
                     "%1 trials · seed %2 · %3 magnets · %4 field points · %5 invalid trials. "
                     "Statistics describe the %6 valid trials.")
                     .arg(r.trials.size())
                     .arg((qulonglong)r.settings.seed)
                     .arg(r.magnets)
                     .arg(r.points)
                     .arg(r.trials.size() - r.valid_trials)
                     .arg(r.valid_trials);
  if (r.settings.hasLimits())
    text += QString(" Acceptance: %1/%2 (%3%); invalid trials count as failures.")
                .arg(r.passed_trials)
                .arg(r.trials.size())
                .arg(100.0 * r.passed_trials / r.trials.size(), 0, 'f', 1);
  else
    text += " Acceptance limits are disabled.";
  text += QString(
              " %1 errors: strength %2%, position %3 mm/axis, direction %4°/axis, body tilt "
              "%5°/axis, size %6%.")
              .arg(r.settings.distribution == ToleranceDistribution::Uniform ? "Uniform ±"
                                                                             : "Normal 1σ")
              .arg(r.settings.strength_percent)
              .arg(r.settings.position_mm)
              .arg(r.settings.direction_degrees)
              .arg(r.settings.body_tilt_degrees)
              .arg(r.settings.size_percent);
  summary->setText(text);
  statistics->setRowCount(r.valid_trials > 0 ? 3 : 0);
  auto stats = [&](int row, const QString& name, double nominal, const ToleranceStatistics& s,
                   double scale) {
    cell(statistics, row, 0, name);
    cell(statistics, row, 1, number(nominal * scale));
    const double values[] = {s.mean, s.stddev, s.p05, s.median, s.p95, s.min, s.max};
    for (int column = 0; column < 7; ++column)
      cell(statistics, row, column + 2, number(values[column] * scale));
  };
  if (r.valid_trials) {
    stats(0, "Mean |B| [mT]", r.nominal.mean_T, r.mean_field, 1000);
    stats(1, "Homogeneity [ppm]", r.nominal.homogeneity_ppm, r.homogeneity, 1);
    stats(2, "Mean-field shift [ppm]", 0, r.field_shift, 1);
  }
  auto* plot = static_cast<Histogram*>(histogram);
  plot->values.clear();
  plot->limit = r.settings.homogeneity_limit_ppm;
  trial_table->setRowCount((int)r.trials.size() + 1);
  auto row = [&](int index, const QString& label, const ToleranceMetrics& m, bool valid,
                 bool passed, const std::string& error) {
    cell(trial_table, index, 0, label);
    cell(trial_table, index, 1, valid ? number(1000 * m.mean_T) : "—");
    cell(trial_table, index, 2, valid ? number(m.homogeneity_ppm) : "—");
    cell(trial_table, index, 3, valid ? number(m.field_shift_ppm) : "—");
    cell(trial_table, index, 4,
         !valid                    ? "Invalid"
         : !r.settings.hasLimits() ? "Disabled"
         : passed                  ? "Pass"
                                   : "Fail");
    cell(trial_table, index, 5, QString::fromStdString(error));
  };
  const bool nominal_pass = (r.settings.homogeneity_limit_ppm <= 0
                             || r.nominal.homogeneity_ppm <= r.settings.homogeneity_limit_ppm)
                            && r.nominal.mean_T >= r.settings.minimum_mean_T;
  row(0, "Nominal", r.nominal, true, nominal_pass, {});
  for (size_t i = 0; i < r.trials.size(); ++i) {
    const auto& t = r.trials[i];
    row((int)i + 1, QString::number(t.trial), t.metrics, t.valid, t.passed, t.error);
    if (t.valid) plot->values.push_back(t.metrics.homogeneity_ppm);
  }
  histogram->update();
  tabs->setCurrentIndex(1);
  updateButtons();
}

void viewer::ToleranceDialog::exportReport(bool as_json) {
  const QString path = QFileDialog::getSaveFileName(
      this, "Export tolerance study", as_json ? "tolerance.json" : "tolerance.csv",
      as_json ? "JSON report (*.json)" : "CSV results (*.csv)");
  if (path.isEmpty()) return;
  try {
    if (!report.source.empty()
        && QFileInfo(path).canonicalFilePath()
               == QFileInfo(QString::fromStdString(report.source)).canonicalFilePath()
        && !QFileInfo(path).canonicalFilePath().isEmpty())
      throw std::invalid_argument("Choose a destination other than the input scene");
    if (as_json)
      writeToleranceJSON(report, path.toStdString());
    else
      writeToleranceCSV(report, path.toStdString());
    status->setText("Report exported to " + path);
  } catch (const std::exception& error) {
    status->setText(QString::fromUtf8(error.what()));
  }
}

void viewer::ToleranceDialog::reject() {
  if (running) emit cancelRequested();
  QDialog::reject();
}

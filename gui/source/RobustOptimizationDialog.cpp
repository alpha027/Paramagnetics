#include "RobustOptimizationDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <stdexcept>

namespace {
  using namespace greeter::service;
  QString number(double v) { return QString::number(v, 'g', 8); }
  void cell(QTableWidget* t, int r, int c, const QString& v) {
    t->setItem(r, c, new QTableWidgetItem(v));
  }
  QSpinBox* count(QWidget* parent, const char* name, int low, int high, int value) {
    auto* s = new QSpinBox(parent);
    s->setObjectName(name);
    s->setRange(low, high);
    s->setValue(value);
    return s;
  }
  QDoubleSpinBox* amount(QWidget* parent, const char* name, double high, double value) {
    auto* s = new QDoubleSpinBox(parent);
    s->setObjectName(name);
    s->setRange(0, high);
    s->setDecimals(4);
    s->setValue(value);
    return s;
  }
  QLineEdit* seedEdit(QWidget* parent, const char* name, const QString& value) {
    auto* s = new QLineEdit(value, parent);
    s->setObjectName(name);
    return s;
  }
  uint64_t readSeed(QLineEdit* edit) {
    bool ok = false;
    auto value = edit->text().toULongLong(&ok);
    if (!ok || edit->text().trimmed().startsWith('-'))
      throw std::invalid_argument("Seeds must be unsigned 64-bit integers");
    return value;
  }
  void tableSetup(QTableWidget* table, const QStringList& headers) {
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  }
}  // namespace

viewer::RobustOptimizationDialog::RobustOptimizationDialog(QWidget* parent) : QDialog(parent) {
  setObjectName("robustOptimizationDialog");
  setWindowTitle("Robust optimization");
  resize(1120, 820);
  auto* layout = new QVBoxLayout(this);
  tabs = new QTabWidget(this);
  layout->addWidget(tabs);
  controls = new QWidget(tabs);
  auto* form = new QFormLayout(controls);
  auto* browse = new QPushButton("Choose optimizer configuration…", controls);
  browse->setObjectName("robustBrowse");
  config_label = new QLabel(
      "Choose a Halbach optimizer configuration or an exported optimized design. A scene without "
      "optimizer metadata cannot define a search.",
      controls);
  config_label->setObjectName("robustConfiguration");
  config_label->setWordWrap(true);
  form->addRow(browse);
  form->addRow(config_label);
  connect(browse, &QPushButton::clicked, this, &RobustOptimizationDialog::chooseConfiguration);
  objective = new QComboBox(controls);
  objective->setObjectName("robustObjective");
  objective->addItems(
      {"Minimize P95 homogeneity", "Maximize P5 mean field", "Maximize acceptance rate"});
  shortlist = count(controls, "robustShortlist", 1, 32, 8);
  population = count(controls, "robustPopulation", 2, 100000, 256);
  generations = count(controls, "robustGenerations", 1, 10000, 30);
  search_seed = seedEdit(controls, "robustSearchSeed", "42");
  trials = count(controls, "robustTrials", 2, 10000, 50);
  validation_trials = count(controls, "robustValidationTrials", 2, 10000, 200);
  seed = seedEdit(controls, "robustSeed", "42");
  validation_seed = seedEdit(controls, "robustValidationSeed", "2026");
  density = count(controls, "robustDensity", 3, 101, 7);
  density->setSingleStep(2);
  validation_density = count(controls, "robustValidationDensity", 3, 101, 11);
  validation_density->setSingleStep(2);
  distribution = new QComboBox(controls);
  distribution->setObjectName("robustDistribution");
  distribution->addItems({"Uniform (± tolerance)", "Normal (one standard deviation)"});
  strength = amount(controls, "robustStrength", 100, 1);
  position = amount(controls, "robustPosition", 100000, .1);
  direction = amount(controls, "robustDirection", 180, 1);
  tilt = amount(controls, "robustTilt", 180, 0);
  size = amount(controls, "robustSize", 100, 0);
  ppm_limit = amount(controls, "robustPpmLimit", 1e12, 0);
  ppm_limit->setSpecialValueText("Disabled");
  minimum_field = amount(controls, "robustMinimumField", 1e9, 0);
  minimum_field->setSpecialValueText("Disabled");
  acceptance = amount(controls, "robustAcceptance", 100, 95);
  acceptance->setEnabled(false);
  connect(objective, &QComboBox::currentIndexChanged, this,
          [this](int i) { acceptance->setEnabled(i == 2); });
  form->addRow("Robust objective", objective);
  form->addRow("Distinct nominal finalists (maximum)", shortlist);
  form->addRow("Nominal search population", population);
  form->addRow("Nominal search generations", generations);
  form->addRow("Nominal search seed", search_seed);
  form->addRow("Screening assemblies per finalist", trials);
  form->addRow("Screening seed", seed);
  form->addRow("Screening grid samples per axis (odd)", density);
  form->addRow("Fresh validation assemblies", validation_trials);
  form->addRow("Fresh validation seed", validation_seed);
  form->addRow("Validation grid samples per axis (odd)", validation_density);
  form->addRow("Error distribution", distribution);
  form->addRow("Strength / remanence error [%]", strength);
  form->addRow("Position error per global axis [mm]", position);
  form->addRow("Magnetization direction error per local axis [°]", direction);
  form->addRow("Body tilt error per global axis [°]", tilt);
  form->addRow("Isotropic size error [%]", size);
  form->addRow("Maximum homogeneity [ppm]", ppm_limit);
  form->addRow("Minimum mean |B| [mT]", minimum_field);
  form->addRow("Required acceptance for acceptance-rate objective [%]", acceptance);
  auto* note = new QLabel(
      "Nominal search uses |B| and the configuration's geometry, search grid, symmetry and genetic "
      "operators; population, generations and seed above override the file. Finalists are screened "
      "on the full DSV sphere. P95/P5 ranking requires all trials valid and applies limits to P95 "
      "homogeneity and P5 field separately. Acceptance-rate ranking counts invalid trials as "
      "failures. Independent per-magnet errors match the tolerance study; size does not affect "
      "dipoles. The winner is validated once with fresh random streams; validation never changes "
      "the selection. Start with small trial counts and grids, then increase both. This is "
      "empirical shortlist screening, not a global optimum or guaranteed yield. No collision, "
      "thermal or material-response checks.",
      controls);
  note->setWordWrap(true);
  form->addRow(note);
  auto* scroll = new QScrollArea(tabs);
  scroll->setWidgetResizable(true);
  scroll->setWidget(controls);
  tabs->addTab(scroll, "Setup");
  auto* results = new QWidget(tabs);
  auto* results_layout = new QVBoxLayout(results);
  summary = new QLabel(
      "Run a search to compare nominal finalists and validate the selected design.", results);
  summary->setObjectName("robustSummary");
  summary->setWordWrap(true);
  results_layout->addWidget(summary);
  candidates = new QTableWidget(results);
  candidates->setObjectName("robustCandidates");
  tableSetup(candidates,
             {"Rank", "Nominal rank", "Selected", "Genome", "Nominal |B| [mT]", "Nominal ppm",
              "P95 ppm", "P5 mean [mT]", "Acceptance", "Valid / total", "Status"});
  results_layout->addWidget(candidates, 2);
  auto* validation_label
      = new QLabel("Selected design: screening versus fresh validation", results);
  results_layout->addWidget(validation_label);
  validation = new QTableWidget(results);
  validation->setObjectName("robustValidation");
  tableSetup(validation, {"Study", "Seed", "Points", "P95 homogeneity [ppm]", "P5 mean |B| [mT]",
                          "Acceptance", "Valid / total", "Requirements met"});
  results_layout->addWidget(validation, 1);
  auto* interpretation = new QLabel(
      "Nominal rank uses the search lattice. Nominal ppm and P95 ppm denote |B| homogeneity. The "
      "comparison table's nominal metrics use the "
      "screening sphere. Validation may use a finer grid. Requirements are empirical: repeat at "
      "more trials and finer density before relying on them. Failed requirements are retained in "
      "the exported design and report.",
      results);
  interpretation->setWordWrap(true);
  results_layout->addWidget(interpretation);
  tabs->addTab(results, "Candidates and validation");
  status = new QLabel(this);
  status->setObjectName("robustStatus");
  status->setWordWrap(true);
  layout->addWidget(status);
  progress = new QProgressBar(this);
  progress->setObjectName("robustProgress");
  progress->hide();
  layout->addWidget(progress);
  auto* buttons = new QDialogButtonBox(this);
  run = buttons->addButton("Run robust optimization", QDialogButtonBox::ActionRole);
  run->setObjectName("robustRun");
  stop = buttons->addButton("Stop", QDialogButtonBox::ActionRole);
  stop->setObjectName("robustStop");
  design = buttons->addButton("Export and view design…", QDialogButtonBox::ActionRole);
  design->setObjectName("robustExportDesign");
  csv = buttons->addButton("Export CSV…", QDialogButtonBox::ActionRole);
  csv->setObjectName("robustExportCsv");
  json = buttons->addButton("Export JSON…", QDialogButtonBox::ActionRole);
  json->setObjectName("robustExportJson");
  buttons->addButton(QDialogButtonBox::Close);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, this, &RobustOptimizationDialog::reject);
  connect(stop, &QPushButton::clicked, this, &RobustOptimizationDialog::cancelRequested);
  connect(run, &QPushButton::clicked, this, [this]() {
    try {
      auto r = request();
      status->clear();
      emit optimizationRequested(r);
    } catch (const std::exception& e) {
      setMessage(QString::fromUtf8(e.what()));
    }
  });
  connect(design, &QPushButton::clicked, this, [this]() { exportResult(0); });
  connect(csv, &QPushButton::clicked, this, [this]() { exportResult(1); });
  connect(json, &QPushButton::clicked, this, [this]() { exportResult(2); });
  updateButtons();
}

void viewer::RobustOptimizationDialog::chooseConfiguration() {
  const QString path = QFileDialog::getOpenFileName(this, "Choose Halbach optimizer configuration",
                                                    source, "JSON (*.json)");
  if (path.isEmpty()) return;
  try {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) throw std::invalid_argument("Cannot open configuration");
    auto data = nlohmann::json::parse(file.readAll().toStdString());
    const auto& spec
        = data.contains("halbach_optimization") ? data.at("halbach_optimization") : data;
    if (!spec.is_object() || !spec.contains("candidates") || !spec.contains("ring_count")
        || !spec.contains("dsv") || !spec.contains("resolution"))
      throw std::invalid_argument(
          "Choose a Halbach optimizer configuration or an exported optimized design");
    configuration = std::move(data);
    source = path;
    has_report = false;
    report = RobustOptimizationReport();
    candidates->setRowCount(0);
    validation->setRowCount(0);
    summary->setText("Configuration loaded; run nominal search and robust screening.");
    config_label->setText(path + " — geometry and search spacing come from this file; run controls below override population, generations and seed.");
    status->clear();
    tabs->setCurrentIndex(0);
    updateButtons();
  } catch (const std::exception& e) {
    setMessage(QString::fromUtf8(e.what()));
  }
}

greeter::service::RobustOptimizationRequest viewer::RobustOptimizationDialog::request() const {
  if (configuration.is_null())
    throw std::invalid_argument("Choose an optimizer configuration first");
  RobustOptimizationRequest r;
  r.configuration = configuration;
  r.source = source.toStdString();
  auto& s = r.settings;
  s.shortlist = shortlist->value();
  s.population = population->value();
  s.generations = generations->value();
  s.search_seed = readSeed(search_seed);
  s.screening.seed = readSeed(seed);
  s.validation_seed = readSeed(validation_seed);
  s.screening.trials = trials->value();
  s.validation_trials = validation_trials->value();
  s.screening.points_per_axis = density->value();
  s.validation_points_per_axis = validation_density->value();
  s.objective = static_cast<RobustObjective>(objective->currentIndex());
  s.required_acceptance = acceptance->value() / 100;
  s.screening.distribution = distribution->currentIndex() == 0 ? ToleranceDistribution::Uniform
                                                               : ToleranceDistribution::Normal;
  s.screening.strength_percent = strength->value();
  s.screening.position_mm = position->value();
  s.screening.direction_degrees = direction->value();
  s.screening.body_tilt_degrees = tilt->value();
  s.screening.size_percent = size->value();
  s.screening.homogeneity_limit_ppm = ppm_limit->value();
  s.screening.minimum_mean_T = minimum_field->value() / 1000;
  s.validate();
  return r;
}

void viewer::RobustOptimizationDialog::updateButtons() {
  controls->setEnabled(!running);
  run->setEnabled(!running && !configuration.is_null());
  stop->setEnabled(running);
  for (auto* b : {design, csv, json}) b->setEnabled(has_report && !running);
}
void viewer::RobustOptimizationDialog::setRunning(bool value) {
  running = value;
  progress->setVisible(value);
  if (value) {
    progress->setValue(0);
    status->setText(
        "Working: basis 0–20%, search 20–40%, screening 40–85%, validation 85–100%. Stop cancels "
        "between basis chunks, generations and field chunks.");
  } else
    status->clear();
  updateButtons();
}
void viewer::RobustOptimizationDialog::setProgress(int percent) { progress->setValue(percent); }
void viewer::RobustOptimizationDialog::setMessage(const QString& text) { status->setText(text); }

void viewer::RobustOptimizationDialog::setReport(const RobustOptimizationReport& given) {
  if (!given.complete || given.cancelled) return;
  report = given;
  has_report = true;
  const auto& s = report.request.settings;
  const bool screen_ok = robustMeetsRequirements(report.candidates[report.selected].screening, s);
  const bool validated = robustMeetsRequirements(report.validation, s);
  summary->setText(
      QString("Objective: %9. Selected nominal finalist #%1 of %2. Screening requirements: %3. "
              "Fresh validation "
              "requirements: %4. Search stopped: %5. Search seed %6, screening seed %7, validation "
              "seed %8. All results and exports use these saved settings. Limits: homogeneity %10; "
              "minimum mean field %11.")
          .arg(report.selected + 1)
          .arg(report.candidates.size())
          .arg(screen_ok ? "MET" : "NOT MET")
          .arg(validated ? "MET" : "NOT MET")
          .arg(QString::fromStdString(report.search_stop_reason))
          .arg((qulonglong)s.search_seed)
          .arg((qulonglong)s.screening.seed)
          .arg((qulonglong)s.validation_seed)
          .arg(s.objective == RobustObjective::P95Homogeneity ? "Minimize P95 homogeneity"
               : s.objective == RobustObjective::P05MeanField ? "Maximize P5 mean field"
                                                              : "Maximize acceptance rate")
          .arg(s.screening.homogeneity_limit_ppm > 0
                   ? number(s.screening.homogeneity_limit_ppm) + " ppm"
                   : "Disabled")
          .arg(s.screening.minimum_mean_T > 0 ? number(1000 * s.screening.minimum_mean_T) + " mT"
                                              : "Disabled"));
  summary->setStyleSheet(validated ? "" : "color: #c74242;");
  auto order = robustCandidateOrder(report.candidates, s);
  candidates->setRowCount(order.size());
  auto acceptanceText = [](const ToleranceReport& t) {
    return t.settings.hasLimits() ? number(100.0 * t.passed_trials / t.trials.size()) + "%"
                                  : "Disabled";
  };
  for (size_t row = 0; row < order.size(); ++row) {
    auto i = order[row];
    const auto& c = report.candidates[i];
    const auto& t = c.screening;
    cell(candidates, row, 0, QString::number(row + 1));
    cell(candidates, row, 1, QString::number(i + 1));
    cell(candidates, row, 2, i == report.selected ? "Yes" : "");
    cell(candidates, row, 3, QString::fromStdString(nlohmann::json(c.genome).dump()));
    cell(candidates, row, 4, number(1000 * t.nominal.mean_T));
    cell(candidates, row, 5, number(t.nominal.homogeneity_ppm));
    cell(candidates, row, 6, t.valid_trials ? number(t.homogeneity.p95) : "—");
    cell(candidates, row, 7, t.valid_trials ? number(1000 * t.mean_field.p05) : "—");
    cell(candidates, row, 8, acceptanceText(t));
    cell(candidates, row, 9, QString("%1 / %2").arg(t.valid_trials).arg(t.trials.size()));
    cell(candidates, row, 10,
         !robustEligible(t, s.objective) ? "Ineligible"
         : robustMeetsRequirements(t, s) ? "Met"
                                         : "Not met");
  }
  validation->setRowCount(2);
  auto studyRow = [&](int row, const QString& label, const ToleranceReport& t) {
    cell(validation, row, 0, label);
    cell(validation, row, 1, QString::number((qulonglong)t.settings.seed));
    cell(validation, row, 2, QString::number(t.points));
    cell(validation, row, 3, t.valid_trials ? number(t.homogeneity.p95) : "—");
    cell(validation, row, 4, t.valid_trials ? number(1000 * t.mean_field.p05) : "—");
    cell(validation, row, 5, acceptanceText(t));
    cell(validation, row, 6, QString("%1 / %2").arg(t.valid_trials).arg(t.trials.size()));
    cell(validation, row, 7, robustMeetsRequirements(t, s) ? "Yes" : "No");
  };
  studyRow(0, "Selected screening", report.candidates[report.selected].screening);
  studyRow(1, "Fresh validation", report.validation);
  tabs->setCurrentIndex(1);
  updateButtons();
}

void viewer::RobustOptimizationDialog::exportResult(int kind) {
  const QString path = QFileDialog::getSaveFileName(
      this, kind == 0 ? "Export and view selected design" : "Export robust optimization",
      kind == 0   ? "robust-design.json"
      : kind == 1 ? "robust-optimization.csv"
                  : "robust-optimization.json",
      kind == 1 ? "CSV (*.csv)" : "JSON (*.json)");
  if (path.isEmpty()) return;
  try {
    const auto canonical = QFileInfo(path).canonicalFilePath();
    if (!canonical.isEmpty()
        && canonical
               == QFileInfo(QString::fromStdString(report.request.source)).canonicalFilePath())
      throw std::invalid_argument("Choose a destination other than the input configuration");
    if (kind == 0)
      writeRobustDesign(report, path.toStdString());
    else if (kind == 1)
      writeRobustOptimizationCSV(report, path.toStdString());
    else
      writeRobustOptimizationJSON(report, path.toStdString());
    status->setText("Exported to " + path);
    if (kind == 0) emit designExported(path);
  } catch (const std::exception& e) {
    setMessage(QString::fromUtf8(e.what()));
  }
}
void viewer::RobustOptimizationDialog::reject() {
  if (running) emit cancelRequested();
  QDialog::reject();
}

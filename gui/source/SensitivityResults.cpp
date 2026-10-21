#include "SensitivityResults.h"

#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
  using namespace greeter::service;
  QString number(double x) { return QString::number(x, 'g', 7); }
  void cell(QTableWidget* t, int r, int c, const QString& value) {
    t->setItem(r, c, new QTableWidgetItem(value));
  }
  class CurvePlot : public QWidget {
  public:
    std::vector<QPointF> values;
    double step = 1.0;
    QString title;
    explicit CurvePlot(QWidget* parent) : QWidget(parent) {
      setMinimumHeight(160);
      setObjectName("sensitivityPlot");
    }
    void paintEvent(QPaintEvent*) override {
      QPainter p(this);
      p.fillRect(rect(), palette().base());
      p.setRenderHint(QPainter::Antialiasing);
      p.setPen(palette().text().color());
      if (values.empty()) {
        p.drawText(rect(), Qt::AlignCenter, "Select a ranked row to view its response curve");
        return;
      }
      double low = values.front().y(), high = low;
      for (const auto& point : values) {
        low = std::min(low, point.y());
        high = std::max(high, point.y());
      }
      const double margin = high > low ? 0.08 * (high - low) : std::max(1.0, std::abs(low) * 0.05);
      low -= margin;
      high += margin;
      const QRectF box(90, 28, std::max(1, width() - 115), std::max(1, height() - 75));
      p.drawText(QRectF(0, 2, width(), 22), Qt::AlignCenter, title);
      p.drawRect(box);
      p.drawText(QRectF(0, box.top(), 84, 20), Qt::AlignRight, number(high));
      p.drawText(QRectF(0, box.bottom() - 20, 84, 20), Qt::AlignRight, number(low));
      for (int percent : {0, 25, 50, 75, 100})
        p.drawText(QRectF(box.left() + box.width() * percent / 100 - 20, box.bottom() + 2, 40, 20),
                   Qt::AlignCenter, QString::number(percent));
      p.drawText(QRectF(box.left(), height() - 23, box.width(), 22), Qt::AlignCenter,
                 "Fraction of configured tolerance [%]");
      QPointF previous;
      double previous_scale = 0.0;
      bool first = true;
      for (const auto& value : values) {
        const QPointF at(box.left() + value.x() * box.width(),
                         box.bottom() - (value.y() - low) / (high - low) * box.height());
        p.setPen(QPen(QColor(48, 139, 185), 2));
        // Missing (invalid) study points leave a gap rather than a false zero.
        if (!first && value.x() - previous_scale <= step + 1e-9) p.drawLine(previous, at);
        p.setBrush(QColor(48, 139, 185));
        p.drawEllipse(at, 3.5, 3.5);
        previous = at;
        previous_scale = value.x();
        first = false;
      }
    }
  };
}  // namespace

viewer::SensitivityResults::SensitivityResults(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  summary = new QLabel(this);
  summary->setObjectName("sensitivitySummary");
  summary->setWordWrap(true);
  layout->addWidget(summary);
  metric = new QComboBox(this);
  metric->setObjectName("sensitivityRankMetric");
  metric->addItems({"Rank by RMS homogeneity change [ppm]", "Rank by RMS mean-field shift [ppm]"});
  layout->addWidget(metric);
  rankings = new QTableWidget(0, 9, this);
  rankings->setObjectName("sensitivityRankings");
  rankings->setHorizontalHeaderLabels({"Kind", "Rank", "Error source / group", "Magnets",
                                       "Homogeneity RMS Δ [ppm]", "Field shift RMS [ppm]",
                                       "P95 homogeneity [ppm]", "Valid trials", "Invalid trials"});
  rankings->setSelectionBehavior(QAbstractItemView::SelectRows);
  rankings->setSelectionMode(QAbstractItemView::SingleSelection);
  rankings->setEditTriggers(QAbstractItemView::NoEditTriggers);
  rankings->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  layout->addWidget(rankings, 2);
  quantity = new QComboBox(this);
  quantity->setObjectName("sensitivityCurveMetric");
  quantity->addItems({"RMS homogeneity change [ppm]", "RMS mean-field shift [ppm]",
                      "Mean homogeneity [ppm]", "Mean |B| [mT]"});
  layout->addWidget(quantity);
  plot = new CurvePlot(this);
  layout->addWidget(plot, 2);
  points = new QTableWidget(0, 7, this);
  points->setObjectName("sensitivityCurvePoints");
  points->setHorizontalHeaderLabels({"Tolerance [%]", "Mean |B| [mT]", "Mean homogeneity [ppm]",
                                     "Homogeneity RMS Δ [ppm]", "Field shift RMS [ppm]",
                                     "Valid / total", "Acceptance"});
  points->setEditTriggers(QAbstractItemView::NoEditTriggers);
  points->setMinimumHeight(125);
  points->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  layout->addWidget(points, 2);
  auto* note = new QLabel(
      "Ranks compare rows within each kind at full tolerance. Scores are RMS changes from nominal, "
      "including bias; they are not additive contributions or interaction effects. Invalid trials "
      "are excluded from scores and count as acceptance failures. Fewer than two valid trials: "
      "unranked.",
      this);
  note->setWordWrap(true);
  layout->addWidget(note);
  connect(metric, &QComboBox::currentIndexChanged, this, [this]() { refreshRanks(); });
  connect(quantity, &QComboBox::currentIndexChanged, this, [this]() { refreshCurve(); });
  connect(rankings, &QTableWidget::itemSelectionChanged, this, [this]() { refreshCurve(); });
  clear();
}

void viewer::SensitivityResults::clear() {
  report = SensitivityReport();
  rankings->setRowCount(0);
  points->setRowCount(0);
  static_cast<CurvePlot*>(plot)->values.clear();
  plot->update();
  summary->setText("Run a sensitivity study to rank error sources and inspect response curves.");
}

void viewer::SensitivityResults::setReport(const SensitivityReport& given) {
  report = given;
  summary->setText(
      QString("%1 cases · %2 trials per point · %3 tolerance levels · seed %4 · %5 magnets · %6 "
              "field points. Select a row for its curve. Full-tolerance settings: strength %7%, "
              "position %8 mm/axis, direction %9°/axis, tilt %10°/axis, size %11%.")
          .arg(report.cases.size())
          .arg(report.settings.tolerances.trials)
          .arg(report.settings.sweep_points)
          .arg((qulonglong)report.settings.tolerances.seed)
          .arg(report.magnets)
          .arg(report.points)
          .arg(report.settings.tolerances.strength_percent)
          .arg(report.settings.tolerances.position_mm)
          .arg(report.settings.tolerances.direction_degrees)
          .arg(report.settings.tolerances.body_tilt_degrees)
          .arg(report.settings.tolerances.size_percent));
  refreshRanks();
}

void viewer::SensitivityResults::refreshRanks() {
  const auto ranks
      = sensitivityRanks(report, metric->currentIndex() == 0 ? SensitivityMetric::Homogeneity
                                                             : SensitivityMetric::FieldShift);
  std::vector<size_t> order(report.cases.size());
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    const auto& x = report.cases[a];
    const auto& y = report.cases[b];
    if (x.kind != y.kind) return x.kind == "source" || (y.kind != "source" && x.kind < y.kind);
    return (ranks[a] ? ranks[a] : SIZE_MAX) < (ranks[b] ? ranks[b] : SIZE_MAX);
  });
  rankings->blockSignals(true);
  rankings->setRowCount((int)order.size());
  for (size_t row = 0; row < order.size(); ++row) {
    const auto i = order[row];
    const auto& c = report.cases[i];
    const auto& p = c.curve.back();
    cell(rankings, row, 0, QString::fromStdString(c.kind));
    rankings->item(row, 0)->setData(Qt::UserRole, (qulonglong)i);
    cell(rankings, row, 1, ranks[i] ? QString::number(ranks[i]) : "Unranked");
    cell(rankings, row, 2, QString::fromStdString(c.label));
    rankings->item(row, 2)->setToolTip(
        "Magnet IDs: " + QString::fromStdString(nlohmann::json(c.magnet_ids).dump()));
    cell(rankings, row, 3, QString::number(c.magnet_indices.size()));
    cell(rankings, row, 4, p.study.valid_trials ? number(p.homogeneity_rms_ppm) : "—");
    cell(rankings, row, 5, p.study.valid_trials ? number(p.field_shift_rms_ppm) : "—");
    cell(rankings, row, 6, p.study.valid_trials ? number(p.study.homogeneity.p95) : "—");
    cell(rankings, row, 7, QString::number(p.study.valid_trials));
    cell(rankings, row, 8, QString::number(p.study.trials.size() - p.study.valid_trials));
  }
  rankings->blockSignals(false);
  if (!order.empty()) rankings->selectRow(0);
  refreshCurve();
}

void viewer::SensitivityResults::refreshCurve() {
  auto* chart = static_cast<CurvePlot*>(plot);
  chart->values.clear();
  points->setRowCount(0);
  const int row = rankings->currentRow();
  if (row < 0 || !rankings->item(row, 0)) {
    plot->update();
    return;
  }
  const auto index = rankings->item(row, 0)->data(Qt::UserRole).toULongLong();
  if (index >= report.cases.size()) return;
  const auto& c = report.cases[index];
  chart->step = 1.0 / (report.settings.sweep_points - 1);
  chart->title = QString::fromStdString(c.label) + " — " + quantity->currentText();
  const double nominal_value = quantity->currentIndex() == 2   ? report.nominal.homogeneity_ppm
                               : quantity->currentIndex() == 3 ? 1000 * report.nominal.mean_T
                                                               : 0.0;
  chart->values.push_back(QPointF(0, nominal_value));
  points->setRowCount((int)c.curve.size() + 1);
  cell(points, 0, 0, "0 (nominal)");
  cell(points, 0, 1, number(1000 * report.nominal.mean_T));
  cell(points, 0, 2, number(report.nominal.homogeneity_ppm));
  cell(points, 0, 3, "0");
  cell(points, 0, 4, "0");
  cell(points, 0, 5, "—");
  cell(points, 0, 6, "—");
  for (size_t i = 0; i < c.curve.size(); ++i) {
    const auto& p = c.curve[i];
    cell(points, i + 1, 0, number(100 * p.scale));
    cell(points, i + 1, 1, p.study.valid_trials ? number(1000 * p.study.mean_field.mean) : "—");
    cell(points, i + 1, 2, p.study.valid_trials ? number(p.study.homogeneity.mean) : "—");
    cell(points, i + 1, 3, p.study.valid_trials ? number(p.homogeneity_rms_ppm) : "—");
    cell(points, i + 1, 4, p.study.valid_trials ? number(p.field_shift_rms_ppm) : "—");
    cell(points, i + 1, 5, QString("%1 / %2").arg(p.study.valid_trials).arg(p.study.trials.size()));
    cell(points, i + 1, 6,
         p.study.settings.hasLimits()
             ? number(100.0 * p.study.passed_trials / p.study.trials.size()) + "%"
             : "Disabled");
    if (p.study.valid_trials) {
      const double values[] = {p.homogeneity_rms_ppm, p.field_shift_rms_ppm,
                               p.study.homogeneity.mean, 1000 * p.study.mean_field.mean};
      chart->values.push_back(QPointF(p.scale, values[quantity->currentIndex()]));
    }
  }
  plot->update();
}

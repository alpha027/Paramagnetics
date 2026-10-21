#ifndef VIEWER_SENSITIVITY_RESULTS_H
#define VIEWER_SENSITIVITY_RESULTS_H

#include <greeter/service/SensitivityAnalysis.h>

#include <QWidget>

class QComboBox;
class QLabel;
class QTableWidget;

namespace viewer {
  class SensitivityResults : public QWidget {
  public:
    explicit SensitivityResults(QWidget* parent = nullptr);
    void clear();
    void setReport(const greeter::service::SensitivityReport& report);

  private:
    void refreshRanks();
    void refreshCurve();
    greeter::service::SensitivityReport report;
    QLabel* summary;
    QComboBox* metric;
    QComboBox* quantity;
    QTableWidget* rankings;
    QTableWidget* points;
    QWidget* plot;
  };
}  // namespace viewer
#endif

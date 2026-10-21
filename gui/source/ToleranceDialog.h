#ifndef VIEWER_TOLERANCE_DIALOG_H
#define VIEWER_TOLERANCE_DIALOG_H

#include <greeter/service/SimulationService.h>

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;

namespace viewer {

  class ToleranceDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ToleranceDialog(QWidget* parent = nullptr);
    greeter::service::ToleranceSettings settings() const;
    void setDomain(bool available, const greeter::service::FieldRequest& request);
    void setRunning(bool running);
    void setProgress(int percent);
    void setReport(const greeter::service::ToleranceReport& report);

  signals:
    void analysisRequested(const greeter::service::ToleranceSettings& settings);
    void cancelRequested();

  protected:
    void reject() override;

  private:
    void exportReport(bool json);
    void updateButtons();
    bool available = false;
    bool running = false;
    bool has_report = false;
    greeter::service::ToleranceReport report;
    QTabWidget* tabs;
    QWidget* controls;
    QSpinBox* trials;
    QSpinBox* density;
    QLineEdit* seed;
    QComboBox* distribution;
    QComboBox* region;
    QDoubleSpinBox* strength;
    QDoubleSpinBox* position;
    QDoubleSpinBox* direction;
    QDoubleSpinBox* tilt;
    QDoubleSpinBox* size;
    QDoubleSpinBox* ppm_limit;
    QDoubleSpinBox* minimum_field;
    QLabel* domain;
    QLabel* summary;
    QLabel* status;
    QProgressBar* progress;
    QPushButton* run;
    QPushButton* stop;
    QPushButton* csv;
    QPushButton* json;
    QTableWidget* statistics;
    QTableWidget* trial_table;
    QWidget* histogram;
  };

}  // namespace viewer
#endif

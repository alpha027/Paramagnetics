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

  class SensitivityResults;

  class ToleranceDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ToleranceDialog(QWidget* parent = nullptr, bool sensitivity_mode = false);
    greeter::service::ToleranceSettings settings() const;
    greeter::service::SensitivitySettings sensitivitySettings() const;
    void setScene(const greeter::view::SceneSnapshot& scene);
    void setDomain(bool available, const greeter::service::FieldRequest& request);
    void setRunning(bool running);
    void setProgress(int percent);
    void setReport(const greeter::service::ToleranceReport& report);
    void setSensitivityReport(const greeter::service::SensitivityReport& report);

  signals:
    void analysisRequested(const greeter::service::ToleranceSettings& settings);
    void sensitivityRequested(const greeter::service::SensitivitySettings& settings);
    void cancelRequested();

  protected:
    void reject() override;

  private:
    void exportReport(bool json);
    void updateButtons();
    void updateWorkEstimate();
    bool sensitivity_mode = false;
    size_t magnet_count = 0;
    size_t group_count = 0;
    bool available = false;
    bool running = false;
    bool has_report = false;
    greeter::service::ToleranceReport report;
    greeter::service::SensitivityReport sensitivity_report;
    SensitivityResults* sensitivity_results = nullptr;
    QComboBox* grouping = nullptr;
    QSpinBox* sweep_points = nullptr;
    QLabel* work_estimate = nullptr;
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

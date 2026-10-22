#ifndef VIEWER_ROBUST_OPTIMIZATION_DIALOG_H
#define VIEWER_ROBUST_OPTIMIZATION_DIALOG_H

#include <greeter/service/RobustOptimization.h>

#include <QDialog>

class QLabel;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTabWidget;

namespace viewer {
  class RobustOptimizationDialog : public QDialog {
    Q_OBJECT
  public:
    explicit RobustOptimizationDialog(QWidget* parent = nullptr);
    greeter::service::RobustOptimizationRequest request() const;
    void setRunning(bool running);
    void setProgress(int percent);
    void setMessage(const QString& text);
    void setReport(const greeter::service::RobustOptimizationReport& report);
  signals:
    void optimizationRequested(const greeter::service::RobustOptimizationRequest& request);
    void cancelRequested();
    void designExported(const QString& path);

  protected:
    void reject() override;

  private:
    void chooseConfiguration();
    void exportResult(int kind);
    void updateButtons();
    bool running = false;
    bool has_report = false;
    nlohmann::json configuration;
    QString source;
    greeter::service::RobustOptimizationReport report;
    QWidget* controls;
    QTabWidget* tabs;
    QLabel* config_label;
    QLabel* summary;
    QLabel* status;
    QComboBox* objective;
    QComboBox* distribution;
    QSpinBox* shortlist;
    QSpinBox* population;
    QSpinBox* generations;
    QSpinBox* trials;
    QSpinBox* validation_trials;
    QSpinBox* density;
    QSpinBox* validation_density;
    QLineEdit* search_seed;
    QLineEdit* seed;
    QLineEdit* validation_seed;
    QDoubleSpinBox* strength;
    QDoubleSpinBox* position;
    QDoubleSpinBox* direction;
    QDoubleSpinBox* tilt;
    QDoubleSpinBox* size;
    QDoubleSpinBox* ppm_limit;
    QDoubleSpinBox* minimum_field;
    QDoubleSpinBox* acceptance;
    QTableWidget* candidates;
    QTableWidget* validation;
    QProgressBar* progress;
    QPushButton* run;
    QPushButton* stop;
    QPushButton* design;
    QPushButton* csv;
    QPushButton* json;
  };
}  // namespace viewer
#endif

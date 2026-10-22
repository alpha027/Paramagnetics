#include <greeter/KokkosDefines.h>
#include <greeter/view/SnapshotIO.h>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include "MainWindow.h"
#include "RobustOptimizationDialog.h"
#include "ToleranceDialog.h"

namespace {
  QString fixture(const QString& directory) {
    const QString path = directory + "/scene.json";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return {};
    file.write(R"({"magnets":[{"id":7,"type":"sphere","parameters":{
    "dimensions":0.01,"magnetization":1,"position":[0,0,0],"orientation":[1,0,0,0]}}],
    "field_of_view":{"x":{"min":0,"max":0,"n":1},"y":{"min":0,"max":0,"n":1},
    "z":{"min":0.1,"max":0.12,"n":3}}})");
    return path;
  }
  void chooseFile(QWidget* parent, const QString& path) {
    QTimer::singleShot(100, parent, [parent, path]() {
      if (auto* dialog = parent->findChild<QFileDialog*>()) {
        dialog->setDirectory(QFileInfo(path).absolutePath());
        dialog->selectFile(QFileInfo(path).fileName());
        if (auto* edit = dialog->findChild<QLineEdit*>("fileNameEdit")) edit->setText(path);
        QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
      }
    });
    QTimer::singleShot(3000, parent, [parent]() {
      if (auto* dialog = parent->findChild<QFileDialog*>()) dialog->reject();
    });
  }
  void exactControls(viewer::ToleranceDialog* dialog, int trials) {
    dialog->findChild<QSpinBox*>("tolTrials")->setValue(trials);
    for (const char* name : {"tolStrength", "tolPosition", "tolDirection", "tolTilt", "tolSize"})
      dialog->findChild<QDoubleSpinBox*>(name)->setValue(0);
  }
  QString robustFixture(const QString& directory) {
    const QString path = directory + "/optimizer.json";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return {};
    file.write(R"({"ring_count":1,"ring_separation":0.03,
      "candidates":[{"radius":0.10,"count":8},{"radius":0.12,"count":10}],
      "outer_radius_offset":0.03,"outer_count_offset":2,
      "element":{"size":0.01,"remanence":1.3},"dsv":0.02,"resolution":0.01,
      "symmetry":"full","objective":"ppm_bmag","field_model":"cuboid",
      "genetic":{"population":32,"generations":2,"seed":7}})");
    return path;
  }
  void robustControls(viewer::RobustOptimizationDialog* dialog) {
    dialog->findChild<QSpinBox*>("robustPopulation")->setValue(32);
    dialog->findChild<QSpinBox*>("robustGenerations")->setValue(2);
    dialog->findChild<QSpinBox*>("robustShortlist")->setValue(4);
    dialog->findChild<QSpinBox*>("robustTrials")->setValue(8);
    dialog->findChild<QSpinBox*>("robustValidationTrials")->setValue(12);
    dialog->findChild<QSpinBox*>("robustDensity")->setValue(3);
    dialog->findChild<QSpinBox*>("robustValidationDensity")->setValue(5);
  }
}  // namespace

class ToleranceGuiTest : public QObject {
  Q_OBJECT
private slots:
  void controlsConvertUnitsAndValidateSeed() {
    viewer::ToleranceDialog dialog;
    greeter::service::FieldRequest request;
    dialog.setDomain(true, request);
    dialog.show();
    dialog.findChild<QLineEdit*>("tolSeed")->setText("18446744073709551615");
    dialog.findChild<QDoubleSpinBox*>("tolMinimumField")->setValue(150);
    dialog.findChild<QComboBox*>("tolDistribution")->setCurrentIndex(1);
    QCOMPARE(dialog.settings().seed, UINT64_MAX);
    QCOMPARE(dialog.settings().minimum_mean_T, 0.15);
    QVERIFY(dialog.settings().distribution == greeter::service::ToleranceDistribution::Normal);
    QSignalSpy run(&dialog, &viewer::ToleranceDialog::analysisRequested);
    dialog.findChild<QLineEdit*>("tolSeed")->setText("-1");
    QTest::mouseClick(dialog.findChild<QPushButton*>("tolRun"), Qt::LeftButton);
    QCOMPARE(run.count(), 0);
    QVERIFY(!dialog.findChild<QLabel*>("tolStatus")->text().isEmpty());
  }

  void menuRunsAnalysisAndExportsBothFormats() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    viewer::MainWindow window;
    const QString source = fixture(directory.path());
    QVERIFY(!source.isEmpty());
    window.openAtStart(source);
    auto* action = window.findChild<QAction*>("toleranceAction");
    QVERIFY(action);
    QTRY_VERIFY(action->isEnabled());
    action->trigger();
    auto* dialog = window.findChild<viewer::ToleranceDialog*>();
    QVERIFY(dialog);
    QVERIFY(dialog->isVisible());
    exactControls(dialog, 7);
    QTest::mouseClick(dialog->findChild<QPushButton*>("tolRun"), Qt::LeftButton);
    auto* csv = dialog->findChild<QPushButton*>("tolExportCsv");
    QTRY_VERIFY_WITH_TIMEOUT(csv->isEnabled(), 15000);
    QCOMPARE(dialog->findChild<QTableWidget*>("tolTrialsTable")->rowCount(), 8);
    auto* stats = dialog->findChild<QTableWidget*>("tolStatistics");
    QCOMPARE(stats->rowCount(), 3);
    QCOMPARE(stats->item(0, 3)->text(), QString("0"));
    QVERIFY(dialog->findChild<QLabel*>("tolSummary")->text().contains("7 valid"));

    const QString json_path = directory.path() + "/report.json";
    chooseFile(&window, json_path);
    QTest::mouseClick(dialog->findChild<QPushButton*>("tolExportJson"), Qt::LeftButton);
    QFile json_file(json_path);
    QVERIFY2(json_file.open(QIODevice::ReadOnly),
             qPrintable(dialog->findChild<QLabel*>("tolStatus")->text()));
    const auto report = nlohmann::json::parse(json_file.readAll().toStdString());
    QCOMPARE(report["format"].get<std::string>(), std::string("paramagnetics-tolerance"));
    QCOMPARE(report["trials"].size(), size_t(7));
    QCOMPARE(report["summary"]["valid_trials"].get<int>(), 7);

    const QString csv_path = directory.path() + "/report.csv";
    chooseFile(&window, csv_path);
    QTest::mouseClick(csv, Qt::LeftButton);
    QFile csv_file(csv_path);
    QVERIFY(csv_file.open(QIODevice::ReadOnly));
    QCOMPARE(csv_file.readAll().trimmed().split('\n').size(), 9);
    if (!qEnvironmentVariableIsEmpty("PARAMAGNETICS_GUI_SCREENSHOT")) {
      const QString prefix = qEnvironmentVariable("PARAMAGNETICS_GUI_SCREENSHOT");
      auto* tabs = dialog->findChild<QTabWidget*>();
      dialog->findChild<QSpinBox*>("tolTrials")->setValue(50);
      dialog->findChild<QDoubleSpinBox*>("tolStrength")->setValue(1);
      dialog->findChild<QDoubleSpinBox*>("tolPosition")->setValue(0.1);
      dialog->findChild<QDoubleSpinBox*>("tolDirection")->setValue(1);
      dialog->findChild<QDoubleSpinBox*>("tolSize")->setValue(0.25);
      dialog->findChild<QDoubleSpinBox*>("tolPpmLimit")->setValue(550000);
      QTest::mouseClick(dialog->findChild<QPushButton*>("tolRun"), Qt::LeftButton);
      QTRY_VERIFY_WITH_TIMEOUT(csv->isEnabled(), 15000);
      QVERIFY(stats->item(0, 3)->text().toDouble() > 0);
      QVERIFY(dialog->grab().save(prefix + ".results.png"));
      tabs->setCurrentIndex(0);
      QTest::qWait(20);
      QVERIFY(dialog->grab().save(prefix + ".setup.png"));
    }
  }

  void immediateStopDoesNotPublishPartialResultsAndRunCanRestart() {
    QTemporaryDir directory;
    viewer::MainWindow window;
    window.openAtStart(fixture(directory.path()));
    auto* action = window.findChild<QAction*>("toleranceAction");
    QTRY_VERIFY(action->isEnabled());
    action->trigger();
    auto* dialog = window.findChild<viewer::ToleranceDialog*>();
    exactControls(dialog, 10000);
    auto* run = dialog->findChild<QPushButton*>("tolRun");
    auto* stop = dialog->findChild<QPushButton*>("tolStop");
    QTest::mouseClick(run, Qt::LeftButton);
    QVERIFY(stop->isEnabled());
    QTest::mouseClick(stop, Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(run->isEnabled(), 15000);
    QCOMPARE(dialog->findChild<QTableWidget*>("tolTrialsTable")->rowCount(), 0);
    QVERIFY(!dialog->findChild<QPushButton*>("tolExportCsv")->isEnabled());
    exactControls(dialog, 2);
    QTest::mouseClick(run, Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(dialog->findChild<QPushButton*>("tolExportCsv")->isEnabled(), 15000);
    QCOMPARE(dialog->findChild<QTableWidget*>("tolTrialsTable")->rowCount(), 3);
  }

  void sensitivityMenuRunsRankingsCurvesAndExports() {
    QTemporaryDir directory;
    viewer::MainWindow window;
    window.openAtStart(fixture(directory.path()));
    auto* action = window.findChild<QAction*>("sensitivityAction");
    QVERIFY(action);
    QTRY_VERIFY(action->isEnabled());
    action->trigger();
    auto* dialog = window.findChild<viewer::ToleranceDialog*>("sensitivityDialog");
    QVERIFY(dialog);
    QVERIFY(dialog->isVisible());
    exactControls(dialog, 30);
    dialog->findChild<QDoubleSpinBox*>("tolStrength")->setValue(1);
    dialog->findChild<QDoubleSpinBox*>("tolPosition")->setValue(0.1);
    dialog->findChild<QDoubleSpinBox*>("tolSize")->setValue(0.25);
    dialog->findChild<QComboBox*>("sensitivityGrouping")->setCurrentIndex(1);
    dialog->findChild<QSpinBox*>("sensitivitySweepPoints")->setValue(3);
    QCOMPARE(dialog->sensitivitySettings().grouping,
             greeter::service::SensitivityGrouping::Magnets);
    QVERIFY(dialog->findChild<QLabel*>("sensitivityWorkEstimate")->text().contains("240 random"));
    QTest::mouseClick(dialog->findChild<QPushButton*>("tolRun"), Qt::LeftButton);
    auto* csv = dialog->findChild<QPushButton*>("tolExportCsv");
    QTRY_VERIFY_WITH_TIMEOUT(csv->isEnabled(), 15000);
    auto* ranks = dialog->findChild<QTableWidget*>("sensitivityRankings");
    auto* points = dialog->findChild<QTableWidget*>("sensitivityCurvePoints");
    QCOMPARE(ranks->rowCount(), 4);
    QCOMPARE(points->rowCount(), 3);
    QCOMPARE(points->item(0, 0)->text(), QString("0 (nominal)"));
    dialog->findChild<QComboBox*>("sensitivityRankMetric")->setCurrentIndex(1);
    QCOMPARE(ranks->item(0, 1)->text(), QString("1"));
    QVERIFY(ranks->item(0, 5)->text().toDouble() > 0);
    ranks->selectRow(3);  // individual-magnet curve
    dialog->findChild<QComboBox*>("sensitivityCurveMetric")->setCurrentIndex(3);
    QVERIFY(points->item(2, 1)->text().toDouble() > 0);
    QVERIFY(dialog->findChild<QLabel*>("sensitivitySummary")->text().contains("4 cases"));

    const QString json_path = directory.path() + "/sensitivity.json";
    chooseFile(&window, json_path);
    QTest::mouseClick(dialog->findChild<QPushButton*>("tolExportJson"), Qt::LeftButton);
    QFile json_file(json_path);
    QVERIFY(json_file.open(QIODevice::ReadOnly));
    const auto report = nlohmann::json::parse(json_file.readAll().toStdString());
    QCOMPARE(report["format"].get<std::string>(), std::string("paramagnetics-sensitivity"));
    QCOMPARE(report["cases"].size(), size_t(4));
    QCOMPARE(report["cases"][3]["magnet_ids"][0].get<int>(), 7);
    QCOMPARE(report["cases"][0]["curve"][1]["study"]["trials"].size(), size_t(30));
    const QString csv_path = directory.path() + "/sensitivity.csv";
    chooseFile(&window, csv_path);
    QTest::mouseClick(csv, Qt::LeftButton);
    QFile csv_file(csv_path);
    QVERIFY(csv_file.open(QIODevice::ReadOnly));
    QCOMPARE(csv_file.readAll().trimmed().split('\n').size(), 9);
    if (!qEnvironmentVariableIsEmpty("PARAMAGNETICS_GUI_SCREENSHOT")) {
      const QString prefix = qEnvironmentVariable("PARAMAGNETICS_GUI_SCREENSHOT");
      QVERIFY(dialog->grab().save(prefix + ".sensitivity-results.png"));
      dialog->findChild<QTabWidget*>()->setCurrentIndex(0);
      QTest::qWait(20);
      QVERIFY(dialog->grab().save(prefix + ".sensitivity-setup.png"));
    }
  }

  void sensitivityImmediateStopCanRestartAndInvalidSetupDoesNotRun() {
    QTemporaryDir directory;
    viewer::MainWindow window;
    window.openAtStart(fixture(directory.path()));
    auto* action = window.findChild<QAction*>("sensitivityAction");
    QTRY_VERIFY(action->isEnabled());
    action->trigger();
    auto* dialog = window.findChild<viewer::ToleranceDialog*>("sensitivityDialog");
    exactControls(dialog, 10000);
    QSignalSpy requested(dialog, &viewer::ToleranceDialog::sensitivityRequested);
    auto* run = dialog->findChild<QPushButton*>("tolRun");
    QTest::mouseClick(run, Qt::LeftButton);
    QCOMPARE(requested.count(), 0);
    QVERIFY(dialog->findChild<QLabel*>("tolStatus")->text().contains("nonzero"));
    dialog->findChild<QDoubleSpinBox*>("tolStrength")->setValue(1);
    QTest::mouseClick(run, Qt::LeftButton);
    QTest::mouseClick(dialog->findChild<QPushButton*>("tolStop"), Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(run->isEnabled(), 15000);
    QCOMPARE(dialog->findChild<QTableWidget*>("sensitivityRankings")->rowCount(), 0);
    QVERIFY(!dialog->findChild<QPushButton*>("tolExportCsv")->isEnabled());
    dialog->findChild<QSpinBox*>("tolTrials")->setValue(4);
    QTest::mouseClick(run, Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(dialog->findChild<QPushButton*>("tolExportCsv")->isEnabled(), 15000);
    QCOMPARE(dialog->findChild<QTableWidget*>("sensitivityRankings")->rowCount(), 1);
  }

  void robustOptimizationRunsFromMenuExportsAndOpensTheTestedDesign() {
    QTemporaryDir directory;
    viewer::MainWindow window;
    auto* action = window.findChild<QAction*>("robustOptimizationAction");
    QVERIFY(action);
    QVERIFY(action->isEnabled());  // optimizer configuration is independent of the displayed scene
    action->trigger();
    auto* dialog = window.findChild<viewer::RobustOptimizationDialog*>();
    QVERIFY(dialog && dialog->isVisible());
    auto* run = dialog->findChild<QPushButton*>("robustRun");
    QVERIFY(!run->isEnabled());
    const QString source = robustFixture(directory.path());
    chooseFile(dialog, source);
    QTest::mouseClick(dialog->findChild<QPushButton*>("robustBrowse"), Qt::LeftButton);
    QVERIFY(run->isEnabled());
    robustControls(dialog);
    dialog->findChild<QDoubleSpinBox*>("robustMinimumField")
        ->setValue(100000);  // explicit unmet requirement
    dialog->findChild<QComboBox*>("robustObjective")->setCurrentIndex(1);
    QCOMPARE(dialog->request().settings.screening.minimum_mean_T, 100.0);
    QCOMPARE(dialog->request().settings.objective, greeter::service::RobustObjective::P05MeanField);
    QTest::mouseClick(run, Qt::LeftButton);
    auto* csv = dialog->findChild<QPushButton*>("robustExportCsv");
    QTRY_VERIFY_WITH_TIMEOUT(csv->isEnabled(), 15000);
    QCOMPARE(dialog->findChild<QTableWidget*>("robustCandidates")->rowCount(), 2);
    auto* validation = dialog->findChild<QTableWidget*>("robustValidation");
    QCOMPARE(validation->rowCount(), 2);
    QCOMPARE(validation->item(1, 1)->text(), QString("2026"));
    QCOMPARE(validation->item(1, 7)->text(), QString("No"));
    QVERIFY(dialog->findChild<QLabel*>("robustSummary")->text().contains("NOT MET"));
    const QString report_path = directory.path() + "/robust-report.json";
    chooseFile(dialog, report_path);
    QTest::mouseClick(dialog->findChild<QPushButton*>("robustExportJson"), Qt::LeftButton);
    QFile json_file(report_path);
    QVERIFY(json_file.open(QIODevice::ReadOnly));
    auto report = nlohmann::json::parse(json_file.readAll().toStdString());
    QCOMPARE(report["format"].get<std::string>(), std::string("paramagnetics-robust-optimization"));
    QCOMPARE(report["candidates"].size(), size_t(2));
    QVERIFY(!report["validation_requirements_met"].get<bool>());
    const QString csv_path = directory.path() + "/robust.csv";
    chooseFile(dialog, csv_path);
    QTest::mouseClick(csv, Qt::LeftButton);
    QFile csv_file(csv_path);
    QVERIFY(csv_file.open(QIODevice::ReadOnly));
    QCOMPARE(csv_file.readAll().trimmed().split('\n').size(), 4);
    if (!qEnvironmentVariableIsEmpty("PARAMAGNETICS_GUI_SCREENSHOT")) {
      const QString prefix = qEnvironmentVariable("PARAMAGNETICS_GUI_SCREENSHOT");
      QVERIFY(dialog->grab().save(prefix + ".robust-results.png"));
      dialog->findChild<QTabWidget*>()->setCurrentIndex(0);
      QTest::qWait(20);
      QVERIFY(dialog->grab().save(prefix + ".robust-setup.png"));
    }
    const QString design_path = directory.path() + "/robust-design.json";
    chooseFile(dialog, design_path);
    QTest::mouseClick(dialog->findChild<QPushButton*>("robustExportDesign"), Qt::LeftButton);
    QTRY_VERIFY(window.windowTitle().contains("robust-design.json"));
    QVERIFY(window.findChild<QAction*>("toleranceAction")->isEnabled());
    greeter::service::SimulationService replay;
    replay.loadFile(design_path.toStdString());
    QVERIFY(replay.isLoaded());
    QVERIFY(replay.getInput() == report["selected_design"]);
  }

  void robustOptimizationValidatesSeedsAndCancelsBeforeBasisWork() {
    QTemporaryDir directory;
    viewer::MainWindow window;
    window.findChild<QAction*>("robustOptimizationAction")->trigger();
    auto* dialog = window.findChild<viewer::RobustOptimizationDialog*>();
    chooseFile(dialog, robustFixture(directory.path()));
    QTest::mouseClick(dialog->findChild<QPushButton*>("robustBrowse"), Qt::LeftButton);
    robustControls(dialog);
    auto* run = dialog->findChild<QPushButton*>("robustRun");
    QSignalSpy requested(dialog, &viewer::RobustOptimizationDialog::optimizationRequested);
    dialog->findChild<QLineEdit*>("robustValidationSeed")->setText("42");
    QTest::mouseClick(run, Qt::LeftButton);
    QCOMPARE(requested.count(), 0);
    QVERIFY(dialog->findChild<QLabel*>("robustStatus")->text().contains("fresh"));
    dialog->findChild<QLineEdit*>("robustValidationSeed")->setText("2026");
    QTest::mouseClick(run, Qt::LeftButton);
    QTest::mouseClick(dialog->findChild<QPushButton*>("robustStop"), Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(run->isEnabled(), 15000);
    QCOMPARE(dialog->findChild<QTableWidget*>("robustCandidates")->rowCount(), 0);
    QVERIFY(!dialog->findChild<QPushButton*>("robustExportDesign")->isEnabled());
    QTest::mouseClick(run, Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(dialog->findChild<QPushButton*>("robustExportDesign")->isEnabled(),
                             15000);
  }

  void jsonSnapshotCannotAnalyzeThePreviouslyLoadedScene() {
    QTemporaryDir directory;
    const QString source = fixture(directory.path());
    viewer::MainWindow window;
    window.openAtStart(source);
    auto* action = window.findChild<QAction*>("toleranceAction");
    QTRY_VERIFY(action->isEnabled());
    greeter::service::SimulationService service;
    service.loadFile(source.toStdString());
    greeter::view::Snapshot snapshot;
    snapshot.scene = service.getScene();
    const QString path = directory.path() + "/snapshot.json";
    greeter::view::SnapshotIO::write(snapshot, path.toStdString());
    chooseFile(&window, path);
    QVERIFY(QMetaObject::invokeMethod(&window, "onOpenSnapshot", Qt::DirectConnection));
    QTRY_VERIFY(window.windowTitle().contains("snapshot.json"));
    QVERIFY(!action->isEnabled());
    QVERIFY(!window.findChild<QAction*>("sensitivityAction")->isEnabled());
    QVERIFY(!window.findChild<viewer::ToleranceDialog*>("sensitivityDialog")
                 ->findChild<QPushButton*>("tolRun")
                 ->isEnabled());
    QVERIFY(!window.findChild<viewer::ToleranceDialog*>()
                 ->findChild<QPushButton*>("tolRun")
                 ->isEnabled());
  }
};

int main(int argc, char** argv) {
  Kokkos::initialize(argc, argv);
  int result;
  {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    qRegisterMetaType<greeter::view::Snapshot>("greeter::view::Snapshot");
    qRegisterMetaType<greeter::service::FieldRequest>("greeter::service::FieldRequest");
    qRegisterMetaType<greeter::service::ToleranceSettings>("greeter::service::ToleranceSettings");
    qRegisterMetaType<greeter::service::ToleranceReport>("greeter::service::ToleranceReport");
    qRegisterMetaType<greeter::service::SensitivitySettings>(
        "greeter::service::SensitivitySettings");
    qRegisterMetaType<greeter::service::SensitivityReport>("greeter::service::SensitivityReport");
    qRegisterMetaType<greeter::service::RobustOptimizationRequest>(
        "greeter::service::RobustOptimizationRequest");
    qRegisterMetaType<greeter::service::RobustOptimizationReport>(
        "greeter::service::RobustOptimizationReport");
    ToleranceGuiTest test;
    result = QTest::qExec(&test, argc, argv);
  }
  Kokkos::finalize();
  return result;
}

#include "toleranceGui.moc"

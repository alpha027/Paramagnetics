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
    ToleranceGuiTest test;
    result = QTest::qExec(&test, argc, argv);
  }
  Kokkos::finalize();
  return result;
}

#include "toleranceGui.moc"

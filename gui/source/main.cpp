/*
  The viewer.

  Two things have to be got right here and nowhere else.

  Kokkos and Qt both want to own the process. Kokkos is started first, because
  it reads its own arguments off the command line, and it is finished last,
  after every object that could be holding a Kokkos view is gone. A service
  that outlives Kokkos::finalize takes the process down with it, which is why
  the window lives in a scope of its own below.

  And the surface format has to be asked for before the first window is made,
  or the viewport gets whatever context the platform felt like giving it.
*/

#include "MainWindow.h"

#include <greeter/service/SimulationService.h>
#include <greeter/view/Snapshot.h>

#include <Kokkos_Core.hpp>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QSurfaceFormat>

#include <iostream>


int main(int argc, char** argv) {

  // Before anything else: Kokkos takes its own --kokkos-* arguments out of
  // argv, and what is left is what Qt sees.
  Kokkos::initialize(argc, argv);

  int status = 0;

  {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication application(argc, argv);

    QApplication::setApplicationName("ParaMagneticS viewer");
    QApplication::setApplicationVersion("1.0");

    // Both cross from the worker thread to the drawing thread in a queued
    // signal, and Qt has to have been told their names to copy them.
    qRegisterMetaType<greeter::view::Snapshot>("greeter::view::Snapshot");
    qRegisterMetaType<greeter::service::FieldRequest>("greeter::service::FieldRequest");

    QCommandLineParser parser;
    parser.setApplicationDescription(
      "Shows a ParaMagneticS scene, the field it makes and the forces in it.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
      "file", "An input file to open, or a .pmsnap snapshot of a finished run.");

    const QCommandLineOption draw_option(
      {"d", "draw"},
      "Draw the run to a PNG and quit, instead of opening a window. Needs a "
      "display or an X server such as xvfb.",
      "png");

    const QCommandLineOption show_option(
      "show",
      "What to draw, comma separated, out of magnets, edges, polarization, "
      "slice, arrows, lines, box, forces and torques. Anything not named is "
      "turned off.",
      "list");

    const QCommandLineOption opacity_option(
      "opacity", "How solid the magnets are, from 0.1 to 1.", "fraction");

    const QCommandLineOption slice_axis_option(
      "slice-axis", "Which axis the slice plane is across, x, y or z.", "axis");

    parser.addOption(draw_option);
    parser.addOption(show_option);
    parser.addOption(opacity_option);
    parser.addOption(slice_axis_option);

    parser.process(application);

    viewer::MainWindow window;
    window.show();

    if (parser.isSet(show_option)) {
      window.applyShowList(parser.value(show_option).split(','));
    }

    if (parser.isSet(opacity_option)) {
      window.setOpacity(parser.value(opacity_option).toFloat());
    }

    if (parser.isSet(slice_axis_option)) {

      const QString axis = parser.value(slice_axis_option).trimmed().toLower();

      window.setSliceAxis(axis == "x" ? 0 : (axis == "y" ? 1 : 2));
    }

    if (parser.isSet(draw_option)) {
      window.renderToFileAndQuit(parser.value(draw_option));
    }

    if (!parser.positionalArguments().isEmpty()) {
      window.openAtStart(parser.positionalArguments().first());
    }

    status = application.exec();
  }

  // Every view is gone by here, the window and its worker having been
  // destroyed at the end of the scope above.
  Kokkos::finalize();

  return status;
}

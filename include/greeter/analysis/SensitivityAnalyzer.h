#ifndef GREETER_SENSITIVITY_ANALYZER_H
#define GREETER_SENSITIVITY_ANALYZER_H

#include <greeter/analysis/ToleranceAnalyzer.h>
#include <greeter/service/SensitivityAnalysis.h>

namespace greeter {
  namespace analysis {
    service::SensitivityReport analyzeSensitivity(const MagnetCollection& collection,
                                                  const view::SceneSnapshot& scene,
                                                  const service::FieldRequest& request,
                                                  const service::SensitivitySettings& settings,
                                                  service::ProgressSink* progress,
                                                  const service::RunOptions& options);
  }
}  // namespace greeter
#endif

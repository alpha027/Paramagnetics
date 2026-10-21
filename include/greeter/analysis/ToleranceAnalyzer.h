#ifndef GREETER_TOLERANCE_ANALYZER_H
#define GREETER_TOLERANCE_ANALYZER_H

#include <greeter/MagnetCollection.h>
#include <greeter/service/SimulationService.h>

namespace greeter {
  namespace analysis {

    service::ToleranceReport analyzeTolerance(const MagnetCollection& collection,
                                              const service::FieldRequest& request,
                                              const service::ToleranceSettings& settings,
                                              service::ProgressSink* progress,
                                              const service::RunOptions& options);

  }  // namespace analysis
}  // namespace greeter
#endif

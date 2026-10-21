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
                                              const service::RunOptions& options,
                                              const std::vector<uint32_t>* selected_magnets
                                              = nullptr);

  }  // namespace analysis
}  // namespace greeter
#endif

#include <boost/python.hpp>

#include "core/pipeline/Pipeline.h"
#include "core/pipeline/StageProcessor.h"
#include "core/pipeline/BatchProcessor.h"

namespace bp = boost::python;
using namespace usdcleaner;

void wrapPipeline() {
    bp::class_<ProcessorConfig>("ProcessorConfig")
        .def_readwrite("enable_welding", &ProcessorConfig::enableWelding)
        .def_readwrite("welding_epsilon", &ProcessorConfig::weldingEpsilon)
        .def_readwrite("auto_epsilon", &ProcessorConfig::autoEpsilon)
        .def_readwrite("enable_degenerate_removal", &ProcessorConfig::enableDegenerateRemoval)
        .def_readwrite("enable_lamina_removal", &ProcessorConfig::enableLaminaRemoval)
        .def_readwrite("keep_opposite_winding", &ProcessorConfig::keepOppositeWinding)
        .def_readwrite("enable_material_dedup", &ProcessorConfig::enableMaterialDedup)
        .def_readwrite("skip_animated_materials", &ProcessorConfig::skipAnimatedMaterials)
        .def_readwrite("enable_cache_optimization", &ProcessorConfig::enableCacheOptimization)
        .def_readwrite("triangulate", &ProcessorConfig::triangulate)
        .def_readwrite("output_format", &ProcessorConfig::outputFormat)
    ;

    bp::class_<StageProcessor>("StageProcessor",
        bp::init<bp::optional<const ProcessorConfig&>>())
        .def("process", &StageProcessor::Process)
        .def("get_metrics_json", &StageProcessor::GetMetricsJson)
    ;

    bp::class_<BatchConfig>("BatchConfig")
        .def_readwrite("processor_config", &BatchConfig::processorConfig)
        .def_readwrite("max_concurrent_files", &BatchConfig::maxConcurrentFiles)
        .def_readwrite("output_directory", &BatchConfig::outputDirectory)
    ;

    bp::class_<BatchProcessor>("BatchProcessor",
        bp::init<bp::optional<const BatchConfig&>>())
        .def("process_directory", &BatchProcessor::ProcessDirectory)
        .def("process_files", &BatchProcessor::ProcessFiles)
    ;
}

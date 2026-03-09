#include <boost/python.hpp>

#include "core/welding/VertexWelder.h"
#include "core/topology/DegenerateFaceRemover.h"
#include "core/topology/LaminaFaceRemover.h"
#include "core/materials/MaterialDeduplicator.h"
#include "core/cache/GpuCacheOptimizer.h"

namespace bp = boost::python;
using namespace usdcleaner;

void wrapPasses() {
    // Base class (abstract -- not directly instantiable from Python)
    bp::class_<OptimizationPass, boost::noncopyable>("OptimizationPass", bp::no_init)
        .def("get_name", bp::pure_virtual(&OptimizationPass::GetName))
        .def("is_enabled", &OptimizationPass::IsEnabled)
        .def("set_enabled", &OptimizationPass::SetEnabled)
    ;

    bp::class_<VertexWelder, bp::bases<OptimizationPass>>("VertexWelder")
        .def("set_epsilon", &VertexWelder::SetEpsilon)
        .def("get_epsilon", &VertexWelder::GetEpsilon)
        .def("set_auto_epsilon", &VertexWelder::SetAutoEpsilon)
        .def("get_auto_epsilon", &VertexWelder::GetAutoEpsilon)
    ;

    bp::class_<DegenerateFaceRemover, bp::bases<OptimizationPass>>(
        "DegenerateFaceRemover")
    ;

    bp::class_<LaminaFaceRemover, bp::bases<OptimizationPass>>(
        "LaminaFaceRemover")
        .def("set_keep_opposite_winding", &LaminaFaceRemover::SetKeepOppositeWinding)
        .def("get_keep_opposite_winding", &LaminaFaceRemover::GetKeepOppositeWinding)
    ;

    bp::class_<MaterialDeduplicator, bp::bases<OptimizationPass>>(
        "MaterialDeduplicator")
        .def("set_skip_animated", &MaterialDeduplicator::SetSkipAnimated)
        .def("get_skip_animated", &MaterialDeduplicator::GetSkipAnimated)
    ;

    bp::class_<GpuCacheOptimizer, bp::bases<OptimizationPass>>(
        "GpuCacheOptimizer")
        .def("set_triangulate", &GpuCacheOptimizer::SetTriangulate)
        .def("get_triangulate", &GpuCacheOptimizer::GetTriangulate)
    ;
}

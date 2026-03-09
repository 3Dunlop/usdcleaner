#include <boost/python.hpp>

#include "core/common/Metrics.h"

namespace bp = boost::python;
using namespace usdcleaner;

void wrapMetrics() {
    bp::class_<PassMetrics>("PassMetrics")
        .def_readonly("pass_name", &PassMetrics::passName)
        .def_readonly("mesh_count", &PassMetrics::meshCount)
        .def_readonly("total_vertices", &PassMetrics::totalVertices)
        .def_readonly("total_faces", &PassMetrics::totalFaces)
        .def_readonly("material_count", &PassMetrics::materialCount)
        .def_readonly("prim_count", &PassMetrics::primCount)
        .def_readonly("vertices_removed", &PassMetrics::verticesRemoved)
        .def_readonly("faces_removed", &PassMetrics::facesRemoved)
        .def_readonly("materials_removed", &PassMetrics::materialsRemoved)
    ;

    bp::class_<MetricsCollector>("MetricsCollector")
        .def("to_json", &MetricsCollector::ToJson)
    ;
}

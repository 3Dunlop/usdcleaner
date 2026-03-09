#include "core/common/Metrics.h"

#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdShade/material.h>

#include <sstream>

namespace usdcleaner {

PassMetrics MetricsCollector::Snapshot(const UsdStageRefPtr& stage,
                                       const std::string& label) const {
    PassMetrics m;
    m.passName = label;

    for (const auto& prim : stage->Traverse()) {
        m.primCount++;

        if (prim.IsA<UsdGeomMesh>()) {
            m.meshCount++;
            UsdGeomMesh mesh(prim);

            VtVec3fArray points;
            if (mesh.GetPointsAttr().Get(&points)) {
                m.totalVertices += points.size();
            }

            VtIntArray faceVertexCounts;
            if (mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts)) {
                m.totalFaces += faceVertexCounts.size();
            }

            VtIntArray faceVertexIndices;
            if (mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices)) {
                m.totalFaceVertices += faceVertexIndices.size();
            }
        }

        if (prim.IsA<UsdGeomXform>() && !prim.IsA<UsdGeomMesh>()) {
            m.xformCount++;
        }

        if (prim.IsA<UsdShadeMaterial>()) {
            m.materialCount++;
        }
    }

    return m;
}

void MetricsCollector::RecordPass(const std::string& passName,
                                   const PassMetrics& before,
                                   const PassMetrics& after) {
    PassMetrics result;
    result.passName = passName;
    result.meshCount = after.meshCount;
    result.totalVertices = after.totalVertices;
    result.totalFaces = after.totalFaces;
    result.totalFaceVertices = after.totalFaceVertices;
    result.materialCount = after.materialCount;
    result.primCount = after.primCount;
    result.xformCount = after.xformCount;

    result.verticesRemoved =
        static_cast<int64_t>(before.totalVertices) -
        static_cast<int64_t>(after.totalVertices);
    result.facesRemoved =
        static_cast<int64_t>(before.totalFaces) -
        static_cast<int64_t>(after.totalFaces);
    result.materialsRemoved =
        static_cast<int64_t>(before.materialCount) -
        static_cast<int64_t>(after.materialCount);
    result.primsRemoved =
        static_cast<int64_t>(before.primCount) -
        static_cast<int64_t>(after.primCount);

    passResults_.push_back(result);
}

std::string MetricsCollector::ToJson() const {
    std::ostringstream ss;
    ss << "{\n  \"passes\": [\n";

    for (size_t i = 0; i < passResults_.size(); ++i) {
        const auto& r = passResults_[i];
        ss << "    {\n";
        ss << "      \"name\": \"" << r.passName << "\",\n";
        ss << "      \"meshCount\": " << r.meshCount << ",\n";
        ss << "      \"totalVertices\": " << r.totalVertices << ",\n";
        ss << "      \"totalFaces\": " << r.totalFaces << ",\n";
        ss << "      \"materialCount\": " << r.materialCount << ",\n";
        ss << "      \"primCount\": " << r.primCount << ",\n";
        ss << "      \"verticesRemoved\": " << r.verticesRemoved << ",\n";
        ss << "      \"facesRemoved\": " << r.facesRemoved << ",\n";
        ss << "      \"materialsRemoved\": " << r.materialsRemoved << ",\n";
        ss << "      \"primsRemoved\": " << r.primsRemoved << "\n";
        ss << "    }";
        if (i + 1 < passResults_.size()) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n}";
    return ss.str();
}

} // namespace usdcleaner

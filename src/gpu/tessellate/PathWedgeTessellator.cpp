/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/tessellate/PathWedgeTessellator.h"

#include "src/core/SkPathPriv.h"
#include "src/gpu/tessellate/AffineMatrix.h"
#include "src/gpu/tessellate/MidpointContourParser.h"
#include "src/gpu/tessellate/PatchWriter.h"
#include "src/gpu/tessellate/PathCurveTessellator.h"
#include "src/gpu/tessellate/WangsFormula.h"

#if SK_GPU_V1
#include "src/gpu/GrMeshDrawTarget.h"
#include "src/gpu/GrOpFlushState.h"
#include "src/gpu/GrResourceProvider.h"
#endif

namespace skgpu {

namespace {

using Writer = PatchWriter<GrVertexChunkBuilder,
                           Required<PatchAttribs::kFanPoint>,
                           Optional<PatchAttribs::kColor>,
                           Optional<PatchAttribs::kWideColorIfEnabled>,
                           Optional<PatchAttribs::kExplicitCurveType>>;

int write_patches(Writer&& patchWriter,
                  const SkMatrix& shaderMatrix,
                  const PathTessellator::PathDrawList& pathDrawList) {
    wangs_formula::VectorXform shaderXform(shaderMatrix);
    for (auto [pathMatrix, path, color] : pathDrawList) {
        AffineMatrix m(pathMatrix);
        if (patchWriter.attribs() & PatchAttribs::kColor) {
            patchWriter.updateColorAttrib(color);
        }
        MidpointContourParser parser(path);
        while (parser.parseNextContour()) {
            patchWriter.updateFanPointAttrib(m.mapPoint(parser.currentMidpoint()));
            SkPoint lastPoint = {0, 0};
            SkPoint startPoint = {0, 0};
            for (auto [verb, pts, w] : parser.currentContour()) {
                switch (verb) {
                    case SkPathVerb::kMove: {
                        startPoint = lastPoint = pts[0];
                        break;
                    }

                    case SkPathVerb::kLine: {
                        // Explicitly convert the line to an equivalent cubic w/ four distinct
                        // control points because it fans better and avoids double-hitting pixels.
                        patchWriter.writeLine(m.map2Points(pts));
                        lastPoint = pts[1];
                        break;
                    }

                    case SkPathVerb::kQuad: {
                        auto [p0, p1] = m.map2Points(pts);
                        auto p2 = m.map1Point(pts+2);

                        patchWriter.writeQuadratic(p0, p1, p2, shaderXform);
                        lastPoint = pts[2];
                        break;
                    }

                    case SkPathVerb::kConic: {
                        auto [p0, p1] = m.map2Points(pts);
                        auto p2 = m.map1Point(pts+2);

                        patchWriter.writeConic(p0, p1, p2, *w, shaderXform);
                        lastPoint = pts[2];
                        break;
                    }

                    case SkPathVerb::kCubic: {
                        auto [p0, p1] = m.map2Points(pts);
                        auto [p2, p3] = m.map2Points(pts+2);

                        patchWriter.writeCubic(p0, p1, p2, p3, shaderXform);
                        lastPoint = pts[3];
                        break;
                    }

                    case SkPathVerb::kClose: {
                        break;  // Ignore. We can assume an implicit close at the end.
                    }
                }
            }
            if (lastPoint != startPoint) {
                SkPoint pts[2] = {lastPoint, startPoint};
                patchWriter.writeLine(m.map2Points(pts));
            }
        }
    }

    return patchWriter.requiredResolveLevel();
}

}  // namespace

void PathWedgeTessellator::WriteFixedVertexBuffer(VertexWriter vertexWriter, size_t bufferSize) {
    SkASSERT(bufferSize >= sizeof(SkPoint));

    // Start out with the fan point. A negative resolve level indicates the fan point.
    vertexWriter << -1.f/*resolveLevel*/ << -1.f/*idx*/;

    // The rest is the same as for curves.
    PathCurveTessellator::WriteFixedVertexBuffer(std::move(vertexWriter),
                                                 bufferSize - sizeof(SkPoint));
}

void PathWedgeTessellator::WriteFixedIndexBuffer(VertexWriter vertexWriter, size_t bufferSize) {
    SkASSERT(bufferSize >= sizeof(uint16_t) * 3);

    // Start out with the fan triangle.
    vertexWriter << (uint16_t)0 << (uint16_t)1 << (uint16_t)2;

    // The rest is the same as for curves, with a baseIndex of 1.
    PathCurveTessellator::WriteFixedIndexBufferBaseIndex(std::move(vertexWriter),
                                                         bufferSize - sizeof(uint16_t) * 3,
                                                         1);
}

#if SK_GPU_V1

SKGPU_DECLARE_STATIC_UNIQUE_KEY(gFixedVertexBufferKey);
SKGPU_DECLARE_STATIC_UNIQUE_KEY(gFixedIndexBufferKey);

void PathWedgeTessellator::prepareFixedCountBuffers(GrMeshDrawTarget* target) {
    GrResourceProvider* rp = target->resourceProvider();

    SKGPU_DEFINE_STATIC_UNIQUE_KEY(gFixedVertexBufferKey);

    fFixedVertexBuffer = rp->findOrMakeStaticBuffer(GrGpuBufferType::kVertex,
                                                    FixedVertexBufferSize(kMaxFixedResolveLevel),
                                                    gFixedVertexBufferKey,
                                                    WriteFixedVertexBuffer);

    SKGPU_DEFINE_STATIC_UNIQUE_KEY(gFixedIndexBufferKey);

    fFixedIndexBuffer = rp->findOrMakeStaticBuffer(GrGpuBufferType::kIndex,
                                                   FixedIndexBufferSize(kMaxFixedResolveLevel),
                                                   gFixedIndexBufferKey,
                                                   WriteFixedIndexBuffer);
}

void PathWedgeTessellator::prepare(GrMeshDrawTarget* target,
                                   int maxTessellationSegments,
                                   const SkMatrix& shaderMatrix,
                                   const PathDrawList& pathDrawList,
                                   int totalCombinedPathVerbCnt,
                                   bool willUseTessellationShaders) {
    if (int patchPreallocCount = PatchPreallocCount(totalCombinedPathVerbCnt)) {
        Writer writer{fAttribs, maxTessellationSegments,
                      target, &fVertexChunkArray, patchPreallocCount};
        int resolveLevel = write_patches(std::move(writer), shaderMatrix, pathDrawList);
        this->updateResolveLevel(resolveLevel);
    }
    if (!willUseTessellationShaders) {
        this->prepareFixedCountBuffers(target);
    }
}

void PathWedgeTessellator::drawTessellated(GrOpFlushState* flushState) const {
    for (const GrVertexChunk& chunk : fVertexChunkArray) {
        flushState->bindBuffers(nullptr, nullptr, chunk.fBuffer);
        flushState->draw(chunk.fCount * 5, chunk.fBase * 5);
    }
}

void PathWedgeTessellator::drawFixedCount(GrOpFlushState* flushState) const {
    if (!fFixedVertexBuffer || !fFixedIndexBuffer) {
        return;
    }
    // Emit 3 vertices per curve triangle, plus 3 more for the fan triangle.
    int fixedIndexCount = (NumCurveTrianglesAtResolveLevel(fFixedResolveLevel) + 1) * 3;
    for (const GrVertexChunk& chunk : fVertexChunkArray) {
        flushState->bindBuffers(fFixedIndexBuffer, chunk.fBuffer, fFixedVertexBuffer);
        flushState->drawIndexedInstanced(fixedIndexCount, 0, chunk.fCount, chunk.fBase, 0);
    }
}

#endif

}  // namespace skgpu

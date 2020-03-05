/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**************************************************************************************************
 *** This file was autogenerated from GrDeviceSpaceEffect.fp; do not modify.
 **************************************************************************************************/
#include "GrDeviceSpaceEffect.h"

#include "include/gpu/GrTexture.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramBuilder.h"
#include "src/sksl/SkSLCPP.h"
#include "src/sksl/SkSLUtil.h"
class GrGLSLDeviceSpaceEffect : public GrGLSLFragmentProcessor {
public:
    GrGLSLDeviceSpaceEffect() {}
    void emitCode(EmitArgs& args) override {
        GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
        const GrDeviceSpaceEffect& _outer = args.fFp.cast<GrDeviceSpaceEffect>();
        (void)_outer;
        SkString _input204 = SkStringPrintf("%s", args.fInputColor);
        SkString _sample204;
        SkString _coords204("sk_FragCoord.xy");
        _sample204 =
                this->invokeChild(_outer.fp_index, _input204.c_str(), args, _coords204.c_str());
        fragBuilder->codeAppendf("%s = %s;\n", args.fOutputColor, _sample204.c_str());
    }

private:
    void onSetData(const GrGLSLProgramDataManager& pdman,
                   const GrFragmentProcessor& _proc) override {}
};
GrGLSLFragmentProcessor* GrDeviceSpaceEffect::onCreateGLSLInstance() const {
    return new GrGLSLDeviceSpaceEffect();
}
void GrDeviceSpaceEffect::onGetGLSLProcessorKey(const GrShaderCaps& caps,
                                                GrProcessorKeyBuilder* b) const {}
bool GrDeviceSpaceEffect::onIsEqual(const GrFragmentProcessor& other) const {
    const GrDeviceSpaceEffect& that = other.cast<GrDeviceSpaceEffect>();
    (void)that;
    return true;
}
GrDeviceSpaceEffect::GrDeviceSpaceEffect(const GrDeviceSpaceEffect& src)
        : INHERITED(kGrDeviceSpaceEffect_ClassID, src.optimizationFlags()), fp_index(src.fp_index) {
    {
        auto clone = src.childProcessor(fp_index).clone();
        clone->setSampledWithExplicitCoords(
                src.childProcessor(fp_index).isSampledWithExplicitCoords());
        this->registerChildProcessor(std::move(clone));
    }
}
std::unique_ptr<GrFragmentProcessor> GrDeviceSpaceEffect::clone() const {
    return std::unique_ptr<GrFragmentProcessor>(new GrDeviceSpaceEffect(*this));
}
GR_DEFINE_FRAGMENT_PROCESSOR_TEST(GrDeviceSpaceEffect);
#if GR_TEST_UTILS
std::unique_ptr<GrFragmentProcessor> GrDeviceSpaceEffect::TestCreate(GrProcessorTestData* d) {
    std::unique_ptr<GrFragmentProcessor> fp;
    // We have a restriction that explicit coords only work for FPs with exactly one
    // coord transform.
    do {
        fp = GrProcessorUnitTest::MakeChildFP(d);
    } while (fp->numCoordTransforms() != 1);
    return GrDeviceSpaceEffect::Make(std::move(fp));
}
#endif

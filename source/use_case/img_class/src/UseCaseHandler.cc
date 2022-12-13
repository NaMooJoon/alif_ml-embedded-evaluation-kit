/*
 * Copyright (c) 2021-2022 Arm Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "UseCaseHandler.hpp"

#include "Classifier.hpp"
#include "InputFiles.hpp"
#include "MobileNetModel.hpp"
#include "ImageUtils.hpp"
#include "UseCaseCommonUtils.hpp"
#include "hal.h"
#include "log_macros.h"
#include "ImgClassProcessing.hpp"

#include <cinttypes>

#include "lvgl.h"
extern lv_obj_t *labelResult1;
extern lv_obj_t *labelResult2;
extern lv_obj_t *labelResult3;
extern lv_obj_t *labelResult4;
extern lv_obj_t *labelResult5;

using ImgClassClassifier = arm::app::Classifier;


#define SKIP_MODEL 0
#if SKIP_MODEL
#define RAW_BUFFER ((void *) 0x08000000)
#else
#define RAW_BUFFER inputTensor->data.data
#endif

extern bool run_requested(void);
extern "C" {
uint32_t tprof1, tprof2, tprof3, tprof4, tprof5;
}

namespace arm {
namespace app {

    /* Image classification inference handler. */
    bool ClassifyImageHandler(ApplicationContext& ctx, uint32_t imgIndex, bool runAll)
    {
        auto& profiler = ctx.Get<Profiler&>("profiler");
        auto& model = ctx.Get<Model&>("model");
        /* If the request has a valid size, set the image index as it might not be set. */
        if (imgIndex < NUMBER_OF_FILES) {
            if (!SetAppCtxIfmIdx(ctx, imgIndex, "imgIndex")) {
                return false;
            }
        }
        auto initialImgIdx = ctx.Get<uint32_t>("imgIndex");

        constexpr uint32_t dataPsnImgDownscaleFactor = 2;
        constexpr uint32_t dataPsnImgStartX = 10;
        constexpr uint32_t dataPsnImgStartY = 35;

        constexpr uint32_t dataPsnTxtInfStartX = 150;
        constexpr uint32_t dataPsnTxtInfStartY = 40;

#if !SKIP_MODEL
        if (!model.IsInited()) {
            printf_err("Model is not initialised! Terminating processing.\n");
            return false;
        }

        TfLiteTensor* inputTensor = model.GetInputTensor(0);
        TfLiteTensor* outputTensor = model.GetOutputTensor(0);
        if (!inputTensor->dims) {
            printf_err("Invalid input tensor dims\n");
            return false;
        } else if (inputTensor->dims->size < 4) {
            printf_err("Input tensor dimension should be = 4\n");
            return false;
        }

        /* Get input shape for displaying the image. */
        TfLiteIntArray* inputShape = model.GetInputShape(0);
        const uint32_t nCols = inputShape->data[arm::app::MobileNetModel::ms_inputColsIdx];
        const uint32_t nRows = inputShape->data[arm::app::MobileNetModel::ms_inputRowsIdx];
        const uint32_t nChannels = inputShape->data[arm::app::MobileNetModel::ms_inputChannelsIdx];

        /* Set up pre and post-processing. */
        ImgClassPreProcess preProcess = ImgClassPreProcess(inputTensor, model.IsDataSigned());

        std::vector<ClassificationResult> results;
        ImgClassPostProcess postProcess = ImgClassPostProcess(outputTensor,
                ctx.Get<ImgClassClassifier&>("classifier"), ctx.Get<std::vector<std::string>&>("labels"),
                results);
#endif

        do {
            int err = hal_get_image_data((uint8_t *) RAW_BUFFER);
            if (err) {
                printf_err("hal_get_image_data failed with : %d", err);
                return false;
            }

            tprof5 = ARM_PMU_Get_CCNTR();
            /* Display this image on the LCD. */
            hal_lcd_display_image(
                (const uint8_t*)RAW_BUFFER,
                224, 224, 3, //nCols, nRows, nChannels,
                dataPsnImgStartX, dataPsnImgStartY, dataPsnImgDownscaleFactor);
            tprof5 = ARM_PMU_Get_CCNTR() - tprof5;

            if (SKIP_MODEL || !run_requested()) {
#if SHOW_PROFILING
                lv_label_set_text_fmt(labelResult1, "tprof1=%.3f ms", (double)tprof1 / SystemCoreClock * 1000);
                lv_label_set_text_fmt(labelResult2, "tprof2=%.3f ms", (double)tprof2 / SystemCoreClock * 1000);
                lv_label_set_text_fmt(labelResult3, "tprof3=%.3f ms", (double)tprof3 / SystemCoreClock * 1000);
                lv_label_set_text_fmt(labelResult4, "tprof4=%.3f ms", (double)tprof4 / SystemCoreClock * 1000);
                lv_label_set_text_fmt(labelResult5, "tprof5=%.3f ms", (double)tprof5 / SystemCoreClock * 1000);
#endif
                break;
            }

#if !SKIP_MODEL
            const size_t imgSz = inputTensor->bytes < IMAGE_DATA_SIZE ?
                                  inputTensor->bytes : IMAGE_DATA_SIZE;

            /* Run the pre-processing, inference and post-processing. */
            if (!preProcess.DoPreProcess(inputTensor->data.data, imgSz)) {
                printf_err("Pre-processing failed.");
                return false;
            }

            if (!RunInference(model, profiler)) {
                printf_err("Inference failed.");
                return false;
            }

            if (!postProcess.DoPostProcess()) {
                printf_err("Post-processing failed.");
                return false;
            }

            /* Add results to context for access outside handler. */
            ctx.Set<std::vector<ClassificationResult>>("results", results);

#if VERIFY_TEST_OUTPUT
            arm::app::DumpTensor(outputTensor);
#endif /* VERIFY_TEST_OUTPUT */

            lv_label_set_text_fmt(labelResult1, "%s (%ld%%)", results[0].m_label.c_str(), (uint32_t)(results[0].m_normalisedVal * 100));
            lv_label_set_text_fmt(labelResult2, "%s (%ld%%)", results[1].m_label.c_str(), (uint32_t)(results[1].m_normalisedVal * 100));
            lv_label_set_text_fmt(labelResult3, "%s (%ld%%)", results[2].m_label.c_str(), (uint32_t)(results[2].m_normalisedVal * 100));

            if (!PresentInferenceResult(results)) {
                return false;
            }

            profiler.PrintProfilingResult();

            IncrementAppCtxIfmIdx(ctx,"imgIndex");
#endif
        } while (runAll && ctx.Get<uint32_t>("imgIndex") != initialImgIdx);

        return true;
    }

} /* namespace app */
} /* namespace arm */

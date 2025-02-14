// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/xr_linear.h"
#include "composition_utils.h"
#include "conformance_framework.h"
#include "utilities/throw_helpers.h"
#include "utilities/types_and_constants.h"
#include "utilities/xrduration_literals.h"

#include <catch2/catch_test_macros.hpp>
#include <openxr/openxr.h>

#include <algorithm>
#include <array>
#include <numeric>

using namespace Conformance;

namespace Conformance
{
    using namespace openxr::math_operators;

    // Purpose: Verify behavior of quad visibility and occlusion with the expectation that:
    // 1. Quads render with painters algo.
    // 2. Quads which are facing away are not visible.
    TEST_CASE("QuadOcclusion", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Quad Occlusion");
        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "quad_occlusion.png",
            "This test includes a blue and green quad at Z=-2 with opposite rotations on Y axis forming X. The green quad should be"
            " fully visible due to painter's algorithm. A red quad is facing away and should not be visible.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSwapchain greenSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Green);
        const XrSwapchain blueSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Blue);
        const XrSwapchain redSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Red);

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        // Each quad is rotated on Y axis by 45 degrees to form an X.
        // Green is added second so it should draw over the blue quad.
        const XrQuaternionf blueRot = Quat::FromAxisAngle({0, 1, 0}, DegToRad(-45));
        interactiveLayerManager.AddLayer(compositionHelper.CreateQuadLayer(blueSwapchain, viewSpace, 1.0f, XrPosef{blueRot, {0, 0, -2}}));
        const XrQuaternionf greenRot = Quat::FromAxisAngle({0, 1, 0}, DegToRad(45));
        interactiveLayerManager.AddLayer(compositionHelper.CreateQuadLayer(greenSwapchain, viewSpace, 1.0f, XrPosef{greenRot, {0, 0, -2}}));
        // Red quad is rotated away from the viewer and should not be visible.
        const XrQuaternionf redRot = Quat::FromAxisAngle({0, 1, 0}, DegToRad(180));
        interactiveLayerManager.AddLayer(compositionHelper.CreateQuadLayer(redSwapchain, viewSpace, 1.0f, XrPosef{redRot, {0, 0, -1}}));

        RenderLoop(session, [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); }).Loop();
    }

    // Purpose: Verify order of transforms by exercising the two ways poses can be specified:
    // 1. A pose offset when creating the space
    // 2. A pose offset when adding the layer
    // If the poses are applied in an incorrect order, the quads will not render in the correct place or orientation.
    TEST_CASE("QuadPoses", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Quad Poses");
        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "quad_poses.png",
            "Render pairs of quads using similar poses to validate order of operations. The blue/green quads apply a"
            " rotation around the Z axis on an XrSpace and then translate the quad out on the Z axis through the quad"
            " layer's pose. The purple/yellow quads apply the same translation on the XrSpace and the rotation on the"
            " quad layer's pose.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSwapchain blueSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Blue);
        const XrSwapchain greenSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Green);
        const XrSwapchain orangeSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Orange);
        const XrSwapchain yellowSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Yellow);

        constexpr int RotationCount = 2;
        constexpr float MaxRotationDegrees = 30;
        // For each rotation there are a pair of quads.
        static_assert(RotationCount * 2 <= XR_MIN_COMPOSITION_LAYERS_SUPPORTED, "Too many layers");

        for (int i = 0; i < RotationCount; i++) {
            const float radians = Math::LinearMap(i, 0, RotationCount - 1, DegToRad(-MaxRotationDegrees), DegToRad(MaxRotationDegrees));

            const XrPosef pose1 = XrPosef{Quat::FromAxisAngle({0, 1, 0}, radians), {0, 0, 0}};
            const XrPosef pose2 = XrPosef{Quat::Identity, {0, 0, -1}};

            const XrSpace viewSpacePose1 = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, pose1);
            const XrSpace viewSpacePose2 = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, pose2);

            auto quad1 = compositionHelper.CreateQuadLayer((i % 2) == 0 ? blueSwapchain : greenSwapchain, viewSpacePose1, 0.25f, pose2);
            interactiveLayerManager.AddLayer(quad1);

            auto quad2 = compositionHelper.CreateQuadLayer((i % 2) == 0 ? orangeSwapchain : yellowSwapchain, viewSpacePose2, 0.25f, pose1);
            interactiveLayerManager.AddLayer(quad2);
        }

        RenderLoop(session, [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); }).Loop();
    }

    // Purpose: Validates alpha blending (both premultiplied and unpremultiplied).
    TEST_CASE("SourceAlphaBlending", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Source Alpha Blending");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "source_alpha_blending.png",
                                                        "All three squares should have an identical blue-green gradient.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        constexpr float QuadZ = -3;  // How far away quads are placed.

        // Creates image with correctly combined green and blue gradient (this is the the source of truth).
        {
            Conformance::RGBAImage blueGradientOverGreen(256, 256);
            for (int y = 0; y < 256; y++) {
                const float t = y / 255.0f;
                const XrColor4f dst = Colors::Green;
                const XrColor4f src{0, 0, t, t};

                // The blended color here has a 0 alpha value to test that the runtime is ignoring the texture alpha when
                // the XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT flag is not set. If the runtime is erroneously
                // reading texture alpha, it is more likely to output black pixels.
                const XrColor4f blended{dst.r * (1 - src.a) + src.r, dst.g * (1 - src.a) + src.g, dst.b * (1 - src.a) + src.b, 0};
                blueGradientOverGreen.DrawRect(0, y, blueGradientOverGreen.width, 1, blended);
            }

            const XrSwapchain answerSwapchain = compositionHelper.CreateStaticSwapchainImage(blueGradientOverGreen);
            XrCompositionLayerQuad* truthQuad =
                compositionHelper.CreateQuadLayer(answerSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {0, 0, QuadZ}});

            // Set the unpremultiplied bit on this quad (and the green ones below) to make it more obvious when a runtime
            // supports the premultiplied flag but not the texture flag. Without this bit set, the final color will be:
            //   ( 1 - alpha ) * dst + src
            // dst is black, and alpha is 0, so the output is just src.
            // If we use unpremultiplied, the formula becomes:
            //   ( 1 - alpha ) * dst + alpha * src
            // which results in black pixels and is obviously wrong.
            truthQuad->layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;

            interactiveLayerManager.AddLayer(truthQuad);
        }

        auto createGradientTest = [&](bool premultiplied, float x, float y) {
            // A solid green quad layer will be composited under a blue gradient.
            {
                const XrSwapchain greenSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::GreenZeroAlpha);
                XrCompositionLayerQuad* greenQuad =
                    compositionHelper.CreateQuadLayer(greenSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {x, y, QuadZ}});
                greenQuad->layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
                interactiveLayerManager.AddLayer(greenQuad);
            }

            // Create gradient of blue lines from 0.0 to 1.0.
            {
                Conformance::RGBAImage blueGradient(256, 256);
                for (int row = 0; row < blueGradient.height; row++) {
                    XrColor4f color{0, 0, 1, row / (float)blueGradient.height};
                    if (premultiplied) {
                        color = XrColor4f{color.r * color.a, color.g * color.a, color.b * color.a, color.a};
                    }

                    blueGradient.DrawRect(0, row, blueGradient.width, 1, color);
                }

                const XrSwapchain gradientSwapchain = compositionHelper.CreateStaticSwapchainImage(blueGradient);
                XrCompositionLayerQuad* gradientQuad =
                    compositionHelper.CreateQuadLayer(gradientSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {x, y, QuadZ}});

                gradientQuad->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                if (!premultiplied) {
                    gradientQuad->layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
                }

                interactiveLayerManager.AddLayer(gradientQuad);
            }
        };

        createGradientTest(true, -1.02f, 0);  // Test premultiplied (left of center "answer")
        createGradientTest(false, 1.02f, 0);  // Test unpremultiplied (right of center "answer")

        RenderLoop(session, [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); }).Loop();
    }

    // Purpose: Validate eye visibility flags.
    TEST_CASE("EyeVisibility", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Eye Visibility");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "eye_visibility.png",
                                                        "A green quad is shown in the left eye and a blue quad is shown in the right eye.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();

        compositionHelper.BeginSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW);

        const XrSwapchain greenSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Green);
        XrCompositionLayerQuad* quad1 =
            compositionHelper.CreateQuadLayer(greenSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {-1, 0, -2}});
        quad1->eyeVisibility = XR_EYE_VISIBILITY_LEFT;
        interactiveLayerManager.AddLayer(quad1);

        const XrSwapchain blueSwapchain = compositionHelper.CreateStaticSwapchainSolidColor(Colors::Blue);
        XrCompositionLayerQuad* quad2 =
            compositionHelper.CreateQuadLayer(blueSwapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {1, 0, -2}});
        quad2->eyeVisibility = XR_EYE_VISIBILITY_RIGHT;
        interactiveLayerManager.AddLayer(quad2);

        RenderLoop(session, [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); }).Loop();
    }

    TEST_CASE("Subimage", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test subimage without a graphics plugin");
        }

        CompositionHelper compositionHelper("Subimage Tests");
        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "subimage.png",
            "Creates a 4x2 grid of quad layers testing subImage array index and imageRect. Red should not be visible except minor bleed in.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, XrPosef{Quat::Identity, {0, 0, -1}});

        constexpr float QuadZ = -4;  // How far away quads are placed.
        constexpr int ImageColCount = 4;
        constexpr int ImageArrayCount = 2;
        constexpr int ImageWidth = 1024;
        constexpr int ImageHeight = ImageWidth / ImageColCount;
        constexpr int RedZoneBorderSize = 16;
        constexpr int CellWidth = (ImageWidth / ImageColCount);
        constexpr int CellHeight = CellWidth;

        // Create an array swapchain
        auto swapchainCreateInfo = compositionHelper.DefaultColorSwapchainCreateInfo(
            ImageWidth, ImageHeight, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT, globalData.graphicsPlugin->GetSRGBA8Format());
        swapchainCreateInfo.arraySize = ImageArrayCount;
        swapchainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        const XrSwapchain swapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);

        // Render a grid of numbers (1,2,3,4) in slice 0 and (5,6,7,8) in slice 1 of the swapchain
        // Create a quad layer referencing each number cell.
        compositionHelper.AcquireWaitReleaseImage(swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
            int number = 1;
            for (int arraySlice = 0; arraySlice < ImageArrayCount; arraySlice++) {
                Conformance::RGBAImage numberGridImage(ImageWidth, ImageHeight);

                // All unused areas are red (should not be seen).
                numberGridImage.DrawRect(0, 0, numberGridImage.width, numberGridImage.height, Colors::Red);

                for (int x = 0; x < ImageColCount; x++) {
                    const auto& color = Colors::UniqueColors[number % Colors::UniqueColors.size()];
                    const XrRect2Di numberRect{{x * CellWidth + RedZoneBorderSize, RedZoneBorderSize},
                                               {CellWidth - RedZoneBorderSize * 2, CellHeight - RedZoneBorderSize * 2}};
                    numberGridImage.DrawRect(numberRect.offset.x, numberRect.offset.y, numberRect.extent.width, numberRect.extent.height,
                                             Colors::Transparent);
                    numberGridImage.PutText(numberRect, std::to_string(number).c_str(), CellHeight, color);
                    numberGridImage.DrawRectBorder(numberRect.offset.x, numberRect.offset.y, numberRect.extent.width,
                                                   numberRect.extent.height, 4, color);
                    number++;

                    const float quadX = Math::LinearMap(x, 0, ImageColCount - 1, -2.0f, 2.0f);
                    const float quadY = Math::LinearMap(arraySlice, 0, ImageArrayCount - 1, 0.75f, -0.75f);
                    XrCompositionLayerQuad* const quad =
                        compositionHelper.CreateQuadLayer(swapchain, viewSpace, 1.0f, XrPosef{Quat::Identity, {quadX, quadY, QuadZ}});
                    quad->layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                    quad->subImage.imageArrayIndex = arraySlice;
                    quad->subImage.imageRect = numberRect;
                    quad->size.height = 1.0f;  // Height needs to be corrected since the imageRect is customized.
                    interactiveLayerManager.AddLayer(quad);
                }
                numberGridImage.ConvertToSRGB();
                globalData.graphicsPlugin->CopyRGBAImage(swapchainImage, arraySlice, numberGridImage);
            }
        });

        RenderLoop(session, [&](const XrFrameState& frameState) { return interactiveLayerManager.EndFrame(frameState); }).Loop();
    }

    TEST_CASE("ProjectionArraySwapchain", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Projection Array Swapchain");
        InteractiveLayerManager interactiveLayerManager(
            compositionHelper, "projection_array.png",
            "Uses a single texture array for a projection layer (each view is a different slice and each slice has a unique color).");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, Pose::Identity);

        const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

        // Because a single swapchain is being used for all views (each view is a slice of the texture array), the maximum dimensions must be used
        // since the dimensions of all slices are the same.
        const auto maxWidth = std::max_element(viewProperties.begin(), viewProperties.end(),
                                               [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                                   return l.recommendedImageRectWidth < r.recommendedImageRectWidth;
                                               })
                                  ->recommendedImageRectWidth;
        const auto maxHeight = std::max_element(viewProperties.begin(), viewProperties.end(),
                                                [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                                    return l.recommendedImageRectHeight < r.recommendedImageRectHeight;
                                                })
                                   ->recommendedImageRectHeight;

        // Create swapchain with array type.
        auto swapchainCreateInfo = compositionHelper.DefaultColorSwapchainCreateInfo(maxWidth, maxHeight);
        swapchainCreateInfo.arraySize = (uint32_t)viewProperties.size() * 3;
        const XrSwapchain swapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);

        // Set up the projection layer
        XrCompositionLayerProjection* const projLayer = compositionHelper.CreateProjectionLayer(localSpace);
        for (uint32_t j = 0; j < projLayer->viewCount; j++) {
            // Use non-contiguous array indices to ferret out any assumptions that implementations are making
            // about array indices. In particular 0 != left and 1 != right, but this should test for other
            // assumptions too.
            uint32_t arrayIndex = swapchainCreateInfo.arraySize - (j * 2 + 1);
            const_cast<XrSwapchainSubImage&>(projLayer->views[j].subImage) = compositionHelper.MakeDefaultSubImage(swapchain, arrayIndex);
        }

        const std::vector<Cube> cubes = {Cube::Make({-1, 0, -2}), Cube::Make({1, 0, -2}), Cube::Make({0, -1, -2}), Cube::Make({0, 1, -2})};

        auto updateLayers = [&](const XrFrameState& frameState) {
            auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                // Render into each slice of the array swapchain using the projection layer view fov and pose.
                compositionHelper.AcquireWaitReleaseImage(swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                    for (uint32_t slice = 0; slice < (uint32_t)views.size(); slice++) {
                        GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage, projLayer->views[slice].subImage.imageArrayIndex);

                        const_cast<XrFovf&>(projLayer->views[slice].fov) = views[slice].fov;
                        const_cast<XrPosef&>(projLayer->views[slice].pose) = views[slice].pose;
                        GetGlobalData().graphicsPlugin->RenderView(projLayer->views[slice], swapchainImage, RenderParams().Draw(cubes));
                    }
                });

                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer));
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(session, updateLayers).Loop();
    }

    TEST_CASE("ProjectionWideSwapchain", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Projection Wide Swapchain");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "projection_wide.png",
                                                        "Uses a single wide texture for a projection layer.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, Pose::Identity);

        const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

        const auto totalWidth =
            std::accumulate(viewProperties.begin(), viewProperties.end(), 0,
                            [](uint32_t l, const XrViewConfigurationView& r) { return l + r.recommendedImageRectWidth; });
        // Because a single swapchain is being used for all views the maximum height must be used.
        const auto maxHeight = std::max_element(viewProperties.begin(), viewProperties.end(),
                                                [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                                    return l.recommendedImageRectHeight < r.recommendedImageRectHeight;
                                                })
                                   ->recommendedImageRectHeight;

        // Create wide swapchain.
        const XrSwapchain swapchain =
            compositionHelper.CreateSwapchain(compositionHelper.DefaultColorSwapchainCreateInfo(totalWidth, maxHeight));

        XrCompositionLayerProjection* const projLayer = compositionHelper.CreateProjectionLayer(localSpace);
        int x = 0;
        for (uint32_t j = 0; j < projLayer->viewCount; j++) {
            XrSwapchainSubImage subImage = compositionHelper.MakeDefaultSubImage(swapchain, 0);
            subImage.imageRect.offset = {x, 0};
            subImage.imageRect.extent = {(int32_t)viewProperties[j].recommendedImageRectWidth,
                                         (int32_t)viewProperties[j].recommendedImageRectHeight};
            const_cast<XrSwapchainSubImage&>(projLayer->views[j].subImage) = subImage;
            x += subImage.imageRect.extent.width;  // Each view is to the left of the previous view.
        }

        const std::vector<Cube> cubes = {Cube::Make({-1, 0, -2}), Cube::Make({1, 0, -2}), Cube::Make({0, -1, -2}), Cube::Make({0, 1, -2})};

        auto updateLayers = [&](const XrFrameState& frameState) {
            auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                // Render into each view port of the wide swapchain using the projection layer view fov and pose.
                compositionHelper.AcquireWaitReleaseImage(swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                    GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);
                    for (size_t view = 0; view < views.size(); view++) {
                        const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                        const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                        GetGlobalData().graphicsPlugin->RenderView(projLayer->views[view], swapchainImage, RenderParams().Draw(cubes));
                    }
                });

                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer));
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(session, updateLayers).Loop();
    }

    TEST_CASE("ProjectionSeparateSwapchains", "[composition][interactive]")
    {
        CompositionHelper compositionHelper("Projection Separate Swapchains");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "projection_separate.png",
                                                        "Uses separate textures for each projection layer view.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        SimpleProjectionLayerHelper simpleProjectionLayerHelper(compositionHelper);

        auto updateLayers = [&](const XrFrameState& frameState) {
            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (XrCompositionLayerBaseHeader* projLayer = simpleProjectionLayerHelper.TryGetUpdatedProjectionLayer(frameState)) {
                layers.push_back(projLayer);
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(session, updateLayers).Loop();
    }

    TEST_CASE("QuadHands", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();

        CompositionHelper compositionHelper("Quad Hands");
        XrInstance instance = compositionHelper.GetInstance();
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "quad_hands.png",
                                                        "10x10cm Quads labeled \'L\' and \'R\' should appear 10cm along the grip "
                                                        "positive Z in front of the center of 10cm cubes rendered at the controller "
                                                        "grip poses, or at the origin if that controller isn't being tested."
                                                        "The quads should face you and be upright when the controllers are in "
                                                        "a thumbs-up pointing-into-screen pose. "
                                                        "Check that the quads are properly backface-culled, "
                                                        "that \'R\' is always rendered atop \'L\', "
                                                        "and both are atop the cubes when visible.");

        const std::vector<XrPath> subactionPaths{StringToPath(instance, "/user/hand/left"), StringToPath(instance, "/user/hand/right")};

        XrActionSet actionSet;
        XrAction gripPoseAction;
        {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy(actionSetInfo.actionSetName, "quad_hands");
            strcpy(actionSetInfo.localizedActionSetName, "Quad Hands");
            XRC_CHECK_THROW_XRCMD(xrCreateActionSet(instance, &actionSetInfo, &actionSet));

            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy(actionInfo.actionName, "grip_pose");
            strcpy(actionInfo.localizedActionName, "Grip pose");
            actionInfo.subactionPaths = subactionPaths.data();
            actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
            XRC_CHECK_THROW_XRCMD(xrCreateAction(actionSet, &actionInfo, &gripPoseAction));
        }

        interactionManager.AddActionSet(actionSet);
        XrPath simpleInteractionProfile = StringToPath(instance, "/interaction_profiles/khr/simple_controller");
        interactionManager.AddActionBindings(simpleInteractionProfile,
                                             {{
                                                 {gripPoseAction, StringToPath(instance, "/user/hand/left/input/grip/pose")},
                                                 {gripPoseAction, StringToPath(instance, "/user/hand/right/input/grip/pose")},
                                             }});

        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        SimpleProjectionLayerHelper simpleProjectionLayerHelper(compositionHelper);

        // Spaces attached to the hand (subaction).
        std::vector<XrSpace> gripSpaces;

        // Create XrSpaces for each grip pose
        for (int i = 0; i < 2; i++) {
            XrSpace space;
            if ((i == 0 && globalData.leftHandUnderTest) || (i == 1 && globalData.rightHandUnderTest)) {
                XrActionSpaceCreateInfo spaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
                spaceCreateInfo.action = gripPoseAction;
                spaceCreateInfo.subactionPath = subactionPaths[i];
                spaceCreateInfo.poseInActionSpace = Pose::Identity;
                XRC_CHECK_THROW_XRCMD(xrCreateActionSpace(session, &spaceCreateInfo, &space));
            }
            else {
                XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
                spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
                spaceCreateInfo.poseInReferenceSpace = Pose::Identity;
                XRC_CHECK_THROW_XRCMD(xrCreateReferenceSpace(session, &spaceCreateInfo, &space));
            }
            gripSpaces.push_back(space);
        }

        // Create 10x10cm L and R quads
        XrCompositionLayerQuad* const leftQuadLayer =
            compositionHelper.CreateQuadLayer(compositionHelper.CreateStaticSwapchainImage(CreateTextImage(64, 64, "L", 48)), gripSpaces[0],
                                              0.1f, {Quat::Identity, {0, 0, 0.1f}});

        XrCompositionLayerQuad* const rightQuadLayer =
            compositionHelper.CreateQuadLayer(compositionHelper.CreateStaticSwapchainImage(CreateTextImage(64, 64, "R", 48)), gripSpaces[1],
                                              0.1f, {Quat::Identity, {0, 0, 0.1f}});

        interactiveLayerManager.AddLayer(leftQuadLayer);
        interactiveLayerManager.AddLayer(rightQuadLayer);

        const XrVector3f cubeSize{0.1f, 0.1f, 0.1f};
        auto updateLayers = [&](const XrFrameState& frameState) {
            std::vector<Cube> cubes;
            for (const auto& space : gripSpaces) {
                XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
                if (XR_SUCCEEDED(
                        xrLocateSpace(space, simpleProjectionLayerHelper.GetLocalSpace(), frameState.predictedDisplayTime, &location))) {
                    if ((location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
                        (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
                        cubes.emplace_back(Cube{location.pose, cubeSize});
                    }
                }
            }
            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (XrCompositionLayerBaseHeader* projLayer = simpleProjectionLayerHelper.TryGetUpdatedProjectionLayer(frameState, cubes)) {
                layers.push_back(projLayer);
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(session, updateLayers).Loop();
    }

    TEST_CASE("ProjectionMutableFieldOfView", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test without a graphics plugin");
        }

        CompositionHelper compositionHelper("Projection Mutable Field-of-View");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "projection_mutable.png",
                                                        "Uses mutable field-of-views for each projection layer view.");
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, Pose::Identity);

        if (!compositionHelper.GetViewConfigurationProperties().fovMutable) {
            SKIP("View configuration does not support mutable FoV");
        }

        const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

        const auto totalWidth =
            std::accumulate(viewProperties.begin(), viewProperties.end(), 0,
                            [](uint32_t l, const XrViewConfigurationView& r) { return l + r.recommendedImageRectWidth; });
        // Because a single swapchain is being used for all views the maximum height must be used.
        const auto maxHeight = std::max_element(viewProperties.begin(), viewProperties.end(),
                                                [](const XrViewConfigurationView& l, const XrViewConfigurationView& r) {
                                                    return l.recommendedImageRectHeight < r.recommendedImageRectHeight;
                                                })
                                   ->recommendedImageRectHeight;

        // Create wide swapchain.
        const XrSwapchain swapchain =
            compositionHelper.CreateSwapchain(compositionHelper.DefaultColorSwapchainCreateInfo(totalWidth, maxHeight));

        XrCompositionLayerProjection* const projLayer = compositionHelper.CreateProjectionLayer(localSpace);
        int x = 0;
        for (uint32_t j = 0; j < projLayer->viewCount; j++) {
            XrSwapchainSubImage subImage = compositionHelper.MakeDefaultSubImage(swapchain, 0);
            subImage.imageRect.offset = {x, 0};
            subImage.imageRect.extent = {(int32_t)viewProperties[j].recommendedImageRectWidth,
                                         (int32_t)viewProperties[j].recommendedImageRectHeight};
            const_cast<XrSwapchainSubImage&>(projLayer->views[j].subImage) = subImage;
            x += subImage.imageRect.extent.width;  // Each view is to the left of the previous view.
        }

        const std::vector<Cube> cubes = {Cube::Make({-.2f, -.2f, -2}), Cube::Make({.2f, -.2f, -2}), Cube::Make({0, .1f, -2})};

        const XrVector3f Forward{0, 0, 1};
        const XrQuaternionf roll180 = Quat::FromAxisAngle(Forward, MATH_PI);

        auto updateLayers = [&](const XrFrameState& frameState) {
            auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                // Render into each view port of the wide swapchain using the projection layer view fov and pose.
                compositionHelper.AcquireWaitReleaseImage(swapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                    GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);
                    for (size_t view = 0; view < views.size(); view++) {
                        // Copy over the provided FOV and pose but use 40% of the suggested FOV.
                        const_cast<XrFovf&>(projLayer->views[view].fov) = views[view].fov;
                        const_cast<XrPosef&>(projLayer->views[view].pose) = views[view].pose;
                        const_cast<float&>(projLayer->views[view].fov.angleUp) *= 0.4f;
                        const_cast<float&>(projLayer->views[view].fov.angleDown) *= 0.4f;
                        const_cast<float&>(projLayer->views[view].fov.angleLeft) *= 0.4f;
                        const_cast<float&>(projLayer->views[view].fov.angleRight) *= 0.4f;

                        // Render using a 180 degree roll on Z which effectively creates a flip on both the X and Y axis.
                        XrCompositionLayerProjectionView rolled = projLayer->views[view];
                        rolled.pose.orientation = roll180 * views[view].pose.orientation;
                        GetGlobalData().graphicsPlugin->RenderView(rolled, swapchainImage, RenderParams().Draw(cubes));

                        // After rendering, report a flipped FOV on X and Y without the 180 degree roll, which has the same
                        // effect. This switcheroo is necessary since rendering with flipped FOV will result in an inverted
                        // winding causing normally hidden triangles to be visible and visible triangles to be hidden.
                        const_cast<float&>(projLayer->views[view].fov.angleUp) = -projLayer->views[view].fov.angleUp;
                        const_cast<float&>(projLayer->views[view].fov.angleDown) = -projLayer->views[view].fov.angleDown;
                        const_cast<float&>(projLayer->views[view].fov.angleLeft) = -projLayer->views[view].fov.angleLeft;
                        const_cast<float&>(projLayer->views[view].fov.angleRight) = -projLayer->views[view].fov.angleRight;
                    }
                });

                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer));
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(session, updateLayers).Loop();
    }

    TEST_CASE("StaleSwapchain", "[composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test stale swapchains without a graphics plugin");
        }

        CompositionHelper compositionHelper("Stale swapchain");
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "stale_swapchain.png",
                                                        "Updates swapchain of each square at 1Hz. "
                                                        "Square on left should be constantly green, and square on right "
                                                        "should switch between green and blue every second. "
                                                        "If there is any flicker on the green square, "
                                                        "likely at the same time as the other square changes color, "
                                                        "that is a failure.");
        compositionHelper.GetInteractionManager().AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace viewSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_VIEW, XrPosef{Quat::Identity, {0, 0, -1}});

        constexpr int ImageSize = 1;

        // Create an array swapchain
        auto swapchainCreateInfo =
            compositionHelper.DefaultColorSwapchainCreateInfo(ImageSize, ImageSize, 0, globalData.graphicsPlugin->GetSRGBA8Format());
        swapchainCreateInfo.usageFlags |= XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        const XrSwapchain constantColorSwapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);
        const XrSwapchain alternatingColorSwapchain = compositionHelper.CreateSwapchain(swapchainCreateInfo);

        RGBAImage images[2] = {RGBAImage(ImageSize, ImageSize), RGBAImage(ImageSize, ImageSize)};
        images[0].DrawRect(0, 0, ImageSize, ImageSize, Colors::Green);
        images[1].DrawRect(0, 0, ImageSize, ImageSize, Colors::Blue);
        images[0].ConvertToSRGB();
        images[1].ConvertToSRGB();

        XrCompositionLayerQuad* const constantQuad =
            compositionHelper.CreateQuadLayer(constantColorSwapchain, viewSpace, 0.02f, XrPosef{Quat::Identity, {-0.1f, 0, -1}});
        interactiveLayerManager.AddLayer(constantQuad);

        XrCompositionLayerQuad* const alternatingQuad =
            compositionHelper.CreateQuadLayer(alternatingColorSwapchain, viewSpace, 0.02f, XrPosef{Quat::Identity, {0.1f, 0, -1}});
        interactiveLayerManager.AddLayer(alternatingQuad);

        XrTime lastUpdate = 0;
        bool alternatingIndex = false;
        RenderLoop(compositionHelper.GetSession(), [&](const XrFrameState& frameState) {
            // Failing this test may create a flashing image. 1Hz is well outside the
            // documented normal range for photosensitive epilepsy (rarely as low as 3Hz).
            // Regardless, failures may e.g. create a black flash every second, so we use a
            // small square to minimise any effects of the failure condition.
            if (lastUpdate == 0 || (frameState.predictedDisplayTime - lastUpdate) >= 1_xrSeconds) {
                lastUpdate = frameState.predictedDisplayTime;
                compositionHelper.AcquireWaitReleaseImage(constantColorSwapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                    globalData.graphicsPlugin->CopyRGBAImage(swapchainImage, 0, images[0]);
                });
                compositionHelper.AcquireWaitReleaseImage(alternatingColorSwapchain, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                    globalData.graphicsPlugin->CopyRGBAImage(swapchainImage, 0, images[(uint32_t)alternatingIndex]);
                    alternatingIndex = !alternatingIndex;
                });
            }
            return interactiveLayerManager.EndFrame(frameState);
        }).Loop();
    }

    TEST_CASE("ProjectionDepth", "[XR_KHR_composition_layer_depth][XR_FB_composition_layer_depth_test][composition][interactive]")
    {
        GlobalData& globalData = GetGlobalData();
        if (!globalData.IsUsingGraphicsPlugin()) {
            SKIP("Cannot test without a graphics plugin");
        }

        if (!globalData.IsInstanceExtensionSupported(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME)) {
            SKIP(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME " not supported");
        }
        if (!globalData.IsInstanceExtensionSupported(XR_FB_COMPOSITION_LAYER_DEPTH_TEST_EXTENSION_NAME)) {
            SKIP(XR_FB_COMPOSITION_LAYER_DEPTH_TEST_EXTENSION_NAME " not supported");
        }

        CompositionHelper compositionHelper(
            "Projection Depth", {XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME, XR_FB_COMPOSITION_LAYER_DEPTH_TEST_EXTENSION_NAME});
        InteractiveLayerManager interactiveLayerManager(compositionHelper, "projection_depth.png",
                                                        "Four cubes each are drawn on two different layers, with the front face"
                                                        " appearing darker on the second layer. All eight cubes should be visible,"
                                                        " with the darker blue front face appearing closer on the left and bottom,"
                                                        " and further away on the right and top.");
        XrSession session = compositionHelper.GetSession();
        InteractionManager& interactionManager = compositionHelper.GetInteractionManager();
        interactionManager.AttachActionSets();
        compositionHelper.BeginSession();

        const XrSpace localSpace = compositionHelper.CreateReferenceSpace(XR_REFERENCE_SPACE_TYPE_LOCAL, Pose::Identity);

        const std::vector<XrViewConfigurationView> viewProperties = compositionHelper.EnumerateConfigurationViews();

        std::vector<XrSwapchainCreateInfo> colorSwapchainCreateInfo;
        std::vector<XrSwapchainCreateInfo> depthSwapchainCreateInfo;
        for (auto& view : viewProperties) {
            colorSwapchainCreateInfo.push_back(
                compositionHelper.DefaultColorSwapchainCreateInfo(view.recommendedImageRectWidth, view.recommendedImageRectHeight));
            depthSwapchainCreateInfo.push_back(
                compositionHelper.DefaultDepthSwapchainCreateInfo(view.recommendedImageRectWidth, view.recommendedImageRectHeight));
        }

        const int LayerCount = 2;
        XrCompositionLayerProjection* projLayer[LayerCount];
        XrCompositionLayerDepthTestFB depthTestInfo[LayerCount];
        std::vector<std::pair<XrSwapchain, XrSwapchain>> swapchain[LayerCount];
        std::vector<XrCompositionLayerDepthInfoKHR> depthInfo[LayerCount];

        // Set up the projection layers
        for (int layer = 0; layer < LayerCount; layer++) {
            projLayer[layer] = compositionHelper.CreateProjectionLayer(localSpace);

            // Add depth test info to the chain for each projection layer
            depthTestInfo[layer].type = XR_TYPE_COMPOSITION_LAYER_DEPTH_TEST_FB;
            depthTestInfo[layer].next = projLayer[layer]->next;
            depthTestInfo[layer].depthMask = true;
            depthTestInfo[layer].compareOp = XR_COMPARE_OP_LESS_FB;
            const_cast<const void*&>(projLayer[layer]->next) = &depthTestInfo[layer];

            depthInfo[layer].resize(projLayer[layer]->viewCount);
            for (uint32_t j = 0; j < projLayer[layer]->viewCount; j++) {
                // create color and depth swapchains
                swapchain[layer].push_back(
                    compositionHelper.CreateSwapchainWithDepth(colorSwapchainCreateInfo[j], depthSwapchainCreateInfo[j]));
                const_cast<XrSwapchainSubImage&>(projLayer[layer]->views[j].subImage) =
                    compositionHelper.MakeDefaultSubImage(swapchain[layer][j].first);

                // Add depth info to the chain for each projection layer view
                depthInfo[layer][j].type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR;
                depthInfo[layer][j].next = projLayer[layer]->views[j].next;
                depthInfo[layer][j].minDepth = 0.0f;
                depthInfo[layer][j].maxDepth = 1.0f;
                depthInfo[layer][j].nearZ = 0.05f;
                depthInfo[layer][j].farZ = 100.0f;
                depthInfo[layer][j].subImage = compositionHelper.MakeDefaultSubImage(swapchain[layer][j].second);
                const_cast<const void*&>(projLayer[layer]->views[j].next) = &depthInfo[layer][j];
            }
        }

        // Alternate which cube should be in front. Rotate every cube in the second layer to tell them apart
        const std::vector<Cube> cubes[LayerCount] = {
            {Cube::Make({-1, 0, -2.5}), Cube::Make({1, 0, -2}), Cube::Make({0, -1, -2.5}), Cube::Make({0, 1, -2})},
            {Cube::Make({-1, 0, -2}, 0.25f, {0, 1, 0, 0}), Cube::Make({1, 0, -2.5}, 0.25f, {0, 1, 0, 0}),
             Cube::Make({0, -1, -2}, 0.25f, {1, 0, 0, 0}), Cube::Make({0, 1, -2.5}, 0.25f, {1, 0, 0, 0})}};

        auto updateLayers = [&](const XrFrameState& frameState) {
            auto viewData = compositionHelper.LocateViews(localSpace, frameState.predictedDisplayTime);
            const auto& viewState = std::get<XrViewState>(viewData);

            std::vector<XrCompositionLayerBaseHeader*> layers;
            if (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT &&
                viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) {
                const auto& views = std::get<std::vector<XrView>>(viewData);

                for (int layer = 0; layer < LayerCount; layer++) {
                    for (size_t j = 0; j < views.size(); j++) {
                        // Render into each view's swapchain using the projection layer view fov and pose.
                        compositionHelper.AcquireWaitReleaseImage(
                            swapchain[layer][j].first, [&](const XrSwapchainImageBaseHeader* swapchainImage) {
                                GetGlobalData().graphicsPlugin->ClearImageSlice(swapchainImage);

                                const_cast<XrFovf&>(projLayer[layer]->views[j].fov) = views[j].fov;
                                const_cast<XrPosef&>(projLayer[layer]->views[j].pose) = views[j].pose;
                                GetGlobalData().graphicsPlugin->RenderView(projLayer[layer]->views[j], swapchainImage,
                                                                           RenderParams().Draw(cubes[layer]));
                            });
                    }
                    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(projLayer[layer]));
                }
            }
            return interactiveLayerManager.EndFrame(frameState, layers);
        };

        RenderLoop(session, updateLayers).Loop();
    }

}  // namespace Conformance

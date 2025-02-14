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

#pragma once

#include "conformance_utils.h"
#include "utilities/feature_availability.h"
#include "utilities/stringification.h"
#include "utilities/types_and_constants.h"
#include "utilities/utils.h"
#include "utilities/android_declarations.h"

#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_tostring.hpp>

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

#ifdef XR_USE_PLATFORM_WIN32
#include "windows.h"
#endif

/**
 * @defgroup cts_framework OpenXR CTS framework
 * @brief Functionality to use when building conformance tests.
 */

/**
 * @defgroup cts_assert_macros Assertion helper macros
 * @brief Helper macros for Catch2 assertions.
 * @ingroup cts_framework
 */

/// @{

/// Like normal CHECK() but with an extra message (an INFO that lasts for just this assert)
///
/// If you're checking XR_SUCCEEDED(result), see CHECK_RESULT_SUCCEEDED.
///
/// Example usage:
/// ```
///     CAPTURE(result = xrCreateSession(instance, &session, ...));
///     CHECK_MSG(session != XR_NULL_HANDLE_CPP, "xrCreateSession failed");
/// ```
///
#define CHECK_MSG(expr, msg) \
    {                        \
        INFO(msg);           \
        CHECK(expr);         \
    }  // Need to create scope or else the INFO leaks into other failures.

/// Like normal REQUIRE() but with an extra message (an INFO that lasts for just this assert)
///
/// If you're checking XR_SUCCEEDED(result), see REQUIRE_RESULT_SUCCEEDED.
///
/// Example usage:
/// ```
///     CAPTURE(result = xrCreateSession(instance, &session, ...));
///     REQUIRE_MSG(session != XR_NULL_HANDLE_CPP, "xrCreateSession failed");
/// ```
///
#define REQUIRE_MSG(expr, msg) \
    {                          \
        INFO(msg);             \
        REQUIRE(expr);         \
    }  // Need to create scope or else the INFO leaks into other failures.

/// Expects result to be exactly equal to expectedResult
///
#define REQUIRE_RESULT(result, expectedResult) REQUIRE(result == expectedResult)

/// Expects XR_SUCCEEDED(result) (any kind of success, not necessarily XR_SUCCESS)
///
#define CHECK_RESULT_SUCCEEDED(result) CHECK(result >= 0)

/// Expects XR_SUCCEEDED(result) (any kind of success, not necessarily XR_SUCCESS)
///
#define REQUIRE_RESULT_SUCCEEDED(result) REQUIRE(result >= 0)

/// Expects XR_UNQUALIFIED_SUCCESS(result) (exactly equal to XR_SUCCESS)
///
#define CHECK_RESULT_UNQUALIFIED_SUCCESS(result) CHECK(result == XR_SUCCESS)

/// Expects XR_UNQUALIFIED_SUCCESS(result) (exactly equal to XR_SUCCESS)
///
#define REQUIRE_RESULT_UNQUALIFIED_SUCCESS(result) REQUIRE(result == XR_SUCCESS)

/// @}

#if defined(XR_USE_PLATFORM_ANDROID)
#define ATTACH_THREAD Conformance_Android_Attach_Current_Thread()
#define DETACH_THREAD Conformance_Android_Detach_Current_Thread()
#else
// We put an expression here so that forgetting the semicolon is an error on all platforms.
#define ATTACH_THREAD \
    do {              \
    } while (0)
#define DETACH_THREAD \
    do {              \
    } while (0)
#endif

namespace Conformance
{
    class FeatureSet;
    struct IGraphicsPlugin;
    struct IPlatformPlugin;
    /// Specifies runtime options for the application.
    /// String options are case-insensitive.
    /// Each of these can be specified from the command line via a command of the same name as
    /// the variable name. For example, the application can be run with --graphicsPlugin "vulkan"
    /// String vector options are specified space delimited strings. For example, the app could be
    /// run with --enabledAPILayers "api_validation handle_validation"
    ///
    struct Options
    {
        /// Describes the option set in a way suitable for printing.
        std::string DescribeOptions() const;

        /// Options include: "vulkan" "d3d11" d3d12" "opengl" "opengles"
        /// Default is none. Must be manually specified.
        std::string graphicsPlugin{};

        /// Options include: "1.0" "1.1"
        /// Default is 1.1.
        std::string desiredApiVersion{"1.1"};
        /// Will contain the results of XR_MAKE_VERSION using the requested major and minor version
        /// combined with the patch component of XR_CURRENT_API_VERSION.
        XrVersion desiredApiVersionValue{XR_CURRENT_API_VERSION};

        /// Options include "hmd" "handheld". See enum XrFormFactor.
        /// Default is hmd.
        std::string formFactor{"Hmd"};
        XrFormFactor formFactorValue{XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};

        /// Which hands have been selected for test. This is to allow for devices which only have
        /// one controller, and also to allow skipping one of the controllers during development.
        /// Options are "left", "right", and "both".
        /// Default is "both".
        std::string enabledHands{"both"};
        bool leftHandEnabled{true};
        bool rightHandEnabled{true};

        /// Description of how long to wait before skipping tests which support auto skip
        /// or 0 when auto skip is disabled.
        std::chrono::milliseconds autoSkipTimeout{0};

        /// Options include "stereo" "mono" "foveatedInset" "firstPersonObserver". See enum XrViewConfigurationType.
        /// Default is stereo.
        std::string viewConfiguration{"Stereo"};
        XrViewConfigurationType viewConfigurationValue{XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};

        /// Options include "opaque" "additive" "alphablend". See enum XrEnvironmentBlendMode.
        /// Default is the first enumerated value
        std::string environmentBlendMode{};
        XrEnvironmentBlendMode environmentBlendModeValue{(XrEnvironmentBlendMode)0};

        /// Options can vary depending on their platform availability. If a requested API layer is
        /// not supported then the test fails.
        /// Default is empty.
        std::vector<std::string> enabledAPILayers;

        /// Options include at least any of the documented extensions. The runtime supported extensions
        /// are enumerated by xrEnumerateApiLayerProperties. If a requested extension is not supported
        /// then the test fails.
        /// Default is empty.
        std::vector<std::string> enabledInstanceExtensions;

        /// Options include at least any of the documented interaction profiles.
        /// The conformance tests will generically test the runtime supports each of the provided
        /// interaction profile.
        /// Default is /interaction_profiles/khr/simple_controller alone.
        std::vector<std::string> enabledInteractionProfiles;

        /// Indicates if the runtime should be tested to ensure it returns XR_ERROR_HANDLE_INVALID
        /// upon usage of invalid handles that are not undefined behavior to read.
        /// The OpenXR specification does not require this because it cannot (uninitialized memory
        /// used as a handle may trigger undefined behavior at the C level), but some runtimes will
        /// attempt to identify bad handles where they can.
        /// Default is false.
        bool invalidHandleValidation{false};

        /// Indicates if the runtime should be tested to ensure it returns XR_ERROR_VALIDATION_FAILURE
        /// upon passing structs with invalid .type fields.
        /// The OpenXR specification does not require this check, but some runtimes will.
        /// Default is false.
        bool invalidTypeValidation{false};

        /// Indicates if the runtime supports disconnecting a device, specifically left and right devices.
        /// Some input tests depends on the side-effects of device disconnection to test various features.
        /// If true the runtime does not support disconnectable devices.
        bool nonDisconnectableDevices{false};

        /// If true then all test diagnostics are reported with the file/line that they occurred on.
        /// Default is true (enabled).
        bool fileLineLoggingEnabled{true};

        /// If true then xrGetSystem will be attempted repeatedly for a limited time at the beginning of a run
        /// before beginning a test case.
        bool pollGetSystem{false};

        /// Defines if executing in debug mode. By default this follows the build type.
        bool debugMode
        {
#if defined(NDEBUG)
            false
#else
            true
#endif
        };
    };

    /// Results of the "test_FrameSubmission" timed pipelined submission test, which verifies correct
    /// waiting behavior in the frame loop.
    class TimedSubmissionResults
    {
    public:
        TimedSubmissionResults() = default;
        TimedSubmissionResults(std::chrono::nanoseconds averageWaitTime_, std::chrono::nanoseconds averageAppFrameTime_,
                               std::chrono::nanoseconds averageDisplayPeriod_, std::chrono::nanoseconds averageBeginWaitTime_)
            : valid(true)
            , averageWaitTime(averageWaitTime_)
            , averageAppFrameTime(averageAppFrameTime_)
            , averageDisplayPeriod(averageDisplayPeriod_)
            , averageBeginWaitTime(averageBeginWaitTime_)
        {
        }

        /// Are the values populated?
        bool IsValid() const noexcept
        {
            return valid;
        }

        /// Average xrWaitFrame wait time
        std::chrono::nanoseconds GetAverageWaitTime() const noexcept
        {
            return averageWaitTime;
        }
        /// Average time spent per frame
        std::chrono::nanoseconds GetAverageAppFrameTime() const noexcept
        {
            return averageAppFrameTime;
        }
        /// Average predicted display period
        std::chrono::nanoseconds GetAverageDisplayPeriod() const noexcept
        {
            return averageDisplayPeriod;
        }
        /// Average xrBeginFrame wait time
        std::chrono::nanoseconds GetAverageBeginWaitTime() const noexcept
        {
            return averageBeginWaitTime;
        }

        /// Get the frame overhead: A value of 1 means 100%.
        ///
        /// An overhead of 50% means a 16.66ms display period ran with an average of 25ms per frame.
        /// Since frames should be discrete multiples of the display period 50% implies that half of the frames
        /// took two display periods to complete, 100% implies every frame took two periods.
        double GetOverheadFactor() const noexcept
        {
            return (averageAppFrameTime.count() / (double)averageDisplayPeriod.count()) - 1.0;
        }

    private:
        /// Set to true if these fields are populated.
        bool valid{false};

        /// Average xrWaitFrame wait time
        std::chrono::nanoseconds averageWaitTime;
        /// Average time spent per frame
        std::chrono::nanoseconds averageAppFrameTime;
        /// Average predicted display period
        std::chrono::nanoseconds averageDisplayPeriod;
        /// Average xrBeginFrame wait time
        std::chrono::nanoseconds averageBeginWaitTime;
    };

    /// Records and produces a conformance report.
    /// Conformance isn't a black-and-white result. Conformance is against a given specification version,
    /// against a selected set of extensions, with a subset of graphics systems and image formats.
    /// We want to produce a report of this upon completion of the tests.
    class ConformanceReport
    {
    public:
        /// Generates a report string.
        std::string GetReportString() const;

    public:
        class Score
        {
        public:
            uint64_t testSuccessCount{};
            uint64_t testFailureCount{};
        };
        /// The total successful test case runs across all test cases.
        uint64_t TestSuccessCount() const;

        /// The total failed test case runs across all test cases.
        uint64_t TestFailureCount() const;

        XrVersion apiVersion{XR_CURRENT_API_VERSION};
        std::unordered_map<std::string, Score> results;
        std::vector<std::string> unmatchedTestSpecs;
        Catch::Totals totals{};
        TimedSubmissionResults timedSubmission;
        std::vector<std::pair<int64_t, std::string>> swapchainFormats;
    };

    // A single place where all singleton data hangs off of.
    class GlobalData
    {
    public:
        GlobalData() = default;

        // Non-copyable
        GlobalData(const GlobalData&) = delete;
        GlobalData& operator=(const GlobalData&) = delete;

        /// Sets up global data for usage. Required before use of GlobalData.
        /// Returns false if already Initialized.
        bool Initialize();

        bool IsInitialized() const;

        /// Matches a successful call to Initialize.
        void Shutdown();

        /// Returns the default random number engine.
        RandEngine& GetRandEngine();

        const FunctionInfo& GetFunctionInfo(const char* functionName) const;

        const Options& GetOptions() const;

        const ConformanceReport& GetConformanceReport() const;

        const XrInstanceProperties& GetInstanceProperties() const;

        /// case sensitive check.
        bool IsAPILayerEnabled(const char* layerName) const;

        /// case sensitive check.
        bool IsInstanceExtensionEnabled(const char* extensionName) const;

        /// case sensitive check.
        bool IsInstanceExtensionSupported(const char* extensionName) const;

        /// Returns a copy of the IPlatformPlugin
        std::shared_ptr<IPlatformPlugin> GetPlatformPlugin();

        /// Returns a copy of the IGraphicsPlugin.
        std::shared_ptr<IGraphicsPlugin> GetGraphicsPlugin();

        /// Returns true if under the current test environment we require a graphics plugin. This may
        /// be false, for example, if the XR_MND_headless extension is enabled.
        bool IsGraphicsPluginRequired() const;

        /// Returns true if a graphics plugin was supplied, or if IsGraphicsPluginRequired() is true.
        bool IsUsingGraphicsPlugin() const;

        /// Returns true if using XR_EXT_conformance_automation
        bool IsUsingConformanceAutomation() const;

        /// Record a swapchain format as being supported and tested.
        void PushSwapchainFormat(int64_t format, const std::string& name);

        /// Calculate the clear color to use for the background based on the XrEnvironmentBlendMode in use.
        XrColor4f GetClearColorForBackground() const;

        /// Populate a FeatureSet with the current core version and all *available* extensions.
        void PopulateVersionAndAvailableExtensions(FeatureSet& out) const;

        /// Populate a FeatureSet with the current core version and (default or manually) enabled extensions.
        void PopulateVersionAndEnabledExtensions(FeatureSet& out) const;

    public:
        /// Guards all member data.
        mutable std::recursive_mutex dataMutex;

        /// Indicates if Init has succeeded.
        bool isInitialized{};

        /// The default random number generation engine we use. Thread safe.
        RandEngine randEngine;

        /// User selected options for the program execution.
        Options options;

        ConformanceReport conformanceReport;

        XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};

        FunctionInfo nullFunctionInfo;

        std::shared_ptr<IPlatformPlugin> platformPlugin;

        std::shared_ptr<IGraphicsPlugin> graphicsPlugin;

        /// Specifies invalid values, which aren't XR_NULL_HANDLE. Used to exercise invalid handles.
        XrInstance invalidInstance{XRC_INVALID_INSTANCE_VALUE};
        XrSession invalidSession{XRC_INVALID_SESSION_VALUE};
        XrSpace invalidSpace{XRC_INVALID_SPACE_VALUE};
        XrSwapchain invalidSwapchain{XRC_INVALID_SWAPCHAIN_VALUE};
        XrActionSet invalidActionSet{XRC_INVALID_ACTION_SET_VALUE};
        XrAction invalidAction{XRC_INVALID_ACTION_VALUE};
        XrSystemId invalidSystemId{XRC_INVALID_SYSTEM_ID_VALUE};
        XrPath invalidPath{XRC_INVALID_PATH_VALUE};

        /// The API layers currently available.
        std::vector<XrApiLayerProperties> availableAPILayers;
        std::vector<std::string> availableAPILayerNames;

        /// The API layers that have been requested to be enabled. Suitable for passing to OpenXR.
        StringVec enabledAPILayerNames;

        /// The instance extensions currently available.
        std::vector<XrExtensionProperties> availableInstanceExtensions;
        std::vector<std::string> availableInstanceExtensionNames;

        /// The instance extensions that are required by the platform (IPlatformPlugin).
        std::vector<std::string> requiredPlatformInstanceExtensions;

        /// The instance extensions that are required by the graphics system (IGraphicsPlugin).
        std::vector<std::string> requiredGraphicsInstanceExtensions;

        /// The instance extensions that have been requested to be enabled. Suitable for passing to OpenXR.
        StringVec enabledInstanceExtensionNames;

        /// The interaction profiles that have been requested to be tested.
        StringVec enabledInteractionProfiles;

        /// The environment blend modes available for the view configuration type.
        std::vector<XrEnvironmentBlendMode> availableBlendModes;

        /// Whether each controller is to be used during testing
        bool leftHandUnderTest{false};
        bool rightHandUnderTest{false};

        /// Required instance creation extension struct, or nullptr.
        /// This is a pointer into IPlatformPlugin-provided memory.
        XrBaseInStructure* requiredPlatformInstanceCreateStruct{};
    };

    /// Returns the default singleton global data.
    GlobalData& GetGlobalData();

    /// Reset global data for a subsequent test run.
    void ResetGlobalData();

}  // namespace Conformance

/// Returns a pointer to an extension function retrieved via xrGetInstanceProcAddr.
///
/// Example usage:
/// ```
///     XrInstance instance; // ... a valid instance
///     auto _xrPollEvent = GetInstanceExtensionFunction<PFN_xrPollEvent>(instance, "xrPollEvent");
///     CHECK(_xrPollEvent != nullptr);
/// ```
///
template <typename FunctionType, bool requireSuccess = true>
FunctionType GetInstanceExtensionFunction(XrInstance instance, const char* functionName)
{
    using namespace Conformance;
    if (instance == XR_NULL_HANDLE) {
        throw std::logic_error("Cannot pass a null instance to GetInstanceExtensionFunction");
    }
    if (functionName == nullptr) {
        throw std::logic_error("Cannot pass a null function name to GetInstanceExtensionFunction");
    }
    FunctionType f = nullptr;
    XrResult result = xrGetInstanceProcAddr(instance, functionName, (PFN_xrVoidFunction*)&f);
    if (requireSuccess) {
        if (result != XR_SUCCESS) {
            throw std::runtime_error(std::string("Failed trying to get function ") + functionName + ": " + ResultToString(result));
        }
    }

    if (XR_SUCCEEDED(result)) {
        if (f == nullptr) {
            throw std::runtime_error(std::string("xrGetInstanceProcAddr claimed to succeed, but returned null trying to get function ") +
                                     functionName);
        }
    }

    return f;
}

// ValidateInstanceExtensionFunctionNotSupported
//
// Validates that no pointer to an extension function can be retrieved via xrGetInstanceProcAddr.
//
// Example usage:
//     XrInstance instance; // ... a valid instance
//     ValidateInstanceExtensionFunctionNotSupported(instance, "xrFoo");
//
inline void ValidateInstanceExtensionFunctionNotSupported(XrInstance instance, const char* functionName)
{
    using namespace Conformance;
    if (instance == XR_NULL_HANDLE) {
        throw std::logic_error("Cannot pass a null instance to ValidateInstanceExtensionFunctionNotSupported");
    }
    if (functionName == nullptr) {
        throw std::logic_error("Cannot pass a null function name to ValidateInstanceExtensionFunctionNotSupported");
    }
    PFN_xrVoidFunction f = nullptr;
    XrResult result = xrGetInstanceProcAddr(instance, functionName, &f);

    if (result != XR_ERROR_FUNCTION_UNSUPPORTED) {
        throw std::runtime_error(std::string("Failed when expecting XR_ERROR_FUNCTION_UNSUPPORTED trying to get function ") + functionName +
                                 ": " + ResultToString(result));
    }

    if (f != nullptr) {
        throw std::runtime_error(std::string("xrGetInstanceProcAddr claimed to fail, but returned non-null trying to get function ") +
                                 functionName);
    }
}

/// Returns a pointer to an extension function retrieved via xrGetInstanceProcAddr, or nullptr in case of error.
///
/// Like @ref GetInstanceExtensionFunction but does not throw, so safe to use in destructors.
///
template <typename FunctionType>
FunctionType GetInstanceExtensionFunctionNoexcept(XrInstance instance, const char* functionName) noexcept
{
    using namespace Conformance;
    if (instance == XR_NULL_HANDLE_CPP) {
        return nullptr;
    }
    if (functionName == nullptr) {
        return nullptr;
    }
    FunctionType f;
    XrResult result = xrGetInstanceProcAddr(instance, functionName, (PFN_xrVoidFunction*)&f);
    if (result != XR_SUCCESS) {
        return nullptr;
    }
    return f;
}

/**
 * @defgroup cts_optional_tests Optional Assertion Helpers
 * @brief Macros for dealing with classes of optional tests
 * @ingroup cts_framework
 */
/// @{

/// Start a scope that checks for handle validation.
/// This is not required by the spec, but some runtimes do it as it is permitted.
#define OPTIONAL_INVALID_HANDLE_VALIDATION_INFO            \
    if (GetGlobalData().options.invalidHandleValidation) { \
        INFO("Invalid handle validation (optional)");      \
    }                                                      \
    if (GetGlobalData().options.invalidHandleValidation)

/// Start a Catch2 SECTION that checks for handle validation.
/// This is not required by the spec, but some runtimes do it as it is permitted.
#define OPTIONAL_INVALID_HANDLE_VALIDATION_SECTION       \
    if (GetGlobalData().options.invalidHandleValidation) \
    SECTION("Invalid handle validation (optional)")

/// Start a Catch2 SECTION that checks for type validation.
/// This is not required by the spec, but some runtimes do it as it is permitted.
#define OPTIONAL_INVALID_TYPE_VALIDATION_SECTION       \
    if (GetGlobalData().options.invalidTypeValidation) \
    SECTION("Invalid type validation (optional)")

/// Start a scope that will require the user to disconnect a device.
/// Not all devices can do this.
#define OPTIONAL_DISCONNECTABLE_DEVICE_INFO                  \
    if (!GetGlobalData().options.nonDisconnectableDevices) { \
        INFO("Disconnectable device (optional)");            \
    }                                                        \
    if (!GetGlobalData().options.nonDisconnectableDevices)

/// Start a Catch2 SECTION that will require the user to disconnect a device.
/// Not all devices can do this.
#define OPTIONAL_DISCONNECTABLE_DEVICE_SECTION             \
    if (!GetGlobalData().options.nonDisconnectableDevices) \
    SECTION("Disconnectable device (optional)")

/// @}

// Stringification for Catch2.
// See https://github.com/catchorg/Catch2/blob/devel/docs/tostring.md
#define ENUM_CASE_STR(name, val) \
    case name:                   \
        return #name;

#define MAKE_ENUM_TO_STRING_FUNC(enumType)                    \
    inline const char* enum_to_string(enumType e)             \
    {                                                         \
        switch (e) {                                          \
            XR_LIST_ENUM_##enumType(ENUM_CASE_STR);           \
        default:                                              \
            return "Unknown " #enumType;                      \
        }                                                     \
    }                                                         \
    namespace Catch                                           \
    {                                                         \
        template <>                                           \
        struct StringMaker<enumType>                          \
        {                                                     \
            static std::string convert(enumType const& value) \
            {                                                 \
                return enum_to_string(value);                 \
            }                                                 \
        };                                                    \
    }  // namespace Catch

MAKE_ENUM_TO_STRING_FUNC(XrResult);
MAKE_ENUM_TO_STRING_FUNC(XrSessionState);
MAKE_ENUM_TO_STRING_FUNC(XrViewConfigurationType);
MAKE_ENUM_TO_STRING_FUNC(XrVisibilityMaskTypeKHR);
MAKE_ENUM_TO_STRING_FUNC(XrFormFactor);
MAKE_ENUM_TO_STRING_FUNC(XrEnvironmentBlendMode);
MAKE_ENUM_TO_STRING_FUNC(XrActionType);

namespace Catch
{
    template <>
    struct StringMaker<XrPosef>
    {
        static std::string convert(XrPosef const& value);
    };
    template <>
    struct StringMaker<XrQuaternionf>
    {
        static std::string convert(XrQuaternionf const& value);
    };
    template <>
    struct StringMaker<XrVector3f>
    {
        static std::string convert(XrVector3f const& value);
    };

    template <>
    struct StringMaker<XrUuidEXT>
    {
        static std::string convert(XrUuidEXT const& value);
    };
}  // namespace Catch

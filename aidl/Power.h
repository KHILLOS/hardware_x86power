/*
 * Copyright (C) 2026 KhillOS
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <aidl/android/hardware/power/BnPower.h>
#include <hardware/power.h>

#include <mutex>

namespace khillos::power {

class Power : public aidl::android::hardware::power::BnPower {
  public:
    explicit Power(power_module_t* module) : module_(module) {}

    ndk::ScopedAStatus setMode(aidl::android::hardware::power::Mode type,
                               bool enabled) override;
    ndk::ScopedAStatus isModeSupported(aidl::android::hardware::power::Mode type,
                                       bool* _aidl_return) override;
    ndk::ScopedAStatus setBoost(aidl::android::hardware::power::Boost type,
                                int32_t durationMs) override;
    ndk::ScopedAStatus isBoostSupported(aidl::android::hardware::power::Boost type,
                                        bool* _aidl_return) override;
    ndk::ScopedAStatus createHintSession(
            int32_t tgid, int32_t uid, const std::vector<int32_t>& threadIds,
            int64_t durationNanos,
            std::shared_ptr<aidl::android::hardware::power::IPowerHintSession>* _aidl_return) override;
    ndk::ScopedAStatus createHintSessionWithConfig(
            int32_t tgid, int32_t uid, const std::vector<int32_t>& threadIds,
            int64_t durationNanos, aidl::android::hardware::power::SessionTag tag,
            aidl::android::hardware::power::SessionConfig* config,
            std::shared_ptr<aidl::android::hardware::power::IPowerHintSession>* _aidl_return) override;
    ndk::ScopedAStatus getHintSessionPreferredRate(int64_t* outNanoseconds) override;
    ndk::ScopedAStatus getSessionChannel(
            int32_t tgid, int32_t uid,
            aidl::android::hardware::power::ChannelConfig* _aidl_return) override;
    ndk::ScopedAStatus closeSessionChannel(int32_t tgid, int32_t uid) override;
    ndk::ScopedAStatus getSupportInfo(
            aidl::android::hardware::power::SupportInfo* _aidl_return) override;
    ndk::ScopedAStatus getCpuHeadroom(
            const aidl::android::hardware::power::CpuHeadroomParams& params,
            aidl::android::hardware::power::CpuHeadroomResult* _aidl_return) override;
    ndk::ScopedAStatus getGpuHeadroom(
            const aidl::android::hardware::power::GpuHeadroomParams& params,
            aidl::android::hardware::power::GpuHeadroomResult* _aidl_return) override;
    ndk::ScopedAStatus sendCompositionData(
            const std::vector<aidl::android::hardware::power::CompositionData>& data) override;
    ndk::ScopedAStatus sendCompositionUpdate(
            const aidl::android::hardware::power::CompositionUpdate& update) override;

  private:
    power_module_t* module_;
    std::mutex moduleMutex_;
};

}  // namespace khillos::power

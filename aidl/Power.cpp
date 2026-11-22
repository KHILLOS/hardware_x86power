/*
 * Copyright (C) 2026 KhillOS
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "Power.h"

namespace khillos::power {

using aidl::android::hardware::power::Boost;
using aidl::android::hardware::power::ChannelConfig;
using aidl::android::hardware::power::CompositionData;
using aidl::android::hardware::power::CompositionUpdate;
using aidl::android::hardware::power::CpuHeadroomParams;
using aidl::android::hardware::power::CpuHeadroomResult;
using aidl::android::hardware::power::GpuHeadroomParams;
using aidl::android::hardware::power::GpuHeadroomResult;
using aidl::android::hardware::power::IPowerHintSession;
using aidl::android::hardware::power::Mode;
using aidl::android::hardware::power::SessionConfig;
using aidl::android::hardware::power::SessionTag;
using aidl::android::hardware::power::SupportInfo;
using ndk::ScopedAStatus;

ScopedAStatus Power::setMode(Mode type, bool enabled) {
    if (type == Mode::INTERACTIVE) {
        std::lock_guard<std::mutex> lock(moduleMutex_);
        if (module_->setInteractive != nullptr) {
            module_->setInteractive(module_, enabled ? 1 : 0);
        }
    }
    // AIDL explicitly permits a platform to ignore other mode hints.
    return ScopedAStatus::ok();
}

ScopedAStatus Power::isModeSupported(Mode type, bool* _aidl_return) {
    *_aidl_return = type == Mode::INTERACTIVE || type == Mode::FIXED_PERFORMANCE;
    return ScopedAStatus::ok();
}

ScopedAStatus Power::setBoost(Boost, int32_t) {
    // Boost hints are unsupported and intentionally ignored. Do not cast AIDL
    // enum values to the unrelated legacy power_hint_t namespace.
    return ScopedAStatus::ok();
}

ScopedAStatus Power::isBoostSupported(Boost, bool* _aidl_return) {
    // The reference power_hint callback is intentionally a no-op for every hint.
    *_aidl_return = false;
    return ScopedAStatus::ok();
}

ScopedAStatus Power::createHintSession(int32_t, int32_t, const std::vector<int32_t>&, int64_t,
                                       std::shared_ptr<IPowerHintSession>* _aidl_return) {
    *_aidl_return = nullptr;
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ScopedAStatus Power::createHintSessionWithConfig(
        int32_t, int32_t, const std::vector<int32_t>&, int64_t, SessionTag, SessionConfig*,
        std::shared_ptr<IPowerHintSession>* _aidl_return) {
    *_aidl_return = nullptr;
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ScopedAStatus Power::getHintSessionPreferredRate(int64_t*) {
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ScopedAStatus Power::getSessionChannel(int32_t, int32_t, ChannelConfig*) {
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ScopedAStatus Power::closeSessionChannel(int32_t, int32_t) {
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ScopedAStatus Power::getSupportInfo(SupportInfo* _aidl_return) {
    SupportInfo supportInfo = {};
    supportInfo.modes = (1LL << static_cast<int32_t>(Mode::INTERACTIVE)) |
                        (1LL << static_cast<int32_t>(Mode::FIXED_PERFORMANCE));
    supportInfo.compositionData.maxBatchSize = 1;
    *_aidl_return = supportInfo;
    return ScopedAStatus::ok();
}

ScopedAStatus Power::getCpuHeadroom(const CpuHeadroomParams&, CpuHeadroomResult*) {
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ScopedAStatus Power::getGpuHeadroom(const GpuHeadroomParams&, GpuHeadroomResult*) {
    return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ScopedAStatus Power::sendCompositionData(const std::vector<CompositionData>&) {
    // Composition reporting is unsupported and intentionally ignored.
    return ScopedAStatus::ok();
}

ScopedAStatus Power::sendCompositionUpdate(const CompositionUpdate&) {
    // Composition reporting is unsupported and intentionally ignored.
    return ScopedAStatus::ok();
}

}  // namespace khillos::power

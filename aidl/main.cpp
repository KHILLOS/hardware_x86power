/*
 * Copyright (C) 2026 KhillOS
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#define LOG_TAG "x86power-service"

#include "Power.h"

#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <hardware/hardware.h>
#include <log/log.h>

#include <memory>
#include <string>

using khillos::power::Power;

int main() {
    const hw_module_t* hwModule = nullptr;
    int error = hw_get_module(POWER_HARDWARE_MODULE_ID, &hwModule);
    if (error != 0 || hwModule == nullptr) {
        ALOGE("hw_get_module(%s) failed: %d", POWER_HARDWARE_MODULE_ID, error);
        return EXIT_FAILURE;
    }

    if (hwModule->methods != nullptr && hwModule->methods->open != nullptr) {
        // power.x86 exposes power_module_t directly. A different legacy
        // power_device_t layout cannot be accepted through this interface.
        ALOGE("Legacy power devices are not supported by x86power-service");
        return EXIT_FAILURE;
    }
    power_module_t* module =
            reinterpret_cast<power_module_t*>(const_cast<hw_module_t*>(hwModule));
    if (module == nullptr) {
        ALOGE("The legacy power module is null");
        return EXIT_FAILURE;
    }
    if (module->init != nullptr) {
        module->init(module);
    }

    ABinderProcess_setThreadPoolMaxThreadCount(0);
    std::shared_ptr<Power> service = ndk::SharedRefBase::make<Power>(module);
    const std::string instance = std::string(Power::descriptor) + "/default";
    binder_status_t status = AServiceManager_addService(service->asBinder().get(), instance.c_str());
    if (status != STATUS_OK) {
        ALOGE("Failed to register %s: %d", instance.c_str(), status);
        return EXIT_FAILURE;
    }

    ALOGI("Registered %s with legacy power.x86", instance.c_str());
    ABinderProcess_joinThreadPool();
    ALOGE("Binder thread pool exited unexpectedly");
    return EXIT_FAILURE;
}

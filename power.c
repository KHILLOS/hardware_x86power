/*
 *
 * Copyright (C) 2016 The Android-x86 Open Source Project
 *
 * Licensed under the GNU General Public License Version 2 or later.
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.gnu.org/licenses/gpl.html
 *
 *
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define LOG_TAG "PowerHAL"
#include <log/log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>
#include <cutils/properties.h>

#define SYS_CPU "/sys/devices/system/cpu"
#define CPU_ONLINE "1"
#define CPU_OFFLINE "0"

#define UNUSED __attribute__((__unused__))

static void set_nonboot_cpu_state(const char *state)
{
    char p[PATH_MAX];
    int cpu_n;

    for (cpu_n = 1; ; cpu_n++) {
        int path_len = snprintf(p, sizeof(p), SYS_CPU "/cpu%d/online", cpu_n);
        if (path_len < 0 || (size_t)path_len >= sizeof(p)) {
            ALOGE("Invalid path for CPU%d", cpu_n);
            return;
        }

        int cpu_n_fd = open(p, O_RDWR);
        if (cpu_n_fd < 0) {
            ALOGV("Stop CPU setup at CPU%d: %s", cpu_n, strerror(errno));
            return;
        }

        ALOGV("Set CPU%d_online state %s ", cpu_n, state);
        ssize_t written = write(cpu_n_fd, state, 1);
        if (written < 0) {
            ALOGW("Set CPU%d_online state %s failed: %s", cpu_n, state, strerror(errno));
        } else if (written != 1) {
            ALOGW("Set CPU%d_online state %s wrote %zd bytes", cpu_n, state, written);
        }
        if (close(cpu_n_fd) != 0) {
            ALOGW("Close CPU%d_online failed: %s", cpu_n, strerror(errno));
        }
    }
}

static void power_init(struct power_module *module UNUSED)
{
}

static void power_set_interactive(struct power_module *module UNUSED, int on)
{
    if (property_get_bool("power.nonboot-cpu-off", 1)) {
        set_nonboot_cpu_state(on ? CPU_ONLINE : CPU_OFFLINE);
    }
}

static void power_hint(struct power_module *module UNUSED, power_hint_t hint, void *data UNUSED)
{
    switch (hint) {
    default:
        break;
    }
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct power_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = POWER_MODULE_API_VERSION_0_2,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = POWER_HARDWARE_MODULE_ID,
        .name = "x86 Power HAL",
        .author = "The Android-x86 Project",
        .methods = &power_module_methods,
    },

    .init = power_init,
    .setInteractive = power_set_interactive,
    .powerHint = power_hint,
};

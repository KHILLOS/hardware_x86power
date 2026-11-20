/*
 * X86 power HAL supervised runtime smoke test.
 *
 * Copyright (C) 2026 KhillOS
 * Licensed under the GNU General Public License Version 2 or later.
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/system_properties.h>
#include <sys/wait.h>

#define LOG_TAG "X86PowerSmoke"
#include <log/log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>
#include <cutils/properties.h>

#define CHILD_TIMEOUT_SECONDS 60

struct cpu_states {
    int count;
    int capacity;
    char *values;
};

static struct cpu_states initial_states;
static char original_power_property[PROPERTY_VALUE_MAX];

static void free_states(struct cpu_states *states)
{
    free(states->values);
    states->values = NULL;
    states->count = 0;
    states->capacity = 0;
}

static int read_cpu_states(struct cpu_states *states)
{
    states->count = 0;
    states->capacity = 0;
    states->values = NULL;

    for (int cpu_n = 1; ; cpu_n++) {
        char path[256];
        int path_len = snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/online", cpu_n);
        if (path_len < 0 || (size_t)path_len >= sizeof(path)) {
            free_states(states);
            return -1;
        }

        int fd;
        do {
            fd = open(path, O_RDONLY);
        } while (fd < 0 && errno == EINTR);
        if (fd < 0) {
            if (errno == ENOENT) {
                break;
            }
            ALOGE("Unable to inspect %s: %s", path, strerror(errno));
            free_states(states);
            return -1;
        }

        if (states->count == states->capacity) {
            int new_capacity = states->capacity == 0 ? 16 : states->capacity * 2;
            if (new_capacity <= states->capacity) {
                close(fd);
                free_states(states);
                return -1;
            }
            char *new_values = realloc(states->values, (size_t)new_capacity);
            if (new_values == NULL) {
                close(fd);
                free_states(states);
                return -1;
            }
            states->values = new_values;
            states->capacity = new_capacity;
        }

        char value;
        ssize_t count;
        do {
            count = read(fd, &value, 1);
        } while (count < 0 && errno == EINTR);
        int close_result = close(fd);
        if (count != 1 || close_result != 0) {
            ALOGE("Failed to read %s", path);
            free_states(states);
            return -1;
        }
        if (value != '0' && value != '1') {
            ALOGE("Unexpected state %c at %s", value, path);
            free_states(states);
            return -1;
        }
        states->values[states->count++] = value;
    }
    return 0;
}

static void print_snapshot(const char *label, const struct cpu_states *states)
{
    printf("%s count=%d", label, states->count);
    for (int i = 0; i < states->count; i++) {
        printf(" cpu%d=%c", i + 1, states->values[i]);
    }
    printf("\n");
}

static int check_current(const char *label, char expected, bool unchanged)
{
    struct cpu_states current;
    int result = 0;

    if (read_cpu_states(&current) != 0) {
        return -1;
    }
    print_snapshot(label, &current);
    if (current.count != initial_states.count) {
        ALOGE("%s: CPU count changed from %d to %d", label, initial_states.count, current.count);
        result = -1;
    } else {
        for (int i = 0; i < current.count; i++) {
            char wanted = unchanged ? initial_states.values[i] : expected;
            if (current.values[i] != wanted) {
                ALOGE("%s: cpu%d=%c, expected %c", label, i + 1, current.values[i], wanted);
                result = -1;
            }
        }
    }
    if (result == 0) {
        printf("%s PASS\n", label);
    }
    free_states(&current);
    return result;
}

static int restore_cpu_states(void)
{
    int result = 0;

    for (int i = 0; i < initial_states.count; i++) {
        char path[256];
        int path_len = snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/online", i + 1);
        if (path_len < 0 || (size_t)path_len >= sizeof(path)) {
            result = -1;
            continue;
        }

        int fd = open(path, O_RDWR);
        if (fd < 0) {
            ALOGE("Restore open %s failed: %s", path, strerror(errno));
            result = -1;
            continue;
        }
        if (write(fd, &initial_states.values[i], 1) != 1) {
            ALOGE("Restore write %s failed: %s", path, strerror(errno));
            result = -1;
        }
        if (close(fd) != 0) {
            ALOGE("Restore close %s failed: %s", path, strerror(errno));
            result = -1;
        }
    }

    if (check_current("restored CPU states", 0, true) != 0) {
        result = -1;
    }
    return result;
}

static int run_child_tests(void)
{
    struct power_module *module = NULL;
    int failures = 0;

    if (hw_get_module(POWER_HARDWARE_MODULE_ID, (const struct hw_module_t **)&module) != 0 ||
        module == NULL || module->setInteractive == NULL) {
        ALOGE("power.x86 discovery or setInteractive failed");
        return 1;
    }
    if (module->init != NULL) {
        module->init(module);
    }

    if (property_set("power.nonboot-cpu-off", "0") != 0) {
        ALOGE("Unable to disable nonboot CPU policy");
        failures = 1;
    } else {
        module->setInteractive(module, 0);
        if (check_current("disabled noninteractive", 0, true) != 0) {
            failures = 1;
        } else {
            module->setInteractive(module, 1);
            if (check_current("disabled interactive", 0, true) != 0) {
                failures = 1;
            }
        }
    }

    if (failures == 0) {
        if (property_set("power.nonboot-cpu-off", "1") != 0) {
            ALOGE("Unable to enable nonboot CPU policy");
            failures = 1;
        } else {
            module->setInteractive(module, 0);
            if (check_current("enabled noninteractive", '0', false) != 0) {
                failures = 1;
            } else {
                module->setInteractive(module, 1);
                if (check_current("enabled interactive", '1', false) != 0) {
                    failures = 1;
                }
            }
        }
    }

    if (failures == 0) {
        if (property_set("power.nonboot-cpu-off", "invalid") != 0) {
            ALOGE("Unable to set unknown property value");
            failures = 1;
        } else {
            module->setInteractive(module, 0);
            if (check_current("unknown-value noninteractive", '0', false) != 0) {
                failures = 1;
            } else {
                module->setInteractive(module, 1);
                if (check_current("unknown-value interactive", '1', false) != 0) {
                    failures = 1;
                }
            }
        }
    }

    printf("CHILD_TEST_RESULT %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures;
}

static int terminate_child(pid_t child)
{
    if (kill(child, SIGKILL) != 0 && errno != ESRCH) {
        ALOGE("Unable to kill child: %s", strerror(errno));
    }

    int status;
    for (;;) {
        pid_t result = waitpid(child, &status, 0);
        if (result == child || (result < 0 && errno == ECHILD)) {
            break;
        }
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result < 0) {
            ALOGE("waitpid during forced cleanup failed: %s", strerror(errno));
            break;
        }
    }
    return -1;
}

static int supervise_child(pid_t child)
{
    struct timespec started;
    if (clock_gettime(CLOCK_MONOTONIC, &started) != 0) {
        ALOGE("CLOCK_MONOTONIC failed: %s", strerror(errno));
        return terminate_child(child);
    }

    for (;;) {
        int status;
        pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child) {
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status) == 0 ? 0 : -1;
            }
            if (WIFSIGNALED(status)) {
                ALOGE("Child terminated by signal %d", WTERMSIG(status));
            }
            return -1;
        }
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            ALOGE("waitpid failed: %s", strerror(errno));
            return terminate_child(child);
        }

        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            ALOGE("CLOCK_MONOTONIC failed: %s", strerror(errno));
            return terminate_child(child);
        }
        time_t elapsed_seconds = now.tv_sec - started.tv_sec;
        if (now.tv_nsec < started.tv_nsec) {
            elapsed_seconds--;
        }
        if (elapsed_seconds >= CHILD_TIMEOUT_SECONDS) {
            ALOGE("Child timed out; killing it before restoration");
            return terminate_child(child);
        }

        struct timespec delay = {.tv_sec = 0, .tv_nsec = 100000000};
        nanosleep(&delay, NULL);
    }
}

int main(void)
{
    char hal_property[PROPERTY_VALUE_MAX];
    int failures = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    property_get("ro.hardware.power", hal_property, "");
    printf("ro.hardware.power=%s\n", hal_property);
    if (strcmp(hal_property, "x86") != 0) {
        ALOGE("Configure ro.hardware.power=x86 before boot; this test never changes ro.* properties");
        failures = 1;
    }

    const prop_info *initial_property = __system_property_find("power.nonboot-cpu-off");
    if (initial_property == NULL) {
        ALOGE("Configure power.nonboot-cpu-off before boot; absence cannot be restored by property_set");
        failures = 1;
    } else {
        property_get("power.nonboot-cpu-off", original_power_property, "");
        printf("initial power.nonboot-cpu-off=%s\n", original_power_property);
    }

    if (read_cpu_states(&initial_states) != 0 || initial_states.count == 0) {
        ALOGE("A multi-CPU guest with readable hotplug nodes is required");
        failures = 1;
    } else {
        print_snapshot("initial CPU states", &initial_states);
    }

    if (failures == 0) {
        pid_t child = fork();
        if (child < 0) {
            ALOGE("fork failed: %s", strerror(errno));
            failures = 1;
        } else if (child == 0) {
            int result = run_child_tests();
            free_states(&initial_states);
            _exit(result);
        } else {
            if (supervise_child(child) != 0) {
                failures = 1;
            }
        }
    }

    if (initial_states.count > 0 && restore_cpu_states() != 0) {
        ALOGE("CPU restoration failed");
        failures = 1;
    }

    if (initial_property != NULL) {
        if (property_set("power.nonboot-cpu-off", original_power_property) != 0) {
            ALOGE("Unable to restore power.nonboot-cpu-off");
            failures = 1;
        }
        char restored_property[PROPERTY_VALUE_MAX];
        const prop_info *current_property = __system_property_find("power.nonboot-cpu-off");
        property_get("power.nonboot-cpu-off", restored_property, "");
        if (current_property == NULL || strcmp(restored_property, original_power_property) != 0) {
            ALOGE("Property restoration failed: value=%s expected=%s",
                  restored_property, original_power_property);
            failures = 1;
        }
    }

    free_states(&initial_states);
    printf("X86POWER_SMOKE_RESULT %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}

/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __FDEVENT_H
#define __FDEVENT_H

#include <stddef.h>
#include <stdint.h>

#include <chrono>
#include <functional>
#include <optional>
#include <variant>

#include "adb_unique_fd.h"

// Events that may be observed
#define FDE_READ 0x0001
#define FDE_WRITE 0x0002
#define FDE_ERROR 0x0004
#define FDE_TIMEOUT 0x0008

// Internal states.
#define FDE_EVENTMASK  0x00ff
#define FDE_STATEMASK  0xff00

#define FDE_ACTIVE     0x0100
#define FDE_PENDING    0x0200
#define FDE_CREATED    0x0400

typedef void (*fd_func)(int fd, unsigned events, void *userdata);
typedef void (*fd_func2)(struct fdevent* fde, unsigned events, void* userdata);

struct fdevent;
std::string dump_fde(const fdevent* fde);

struct fdevent_context {
    virtual ~fdevent_context() = default;

    // Allocate and initialize a new fdevent object.
    virtual fdevent* Create(unique_fd fd, std::variant<fd_func, fd_func2> func, void* arg) = 0;

    // Deallocate an fdevent object, returning the file descriptor that was owned by it.
    virtual unique_fd Destroy(fdevent* fde) = 0;

    // Change which events should cause notifications.
    virtual void Set(fdevent* fde, unsigned events) = 0;
    virtual void Add(fdevent* fde, unsigned events) = 0;
    virtual void Del(fdevent* fde, unsigned events) = 0;

    // Set a timeout on an fdevent.
    // If no events are triggered by the timeout, an FDE_TIMEOUT will be generated.
    // Note timeouts are not defused automatically; if a timeout is set on an fdevent, it will
    // trigger repeatedly every |timeout| ms.
    virtual void SetTimeout(fdevent* fde, std::optional<std::chrono::milliseconds> timeout) = 0;

    // Loop forever, handling events.
    virtual void Loop() = 0;

    // Assert that the caller is running on the context's main thread.
    virtual void CheckMainThread() = 0;

    // Queue an operation to be run on the main thread.
    virtual void Run(std::function<void()> fn) = 0;

    // Test-only functionality:
    virtual void TerminateLoop() = 0;
    virtual size_t InstalledCount() = 0;
};

struct fdevent {
    uint64_t id;

    unique_fd fd;
    int force_eof = 0;

    uint16_t state = 0;
    uint16_t events = 0;
    std::optional<std::chrono::milliseconds> timeout;
    std::chrono::steady_clock::time_point last_active;

    std::variant<fd_func, fd_func2> func;
    void* arg = nullptr;
};

// Backwards compatibility shims that forward to the global fdevent_context.
fdevent* fdevent_create(int fd, fd_func func, void* arg);
fdevent* fdevent_create(int fd, fd_func2 func, void* arg);

unique_fd fdevent_release(fdevent* fde);
void fdevent_destroy(fdevent* fde);

void fdevent_set(fdevent *fde, unsigned events);
void fdevent_add(fdevent *fde, unsigned events);
void fdevent_del(fdevent *fde, unsigned events);
void fdevent_set_timeout(fdevent* fde, std::optional<std::chrono::milliseconds> timeout);
void fdevent_loop();
void check_main_thread();

// Queue an operation to run on the main thread.
void fdevent_run_on_main_thread(std::function<void()> fn);

// The following functions are used only for tests.
void fdevent_terminate_loop();
size_t fdevent_installed_count();
void fdevent_reset();

#endif
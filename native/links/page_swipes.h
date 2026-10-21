#pragma once
#include <QFile>
#include <QCryptographicHash>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <vector>

struct SwipeCallSite { uintptr_t offset; unsigned char original[4]; };
#include "page_swipe_sites.h"

// Validate all sites before changing any. GUI-thread only; never writes firmware files.
static bool setSwipeCalls(void *base, const SwipeCallSite *sites, size_t count, bool block) {
    const unsigned char nop[4] = {0x00, 0xbf, 0x00, 0xbf};
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (!base || pageSize <= 0) return false;
    std::vector<void *> pages;
    for (size_t i = 0; i < count; ++i) {
        unsigned char *call = static_cast<unsigned char *>(base) + sites[i].offset;
        if (std::memcmp(call, sites[i].original, 4) && std::memcmp(call, nop, 4)) return false;
        for (size_t byte = 0; byte < 4; ++byte) {
            void *page = reinterpret_cast<void *>((reinterpret_cast<uintptr_t>(call + byte)
                / static_cast<uintptr_t>(pageSize)) * static_cast<uintptr_t>(pageSize));
            bool found = false;
            for (void *existing : pages) if (existing == page) found = true;
            if (!found) pages.push_back(page);
        }
    }
    size_t writable = 0;
    for (; writable < pages.size(); ++writable)
        if (mprotect(pages[writable], pageSize, PROT_READ | PROT_WRITE | PROT_EXEC)) break;
    if (writable != pages.size()) {
        while (writable) mprotect(pages[--writable], pageSize, PROT_READ | PROT_EXEC);
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        char *call = static_cast<char *>(base) + sites[i].offset;
        std::memcpy(call, block ? nop : sites[i].original, 4);
        __builtin___clear_cache(call, call + 4);
    }
    bool restored = true;
    for (void *page : pages)
        if (mprotect(page, pageSize, PROT_READ | PROT_EXEC)) restored = false;
    return restored;
}

class PageSwipeModule {
    void *handle = nullptr; // Keep the already-loaded module resident.
    void *base = nullptr;
    bool blocked = false;
    bool failed = false;
public:
    void update(const char *library, const char *digest, const char *symbol,
                const SwipeCallSite *sites, size_t count, bool block) {
        if (failed || blocked == block) return;
        if (!handle) {
            handle = dlopen(library, RTLD_LAZY | RTLD_NOLOAD);
            if (!handle) return; // Never force-load the notebook module.
            QFile file(QString::fromLatin1(library));
            QCryptographicHash hash(QCryptographicHash::Sha256);
            Dl_info info = {};
            void *entry = dlsym(handle, symbol);
            if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file)
                || hash.result().toHex() != digest || !entry || !dladdr(entry, &info)) {
                failed = true;
                nh_log("Swipe validation failed: %s", library);
                return;
            }
            base = info.dli_fbase;
        }
        if (!setSwipeCalls(base, sites, count, block)) {
            failed = true;
            nh_log("Could not update swipe call sites: %s", library);
            return;
        }
        blocked = block;
    }
};

static void updatePageSwipes(bool block) {
    static PageSwipeModule books, notebooks;
    books.update("/usr/local/Kobo/libnickel.so.1.0.0",
        "25ec19024666dca7f134335af311593a21cfbd75a0235f1b77c27d9a28f733dd",
        "_ZN11ReadingView12processSwipeEP12SwipeGesture",
        bookSwipeSites, sizeof(bookSwipeSites) / sizeof(*bookSwipeSites), block);
    notebooks.update("/usr/local/Kobo/libiinknote.so",
        "2fa70e17773c645d659ff10eefc3692ffe474feb53a9b4b43d964917653c6b7c",
        "_ZN17IInkNotePadWidget12swipeGestureEP15GestureReceiverP12SwipeGesture",
        notebookSwipeSites, sizeof(notebookSwipeSites) / sizeof(*notebookSwipeSites), block);
}

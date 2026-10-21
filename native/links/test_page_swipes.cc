#include <cstdarg>
static void nh_log(const char *, ...) {}
#include "page_swipes.h"
#include <cassert>
int main() {
    updatePageSwipes(false); // Default state must not load native libraries.
    const long page = sysconf(_SC_PAGESIZE);
    assert(page > 0);
    void *memory = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(memory != MAP_FAILED);
    unsigned char *bytes = static_cast<unsigned char *>(memory);
    std::memset(bytes, 0x5a, page * 2);
    const SwipeCallSite sites[] = {
        {2, {1, 2, 3, 4}}, {static_cast<uintptr_t>(page - 2), {5, 6, 7, 8}}
    };
    for (const auto &site : sites) std::memcpy(bytes + site.offset, site.original, 4);
    std::vector<unsigned char> original(bytes, bytes + page * 2);
    assert(mprotect(memory, page * 2, PROT_READ | PROT_EXEC) == 0);
    assert(setSwipeCalls(memory, sites, 2, true));
    assert(setSwipeCalls(memory, sites, 2, true)); // Idempotent.
    for (long i = 0; i < page * 2; ++i) {
        bool patched = false;
        for (const auto &site : sites)
            if (static_cast<uintptr_t>(i) >= site.offset && static_cast<uintptr_t>(i) < site.offset + 4)
                patched = true;
        if (!patched) assert(bytes[i] == original[i]);
    }
    const unsigned char nop[] = {0, 0xbf, 0, 0xbf};
    for (const auto &site : sites) assert(!std::memcmp(bytes + site.offset, nop, 4));
    assert(setSwipeCalls(memory, sites, 2, false));
    assert(!std::memcmp(bytes, original.data(), page * 2));
    // A later mismatched site must reject the whole group before any writes.
    assert(mprotect(memory, page * 2, PROT_READ | PROT_WRITE) == 0);
    bytes[sites[1].offset] = 0xff;
    std::vector<unsigned char> mismatched(bytes, bytes + page * 2);
    assert(mprotect(memory, page * 2, PROT_READ | PROT_EXEC) == 0);
    assert(!setSwipeCalls(memory, sites, 2, true));
    assert(!std::memcmp(bytes, mismatched.data(), page * 2));
    assert(!setSwipeCalls(nullptr, sites, 2, true));
    munmap(memory, page * 2);
}

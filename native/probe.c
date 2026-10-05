/* One-shot symbol probe. Never invokes native methods or installs PLT hooks. */
#include <NickelHook.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define ENABLE "/mnt/onboard/.adds/kobo-os-probe.enabled"
#define REPORT "/mnt/onboard/.adds/kobo-os-probe.txt"
#define TEMP REPORT ".tmp"

static const char *symbols[] = {
    "_ZN13VolumeManager21getNotebookByObjectIdERK7QStringRK6Device",
    "_ZN14N3ReaderOpener11openNotePadERK6Volume",
    "_ZN14N3ReaderOpener11showNotePadERK6Volume",
    "_ZN14N3ReaderOpenerC1EP7QObjectRK6VolumeRK7QVectorI10ContentKeyEP25AbstractLibraryControllerIS2_E",
    "_ZN21IInkNotePadControllerC1ERK6Volume",
    "_ZN20MainWindowController14sharedInstanceEv",
    "_ZN20MainWindowController4pushEP18AbstractControllerb",
    "_ZNK7Content13getExternalIdEv",
    "_ZNK6Volume15isBasicNotebookEv",
    NULL
};

static int probe_init(void) {
    FILE *version, *report;
    char line[512], *token;
    int supported = 0, missing = 0, failed = 0;
    size_t i;
    if (access(ENABLE, F_OK) != 0) return 0;
    version = fopen("/mnt/onboard/.kobo/version", "r");
    if (!version) return 0;
    if (fgets(line, sizeof(line), version)) {
        for (token = strtok(line, ",\r\n"); token; token = strtok(NULL, ",\r\n")) {
            if (!strcmp(token, "4.46.23836")) supported = 1;
        }
    }
    fclose(version);
    if (!supported) {
        nh_log("KoboLinkProbe: unsupported firmware; probe skipped");
        return 0;
    }
    report = fopen(TEMP, "wx");
    if (!report) { nh_log("KoboLinkProbe: cannot create report"); return 0; }
    if (fprintf(report, "firmware=4.46.23836\nmode=symbol-resolution-only\n") < 0) failed = 1;
    for (i = 0; symbols[i]; i++) {
        int found = dlsym(RTLD_DEFAULT, symbols[i]) != NULL;
        missing += !found;
        if (fprintf(report, "%s=%s\n", symbols[i], found ? "present" : "missing") < 0) failed = 1;
    }
    if (fprintf(report, "missing=%d\nnative_calls=0\n", missing) < 0) failed = 1;
    if (fflush(report) || fsync(fileno(report))) failed = 1;
    if (fclose(report)) failed = 1;
    if (failed || rename(TEMP, REPORT)) {
        unlink(TEMP);
        nh_log("KoboLinkProbe: report failed");
        return 0;
    }
    nh_log("KoboLinkProbe: completed, missing=%d; native calls=0", missing);
    return 0;
}

static struct nh_info info = {
    .name = "KoboLinkProbe",
    .desc = "Diagnostic only: resolve notebook symbols without calling them",
    .uninstall_flag = "/mnt/onboard/.adds/kobo-os-probe.uninstall",
};

NickelHook(
    .init = probe_init,
    .info = &info,
)

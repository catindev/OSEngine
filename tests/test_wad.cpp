// WAD3 loader tests.

#include "../src/formats/WAD.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace OS;

static void Assert(bool cond, const char* msg) {
    if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); exit(1); }
    printf("PASS: %s\n", msg);
}

static void TestWADNotFound() {
    WADFile wad("/nonexistent/path/file.wad");
    Assert(!wad.IsValid(), "Non-existent WAD reports invalid");
    Assert(!wad.Error().empty(), "Non-existent WAD has error message");
    printf("  Error: %s\n", wad.Error().c_str());
}

static void TestWADWrongMagic() {
    // Build a fake WAD with wrong magic
    const char* tmpPath = "/tmp/test_bad_magic.wad";
    FILE* f = fopen(tmpPath, "wb");
    if (f) {
        WAD3Header hdr;
        memcpy(hdr.magic, "WAD1", 4);
        hdr.numEntries = 0;
        hdr.dirOffset  = sizeof(WAD3Header);
        fwrite(&hdr, sizeof(hdr), 1, f);
        fclose(f);

        WADFile wad(tmpPath);
        Assert(!wad.IsValid(), "WAD with wrong magic rejected");
    }
}

static void TestWADManager() {
    WADManager mgr;
    Assert(mgr.WADCount() == 0, "Empty manager has 0 WADs");

    // Adding non-existent WAD returns false
    bool ok = mgr.AddWAD("/nonexistent.wad");
    Assert(!ok, "Adding non-existent WAD returns false");
    // WAD count stays 1 (it records the attempt but IsValid=false)

    // FindTexture on empty manager returns null
    const WADTexture* t = mgr.FindTexture("some_texture");
    Assert(t == nullptr, "FindTexture on empty manager returns null");
}

int main() {
    printf("=== WAD Tests ===\n\n");

    TestWADNotFound();
    TestWADWrongMagic();
    TestWADManager();

    printf("\nAll WAD tests passed.\n");
    return 0;
}

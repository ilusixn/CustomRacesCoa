#ifndef COA_SOURCE_COA_H
#define COA_SOURCE_COA_H

namespace CoA {
    // A ported Extensions.dll module: its Init installs what the DLL's module init did (packet handlers, CVars,
    // lifecycle callbacks).
    struct Module {
        const char* name;
        void (*init)();
    };

    // Called once on the main thread where Ascension.exe loaded Extensions.dll (cave 0x4E5CB0, before main()).
    // Order as the DLL's attach: runtime plumbing, then every module's Init.
    void Attach();
}

#endif

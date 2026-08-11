#pragma once

namespace GW {
    struct Module {
        const char  *name;
        void        *param;

        void       (*init_module)();
        void       (*exit_module)();

        // Call these from the game thread to be safe, and do not free the trampoline.
        void       (*enable_hooks)();
        void       (*disable_hooks)();
    };
}

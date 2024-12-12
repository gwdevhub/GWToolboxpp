#pragma once

namespace GW {
    struct Module {
        const char  *name;
        void        *param;

        void       (*init_module)();
        void       (*exit_module)();

        // Call those from game thread to be safe
        // Do not free trampoline
        void       (*enable_hooks)();
        void       (*disable_hooks)();
    };
}

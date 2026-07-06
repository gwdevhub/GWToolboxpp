#include "stdafx.h"

#include "AvReadinessWindow.h"

// The antivirus readiness checklist runs as its own tiny, non-injecting executable so it carries a clean reputation -
// unlike the launcher, which antivirus distrusts because it injects into Guild Wars. Read-only: never changes an AV setting.
#ifdef _DEBUG
int main()
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#endif
{
    AvReadinessWindow::Run();
    return 0;
}

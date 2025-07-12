#include <windows.h>
#include <string>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // Get path of current executable (funsielauncher.exe)
    char modulePath[MAX_PATH];
    GetModuleFileNameA(NULL, modulePath, MAX_PATH);

    std::string path(modulePath);

    // Replace "funsielauncher.exe" with "winhost.exe"
    size_t pos = path.find("funsielauncher.exe");
    if (pos != std::string::npos)
        path.replace(pos, strlen("funsielauncher.exe"), "winhost.exe");
    else
    {
        MessageBoxA(NULL, "Launcher must be named funsielauncher.exe", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Start hidden

    BOOL success = CreateProcessA(
        NULL,
        const_cast<char*>(path.c_str()),
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (success)
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    else
    {
        MessageBoxA(NULL, "Failed to launch winhost.exe", "Error", MB_OK | MB_ICONERROR);
    }

    // Exit immediately after launching stub
    return 0;
}

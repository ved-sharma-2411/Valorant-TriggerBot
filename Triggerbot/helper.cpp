#include <stdio.h>
#include <windows.h>
#include <stdbool.h>
#include <shlwapi.h>
#include <iostream>
#include <vector>
#include "helper.h"

#pragma comment(lib, "Shlwapi.lib")

Triggerbot::Triggerbot(CONFIG* cfg) : 
    screen_dc(0), bitmap(0), mem_dc(0)
{
    if (!GetConfig(cfg)) {
        printf("ERROR: GetConfig failed!\n");
        throw std::runtime_error("GetConfig failed!");
    }

    print_banner();
}

Triggerbot::~Triggerbot() {
    if (bitmap) DeleteObject(bitmap);
    if (mem_dc) DeleteDC(mem_dc);
    if (screen_dc) ReleaseDC(0, screen_dc);
}

std::vector<DWORD> Triggerbot::GetScreenshot(std::wstring* save_name, int crop_width, int crop_height) {
    std::vector<DWORD> pixels;

    if (crop_width <= 0 || crop_height <= 0) {
        printf("ERROR: invalid crop dimensions!\n");
        return pixels;
    }

    FILE* file = 0;

    screen_dc = GetDC(0);
    if (screen_dc == 0) {
        printf("ERROR: GetDC() failed!\n");
        return pixels;
    }

    BITMAP bmp = { 0 };
    BITMAPFILEHEADER bmp_file_header = { 0 };

    int screen_width = GetDeviceCaps(screen_dc, DESKTOPHORZRES);
    int screen_height = GetDeviceCaps(screen_dc, DESKTOPVERTRES);

    int crop_x = 0;
    int crop_y = 0;

    // Calculate coordinates for center crop 
    crop_x = (screen_width - crop_width) / 2;
    crop_y = (screen_height - crop_height) / 2;

    // Create memory DC and compatible bitmap 
    mem_dc = CreateCompatibleDC(screen_dc);

    if (mem_dc == 0) {
        printf("ERROR: CreateCompatibleDC() failed!\n");
        return pixels;
    }

    bitmap = CreateCompatibleBitmap(screen_dc, crop_width, crop_height);

    if (bitmap == 0) {
        printf("ERROR: CreateCompatibleBitmap() failed!\n");
        return pixels;
    }

    SelectObject(mem_dc, bitmap);

    // Copy cropped screen contents to memory DC
    if (BitBlt(mem_dc, 0, 0, crop_width, crop_height, screen_dc, crop_x, crop_y, SRCCOPY) == 0) {
        printf("ERROR: BitBlt() failed!\n");
        return pixels;
    }

    // Prepare bitmap header 
    GetObject(bitmap, sizeof(BITMAP), &bmp);

    // Create bitmap file header 
    bmp_file_header.bfType = 0x4D42; // BM 
    bmp_file_header.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) +
        bmp.bmWidthBytes * crop_height;
    bmp_file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    // Create bitmap info header 
    BITMAPINFOHEADER bmp_info_header = { 0 };
    bmp_info_header.biSize = sizeof(BITMAPINFOHEADER);
    bmp_info_header.biWidth = bmp.bmWidth;
    bmp_info_header.biHeight = bmp.bmHeight;
    bmp_info_header.biPlanes = 1;
    bmp_info_header.biBitCount = 32; // Use 32-bit color 
    bmp_info_header.biCompression = BI_RGB;
    bmp_info_header.biSizeImage = bmp.bmWidthBytes * bmp.bmHeight;

    // pixels = new unsigned int[(bmp.bmWidthBytes * crop_height) / sizeof(DWORD)];
    pixels.resize((bmp.bmWidthBytes * crop_height) / sizeof(DWORD));

    // Get bitmap data 
    if (GetDIBits(mem_dc, bitmap, 0, crop_height, pixels.data(), (BITMAPINFO*)&bmp_info_header, DIB_RGB_COLORS) == 0) {
        printf("ERROR: GetDIBits() failed!\n");
        pixels.clear();
        return pixels;
    }

    if (save_name) {
        file = _wfopen(save_name->c_str(), L"wb");

        if (!file) {
            printf("Error: fopen() failed!\n");
            return pixels;
        }

        fwrite(&bmp_file_header, sizeof(BITMAPFILEHEADER), 1, file);
        fwrite(&bmp_info_header, sizeof(BITMAPINFOHEADER), 1, file);
        fwrite(pixels.data(), bmp.bmWidthBytes * crop_height, 1, file);
        fclose(file);
    }

    DeleteObject(bitmap);
    DeleteDC(mem_dc);
    ReleaseDC(0, screen_dc);

    return pixels;
}

bool Triggerbot::IsColorFound(DWORD* pixels, int pixel_count, int red, int green, int blue, int color_sens) {
    for (int i = 0; i < pixel_count; i++) {
        DWORD pixelColor = pixels[i];

        int r = GetRed(pixelColor);
        int g = GetGreen(pixelColor);
        int b = GetBlue(pixelColor);

        if (r + color_sens >= red && r - color_sens <= red) 
            if (g + color_sens >= green && g - color_sens <= green) 
                if (b + color_sens >= blue && b - color_sens <= blue) 
                    return true;
    }

    return false;
}

bool Triggerbot::GetConfig(CONFIG* cfg) {
    wchar_t temp[256];
    DWORD result;

    std::wstring RelativePath = L"config.txt";
    wchar_t FullPath[MAX_PATH];

    if (!GetFullPathNameW(RelativePath.c_str(), MAX_PATH, FullPath, NULL)) {
        printf("ERROR getting full path\n");
        return false;
    }

    if (!PathFileExistsW(FullPath)) {
        printf("ERROR: config.txt was not found. \n");
        return false;
    }

    result = GetPrivateProfileStringW(L"General", L"hold_mode", NULL, temp, sizeof(temp) / sizeof(temp[0]), FullPath);
    if (!result) {
        printf("ERROR: Failed to read 'hold_mode' from config!\n");
        return false;
    }
    cfg->hold_mode = temp;

    cfg->toggle_key = GetPrivateProfileIntW(L"General", L"toggle_key", -1, FullPath);
    if (cfg->toggle_key <= -1) {
        printf("ERROR: Failed to read 'toggle_key' from config!\n");
        return false;
    }

    result = GetPrivateProfileStringW(L"General", L"shoot_while_moving", NULL, temp, sizeof(temp) / sizeof(temp[0]), FullPath);
    if (!result) {
        printf("ERROR: Failed to read 'shoot_while_moving' from config!\n");
        return false;
    }
    cfg->shoot_while_moving = temp;

    cfg->tap_time = GetPrivateProfileIntW(L"General", L"tap_time", -1, FullPath);
    if (cfg->tap_time <= -1) {
        printf("ERROR: Failed to read 'tap_time' from config!\n");
        return false;
    }

    cfg->reaction_delay = GetPrivateProfileIntW(L"General", L"reaction_delay", -1, FullPath);
    if (cfg->reaction_delay <= -1) {
        printf("ERROR: Failed to read 'reaction_delay' from config!\n");
        return false;
    }

    cfg->scan_area = GetPrivateProfileIntW(L"FOV", L"scan_area", 0, FullPath);
    if (cfg->scan_area <= 0) {
        printf("ERROR: Failed to read 'scan_area' from config!\n");
        return false;
    }

    result = GetPrivateProfileStringW(L"Burst", L"burst_mode", NULL, temp, sizeof(temp) / sizeof(temp[0]), FullPath);
    if (!result) {
        printf("ERROR: Failed to read 'burst_mode' from config!\n");
        return false;
    }
    cfg->burst_mode = temp;

    cfg->burst_count = GetPrivateProfileIntW(L"Burst", L"burst_count", 0, FullPath);
    if (cfg->burst_count <= 0) {
        printf("ERROR: Failed to read 'burst_count' from config!\n");
        return false;
    }

    cfg->color_sens = GetPrivateProfileIntW(L"ColorRGB", L"color_sens", -1, FullPath);
    if (cfg->color_sens <= -1) {
        printf("ERROR: Failed to read 'color_sens' from config!\n");
        return false;
    }

    cfg->red = GetPrivateProfileIntW(L"ColorRGB", L"red", -1, FullPath);
    if (cfg->red <= -1) {
        printf("ERROR: Failed to read 'red' from config!\n");
        return false;
    }

    cfg->green = GetPrivateProfileIntW(L"ColorRGB", L"green", -1, FullPath);
    if (cfg->green <= -1) {
        printf("ERROR: Failed to read 'green' from config!\n");
        return false;
    }

    cfg->blue = GetPrivateProfileIntW(L"ColorRGB", L"blue", -1, FullPath);
    if (cfg->blue <= -1) {
        printf("ERROR: Failed to read 'blue' from config!\n");
        return false;
    }

    if (cfg->hold_mode != L"ON" && cfg->hold_mode != L"OFF") {
        printf("ERROR: Invalid hold mode value. Hold mode can only be ON or OFF\n");
        return false;
    }

    if (cfg->shoot_while_moving != L"ON" && cfg->shoot_while_moving != L"OFF") {
        printf("ERROR: Invalid Shoot while moving value. Shoot while moving can only be ON or OFF\n");
        return false;
    }

    if (cfg->burst_mode != L"ON" && cfg->burst_mode != L"OFF") {
        printf("ERROR: Invalid Burst mode value. Burst mode can only be ON or OFF\n");
        return false;
    }

    return true;
}

bool Triggerbot::LeftClick(HWND h_window) {
    if (!PostMessageW(h_window, WM_LBUTTONDOWN, MK_LBUTTON, 0)) {
        return false;
    }

    if (!PostMessageW(h_window, WM_LBUTTONUP, 0, 0)) {
        return false;
    }

    return true;
}

bool Triggerbot::IsMoveKeysPressed() {
    if (GetAsyncKeyState('W') & 0x8000 ||
        GetAsyncKeyState('A') & 0x8000 ||
        GetAsyncKeyState('S') & 0x8000 ||
        GetAsyncKeyState('D') & 0x8000)
        return true;
    else
        return false;
}

bool Triggerbot::burst(int burst_count, HWND hwnd) {
    for (int i = 0; i < burst_count; i++) {
        if (!LeftClick(hwnd))
            return false;
        Sleep(100);
    }

    return true;
}

void Triggerbot::disable_quickedit() {
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD prevMode;

    GetConsoleMode(hInput, &prevMode);
    SetConsoleMode(hInput, prevMode & ~ENABLE_QUICK_EDIT_MODE);
}

bool Triggerbot::IsKeyHeld(int hold_key) {
    return GetAsyncKeyState(hold_key) & 0x8000;
}

bool Triggerbot::IsKeyPressed(int toggle_key) {
    return GetAsyncKeyState(toggle_key) & 0x0001;
}

void Triggerbot::print_banner() {
    system("cls");
    disable_quickedit();
    SetConsoleTitleW(L"Highway TB by Ved");
    printf("\n");

    printf(" /$$   /$$ /$$           /$$                                 \n");
    printf("| $$  | $$|__/          | $$                                 \n");
    printf("| $$  | $$ /$$  /$$$$$$ | $$$$$$$  /$$  /$$  /$$  /$$$$$$  /$$   /$$\n");
    printf("| $$$$$$$$| $$ /$$__  $$| $$__  $$| $$ | $$ | $$ |____  $$| $$  | $$\n");
    printf("| $$__  $$| $$| $$  \\ $$| $$  \\ $$| $$ | $$ | $$  /$$$$$$$| $$  | $$\n");
    printf("| $$  | $$| $$| $$  | $$| $$  | $$| $$ | $$ | $$ /$$__  $$| $$  | $$\n");
    printf("| $$  | $$| $$|  $$$$$$$| $$  | $$|  $$$$$/$$$$/|  $$$$$$$|  $$$$$$$\n");
    printf("|__/  |__/|__/ \\____  $$|__/  |__/ \\_____/\___/  \\_______/ \\____  $$\n");
    printf("               /$$  \\ $$                                   /$$  | $$\n");
    printf("              |  $$$$$$/                                  |  $$$$$$/\n");
    printf("               \\______/                                    \\______/\n");
    printf("                                                             \n");
    printf("                       Made By Ved-Sharma-2411               \n");
    printf("                                                             \n");

    printf("\n");
    printf("\n");

    printf("Trigger some stars & forks on my repo — it's free Auto Shoot! -> github.com/ved-sharma-2411/Valorant-TriggerBot\n");
    printf("[IMP-INFO] If you love detection issues, ignore GitHub updates. Otherwise, check regularly! 😉\n");

    printf("The triggerbot is running successfully. Go into a game and enjoy!\n\n");
}
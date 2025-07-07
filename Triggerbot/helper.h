#include <stdio.h>
#include <windows.h>

#define GetBlue(rgb)     (LOBYTE(rgb))
#define GetGreen(rgb)    (LOBYTE(((WORD)(rgb)) >> 8))
#define GetRed(rgb)      (LOBYTE((rgb)>>16))

#define ReactionTime(stop, start, freq) ((double)((stop).QuadPart - (start).QuadPart) * 1000.0 / (freq).QuadPart)

typedef struct {
    std::wstring hold_mode;
    std::wstring shoot_while_moving;
    std::wstring burst_mode;

    int toggle_key;
    int tap_time;
    int reaction_delay;
    int scan_area;
    int burst_count;
    int color_sens;
    int red;
    int green;
    int blue;
} CONFIG;

class Triggerbot {
public:
    Triggerbot(CONFIG* cfg);
    ~Triggerbot();
	std::vector<DWORD> GetScreenshot(std::wstring* save_name, int crop_width, int crop_height);
    bool IsColorFound(DWORD* pixels, int pixel_count, int red, int green, int blue, int color_sens);
    bool LeftClick(HWND h_window);
    bool IsMoveKeysPressed();
    bool IsKeyPressed(int toggle_key);
    bool IsKeyHeld(int hold_key);
    bool burst(int burst_count, HWND hwnd);
    void print_banner();

private:
	HDC screen_dc;
	HBITMAP bitmap;
	HDC mem_dc;

    bool GetConfig(CONFIG* cfg);
    void disable_quickedit();
};
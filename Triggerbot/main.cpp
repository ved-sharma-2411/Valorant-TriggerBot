#include <stdio.h>
#include <windows.h>
#include <stdbool.h>
#include <shlwapi.h>
#include <iostream>
#include <vector>
#include "helper.h"

int main() {
	LARGE_INTEGER freq, start, stop;
	Triggerbot* triggerbot = nullptr;
	CONFIG cfg;

	HWND hwnd = FindWindowW(NULL, L"VALORANT  ");
	if (!hwnd) {
		printf("ERROR: FindWindow failed! Make sure that Valorant is running.\n");
		getchar();
		return 1;
	}

	try {
		triggerbot = new Triggerbot(&cfg);
	}
	catch (...) {
		getchar();
		return 1;
	}

	bool is_active = true;
	int pixel_count = cfg.scan_area * cfg.scan_area;

	QueryPerformanceFrequency(&freq);

	while (true) {
		if (cfg.hold_mode == L"ON" && !triggerbot->IsKeyHeld(cfg.toggle_key)) {
			Sleep(1);
			continue;
		}

		if (cfg.shoot_while_moving == L"OFF" && triggerbot->IsMoveKeysPressed()) {
			Sleep(1);
			continue;
		}

		if (cfg.hold_mode == L"OFF" && triggerbot->IsKeyPressed(cfg.toggle_key)) {
			is_active = !is_active;

			if (is_active)
				Beep(750, 200);
			else
				Beep(350, 150);

			Sleep(10);
		}

		if (!is_active) {
			Sleep(1);
			continue;
		}

		QueryPerformanceCounter(&start);

		std::vector<DWORD> screenshot = triggerbot->GetScreenshot(nullptr, cfg.scan_area, cfg.scan_area);
		if (screenshot.empty()) {
			printf("ERROR: GetScreenshot failed!\n");
			getchar();
			return 1;
		}

		if (triggerbot->IsColorFound(screenshot.data(), pixel_count, cfg.red, cfg.green, cfg.blue, cfg.color_sens)) {
			Sleep(cfg.reaction_delay);
		
			QueryPerformanceCounter(&stop);
			printf("\rReaction time: %.0f ms  ", ReactionTime(stop, start, freq));

			if (cfg.burst_mode == L"ON") {
				if (!triggerbot->burst(cfg.burst_count, hwnd)) {
					printf("\nERROR: Failed to simulate left click!\n");
					return 1;
				}
			}
			else {
				if (!triggerbot->LeftClick(hwnd)) {
					printf("\nERROR: Failed to simulate left click!\n");
					return 1;
				}
			}

			Sleep(cfg.tap_time);
		}

	}

	delete triggerbot;
	return 0;
}
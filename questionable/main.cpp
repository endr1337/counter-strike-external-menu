#include <windows.h>
#include "ImGui/imgui_impl_win32.h"
#include "w2s.h"
#include "esp.h"
#include "offsets.h"
#include <iostream>
#include <TlHelp32.h>
#include <thread>
#include <string>
#include <cmath>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <cstring>
#include "menu.h"
#include <dwmapi.h>
#include <d3d9.h>
bool cnseemenu = true;

uintptr_t GetModuleBaseAddress(DWORD processID, const TCHAR* moduleName) {
	uintptr_t baseAddress = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processID);

	if (hSnapshot != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 moduleEntry;
		moduleEntry.dwSize = sizeof(MODULEENTRY32);

		if (Module32First(hSnapshot, &moduleEntry)) {
			do {
				if (_tcscmp(moduleEntry.szModule, moduleName) == 0) {
					baseAddress = (uintptr_t)moduleEntry.modBaseAddr;
					break;
				}
			} while (Module32Next(hSnapshot, &moduleEntry));
		}
		CloseHandle(hSnapshot);
	}
	return baseAddress;
}
HWND CreateOverlayWindow(HWND game_hwnd)
{
	RECT rect;
	GetWindowRect(game_hwnd, &rect);
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
					  GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
					  _T("OverlayWindow"), NULL };
	RegisterClassEx(&wc);
	HWND overlay_hwnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED,
		wc.lpszClassName, _T("Overlay"),
		WS_POPUP,
		rect.left, rect.top,
		rect.right - rect.left, rect.bottom - rect.top,
		NULL, NULL, wc.hInstance, NULL);
	COLORREF colorkey = RGB(255, 0, 255); // Magenta color key
	SetLayeredWindowAttributes(overlay_hwnd, 0, 0, LWA_COLORKEY);
	ShowWindow(overlay_hwnd, SW_SHOW);
	UpdateWindow(overlay_hwnd);
	return overlay_hwnd;
}



DWORD pid;
int main() {
	HWND hwnd = FindWindowA(NULL, "Counter-Strike");
	HWND overlay_hwnd = CreateOverlayWindow(hwnd);
	if (!hwnd) {
		std::cout << "failed to find cs1.6 open";
		Sleep(100);
		return 1;
	}

	GetWindowThreadProcessId(hwnd, &pid);
	HANDLE pHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (pHandle == NULL) {
		std::cerr << "failed to open process :c\n";
		Sleep(5000);
		return 1;
	}
	uintptr_t clientbaseaddr = GetModuleBaseAddress(pid, "client.dll");
	uintptr_t hwbaseaddr = GetModuleBaseAddress(pid, "hw.dll");
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L };
	RegisterClassEx(&wc);
	printf("[*] Base addr of client.dll: 0x%p\n", clientbaseaddr);
	printf("[*] Base addr of hw.dll: 0x%p\n", hwbaseaddr);
	if (!CreateDeviceD3D(overlay_hwnd)) {
		CleanupDeviceD3D();
		UnregisterClass(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	ShowWindow(GetConsoleWindow(), SW_HIDE);
	UpdateWindow(overlay_hwnd);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	ImGuiStyle& style = ImGui::GetStyle();

	ImGui_ImplWin32_Init(overlay_hwnd);
	ImGui_ImplDX9_Init(g_pd3dDevice);
	bool done = true;
	std::string str_window_title = ";)";
	const char* window_title = str_window_title.c_str();

	ImVec2 window_size{ 200, 200 };

	DWORD window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

	ImGui::StyleColorsDark();

	while (done) {
		RECT clientRect;
		GetClientRect(hwnd, &clientRect);
		POINT pt = { 0, 0 };
		ClientToScreen(hwnd, &pt);

		SetWindowPos(
			overlay_hwnd,
			HWND_TOPMOST,
			pt.x,
			pt.y,
			clientRect.right - clientRect.left,
			clientRect.bottom - clientRect.top,
			SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW
		);
		MSG msg;
		ZeroMemory(&msg, sizeof(msg));
		while (msg.message != WM_QUIT)
		{
			if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE) && msg.message != WM_QUIT)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				break;
			}
		}


		ImGuiStyle* style = &ImGui::GetStyle();

		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		{
			ImGuiStyle& style = ImGui::GetStyle();
			style.WindowRounding = 0.0f;
			style.WindowBorderSize = 0.0f;
			style.FrameRounding = 0.0f;
			style.AntiAliasedFill = false;
			style.AntiAliasedLines = false;
			ImGui::SetNextWindowBgAlpha(1.0f);
			ImGui::SetNextWindowSize(window_size);
			ImGui::SetNextWindowBgAlpha(1.0f);
			static bool themec = false;
			ImGui::Begin(window_title, &cnseemenu, window_flags);
			ImGui::End();

		}
		ImGui::EndFrame();
		g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, NULL, 1.0f, 0);


		if (g_pd3dDevice->BeginScene() >= 0)
		{
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			g_pd3dDevice->EndScene();
		}

		HRESULT result = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);

		if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
			ResetDevice();

	}
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	DestroyWindow(overlay_hwnd);

	UnregisterClass(wc.lpszClassName, wc.hInstance);
	return 0;

}

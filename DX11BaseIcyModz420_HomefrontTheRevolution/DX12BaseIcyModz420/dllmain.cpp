#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <string>
#include <functional>
#include <cstdio>
#include <fstream>
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "MinHook.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// ---------------------------------------------------------------------------
// Present / ResizeBuffers
// ---------------------------------------------------------------------------
typedef HRESULT(APIENTRY* PresentFn)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
PresentFn oPresent = nullptr;

typedef HRESULT(APIENTRY* ResizeBuffersFn)(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
ResizeBuffersFn oResizeBuffers = nullptr;

HWND g_hWnd = nullptr;
WNDPROC oWndProc = nullptr;
bool g_init = false;
bool showMenu = true;

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dContext = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ---------------------------------------------------------------------------
// Configurable Settings
// ---------------------------------------------------------------------------
struct MenuConfig {
    float globalScale = 1.43f;
    float windowWidth = 1842.0f;
    float windowHeight = 1550.0f;
    int colorTheme = 0;
    float accentColor[4] = { 0.0f, 0.65f, 1.0f, 1.0f };
    float textColor[4] = { 0.95f, 0.96f, 0.98f, 1.00f };
    float buttonColor[4] = { 0.14f, 0.16f, 0.20f, 1.00f };
    float sliderColor[4] = { 0.0f, 0.65f, 1.0f, 1.0f };
    float bgAlpha = 0.94f;
    float customBgColor[4] = { 0.06f, 0.06f, 0.08f, 1.00f };
    int fontChoice = 0;
    float fontScaleFactor = 2.50f;
    float buttonPaddingX = 25.0f;
    float buttonPaddingY = 10.0f;
    float buttonWidthScale = 2.83f;
    float buttonHeightScale = 1.78f;
    float titlePaddingX = 21.0f;
    float submenuItemPaddingX = 22.9f;
    float titleBarPaddingX = 16.0f;
    float titleBarPaddingY = 10.0f;
};
MenuConfig g_config;

void SaveConfig() {
    FILE* f = fopen("ProjectUnity_Config.ini", "w");
    if (f) {
        fprintf(f, "[Settings]\n");
        fprintf(f, "Scale=%f\n", g_config.globalScale);
        fprintf(f, "Width=%f\n", g_config.windowWidth);
        fprintf(f, "Height=%f\n", g_config.windowHeight);
        fprintf(f, "Theme=%d\n", g_config.colorTheme);
        fprintf(f, "BgAlpha=%f\n", g_config.bgAlpha);
        fprintf(f, "Font=%d\n", g_config.fontChoice);
        fprintf(f, "FontScaleFactor=%f\n", g_config.fontScaleFactor);
        fprintf(f, "ButtonPaddingX=%f\n", g_config.buttonPaddingX);
        fprintf(f, "ButtonPaddingY=%f\n", g_config.buttonPaddingY);
        fprintf(f, "ButtonWidthScale=%f\n", g_config.buttonWidthScale);
        fprintf(f, "ButtonHeightScale=%f\n", g_config.buttonHeightScale);
        fprintf(f, "TitlePaddingX=%f\n", g_config.titlePaddingX);
        fprintf(f, "SubmenuItemPaddingX=%f\n", g_config.submenuItemPaddingX);
        fprintf(f, "TitleBarPaddingX=%f\n", g_config.titleBarPaddingX);
        fprintf(f, "TitleBarPaddingY=%f\n", g_config.titleBarPaddingY);
        fprintf(f, "AccentR=%f\n", g_config.accentColor[0]);
        fprintf(f, "AccentG=%f\n", g_config.accentColor[1]);
        fprintf(f, "AccentB=%f\n", g_config.accentColor[2]);
        fprintf(f, "TextR=%f\n", g_config.textColor[0]);
        fprintf(f, "TextG=%f\n", g_config.textColor[1]);
        fprintf(f, "TextB=%f\n", g_config.textColor[2]);
        fprintf(f, "ButtonR=%f\n", g_config.buttonColor[0]);
        fprintf(f, "ButtonG=%f\n", g_config.buttonColor[1]);
        fprintf(f, "ButtonB=%f\n", g_config.buttonColor[2]);
        fprintf(f, "BgR=%f\n", g_config.customBgColor[0]);
        fprintf(f, "BgG=%f\n", g_config.customBgColor[1]);
        fprintf(f, "BgB=%f\n", g_config.customBgColor[2]);
        fclose(f);
        OutputDebugStringA("[Project Unity] Settings saved successfully.\n");
    }
}

void LoadConfig() {
    FILE* f = fopen("ProjectUnity_Config.ini", "r");
    if (f) {
        char key[64];
        float val;
        int ival;
        while (fscanf(f, "%[^=]=%f\n", key, &val) == 2) {
            std::string skey(key);
            if (skey == "Scale") g_config.globalScale = val;
            else if (skey == "Width") g_config.windowWidth = val;
            else if (skey == "Height") g_config.windowHeight = val;
            else if (skey == "BgAlpha") g_config.bgAlpha = val;
            else if (skey == "FontScaleFactor") g_config.fontScaleFactor = val;
            else if (skey == "ButtonPaddingX") g_config.buttonPaddingX = val;
            else if (skey == "ButtonPaddingY") g_config.buttonPaddingY = val;
            else if (skey == "ButtonWidthScale") g_config.buttonWidthScale = val;
            else if (skey == "ButtonHeightScale") g_config.buttonHeightScale = val;
            else if (skey == "TitlePaddingX") g_config.titlePaddingX = val;
            else if (skey == "SubmenuItemPaddingX") g_config.submenuItemPaddingX = val;
            else if (skey == "TitleBarPaddingX") g_config.titleBarPaddingX = val;
            else if (skey == "TitleBarPaddingY") g_config.titleBarPaddingY = val;
            else if (skey == "AccentR") g_config.accentColor[0] = val;
            else if (skey == "AccentG") g_config.accentColor[1] = val;
            else if (skey == "AccentB") g_config.accentColor[2] = val;
            else if (skey == "TextR") g_config.textColor[0] = val;
            else if (skey == "TextG") g_config.textColor[1] = val;
            else if (skey == "TextB") g_config.textColor[2] = val;
            else if (skey == "ButtonR") g_config.buttonColor[0] = val;
            else if (skey == "ButtonG") g_config.buttonColor[1] = val;
            else if (skey == "ButtonB") g_config.buttonColor[2] = val;
            else if (skey == "BgR") g_config.customBgColor[0] = val;
            else if (skey == "BgG") g_config.customBgColor[1] = val;
            else if (skey == "BgB") g_config.customBgColor[2] = val;
        }
        rewind(f);
        while (fscanf(f, "%[^=]=%d\n", key, &ival) == 2) {
            std::string skey(key);
            if (skey == "Theme") g_config.colorTheme = ival;
            else if (skey == "Font") g_config.fontChoice = ival;
        }
        fclose(f);
        OutputDebugStringA("[Project Unity] Settings loaded successfully.\n");
    }
}

void ApplyThemeColors() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f * g_config.globalScale;
    style.ChildRounding = 3.0f * g_config.globalScale;
    style.FrameRounding = 3.0f * g_config.globalScale;
    style.GrabRounding = 3.0f * g_config.globalScale;
    style.PopupRounding = 3.0f * g_config.globalScale;
    style.ScrollbarRounding = 3.0f * g_config.globalScale;
    style.WindowBorderSize = 1.0f * g_config.globalScale;
    style.FrameBorderSize = 1.0f * g_config.globalScale;
    style.ItemSpacing = ImVec2(8.0f * g_config.globalScale, 6.0f * g_config.globalScale);
    style.FramePadding = ImVec2(g_config.buttonPaddingX, g_config.buttonPaddingY);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(g_config.textColor[0], g_config.textColor[1], g_config.textColor[2], 1.00f);

    if (g_config.colorTheme == 0) {
        colors[ImGuiCol_WindowBg] = ImVec4(g_config.customBgColor[0], g_config.customBgColor[1], g_config.customBgColor[2], g_config.bgAlpha);
        colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.25f, 0.28f, 0.35f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(g_config.buttonColor[0], g_config.buttonColor[1], g_config.buttonColor[2], 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.23f, 0.30f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.32f, 0.42f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);
    }
    else if (g_config.colorTheme == 1) {
        colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.02f, 0.05f, g_config.bgAlpha);
        colors[ImGuiCol_ChildBg] = ImVec4(0.05f, 0.05f, 0.10f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.00f, 0.50f, 0.80f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(g_config.buttonColor[0], g_config.buttonColor[1], g_config.buttonColor[2], 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.20f, 0.40f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.00f, 0.40f, 0.80f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.06f, 0.15f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.10f, 0.25f, 1.00f);
    }
    else if (g_config.colorTheme == 2) {
        colors[ImGuiCol_WindowBg] = ImVec4(g_config.customBgColor[0], g_config.customBgColor[1], g_config.customBgColor[2], g_config.bgAlpha);
        colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.04f, 0.04f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(g_config.accentColor[0], g_config.accentColor[1], g_config.accentColor[2], 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(g_config.buttonColor[0], g_config.buttonColor[1], g_config.buttonColor[2], 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.40f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.60f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.18f, 0.04f, 0.04f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.30f, 0.06f, 0.06f, 1.00f);
    }

    colors[ImGuiCol_CheckMark] = ImVec4(g_config.accentColor[0], g_config.accentColor[1], g_config.accentColor[2], 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(g_config.sliderColor[0], g_config.sliderColor[1], g_config.sliderColor[2], 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(g_config.sliderColor[0], g_config.sliderColor[1], g_config.sliderColor[2], 1.00f);
    colors[ImGuiCol_Button] = colors[ImGuiCol_FrameBg];
    colors[ImGuiCol_ButtonHovered] = colors[ImGuiCol_FrameBgHovered];
    colors[ImGuiCol_ButtonActive] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_Header] = colors[ImGuiCol_FrameBg];
    colors[ImGuiCol_HeaderHovered] = colors[ImGuiCol_FrameBgHovered];
    colors[ImGuiCol_HeaderActive] = colors[ImGuiCol_FrameBgActive];
}

bool CustomButton(const char* label, const ImVec2& size_arg = ImVec2(0, 0)) {
    ImVec2 size = size_arg;
    if (size.x > 0) size.x *= g_config.buttonWidthScale;
    if (size.y == 0) {
        float defaultHeight = ImGui::GetFontSize() + g_config.buttonPaddingY * 2.0f;
        size.y = defaultHeight * g_config.buttonHeightScale;
    }
    else {
        size.y *= g_config.buttonHeightScale;
    }
    return ImGui::Button(label, size);
}

float ComputeMenuRowHeight() {
    float base = ImGui::GetFontSize() + g_config.buttonPaddingY * 2.0f;
    return base * g_config.buttonHeightScale;
}

// ---------------------------------------------------------------------------
// Menu system
// ---------------------------------------------------------------------------
enum class MenuItemType { Category, Toggle, Slider, Action, CustomSettings, ColorSubmenu };

struct MenuItem {
    std::string label;
    MenuItemType type = MenuItemType::Category;
    std::vector<MenuItem> children;
    bool* toggleValue = nullptr;
    float* sliderValue = nullptr;
    float sliderMin = 0.0f, sliderMax = 1000.0f, sliderStep = 1.0f;
    std::function<void()> onAction;
};

bool g_dummyToggle[5][5] = { {false} };
float g_dummySlider[5][5] = { {50.0f} };

MenuItem BuildMenu() {
    MenuItem root;
    root.label = "Main Menu";
    root.type = MenuItemType::Category;

    for (int m = 1; m <= 4; m++) {
        MenuItem cat;
        char catName[64];
        snprintf(catName, sizeof(catName), "Main Menu %d", m);
        cat.label = catName;
        cat.type = MenuItemType::Category;

        for (int opt = 1; opt <= 5; opt++) {
            MenuItem item;
            char optName[64];
            snprintf(optName, sizeof(optName), "Option %d.%d", m, opt);
            item.label = optName;

            if (opt == 1) {
                item.type = MenuItemType::Toggle;
                item.toggleValue = &g_dummyToggle[m - 1][opt - 1];
            }
            else if (opt == 2) {
                item.type = MenuItemType::Slider;
                item.sliderValue = &g_dummySlider[m - 1][opt - 1];
                item.sliderMin = -5000.0f;
                item.sliderMax = 5000.0f;
                item.sliderStep = 10.0f;
            }
            else {
                item.type = MenuItemType::Action;
                item.onAction = [m, opt]() {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "[Project Unity] Main %d Option %d executed.\n", m, opt);
                    OutputDebugStringA(buf);
                    };
            }
            cat.children.push_back(item);
        }
        root.children.push_back(cat);
    }

    MenuItem settingsCat;
    settingsCat.label = "Menu Settings";
    settingsCat.type = MenuItemType::CustomSettings;
    root.children.push_back(settingsCat);

    return root;
}

struct MenuNavLevel {
    MenuItem* item;
    int selectedIndex;
};

MenuItem g_rootMenu;
std::vector<MenuNavLevel> g_navStack;
bool g_menuBuilt = false;
bool g_inColorSubmenu = false;

void ActivateItem(MenuItem& item) {
    switch (item.type) {
    case MenuItemType::Category:
    case MenuItemType::CustomSettings:
        g_navStack.push_back({ &item, 0 });
        break;
    case MenuItemType::ColorSubmenu:
        g_inColorSubmenu = true;
        break;
    case MenuItemType::Toggle:
        if (item.toggleValue) *item.toggleValue = !*item.toggleValue;
        break;
    case MenuItemType::Action:
        if (item.onAction) item.onAction();
        break;
    case MenuItemType::Slider:
        break;
    }
}

void RenderColorCustomizationSubmenu() {
    if (g_config.titlePaddingX != 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + g_config.titlePaddingX);

    ImGui::TextColored(ImVec4(g_config.accentColor[0], g_config.accentColor[1], g_config.accentColor[2], 1.0f),
        "=== VISUAL PALETTE CUSTOMIZER ===");
    ImGui::Separator();
    ImGui::Spacing();

    auto pad = []() {
        if (g_config.submenuItemPaddingX != 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + g_config.submenuItemPaddingX);
        };

    pad(); if (ImGui::ColorEdit4("Background Color", g_config.customBgColor)) ApplyThemeColors();
    pad(); if (ImGui::SliderFloat("Background Opacity", &g_config.bgAlpha, 0.1f, 1.0f, "Alpha: %.2f")) ApplyThemeColors();
    ImGui::Spacing();
    pad(); if (ImGui::ColorEdit4("Text Color", g_config.textColor)) ApplyThemeColors();
    pad(); if (ImGui::ColorEdit4("Button Color", g_config.buttonColor)) ApplyThemeColors();
    pad(); if (ImGui::ColorEdit4("Slider Grab Color", g_config.sliderColor)) ApplyThemeColors();
    pad(); if (ImGui::ColorEdit4("Accent Border Color", g_config.accentColor)) ApplyThemeColors();

    ImGui::Spacing();
    ImGui::Separator();
    pad();
    if (CustomButton("< Return to Menu Settings", ImVec2(250, 35)))
        g_inColorSubmenu = false;
}

void RenderMenuSettingsPanel() {
    if (g_inColorSubmenu) {
        RenderColorCustomizationSubmenu();
        return;
    }

    if (g_config.titlePaddingX != 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + g_config.titlePaddingX);

    ImGui::TextColored(ImVec4(g_config.accentColor[0], g_config.accentColor[1], g_config.accentColor[2], 1.0f),
        "=== UI & MENU CUSTOMIZATION ===");
    ImGui::Separator();
    ImGui::Spacing();

    auto pad = []() {
        if (g_config.submenuItemPaddingX != 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + g_config.submenuItemPaddingX);
        };

    pad(); ImGui::Text("Global UI Scale");
    pad(); if (ImGui::SliderFloat("##GlobalScale", &g_config.globalScale, 0.1f, 50.0f, "Scale: %.2fx")) ApplyThemeColors();

    ImGui::Spacing();
    pad(); ImGui::Text("Button & Control Element Sizing");
    pad(); if (ImGui::SliderFloat("Button Padding X", &g_config.buttonPaddingX, 2.0f, 50.0f, "Pad X: %.1f px")) ApplyThemeColors();
    pad(); if (ImGui::SliderFloat("Button Padding Y", &g_config.buttonPaddingY, 2.0f, 40.0f, "Pad Y: %.1f px")) ApplyThemeColors();
    pad(); ImGui::SliderFloat("Button Width Scale Multiplier", &g_config.buttonWidthScale, 0.1f, 10.0f, "Width Scale: %.2fx");
    pad(); ImGui::SliderFloat("Button Height Scale Multiplier", &g_config.buttonHeightScale, 0.1f, 10.0f, "Height Scale: %.2fx");

    ImGui::Spacing();
    pad(); ImGui::Text("Menu Box Dimensions & Content Offset");
    pad(); ImGui::SliderFloat("Width", &g_config.windowWidth, 300.0f, 5000.0f, "Width: %.0f px");
    pad(); ImGui::SliderFloat("Height", &g_config.windowHeight, 300.0f, 5000.0f, "Height: %.0f px");
    pad(); ImGui::SliderFloat("Breadcrumb Horizontal Padding", &g_config.titlePaddingX, -200.0f, 500.0f, "Offset: %.1f px");
    pad(); ImGui::SliderFloat("Submenu Items Horizontal Padding", &g_config.submenuItemPaddingX, -200.0f, 500.0f, "Item Offset: %.1f px");

    ImGui::Spacing();
    pad(); ImGui::Text("Title Bar Padding (the top strip itself)");
    pad(); ImGui::SliderFloat("Title Bar Padding X", &g_config.titleBarPaddingX, 0.0f, 100.0f, "Pad X: %.1f px");
    pad(); ImGui::SliderFloat("Title Bar Padding Y", &g_config.titleBarPaddingY, 0.0f, 60.0f, "Pad Y: %.1f px");

    ImGui::Spacing();
    pad(); ImGui::Text("Font Resizing Engine (Global Scale)");
    pad();
    const char* fonts[] = { "Standard Default (1.0x)", "Large High-Visibility (1.5x)", "Ultra Scaling (2.0x)" };
    if (ImGui::Combo("Font Mode Preset", &g_config.fontChoice, fonts, 3)) {
        if (g_config.fontChoice == 0) g_config.fontScaleFactor = 1.0f;
        else if (g_config.fontChoice == 1) g_config.fontScaleFactor = 1.5f;
        else if (g_config.fontChoice == 2) g_config.fontScaleFactor = 2.0f;
    }
    pad();
    if (ImGui::SliderFloat("Fine Font Scale Multiplier", &g_config.fontScaleFactor, 0.1f, 10.0f, "Multiplier: %.2fx"))
        ImGui::GetIO().FontGlobalScale = g_config.fontScaleFactor;

    ImGui::Spacing();
    pad(); ImGui::Text("Color Theme Presets");
    pad();
    const char* themes[] = { "Starfield Dark", "Classic Cyber", "Custom Palette" };
    int currentTheme = g_config.colorTheme;
    if (ImGui::Combo("Theme Preset", &currentTheme, themes, 3)) {
        g_config.colorTheme = currentTheme;
        ApplyThemeColors();
    }

    ImGui::Spacing();
    pad();
    if (CustomButton("Open Color & Sub-Theme Customizer...", ImVec2(320, 40)))
        g_inColorSubmenu = true;

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    pad();
    if (CustomButton("Save Settings to Disk", ImVec2(240, 45)))
        SaveConfig();

    ImGui::SameLine();
    if (CustomButton("Reset Defaults", ImVec2(150, 45))) {
        g_config.globalScale = 1.43f;
        g_config.windowWidth = 1842.0f;
        g_config.windowHeight = 1550.0f;
        g_config.colorTheme = 0;
        g_config.bgAlpha = 0.94f;
        g_config.fontChoice = 0;
        g_config.fontScaleFactor = 2.50f;
        g_config.buttonPaddingX = 25.0f;
        g_config.buttonPaddingY = 10.0f;
        g_config.buttonWidthScale = 2.83f;
        g_config.buttonHeightScale = 1.78f;
        g_config.titlePaddingX = 21.0f;
        g_config.submenuItemPaddingX = 22.9f;
        g_config.titleBarPaddingX = 16.0f;
        g_config.titleBarPaddingY = 10.0f;
        ImGui::GetIO().FontGlobalScale = g_config.fontScaleFactor;
        ApplyThemeColors();
    }

    ImGui::Spacing();
    pad();
    if (CustomButton("< Back to Index", ImVec2(200, 35)))
        g_navStack.pop_back();
}

void RenderCustomMenu() {
    if (!g_menuBuilt) {
        LoadConfig();
        ApplyThemeColors();
        ImGui::GetIO().FontGlobalScale = g_config.fontScaleFactor;
        g_rootMenu = BuildMenu();
        g_navStack.push_back({ &g_rootMenu, 0 });
        g_menuBuilt = true;
    }

    MenuNavLevel& level = g_navStack.back();
    MenuItem& currentCategory = *level.item;

    if (currentCategory.type == MenuItemType::CustomSettings) {
        RenderMenuSettingsPanel();
        return;
    }

    int itemCount = (int)currentCategory.children.size();
    if (itemCount == 0) itemCount = 1;

    // Breadcrumb
    {
        if (g_config.titlePaddingX != 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + g_config.titlePaddingX);

        std::string path;
        for (size_t i = 0; i < g_navStack.size(); i++) {
            if (i > 0) path += " > ";
            path += g_navStack[i].item->label;
        }
        ImGui::TextColored(ImVec4(g_config.accentColor[0], g_config.accentColor[1], g_config.accentColor[2], 1.0f),
            "%s", path.c_str());
    }
    ImGui::Separator();
    ImGui::Spacing();

    if (!currentCategory.children.empty()) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            level.selectedIndex = (level.selectedIndex + 1) % itemCount;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            level.selectedIndex = (level.selectedIndex - 1 + itemCount) % itemCount;
    }

    float rowHeight = ComputeMenuRowHeight();

    // Back row
    if (g_navStack.size() > 1) {
        if (g_config.submenuItemPaddingX != 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + g_config.submenuItemPaddingX);

        bool backSelected = (level.selectedIndex == -1);
        if (ImGui::Selectable("<  Back", backSelected, 0, ImVec2(0, rowHeight))) {
            g_navStack.pop_back();
            return;
        }
        ImGui::Spacing();
    }

    for (int i = 0; i < (int)currentCategory.children.size(); i++) {
        MenuItem& item = currentCategory.children[i];
        bool isHighlighted = (i == level.selectedIndex);
        ImGui::PushID(i);

        if (g_config.submenuItemPaddingX != 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + g_config.submenuItemPaddingX);

        std::string displayLabel = item.label;
        if (item.type == MenuItemType::Toggle)
            displayLabel += (item.toggleValue && *item.toggleValue) ? "    [ON]" : "    [OFF]";
        else if (item.type == MenuItemType::Slider) {
            char buf[64];
            snprintf(buf, sizeof(buf), "    [%.2f]", item.sliderValue ? *item.sliderValue : 0.0f);
            displayLabel += buf;
        }
        else if (item.type == MenuItemType::Category || item.type == MenuItemType::CustomSettings)
            displayLabel += "    >";

        bool clicked = ImGui::Selectable(displayLabel.c_str(), isHighlighted, 0, ImVec2(0, rowHeight));
        if (clicked) {
            level.selectedIndex = i;
            ActivateItem(item);
            ImGui::PopID();
            break;
        }

        if (isHighlighted && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
            ActivateItem(item);
            ImGui::PopID();
            break;
        }

        if (isHighlighted && item.type == MenuItemType::Slider) {
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && item.sliderValue) {
                *item.sliderValue += item.sliderStep;
                if (*item.sliderValue > item.sliderMax) *item.sliderValue = item.sliderMax;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && item.sliderValue) {
                *item.sliderValue -= item.sliderStep;
                if (*item.sliderValue < item.sliderMin) *item.sliderValue = item.sliderMin;
            }
        }
        else if (isHighlighted && (item.type == MenuItemType::Category || item.type == MenuItemType::CustomSettings)) {
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                ActivateItem(item);
                ImGui::PopID();
                break;
            }
        }

        ImGui::PopID();
    }

    if (g_navStack.size() > 1 && (ImGui::IsKeyPressed(ImGuiKey_Backspace) || ImGui::IsKeyPressed(ImGuiKey_Escape)))
        g_navStack.pop_back();

    ImGui::Spacing();
    ImGui::Separator();
    if (g_config.submenuItemPaddingX != 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + g_config.submenuItemPaddingX);
    ImGui::TextDisabled("Up/Down: Navigate | Enter: Select | Left/Right: Adjust Values | Backspace: Back");
}

// ---------------------------------------------------------------------------
// Custom title bar
// ---------------------------------------------------------------------------
void DrawCustomTitleBar(const char* title) {
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGuiStyle& style = ImGui::GetStyle();

    float barHeight = ImGui::GetFontSize() + g_config.titleBarPaddingY * 2.0f;
    ImVec2 barMin = winPos;
    ImVec2 barMax = ImVec2(winPos.x + winSize.x, winPos.y + barHeight);

    ImU32 barColor = ImGui::GetColorU32(ImGuiCol_TitleBgActive);
    drawList->AddRectFilled(barMin, barMax, barColor, style.WindowRounding, ImDrawFlags_RoundCornersTop);

    ImVec2 textPos = ImVec2(barMin.x + g_config.titleBarPaddingX, barMin.y + g_config.titleBarPaddingY);
    drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), title);

    // Close button
    float closeSize = barHeight * 0.5f;
    ImVec2 closeMin = ImVec2(barMax.x - closeSize - g_config.titleBarPaddingX,
        barMin.y + (barHeight - closeSize) * 0.5f);

    ImGui::SetCursorScreenPos(closeMin);
    ImGui::PushID("##CustomTitleBarClose");
    ImGui::InvisibleButton("close", ImVec2(closeSize, closeSize));
    bool closeHovered = ImGui::IsItemHovered();
    bool closeClicked = ImGui::IsItemClicked();
    ImGui::PopID();

    ImU32 xColor = closeHovered ? IM_COL32(255, 90, 90, 255) : ImGui::GetColorU32(ImGuiCol_Text);
    drawList->AddLine(closeMin, ImVec2(closeMin.x + closeSize, closeMin.y + closeSize), xColor, 2.0f);
    drawList->AddLine(ImVec2(closeMin.x, closeMin.y + closeSize),
        ImVec2(closeMin.x + closeSize, closeMin.y), xColor, 2.0f);

    if (closeClicked)
        showMenu = false;

    // Drag region
    float dragWidth = winSize.x - closeSize - g_config.titleBarPaddingX * 2.0f;
    if (dragWidth > 0.0f) {
        ImGui::SetCursorScreenPos(barMin);
        ImGui::PushID("##CustomTitleBarDrag");
        ImGui::InvisibleButton("drag", ImVec2(dragWidth, barHeight));
        ImGui::PopID();

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            ImGui::SetWindowPos(ImVec2(winPos.x + delta.x, winPos.y + delta.y));
        }
    }

    // Move content cursor below the bar
    ImGui::SetCursorScreenPos(ImVec2(winPos.x + style.WindowPadding.x, barMax.y + style.WindowPadding.y));
}

// ---------------------------------------------------------------------------
// Render target helpers
// ---------------------------------------------------------------------------
void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

void CreateRenderTarget() {
    if (!g_pSwapChain || !g_pd3dDevice)
        return;

    CleanupRenderTarget();

    ID3D11Texture2D* pBackBuffer = nullptr;
    if (SUCCEEDED(g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer))) {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_UNKNOWN;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, &rtvDesc, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

// ---------------------------------------------------------------------------
// WndProc
// ---------------------------------------------------------------------------
LRESULT CALLBACK hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT)
        showMenu = !showMenu;

    if (showMenu && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// ResizeBuffers (critical for CryEngine / Homefront)
// ---------------------------------------------------------------------------
HRESULT APIENTRY hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    if (g_init) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        CleanupRenderTarget();
    }

    HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

    if (g_init && SUCCEEDED(hr)) {
        CreateRenderTarget();
        ImGui_ImplDX11_CreateDeviceObjects();
    }

    return hr;
}

// ---------------------------------------------------------------------------
// Present
// ---------------------------------------------------------------------------
HRESULT APIENTRY hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_init) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice))) {
            g_pd3dDevice->GetImmediateContext(&g_pd3dContext);
            g_pSwapChain = pSwapChain;

            DXGI_SWAP_CHAIN_DESC desc = {};
            pSwapChain->GetDesc(&desc);
            g_hWnd = desc.OutputWindow;

            if (g_hWnd) {
                oWndProc = (WNDPROC)SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

                ImGui::CreateContext();
                LoadConfig();
                ApplyThemeColors();
                ImGui::GetIO().FontGlobalScale = g_config.fontScaleFactor;

                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

                ImGui_ImplWin32_Init(g_hWnd);
                ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

                CreateRenderTarget();
                g_init = true;
            }
        }
    }

    if (!g_init || !g_mainRenderTargetView)
        return oPresent(pSwapChain, SyncInterval, Flags);

    // Start ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::GetIO().FontGlobalScale = g_config.fontScaleFactor;

    if (showMenu) {
        ImGui::SetNextWindowSize(ImVec2(g_config.windowWidth, g_config.windowHeight), ImGuiCond_Always);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
        ImGui::Begin("Project Unity | DX11GameHookz", nullptr, flags);
        DrawCustomTitleBar("Project Unity | DX11GameHookz");
        RenderCustomMenu();
        ImGui::End();
    }

    ImGui::Render();

    // Save current RTVs (CryEngine is sensitive)
    ID3D11RenderTargetView* oldRTV = nullptr;
    ID3D11DepthStencilView* oldDSV = nullptr;
    g_pd3dContext->OMGetRenderTargets(1, &oldRTV, &oldDSV);

    g_pd3dContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Restore
    g_pd3dContext->OMSetRenderTargets(1, &oldRTV, oldDSV);
    if (oldRTV) oldRTV->Release();
    if (oldDSV) oldDSV->Release();

    return oPresent(pSwapChain, SyncInterval, Flags);
}

// ---------------------------------------------------------------------------
// Main thread
// ---------------------------------------------------------------------------
DWORD WINAPI MainThread(LPVOID) {
    while (!GetModuleHandleA("dxgi.dll"))
        Sleep(50);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, DefWindowProcW, 0, 0,
                       GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                       L"HFTR_Dummy", nullptr };
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.Width = 2;
    sd.BufferDesc.Height = 2;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ID3D11Device* pDev = nullptr;
    ID3D11DeviceContext* pCtx = nullptr;
    IDXGISwapChain* pSC = nullptr;
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;

    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        &fl, 1, D3D11_SDK_VERSION, &sd, &pSC, &pDev, nullptr, &pCtx)))
    {
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    void** vtable = *reinterpret_cast<void***>(pSC);
    void* pPresent = vtable[8];   // Present
    void* pResize = vtable[13];  // ResizeBuffers

    pSC->Release();
    pCtx->Release();
    pDev->Release();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    if (MH_Initialize() != MH_OK)
        return 1;

    MH_CreateHook(pPresent, &hkPresent, reinterpret_cast<void**>(&oPresent));
    MH_CreateHook(pResize, &hkResizeBuffers, reinterpret_cast<void**>(&oResizeBuffers));
    MH_EnableHook(MH_ALL_HOOKS);

    return 0;
}

// ---------------------------------------------------------------------------
// DLL entry
// ---------------------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        if (g_init) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            CleanupRenderTarget();
            if (g_pd3dContext) { g_pd3dContext->Release(); g_pd3dContext = nullptr; }
            if (g_pd3dDevice) { g_pd3dDevice->Release();  g_pd3dDevice = nullptr; }
        }
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }
    return TRUE;
}
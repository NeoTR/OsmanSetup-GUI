#include <iostream>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <glad/glad.h>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem> 
#include <Windows.h>
#include <shlobj.h> 
#include <cstdlib>
#include <powrprof.h>
#include <tchar.h>
#include <cstdint>
#include <winreg.h>
#include <initguid.h>
#include <GPEdit.h>
#include <array>
#include <algorithm>
#include <cctype>
#include <future>
#include "../vendor/imgui_impl_glfw.h"
#include "../vendor/imgui_impl_opengl3.h"

#pragma comment(lib, "Powrprof.lib")
#pragma comment(lib, "Shell32.lib") 

#define GLSL_VERSION "#version 330"

struct App {
    std::string cmd;
    std::string name;
    bool shouldSetup; 
};

std::vector<App> apps = {
    {"powershell -Command \"iwr -useb https://raw.githubusercontent.com/spicetify/spicetify-marketplace/main/resources/install.ps1 | iex\"", "Spicetify", false},
    {"https://github.com/Vencord/Installer/releases/latest/download/VencordInstallerCli.exe", "VenCord", false},
};

void installFont(const std::string & fontURL, const std::string & outputPath) {
    std::string curlCommand = "curl -L -o \"" + outputPath + "\" \"" + fontURL + "\"";
    if (std::system(curlCommand.c_str()) != 0) {
        std::cerr << "Failed to download font file." << std::endl;
        return;
    }
}

void installFontsAsync(const std::vector<std::pair<std::string, std::string>>& fontDownloads) {
    for (const auto& font : fontDownloads) {
        std::async(std::launch::async, installFont, font.first, font.second);
    }
}

std::string executeCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::string trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(), ::isspace);
    auto end = std::find_if_not(str.rbegin(), str.rend(), ::isspace).base();
    return (start < end) ? std::string(start, end) : std::string();
}

void listRegistryKey(HKEY hkey, LPCTSTR subkey) {
    HKEY keyHandle;
    LONG result = RegOpenKeyEx(hkey, subkey, 0, KEY_READ, &keyHandle);
    if (result == ERROR_SUCCESS) {
        TCHAR valueName[256];
        DWORD valueNameSize, valueType;
        BYTE valueData[1024];
        DWORD valueDataSize = sizeof(valueData);
        DWORD i = 0;

        while (RegEnumValue(keyHandle, i, valueName, &(valueNameSize = sizeof(valueName)), NULL, &valueType, valueData, &valueDataSize) == ERROR_SUCCESS) {
            ImGui::Text("%s", valueName);
            i++;
        }

        RegCloseKey(keyHandle);

    }
    else {
        std::cerr << "Failed to open registry key." << std::endl;
    }
}

void listStartupFolder(int csidl) {
    TCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, csidl, NULL, 0, path))) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            std::wcout << entry.path().filename().wstring() << std::endl;
        }
    }
    else {
        std::cerr << "Failed to get Startup folder path." << std::endl;
    }
}

void listStartupPrograms() {
    listRegistryKey(HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"));
    listRegistryKey(HKEY_LOCAL_MACHINE, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"));
}

void listFilesInDirectory(const std::string& path, WORD color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_directory()) {
            std::wcout << "    " << entry.path().filename() << std::endl;
        }
    }

    SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
}

void executeFile(const std::wstring& filePath) {
    ShellExecuteW(NULL, L"open", filePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void installPrograms(const std::string& selectedPath) {
    std::string downloadsPath = selectedPath;
    listFilesInDirectory(downloadsPath, FOREGROUND_GREEN);
    for (const auto& entry : std::filesystem::directory_iterator(downloadsPath)) {
        if (!entry.is_directory()) {
            executeFile(entry.path());
        }
    }
}

void setupProgram(const std::string& selectedPath) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    for (const auto& app : apps) {
        if (app.shouldSetup) {
            // Spicetify
            if (app.name == "Spicetify") {
                std::string powershellCommand = "powershell -Command \"iwr -useb https://raw.githubusercontent.com/spicetify/spicetify-marketplace/main/resources/install.ps1 | iex\"";
                std::system(powershellCommand.c_str());
            }

            // VenCord
            if (app.name == "VenCord") {
                PWSTR appDataPath;
                SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataPath);
                std::wstring appDataPathW(appDataPath);
                std::string appDataPathA(appDataPathW.begin(), appDataPathW.end());
                CoTaskMemFree(appDataPath);

                std::string osmanSetupPath = appDataPathA + "\\OsmanSetup";
                std::string downloadsPath = osmanSetupPath + "\\Downloads";
                std::filesystem::create_directories(downloadsPath);

                std::string vencordURL = "https://github.com/Vencord/Installer/releases/latest/download/VencordInstallerCli.exe";
                std::string curlCommand = "curl -L -o " + downloadsPath + "\\VencordInstallerCli.exe " + vencordURL;
                std::system(curlCommand.c_str());

                std::string vencordPath = downloadsPath + "\\VencordInstallerCli.exe";
                executeFile(std::wstring(vencordPath.begin(), vencordPath.end()));
            }
        }
    }
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(600, 800, "Hello, ImGui!", nullptr, nullptr);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(GLSL_VERSION);

    PWSTR appDataPath;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataPath))) {
        std::cerr << "Failed to get AppData path." << std::endl;
        return -1;
    }

    std::wstring appDataPathW(appDataPath);
    std::string appDataPathA(appDataPathW.begin(), appDataPathW.end());
    CoTaskMemFree(appDataPath);

    std::string downloadsPath = appDataPathA + "\\OsmanSetup\\Downloads";
    std::error_code ec;
    if (!std::filesystem::create_directories(downloadsPath, ec) && ec) {
        std::cerr << "Failed to create directory: " << ec.message() << std::endl;
        return -1;
    }

    std::vector<std::pair<std::string, std::string>> fontDownloads = {
        {"https://cdn.discordapp.com/attachments/1109803908685107263/1213461443823935548/font.ttf?ex=65f58f09&is=65e31a09&hm=a9c3a6953ac6c5ada2cac08cd542b8ddffdc1a0d32989b1b35b1cc97c76ac7fc&", "font.ttf"}
    };

    installFontsAsync(fontDownloads);

    ImFont* font = io.Fonts->AddFontFromFileTTF((downloadsPath + "\\font.ttf").c_str(), 18.0f);
    if (font == nullptr) {
        std::cerr << "Failed to load font file 'font.ttf'" << std::endl;
        return -1;
    }

    if (font == nullptr) {
        std::cout << "Failed to load font file 'font.ttf'" << std::endl;
        return -1;
    }

    io.FontDefault = font;

    int currentTab = 0;
    const char* tabNames[] = { "Install", "Setup", "Optimizations", "Find Drivers" };
    bool tabContents[] = { true, false, false, false };

    char pathstr[100] = "C:/";
    std::string powerPlanName, powerPlan;
    size_t start, end;

    std::string systemInfo = executeCommand("wmic cpu get name && wmic path win32_videocontroller get name && wmic baseboard get product");


    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        ImGui::StyleColorsDark();
        ImVec4 borderColor = ImVec4(0.8f, 0.1f, 0.1f, 1.0f);

        ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

        style.FrameBorderSize = 1.0f;
        style.FrameRounding = 5.0f;
        ImGui::PushStyleColor(ImGuiCol_Border, borderColor);

        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.98f);
        colors[ImGuiCol_Border] = ImVec4(1.00f, 0.00f, 0.00f, 0.50f);
        colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.10f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 0.00f, 0.00f, 0.18f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.98f, 0.26f, 0.26f, 0.09f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.82f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(1.00f, 0.00f, 0.00f, 0.40f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 0.00f, 0.00f, 0.64f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.98f, 0.06f, 0.06f, 0.62f);

        style.TabBorderSize = 1.0f; // Border size for tabs
        style.TabRounding = 5.0f; // Rounding for tabs
        style.TabMinWidthForCloseButton = -1.0f; // Disable close buttons

        colors[ImGuiCol_Tab] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.7f, 0.0f, 0.0f, 1.0f); 
        colors[ImGuiCol_TabActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);




        ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_Always);
        ImGui::Begin("Window", nullptr, ImGuiWindowFlags_NoDecoration);

        {
            ImGui::SetWindowFontScale(1.2f);

            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "OsmanSetup");
            ImGui::SameLine(ImGui::GetWindowWidth() - 50);

            ImGui::SetWindowFontScale(1.0f);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.f, 0.f, 0.f, 0.f));

            ImGui::SetWindowFontScale(1.2f);

            if (ImGui::Button("X", ImVec2(30, 30))) {
                glfwSetWindowShouldClose(window, true);
            }

            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor(4);


            ImGui::BeginTabBar("TabBar");
            for (int i = 0; i < IM_ARRAYSIZE(tabNames); i++) {
                if (ImGui::BeginTabItem(tabNames[i])) {
                    currentTab = i;
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();

            switch (currentTab) {
            case 0:
                ImGui::Spacing();
                ImGui::Text("Select the directory:");
                ImGui::SameLine();
                if (ImGui::Button("Select")) {
                    BROWSEINFO bi = { 0 };
                    bi.lpszTitle = ("Select a folder");
                    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
                    if (pidl != 0) {
                        SHGetPathFromIDList(pidl, pathstr);
                        IMalloc* imalloc = 0;
                        if (SUCCEEDED(SHGetMalloc(&imalloc))) {
                            imalloc->Free(pidl);
                            imalloc->Release();
                        }
                    }
                }

                ImGui::Text("Selected Directory: ");
                ImGui::SameLine();
                ImGui::Text(pathstr);
                ImGui::Separator();

                ImGui::Text("Programs to install:");

                ImGui::SetWindowFontScale(0.8f);
                for (const auto& entry : std::filesystem::directory_iterator(pathstr)) {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),entry.path().filename().string().c_str());
                }
                ImGui::SetWindowFontScale(1.0f);



                ImGui::Separator();
                if (ImGui::Button("Install")) {
                    std::string path = pathstr;
                    if (path == "C:/") {
                        MessageBox(NULL, "Please select a directory", "Error", MB_OK | MB_ICONERROR);
                    }
                    else {
                        installPrograms(path);
                    }

                }
                break;
            case 1:
                ImGui::Spacing();
                ImGui::Text("Programs to get setup:");
                for (int i = 0; i < apps.size(); i++) {
                    ImGui::SetWindowFontScale(0.8f);
					ImGui::Text(apps[i].name.c_str());
                    ImGui::SetWindowFontScale(1.0f);
                    ImGui::SameLine(ImGui::GetWindowWidth() - 50);
                    ImGui::SetWindowFontScale(0.6f);
                    ImGui::Checkbox(("##" + std::to_string(i)).c_str(), &apps[i].shouldSetup);
				}
                ImGui::SetWindowFontScale(1.0f);
                ImGui::Separator();
                if (ImGui::Button("Setup")) {
					setupProgram(pathstr);
				}



                break;
            case 2:
                // Power Plan
                ImGui::Text("Power Plan:");
                if (ImGui::Button("Ultimate Performance")) {
                    std::system("powercfg /SETACTIVE e08daac4-7f48-4819-97cf-ce5c27a203df");
                }
                if (ImGui::Button("High Performance")) {
                    std::system("powercfg -s 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c");
				}
                ImGui::SameLine();
                if (ImGui::Button("Balanced")) {
                    std::system("powercfg -s 381b4222-f694-41f0-9685-ff5bb260df2e");
                }
                ImGui::SameLine();
                if (ImGui::Button("Power Saver")) {
                    std::system("powercfg -s a1841308-3541-4fab-bc81-f71556f20b4a");
				}
                ImGui::Dummy(ImVec2(0.0f, 6.0f));
                ImGui::Text("Current Power Plan:");
                ImGui::SameLine();

                if (ImGui::Button("Get")) {
                    powerPlan = executeCommand("powercfg /GETACTIVESCHEME");
                }

                powerPlan = powerPlan.substr(0, powerPlan.find("\n"));
                start = powerPlan.find("(");
                end = powerPlan.find(")");
                powerPlanName = powerPlan.substr(start + 1, end - start - 1);
                ImGui::Text(powerPlanName.c_str());




                ImGui::Separator();

                // Windows Activation
                ImGui::Text("Windows Activation:");
                if (ImGui::Button("Activate Windows")) {
                    ShellExecute(NULL, "runas", "powershell", "irm https://massgrave.dev/get | iex", NULL, SW_SHOW);
                }
                ImGui::SetWindowFontScale(0.8f);
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Credits to massgravel");
				ImGui::SetWindowFontScale(1.0f);

                break;
            case 3:
                // Get Drivers
                ImGui::Text("CPU, GPU and Motherboard Specs:");

                ImGui::Separator();

                ImGui::Text(systemInfo.c_str());
                ImGui::Separator();

                if (systemInfo.find("Intel") != std::string::npos) {
                    if (ImGui::Button("Open Intel Driver Site")) {
                        ShellExecute(NULL, "open", "https://downloadcenter.intel.com/", NULL, NULL, SW_SHOWNORMAL);
                    }
                }

                if (systemInfo.find("AMD") != std::string::npos) {
                    if (ImGui::Button("Open AMD Driver Site")) {
                        ShellExecute(NULL, "open", "https://www.amd.com/en/support", NULL, NULL, SW_SHOWNORMAL);
                    }
                }

                if (systemInfo.find("NVIDIA") != std::string::npos) {
                    if (ImGui::Button("Open NVIDIA Driver Site")) {
                        ShellExecute(NULL, "open", "https://www.nvidia.com/Download/index.aspx", NULL, NULL, SW_SHOWNORMAL);
                    }
                }

                // Motherboard
                if (systemInfo.find("ASUS") != std::string::npos) {
                    if (ImGui::Button("Open ASUS Driver Site")) {
                        ShellExecute(NULL, "open", "https://www.asus.com/support/Download-Center/", NULL, NULL, SW_SHOWNORMAL);
                    }
                }

                if (systemInfo.find("Gigabyte") != std::string::npos) {
                    if (ImGui::Button("Open Gigabyte Driver Site")) {
                        ShellExecute(NULL, "open", "https://www.gigabyte.com/Support", NULL, NULL, SW_SHOWNORMAL);
                    }
                }

                if (systemInfo.find("MSI") != std::string::npos) {
                    if (ImGui::Button("Open MSI Driver Site")) {
                        ShellExecute(NULL, "open", "https://www.msi.com/support/download", NULL, NULL, SW_SHOWNORMAL);
                    }
                }
                break;
            }

            ImGui::End();
        }
        ImGui::PopStyleColor(); // Restore border color

        // Render
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Transparent background
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

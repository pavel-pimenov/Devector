#include <vector>
#include <string>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include "GLFW/glfw3.h"
#include "DevectorApp.h"
#include "Utils/Utils.h"
#include "Utils/JsonUtils.h"
#include "Utils/StrUtils.h"

dev::DevectorApp::DevectorApp(
	const std::string& _stringPath, nlohmann::json _settingsJ)
	:
	ImGuiApp(_settingsJ, _stringPath, APP_NAME),
	m_glUtils()
{
	SettingsInit();
	WindowsInit();
}

void dev::DevectorApp::WindowsInit()
{
	std::wstring pathBootData = dev::StrToStrW(GetSettingsString("bootPath", ""));

	m_hardwareP = std::make_unique < dev::Hardware>(pathBootData);
	m_debuggerP = std::make_unique < dev::Debugger>(*m_hardwareP);
	m_hardwareStatsWindowP = std::make_unique<dev::HardwareStatsWindow>(*m_hardwareP, &m_fontSize, &m_dpiScale, m_reqHardwareStatsReset);
	m_disasmWindowP = std::make_unique<dev::DisasmWindow>(*m_hardwareP, *m_debuggerP, 
		m_fontItalic, &m_fontSize, &m_dpiScale, m_reqDisasm, m_reqHardwareStatsReset, m_reqMainWindowReload);
	m_displayWindowP = std::make_unique<dev::DisplayWindow>(*m_hardwareP, &m_fontSize, &m_dpiScale, m_glUtils);
	m_breakpointsWindowP = std::make_unique<dev::BreakpointsWindow>(*m_debuggerP, &m_fontSize, &m_dpiScale, m_reqDisasm);
	m_watchpointsWindowP = std::make_unique<dev::WatchpointsWindow>(*m_debuggerP, &m_fontSize, &m_dpiScale, m_reqHexViewer);
	m_memDisplayWindowP = std::make_unique<dev::MemDisplayWindow>(*m_hardwareP, *m_debuggerP, &m_fontSize, &m_dpiScale, m_glUtils, m_reqHexViewer);
	m_hexViewerWindowP = std::make_unique<dev::HexViewerWindow>(*m_hardwareP, *m_debuggerP, &m_fontSize, &m_dpiScale, m_reqHexViewer);
	m_traceLogWindowP = std::make_unique<dev::TraceLogWindow>(*m_hardwareP, *m_debuggerP, &m_fontSize, &m_dpiScale, m_reqDisasm);

	// Set the key callback function
	glfwSetWindowUserPointer(m_window, this);
	ImGui_ImplGlfw_KeyCallback = glfwSetKeyCallback(m_window, DevectorApp::KeyHandling);
}

void dev::DevectorApp::SettingsInit()
{
	Request(REQ::LOAD_FONT);
	AppStyleInit();
	RecentFilesInit();
}

// Function to open a file dialog
bool OpenFileDialog(WCHAR* filePath, int size)
{
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = filePath;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = size;
	ofn.lpstrFilter = L"All Files\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	// Display the open file dialog
	if (GetOpenFileName(&ofn) == TRUE)
		return true; // User selected a file
	else
		return false; // User cancelled or an error occurred
}

void dev::DevectorApp::Update()
{
	if (m_reqMainWindowReload) {
		m_reqMainWindowReload = false;
		Reload();
	}
	MainMenuUpdate();

	m_hardwareStatsWindowP->Update();
	m_disasmWindowP->Update();
	m_displayWindowP->Update();
	m_breakpointsWindowP->Update();
	m_watchpointsWindowP->Update();
	m_memDisplayWindowP->Update();
	m_hexViewerWindowP->Update();
	m_traceLogWindowP->Update();
}

void dev::DevectorApp::LoadRom(const std::wstring& _path)
{
	if (!IsFileExist(_path)) {
		dev::Log(L"File not found: {}", _path);
		return;
	}

	auto result = dev::LoadFile(_path);
	if (!result || result->empty()) {
		dev::Log(L"Error occurred while loading the file. Path: {}. "
			"Please ensure the file exists and you have the correct permissions to read it.", _path);
		return;
	}

	m_hardwareP->Request(Hardware::Req::STOP);
	m_hardwareP->Request(Hardware::Req::RESET);

	m_debuggerP->LoadDebugData(_path);

	auto reqData = nlohmann::json({ {"data", *result}, {"addr", Memory::ROM_LOAD_ADDR} });
	m_hardwareP->Request(Hardware::Req::SET_MEM, reqData);

	m_debuggerP->Reset();
	m_hardwareP->Request(Hardware::Req::RUN);
	
	RecentFilesUpdate(_path);
	RecentFilesStore();

	Log(L"File loaded: {}", _path);
}

void dev::DevectorApp::LoadFdd(const std::wstring& _path, const int _driveIdx, const bool _autoBoot)
{
	if (!IsFileExist(_path)) {
		dev::Log(L"File not found: {}", _path);
		return;
	}

	auto fddResult = dev::LoadFile(_path);
	if (!fddResult || fddResult->empty()) {
		dev::Log(L"Error occurred while loading the file. Path: {}. "
			"Please ensure the file exists and you have the correct permissions to read it.", _path);
		return;
	}

	if (_autoBoot)
	{
		/*
		// loading the loader
		auto bootPath = dev::StrToStrW(GetSettingsString("bootPath", ""));
		if (bootPath.empty()) {
			Log("bootPath field not found in the settings");
			return;
		}
		auto bootResult = dev::LoadFile(bootPath);
		if (!bootResult || bootResult->empty()) {
			dev::Log(L"Error occurred while loading the boot file. Path: {}. "
				"Please ensure the file exists and you have the correct permissions to read it.", bootPath);
			return;
		}
		*/
		m_hardwareP->Request(Hardware::Req::STOP);
		//m_hardwareP->Request(Hardware::Req::RESET);

		//auto reqData = nlohmann::json({ {"data", *bootResult}, {"addr", 0} });
	}

	// loading the fdd data
	//m_hardwareP->Request(Hardware::Req::LOAD_FDD, { {"data", *fddResult }, {"driveIdx", _driveIdx}});
	m_hardwareP->Request(Hardware::Req::LOAD_FDD, { {"path", dev::StrWToStr(_path) }, {"driveIdx", _driveIdx} });

	if (_autoBoot)
	{
		m_debuggerP->Reset();
		m_hardwareP->Request(Hardware::Req::RESET);
	}

	m_hardwareP->Request(Hardware::Req::RUN); // because it can be stopped by the ::Reload

	RecentFilesUpdate(_path, _driveIdx, _autoBoot);
	RecentFilesStore();

	Log(L"File loaded: {}", _path);
}

void dev::DevectorApp::Reload()
{
	if (m_recentFilePaths.empty()) return;
	// get latest recent path
	const auto& [path, _driveIdx, _autoBoot] = m_recentFilePaths.front();

	if (_driveIdx < 0) LoadRom(path);
	else LoadFdd(path, _driveIdx, _autoBoot);
}

void dev::DevectorApp::MainMenuUpdate()
{
	static std::wstring fddPath = L"";
	bool openFddPopup = false;

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open", "Ctrl+O"))
			{
				WCHAR path[MAX_PATH];

				// Open the file dialog
				if (OpenFileDialog(path, MAX_PATH))
				{
				
					auto ext = StrToUpper(dev::GetExt(path));
					if (ext == L".ROM") {
						LoadRom(path);
					}
					else if (ext == L".FDD")
					{
						fddPath = path;
						//ImGui::OpenPopup("Boot or load?");
						//ImGui::OpenPopup("Delete?");
						openFddPopup = true;
					}
					else {
						dev::Log(L"Not supported file type: {}", path);
					}
				}
			}
			if (ImGui::BeginMenu("Recent Files"))
			{
				for (const auto& [path, driveIdx, autoBoot] : m_recentFilePaths)
				{
					std::string itemS = dev::StrWToStr(path);
					if (driveIdx >= 0) 
					{
						itemS += std::format(":{}", driveIdx);
						itemS += autoBoot ? "A" : "";
					}
					
					if (ImGui::MenuItem(itemS.c_str()))
					{
						if (driveIdx < 0){
							LoadRom(path);
						}
						else
						{
							fddPath = path;
							ImGui::OpenPopup("Boot or load?");
						}
						break; // because m_recentFilePaths were modified
					}
				}
				ImGui::EndMenu();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Quit", "Alt+F4")) { m_status = AppStatus::EXIT; }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Tools"))
		{
			ImGui::MenuItem("Debugger", NULL, &m_hardwareStatsWindowShow);
			ImGui::MenuItem("Memory Map", NULL, &m_memoryMapWindowShow);
			ImGui::EndMenu();
		}
	
		ImGui::EndMenuBar();
	}

	// FDD: "boot or mount?" popup window
	if (openFddPopup) ImGui::OpenPopup(POPUP_FDD.c_str());

	ImVec2 center = ImGui::GetMainViewport()->GetCenter(); 	// Always center this window when appearing
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal(POPUP_FDD.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Specify the drive to mount the FDD file as \nwell as the auto boot option if required.");
		ImGui::Separator();

		static int driveIdx = 0;
		ImGui::Combo("##DiskSelect", &driveIdx, "Drive0 Boot\0Drive0\0Drive1\0Drive2\0Drive3\0");

		if (ImGui::Button("OK", ImVec2(120, 0))) 
		{ 
			ImGui::CloseCurrentPopup();

			bool autoBoot = driveIdx == 0;
			LoadFdd(fddPath, dev::Max(driveIdx - 1, 0), autoBoot); // "driveIdx - 1" because 0, 1 are associated with FDisk 0
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}

}

void dev::DevectorApp::RecentFilesInit()
{
	auto recentFiles = GetSettingsObject("recentFiles");
	for (const auto& path_driveIdx_autoBoot : recentFiles)
	{
		auto path = dev::StrToStrW(path_driveIdx_autoBoot[0]);
		int driveIdx = path_driveIdx_autoBoot[1];
		bool autoBoot = path_driveIdx_autoBoot[2];

		m_recentFilePaths.push_back({ path, driveIdx, autoBoot });
	}
}

void dev::DevectorApp::RecentFilesStore()
{
	nlohmann::json recentFiles;
	for (const auto& [path, driveIdx, autoBoot] : m_recentFilePaths)
	{
		nlohmann::json item = { dev::StrWToStr(path), driveIdx, autoBoot };

		recentFiles.push_back(item);
	}
	SettingsUpdate("recentFiles", recentFiles);
	SettingsSave(m_stringPath);
}

void dev::DevectorApp::RecentFilesUpdate(const std::wstring& _path, const int _driveIdx, const bool _autoBoot)
{
	// make a copy
	auto path{ _path };

	// remove if it contains
	m_recentFilePaths.remove_if(
		[&_path](const auto& tuple) {
			return std::get<0>(tuple) == _path;
		}
	);

	// add a new one
	m_recentFilePaths.push_front({path, _driveIdx, _autoBoot });
	// check the amount
	if (m_recentFilePaths.size() > RECENT_FILES_MAX)
	{
		m_recentFilePaths.pop_back();
	}
}

void dev::DevectorApp::KeyHandling(GLFWwindow* _window, int _key, int _scancode, int _action, int _modes)
{
	// Retrieve the user pointer to access the class instance
	// TODO: check if a mouse cursor hovers the Display window or a Display window or any its controls are selected.
	DevectorApp* instance = static_cast<DevectorApp*>(glfwGetWindowUserPointer(_window));
	if (instance) 
	{
		instance->ImGui_ImplGlfw_KeyCallback(_window, _key, _scancode, _action, _modes);

		if (instance->m_displayWindowP->IsHovered()) 
		{
			instance->m_hardwareP->Request(Hardware::Req::KEY_HANDLING, { { "key", _key }, { "action", _action} });
		}
	}
}

void dev::DevectorApp::AppStyleInit()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.FrameBorderSize = 1.0f;

	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(0.83f, 0.83f, 0.83f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.09f, 0.09f, 0.12f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
	colors[ImGuiCol_Border] = ImVec4(0.24f, 0.24f, 0.24f, 0.25f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.08f, 0.07f, 0.07f, 0.11f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.13f, 0.45f, 0.80f, 0.52f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.00f, 0.50f, 0.83f, 0.63f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.24f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.49f, 0.49f, 0.49f, 0.45f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.46f, 0.46f, 0.46f, 0.61f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.25f, 0.57f, 0.82f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.13f, 0.43f, 0.78f, 0.55f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.14f, 0.56f, 0.97f, 0.83f);
	colors[ImGuiCol_Button] = ImVec4(0.22f, 0.56f, 1.00f, 0.69f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.05f, 0.37f, 0.74f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.08f, 0.35f, 0.70f, 0.69f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.31f, 0.70f, 0.64f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.08f, 0.35f, 0.60f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.53f, 0.55f, 0.75f, 0.11f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.35f, 0.41f, 0.20f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_Tab] = ImVec4(0.27f, 0.29f, 0.31f, 0.86f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.07f, 0.36f, 0.71f, 0.80f);
	colors[ImGuiCol_TabActive] = ImVec4(0.09f, 0.35f, 0.66f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
	colors[ImGuiCol_DockingPreview] = ImVec4(0.45f, 0.44f, 0.53f, 0.32f);
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.53f, 0.53f, 0.53f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.53f, 0.69f, 0.84f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.15f, 0.40f, 0.93f, 0.66f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.45f, 0.54f, 0.73f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.20f, 0.20f, 0.21f, 1.00f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.24f, 0.24f, 0.24f, 0.82f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.24f, 0.25f, 1.00f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.35f, 0.34f, 0.40f, 0.11f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.02f, 0.41f, 0.87f, 0.84f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.04f, 0.43f, 0.88f, 0.76f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}


﻿#include "ui/keyboard_window.h"

#include <format>
#include "utils/str_utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"

dev::KeyboardWindow::KeyboardWindow(Hardware& _hardware,
	const float* const _dpiScaleP,
	ReqUI& _reqUI, const std::string& _pathImgVector)
	:
	BaseWindow("Keyboard", DEFAULT_WINDOW_W, DEFAULT_WINDOW_H, _dpiScaleP),
	m_hardware(_hardware), 
	m_reqUI(_reqUI), m_keyboardImgPath(_pathImgVector)
{
	int width, height, channels;
	auto dataP = stbi_load(_pathImgVector.c_str(), &width, &height, &channels, 0);
	if (!dataP){
		dev::Log("Keyboard Window: Failed to load keyboard image. Reason: {}\nPath: {}", stbi_failure_reason(), _pathImgVector);
		return;
	}
	m_inited = true;
}

void dev::KeyboardWindow::Update(bool& _visible)
{
	BaseWindow::Update();

	if (_visible && ImGui::Begin(m_name.c_str(), &_visible, ImGuiWindowFlags_NoCollapse))
	{
		bool isRunning = m_hardware.Request(Hardware::Req::IS_RUNNING)->at("isRunning");

		if (m_inited) 
		{
			//UpdateData(isRunning);
			Draw(isRunning);
		}

		ImGui::End();
	}
}

void dev::KeyboardWindow::Draw(const bool _isRunning)
{

/*
	ImGui::Text("State current / recorded: %d / %d", m_stateCurrent, m_stateRecorded);

	ImGui::Separator();

	if (ImGui::Button(_isRunning ? "Break" : " Run "))
	{
		m_hardware.Request(_isRunning ? Hardware::Req::STOP : Hardware::Req::RUN);
	}

	if (_isRunning) ImGui::BeginDisabled();

	ImGui::SameLine();
	if (ImGui::Button("Clear"))
	{
		m_hardware.Request(Hardware::Req::DEBUG_RECORDER_RESET);
		m_stateRecorded = m_hardware.Request(Hardware::Req::DEBUG_RECORDER_GET_STATE_RECORDED)->at("states");
		m_stateCurrent = m_hardware.Request(Hardware::Req::DEBUG_RECORDER_GET_STATE_CURRENT)->at("states");
	}
	if (_isRunning) ImGui::EndDisabled();
	
	ImGui::SameLine();
	dev::DrawHelpMarker("Left Ctrl + R - reverse playback. Works runtime and while break\n"
						"Left Ctrl + F - forward playback. Works while break only");

	ImGui::Dummy({ 1, 5 });

	// Frame slider
	dev::PushStyleCompact(0.5f);
	int stateCurrentOld = m_stateCurrent;
	if (ImGui::SliderInt("##recTimeline", &m_stateCurrent, 1, m_stateRecorded, "%d", ImGuiSliderFlags_AlwaysClamp))
	{
		int diff = m_stateCurrent - stateCurrentOld;
		if (diff < 0) {
			m_hardware.Request(Hardware::Req::DEBUG_RECORDER_PLAY_REVERSE, { {"frames", abs(diff)} });
		}
		else {
			m_hardware.Request(Hardware::Req::DEBUG_RECORDER_PLAY_FORWARD, { {"frames", abs(diff)} });
		}

		m_stateCurrent = m_hardware.Request(Hardware::Req::DEBUG_RECORDER_GET_STATE_CURRENT)->at("states");
	}

	ImGui::SameLine();
	if (ImGui::Button(" - ") ||
		(ImGui::IsKeyPressed(ImGuiKey_R) && ImGui::IsKeyPressed(ImGuiKey_LeftCtrl)))
	{
		m_hardware.Request(Hardware::Req::DEBUG_RECORDER_PLAY_REVERSE, { {"frames", 1}});
		m_stateCurrent = m_hardware.Request(Hardware::Req::DEBUG_RECORDER_GET_STATE_CURRENT)->at("states");
	}

	ImGui::SameLine();
	if (ImGui::Button(" + "))
	{
		m_hardware.Request(Hardware::Req::DEBUG_RECORDER_PLAY_FORWARD, { {"frames", 1} });
		m_stateCurrent = m_hardware.Request(Hardware::Req::DEBUG_RECORDER_GET_STATE_CURRENT)->at("states");
	}
	dev::PopStyleCompact();
	*/
}
/*
void dev::KeyboardWindow::UpdateData(const bool _isRunning)
{
	// check if the hardware updated its state
	uint64_t cc = m_hardware.Request(Hardware::Req::GET_CC)->at("cc");
	auto ccDiff = cc - m_ccLast;
	if (ccDiff == 0) return;
	m_ccLast = cc;

	// update
	m_stateRecorded = m_hardware.Request(Hardware::Req::DEBUG_RECORDER_GET_STATE_RECORDED)->at("states");
	m_stateCurrent = m_hardware.Request(Hardware::Req::DEBUG_RECORDER_GET_STATE_CURRENT)->at("states");
}
*/
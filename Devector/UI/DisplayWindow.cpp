#include "DisplayWindow.h"

#include "imgui.h"
#include "imgui_impl_opengl3_loader.h"

// Vertex shader source code
const char* vtxShaderS = R"#(
	#version 330 core
	precision highp float;

	layout (location = 0) in vec3 pos;
	layout (location = 1) in vec2 uv;

	out vec2 uv0;

	void main()
	{
		uv0 = vec2(uv.x, 1.0f - uv.y);
		gl_Position = vec4(pos.xyz, 1.0f);
	}
)#";

// Fragment shader source code
const char* fragShaderS = R"#(
	#version 330 core
	precision highp float;
	precision highp int;

	in vec2 uv0;

	uniform sampler2D texture0;
	uniform vec4 m_activeArea_pxlSize;
	uniform vec4 m_bordsLRTB;
	uniform vec4 m_scrollV_crtXY_highlightMul;

	layout (location = 0) out vec4 out0;

	void main()
	{
		vec2 uv = uv0;

		// vertical scrolling
		if (uv.x >= m_bordsLRTB.x &&
			uv.x < m_bordsLRTB.y &&
			uv.y >= m_bordsLRTB.z &&
			uv.y < m_bordsLRTB.w)
		{
			uv.y -= m_scrollV_crtXY_highlightMul.x;
			// wrap V
			uv.y += uv.y < m_bordsLRTB.z ? m_activeArea_pxlSize.y * m_activeArea_pxlSize.w : 0.0f;
		}

		vec3 color = texture(texture0, uv).rgb;

		// crt scanline highlight
		if (uv.y > m_scrollV_crtXY_highlightMul.z ||
			(uv.y >= m_scrollV_crtXY_highlightMul.z && uv.x > m_scrollV_crtXY_highlightMul.y))
		{
			color.xyz *= m_scrollV_crtXY_highlightMul.w;
		}

		out0 = vec4(color, 1.0f);
	}
)#";

dev::DisplayWindow::DisplayWindow(Hardware& _hardware,
		const float* const _fontSizeP, const float* const _dpiScaleP, GLUtils& _glUtils)
	:
	BaseWindow("Display", DEFAULT_WINDOW_W, DEFAULT_WINDOW_H, _fontSizeP, _dpiScaleP),
	m_hardware(_hardware), m_glUtils(_glUtils)
{
	m_isGLInited = Init();
}

bool dev::DisplayWindow::Init()
{
	auto vramShaderRes = m_glUtils.InitShader(vtxShaderS, fragShaderS);
	if (!vramShaderRes) return false;
	m_vramShaderId = *vramShaderRes;

	auto m_vramTexRes = m_glUtils.InitTexture(Display::FRAME_W, Display::FRAME_H, GLUtils::Texture::Format::RGBA);
	if (!m_vramTexRes) return false;
	m_vramTexId = *m_vramTexRes;

	GLUtils::ShaderParams shaderParams = {
		{ "m_activeArea_pxlSize", &m_activeArea_pxlSize },
		{ "m_scrollV_crtXY_highlightMul", &m_scrollV_crtXY_highlightMul },
		{ "m_bordsLRTB", &m_bordsLRTB }};
	auto vramMatRes = m_glUtils.InitMaterial(m_vramShaderId, Display::FRAME_W, Display::FRAME_H,
			{m_vramTexRes}, shaderParams);
	if (!vramMatRes) return false;
	m_vramMatId = *vramMatRes;

	return true;
}

void dev::DisplayWindow::Update(bool& _visible)
{
	BaseWindow::Update();

	if (_visible && ImGui::Begin(m_name.c_str(), &_visible, ImGuiWindowFlags_NoCollapse))
	{
		static int dev_IRQ_COMMIT_PXL = 72;

		//TODO: why dev::IRQ_COMMIT_PXL is not changing???
		dev::IRQ_COMMIT_PXL = dev_IRQ_COMMIT_PXL;

		bool isRunning = m_hardware.Request(Hardware::Req::IS_RUNNING)->at("isRunning");
		m_isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_None);
		
		// switch the border type
		if (ImGui::IsKeyPressed(ImGuiKey_B) && ImGui::IsKeyPressed(ImGuiKey_LeftAlt)) {
			m_borderType = static_cast<BorderType>((static_cast<int>(m_borderType) + 1) % static_cast<int>(BorderType::LEN));
		}
		// switch the display size
		if (ImGui::IsKeyPressed(ImGuiKey_S) && ImGui::IsKeyPressed(ImGuiKey_LeftAlt)) {
			m_displaySize = static_cast<DisplaySize>((static_cast<int>(m_displaySize) + 1) % static_cast<int>(DisplaySize::LEN));
		}

		UpdateData(isRunning);
		DrawDisplay();

		ImGui::End();
	}
}

bool dev::DisplayWindow::IsHovered() const
{
	return m_isHovered;
}

void dev::DisplayWindow::UpdateData(const bool _isRunning)
{
	auto res = m_hardware.Request(Hardware::Req::GET_REGS);
	const auto& data = *res;
	uint64_t cc = data["cc"];
	auto ccDiff = cc - m_ccLast;
	m_ccLastRun = ccDiff == 0 ? m_ccLastRun : ccDiff;
	m_ccLast = cc;

	m_scrollV_crtXY_highlightMul.w = 1.0f;

	if (!_isRunning)
	{
		if (ccDiff) 
		{
			res = m_hardware.Request(Hardware::Req::GET_DISPLAY_DATA);
			const auto& displayData = *res;
			m_rasterPixel = displayData["rasterPixel"];
			m_rasterLine = displayData["rasterLine"];
		}
		if (m_isHovered)
		{
			m_scrollV_crtXY_highlightMul.y = (float)m_rasterPixel * FRAME_PXL_SIZE_W;
			m_scrollV_crtXY_highlightMul.z = (float)m_rasterLine * FRAME_PXL_SIZE_H;
			m_scrollV_crtXY_highlightMul.w = SCANLINE_HIGHLIGHT_MUL;
		}
	}

	// update
	if (m_isGLInited)
	{
		float scrollVert = static_cast<uint8_t>(m_hardware.Request(Hardware::Req::SCROLL_VERT)->at("scrollVert") + 1); // adding +1 offset because the default is 255
		m_scrollV_crtXY_highlightMul.x = FRAME_PXL_SIZE_H * scrollVert;

		auto frameP = m_hardware.GetFrame(_isRunning);
		m_glUtils.UpdateTexture(m_vramTexId, (uint8_t*)frameP->data());
		m_glUtils.Draw(m_vramMatId);
	}
}

void dev::DisplayWindow::DrawDisplay()
{
	if (m_isGLInited)
	{
		float border = 0;
		ImVec2 borderMin;
		ImVec2 borderMax;
		switch (m_borderType)
		{
		case dev::DisplayWindow::BorderType::NONE:
			borderMin = { 0.0f, 0.0f };
			borderMax = { 1.0f, 1.0f };
			break;

		case dev::DisplayWindow::BorderType::NORMAL:
			border = Display::BORDER_VISIBLE;
			[[fallthrough]];

		case dev::DisplayWindow::BorderType::FULL:
			borderMin = {
				(Display::BORDER_LEFT - border * 2) * FRAME_PXL_SIZE_W,
				(Display::SCAN_ACTIVE_AREA_TOP - border) * FRAME_PXL_SIZE_H };
			borderMax = {
				borderMin.x + (Display::ACTIVE_AREA_W + border * 4) * FRAME_PXL_SIZE_W,
				borderMin.y + (Display::ACTIVE_AREA_H + border * 2) * FRAME_PXL_SIZE_H };
			break;
		}

		ImVec2 displaySize;
		switch (m_displaySize)
		{
		case dev::DisplayWindow::DisplaySize::R256_256:
			displaySize.x = 256.0f;
			displaySize.y = 256.0f;
			break;
		case dev::DisplayWindow::DisplaySize::R512_256:
			displaySize.x = 512.0f;
			displaySize.y = 256.0f;
			break;
		case dev::DisplayWindow::DisplaySize::R512_512:
			displaySize.x = 512.0f;
			displaySize.y = 512.0f;
			break;
		case dev::DisplayWindow::DisplaySize::MAX:
		{
			ImGuiStyle& style = ImGui::GetStyle();
			displaySize.x = ImGui::GetWindowWidth() - style.FramePadding.x * 4;
			displaySize.y = displaySize.x * WINDOW_ASPECT;
			break;
		}
		}

		auto framebufferTex = m_glUtils.GetFramebufferTexture(m_vramMatId);
		ImGui::Image((void*)(intptr_t)framebufferTex, displaySize, borderMin, borderMax);
	}
}
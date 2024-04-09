#include "DisplayWindow.h"

#include "imgui.h"
#include "imgui_impl_opengl3_loader.h"

// Vertex shader source code
const char* vertexShaderSource = R"(
    #version 330 core
    precision highp float;
    
    layout (location = 0) in vec3 vtxPos;
    layout (location = 1) in vec2 vtxUV;
    
    out vec2 uv0;

    void main()
    {
        uv0 = vtxUV;
        gl_Position = vec4(vtxPos.xyz, 1.0f);
    }
)";

// Fragment shader source code
const char* fragmentShaderSource = R"(
    #version 330 core
    precision highp float;

    in vec2 uv0;

    uniform sampler2D texture0;
    uniform ivec2 iresolution;

    layout (location = 0) out vec4 out0;

    void main()
    {
        vec3 color = texture(texture0, vec2(uv0.x, 1.0f - uv0.y)).rgb;
        out0 = vec4(color, 1.0f);
    }
)";

dev::DisplayWindow::DisplayWindow(Hardware& _hardware,
        const float* const _fontSizeP, const float* const _dpiScaleP, GLUtils& _glUtils)
	:
	BaseWindow(DEFAULT_WINDOW_W, DEFAULT_WINDOW_H, _fontSizeP, _dpiScaleP),
    m_hardware(_hardware), m_isHovered(false), m_glUtils(_glUtils)
{
    //CreateTexture(true);
    //UpdateData(false);

    m_shaderParamData = {};
    //{ "globalColorBg", globalColorBg },
    //{ "globalColorFg", globalColorFg } };

    m_renderDataIdx = m_glUtils.InitRenderData(vertexShaderSource, fragmentShaderSource, FRAME_BUFFER_W, FRAME_BUFFER_H, {}, 1);
}

void dev::DisplayWindow::Update()
{
	BaseWindow::Update();

	static bool open = true;
	ImGui::Begin(m_name.c_str(), &open, ImGuiWindowFlags_NoCollapse);

    bool isRunning = m_hardware.Request(Hardware::Req::IS_RUNNING)->at("isRunning");
    UpdateData(isRunning);
	
    DrawDisplay();

    m_isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_None);
	ImGui::End();
}

bool dev::DisplayWindow::IsHovered()
{
    return m_isHovered;
}

void dev::DisplayWindow::DrawDisplay()
{
    if (m_renderDataIdx >= 0 && m_glUtils.IsShaderDataReady(m_renderDataIdx))
    {
        int scrollVert = m_hardware.Request(Hardware::Req::SCROLL_VERT)->at("scrollVert");

        auto& framebufferTextures = m_glUtils.GetFramebufferTextures(m_renderDataIdx);

        ImGui::Image((void*)(intptr_t)framebufferTextures[0], ImVec2(DEFAULT_WINDOW_W, DEFAULT_WINDOW_H));
    }
}

// creates a textre
void dev::DisplayWindow::CreateTexture(const bool _vsync)
{
    auto frameP = m_hardware.GetFrame(_vsync);

    // Create a OpenGL texture identifier
    if (!m_frameTextureId)
    {
        glGenTextures(1, &m_frameTextureId);
    }
    glBindTexture(GL_TEXTURE_2D, m_frameTextureId);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Display::FRAME_W, Display::FRAME_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, frameP->data());
}

void dev::DisplayWindow::UpdateData(const bool _isRunning)
{
    auto res = m_hardware.Request(Hardware::Req::GET_REGS);
    const auto& data = *res;

    uint64_t cc = data["cc"];
    auto ccDiff = cc - m_ccLast;
    m_ccLastRun = ccDiff == 0 ? m_ccLastRun : ccDiff;
    m_ccLast = cc;
    if (ccDiff == 0) return;

    // update
    //CreateTexture(_isRunning);
    if (m_renderDataIdx >= 0)
    {
        //auto memP = m_hardware.GetRam()->data();
        auto frameP = m_hardware.GetFrame(_isRunning);

        m_glUtils.UpdateTextures(m_renderDataIdx, (uint8_t*)frameP->data(), Display::FRAME_W, Display::FRAME_H, 3);
        m_glUtils.Draw(m_renderDataIdx, m_shaderParamData);
    }
}
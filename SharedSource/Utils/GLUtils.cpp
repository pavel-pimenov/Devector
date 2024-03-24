#include "GLUtils.h"

#include <format>
#include "GL/glew.h"

// vertices of a quad with UV coordinates
GLfloat vertices[] = {
    // Positions          // UV Coords
    /*
     -1.0f, -1.0f, 0.0f,  1.0f, 1.0f,
     -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
      1.0f,  1.0f, 0.0f,  0.0f, 0.0f,
      1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
      */
     -1.0f, -1.0f, 0.0f,  0.0f, 1.0f,
     -1.0f,  1.0f, 0.0f,  0.0f, 0.0f,
      1.0f,  1.0f, 0.0f,  1.0f, 0.0f,
      1.0f, -1.0f, 0.0f,  1.0f, 1.0f,
};


// Vertex shader source code
const char* vertexShaderSource = R"(
    #version 330 core
    precision highp float;
    
    layout (location = 0) in vec3 vtxPos;
    layout (location = 1) in vec2 vtxUV;

    uniform vec4 globalColor;
    
    out vec2 uv0;
    out vec4 globalColor0;

    void main()
    {
        uv0 = vtxUV;
        globalColor0 = globalColor;        
        gl_Position = vec4(vtxPos.xyz, 1.0f);
    }
)";

// Fragment shader source code
const char* fragmentShaderSource = R"(
    #version 330 core
    precision highp float;

    in vec2 uv0;
    in vec4 globalColor0;

    uniform sampler2D texture0;
    uniform ivec2 iresolution;

    layout (location = 0) out vec4 out0;

    int GetBit(float _color, int _bitIdx) {
        return (int(_color * 255.0) >> _bitIdx) & 1;
    }

    #define RESOLUTION_X = 256
    #define RESOLUTION_y = 256

    void main()
    {
        
        int addrOffset = int(uv0.x * 255.0 / 8.0) + int(uv0.y * 255.0) * 256;
        vec2 uv = vec2( int(addrOffset / 256) / 255.0, int(addrOffset % 256)/ 255.0);
        float byteColor = texture(texture0, uv).r;
        
        // int bitIdx = int(uv0.x / 8.0) % 8;
        // int bitColor = GetBit(byteColor, bitIdx);

        out0 = globalColor0 * byteColor;
        //out0 = globalColor0 * vec4(uv0, 0, 1);
    }
)";

dev::GLUtils::GLUtils(Hardware& _hardware, const int _frameSizeW, const int _frameSizeH)
    :
    m_hardware(_hardware), m_frameSizeW(_frameSizeW), m_frameSizeH(_frameSizeH)
{
    Init();
}

void dev::GLUtils::Update()
{
    CreateRamTexture();
    DrawDisplay();
}

void dev::GLUtils::DrawDisplay()
{

    if (IsShaderDataReady())
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_shaderData.framebuffer);
        glViewport(0, 0, m_frameSizeW, m_frameSizeH);
        glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render the quad
        glUseProgram(m_shaderData.shaderProgram);

        // send the color
        glUniform4f(m_shaderData.globalColorId, 1.0f, 1.0f, 1.0f, 1.0f);
        // assign a texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_shaderData.texture);

        glBindVertexArray(m_shaderData.vtxArrayObj);
        glDrawArrays(GL_QUADS, 0, 4);
        glBindVertexArray(0);
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void dev::GLUtils::CreateRamTexture()
{
    auto ramP = m_hardware.GetRam8K(0);

    // Create a OpenGL texture identifier
    if (!m_shaderData.texture)
    {
        glGenTextures(1, &m_shaderData.texture);
    }
    glBindTexture(GL_TEXTURE_2D, m_shaderData.texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Display::FRAME_W, Display::FRAME_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, ram.data());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 256, 32, 0, GL_RED, GL_UNSIGNED_BYTE, ramP->data());
}


// it is not initializing the Window and OpenGL 3.3 context 
// assumming ImGui and did it already
GLenum dev::GLUtils::Init()
{
    auto glewInitCode = glewInit();
    if (glewInitCode != GLEW_OK) {
        dev::Log("Failed to initialize GLEW");
        return glewInitCode;
    }

    CreateRamTexture();

    // Create Vertex Array Object (VAO) and Vertex Buffer Object (VBO)
    glGenVertexArrays(1, &m_shaderData.vtxArrayObj);
    glGenBuffers(1, &m_shaderData.vtxBufferObj);
    glBindVertexArray(m_shaderData.vtxArrayObj);
    glBindBuffer(GL_ARRAY_BUFFER, m_shaderData.vtxBufferObj);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Create and bind a framebuffer object (FBO)
    glGenFramebuffers(1, &m_shaderData.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shaderData.framebuffer);
    // Create a texture to render to
    glGenTextures(1, &m_shaderData.framebufferTexture);
    glBindTexture(GL_TEXTURE_2D, m_shaderData.framebufferTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_frameSizeW, m_frameSizeH, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_shaderData.framebufferTexture, 0);


    // Check framebuffer status
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        dev::Log("Framebuffer is not complete!");
    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Create shader program
    m_shaderData.shaderProgram = CreateShaderProgram(vertexShaderSource, fragmentShaderSource);
    
    // get uniform vars ids
    m_shaderData.globalColorId = glGetUniformLocation(m_shaderData.shaderProgram, "globalColor");
    
    // assign a texture
    glUseProgram(m_shaderData.shaderProgram);
    glUniform1i(glGetUniformLocation(m_shaderData.shaderProgram, "texture0"), 0);

    return IsShaderDataReady() ? glewInitCode : -1;
}

dev::GLUtils::~GLUtils()
{
    // Clean up
    glDeleteFramebuffers(1, &m_shaderData.framebuffer);
    glDeleteTextures(1, &m_shaderData.texture);
    glDeleteTextures(1, &m_shaderData.framebufferTexture);
    glDeleteTextures(1, &m_shaderData.texture);
    glDeleteVertexArrays(1, &m_shaderData.vtxArrayObj);
    glDeleteBuffers(1, &m_shaderData.vtxBufferObj);

    // Delete shader program
    glDeleteProgram(m_shaderData.shaderProgram);
}

GLuint dev::GLUtils::GLCheckError(GLuint1 _obj, const std::string& _txt)
{
    // Check for compilation errors
    GLint success;
    glGetShaderiv(_obj, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(_obj, 512, NULL, infoLog);
        dev::Log("{}:\n {}", _txt, std::string(infoLog));
        return 0;
    }
    return _obj;
}

GLuint dev::GLUtils::CompileShader(GLenum _shaderType, const char* _source) 
{
    GLuint shader = glCreateShader(_shaderType);
    glShaderSource(shader, 1, &_source, NULL);
    glCompileShader(shader);

    return GLCheckError(shader, std::format("Shader compilation failed:\n {}", _source));
}

GLuint dev::GLUtils::CreateShaderProgram(const char* _vertexShaderSource, const char* _fragmentShaderSource)
{
    // Compile vertex and fragment shaders
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, _vertexShaderSource);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, _fragmentShaderSource);

    // Create shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    shaderProgram = GLCheckError(shaderProgram, "Shader program linking failed:\n");

    // Delete shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

auto dev::GLUtils::GetShaderData() 
-> const ShaderData*
{
    return &m_shaderData;
}

auto dev::GLUtils::IsShaderDataReady()
-> const bool
{
    return m_shaderData.framebuffer && m_shaderData.framebufferTexture &&
        m_shaderData.shaderProgram && m_shaderData.texture && 
        m_shaderData.vtxArrayObj && m_shaderData.vtxBufferObj;
}
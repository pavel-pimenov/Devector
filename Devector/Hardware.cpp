#include "Hardware.h"

dev::Hardware::Hardware()
    :
    m_memory(),
    m_io(),
    m_debugger(),
    m_cpu(
        std::bind(&Memory::GetByte, &m_memory, std::placeholders::_1, std::placeholders::_2),
        std::bind(&Memory::SetByte, &m_memory, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
        std::bind(&IO::PortIn, &m_io, std::placeholders::_1),
        std::bind(&IO::PortOut, &m_io, std::placeholders::_1, std::placeholders::_2),
        std::bind(&Debugger::MemStats, &m_debugger, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
    m_display(m_memory)
{}

void dev::Hardware::LoadRom(const std::wstring& _path)
{
    auto fileSize = GetFileSize(_path);
    if (fileSize > Memory::MEMORY_MAIN_LEN){
        // TODO: communicate the fail state
        return;
    }

    auto result = dev::LoadFile(_path);

    if (!result || result->empty()){
        // TODO: communicate the fail state
        return;
    }

    Init();
    m_memory.Load(*result);
    Log("file loaded: %f", _path);
}

// rasterizes the frame. For realtime emulation it should be called by the 50.08 Hz (3000000/59904) timer
void dev::Hardware::ExecuteFrame()
{
    do
    {
        ExecuteInstruction();
    } while (!display.T50HZ);
}

void dev::Hardware::ExecuteInstruction()
{
}

void dev::Hardware::Init()
{
    m_memory.Init();
    m_cpu.Init();
    m_display.Init();
    m_debugger.Init();
}

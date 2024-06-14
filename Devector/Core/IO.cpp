// the hardware logic is mostly taken from:
// https://github.com/parallelno/v06x/blob/master/src/board.cpp
// https://github.com/parallelno/v06x/blob/master/src/vio.h

#include "IO.h"

dev::IO::IO(Keyboard& _keyboard, Memory& _memory, TimerI8253& _timer,
    Fdc1793& _fdc, VectorColorToArgbFunc _vectorColorToArgbFunc)
    :
    m_keyboard(_keyboard), m_memory(_memory), m_timer(_timer),
    m_fdc(_fdc),
    VectorColorToArgb(_vectorColorToArgbFunc)
{
    Init();
}

void dev::IO::Init()
{
    CW = 0x08;
    m_portA = 0xFF;
    m_portB = 0xFF;
    m_portC = 0xFF;
    CW2 = 0;
    m_portA2 = 0xFF;
    m_portB2 = 0xFF;
    m_portC2 = 0xFF;
    m_outport = PORT_NO_COMMIT;
    m_outbyte = PORT_NO_COMMIT;
    m_palettebyte = PORT_NO_COMMIT;
    joy_0e = 0xFF;
    joy_0f = 0xFF;
    m_borderColorIdx = 0;
    m_displayMode = DISPLAY_MODE_256;
    m_outCommitTimer = IO::PORT_NO_COMMIT;
    m_paletteCommitTimer = IO::PORT_NO_COMMIT;
    m_ruslat = 0;
    m_ruslatHistory = 0;

    m_palette.fill(0xFF000000);
}

auto dev::IO::PortIn(uint8_t _port)
-> uint8_t
{
    int result = 0xFF;

    switch (_port) {
    case 0x00:
        //result = 0xFF; TODO: learn what it's for
        break;
    case 0x01:
    {
        /* PortC.low input ? */
        auto portCLow = (CW & 0x01) ? 0x0b : m_portC & 0x0f;
        /* PortC.high input ? */
        auto portCHigh = (CW & 0x08) ?
                /*(tape_player.sample() << 4) |*/
                (m_keyboard.m_keySS  ? 0 : 1 << 5) |
                (m_keyboard.m_keyUS  ? 0 : 1 << 6) |
                (m_keyboard.m_keyRus ? 0 : 1 << 7) : m_portC & 0xf0;
        result = portCLow | portCHigh;
    }
        break;

    case 0x02:
        if ((CW & 0x02) != 0) {
            result = m_keyboard.Read(m_portA); // input
        }
        else {
            result = m_portB;       // output
        }
        break;
    case 0x03:
        if ((CW & 0x10) == 0) {
            result = m_portA;       // output
        }
        else {
            result = 0xFF;          // input
        }
        break;

    case 0x04:
        result = CW2;
        break;
    case 0x05:
        result = m_portC2;
        break;
    case 0x06:
        result = m_portB2;
        break;
    case 0x07:
        result = m_portA2;
        break;

        // Timer
    case 0x08: [[fallthrough]];
    case 0x09: [[fallthrough]];
    case 0x0a: [[fallthrough]];
    case 0x0b:
        //return m_timer.read(_port);

        // Joystick "C"
    case 0x0e:
        return joy_0e;
    case 0x0f:
        return joy_0f;

    case 0x14: [[fallthrough]];
    case 0x15:
        //result = ay.read(port & 1);
        break;

    case 0x18:
        result = m_fdc.Read(Fdc1793::Port::DATA);
        break;
    case 0x19:
        result = m_fdc.Read(Fdc1793::Port::SECTOR);
        break;
    case 0x1a:
        result = m_fdc.Read(Fdc1793::Port::TRACK);
        break;
    case 0x1b:
        result = m_fdc.Read(Fdc1793::Port::STATUS);
        break;
    case 0x1c:
        result = m_fdc.Read(Fdc1793::Port::READY);
        break;
    default:
        break;
    }

    return result;
}

// cpu sends this data
void dev::IO::PortOut(uint8_t _port, uint8_t _value)
{
    m_outport = _port;
    m_outbyte = _value;

    m_outCommitTimer = OUT_COMMIT_TIME;
    if (_port == PORT_OUT_BORDER_COLOR) {
        m_paletteCommitTimer = PALETTE_COMMIT_TIME;
    }
}

void dev::IO::PortOutCommit()
{
    PortOutHandling(m_outport, m_outbyte);
}

void dev::IO::PaletteCommit(const int _index)
{
    m_palette[_index] = VectorColorToArgb(m_palettebyte);
}

// data sent by cpu handled here at the commit time
void dev::IO::PortOutHandling(uint8_t _port, uint8_t _value)
{
    //bool ruslat;
    switch (_port) {
        // PortInputA 
    case 0x00:
        //m_ruslat = (m_portC >> 3) & 1;
        if ((_value & 0x80) == 0) {
            // port C BSR: 
            //   bit 0: 1 = set, 0 = reset
            //   bit 1-3: bit number
            int bit = (_value >> 1) & 7;
            if ((_value & 1) == 1) {
                m_portC |= 1 << bit;
            }
            else {
                m_portC &= ~(1 << bit);
            }
            //ontapeoutchange(m_portC & 1);
        }
        else {
            CW = _value;
            PortOutHandling(1, 0);
            PortOutHandling(2, 0);
            PortOutHandling(3, 0);
        }
        /*if (((m_portC & 8) > 0) != ruslat) {
            m_ruslat((m_portC & 8) == 0);
        }*/
        break;
    case 0x01:
        m_ruslat = (m_portC >> 3) & 1;
        m_ruslatHistory = (m_ruslatHistory<<1) + m_ruslat;
        m_portC = _value;
        //ontapeoutchange(m_portC & 1);
        /*if (((m_portC & 8) > 0) != ruslat && onruslat) {
            onruslat((m_portC & 8) == 0);
        }*/
        break;
    case 0x02:
        m_portB = _value;
        m_borderColorIdx = m_portB & 0x0f;
        m_displayMode = (m_portB & 0x10) != 0;
        break;
        // vertical scroll
    case 0x03:
        m_portA = _value;
        break;
        // PPI2
    case 0x04:
        CW2 = _value;
        break;
    case 0x05:
        m_portC2 = _value;
        break;
    case 0x06:
        m_portB2 = _value;
        break;
    case 0x07:
        m_portA2 = _value;
        break;

        // Timer
    case 0x08: [[fallthrough]];
    case 0x09: [[fallthrough]];
    case 0x0a: [[fallthrough]];
    case 0x0b:
        //m_timer.write(_port, _value);
        break;

        // palette (ask Svofski why 0x0d and 0x0e ports are for pallete)
    case PORT_OUT_BORDER_COLOR: [[fallthrough]];
    case 0x0d: [[fallthrough]];
    case 0x0e: [[fallthrough]];
    case 0x0f:
        m_palettebyte = _value;
        break;
    case 0x10:
        m_memory.SetRamDiskMode(_value);
        break;
    case 0x14: [[fallthrough]];
    case 0x15:
        //ay.Write(port & 1, _value);
        break;

    case 0x18:
        m_fdc.Write(Fdc1793::Port::DATA, _value);
        break;
    case 0x19:
        m_fdc.Write(Fdc1793::Port::SECTOR, _value);
        break;
    case 0x1a:
        m_fdc.Write(Fdc1793::Port::TRACK, _value);
        break;
    case 0x1b:
        m_fdc.Write(Fdc1793::Port::COMMAND, _value);
        break;
    case 0x1c:
        m_fdc.Write(Fdc1793::Port::SYSTEM, _value);
        break;
    default:
        break;
    }
}

void dev::IO::TryToCommit(const uint8_t _colorIdx)
{
    if (m_outCommitTimer >= 0){
        if (--m_outCommitTimer == 0)
        {
            PortOutCommit();
        }
    }

    if (m_paletteCommitTimer >= 0) {
        if (--m_paletteCommitTimer == 0)
        {
            PaletteCommit(_colorIdx);
        }
    }
}

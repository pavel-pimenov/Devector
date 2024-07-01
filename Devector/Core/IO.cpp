// the hardware logic is mostly taken from:
// https://github.com/parallelno/v06x/blob/master/src/board.cpp
// https://github.com/parallelno/v06x/blob/master/src/vio.h

#include "IO.h"

#define CW		m_state.CW
#define PORT_A	m_state.portA
#define PORT_B	m_state.portB
#define PORT_C	m_state.portC
#define CW2		m_state.CW2
#define PORT_A2	m_state.portA2
#define PORT_B2	m_state.portB2
#define PORT_C2	m_state.portC2

#define OUT_PORT	m_state.outport
#define OUT_BYTE	m_state.outbyte
#define HW_COLOR	m_state.hwColor
#define JOY_0		m_state.joy0
#define JOY_1		m_state.joy1
#define RUS_LAT		m_state.ruslat

#define BRD_COLOR_IDX	m_state.brdColorIdx
#define DISPLAY_MODE	m_state.displayMode

#define OUT_COMMIT_TIMER		m_state.outCommitTimer
#define PALLETE_COMMIT_TIMER	m_state.paletteCommitTimer

dev::IO::IO(Keyboard& _keyboard, Memory& _memory, TimerI8253& _timer,
	Fdc1793& _fdc)
	:
	m_keyboard(_keyboard), m_memory(_memory), m_timer(_timer),
	m_fdc(_fdc)
{
	Init();
	int a = sizeof(m_state);
}

void dev::IO::Init()
{
	CW = 0x08;
	CW2 = 0;
	PORT_A = PORT_B = PORT_C = PORT_A2 = PORT_B2 = PORT_C2 = JOY_0 = JOY_1 = 0xFF;
	OUT_PORT = OUT_BYTE = HW_COLOR = BRD_COLOR_IDX = 0;

	DISPLAY_MODE = MODE_256;

	OUT_COMMIT_TIMER = IO::PORT_NO_COMMIT;
	PALLETE_COMMIT_TIMER = IO::PORT_NO_COMMIT;

	RUS_LAT = 0;
	m_ruslatHistory = 0;

	for (int i=0; i<PALETTE_LEN; i++) m_state.palette[i] = 0;
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
		auto portCLow = (CW & 0x01) ? 0x0b : PORT_C & 0x0f;
		/* PortC.high input ? */
		auto portCHigh = (CW & 0x08) ?
				/*(tape_player.sample() << 4) |*/
				(m_keyboard.m_keySS  ? 0 : 1 << 5) |
				(m_keyboard.m_keyUS  ? 0 : 1 << 6) |
				(m_keyboard.m_keyRus ? 0 : 1 << 7) : PORT_C & 0xf0;
		result = portCLow | portCHigh;
	}
		break;

	case 0x02:
		if ((CW & 0x02) != 0) {
			result = m_keyboard.Read(PORT_A); // input
		}
		else {
			result = PORT_B;       // output
		}
		break;
	case 0x03:
		if ((CW & 0x10) == 0) {
			result = PORT_A;       // output
		}
		else {
			result = 0xFF;          // input
		}
		break;

	case 0x04:
		result = CW2;
		break;
	case 0x05:
		result = PORT_C2;
		break;
	case 0x06:
		result = PORT_B2;
		break;
	case 0x07:
		result = PORT_A2;
		break;

		// Timer
	case 0x08: [[fallthrough]];
	case 0x09: [[fallthrough]];
	case 0x0a: [[fallthrough]];
	case 0x0b:
		//return m_timer.read(_port);

		// Joystick "C"
	case 0x0e:
		return JOY_0;
	case 0x0f:
		return JOY_1;

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
	OUT_PORT = _port;
	OUT_BYTE = _value;

	OUT_COMMIT_TIMER = OUT_COMMIT_TIME;
	if (_port == PORT_OUT_BORDER_COLOR) {
		PALLETE_COMMIT_TIMER = PALETTE_COMMIT_TIME;
	}
}

void dev::IO::PortOutCommit()
{
	PortOutHandling(OUT_PORT, OUT_BYTE);
}

// data sent by cpu handled here at the commit time
void dev::IO::PortOutHandling(uint8_t _port, uint8_t _value)
{
	//bool ruslat;
	switch (_port) {
		// PortInputA 
	case 0x00:
		//m_ruslat = (PORT_C >> 3) & 1;
		if ((_value & 0x80) == 0) {
			// port C BSR: 
			//   bit 0: 1 = set, 0 = reset
			//   bit 1-3: bit number
			int bit = (_value >> 1) & 7;
			if ((_value & 1) == 1) {
				PORT_C |= 1 << bit;
			}
			else {
				PORT_C &= ~(1 << bit);
			}
			//ontapeoutchange(PORT_C & 1);
		}
		else {
			CW = _value;
			PortOutHandling(1, 0);
			PortOutHandling(2, 0);
			PortOutHandling(3, 0);
		}
		/*if (((PORT_C & 8) > 0) != ruslat) {
			m_ruslat((PORT_C & 8) == 0);
		}*/
		break;
	case 0x01:
		RUS_LAT = (PORT_C >> 3) & 1;
		m_ruslatHistory = (m_ruslatHistory<<1) + RUS_LAT;
		PORT_C = _value;
		//ontapeoutchange(PORT_C & 1);
		/*if (((PORT_C & 8) > 0) != ruslat && onruslat) {
			onruslat((PORT_C & 8) == 0);
		}*/
		break;
	case 0x02:
		PORT_B = _value;
		BRD_COLOR_IDX = PORT_B & 0x0f;
		DISPLAY_MODE = (PORT_B & 0x10) != 0;
		break;
		// vertical scroll
	case 0x03:
		PORT_A = _value;
		break;
		// PPI2
	case 0x04:
		CW2 = _value;
		break;
	case 0x05:
		PORT_C2 = _value;
		break;
	case 0x06:
		PORT_B2 = _value;
		break;
	case 0x07:
		PORT_A2 = _value;
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
		HW_COLOR = _value;
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
	if (OUT_COMMIT_TIMER >= 0){
		if (--OUT_COMMIT_TIMER == 0)
		{
			PortOutCommit();
		}
	}

	if (PALLETE_COMMIT_TIMER >= 0) {
		if (--PALLETE_COMMIT_TIMER == 0)
		{
			SetColor(_colorIdx);
		}
	}
}
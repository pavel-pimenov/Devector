﻿// Emulation of Soviet KR1818WG93 (КР1818ВГ93) Floppy Disk Controller (WD1793 analog)

// based on:
// https://github.com/libretro/fmsx-libretro/blob/master/EMULib/WD1793.c

#pragma once
#ifndef DEV_FDC1793_H
#define DEV_FDC1793_H

#include <string>

namespace dev
{
	static constexpr int DRIVES_MAX = 4;

	struct FDisk
	{
	public:
		static constexpr int sidesPerDisk = 2;
		static constexpr int tracksPerSide = 82;
		static constexpr int sectorsPerTrack = 5;
		static constexpr int sectorLen = 1024;
		static constexpr int dataLen = sidesPerDisk * tracksPerSide * sectorsPerTrack * sectorLen;
	
	private:
		uint8_t data[dataLen];
		bool loaded = false;
	public:

		uint8_t header[6];		// current header, result of Seek()
		bool updated = false;

		FDisk();
		void Attach(const std::wstring& _path);
		auto GetData() -> uint8_t*;
		auto GetDisk() -> FDisk*;
	};

	class Fdc1793
	{
	public:
		enum class Port : int { 
			COMMAND = 0, 
			STATUS = 0, 
			TRACK = 1, 
			SECTOR = 2, 
			DATA = 3, 
			READY = 4, 
			SYSTEM = 4 
		};

	private:
		FDisk m_disks[DRIVES_MAX];

		uint8_t m_regs[5];	// Registers
		uint8_t m_drive;	// Current disk #
		uint8_t m_side;		// Current side #
		uint8_t m_track;	// Current track #
		uint8_t m_lastS;	// Last STEP direction
		uint8_t m_irq;		// 0x80: IRQ pending, 0x40: DRQ pending
		uint8_t m_wait;		// Expiration counter
		uint8_t m_cmd;		// Last command

		int  m_wrLength;	// Data left to write
		int  m_rdLength;	// Data left to read

		uint8_t* m_ptr;     // Pointer to data
		FDisk* m_disk = nullptr; // current disk images

		auto Seek(int _side, int _track, int _sideID, int _trackID, int _sectorID) -> uint8_t*;
		void Reset();

	public:
		void Attach(const int _driveIdx, const std::wstring& _path);
		auto Read(const Port _port) -> uint8_t;
		auto Write(const Port _port, uint8_t _val) -> uint8_t;
	};
}

#endif // DEV_FDC1793_H
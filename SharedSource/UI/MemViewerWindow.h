#pragma once
#ifndef DEV_MEM_VIEWER_WINDOW_H
#define DEV_MEM_VIEWER_WINDOW_H

#include <vector>

#include "../Devector/Types.h"
#include "Utils/Globals.h"
#include "Utils/ImGuiUtils.h"
#include "Ui/BaseWindow.h"
#include "../Devector/Hardware.h"

namespace dev
{
	class MemViewerWindow : public BaseWindow
	{
		static constexpr int DEFAULT_WINDOW_W = 512;
		static constexpr int DEFAULT_WINDOW_H = 512;

		static constexpr ImU32 BG_COLOR_ADDR = dev::IM_U32(0x303030FF);

		static constexpr ImVec4 COLOR_ADDR = dev::IM_VEC4(0x909090FF); 
		static constexpr ImVec4 COLOR_VALUE = dev::IM_VEC4(0xD4D4D4FF);

		Hardware& m_hardware;
		int64_t m_ccLast = -1; // to force the first stats update
		int64_t m_ccLastRun = 0;
		std::array<uint8_t, Memory::MEMORY_MAIN_LEN> m_ram;

		void UpdateData(const bool _isRunning);
		void DrawHex(const bool _isRunning);

	public:
		MemViewerWindow(Hardware& _hardware, const float* const _fontSizeP, const float* const _dpiScaleP);
		void Update();
	};

};

#endif // !DEV_MEM_VIEWER_WINDOW_H
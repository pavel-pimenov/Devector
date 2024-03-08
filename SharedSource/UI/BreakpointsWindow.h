#pragma once
#ifndef DEV_BREAKPOINTS_WINDOW_H
#define DEV_BREAKPOINTS_WINDOW_H

#include "Utils/Globals.h"
#include "Utils/ImGuiUtils.h"
#include "Ui/BaseWindow.h"
#include "../Devector/Hardware.h"

namespace dev
{
	class BreakpointsWindow : public BaseWindow
	{
		static constexpr int DEFAULT_WINDOW_W = 500;
		static constexpr int DEFAULT_WINDOW_H = 300;

		Hardware& m_hardware;

		void DrawTable();
		void DrawContextMenu(const char* _itemID);
/*
		void DrawStats();
		void DrawRegs();
		void DrawFlags();
		void DrawStack();
		void DrawHardware();
		void DrawProperty1(const std::string& _name, const std::string& _value, const ImVec2& _aligment = { 1.0f, 0.5f });
		void DrawProperty2(const std::string& _name, const std::string& _value);
		void DrawSeparator2(const std::string& _text);
		*/
	public:
		BreakpointsWindow(Hardware& _hardware,
			const float const* _fontSizeP, const float const* _dpiScaleP);

		void Update();

		void DrawProperty(const std::string& _name, const ImVec2& _aligment = { 0.5f, 0.5f });
	};

};

#endif // !DEV_BREAKPOINTS_WINDOW_H
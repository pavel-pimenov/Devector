#pragma once
#ifndef DEV_IMGUI_UTILS_H
#define DEV_IMGUI_UTILS_H

#include <string>
#include <list>
#include <format>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace dev 
{

#define DEV_COL32_R_MASK     0xFF000000
#define DEV_COL32_G_MASK     0x00FF0000
#define DEV_COL32_B_MASK     0x0000FF00
#define DEV_COL32_A_MASK     0x000000FF

#define DEV_COL32_R_SHIFT    24
#define DEV_COL32_G_SHIFT    16
#define DEV_COL32_B_SHIFT    8
#define DEV_COL32_A_SHIFT    0

	// Make the UI compact because there are so many fields
	void PushStyleCompact();
	void PopStyleCompact();
	void UpdatePropertyPrintStat(const char* _parameterName);

	void DrawTextSelectable(const std::string& _text);
	//void DrawTextSelectableColored(const ImVec4& col, const std::string& _text); 
	template <typename... Args>
	void DrawTextSelectableColored(const ImVec4& col, const std::string& _fmt, Args&&... args)
	{
		std::string text = std::vformat(_fmt, std::make_format_args(args...));

		ImVec2 text_size = ImGui::CalcTextSize(text.c_str(), text.c_str() + text.size());
		text_size.x = -FLT_MIN; // fill width (suppresses label)
		text_size.y += ImGui::GetStyle().FramePadding.y; // single pad

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0, 0 }); // make align with text height
		ImGui::PushStyleColor(ImGuiCol_FrameBg, { 0.f, 0.f, 0.f, 0.f }); // remove text input box

		ImGui::PushStyleColor(ImGuiCol_Text, col);


		ImGui::InputTextMultiline(
			"",
			const_cast<char*>(text.c_str()), // ugly const cast
			text.size() + 1, // needs to include '\0'
			text_size,
			ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoHorizontalScroll
		);
		ImGui::PopStyleColor();

		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	}
	void DrawHelpMarker(const char* _text);
	void DrawPopup(const char* _title, const char* _text);

	static constexpr ImVec4 IM_VEC4(const uint32_t col)
	{
		return ImVec4(
			((col & DEV_COL32_R_MASK) >> DEV_COL32_R_SHIFT) / 255.0f,
			((col & DEV_COL32_G_MASK) >> DEV_COL32_G_SHIFT) / 255.0f,
			((col & DEV_COL32_B_MASK) >> DEV_COL32_B_SHIFT) / 255.0f,
			((col & DEV_COL32_A_MASK) >> DEV_COL32_A_SHIFT) / 255.0f);
	}
	static constexpr ImU32 IM_U32(const uint32_t col)
	{
		return
			(col & DEV_COL32_R_MASK) >> DEV_COL32_R_SHIFT << 0 |
			(col & DEV_COL32_G_MASK) >> DEV_COL32_G_SHIFT << 8 |
			(col & DEV_COL32_B_MASK) >> DEV_COL32_B_SHIFT << 16 |
			(col & DEV_COL32_A_MASK) >> DEV_COL32_A_SHIFT << 24;
	}
}

#endif // !DEV_IMGUI_UTILS_H


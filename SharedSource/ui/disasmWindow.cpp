#include <format>

#include "DisasmWindow.h"
#include "Utils\StringUtils.h"

dev::DisasmWindow::DisasmWindow(dev::Hardware& _hardware, ImFont* fontComment)
    :
    BaseWindow(DEFAULT_WINDOW_W, DEFAULT_WINDOW_H),
    m_hardware(_hardware),
    m_fontComment(fontComment)
{}

void dev::DisasmWindow::Update()
{
    BaseWindow::Update();

	static bool open = true;
	ImGui::Begin("Disasm", &open, ImGuiWindowFlags_NoCollapse);

    DrawDebugControls();
	DrawSearch();
	DrawCode();

	ImGui::End();
}

void dev::DisasmWindow::DrawDebugControls()
{
    if (ImGui::Button("Step"))
    {
        UpdateDisasm();
    }
    ImGui::SameLine(); 
    if (ImGui::Button("Step 100"))
    {
        UpdateDisasm();
    }
    ImGui::SameLine();
    if (ImGui::Button("Step Frame"))
    {
        UpdateDisasm();
    }
}

void dev::DisasmWindow::UpdateDisasm()
{
    auto addr = m_hardware.m_cpu.m_sp;
    m_disasm = m_hardware.m_debugger.GetDisasm(addr, 1000, 6);
}

void dev::DisasmWindow::DrawSearch()
{
    ImGui::PushItemWidth(-100);
    ImGui::InputTextWithHint("##empty 1", "0x100", m_searchText, IM_ARRAYSIZE(m_searchText));
    ImGui::SameLine(); dev::DrawHelpMarker(
        "Search by a hexadecimal address in the format of 0x100 or 100,\n"
        "or by a case-sensitive label name.");
    ImGui::PopItemWidth();
}

// Make the UI compact because there are so many fields
static void PushStyleCompact()
{
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 5, 3 });
}
static void PopStyleCompact()
{
    ImGui::PopStyleVar(2);
}

static auto isDisasmTableOutOfWindow()
{
    ImVec2 cursorPos = ImGui::GetCursorPos();
    float remainingSpace = ImGui::GetWindowSize().y - cursorPos.y - 40.0f;

    return remainingSpace < 0;
}

static auto isLastLine()
{
    ImVec2 cursorPos = ImGui::GetCursorPos();
    float remainingSpace = ImGui::GetWindowSize().y - cursorPos.y - 40.0f - ImGui::GetTextLineHeightWithSpacing() - 1.0f;

    return remainingSpace <= 0;
}

void dev::DisasmWindow::DrawDisassembly()
{
    if (m_disasm.empty()) return;

    static int item_current_idx = 0;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 5, 0 });
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    static ImGuiTableFlags tbl_flags = ImGuiTableFlags_NoBordersInBody;

    const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

    if (ImGui::BeginTable("##disassembly", 1, tbl_flags | ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_ScrollY)) // (labels) or (comment) or (brk, addr, code, stats, consts)
    {
        ImGuiListClipper clipper;
        clipper.Begin(m_disasm.size());
        while (clipper.Step())
            for (int row_idx = clipper.DisplayStart; row_idx < clipper.DisplayEnd; row_idx++)
        //for (int row_idx = 0; row_idx < DISASM_LINES_MAX+10000; row_idx++)
        {
            //if (isDisasmTableOutOfWindow()) break;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            int line_idx = row_idx;//% DISASM_LINES_VISIBLE_MAX;

            // Parse the line into tokens
            auto line_splited = dev::Split(m_disasm[line_idx], '\t');

            // draw a comment
            if (line_splited[0][0] == ';')
            {
                if (ImGui::BeginTable("##comment", 3, tbl_flags)) // brk, addr, comment
                {
                    if (m_fontComment) {
                        ImGui::PushFont(m_fontComment);
                    }
                    PushStyleCompact();

                    ImGui::TableSetupColumn("Brk", ImGuiTableColumnFlags_WidthFixed, BRK_W);
                    ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed, ADDR_W);
                    ImGui::TableSetupColumn("command", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();

                    // the breakpoints and the execution cursor column
                    ImGui::TableNextColumn();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, DISASM_TBL_BG_COLOR_BRK);
                    const bool is_selected = (item_current_idx == line_idx);
                    if (ImGui::Selectable("", is_selected, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        item_current_idx = line_idx;
                    }

                    // the addr column
                    ImGui::TableNextColumn();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, DISASM_TBL_BG_COLOR_ADDR);
                    //ImGui::TextColored(DISASM_TBL_COLOR_LABEL_MINOR, "   ...");
                    
                    // the code column
                    ImGui::TableNextColumn();
                    ImGui::TextColored(DISASM_TBL_COLOR_COMMENT, line_splited[0].c_str());

                    PopStyleCompact();
                    // Revert to the default font
                    if (m_fontComment) {
                        ImGui::PopFont();
                    }
                    ImGui::EndTable();
                }
            }
            // draw labels
            else if (line_splited[0][0] != '0')
            {
                if (ImGui::BeginTable("##labels", 3, tbl_flags)) // brk, addr, labels
                {
                    PushStyleCompact();

                    ImGui::TableSetupColumn("Brk", ImGuiTableColumnFlags_WidthFixed, BRK_W);
                    ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed, ADDR_W);
                    ImGui::TableSetupColumn("command", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();

                    // the breakpoints and the execution cursor column
                    ImGui::TableNextColumn();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, DISASM_TBL_BG_COLOR_BRK);
                    const bool is_selected = (item_current_idx == line_idx);
                    if (ImGui::Selectable("", is_selected, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        item_current_idx = line_idx;
                    }

                    // the addr column
                    ImGui::TableNextColumn();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, DISASM_TBL_BG_COLOR_ADDR);
                    //ImGui::TextColored(DISASM_TBL_COLOR_LABEL_MINOR, "   ...");

                    // the code column
                    ImGui::TableNextColumn();
                    int i = 0;
                    for (auto const& label : line_splited)
                    {
                        // the label that matches the address and the code context
                        if (i == 0)
                        {
                            if (label[0] == '@')
                            {
                                ImGui::TextColored(DISASM_TBL_COLOR_LABEL_LOCAL, label.c_str());
                            }
                            else
                            {
                                ImGui::TextColored(DISASM_TBL_COLOR_LABEL_GLOBAL, label.c_str());
                            }
                        }
                        // all other labels that matches the address
                        else
                        {
                            ImGui::SameLine();
                            ImGui::TextColored(DISASM_TBL_COLOR_LABEL_MINOR, " %s", label.c_str());
                        }
                        i++;
                    }

                    PopStyleCompact();
                    ImGui::EndTable();
                }
            }
            else
            {
                if (ImGui::BeginTable("##code", 5, tbl_flags)) // brk, addr, code, stats, consts
                {
                    PushStyleCompact();

                    ImGui::TableSetupColumn("Brk", ImGuiTableColumnFlags_WidthFixed, BRK_W);
                    ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed, ADDR_W);
                    ImGui::TableSetupColumn("command", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("stats", ImGuiTableColumnFlags_WidthFixed, STATS_W);
                    ImGui::TableSetupColumn("consts");

                    ImGui::TableNextRow();

                    // the breakpoints and the execution cursor column
                    ImGui::TableNextColumn();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, DISASM_TBL_BG_COLOR_BRK);
                    const bool is_selected = (item_current_idx == line_idx);
                    if (ImGui::Selectable("", is_selected, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        item_current_idx = line_idx;
                    }

                    // the addr column
                    ImGui::TableNextColumn();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, DISASM_TBL_BG_COLOR_ADDR);
                    ImGui::TextColored(DISASM_TBL_COLOR_LABEL_MINOR, line_splited[0].c_str());

                    /*
                    bool tableHasFocus = ImGui::IsItemHovered();
                    if (tableHasFocus)
                    {
                        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && row_idx == 0)
                        {
                            scroll_line_offset -= 1;
                            scroll_line_offset = scroll_line_offset < 0 ? 0 : scroll_line_offset;
                        }
                    }
                    else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && isLastLine())
                    {
                        scroll_line_offset += 1;
                        //scroll_line_offset = scroll_line_offset < 0 ? 0 : scroll_line_offset;
                    }
                    */



                    // the code column
                    ImGui::TableNextColumn();
                    auto cmd_splitted = dev::Split(line_splited[1], ' ');
                    int i = 0;
                    for (const auto& cmd_parts : cmd_splitted)
                    {
                        if (i == 0)
                        {
                            // draw a mnenonic
                            ImGui::TextColored(DISASM_TBL_COLOR_MNEMONIC, "\t%s ", cmd_parts.c_str());
                        }
                        else
                        {
                            // draw an operand separator
                            if (i == 2)
                            {
                                ImGui::SameLine();
                                ImGui::TextColored(DISASM_TBL_COLOR_NUMBER, ", ");
                            }

                            // draw an operand
                            auto operands = dev::Split(cmd_parts, '=');

                            for (const auto& operand : operands)
                            {
                                if (operand[0] == '0')
                                {
                                    // draw a hexadecimal literal
                                    ImGui::SameLine();
                                    ImGui::TextColored(DISASM_TBL_COLOR_NUMBER, "%s", operand.c_str());
                                }
                                else if (cmd_parts.size() <= 2)
                                {
                                    // draw a reg
                                    ImGui::SameLine();
                                    ImGui::TextColored(DISASM_TBL_COLOR_REG, "%s", operand.c_str());
                                }
                                else
                                {
                                    // draw a const
                                    ImGui::SameLine();
                                    ImGui::TextColored(DISASM_TBL_COLOR_CONST, "%s = ", operand.c_str());
                                }

                            }
                        }
                        i++;
                    }

                    // the stats column
                    ImGui::TableNextColumn();
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(DISASM_TBL_BG_COLOR_ADDR));
                    ImGui::TextColored(DISASM_TBL_COLOR_ADDR, line_splited[2].c_str());

                    // the consts column
                    ImGui::TableNextColumn();
                    if (line_splited.size() >= 4)
                    {
                        ImGui::TextColored(DISASM_TBL_COLOR_ADDR, line_splited[3].c_str());
                    }

                    PopStyleCompact();
                    ImGui::EndTable();
                }
            }
        }
        
        ImGui::EndTable();
    }
    ImGui::PopStyleVar(2);
}

void dev::DisasmWindow::DrawCode() 
{
    DrawDisassembly();
}
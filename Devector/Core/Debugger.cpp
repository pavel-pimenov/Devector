#include "Debugger.h"
#include <string>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <vector>
#include "Utils/StrUtils.h"
#include "Utils/Utils.h"

dev::Debugger::Debugger(Hardware& _hardware)
	:
	m_hardware(_hardware),
	m_wpBreak(false),
	m_traceLog(),
	m_lastReadsAddrs(), m_lastWritesAddrs(),
	m_memLastRW(),
	m_lastReadsAddrsOld(), m_lastWritesAddrsOld(), 
	m_lastRWAddrsOut()
{
    Init();
}

void dev::Debugger::Init()
{
	m_checkBreakFunc = std::bind(&Debugger::CheckBreak, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	m_debugOnReadInstrFunc = std::bind(&Debugger::ReadInstr, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
	m_debugOnReadFunc = std::bind(&Debugger::Read, this, std::placeholders::_1, std::placeholders::_2);
	m_debugOnWriteFunc = std::bind(&Debugger::Write, this, std::placeholders::_1, std::placeholders::_2);

	m_hardware.AttachCheckBreak( &m_checkBreakFunc );
	m_hardware.AttachDebugOnReadInstr( &m_debugOnReadInstrFunc );
	m_hardware.AttachDebugOnRead( &m_debugOnReadFunc );
	m_hardware.AttachDebugOnWrite( &m_debugOnWriteFunc );

	Reset();

	m_breakpoints.clear();
	m_watchpoints.clear();

	m_hardware.Request(Hardware::Req::RUN);
}

dev::Debugger::~Debugger()
{
	m_hardware.AttachCheckBreak(nullptr);
	m_hardware.AttachDebugOnReadInstr(nullptr);
	m_hardware.AttachDebugOnRead(nullptr);
	m_hardware.AttachDebugOnWrite(nullptr);
}

void dev::Debugger::Reset()
{
	m_memRuns.fill(0);
	m_memReads.fill(0);
	m_memWrites.fill(0);

	m_lastWritesAddrs.fill(uint32_t(LAST_RW_NO_DATA));
	m_lastReadsAddrs.fill(uint32_t(LAST_RW_NO_DATA));
	m_lastWritesIdx = 0;
	m_lastReadsIdx = 0;
	m_memLastRW.fill(0);

	for (size_t i = 0; i < TRACE_LOG_SIZE; i++)
	{
		m_traceLog[i].Clear();
	}
	m_traceLogIdx = 0;
	m_traceLogIdxViewOffset = 0;
}

// a hardware thread
void dev::Debugger::ReadInstr(
	const GlobalAddr _globalAddr, const uint8_t _val, const uint8_t _dataH, const uint8_t _dataL, const Addr _hl)
{
	m_memRuns[_globalAddr]++;
	TraceLogUpdate(_globalAddr, _val, _dataH, _dataL, _hl);
}

// a hardware thread
void dev::Debugger::Read(
    const GlobalAddr _globalAddr, const uint8_t _val)
{
	m_memReads[_globalAddr]++;
    m_wpBreak |= CheckWatchpoint(Watchpoint::Access::R, _globalAddr, _val);

	std::lock_guard<std::mutex> mlock(m_lastRWMutex);
	m_lastReadsAddrs[m_lastReadsIdx++] = _globalAddr;
	m_lastReadsIdx %= LAST_RW_MAX;
}
// a hardware thread
void dev::Debugger::Write(const GlobalAddr _globalAddr, const uint8_t _val)
{
    m_memWrites[_globalAddr]++;
    m_wpBreak |= CheckWatchpoint(Watchpoint::Access::W, _globalAddr, _val);
	
	std::lock_guard<std::mutex> mlock(m_lastRWMutex);
	m_lastWritesAddrs[m_lastWritesIdx++] = _globalAddr;
	m_lastWritesIdx %= LAST_RW_MAX;
}


//////////////////////////////////////////////////////////////
//
// Disasm
//
//////////////////////////////////////////////////////////////

static const char* mnemonics[0x100] =
{
	"nop",	   "lxi b",  "stax b", "inx b",  "inr b", "dcr b", "mvi b", "rlc", "db 0x08", "dad b",  "ldax b", "dcx b",  "inr c", "dcr c", "mvi c", "rrc",
	"db 0x10", "lxi d",  "stax d", "inx d",  "inr d", "dcr d", "mvi d", "ral", "db 0x18", "dad d",  "ldax d", "dcx d",  "inr e", "dcr e", "mvi e", "rar",
	"db 0x20", "lxi h",  "shld",   "inx h",  "inr h", "dcr h", "mvi h", "daa", "db 0x28", "dad h",  "lhld",   "dcx h",  "inr l", "dcr l", "mvi l", "cma",
	"db 0x30", "lxi sp", "sta",    "inx sp", "inr m", "dcr m", "mvi m", "stc", "db 0x38", "dad sp", "lda",    "dcx sp", "inr a", "dcr a", "mvi a", "cmc",

	"mov b b", "mov b c", "mov b d", "mov b e", "mov b h", "mov b l", "mov b m", "mov b a", "mov c b", "mov c c", "mov c d", "mov c e", "mov c h", "mov c l", "mov c m", "mov c a",
	"mov d b", "mov d c", "mov d d", "mov d e", "mov d h", "mov d l", "mov d m", "mov d a", "mov e b", "mov e c", "mov e d", "mov e e", "mov e h", "mov e l", "mov e m", "mov e a",
	"mov h b", "mov h c", "mov h d", "mov h e", "mov h h", "mov h l", "mov h m", "mov h a", "mov l b", "mov l c", "mov l d", "mov l e", "mov l h", "mov l l", "mov l m", "mov l a",
	"mov m b", "mov m c", "mov m d", "mov m e", "mov m h", "mov m l", "hlt",     "mov m a", "mov a b", "mov a c", "mov a d", "mov a e", "mov a h", "mov a l", "mov a m", "mov a a",

	"add b", "add c", "add d", "add e", "add h", "add l", "add m", "add a", "adc b", "adc c", "adc d", "adc e", "adc h", "adc l", "adc m", "adc a",
	"sub b", "sub c", "sub d", "sub e", "sub h", "sub l", "sub m", "sub a", "sbb b", "sbb c", "sbb d", "sbb e", "sbb h", "sbb l", "sbb m", "sbb a",
	"ana b", "ana c", "ana d", "ana e", "ana h", "ana l", "ana m", "ana a", "xra b", "xra c", "xra d", "xra e", "xra h", "xra l", "xra m", "xra a",
	"ora b", "ora c", "ora d", "ora e", "ora h", "ora l", "ora m", "ora a", "cmp b", "cmp c", "cmp d", "cmp e", "cmp h", "cmp l", "cmp m", "cmp a",

	"rnz", "pop b",   "jnz", "jmp",  "cnz", "push b",   "adi", "rst 0x0", "rz",  "ret",     "jz",  "db 0xCB", "cz",  "call",    "aci", "rst 0x1",
	"rnc", "pop d",   "jnc", "out",  "cnc", "push d",   "sui", "rst 0x2", "rc",  "db 0xD9", "jc",  "in",      "cc",  "db 0xDD", "sbi", "rst 0x3",
	"rpo", "pop h",   "jpo", "xthl", "cpo", "push h",   "ani", "rst 0x4", "rpe", "pchl",    "jpe", "xchg",    "cpe", "db 0xED", "xri", "rst 0x5",
	"rp",  "pop PSW", "jp",  "di",   "cp",  "push PSW", "ori", "rst 0x6", "rm",  "sphl",    "jm",  "ei",      "cm",  "db 0xFD", "cpi", "rst 0x7"
};

// define the maximum number of bytes in a command
#define CMD_LEN_MAX 3

// array containing instruction lengths, indexed by an opcode
static const uint8_t cmd_lens[0x100] =
{
	1,3,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
	1,3,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
	1,3,3,1,1,1,2,1,1,1,3,1,1,1,2,1,
	1,3,3,1,1,1,2,1,1,1,3,1,1,1,2,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,3,3,3,1,2,1,1,1,3,1,3,3,2,1,
	1,1,3,2,3,1,2,1,1,1,3,2,3,1,2,1,
	1,1,3,1,3,1,2,1,1,1,3,1,3,1,2,1,
	1,1,3,1,3,1,2,1,1,1,3,1,3,1,2,1
};

auto GetMnemonic1(const uint8_t _opcode,
	const uint8_t _dataL, const uint8_t _dataH)
{
	std::string out(mnemonics[_opcode]);

	if (cmd_lens[_opcode] == 2)
	{
		out += std::format(" 0x{:02X}", _dataL);
	}
	else if (cmd_lens[_opcode] == 3)
	{
		auto dataW = _dataH << 8 | _dataL;
		out += std::format(" 0x{:04X}", dataW);
	}
	return out;
}

auto dev::Debugger::GetDisasmLine(const uint8_t _opcode,
	const uint8_t _dataL, const uint8_t _dataH) const
-> const std::string
{
	std::string out(mnemonics[_opcode]);

	if (cmd_lens[_opcode] == 2)
	{
		auto labelI = m_labels.find(_dataL);
		if (labelI != m_labels.end() && labelI->second.size() == 1) 
		{
			out += std::format(" {};0x{:02X}", labelI->second[0], _dataL);
		}
		else {
			out += std::format(" 0x{:02X}", _dataL);
		}
	}
	else if (cmd_lens[_opcode] == 3)
	{
		auto dataW = _dataH << 8 | _dataL;
		std::string constant;

		auto labelI = m_labels.find(dataW);
		auto constI = m_consts.find(dataW);
		if (labelI != m_labels.end() && labelI->second.size() == 1) {
			constant = labelI->second[0];
		}
		else if (constI != m_consts.end() && constI->second.size() == 1)
		{
			constant = constI->second[0];
		}
		if (!constant.empty()) 
		{
			out += std::format(" {};0x{:04X}", constant, dataW);
		}
		else {
			out += std::format(" 0x{:04X}", dataW);
		}
	}
	return out;
}

// 0 - call
// 1 - c*
// 2 - rst
// 3 - pchl
// 4 - jmp, 
// 5 - j*
// 6 - ret, r*
// 7 - other
static const uint8_t opcode_types[0x100] =
{
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,

	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,

	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,

	6, 7, 5, 4, 1, 7, 7, 2, 6, 6, 5, 7, 1, 0, 7, 2,
	6, 7, 5, 7, 1, 7, 7, 2, 6, 7, 5, 7, 1, 7, 7, 2,
	6, 7, 5, 7, 1, 7, 7, 2, 6, 3, 5, 7, 1, 7, 7, 2,
	6, 7, 5, 7, 1, 7, 7, 2, 6, 7, 5, 7, 1, 7, 7, 2,
};

#define OPCODE_TYPE_MAX 7

// returns the type of an instruction
inline uint8_t get_opcode_type(const uint8_t _opcode)
{
	return opcode_types[_opcode];
}

#define OPCODE_PCHL 0xE9
#define OPCODE_HLT 0x76

// disassembles a data byte
auto dev::Debugger::GetDisasmLineDb(const uint8_t _data) const
->const std::string
{
	return std::format("DB 0x{:02X}", _data);
}

// shifts the addr by _instructionsOffset instruction counter
// if _instructionsOffset=3, it returns the addr of a third instruction after _addr, and vice versa
#define MAX_ATTEMPTS 41 // max attemts to find an addr of an instruction before _addr 
// check the perf of this func
auto dev::Debugger::GetAddr(const Addr _addr, const int _instructionOffset) const
-> Addr
{
	int instructions = dev::Abs(_instructionOffset);

	if (_instructionOffset > 0)
	{
		Addr addr = _addr;
		for (int i = 0; i < instructions; i++)
		{
			auto resOpcode = m_hardware.Request(Hardware::Req::GET_BYTE_RAM, { { "addr", addr } });
			uint8_t opcode = resOpcode->at("data");

			auto cmdLen = cmd_lens[opcode];
			addr = addr + cmdLen;
		}
		return addr;
	}
	else if (_instructionOffset < 0)
	{
		std::vector<Addr> possibleDisasmStartAddrs;

		int disasmStartAddr = _addr - instructions * CMD_LEN_MAX;

		for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++)
		{
			int addr = disasmStartAddr;
			int currentInstruction = 0;

			while (addr < _addr && currentInstruction < instructions)
			{
				auto resOpcode = m_hardware.Request(Hardware::Req::GET_BYTE_RAM, { { "addr", addr } });
				uint8_t opcode = resOpcode->at("data");

				auto cmdLen = cmd_lens[opcode];
				addr = addr + cmdLen;
				currentInstruction++;
			}

			// if we reached the _addr address with counted instructions equals instructions
			if (addr == _addr && currentInstruction == instructions)
			{
				possibleDisasmStartAddrs.push_back((Addr)disasmStartAddr);
			}
			disasmStartAddr++;

			// return _addr if it fails to find a seaquence legal instructons
			if (disasmStartAddr + instructions > _addr)
			{
				break;
			}
		}
		if (possibleDisasmStartAddrs.empty()) return _addr;

		// get the best result basing on the execution counter
		for (const auto possibleDisasmStartAddr : possibleDisasmStartAddrs)
		{
			if (m_memRuns[possibleDisasmStartAddr] > 0) return possibleDisasmStartAddr;
		}
		return possibleDisasmStartAddrs[0];
	}

	return _addr;
}

// _instructionOffset defines the start address of the disasm. 
// _instructionOffset = 0 means the start address is the _addr, 
// _instructionOffset = -5 means the start address is 5 instructions prior the _addr, and vise versa.
void dev::Debugger::UpdateDisasm(const Addr _addr, const size_t _linesNum, const int _instructionOffset)
{
	if (_linesNum <= 0) return;
	size_t lines = dev::Max(_linesNum, Disasm::DISASM_LINES_MAX);
	m_disasm.Init(_linesNum);

	// calculate a new address that precedes the specified 'addr' by the instructionOffset
	Addr addr = GetAddr(_addr, _instructionOffset);

	if (_instructionOffset < 0 && addr == _addr)
	{
		// _instructionOffset < 0 means we want to disasm several intructions prior the _addr.
		// if the GetAddr output addr is equal to input _addr, that means 
		// there is no valid instructions fit into the range (_addr+_instructionOffset, _addr) 
		// and that means a data blob is ahead
		addr += (Addr)_instructionOffset;

		for (; m_disasm.GetLineIdx() < -_instructionOffset;)
		{
			m_disasm.AddLabes(addr, m_labels);
			m_disasm.AddComment(addr, m_comments);

			uint8_t db = m_hardware.Request(Hardware::Req::GET_BYTE_RAM, { { "addr", addr } })->at("data");
			auto breakpointStatus = GetBreakpointStatus(addr);
			GlobalAddr globalAddr = m_hardware.Request(Hardware::Req::GET_GLOBAL_ADDR_RAM, { { "addr", addr } })->at("data");
			addr += m_disasm.AddDb(addr, db, m_consts,
				m_memRuns[globalAddr], m_memReads[globalAddr], m_memWrites[globalAddr], breakpointStatus);
		}
	}

	while (!m_disasm.IsDone())
	{
		m_disasm.AddLabes(addr, m_labels);
		m_disasm.AddComment(addr, m_comments);

		uint32_t cmd = m_hardware.Request(Hardware::Req::GET_THREE_BYTES_RAM, { { "addr", addr } })->at("data");

		GlobalAddr globalAddr = m_hardware.Request(Hardware::Req::GET_GLOBAL_ADDR_RAM, { { "addr", addr } })->at("data");
		auto breakpointStatus = GetBreakpointStatus(globalAddr);

		addr += m_disasm.AddCode(addr, cmd, m_labels, m_consts,
			m_memRuns[globalAddr], m_memReads[globalAddr], m_memWrites[globalAddr], breakpointStatus);
	}
}

bool IsConstLabel(const char* _s)
{
	// Iterate through each character in the string
	while (*_s != '\0') {
		// Check if the character is uppercase or underscore
		if (!(std::isupper(*_s) || *_s == '_' || (*_s >= '0' && *_s <= '9'))) {
			return false; // Not all characters are capital letters or underscores
		}
		++_s; // Move to the next character
	}
	return true; // All characters are capital letters or underscores
}

void dev::Debugger::LoadDebugData(const std::wstring& _path)
{
	// check if the file exists
	auto romDir = dev::GetDir(_path);
	auto debugPath = romDir + L"\\" + dev::GetFilename(_path) + L".json";
	if (!dev::IsFileExist(debugPath)) return;

	ResetLabels();
	
	auto debugDataJ = LoadJson(debugPath);

	// add labels
	if (debugDataJ.contains("labels")){
		for(auto& [str, addrS] : debugDataJ["labels"].items())
		{
			Addr addr = dev::StrHexToInt(addrS.get<std::string>().c_str());
			m_labels.emplace(addr, AddrLabels{}).first->second.emplace_back(str);
		}
	}
	// add consts
	if (debugDataJ.contains("consts")){
		for(auto& [str, addrS] : debugDataJ["consts"].items())
		{
			Addr addr = dev::StrHexToInt(addrS.get<std::string>().c_str());
			m_consts.emplace(addr, AddrLabels{}).first->second.emplace_back(str);
		}
	}
	// add comments
	if (debugDataJ.contains("comments")) {
		for (auto& [addrS, str] : debugDataJ["comments"].items())
		{
			Addr addr = dev::StrHexToInt(addrS.c_str());
			m_comments.emplace(addr, str);
		}
	}
}

void dev::Debugger::ResetLabels()
{
	m_labels.clear();
	m_consts.clear();
	m_comments.clear();
}

auto dev::Debugger::GetComment(const Addr _addr) const
-> const std::string*
{
	auto commentI = m_comments.find(_addr);
	return (commentI != m_comments.end()) ? &commentI->second : nullptr;
}

void dev::Debugger::SetComment(const Addr _addr, const std::string& _comment)
{
	m_comments[_addr] = _comment;
}

//////////////////////////////////////////////////////////////
//
// Tracelog
//
//////////////////////////////////////////////////////////////

// a hardware thread
void dev::Debugger::TraceLogUpdate(const GlobalAddr _globalAddr, 
	const uint8_t _opcode, const uint8_t _dataH, const uint8_t _dataL, const Addr _hl)
{
	// skip repeataive HLT
	if (_opcode == OPCODE_HLT && m_traceLog[m_traceLogIdx].m_opcode == OPCODE_HLT) {
		return;
	}

	m_traceLogIdx = --m_traceLogIdx % TRACE_LOG_SIZE;
	m_traceLog[m_traceLogIdx].m_globalAddr = _globalAddr;
	m_traceLog[m_traceLogIdx].m_opcode = _opcode;
	m_traceLog[m_traceLogIdx].m_dataL = _opcode != OPCODE_PCHL ? _dataL : _hl & 0xff;
	m_traceLog[m_traceLogIdx].m_dataH = _opcode != OPCODE_PCHL ? _dataH : _hl >> 8;
}

auto dev::Debugger::GetTraceLog(const int _offset, const size_t _lines, const size_t _filter)
-> const Disasm::Lines*
{
	//size_t filter = dev::Min(_filter, OPCODE_TYPE_MAX);
	//size_t offset = dev::Max(_offset, 0);

	//DisasmLines out;

	//for (int i = 0; i < offset; i++)
	//{
	//	m_traceLogIdxViewOffset = TraceLogNextLine(m_traceLogIdxViewOffset, _offset < 0, filter);
	//}

	//size_t idx = m_traceLogIdx + m_traceLogIdxViewOffset;
	//size_t idx_last = m_traceLogIdx + TRACE_LOG_SIZE - 1;
	//size_t line = 0;
	//size_t first_line_idx = TraceLogNearestForwardLine(m_traceLogIdx, filter);

	//for (; idx <= idx_last && line < _lines; idx++)
	//{
	//	auto globalAddr = m_traceLog[idx % TRACE_LOG_SIZE].m_globalAddr;
	//	if (globalAddr < 0) break;

	//	if (get_opcode_type(m_traceLog[idx % TRACE_LOG_SIZE].m_opcode) <= filter)
	//	{
	//		std::string str = m_traceLog[idx % TRACE_LOG_SIZE].ToStr();

	//		const Addr operand_addr = m_traceLog[idx % TRACE_LOG_SIZE].m_dataH << 8 | m_traceLog[idx % TRACE_LOG_SIZE].m_dataL;
	//		std::string constsS = LabelsToStr(operand_addr, LABEL_TYPE_ALL);

	//		DisasmLine lineDisasm(DisasmLine::Type::CODE, Addr(globalAddr), str, 0,0,0, constsS);
	//		out.emplace_back(std::move(lineDisasm));

	//		line++;
	//	}
	//}

	//return out;
	return nullptr;
}

auto dev::Debugger::TraceLogNextLine(const int _idxOffset, const bool _reverse, const size_t _filter) const
->int
{
	size_t filter = dev::Min(_filter, OPCODE_TYPE_MAX);

	size_t idx = m_traceLogIdx + _idxOffset;
	size_t idx_last = m_traceLogIdx + TRACE_LOG_SIZE - 1;

	int dir = _reverse ? -1 : 1;
	// for a forward scrolling we need to go to the second line if we were not exactly at the filtered line
	bool forward_second_search = false;
	size_t first_line_idx = idx;

	for (; idx >= m_traceLogIdx && idx <= idx_last; idx += dir)
	{
		if (get_opcode_type(m_traceLog[idx % TRACE_LOG_SIZE].m_opcode) <= filter)
		{
			if ((!_reverse && !forward_second_search) ||
				(_reverse && idx == first_line_idx && !forward_second_search))
			{
				forward_second_search = true;
				continue;
			}
			else
			{
				return (int)(idx - m_traceLogIdx);
			}
		}
	}

	return _idxOffset; // fails to reach the next line
}

auto dev::Debugger::TraceLogNearestForwardLine(const size_t _idx, const size_t _filter) const
->int
{
	size_t filter = _filter > OPCODE_TYPE_MAX ? OPCODE_TYPE_MAX : _filter;

	size_t idx = _idx;
	size_t idx_last = m_traceLogIdx + TRACE_LOG_SIZE - 1;

	for (; idx >= m_traceLogIdx && idx <= idx_last; idx++)
	{
		if (get_opcode_type(m_traceLog[idx % TRACE_LOG_SIZE].m_opcode) <= filter)
		{
			return (int)idx;
		}
	}

	return (int)_idx; // fails to reach the nearest line
}

auto dev::Debugger::TraceLog::ToStr() const
->std::string
{
	return GetMnemonic1(m_opcode, m_dataL, m_dataH);
}

void dev::Debugger::TraceLog::Clear()
{
	m_globalAddr = -1;
	m_opcode = 0;
	m_dataL = 0;
	m_dataH = 0;
}

//////////////////////////////////////////////////////////////
//
// Debug flow
//
//////////////////////////////////////////////////////////////

// m_hardware thread
bool dev::Debugger::CheckBreak(const Addr _addr, const uint8_t _mappingModeRam, const uint8_t _mappingPageRam)
{
	if (m_wpBreak)
	{
		m_wpBreak = false;
		ResetWatchpoints();
		return true;
	}

	auto break_ = CheckBreakpoints(_addr, _mappingModeRam, _mappingPageRam);

	return break_;
}

//////////////////////////////////////////////////////////////
//
// Breakpoints
//
//////////////////////////////////////////////////////////////

void dev::Debugger::SetBreakpointStatus(const Addr _addr, const Breakpoint::Status _status)
{
	{
		std::lock_guard<std::mutex> mlock(m_breakpointsMutex);
		auto bpI = m_breakpoints.find(_addr);
		if (bpI != m_breakpoints.end()) {
			bpI->second.SetStatus(_status);
			return;
		}
	}
	AddBreakpoint(_addr);
}

void dev::Debugger::AddBreakpoint(const Addr _addr, 
	const uint8_t _mappingPages,
	const Breakpoint::Status _status,
	const bool _autoDel, const std::string& _comment)
{
	std::lock_guard<std::mutex> mlock(m_breakpointsMutex);
	auto bpI = m_breakpoints.find(_addr);
	if (bpI != m_breakpoints.end())
	{
		bpI->second.Update(_addr, _mappingPages, _status, _autoDel, _comment);
		return;
	}

	m_breakpoints.emplace(_addr, std::move(Breakpoint(_addr, _mappingPages, _status, _autoDel, _comment)));
}

void dev::Debugger::DelBreakpoint(const Addr _addr)
{
	std::lock_guard<std::mutex> mlock(m_breakpointsMutex);
	auto bpI = m_breakpoints.find(_addr);
	if (bpI != m_breakpoints.end())
	{
		m_breakpoints.erase(bpI);
	}
}

void dev::Debugger::DelBreakpoints()
{
	std::lock_guard<std::mutex> mlock(m_breakpointsMutex);
	m_breakpoints.clear();
}

bool dev::Debugger::CheckBreakpoints(const Addr _addr, const uint8_t _mappingModeRam, const uint8_t _mappingPageRam)
{
	std::lock_guard<std::mutex> mlock(m_breakpointsMutex);
	auto bpI = m_breakpoints.find(_addr);
	if (bpI == m_breakpoints.end()) return false;
	auto status = bpI->second.CheckStatus(_mappingModeRam, _mappingPageRam);
	if (bpI->second.GetData().autoDel) m_breakpoints.erase(bpI);
	return status;
}

auto dev::Debugger::GetBreakpoints() -> const Breakpoints
{
	Breakpoints out;
	std::lock_guard<std::mutex> mlock(m_breakpointsMutex);
	for (const auto& [addr, bp] : m_breakpoints)
	{
		out.insert({ addr, bp });
	}
	return out;
}

auto dev::Debugger::GetBreakpointStatus(const Addr _addr)
-> const Breakpoint::Status
{
	std::lock_guard<std::mutex> mlock(m_breakpointsMutex);
	auto bpI = m_breakpoints.find(_addr);

	return bpI == m_breakpoints.end() ? Breakpoint::Status::DELETED : bpI->second.GetData().status;
}

//////////////////////////////////////////////////////////////
//
// Watchpoint
//
//////////////////////////////////////////////////////////////

void dev::Debugger::AddWatchpoint(
	const dev::Id _id, const Watchpoint::Access _access, 
	const GlobalAddr _globalAddr, const Watchpoint::Condition _cond,
	const uint16_t _value, const Watchpoint::Type _type, const int _len, const bool _active, const std::string& _comment)
{
	std::lock_guard<std::mutex> mlock(m_watchpointsMutex);
	
	auto wpI = m_watchpoints.find(_id);
	if (wpI != m_watchpoints.end())
	{
		wpI->second.Update(_access, _globalAddr, _cond, _value, _type, _len, _active, _comment);
	}
	else {
		auto wp = Watchpoint(_access, _globalAddr, _cond, _value, _type, _len, _active, _comment);
		m_watchpoints.emplace(wp.GetId(), std::move(wp));
	}
}

void dev::Debugger::DelWatchpoint(const dev::Id _id)
{
	std::lock_guard<std::mutex> mlock(m_watchpointsMutex);

	auto bpI = m_watchpoints.find(_id);
	if (bpI != m_watchpoints.end())
	{
		m_watchpoints.erase(bpI);
	}
}

void dev::Debugger::DelWatchpoints()
{
	std::lock_guard<std::mutex> mlock(m_watchpointsMutex);
	m_watchpoints.clear();
}

// a hardware thread
bool dev::Debugger::CheckWatchpoint(const Watchpoint::Access _access, const GlobalAddr _globalAddr, const uint8_t _value)
{
	std::lock_guard<std::mutex> mlock(m_watchpointsMutex);
	
	auto wpI = std::find_if(m_watchpoints.begin(), m_watchpoints.end(), 
		[_access, _globalAddr, _value](Watchpoints::value_type& pair) 
		{
			return pair.second.Check(_access, _globalAddr, _value);
		});

	if (wpI == m_watchpoints.end()) return false;

	return true;
}

void dev::Debugger::ResetWatchpoints()
{
	std::lock_guard<std::mutex> mlock(m_watchpointsMutex);
	for (auto& [id, watchpoint] : m_watchpoints)
	{
		watchpoint.Reset();
	}
}

auto dev::Debugger::GetWatchpoints() -> const Watchpoints
{
	Watchpoints out;
	std::lock_guard<std::mutex> mlock(m_breakpointsMutex);
	for (const auto& [id, wp] : m_watchpoints)
	{
		out.emplace(id, Watchpoint{ wp });
	}
	return out;
}

//////////////////////////////////////////////////////////////
//
// Utils
//
//////////////////////////////////////////////////////////////

auto dev::Debugger::GetDisasmLabels(const Addr _addr) const
-> const std::string
{
	std::string out;

	if (m_labels.contains(_addr))
	{
		int i = 0;
		for (const auto& label : m_labels.at(_addr))
		{
			out += label;
			if (i == 0)
			{
				out += ":\t";
			}
			else
			{
				out += ", ";
			}
			i++;
		}
	}
	return out;
}

auto dev::Debugger::LabelsToStr(const Addr _addr, int _labelTypes) const
-> const std::string
{
	std::string out;

	if (_labelTypes & LABEL_TYPE_LABEL && m_labels.contains(_addr))
	{
		for (const auto& label : m_labels.at(_addr))
		{
			out += label + ", ";
		}
	}
	if (_labelTypes & LABEL_TYPE_CONST && m_consts.contains(_addr))
	{
		for (const auto& label : m_consts.at(_addr))
		{
			out += label + ", ";
		}
	}

	return out;
}

//////////////////////////////////////////////////////////////
//
// Requests
//
//////////////////////////////////////////////////////////////

void dev::Debugger::UpdateLastRW()
{
	// remove old stats
	for (int i = 0; i < m_lastReadsAddrsOld.size(); i++) 
	{
		auto globalAddrLastRead = m_lastReadsAddrsOld[i];
		if (globalAddrLastRead != LAST_RW_NO_DATA) {
			m_memLastRW[globalAddrLastRead] = 0;
		}
		auto globalAddrLastWrite = m_lastWritesAddrsOld[i];
		if (globalAddrLastWrite != LAST_RW_NO_DATA) {
			m_memLastRW[globalAddrLastWrite] = 0;
		}
	}

	// copy new reads stats
	std::lock_guard<std::mutex> mlock(m_lastRWMutex);
	uint16_t readsIdx = m_lastReadsIdx;
	for (auto globalAddr : m_lastReadsAddrs){
		if (globalAddr != LAST_RW_NO_DATA) 
		{
			auto val = m_memLastRW[globalAddr] & 0xFFFF0000; // remove reads, keep writes
			m_memLastRW[globalAddr] = val | static_cast<uint16_t>(LAST_RW_MAX - readsIdx) % LAST_RW_MAX;
		}
		readsIdx--;
	}

	// copy new writes stats
	uint16_t writesIdx = m_lastWritesIdx;
	for (auto globalAddr : m_lastWritesAddrs){
		if (globalAddr != LAST_RW_NO_DATA) 
		{
			auto val = m_memLastRW[globalAddr] & 0x0000FFFF; // remove writes, keep reads
			m_memLastRW[globalAddr] = val | (static_cast<uint16_t>(LAST_RW_MAX - writesIdx) % LAST_RW_MAX)<<16;
		}
		writesIdx--;
	}
	
	m_lastReadsAddrsOld = m_lastReadsAddrs;
	m_lastWritesAddrsOld = m_lastWritesAddrs;
}

auto dev::Debugger::GetLastRW() -> const MemLastRW* { return &m_memLastRW; }
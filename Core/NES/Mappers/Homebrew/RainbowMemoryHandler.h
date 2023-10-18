#pragma once
#include "pch.h"
#include "NES/INesMemoryHandler.h"
#include "NES/NesConsole.h"

class RainbowMemoryHandler : public INesMemoryHandler
{
	NesConsole* _console = nullptr;
	uint8_t _ppuRegs[8] = {};
	uint8_t _oamOffset;
	uint8_t _sprY[64];
	uint16_t _oamWrite;

public:
	RainbowMemoryHandler(NesConsole* console)
	{
		_console = console;
		_oamOffset = 2;
	}

	uint8_t GetPpuReg(uint16_t addr)
	{
		return _ppuRegs[addr & 0x07];
	}

	uint8_t GetSpriteYPos(uint8_t spriteIndex)
	{
		return _sprY[spriteIndex];
	}

	void GetMemoryRanges(MemoryRanges& ranges) override {}
	uint8_t ReadRam(uint16_t addr) override
	{
		uint8_t ramValue = _console->GetMemoryManager()->GetInternalRam()[addr];
		uint16_t start = _oamOffset << 8;
		uint16_t end = start | 0xFF;
		if(addr >= start && addr <= end && (addr & 0x03) == 0) {
			uint8_t spriteIndex = (addr >> 2) & 0x3F;
			_sprY[spriteIndex] = ramValue;
		}
		return ramValue;
	}

	void WriteRam(uint16_t addr, uint8_t value) override
	{
		_console->GetPpu()->WriteRam(addr, value);

		if(addr >= 0x2000 && addr <= 0x2007) {
			_ppuRegs[addr & 0x07] = value;
			if(addr == 0x2004) {
				uint8_t spriteIndex = (_oamWrite >> 2) & 0x3f;
				_sprY[spriteIndex] = value;
				_oamWrite++;
			}
		}

		if(addr == 0x4014) {
			_oamOffset = value;
			_oamWrite = 0;
		}
	}
};

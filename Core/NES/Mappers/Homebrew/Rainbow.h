#pragma once
#include "pch.h"
#include "NES/BaseMapper.h"
#include "NES/BaseNesPpu.h"
#include "NES/Mappers/Homebrew/Flash3_3V.h"
#include "NES/Mappers/Homebrew/RainbowESP.h"
#include "NES/Mappers/Homebrew/RainbowBootromChr.h"
#include "NES/Mappers/Homebrew/RainbowMemoryHandler.h"
#include "NES/Mappers/Audio/RainbowAudio.h"
#include "Shared/BatteryManager.h"
#include "Utilities/Patches/IpsPatcher.h"
#include "Utilities/Serializer.h"

// mapper 682 - Rainbow2 board v1.0 revA and v1.1 by Broke Studio
//
// documentation available here: https://github.com/BrokeStudio/rainbow-lib

#define MAPPER_PLATFORM_PCB 0
#define MAPPER_PLATFORM_EMU 1
#define MAPPER_PLATFORM_WEB 2
#define MAPPER_VERSION_PROTOTYPE 0
#define MAPPER_VERSION		(MAPPER_PLATFORM_EMU << 5) | MAPPER_VERSION_PROTOTYPE

#define MIRR_VERTICAL		0b00 // VRAM
#define MIRR_HORIZONTAL		0b01 // VRAM
#define MIRR_ONE_SCREEN		0b10 // VRAM [+ CHR-RAM]
#define MIRR_FOUR_SCREEN	0b11 // CHR-RAM

#define PRG_ROM_MODE_0		0b000	// 32K
#define PRG_ROM_MODE_1		0b001	// 16K + 16K
#define PRG_ROM_MODE_2		0b010	// 16K + 8K + 8K
#define PRG_ROM_MODE_3		0b011	// 8K + 8K + 8K + 8K
#define PRG_ROM_MODE_4		0b100	// 4K + 4K + 4K + 4K + 4K + 4K + 4K + 4K

#define PRG_RAM_MODE_0		0b0	// 8K
#define PRG_RAM_MODE_1		0b1	// 4K

#define CHR_CHIP_ROM			0b00  // CHR-ROM
#define CHR_CHIP_RAM			0b01  // CHR-RAM
#define CHR_CHIP_FPGA_RAM	0b10  // FPGA-RAM

#define CHR_MODE_0			0b000 // 8K mode
#define CHR_MODE_1			0b001 // 4K mode
#define CHR_MODE_2			0b010 // 2K mode
#define CHR_MODE_3			0b011 // 1K mode
#define CHR_MODE_4			0b100 // 512B mode

#define CHIP_TYPE_PRG		0
#define CHIP_TYPE_CHR		1

#define NT_CIRAM				0b00
#define NT_CHR_RAM			0b01
#define NT_FPGA_RAM			0b10
#define NT_CHR_ROM			0b11

class RNBW : public BaseMapper
{
private:
	unique_ptr<RainbowMemoryHandler> _rainbowMemoryHandler;

	// PRG-ROM/RAM/FPGA
	static constexpr int FpgaRamSize = 0x2000;
	static constexpr int FpgaFlashSize = 0x3000;
	uint8_t _prgRomMode, _prgRamMode;
	uint16_t _prg[11]; // 0: $5000, 1: $6000, 2: $7000, 3: $8000, 4: $9000, etc
	int32_t _realWorkRamSize;
	int32_t _realSaveRamSize;
	uint8_t* _fpgaRam = nullptr;
	uint8_t* _fpgaFlash = nullptr;
	unique_ptr<Flash3_3V> _prgFlash;
	vector<uint8_t> _orgPrgRom;
	bool _bootrom = false;

	// CHR-ROM/RAM
	uint8_t _chrChip, _chrSprExtMode, _chrMode;
	uint16_t _chr[16];
	uint8_t _ntBank[5], _ntControl[5];
	unique_ptr<Flash3_3V> _chrFlash;
	vector<uint8_t> _orgChrRom;

	// Sprite Extended Mode
	uint8_t _oamOffset;
	uint8_t _spriteBankOffset;
	uint8_t _spriteBank[64];
	uint8_t _spriteYOrder[64];
	uint8_t _spriteIndex;
	uint8_t _spriteCounter;
	uint8_t _spriteOrderingStep = 0;
	bool _spriteOrdering = false;

	// Extended modes
	uint8_t _bgBankOffset;
	uint16_t _extModeLastNametableFetch = 0;
	int8_t _extModeLastFetchCounter = 0;
	uint8_t _extModeLastNtIdx;
	uint8_t _extModeLast1kDest;
	uint8_t _extModeLastMode;
	uint8_t _extModeLastValue;
	uint8_t _windowExtModeLast1kDest;
	uint8_t _windowExtModeLastMode;
	uint8_t _windowExtModeLastValue;

	// Window mode
	bool _windowModeEnabled = false;
	bool _windowInSplitRegion = false;
	uint8_t _windowXStartTile;
	uint8_t _windowXEndTile;
	uint8_t _windowYStart;
	uint8_t _windowYEnd;
	uint8_t _windowXScroll;
	uint8_t _windowYScroll;

	uint32_t _splitTile = 0;
	int32_t _splitTileNumber = 0;

	uint8_t _windowSplitXPos = 0;
	uint8_t _windowSplitYPos = 0;

	// ESP/WIFI
	BrokeStudioFirmware* _esp = NULL;
	bool _espEnable;
	bool _espIrqEnable;
	bool _espHasReceivedMessage;
	bool _espMessageSent;
	uint8_t _espRxAddress, _espTxAddress, _espRxIndex;

	// MISC
	uint8_t _audioOutput;
	size_t _miscRomSize = 0;

	// AUDIO EXPANSION
	unique_ptr<RNBWAudio> _audio;

	// SCANLINE IRQ
	bool _scanlineIrqEnable;
	bool _scanlineIrqPendingLast;
	bool _scanlineIrqPending;
	bool _scanlineIrqInFrame;
	bool _scanlineIrqHblank;
	uint8_t _scanlineIrqCounter;
	uint8_t _scanlineIrqLatch;
	uint8_t _scanlineIrqOffset;
	uint8_t _scanlineIrqJitterCounter;
	uint8_t _scanlineCounter;
	uint16_t _dotCounter;

	bool _needInFrame = false;
	bool _ppuInFrame = false;
	uint8_t _ppuIdleCounter = 0;
	uint16_t _lastPpuReadAddr = 0;
	uint8_t _ntReadCounter = 0;

	// CPU CYCLE IRQ
	bool _cpuCycleIrqEnable, _cpuCycleIrqReset, _cpuCycleIrqPending;
	uint16_t _cpuCycleIrqLatch;
	int32_t _cpuCycleIrqCount;

protected:
	uint16_t GetPrgPageSize() override { return 0x1000; }
	uint16_t GetChrPageSize() override { return 0x200; }
	uint16_t GetChrRamPageSize() override { return 0x200; }

	uint16_t RegisterStartAddress() override { return 0x4100; }
	uint16_t RegisterEndAddress() override { return 0xFFFF; }

	uint32_t GetWorkRamPageSize() override { return 0x2000; }
	uint32_t GetSaveRamPageSize() override { return 0x2000; }

	bool AllowRegisterRead() override { return true; }
	bool ForceSaveRamSize() override { return true; }
	bool ForceWorkRamSize() override { return true; }

	uint32_t GetSaveRamSize() override
	{
		uint32_t size;
		if(IsNes20()) {
			size = _romInfo.Header.GetSaveRamSize();
		} else if(_romInfo.IsInDatabase) {
			size = _romInfo.DatabaseInfo.SaveRamSize;
		} else {
			//Emulate as if a single 32k block of work/save ram existed (+ 8kb of FPGA-RAM)
			size = _romInfo.HasBattery ? 0x8000 : 0;
		}
		_realSaveRamSize = size;
		if(HasBattery()) {
			//If there's a battery on the board, FPGA-RAM gets saved, too.
			size += RNBW::FpgaRamSize;
			size += RNBW::FpgaFlashSize;
		}

		return size;
	}

	uint32_t GetWorkRamSize() override
	{
		uint32_t size;
		if(IsNes20()) {
			size = _romInfo.Header.GetWorkRamSize();
		} else if(_romInfo.IsInDatabase) {
			size = _romInfo.DatabaseInfo.WorkRamSize;
		} else {
			//Emulate as if a single 32k block of work/save ram existed (+ 8kb of FPGA-RAM)
			size = _romInfo.HasBattery ? 0 : 0x8000;
		}
		_realWorkRamSize = size;
		if(!HasBattery()) {
			size += RNBW::FpgaRamSize;
			size += RNBW::FpgaFlashSize;
		}

		return size;
	}

	void ClearIrq()
	{
		if(!_cpuCycleIrqPending && !(_scanlineIrqPending && _scanlineIrqEnable) && !(_esp->getDataReadyIO() && _espHasReceivedMessage)) {
			_console->GetCpu()->ClearIrqSource(IRQSource::External);
		}
	}

	void Reset(bool softReset) override
	{
		_console->GetMemoryManager()->RegisterWriteHandler(_rainbowMemoryHandler.get(), 0x2000, 0x2007);
		_console->GetMemoryManager()->RegisterReadHandler(_rainbowMemoryHandler.get(), 0x4011, 0x4011);
		_console->GetMemoryManager()->RegisterWriteHandler(_rainbowMemoryHandler.get(), 0x4014, 0x4014);

		// PRG - 32K banks mapped to first PRG-ROM bank
		_prgRomMode = PRG_ROM_MODE_0;
		_prg[3] = 0;

		// CHR - 8K banks mapped to first bank of CHR-ROM
		// extended sprite mode disabled
		_chrChip = CHR_CHIP_ROM;
		_chrSprExtMode = 0;
		_chrMode = CHR_MODE_0;
		_chr[0] = 0;

		// Nametables - CIRAM horizontal mirroring
		_ntBank[0] = 0;
		_ntBank[1] = 0;
		_ntBank[2] = 1;
		_ntBank[3] = 1;
		_ntControl[0] = 0;
		_ntControl[1] = 0;
		_ntControl[2] = 0;
		_ntControl[3] = 0;

		// Scanline IRQ
		_scanlineIrqEnable = false;
		_scanlineIrqPending = false;
		_scanlineIrqPendingLast = false;
		_scanlineIrqInFrame = false;
		_scanlineIrqHblank = false;
		_scanlineIrqOffset = 135;

		// CPU Cycle IRQ
		_cpuCycleIrqEnable = false;
		_cpuCycleIrqPending = false;
		_cpuCycleIrqReset = false;

		// Audio Output
		_audioOutput = 1;

		// ESP / WiFi
		_espEnable = false;
		EspClearMessageReceived();
		_espMessageSent = true;
		_espRxAddress = 0;
		_espTxAddress = 0;
		_espRxIndex = 0;

		//_scanlineIrqEnable = _scanlineIrqPending = false;
		//_cpuCycleIrqEnable = _cpuCycleIrqPending = _cpuCycleIrqReset = false;
		//_espIrqPending = false;
		UpdatePrgBanks();
		UpdateChrBanks();
	}

	void UpdatePrgBanks()
	{

		// $8000-$ffff
		uint8_t t_prgRomMode = _bootrom ? PRG_ROM_MODE_3 : _prgRomMode;
		PrgMemoryType ramMemoryType = HasBattery() ? PrgMemoryType::SaveRam : PrgMemoryType::WorkRam;
		uint16_t ramOffset = (HasBattery() ? _realSaveRamSize : _realWorkRamSize); // +(_fpgaRamBank & 0x01) * 0x1000;
		uint16_t prgOffset;

		// 32K
		if(t_prgRomMode == PRG_ROM_MODE_0) {
			prgOffset = 0x8000;
			if(_prg[3] & 0x8000) {
				// WRAM
				SetCpuMemoryMapping(0x8000, 0xFFFF, ramMemoryType, (_prg[3] & 0x0003) * prgOffset, MemoryAccessType::ReadWrite);
			} else {
				// PRG-ROM
				SetCpuMemoryMapping(0x8000, 0xFFFF, PrgMemoryType::PrgRom, (_prg[3] & 0x7fff) * prgOffset, MemoryAccessType::Read);
			}
		}

		// 2 x 16K
		if(t_prgRomMode == PRG_ROM_MODE_1) {
			prgOffset = 0x4000;
			if(_prg[3] & 0x8000) {
				// WRAM
				SetCpuMemoryMapping(0x8000, 0xBFFF, ramMemoryType, (_prg[3] & 0x0007) * prgOffset, MemoryAccessType::ReadWrite);
			} else {
				// PRG-ROM
				SetCpuMemoryMapping(0x8000, 0xBFFF, PrgMemoryType::PrgRom, (_prg[3] & 0x7FFF) * prgOffset, MemoryAccessType::Read);
			}

			if(_prg[7] & 0x8000)
				// WRAM
				SetCpuMemoryMapping(0xC000, 0xFFFF, ramMemoryType, (_prg[7] & 0x0007) * prgOffset, MemoryAccessType::ReadWrite);
			else
				// PRG-ROM
				SetCpuMemoryMapping(0xC000, 0xFFFF, PrgMemoryType::PrgRom, (_prg[7] & 0x7FFF) * prgOffset, MemoryAccessType::Read);
		}

		// 16K + 8K + 8K
		if(t_prgRomMode == PRG_ROM_MODE_2) {
			prgOffset = 0x4000;
			if(_prg[3] & 0x8000) {
				// WRAM
				SetCpuMemoryMapping(0x8000, 0xBFFF, ramMemoryType, (_prg[3] & 0x0007) * prgOffset, MemoryAccessType::ReadWrite);
			} else {
				// PRG-ROM
				SetCpuMemoryMapping(0x8000, 0xBFFF, PrgMemoryType::PrgRom, (_prg[3] & 0x7FFF) * prgOffset, MemoryAccessType::Read);
			}

			prgOffset = 0x2000;
			if(_prg[7] & 0x8000) {
				// WRAM
				SetCpuMemoryMapping(0xC000, 0xDFFF, ramMemoryType, (_prg[7] & 0x000F) * prgOffset, MemoryAccessType::ReadWrite);
			} else {
				// PRG-ROM
				SetCpuMemoryMapping(0xC000, 0xDFFF, PrgMemoryType::PrgRom, (_prg[7] & 0x7FFF) * prgOffset, MemoryAccessType::Read);
			}

			if(_prg[9] & 0x8000) {
				// WRAM
				SetCpuMemoryMapping(0xE000, 0xFFFF, ramMemoryType, (_prg[9] & 0x000F) * prgOffset, MemoryAccessType::ReadWrite);
			} else {
				// PRG-ROM
				SetCpuMemoryMapping(0xE000, 0xFFFF, PrgMemoryType::PrgRom, (_prg[9] & 0x7FFF) * prgOffset, MemoryAccessType::Read);
			}
		}

		// 4 x 8K
		if(t_prgRomMode == PRG_ROM_MODE_3) {
			prgOffset = 0x2000;
			for(uint8_t i = 0; i < 4; i++) {
				if(i == 3 && _bootrom) {
					// FPGA FLASH
					SetCpuMemoryMapping(0xE000, 0xFFFF, ramMemoryType, ramOffset + FpgaRamSize, MemoryAccessType::Read);
				} else {
					if(_prg[3 + i * 2] & 0x8000) {
						// WRAM
						SetCpuMemoryMapping(0x8000 + 0x2000 * i, 0x8000 + 0x2000 * i + 0x1FFF, ramMemoryType, (_prg[3 + i * 2] & 0x000F) * prgOffset, MemoryAccessType::ReadWrite);
					} else {
						// PRG-ROM
						SetCpuMemoryMapping(0x8000 + 0x2000 * i, 0x8000 + 0x2000 * i + 0x1FFF, PrgMemoryType::PrgRom, (_prg[3 + i * 2] & 0x7FFF) * prgOffset, MemoryAccessType::Read);
					}
				}
			}
		}

		// 8 x 4K
		if(t_prgRomMode == PRG_ROM_MODE_4) {
			prgOffset = 0x1000;
			for(uint8_t i = 0; i < 8; i++) {
				if(_prg[3 + i] & 0x8000) {
					// WRAM
					SetCpuMemoryMapping(0x8000 + 0x1000 * i, 0x8000 + 0x1000 * i + 0x0FFF, ramMemoryType, (_prg[3 + i] & 0x001F) * prgOffset, MemoryAccessType::ReadWrite);
				} else {
					// PRG-ROM
					SetCpuMemoryMapping(0x8000 + 0x1000 * i, 0x8000 + 0x1000 * i + 0x0FFF, PrgMemoryType::PrgRom, (_prg[3 + i] & 0x7FFF) * prgOffset, MemoryAccessType::Read);
				}
			}
		}

		// $6000-$7fff

		// 8K
		if(_prgRamMode == PRG_RAM_MODE_0) {
			prgOffset = 0x2000;
			switch((_prg[1] & 0xC000) >> 14) {
				// PRG-ROM
				case 0:
				case 1:
					SetCpuMemoryMapping(0x6000, 0x7FFF, PrgMemoryType::PrgRom, (_prg[1] & 0x7FFF) * prgOffset, MemoryAccessType::Read);
					break;
					// WRAM
				case 2:
					SetCpuMemoryMapping(0x6000, 0x7FFF, ramMemoryType, (_prg[1] & 0x000F) * prgOffset, MemoryAccessType::ReadWrite);
					break;
					// FPGA-RAM
				case 3:
					SetCpuMemoryMapping(0x6000, 0x7FFF, ramMemoryType, ramOffset + (_prg[1] & 0x000F) * prgOffset, MemoryAccessType::ReadWrite);
					break;
			}
		}

		// 4K + 4K
		if(_prgRamMode == PRG_RAM_MODE_1) {
			prgOffset = 0x1000;
			switch((_prg[1] & 0xC000) >> 14) {
				// PRG-ROM
				case 0:
				case 1:
					SetCpuMemoryMapping(0x6000, 0x6FFF, PrgMemoryType::PrgRom, (_prg[1] & 0x7FFF) * prgOffset, MemoryAccessType::Read);
					break;
					// WRAM
				case 2:
					SetCpuMemoryMapping(0x6000, 0x6FFF, ramMemoryType, (_prg[1] & 0x001F) * prgOffset, MemoryAccessType::ReadWrite);
					break;
					// FPGA-RAM
				case 3:
					SetCpuMemoryMapping(0x6000, 0x6FFF, ramMemoryType, ramOffset + (_prg[1] & 0x001F) * prgOffset, MemoryAccessType::ReadWrite);
					break;
			}

			switch((_prg[2] & 0xC000) >> 14) {
				// PRG-ROM
				case 0:
				case 1:
					SetCpuMemoryMapping(0x7000, 0x7FFF, PrgMemoryType::PrgRom, (_prg[2] & 0x7FFF) * prgOffset, MemoryAccessType::Read);
					break;
					// WRAM
				case 2:
					SetCpuMemoryMapping(0x7000, 0x7FFF, ramMemoryType, (_prg[2] & 0x001F) * prgOffset, MemoryAccessType::ReadWrite);
					break;
					// FPGA-RAM
				case 3:
					SetCpuMemoryMapping(0x7000, 0x7FFF, ramMemoryType, ramOffset + (_prg[2] & 0x001F) * prgOffset, MemoryAccessType::ReadWrite);
					break;
			}
		}

		// $5000-$5fff - 4K FPGA-RAM
		if(_bootrom)
			// FPGA FLASH
			SetCpuMemoryMapping(0x5000, 0x5FFF, ramMemoryType, ramOffset + FpgaRamSize + 0x2000, MemoryAccessType::Read);
		else
			// FPGA RAM
			SetCpuMemoryMapping(0x5000, 0x5FFF, ramMemoryType, ramOffset + (_prg[0] & 0x0001) * 0x1000, MemoryAccessType::ReadWrite);

		// $4800-$4fff
		SetCpuMemoryMapping(0x4800, 0x4FFF, ramMemoryType, ramOffset + 0x1800, MemoryAccessType::ReadWrite);

	}

	void UpdateChrBanks()
	{
		uint16_t ramOffset = (HasBattery() ? _realSaveRamSize : _realWorkRamSize); // +(_fpgaRamBank & 0x01) * 0x1000;
		uint16_t chrOffset;
		ChrMemoryType chrMemoryType = (ChrMemoryType)(_chrChip + 1);
		MemoryAccessType chrMemoryAccessType = chrMemoryType == ChrMemoryType::ChrRom ? MemoryAccessType::Read : MemoryAccessType::ReadWrite;

		// CHR data
		switch(_chrMode) {

			case CHR_MODE_0:	// 8K
				chrOffset = 0x2000;
				SetPpuMemoryMapping(0x0000, 0x1FFF, chrMemoryType, (_chr[0] & 0x03FF) * chrOffset, chrMemoryAccessType);
				break;

			case CHR_MODE_1:	// 4K
				chrOffset = 0x1000;
				SetPpuMemoryMapping(0x0000, 0x0FFF, chrMemoryType, (_chr[0] & 0x07FF) * chrOffset, chrMemoryAccessType);
				SetPpuMemoryMapping(0x1000, 0x1FFF, chrMemoryType, (_chr[1] & 0x07FF) * chrOffset, chrMemoryAccessType);
				break;

			case CHR_MODE_2:	// 2K
				chrOffset = 0x0800;
				for(uint8_t i = 0; i < 4; i++) {
					SetPpuMemoryMapping(0x800 * i, 0x800 * i + 0x7FF, chrMemoryType, (_chr[i] & 0x0FFF) * chrOffset, chrMemoryAccessType);
				}
				break;

			case CHR_MODE_3:	// 1K
				chrOffset = 0x0400;
				for(uint8_t i = 0; i < 8; i++) {
					SetPpuMemoryMapping(0x400 * i, 0x400 * i + 0x3FF, chrMemoryType, (_chr[i] & 0x1FFF) * chrOffset, chrMemoryAccessType);
				}
				break;

			case CHR_MODE_4:	// 512B
				chrOffset = 0x0200;
				for(uint8_t i = 0; i < 16; i++) {
					SetPpuMemoryMapping(0x200 * i, 0x200 * i + 0x1FF, chrMemoryType, (_chr[i] & 0x3FFF) * chrOffset, chrMemoryAccessType);
				}
				break;
		}

		// Nametables data
		for(int i = 0; i < 4; i++) {
			uint8_t curNtBank = _ntBank[i];
			uint8_t curNtChip = _ntControl[i] >> 6;
			//uint8_t curNt1kExtDest = (_ntControl[i] & 0x0C) >> 2;
			//uint8_t curNtExtMode = _ntControl[i] & 0x03;
			uint16_t startAddr = 0x2000 + i * 0x400;
			uint16_t endAddr = startAddr + 0x3FF;

			switch(curNtChip) {
				case NT_CIRAM:
					SetNametable(i, curNtBank & 0x01);
					break;
				case NT_CHR_RAM:
					SetPpuMemoryMapping(startAddr, endAddr, ChrMemoryType::ChrRam, (curNtBank & 0x7F) * 0x400, MemoryAccessType::ReadWrite);
					break;
				case NT_FPGA_RAM:
					SetPpuMemoryMapping(startAddr, endAddr, _fpgaRam, ((curNtBank & 0x03) * 0x400), RNBW::FpgaRamSize, MemoryAccessType::ReadWrite);
					break;
				case NT_CHR_ROM:
					SetPpuMemoryMapping(startAddr, endAddr, ChrMemoryType::ChrRom, (curNtBank & 0x7F) * 0x400, MemoryAccessType::Read);
					break;
			}
		}
	}

	void ProcessCpuClock() override
	{
		// Expansion audio
		_audio->Clock();

		// CPU cycle IRQ
		if(_cpuCycleIrqEnable) {
			_cpuCycleIrqCount--;
			if(_cpuCycleIrqCount == 0) {
				_cpuCycleIrqCount = _cpuCycleIrqLatch;
				_cpuCycleIrqPending = true;
				TriggerIrq();
			}
		}

		// Sprite Extended Mode - sprite ordering
		switch(_spriteOrderingStep) {
			case 0:
				if(_spriteOrdering) {
					_spriteOrderingStep++;
					_spriteIndex = 0;
					_spriteCounter = 0;
				}
				break;
			case 1:
				//_console->GetNesConfig().RemoveSpriteLimit;
				bool largeSprites = (_rainbowMemoryHandler->GetPpuReg(0x2000) & 0x20) != 0;
				uint8_t spriteCurrentYPos = _rainbowMemoryHandler->GetSpriteYPos(_spriteIndex);
				uint8_t spriteMaxYPos = _rainbowMemoryHandler->GetSpriteYPos(_spriteIndex) + (largeSprites ? 16 : 8);
				if(_spriteIndex < 8 && _spriteCounter < 64) {
					if(_scanlineCounter >= spriteCurrentYPos && _scanlineCounter <= spriteMaxYPos) {
						_spriteYOrder[_spriteIndex] = _spriteBank[_spriteCounter];
						_spriteIndex++;
					}
					_spriteCounter++;
				} else {
					_spriteOrdering = false;
					_spriteOrderingStep = 0;
				}
				break;
		}

		// PPU update
		// if(!_scanlineIrqPendingLast & _scanlineIrqPending) {
		// 	_scanlineIrqPendingLast = _scanlineIrqPending;
		// 	_scanlineIrqJitterCounter = 0;
		// } else {
		_scanlineIrqJitterCounter++;
		// }

		if(_ppuIdleCounter) {
			_ppuIdleCounter--;
			if(_ppuIdleCounter == 0) {
				//"The "in-frame" flag is cleared when the PPU is no longer rendering. This is detected when 3 CPU cycles pass without a PPU read having occurred (PPU /RD has not been low during the last 3 M2 rises)."
				_ppuInFrame = false;
				//UpdateChrBanks(true);
				UpdateChrBanks();
			}
		}
	}

	void InitMapper() override
	{
		// PRG self-flashing
		_prgFlash.reset(new Flash3_3V(_prgRom, _prgSize));
		_orgPrgRom = vector<uint8_t>(_prgRom, _prgRom + _prgSize);
		AddRegisterRange(0x8000, 0xFFFF, MemoryOperation::Any);

		// CHR self-flashing
		_chrFlash.reset(new Flash3_3V(_chrRom, _chrRomSize));
		_orgChrRom = vector<uint8_t>(_chrRom, _chrRom + _chrRomSize);

		// Expansion audio
		_audio.reset(new RNBWAudio(_console));
		_audio->Reset();

		//Override the 2000-2007 registers to catch all writes to the PPU registers (but not their mirrors)
		_rainbowMemoryHandler.reset(new RainbowMemoryHandler(_console, _audio.get()));

		// CPU cycle IRQ

		// init FPGA RAM pointer
		_fpgaRam = HasBattery() ? _saveRam + _realSaveRamSize : _workRam + _realWorkRamSize;
		_fpgaFlash = HasBattery() ? _saveRam + _realSaveRamSize + FpgaRamSize : _workRam + _realWorkRamSize + FpgaRamSize;

		// init FPGA RAM with bootrom CHR data
		for(size_t i = 0; i < 0x1000; i++) {
			_fpgaRam[i] = bootrom_chr[i];
		}

		_esp = new BrokeStudioFirmware;
		EspClearMessageReceived();

		ApplySaveData();
	}

	void InitMapper(RomData& romData) override
	{
		// init FPGA RAM with MISC ROM data if needed
		if(_romInfo.Header.GetMiscRomsNumber() != 0) {
			// FPGA Flash
			for(size_t i = 0; i < 0x2000; i++) {
				_fpgaFlash[i] = romData.RawData.at(romData.RawData.size() - 0x2000 + i);
			}
			for(size_t i = 0; i < 0x1000; i++) {
				_fpgaFlash[0x2000 + i] = romData.RawData.at(romData.RawData.size() - 0x3000 + i);
			}
			// FPGA Ram
			for(size_t i = 0; i < 0xE00; i++) {
				_fpgaRam[0x1000 + i] = romData.RawData.at(romData.RawData.size() - 0x4000 + i);
			}
			_bootrom = true;
			//UpdatePrgBanks();

			// _miscRomSize = romData.RawData.at(romData.RawData.size();
			// if(_romInfo.Header.HasTrainer()) {
			// 	_miscRomSize = _miscRomSize - 512;
			// }
			// _miscRomSize = _miscRomSize - 16;
			// _miscRomSize = _miscRomSize - _romInfo.Header.GetPrgSize();
			// _miscRomSize = _miscRomSize - _romInfo.Header.GetChrSize();
			// MessageManager::Log("[Rainbow] Unsupported bootrom size value.");
		}
	}

	void Serialize(Serializer& s) override
	{
		BaseMapper::Serialize(s);

		// PRG-ROM/RAM/FPGA
		SV(_prgRomMode); SV(_prgRamMode);
		SVArray(_prg, 11);
		SV(_realWorkRamSize); SV(_realSaveRamSize);
		// _fpgaRam ??
		// _fpgaFlash ??
		// _prgFlash ??
		// _orgPrgRom ??
		SV(_bootrom);

		// CHR-ROM/RAM
		SV(_chrChip); SV(_chrSprExtMode); SV(_chrMode);
		SVArray(_chr, 16);
		SVArray(_ntBank, 5); SVArray(_ntControl, 5);
		// _chrFlash ??
		// _orgChrRom ??

		// Sprite Extended Mode
		SV(_oamOffset);
		SV(_spriteBankOffset);
		SVArray(_spriteBank, 64);
		SVArray(_spriteYOrder, 64);
		SV(_spriteIndex);
		SV(_spriteCounter);
		SV(_spriteOrderingStep);
		SV(_spriteOrdering);

		// Extended modes
		SV(_bgBankOffset);
		SV(_extModeLastNametableFetch);
		SV(_extModeLastFetchCounter);
		SV(_extModeLastNtIdx);
		SV(_extModeLast1kDest);
		SV(_extModeLastMode);
		SV(_extModeLastValue);
		SV(_windowExtModeLast1kDest);
		SV(_windowExtModeLastMode);
		SV(_windowExtModeLastValue);

		// Window mode
		SV(_windowModeEnabled);
		SV(_windowInSplitRegion);
		SV(_windowXStartTile);
		SV(_windowXEndTile);
		SV(_windowYStart);
		SV(_windowYEnd);
		SV(_windowXScroll);
		SV(_windowYScroll);
		SV(_splitTile);
		SV(_splitTileNumber);
		SV(_windowSplitXPos);
		SV(_windowSplitYPos);

		// ESP/WIFI
		//BrokeStudioFirmware* _esp = NULL; ??
		SV(_espEnable);
		SV(_espIrqEnable);
		SV(_espHasReceivedMessage);
		SV(_espMessageSent);
		SV(_espRxAddress); SV(_espTxAddress); SV(_espRxIndex);

		// MISC
		SV(_audioOutput);
		SV(_miscRomSize);

		// SCANLINE IRQ
		SV(_scanlineIrqEnable);
		SV(_scanlineIrqPendingLast);
		SV(_scanlineIrqPending);
		SV(_scanlineIrqInFrame);
		SV(_scanlineIrqHblank);
		SV(_scanlineIrqCounter);
		SV(_scanlineIrqLatch);
		SV(_scanlineIrqOffset);
		SV(_scanlineIrqJitterCounter);
		SV(_scanlineCounter);
		SV(_dotCounter);

		SV(_needInFrame);
		SV(_ppuInFrame);
		SV(_ppuIdleCounter);
		SV(_lastPpuReadAddr);
		SV(_ntReadCounter);

		// CPU CYCLE IRQ
		SV(_cpuCycleIrqEnable); SV(_cpuCycleIrqReset); SV(_cpuCycleIrqPending);
		SV(_cpuCycleIrqLatch); SV(_cpuCycleIrqCount);

		if(!s.IsSaving()) {
			UpdatePrgBanks();
			UpdateChrBanks();
		}
	}

	void ApplySaveData()
	{
		//Apply save data (saved as an IPS file), if found
		vector<uint8_t> ipsData = _emu->GetBatteryManager()->LoadBattery(".ips");
		if(!ipsData.empty()) {
			vector<uint8_t> patchedRom;
			vector<uint8_t> orgRom;
			vector<uint8_t> prgRom = vector<uint8_t>(_prgRom, _prgRom + _prgSize);
			vector<uint8_t> chrRom = vector<uint8_t>(_chrRom, _chrRom + _chrRomSize);
			orgRom.insert(orgRom.end(), prgRom.begin(), prgRom.end());
			orgRom.insert(orgRom.end(), chrRom.begin(), chrRom.end());
			if(IpsPatcher::PatchBuffer(ipsData, orgRom, patchedRom)) {
				memcpy(_prgRom, patchedRom.data(), _prgSize);
				memcpy(_chrRom, patchedRom.data() + _prgSize, _chrRomSize);
			}
		}
	}

	void SaveBattery() override
	{
		if(HasBattery()) {

			vector<uint8_t> orgRoms;
			orgRoms.insert(orgRoms.end(), _orgPrgRom.begin(), _orgPrgRom.end());
			orgRoms.insert(orgRoms.end(), _orgChrRom.begin(), _orgChrRom.end());

			vector<uint8_t> roms;
			vector<uint8_t> prgRom = vector<uint8_t>(_prgRom, _prgRom + _prgSize);
			vector<uint8_t> chrRom = vector<uint8_t>(_chrRom, _chrRom + _chrRomSize);
			roms.insert(roms.end(), prgRom.begin(), prgRom.end());
			roms.insert(roms.end(), chrRom.begin(), chrRom.end());

			vector<uint8_t> ipsData = IpsPatcher::CreatePatch(orgRoms, roms);
			if(ipsData.size() > 8) {
				_emu->GetBatteryManager()->SaveBattery(".ips", ipsData.data(), (uint32_t)ipsData.size());
			}
		}
	}

	uint8_t ReadRegister(uint16_t addr) override
	{
		switch(addr) {
			case 0x4100: return (_prgRamMode << 6) | _prgRomMode;
			case 0x4120: return (_chrChip << 6) | (_chrSprExtMode << 5) | (_windowModeEnabled << 4) | _chrMode;
			case 0x4151:
			{
				uint8_t rv = (_scanlineIrqHblank ? 0x80 : 0) | (_scanlineIrqInFrame ? 0x40 : 0 | (_scanlineIrqPending ? 0x01 : 0));
				_scanlineIrqPending = false;
				_scanlineIrqPendingLast = false;
				ClearIrq();
				return rv;
			}
			case 0x4154: return _scanlineIrqJitterCounter;
			case 0x4160: return MAPPER_VERSION;
			case 0x4161: return (_scanlineIrqPending ? 0x80 : 0) | (_cpuCycleIrqPending ? 0x40 : 0) | (_espIrqPending ? 0x01 : 0);
				// ESP - WiFi
			case 0x4190:
			{
				uint8_t espEnableFlag = _espEnable ? 0x01 : 0x00;
				uint8_t espIrqEnableFlag = _espIrqEnable ? 0x02 : 0x00;
				//UDBG("RAINBOW read flags %04x => %02xs\n", A, espEnableFlag | espIrqEnableFlag);
				MessageManager::Log("[Rainbow] read flags " + HexUtilities::ToHex(addr) + " => " + HexUtilities::ToHex(espEnableFlag | espIrqEnableFlag));
				return espEnableFlag | espIrqEnableFlag;
			}
			case 0x4191:
			{
				uint8_t espMessageReceivedFlag = EspMessageReceived() ? 0x80 : 0;
				uint8_t espRtsFlag = _esp->getDataReadyIO() ? 0x40 : 0x00;
				return espMessageReceivedFlag | espRtsFlag;
			}
			case 0x4192: return _espMessageSent ? 0x80 : 0;
			case 0x4195:
			{
				uint8_t retval = _fpgaRam[0x1800 + (_espRxAddress << 8) + _espRxIndex];
				_espRxIndex++;
				return retval;
			}
			case 0xFFFA:
			case 0xFFFB:
				_ppuInFrame = false;
				_lastPpuReadAddr = 0;
				_scanlineIrqCounter = 0;
				_scanlineIrqPending = false;
				ClearIrq();
				return DebugReadRam(addr);
		}
		int16_t value = _prgFlash->Read(addr);
		if(value >= 0) {
			return (uint8_t)value;
		}

		return InternalReadRam(addr);
	}

	void WriteRegister(uint16_t addr, uint8_t value) override
	{
		int bank;
		uint32_t flashAddr = addr;
		uint8_t prgIdx;
		uint8_t t_prgRomMode = _bootrom ? PRG_ROM_MODE_3 : _prgRomMode;

		if(addr >= 0x4100 && addr <= 0x47FF) {	// MAPPER REGISTERS
			switch(addr) {
				// Mapper configuration
				case 0x4100:
					_prgRomMode = value & 0x07;
					_prgRamMode = (value & 0x80) >> 7;
					UpdatePrgBanks();
					break;

					// PRG banking
				case 0x4106:
				case 0x4107:
				case 0x4108:
				case 0x4109:
				case 0x410A:
				case 0x410B:
				case 0x410C:
				case 0x410D:
				case 0x410E:
				case 0x410F:
					bank = (addr & 0x0f) - 5;
					_prg[bank] = (_prg[bank] & 0x00ff) | (value << 8);
					UpdatePrgBanks();
					break;
				case 0x4115: _prg[0] = value & 0x01; UpdatePrgBanks(); break;
				case 0x4116:
				case 0x4117:
				case 0x4118:
				case 0x4119:
				case 0x411A:
				case 0x411B:
				case 0x411C:
				case 0x411D:
				case 0x411E:
				case 0x411F:
					bank = (addr & 0x0f) - 5;
					_prg[bank] = (_prg[bank] & 0xff00) | value;
					UpdatePrgBanks();
					break;

					// CHR banking / chip selector / vertical split-screen / sprite extended mode
				case 0x4120:
					_chrChip = (value & 0xC0) >> 6;
					_chrSprExtMode = (value & 0x20) >> 5;
					_windowModeEnabled = (value & 0x10) >> 4;
					_chrMode = value & 0x07;
					UpdateChrBanks();
					break;
				case 0x4121:
					_bgBankOffset = value & 0x1f;
					break;

					// Nametables bank
				case 0x4126: _ntBank[0] = value; UpdateChrBanks(); break;
				case 0x4127: _ntBank[1] = value; UpdateChrBanks(); break;
				case 0x4128: _ntBank[2] = value; UpdateChrBanks(); break;
				case 0x4129: _ntBank[3] = value; UpdateChrBanks(); break;
				case 0x412E: _ntBank[4] = value; UpdateChrBanks(); break;

					// Nametables control
				case 0x412A: _ntControl[0] = value; UpdateChrBanks(); break;
				case 0x412B: _ntControl[1] = value; UpdateChrBanks(); break;
				case 0x412C: _ntControl[2] = value; UpdateChrBanks(); break;
				case 0x412D: _ntControl[3] = value; UpdateChrBanks(); break;
				case 0x412F: _ntControl[4] = value; UpdateChrBanks(); break;

					// CHR banking
				case 0x4130:
				case 0x4131:
				case 0x4132:
				case 0x4133:
				case 0x4134:
				case 0x4135:
				case 0x4136:
				case 0x4137:
				case 0x4138:
				case 0x4139:
				case 0x413A:
				case 0x413B:
				case 0x413C:
				case 0x413D:
				case 0x413E:
				case 0x413F:
					bank = addr & 0x0f;
					_chr[bank] = (_chr[bank] & 0x00ff) | (value << 8);
					UpdateChrBanks();
					break;
				case 0x4140:
				case 0x4141:
				case 0x4142:
				case 0x4143:
				case 0x4144:
				case 0x4145:
				case 0x4146:
				case 0x4147:
				case 0x4148:
				case 0x4149:
				case 0x414A:
				case 0x414B:
				case 0x414C:
				case 0x414D:
				case 0x414E:
				case 0x414F:
					bank = addr & 0x0f;
					_chr[bank] = (_chr[bank] & 0xff00) | value;
					UpdateChrBanks();
					break;

					// Scanline IRQ
				case 0x4150: _scanlineIrqLatch = value; break;
				case 0x4151: _scanlineIrqEnable = true; break;
				case 0x4152:
					_scanlineIrqPending = false;
					_scanlineIrqPendingLast = false;
					_scanlineIrqEnable = false;
					ClearIrq();
					break;
				case 0x4153: _scanlineIrqOffset = value > 169 ? 169 : value; break;

					// CPU Cycle IRQ
				case 0x4158:
					_cpuCycleIrqLatch &= 0xFF00;
					_cpuCycleIrqLatch |= value;
					break;
				case 0x4159:
					_cpuCycleIrqLatch &= 0x00FF;
					_cpuCycleIrqLatch |= value << 8;
					break;
				case 0x415A:
					_cpuCycleIrqEnable = value & 0x01;
					_cpuCycleIrqReset = (value & 0x02) >> 1;
					if(_cpuCycleIrqEnable)
						_cpuCycleIrqCount = _cpuCycleIrqLatch;
					break;
				case 0x415B:
					_cpuCycleIrqPending = false;
					_cpuCycleIrqEnable = _cpuCycleIrqReset;
					ClearIrq();
					break;

					// Window Mode
				case 0x4170: _windowXStartTile = value & 0x1f; break;
				case 0x4171: _windowXEndTile = value & 0x1f; break;
				case 0x4172: _windowYStart = value; break;
				case 0x4173: _windowYEnd = value; break;
				case 0x4174: _windowXScroll = value & 0x1f; break;
				case 0x4175: _windowYScroll = value; break;

					// ESP - WiFi
				case 0x4190:
					_espEnable = value & 0x01;
					_espIrqEnable = value & 0x02;
					break;
				case 0x4191:
					if(_espEnable) EspClearMessageReceived();
					//else FCEU_printf("RAINBOW warning: $4190.0 is not set\n");
					else MessageManager::Log("[Rainbow] warning: $4190.0 is not set.");
					break;
				case 0x4192:
					if(_espEnable) {
						_espMessageSent = false;
						uint8_t message_length = _fpgaRam[0x1800 + (_espTxAddress << 8)];
						_esp->rx(message_length);
						for(uint8_t i = 0; i < message_length; i++) {
							_esp->rx(_fpgaRam[0x1800 + (_espTxAddress << 8) + 1 + i]);
						}
						_espMessageSent = true;
					}
					//else FCEU_printf("RAINBOW warning: $4190.0 is not set\n");
					else MessageManager::Log("[Rainbow] warning: $4190.0 is not set.");
					break;
				case 0x4193:
					_espRxAddress = value & 0x07;
					break;
				case 0x4194:
					_espTxAddress = value & 0x07;
					break;
				case 0x4195:
					_espRxIndex = value;
					break;

					// Audio Expansion
				case 0x41A0: case 0x41A1: case 0x41A2:
				case 0x41A3: case 0x41A4: case 0x41A5:
				case 0x41A6: case 0x41A7: case 0x41A8:
				case 0x41A9:
					_audio->WriteRegister(addr, value);
					break;

					// Bootrom config
				case 0x41FF: _bootrom = value & 0x01; UpdatePrgBanks(); break;

					// Sprite Extended Mode
				case 0x4240:
					_spriteBankOffset = value & 0x07;
					break;
			}

			// $4200-$423F Sprite 4K bank lower bits
			if((addr >= 0x4200) && (addr <= 0x423f))
				_spriteBank[addr & 0x3f] = value;

			return;

		} else if(addr >= 0x4800 && addr <= 0x5FFF) {	// FPGA-RAM Writes
			return WritePrgRam(addr, value); // FPGA_RAM
		} else if(addr >= 0x6000 && addr <= 0x7FFF) {	// PRG-RAM Writes, could be PRG-ROM, PRG-RAM, FPGA-RAM, we need to check
			switch(_prgRamMode) {
				case PRG_RAM_MODE_0:
					// 8K
					if(((_prg[1] & 0x8000) >> 14) != 0)
						return WritePrgRam(addr, value); // FPGA_RAM or WRAM

					// PRG-ROM
					flashAddr &= 0x1FFF;
					flashAddr |= (_prg[1] & 0x7FFF) << 13;

					break;
				case PRG_RAM_MODE_1:
					// 2 x 4K
					prgIdx = ((addr >> 12) & 0x07) - 6;
					if(((_prg[prgIdx] & 0x8000) >> 14) != 0)
						return WritePrgRam(addr, value); // FPGA_RAM or WRAM

					// PRG-ROM
					flashAddr &= 0x1FFF;
					flashAddr |= (_prg[prgIdx] & 0x7FFF) << 13;

					break;
			}
		} else if(addr >= 0x8000 && addr <= 0xFFFF) {	// PRG-ROM Writes, could be PRG-ROM, PRG-RAM, FPGA-RAM, we need to check
			switch(t_prgRomMode) {
				case PRG_ROM_MODE_0:
					// 32K
					if((_prg[3] >> 14) != 0)
						return WritePrgRam(addr, value); // WRAM

					// PRG-ROM
					flashAddr &= 0x7FFF;
					flashAddr |= (_prg[3] & 0x7FFF) << 15;

					break;
				case PRG_ROM_MODE_1:
					// 2 x 16K
					prgIdx = ((addr >> 12) & 0x04) + 3;
					if((_prg[prgIdx] >> 14) != 0)
						return WritePrgRam(addr, value); // WRAM

					// PRG-ROM
					flashAddr &= 0x3FFF;
					flashAddr |= (_prg[prgIdx] & 0x7FFF) << 14;

					break;
				case PRG_ROM_MODE_2:
					if(addr >= 0x8000 && addr <= 0xBFFF) {
						// 16K
						if((_prg[3] >> 14) != 0)
							return WritePrgRam(addr, value); // WRAM

						// PRG-ROM
						flashAddr &= 0x3FFF;
						flashAddr |= (_prg[3] & 0x3F) << 14;
					} else if(addr >= 0xC000 && addr <= 0xFFFF) {
						// 2 x 8K
						prgIdx = ((addr >> 12) & 0x06) + 3;
						if((_prg[prgIdx] >> 14) != 0)
							return WritePrgRam(addr, value); // WRAM

						// PRG-ROM
						flashAddr &= 0x1FFF;
						flashAddr |= (_prg[prgIdx] & 0x7FFF) << 13;

					}
					break;
				case PRG_ROM_MODE_3:
					// 4 x 8K
					prgIdx = ((addr >> 12) & 0x06) + 3;
					if((_prg[prgIdx] >> 14) != 0)
						return WritePrgRam(addr, value); // WRAM

					// PRG-ROM
					flashAddr &= 0x1FFF;
					flashAddr |= (_prg[prgIdx] & 0x7FFF) << 13;

					break;
				case PRG_ROM_MODE_4:
					// 8 x 4K
					prgIdx = ((addr >> 12) & 0x07) + 3;

					if((_prg[prgIdx] >> 14) != 0)
						return WritePrgRam(addr, value); // WRAM

					// PRG-ROM
					flashAddr &= 0x0FFF;
					flashAddr |= (_prg[prgIdx] & 0x7FFF) << 13;
					break;

				default:
					return WritePrgRam(addr, value);
			}
		}
		_prgFlash->Write(flashAddr, value);
	}

	void TriggerIrq()
	{
		_console->GetCpu()->SetIrqSource(IRQSource::External);
	}

	void DetectScanlineStart(uint16_t addr)
	{
		bool newLine = false;
		if(_ntReadCounter >= 2) {
			//After 3 identical NT reads, trigger IRQ when the following attribute byte is read
			if(!_ppuInFrame && !_needInFrame) {
				_needInFrame = true;
				_scanlineCounter = 0;
				_dotCounter = 0;
				_windowSplitYPos = _windowYScroll;
			} else {
				_scanlineCounter++;
				_spriteOrdering = true;
				_spriteOrderingStep = 0;
				_dotCounter = 0;
				newLine = true;
			}
			_splitTileNumber = 0;
			_ntReadCounter = 0;
		} else if(addr >= 0x2000 && addr <= 0x2FFF) {
			if(_lastPpuReadAddr == addr) {
				//Count consecutive identical reads
				_ntReadCounter++;
			} else {
				_ntReadCounter = 0;
			}
		} else {
			_ntReadCounter = 0;
		}

		_dotCounter++;

		if((_dotCounter >= 130) && (_dotCounter < 160))
			_windowSplitXPos = 0;
		else {
			if(newLine) _windowSplitXPos = (_windowSplitXPos - 1) & 0x7f;
			else _windowSplitXPos = (_windowSplitXPos + 1) & 0x7f;
		}

		if(_dotCounter == 128) {
			if(_windowSplitYPos == 239) _windowSplitYPos = 0;
			else _windowSplitYPos++;
		}
	}

	uint8_t MapperReadVram(uint16_t addr, MemoryOperationType memoryOperationType) override
	{
		bool isNtFetch = addr >= 0x2000 && addr <= 0x2FFF && (addr & 0x3FF) < 0x3C0;
		//bool isAtFetch = addr >= 0x2000 && addr <= 0x2FFF && (addr & 0x3FF) >= 0x3C0;
		if(isNtFetch) {
			//Nametable data, not an attribute fetch
			_windowInSplitRegion = false;
			_splitTileNumber++;

			if(_ppuInFrame) {
				//UpdateChrBanks(false);
				UpdateChrBanks();
			} else if(_needInFrame) {
				_needInFrame = false;
				_ppuInFrame = true;
				//UpdateChrBanks(false);
				UpdateChrBanks();
			}
		}

		// Scanline IRQ + HBlank
		DetectScanlineStart(addr);

		_ppuIdleCounter = 3;
		_lastPpuReadAddr = addr;
		_scanlineIrqHblank = _dotCounter > 127;

		if(_scanlineCounter == _scanlineIrqLatch && !_scanlineIrqPending) {
			if(_dotCounter == _scanlineIrqOffset || (_scanlineIrqOffset == 0 && _dotCounter == 1)) {
				_scanlineIrqPending = true;
				_scanlineIrqJitterCounter = 0;
				if(_scanlineIrqEnable) {
					TriggerIrq();
				}
			}
		}

		uint32_t ppuCycle = _console->GetPpu()->GetCurrentCycle();

		// Sprite Extended Mode
		if((_dotCounter >= 130) && (_dotCounter < 160) && _chrSprExtMode) {
			bool largeSprites = (_rainbowMemoryHandler->GetPpuReg(0x2000) & 0x20) != 0;
			uint8_t ppuSpriteFetch = (_dotCounter - 130) >> 2;
			uint8_t ppuSpriteIndex = _spriteYOrder[ppuSpriteFetch];
			if(largeSprites)
				return ReadFromChr(((_spriteBankOffset & 0x03) << 21) + (_spriteBank[ppuSpriteIndex] << 13) + (addr & 0x1FFF));
			else
				return ReadFromChr((_spriteBankOffset << 20) + (_spriteBank[ppuSpriteIndex] << 12) + (addr & 0xFFF));
		}

		// Window Mode
		if(_ppuInFrame && _windowModeEnabled) {
			_windowExtModeLast1kDest = (_ntControl[4] & 0x0C) >> 2;
			_windowExtModeLastMode = _ntControl[4] & 0x03;
			if(addr >= 0x2000) {
				if(isNtFetch) {
					uint8_t tileX = (_windowSplitXPos >> 2) & 0x1f;
					uint8_t tileY = (_windowSplitYPos >> 3) & 0x1f;
					bool inWindowX = ((_windowXStartTile < _windowXEndTile) ?
						(tileX >= _windowXStartTile) & (tileX <= _windowXEndTile) :
						(tileX <= _windowXEndTile) | (tileX >= _windowXStartTile));

					bool inWindowY = ((_windowYStart < _windowYEnd) ?
						(_scanlineCounter >= _windowYStart) & (_scanlineCounter <= _windowYEnd) :
						(_scanlineCounter <= _windowYEnd) | (_scanlineCounter >= _windowYStart));

					bool inWindow = inWindowX && inWindowY;
					if(tileX <= 32 && inWindow) {
						//Split region (for next 3 fetches, attribute + 2x tile data)
						_windowInSplitRegion = true;
						_splitTile = (tileY << 5) | ((tileX - _windowXScroll) & 0x1f);
						_windowExtModeLastValue = _fpgaRam[_windowExtModeLast1kDest * 0x400 + _splitTile];
						uint16_t tileAddr = _ntBank[4] * 0x400 + _splitTile;
						return _fpgaRam[tileAddr];
					} else {
						//Outside of split region (or sprite data), result can get modified by ex ram mode code below
						_windowInSplitRegion = false;
					}
				} else if(_windowInSplitRegion) {
					//Attributes
					switch(_windowExtModeLastMode) {
						case 0:	// extended mode disabled
						case 2:	// extended tiles
						{
							uint16_t attrAddr = (0x03C0 + (_ntBank[4] * 0x400)) | ((_splitTile & 0x380) >> 4) | ((_splitTile & 0x1F) >> 2);
							return _fpgaRam[attrAddr];
						}
						case 1:	// extended attributes
						case 3:	// extended attributes + tiles
						{
							uint8_t palette = (_windowExtModeLastValue & 0xC0) >> 6;
							return palette | palette << 2 | palette << 4 | palette << 6;
						}
					}

				}
			} else if(_windowInSplitRegion) {
				//CHR tile fetches for split region
				switch(_windowExtModeLastMode) {
					case 0:	// extended mode disabled
					case 1:	// extended attributes
						return ReadFromChr(((addr & ~0x07) | (_windowSplitYPos & 0x07)) & 0xFFF);
					case 2:	// extended tiles
					case 3:	// extended attributes + tiles
						return ReadFromChr(((_windowExtModeLastValue & 0x3F) << 12) + (((addr & ~0x07) | (_windowSplitYPos & 0x07)) & 0xFFF));
				}
			}
		}

		//When fetching NT data, we set a flag and then alter the VRAM values read by the PPU on the following 3 cycles (palette, tile low/high byte)
		if(isNtFetch) {
			//Nametable fetches
			_extModeLastNametableFetch = addr & 0x03FF;
			_extModeLastFetchCounter = 3;
			_extModeLastNtIdx = (addr >> 10) & 0x03;
			_extModeLast1kDest = (_ntControl[_extModeLastNtIdx] & 0x0C) >> 2;
			_extModeLastMode = _ntControl[_extModeLastNtIdx] & 0x03;
			_extModeLastValue = _fpgaRam[_extModeLast1kDest * 0x400 + (addr & 0x3ff)];
			switch(_extModeLastMode) {
				case 0:	// extended mode disabled
				case 1:	// extended attributes
					return InternalReadVram(addr);
				case 2:	// extended tiles
				case 3:	// extended attributes + tiles
				{
					return InternalReadVram(addr);
				}
			}
		} else if(_ppuInFrame && _extModeLastFetchCounter > 0) {
			//Attribute fetches or PPU tile data fetches
			_extModeLastFetchCounter--;
			switch(_extModeLastFetchCounter) {
				case 2:
				{
					//PPU palette fetch
					switch(_extModeLastMode) {
						case 0:	// extended mode disabled
						case 2:	// extended tiles
							return InternalReadVram(addr);
						case 1:	// extended attributes
						case 3:	// extended attributes + tiles
						{
							uint8_t palette = (_extModeLastValue & 0xC0) >> 6;
							return palette | palette << 2 | palette << 4 | palette << 6;
						}
					}
				}

				case 1:
				case 0:
					//PPU tile data fetch (high byte & low byte)
					switch(_extModeLastMode) {
						case 0:	// extended mode disabled
						case 1:	// extended attributes
							return InternalReadVram(addr);
						case 2:	// extended tiles
						case 3:	// extended attributes + tiles
							return ReadFromChr(((_extModeLastValue & 0x3f) << 12) + (addr & 0xFFF));
					}
			}
		}

		if((_chrChip == CHR_CHIP_ROM) && (_chrRomSize != 0) && (addr < 0x2000)) {
			uint32_t flashAddr = GetChrFlashAddress(addr);
			flashAddr &= (_chrRomSize - 1);
			int16_t value = _chrFlash->Read(flashAddr);
			if(value >= 0) {
				return (uint8_t)value;
			}
		}

		return InternalReadVram(addr);
	}

	void MapperWriteVram(uint16_t addr, uint8_t value)
	{
		if((_chrChip == CHR_CHIP_ROM) && (_chrRomSize != 0) && (addr < 0x2000)) {
			uint32_t flashAddr = GetChrFlashAddress(addr);
			flashAddr &= (_chrRomSize - 1);
			_chrFlash->Write(flashAddr, value);
		}
	}

	uint32_t GetChrFlashAddress(uint16_t addr)
	{
		uint32_t flashAddr = addr;
		switch(_chrMode) {
			case CHR_MODE_0:
				flashAddr &= 0x1FFF;
				flashAddr |= (_chr[addr >> 13] & 0xFFFF) << 13;
				break;
			case CHR_MODE_1:
				flashAddr &= 0x0FFF;
				flashAddr |= (_chr[addr >> 12] & 0xFFFF) << 12;
				break;
			case CHR_MODE_2:
				flashAddr &= 0x07FF;
				flashAddr |= (_chr[addr >> 11] & 0xFFFF) << 11;
				break;
			case CHR_MODE_3:
				flashAddr &= 0x03FF;
				flashAddr |= (_chr[addr >> 10] & 0xFFFF) << 10;
				break;
			case CHR_MODE_4:
				flashAddr &= 0x01FF;
				flashAddr |= (_chr[addr >> 9] & 0xFFFF) << 9;
				break;
		}
		return flashAddr;
	}
	/*
	vector<MapperStateEntry> GetMapperStateEntries() override
	{
		vector<MapperStateEntry> entries;

		entries.push_back(MapperStateEntry("$4100.0-2", "PRG ROM Mode", _prgRomMode, MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4100.7", "PRG RAM Mode", _prgRamMode, MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4115.0", "PRG Bank Register 0", _prg[0], MapperStateValueType::Number8));
		for(int i = 1; i < 12; i++) {
			if(i == 1 || i == 2) {
				if((_prg[i] & 8000) >> 15 == 0) { // PRG-ROM
					entries.push_back(MapperStateEntry("$" + HexUtilities::ToHex(0x4106 + i) + ".7", "PRG Bank Chip Selector Register " + HexUtilities::ToHex(0x6000 + i * 0x1000), (_prg[i] & 0x8000) >> 15, MapperStateValueType::Number8));
				} else { // PRG-RAM or FPGA-RAM
					entries.push_back(MapperStateEntry("$" + HexUtilities::ToHex(0x4106 + i) + ".6-7", "PRG Bank Chip Selector Register " + HexUtilities::ToHex(0x6000 + i * 0x1000), (_prg[i] & 0xC000) >> 14, MapperStateValueType::Number8));
				}
			} else {
				entries.push_back(MapperStateEntry("$" + HexUtilities::ToHex(0x4106 + i) + ".7", "PRG Bank Chip Selector Register " + HexUtilities::ToHex(0x6000 + i * 0x1000), (_prg[i] & 0x8000) >> 15, MapperStateValueType::Number8));
			}
			entries.push_back(MapperStateEntry("$" + HexUtilities::ToHex(0x4106 + i) + ".0-6", "PRG Bank Upper Bits Register " + HexUtilities::ToHex(0x6000 + i * 0x1000), (_prg[i] & 0x7F00) >> 8, MapperStateValueType::Number8));
			entries.push_back(MapperStateEntry("$" + HexUtilities::ToHex(0x4116 + i), "PRG Bank Lower Bits Register " + HexUtilities::ToHex(0x6000 + i * 0x1000), _prg[i] & 0xFF, MapperStateValueType::Number8));
		}

		entries.push_back(MapperStateEntry("$4120.0-2", "CHR Mode", _chrMode, MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4120.4", "Window Split Mode", _windowModeEnabled));
		entries.push_back(MapperStateEntry("$4120.5", "Sprite Extended Mode", _chrSprExtMode));
		entries.push_back(MapperStateEntry("$4120.6-7", "CHR Chip Selector", _chrChip, MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4121.0-4", "Background Extended Mode Bank Upper Bits", _bgBankOffset, MapperStateValueType::Number8));

		entries.push_back(MapperStateEntry("$4126", "Nametable A ($2000) Bank", _ntBank[0], MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4127", "Nametable B ($2400) Bank", _ntBank[1], MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4128", "Nametable C ($2800) Bank", _ntBank[2], MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4129", "Nametable D ($2C00) Bank", _ntBank[3], MapperStateValueType::Number8));

		entries.push_back(MapperStateEntry("$412A", "Nametable A ($2000) Control", _ntControl[0], MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$412B", "Nametable B ($2400) Control", _ntControl[1], MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$412C", "Nametable C ($2800) Control", _ntControl[2], MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$412D", "Nametable D ($2C00) Control", _ntControl[3], MapperStateValueType::Number8));

		entries.push_back(MapperStateEntry("$412E", "Nametable W (Window Split) Bank", _ntBank[4], MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$412F", "Nametable W (Window Split) Control", _ntControl[4], MapperStateValueType::Number8));

		for(int i = 0; i < 16; i++) {
			entries.push_back(MapperStateEntry("$" + HexUtilities::ToHex(0x4130 + i), "CHR Bank Upper Bits Register " + std::to_string(i), _chr[i] & 0xFF, MapperStateValueType::Number8));
			entries.push_back(MapperStateEntry("$" + HexUtilities::ToHex(0x4140 + i), "CHR Bank Lower Bits Register " + std::to_string(i), (_chr[i] & 0xFF00) >> 8, MapperStateValueType::Number8));
		}

		entries.push_back(MapperStateEntry("$4150", "Scanline IRQ Latch Value", _scanlineIrqLatch, MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4151", "Scanline IRQ Enable", _scanlineIrqEnable));
		entries.push_back(MapperStateEntry("$4153", "Scanline IRQ Offset Value", _scanlineIrqLatch, MapperStateValueType::Number8));

		entries.push_back(MapperStateEntry("$4158", "CPU Cycle IRQ Latch Low Value", _cpuCycleIrqLatch & 0xFF, MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4159", "CPU Cycle IRQ Latch High Value", (_cpuCycleIrqLatch & 0xFF00) >> 8, MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$415A.0", "CPU Cycle IRQ Enable", _cpuCycleIrqEnable));
		entries.push_back(MapperStateEntry("$415A.1", "CPU Cycle IRQ Reset", _cpuCycleIrqReset));

		entries.push_back(MapperStateEntry("$4160", "Mapper Version", (uint8_t)MAPPER_VERSION, MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4161.0", "ESP/Wi-Fi IRQ Status", _espIrqPending));
		entries.push_back(MapperStateEntry("$4161.6", "CPU Cycle IRQ Status", _cpuCycleIrqPending));
		entries.push_back(MapperStateEntry("$4161.7", "Scanline IRQ Status", _scanlineIrqPending));

		entries.push_back(MapperStateEntry("$4170.0-4", "Window Split Mode X Start Tile", _windowXStartTile, MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4171.0-4", "Window Split Mode X End Tile", _windowXEndTile, MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4172", "Window Split Mode Y Start", _windowYStart, MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4173", "Window Split Mode Y End", _windowYEnd, MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4174.0-4", "Window Split Mode X Scroll (Tile)", _windowXScroll, MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4175", "Window Split Mode Y Scroll", _windowYScroll, MapperStateValueType::Number8));

		entries.push_back(MapperStateEntry("$4190.0", "ESP/Wi-Fi Enable", _espEnable));
		entries.push_back(MapperStateEntry("$4190.1", "ESP/Wi-Fi IRQ Enable", _espIrqEnable));
		entries.push_back(MapperStateEntry("$4191.6", "ESP/Wi-Fi RX Data Ready (R)", _esp->getDataReadyIO()));
		entries.push_back(MapperStateEntry("$4191.7", "ESP/Wi-Fi RX Data Received (R)", _espHasReceivedMessage));
		entries.push_back(MapperStateEntry("$4192.7", "ESP/Wi-Fi TX Data Sent (R)", _espMessageSent));
		entries.push_back(MapperStateEntry("$4193.0-2", "ESP/Wi-Fi RX Ram Destination", _espRxAddress, MapperStateValueType::Number8));
		entries.push_back(MapperStateEntry("$4194.0-2", "ESP/Wi-Fi TX Ram Destination", _espTxAddress, MapperStateValueType::Number8));

		for(int i = 0; i < 64; i++) {
			entries.push_back(MapperStateEntry("$" + HexUtilities::ToHex(0x4200 + i), "Sprite Extended Mode Lower Bits Bank Register " + std::to_string(i), _spriteBank[i], MapperStateValueType::Number8));
		}
		entries.push_back(MapperStateEntry("$4240.0-2", "Sprite Extended Mode Upper Bits Bank", _spriteBankOffset, MapperStateValueType::Number8));

		// TODO
		// _audio->GetMapperStateEntries(entries);

		return entries;
	}
	*/
	__forceinline uint8_t ReadFromChr(uint32_t pos)
	{
		switch(_chrChip) {
			case CHR_CHIP_ROM:
				if(_chrRomSize == 0) return 0;
				return _chrRom[pos & (_chrRomSize - 1)];
			case CHR_CHIP_RAM:
				if(_chrRamSize == 0) return 0;
				return _chrRam[pos & (_chrRamSize - 1)];
			case CHR_CHIP_FPGA_RAM:
				if(FpgaRamSize == 0) return 0;
				return _fpgaRam[pos & (FpgaRamSize - 1)];
			default:
				return 0;
		}
	}

	void EspCheckNewMessage()
	{
		// get new message if needed
		if(_espEnable && _esp->getDataReadyIO() && _espHasReceivedMessage == false) {
			uint8_t message_length = _esp->tx();
			_fpgaRam[0x1800 + (_espRxAddress << 8)] = message_length;
			for(uint8_t i = 0; i < message_length; i++) {
				_fpgaRam[0x1800 + (_espRxAddress << 8) + 1 + i] = _esp->tx();
			}
			_espRxIndex = 0;
			_espHasReceivedMessage = true;
		}
	}

	bool EspMessageReceived()
	{
		EspCheckNewMessage();
		return _espHasReceivedMessage;
	}

	void EspClearMessageReceived()
	{
		_espHasReceivedMessage = false;
	}

};

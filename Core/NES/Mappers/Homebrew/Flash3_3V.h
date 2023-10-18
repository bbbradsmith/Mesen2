#pragma once
#include "pch.h"
#include "Utilities/ISerializable.h"
#include "Utilities/Serializer.h"

//3.3V Flash chips emulation - used by mapper 682 (Rainbow)
class Flash3_3V final : public ISerializable
{
private:
	enum class ChipMode
	{
		WaitingForCommand,
		UnlockBypass,
		UnlockBypassWrite,
		UnlockBypassReset,
		Write,
		Erase
	};

	enum class BootSectorConfig
	{
		//Uniform,
		Top,
		Boot
	};

	ChipMode _mode = ChipMode::WaitingForCommand;
	uint8_t _cycle = 0;
	uint8_t _softwareId = false;
	BootSectorConfig _bootSectorConfig = BootSectorConfig::Top;

	//PRG/CHR data and size
	uint8_t* _data = nullptr;
	uint32_t _size = 0;

protected:
	void Serialize(Serializer& s)
	{
		SV(_mode);
		SV(_cycle);
		SV(_softwareId);
		SV(_bootSectorConfig);
	}

public:
	Flash3_3V(uint8_t* data, uint32_t size)
	{
		_data = data;
		_size = size;
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> distr(0, 1);
		_bootSectorConfig = (BootSectorConfig)distr(gen);
	}

	int16_t Read(uint32_t addr)
	{
		uint32_t size = _size / 1024;
		if(_softwareId) {
			switch(addr & 0x1FF) {
				case 0x00:
				{
					if(size < 1024)
						return 0xFF; // size less than 1024KiB is not supported by the Rainbow board
					else
						return 0x01; // 0x01 = Cypress
				}
				case 0x02:
				{
					switch(size) {
						case 1024: return _bootSectorConfig == BootSectorConfig::Top ? 0xDA : 0x5B;	// S29AL008: 0xDA = top boot block | 0x5B = bottom boot block
						case 2048: return _bootSectorConfig == BootSectorConfig::Top ? 0xC4 : 0x49;	// S29AL016: 0xC4 = top boot block | 0x49 = bottom boot block
						case 4096: return 0x7E; // S29JL032
						case 8192: return 0x7E;	// S29GL064S
						default: return 0xFF;
					}
				}
				case 0x1C:
				{
					switch(size) {
						case 4096: return 0x0A; // S29JL032
						case 8192: return 0x10;	// S29GL064S
						default: return 0xFF;
					}
				}
				case 0x1E:
				{
					switch(size) {
						case 4096: return _bootSectorConfig == BootSectorConfig::Top ? 0x01 : 0x00;	// S29JL032: 0x00 = bottom boot block | 0x01 = top boot block
						case 8192: return _bootSectorConfig == BootSectorConfig::Top ? 0x01 : 0x00;	// S29GL064S: 0x00 = bottom boot block | 0x01 = top boot block
						default: return 0xFF;
					}
				}
				default: return 0xFF;
			}
		}
		return -1;
	}

	void ResetState()
	{
		_mode = ChipMode::WaitingForCommand;
		_cycle = 0;
	}

	void Write(uint32_t addr, uint8_t value)
	{
		uint16_t cmd = addr & 0x0FFF;
		if(_mode == ChipMode::WaitingForCommand) {
			if(_cycle == 0) {
				if(cmd == 0x0AAA && value == 0xAA) {
					//1st write, $0AAA = $AA
					_cycle++;
				} else if(value == 0xF0) {
					//Software ID exit
					ResetState();
					_softwareId = false;
				}
			} else if(_cycle == 1 && cmd == 0x0555 && value == 0x55) {
				//2nd write, $0555 = $55
				_cycle++;
			} else if(_cycle == 2 && cmd == 0x0AAA) {
				//3rd write, determines command type
				_cycle++;
				switch(value) {
					case 0x20: _mode = ChipMode::UnlockBypass; _cycle = 0; break;
					case 0x80: _mode = ChipMode::Erase; break;
					case 0x90: ResetState();  _softwareId = true; break;
					case 0xA0: _mode = ChipMode::Write; break;
					case 0xF0: ResetState(); _softwareId = false; break;
				}
			} else {
				_cycle = 0;
			}
		} else if(_mode == ChipMode::UnlockBypass) {
			if(_cycle == 0) {
				//1st write for unlock bypass command
				switch(value) {
					case 0xA0: _cycle++; _mode = ChipMode::UnlockBypassWrite; break;
					case 0x90: _cycle++; _mode = ChipMode::UnlockBypassReset; break;
				}
			}
		} else if(_mode == ChipMode::UnlockBypassWrite) {
			//2nd write for unlock bypass write command
			if(_cycle == 1) {
				if(addr < _size) {
					_data[addr] &= value;
				}
				_cycle = 0;
				_mode = ChipMode::UnlockBypass;
			}
		} else if(_mode == ChipMode::UnlockBypassReset) {
			//2nd write for unlock bypass reset command
			if(_cycle == 1 && value == 0x00) {
				ResetState();
			} else {
				_cycle = 0;
				_mode = ChipMode::UnlockBypass;
			}
		} else if(_mode == ChipMode::Write) {
			//Write a single byte
			if(addr < _size) {
				_data[addr] &= value;
			}
			ResetState();
		} else if(_mode == ChipMode::Erase) {
			if(_cycle == 3) {
				//4th write for erase command, $0AAA = $AA
				if(cmd == 0x0AAA && value == 0xAA) {
					_cycle++;
				} else {
					ResetState();
				}
			} else if(_cycle == 4) {
				//5th write for erase command, $0555 = $55
				if(cmd == 0x0555 && value == 0x55) {
					_cycle++;
				} else {
					ResetState();
				}
			} else if(_cycle == 5) {
				if(cmd == 0x0AAA && value == 0x10) {
					//Chip erase
					memset(_data, 0xFF, _size);
				} else if(value == 0x30) {
					//Sector erase
					uint32_t sectorOffset = (addr & 0xFF0000);
					uint8_t sectorIndex = sectorOffset >> 16;
					uint8_t sectorSize = 64;
					uint32_t flashSize = _size / 1024;
					switch(flashSize) {
						case 1024: // S29AL008
							if(_bootSectorConfig == BootSectorConfig::Top && sectorIndex == 15) {
								if(addr >= 0xF0000 && addr <= 0xF7FFF) { sectorOffset = 0xF0000; sectorSize = 32; } else if(addr >= 0xF8000 && addr <= 0xF9FFF) { sectorOffset = 0xF8000; sectorSize = 8; } else if(addr >= 0xFA000 && addr <= 0xFBFFF) { sectorOffset = 0xFA000; sectorSize = 8; } else if(addr >= 0xFC000 && addr <= 0xFFFFF) { sectorOffset = 0xFC000; sectorSize = 16; }
							} else if(_bootSectorConfig != BootSectorConfig::Top && sectorIndex == 0) {
								if(addr >= 0x00000 && addr <= 0x03FFF) { sectorOffset = 0x00000; sectorSize = 16; } else if(addr >= 0x04000 && addr <= 0x05FFF) { sectorOffset = 0x04000; sectorSize = 8; } else if(addr >= 0x06000 && addr <= 0x07FFF) { sectorOffset = 0x06000; sectorSize = 8; } else if(addr >= 0x08000 && addr <= 0x0FFFF) { sectorOffset = 0x08000; sectorSize = 32; }
							}
							break;
						case 2048: // S29AL016
							if(_bootSectorConfig == BootSectorConfig::Top && sectorIndex == 31) {
								if(addr >= 0x1F0000 && addr <= 0x1F7FFF) { sectorOffset = 0xF0000; sectorSize = 32; } else if(addr >= 0x1F8000 && addr <= 0x1F9FFF) { sectorOffset = 0xF8000; sectorSize = 8; } else if(addr >= 0x1FA000 && addr <= 0x1FBFFF) { sectorOffset = 0xFA000; sectorSize = 8; } else if(addr >= 0x1FC000 && addr <= 0x1FFFFF) { sectorOffset = 0xFC000; sectorSize = 16; }
							} else if(_bootSectorConfig != BootSectorConfig::Top && sectorIndex == 0) {
								if(addr >= 0x00000 && addr <= 0x03FFF) { sectorOffset = 0x00000; sectorSize = 16; } else if(addr >= 0x04000 && addr <= 0x05FFF) { sectorOffset = 0x04000; sectorSize = 8; } else if(addr >= 0x06000 && addr <= 0x07FFF) { sectorOffset = 0x06000; sectorSize = 8; } else if(addr >= 0x08000 && addr <= 0x0FFFF) { sectorOffset = 0x08000; sectorSize = 32; }
							}
							break;
						case 4096: // S29JL032
							if(_bootSectorConfig == BootSectorConfig::Top && sectorIndex == 63) {
								if(addr >= 0x3F0000 && addr <= 0x3F1FFF) { sectorOffset = 0x3F0000; sectorSize = 8; } else if(addr >= 0x3F2000 && addr <= 0x3F3FFF) { sectorOffset = 0x3F2000; sectorSize = 8; } else if(addr >= 0x3F4000 && addr <= 0x3F5FFF) { sectorOffset = 0x3F4000; sectorSize = 8; } else if(addr >= 0x3F6000 && addr <= 0x3F7FFF) { sectorOffset = 0x3F6000; sectorSize = 8; } else if(addr >= 0x3F8000 && addr <= 0x3F9FFF) { sectorOffset = 0x3F8000; sectorSize = 8; } else if(addr >= 0x3FA000 && addr <= 0x3FBFFF) { sectorOffset = 0x3FA000; sectorSize = 8; } else if(addr >= 0x3FC000 && addr <= 0x3FDFFF) { sectorOffset = 0x3FC000; sectorSize = 8; } else if(addr >= 0x3FE000 && addr <= 0x3FFFFF) { sectorOffset = 0x3FE000; sectorSize = 8; }

							} else if(_bootSectorConfig != BootSectorConfig::Top && sectorIndex == 0) {
								if(addr >= 0x000000 && addr <= 0x001FFF) { sectorOffset = 0x000000; sectorSize = 8; } else if(addr >= 0x002000 && addr <= 0x003FFF) { sectorOffset = 0x002000; sectorSize = 8; } else if(addr >= 0x004000 && addr <= 0x005FFF) { sectorOffset = 0x004000; sectorSize = 8; } else if(addr >= 0x006000 && addr <= 0x007FFF) { sectorOffset = 0x006000; sectorSize = 8; } else if(addr >= 0x008000 && addr <= 0x009FFF) { sectorOffset = 0x008000; sectorSize = 8; } else if(addr >= 0x00A000 && addr <= 0x00BFFF) { sectorOffset = 0x00A000; sectorSize = 8; } else if(addr >= 0x00C000 && addr <= 0x00DFFF) { sectorOffset = 0x00C000; sectorSize = 8; } else if(addr >= 0x00E000 && addr <= 0x00FFFF) { sectorOffset = 0x00E000; sectorSize = 8; }
							}
							break;
						case 8192: // S29GL064S
							if(_bootSectorConfig != BootSectorConfig::Top && sectorIndex == 0) {
								if(addr >= 0x000000 && addr <= 0x001FFF) { sectorOffset = 0x000000; sectorSize = 8; } else if(addr >= 0x002000 && addr <= 0x003FFF) { sectorOffset = 0x002000; sectorSize = 8; } else if(addr >= 0x004000 && addr <= 0x005FFF) { sectorOffset = 0x004000; sectorSize = 8; } else if(addr >= 0x006000 && addr <= 0x007FFF) { sectorOffset = 0x006000; sectorSize = 8; } else if(addr >= 0x008000 && addr <= 0x009FFF) { sectorOffset = 0x008000; sectorSize = 8; } else if(addr >= 0x00A000 && addr <= 0x00BFFF) { sectorOffset = 0x00A000; sectorSize = 8; } else if(addr >= 0x00C000 && addr <= 0x00DFFF) { sectorOffset = 0x00C000; sectorSize = 8; } else if(addr >= 0x00E000 && addr <= 0x00FFFF) { sectorOffset = 0x00E000; sectorSize = 8; }
							} else if(_bootSectorConfig == BootSectorConfig::Top && sectorIndex == 127) {
								if(addr >= 0x7F0000 && addr <= 0x7F1FFF) { sectorOffset = 0x7F0000; sectorSize = 8; } else if(addr >= 0x7F2000 && addr <= 0x7F3FFF) { sectorOffset = 0x7F2000; sectorSize = 8; } else if(addr >= 0x7F4000 && addr <= 0x7F5FFF) { sectorOffset = 0x7F4000; sectorSize = 8; } else if(addr >= 0x7F6000 && addr <= 0x7F7FFF) { sectorOffset = 0x7F6000; sectorSize = 8; } else if(addr >= 0x7F8000 && addr <= 0x7F9FFF) { sectorOffset = 0x7F8000; sectorSize = 8; } else if(addr >= 0x7FA000 && addr <= 0x7FBFFF) { sectorOffset = 0x7FA000; sectorSize = 8; } else if(addr >= 0x7FC000 && addr <= 0x7FDFFF) { sectorOffset = 0x7FC000; sectorSize = 8; } else if(addr >= 0x7FE000 && addr <= 0x7FFFFF) { sectorOffset = 0x7FE000; sectorSize = 8; }
							}
							break;
					}
					memset(_data + sectorOffset, 0xFF, 1024 * sectorSize);
				}
				ResetState();
			}
		}
	}
};

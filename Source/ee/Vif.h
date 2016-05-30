#pragma once

#include "Types.h"
#include "Convertible.h"
#include "../uint128.h"
#include "../Profiler.h"
#include "zip/ZipArchiveWriter.h"
#include "zip/ZipArchiveReader.h"

class CVpu;
class CINTC;

class CVif
{
public:
	enum
	{
		REGS0_START = 0x10003800,
		VIF0_STAT = 0x10003800,
		VIF0_FBRST = 0x10003810,
		VIF0_ERR = 0x10003820,
		VIF0_MARK = 0x10003830,
		VIF0_CYCLE = 0x10003840,
		VIF0_MODE = 0x10003850,
		VIF0_NUM = 0x10003860,
		VIF0_MASK = 0x10003870,
		VIF0_CODE = 0x10003880,
		VIF0_R0 = 0x10003900,
		VIF0_R1 = 0x10003910,
		VIF0_R2 = 0x10003920,
		VIF0_R3 = 0x10003930,
		REGS0_END = 0x10003A00,

		REGS1_START = 0x10003C00,
		VIF1_STAT = 0x10003C00,
		VIF1_FBRST = 0x10003C10,
		VIF1_ERR = 0x10003C20,
		VIF1_MARK = 0x10003C30,
		VIF1_CYCLE = 0x10003C40,
		VIF1_MODE = 0x10003C50,
		VIF1_NUM = 0x10003C60,
		VIF1_MASK = 0x10003C70,
		VIF1_CODE = 0x10003C80,
		VIF1_R0 = 0x10003D00,
		VIF1_R1 = 0x10003D10,
		VIF1_R2 = 0x10003D20,
		VIF1_R3 = 0x10003D30,
		REGS1_END = 0x10003E00,

		VIF0_FIFO_START = 0x10004000,
		VIF0_FIFO_END = 0x10004FFF,
		VIF1_FIFO_START = 0x10005000,
		VIF1_FIFO_END = 0x10005FFF,
	};

	CVif(unsigned int, CVpu&, CINTC&, uint8*, uint8*);
	virtual ~CVif() = default;

	virtual void Reset();
	uint32 GetRegister(uint32);
	void SetRegister(uint32, uint32);
	virtual void SaveState(Framework::CZipArchiveWriter&);
	virtual void LoadState(Framework::CZipArchiveReader&);

	virtual uint32 GetTOP() const;
	virtual uint32 GetITOP() const;

	virtual uint32 ReceiveDMA(uint32, uint32, uint32, bool);

	bool IsWaitingForProgramEnd() const;

protected:
	enum
	{
		CODE_CMD_MARK = 0x07,
	};

	enum
	{
		FBRST_RST = 0x01,
		FBRST_FBK = 0x02,
		FBRST_STP = 0x04,
		FBRST_STC = 0x08
	};

	enum
	{
		STAT_FDR = 0x00800000,
	};

	enum
	{
		FIFO_SIZE = 0x100
	};

	class CFifoStream
	{
	public:
		CFifoStream(uint8*, uint8*);
		virtual ~CFifoStream() = default;

		void Reset();

		uint32 GetAvailableReadBytes() const;
		uint32 GetRemainingDmaTransferSize() const;
		void Read(void*, uint32);
		void Flush();
		void Align32();
		void SetDmaParams(uint32, uint32, bool);
		void SetFifoParams(uint8*, uint32);

		uint8* GetDirectPointer() const;
		void Advance(uint32);

	private:
		void SyncBuffer();

		enum
		{
			BUFFERSIZE = 0x10
		};

		uint8* m_ram = nullptr;
		uint8* m_spr = nullptr;

		uint128 m_buffer;
		uint32 m_bufferPosition = BUFFERSIZE;
		uint32 m_startAddress = 0;
		uint32 m_nextAddress = 0;
		uint32 m_endAddress = 0;
		bool m_tagIncluded = false;
		uint8* m_source = nullptr;
	};

	typedef CFifoStream StreamType;

	struct STAT : public convertible<uint32>
	{
		unsigned int nVPS : 2;
		unsigned int nVEW : 1;
		unsigned int nReserved0 : 3;
		unsigned int nMRK : 1;
		unsigned int nDBF : 1;
		unsigned int nVSS : 1;
		unsigned int nVFS : 1;
		unsigned int nVIS : 1;
		unsigned int nINT : 1;
		unsigned int nER0 : 1;
		unsigned int nER1 : 1;
		unsigned int nReserved2 : 9;
		unsigned int nFDR : 1; //VIF1 only
		unsigned int nFQC : 4;
		unsigned int nReserved3 : 4;
	};
	static_assert(sizeof(STAT) == sizeof(uint32), "Size of STAT struct must be 4 bytes.");

	struct ERR : public convertible<uint32>
	{
		unsigned int nMII : 1;
		unsigned int nME0 : 1;
		unsigned int nME1 : 1;
		unsigned int nReserved : 29;
	};
	static_assert(sizeof(ERR) == sizeof(uint32), "Size of ERR struct must be 4 bytes.");

	struct CYCLE : public convertible<uint32>
	{
		unsigned int nCL : 8;
		unsigned int nWL : 8;
		unsigned int reserved : 16;
	};
	static_assert(sizeof(CYCLE) == sizeof(uint32), "Size of CYCLE struct must be 4 bytes.");

	struct CODE : public convertible<uint32>
	{
		unsigned int nIMM : 16;
		unsigned int nNUM : 8;
		unsigned int nCMD : 7;
		unsigned int nI : 1;
	};
	static_assert(sizeof(CODE) == sizeof(uint32), "Size of CODE struct must be 4 bytes.");

	enum ADDMODE
	{
		MODE_NORMAL = 0,
		MODE_OFFSET = 1,
		MODE_DIFFERENCE = 2
	};

	enum MASKOP
	{
		MASK_DATA = 0,
		MASK_ROW = 1,
		MASK_COL = 2,
		MASK_MASK = 3
	};

	void ProcessFifoWrite(uint32, uint32);

	void ProcessPacket(StreamType&);
	virtual void ExecuteCommand(StreamType&, CODE);
	virtual void Cmd_UNPACK(StreamType&, CODE, uint32);

	void Cmd_MPG(StreamType&, CODE);
	void Cmd_STROW(StreamType&, CODE);
	void Cmd_STCOL(StreamType&, CODE);
	void Cmd_STMASK(StreamType&, CODE);

	bool Unpack_ReadValue(const CODE&, StreamType&, uint128&, bool);
	bool Unpack_S32(StreamType&, uint128&);
	bool Unpack_S16(StreamType&, uint128&, bool);
	bool Unpack_S8(StreamType&, uint128&, bool);
	bool Unpack_V16(StreamType&, uint128&, unsigned int, bool);
	bool Unpack_V8(StreamType&, uint128&, unsigned int, bool);
	bool Unpack_V32(StreamType&, uint128&, unsigned int);
	bool Unpack_V45(StreamType&, uint128&);

	uint32 GetMaskOp(unsigned int, unsigned int) const;

	template <uint8 dataType>
	bool UnpackReadValueGeneric(StreamType& stream, uint128& writeValue, bool usn)
	{
		bool success = false;
		switch(dataType)
		{
		case 0x00:
			//S-32
			success = Unpack_S32(stream, writeValue);
			break;
		case 0x01:
			//S-16
			success = Unpack_S16(stream, writeValue, usn);
			break;
		case 0x02:
			//S-8
			success = Unpack_S8(stream, writeValue, usn);
			break;
		case 0x04:
			//V2-32
			success = Unpack_V32(stream, writeValue, 2);
			break;
		case 0x05:
			//V2-16
			success = Unpack_V16(stream, writeValue, 2, usn);
			break;
		case 0x06:
			//V2-8
			success = Unpack_V8(stream, writeValue, 2, usn);
			break;
		case 0x08:
			//V3-32
			success = Unpack_V32(stream, writeValue, 3);
			break;
		case 0x09:
			//V3-16
			success = Unpack_V16(stream, writeValue, 3, usn);
			break;
		case 0x0A:
			//V3-8
			success = Unpack_V8(stream, writeValue, 3, usn);
			break;
		case 0x0C:
			//V4-32
			success = Unpack_V32(stream, writeValue, 4);
			break;
		case 0x0D:
			//V4-16
			success = Unpack_V16(stream, writeValue, 4, usn);
			break;
		case 0x0E:
			//V4-8
			success = Unpack_V8(stream, writeValue, 4, usn);
			break;
		case 0x0F:
			//V4-5
			success = Unpack_V45(stream, writeValue);
			break;
		default:
			assert(0);
			break;
		}
		return success;
	}

	template <uint8 dataType, bool clGreaterEqualWl, bool useMask>
	void UnpackGeneric(StreamType& stream, CODE nCommand, uint32 nDstAddr)
	{
		assert((nCommand.nCMD & 0x60) == 0x60);

		const auto vuMem = m_vpu.GetVuMemory();
		const auto vuMemSize = m_vpu.GetVuMemorySize();
		bool usn = (m_CODE.nIMM & 0x4000) != 0;
		//bool useMask = (nCommand.nCMD & 0x10) != 0;
		uint32 cl = m_CYCLE.nCL;
		uint32 wl = m_CYCLE.nWL;
		if(wl == 0) 
		{
			wl = UINT_MAX;
			cl = UINT_MAX;
		}

		if(m_NUM == nCommand.nNUM)
		{
			m_readTick = 0;
			m_writeTick = 0;
		}

		uint32 currentNum = (m_NUM == 0) ? 256 : m_NUM;
		uint32 codeNum = (m_CODE.nNUM == 0) ? 256 : m_CODE.nNUM;
		uint32 transfered = codeNum - currentNum;

		if(cl > wl)
		{
			nDstAddr += cl * (transfered / wl) + (transfered % wl);
		}
		else
		{
			nDstAddr += transfered;
		}

		nDstAddr *= 0x10;

		while(currentNum != 0)
		{
			bool mustWrite = false;
			uint128 writeValue;
			memset(&writeValue, 0, sizeof(writeValue));

			//if(cl >= wl)
			if(clGreaterEqualWl)
			{
				if(m_readTick < wl || wl == 0)
				{
					bool success = UnpackReadValueGeneric<dataType>(stream, writeValue, usn);
					if(!success) break;
					mustWrite = true;
				}
			}
			else
			{
				if(m_writeTick < cl)
				{
					bool success = UnpackReadValueGeneric<dataType>(stream, writeValue, usn);
					if(!success) break;
				}

				mustWrite = true;
			}

			if(mustWrite)
			{
				auto dst = reinterpret_cast<uint128*>(vuMem + nDstAddr);

				for(unsigned int i = 0; i < 4; i++)
				{
					uint32 maskOp = useMask ? GetMaskOp(i, m_writeTick) : MASK_DATA;

					if(maskOp == MASK_DATA)
					{
						if(m_MODE == MODE_OFFSET)
						{
							writeValue.nV[i] += m_R[i];
						}
						else if(m_MODE == MODE_DIFFERENCE)
						{
							writeValue.nV[i] += m_R[i];
							m_R[i] = writeValue.nV[i];
						}

						dst->nV[i] = writeValue.nV[i];
					}
					else if(maskOp == MASK_ROW)
					{
						dst->nV[i] = m_R[i];
					}
					else if(maskOp == MASK_COL)
					{
						int index = (m_writeTick > 3) ? 3 : m_writeTick;
						dst->nV[i] = m_C[index];
					}
					else if(maskOp == MASK_MASK)
					{
						//Don't write anything
					}
					else
					{
						assert(0);
					}
				}

				currentNum--;
			}

			//if(cl >= wl)
			if(clGreaterEqualWl)
			{
				m_writeTick = std::min<uint32>(m_writeTick + 1, wl);
				m_readTick = std::min<uint32>(m_readTick + 1, cl);

				if(m_readTick == cl)
				{
					m_writeTick = 0;
					m_readTick = 0;
				}
			}
			else
			{
				m_writeTick = std::min<uint32>(m_writeTick + 1, wl);
				m_readTick = std::min<uint32>(m_readTick + 1, cl);

				if(m_writeTick == wl)
				{
					m_writeTick = 0;
					m_readTick = 0;
				}
			}

			nDstAddr += 0x10;
			nDstAddr &= (vuMemSize - 1);
		}

		if(currentNum != 0)
		{
			m_STAT.nVPS = 1;
		}
		else
		{
			stream.Align32();
			m_STAT.nVPS = 0;
		}

		m_NUM = static_cast<uint8>(currentNum);
	}

	typedef void (CVif::*Unpacker)(StreamType& stream, CODE nCommand, uint32 nDstAddr);

	Unpacker m_unpacker[0x40];

	virtual void PrepareMicroProgram();
	void StartMicroProgram(uint32);
	void StartDelayedMicroProgram(uint32);
	bool ResumeDelayedMicroProgram();

	void DisassembleCommand(CODE);
	void DisassembleGet(uint32);
	void DisassembleSet(uint32, uint32);

	unsigned int m_number = 0;
	CVpu& m_vpu;
	CINTC& m_intc;
	uint8* m_ram = nullptr;
	uint8* m_spr = nullptr;
	CFifoStream m_stream;

	uint8 m_fifoBuffer[FIFO_SIZE];
	uint32 m_fifoIndex = 0;

	STAT m_STAT;
	ERR m_ERR;
	CYCLE m_CYCLE;
	CODE m_CODE;
	uint8 m_NUM;
	uint32 m_MODE;
	uint32 m_R[4];
	uint32 m_C[4];
	uint32 m_MASK;
	uint32 m_MARK;
	uint32 m_ITOP;
	uint32 m_ITOPS;
	uint32 m_readTick;
	uint32 m_writeTick;
	uint32 m_pendingMicroProgram;
	uint32 m_incomingFifoDelay;

	CProfiler::ZoneHandle m_vifProfilerZone = 0;
};

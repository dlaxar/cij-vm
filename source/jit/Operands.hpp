#pragma once

#include <types.hpp>
#include <ostream>

namespace am2017s::jit
{
	enum OperandSize
	{
		BYTE  = 1,
		WORD  = 2,
		DWORD = 4,
		QWORD = 8,
	};

	// do not re-order; values are in encoded order
	enum RegOp : u8
	{
		RAX,
		RCX,
		RDX,
		RBX,

		RSP,
		RBP,

		RSI,
		RDI,

		R8,
		R9,
		R10,
		R11,
		R12,
		R13,
		R14,
		R15,

		NONE = 0xFF,
	};

	enum XMMOp : u8 {
		XMM0,
		XMM1,
		XMM2,
		XMM3,
		XMM4,
		XMM5,
		XMM6,
		XMM7,
		XMM8,
		XMM9,
		XMM10,
		XMM11,
		XMM12,
		XMM13,
		XMM14,

		XMMNONE = 0xFF
	};

	std::ostream& operator<<(std::ostream& os, RegOp const& reg);

	inline
	bool isExtended(RegOp reg)
	{
		return reg >= R8;
	}

	inline
	bool isExtended(XMMOp xmm) {
		return xmm >= XMM8;
	}

	struct MemOp
	{
		RegOp base;
		RegOp index;
		u8 scale;
		i32 offset;

		MemOp(RegOp base)
			: MemOp(base, 0)
		{}

		MemOp(RegOp base, i32 offset)
			: MemOp(base, NONE, 0, offset)
		{}

		MemOp(RegOp base, RegOp index, u8 scale)
			: MemOp(base, index, scale, 0)
		{}

		MemOp(RegOp base, RegOp index, u8 scale, i32 offset)
			: base(base), index(index), scale(scale), offset(offset)
		{
			if(index == RSP)
				throw std::runtime_error("invalid index register");
		}

		inline bool operator<(MemOp const& other) const {
			if(base < other.base) {
				return true;
			}

			if(base == other.base && index < other.index) {
				return true;
			}

			if(base == other.base && index == other.index && scale < other.scale) {
				return true;
			}

			if(base == other.base && index == other.index && scale == other.scale) {
				return offset < other.offset;
			}

			return false;
		};

		inline bool operator==(MemOp const& other) const {
			return base == other.base && index == other.index && scale == other.scale && offset == other.offset;
		}

		friend std::ostream& operator<<(std::ostream& os, MemOp const& obj);
	};

	class RegMemOp
	{
		union
		{
			RegOp _rop;
			XMMOp _xmm;
			MemOp _mop;
		};

		bool _isReg;
		bool _isXMM;

	public:
		// todo we shouldn't need this default constructor
		RegMemOp() {}
		RegMemOp(RegOp rop)
			: _rop(rop), _isReg(true), _isXMM(false)
		{}

		RegMemOp(XMMOp xmm)
				: _xmm(xmm), _isReg(false), _isXMM(true)
		{}

		RegMemOp(MemOp mop)
			: _mop(mop), _isReg(false), _isXMM(false)
		{}


		bool isReg() const { return _isReg; }
		bool isXMM() const { return _isXMM; }
		bool isMem() const { return !_isReg && !_isXMM; }

		RegOp reg() const { return _rop; }
		XMMOp xmm() const { return _xmm; }
		MemOp mem() const { return _mop; }

		RegOp base() const { return _mop.base; }
		RegOp index() const { return _mop.index; }
		u8 scale() const { return _mop.scale; }
		i32 offset() const { return _mop.offset; }

		inline bool operator<(RegMemOp const& other) const {
			if(_isReg && !other._isReg) {
				// this is a reg; other is not a reg
				return true;
			} else if(_isReg && other._isReg) {
				// this is a reg and the other one is also a reg
				return _rop < other._rop;
			} else if(!_isReg && other._isReg) {
				// this is not a reg but the other one is a reg
				return false;
			} else if(_isXMM && !other._isXMM) {
				// non is a reg, this is an XMM but the other is not
				return true;
			} else if(_isXMM && other._isXMM) {
				// non is a reg, both are XMM
				return _xmm < other._xmm;
			} else if(!_isXMM && other._isXMM) {
				// non is a reg, this is not a XMM but the other is
				return false;
			} else {
				// none are regs or xmms
				return _mop < other._mop;
			}
		};

		inline bool operator==(RegMemOp const& other) const {
			if(isReg()) {
				return other.isReg() && reg() == other.reg();
			} else if(isXMM()) {
				return other.isXMM() && xmm() == other.xmm();
			} else {
				return other.isMem() && mem() == other.mem();
			}
		}

		friend std::ostream& operator<<(std::ostream& os, RegMemOp const& obj)
		{
			if(obj.isReg()) {
				return os << std::to_string(obj.reg());
			} else if(obj.isXMM()) {
				return os << std::to_string(obj.xmm());
			} else {
				return os << obj.mem();
			}
		}
	};
}

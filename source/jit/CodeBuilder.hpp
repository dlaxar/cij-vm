#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

#include <types.hpp>
#include <jit/Operands.hpp>
#include <exception/NotImplementedException.hpp>

namespace am2017s::jit::internal
{
	template <typename T, typename U>
	bool fitsInto(U val)
	{
		return static_cast<T>(val) == val;
	}

	inline
	u8 log2(u8 val)
	{
		return __builtin_ctz(val);
	}

	// those are in big endian because that's how they are represented in the instruction
	enum Comparison : u16
	{
		EQ = 0x0F94,
		NEQ = 0x0F95,
		LT = 0x0F9C,
		GTE = 0x0F9D,
		LTE = 0x0F9E,
		GT = 0x0F9F,
	};
}

namespace am2017s::jit
{
	/*
	 * AMD64 instruction format:
	 * - prefixes (0-4, one byte each) [ex: LOCK prefix]
	 * - REX prefix if needed (register extension)
	 * - opcode (1-3 bytes)
	 * - ModR/M byte if needed
	 * - SIB byte if needed (scale/index/base)
	 * - displacement if needed
	 * - immediate if needed
	 * - max. 15 bytes
	 */
	class CodeBuilder
	{
		std::vector<u8> _buf;

		void byte(u8 value)
		{
			_buf.push_back(value);
		}

		void word(i16 value)
		{
			byte(value);
			byte(value >> 8);
		}

		void dword(i32 value)
		{
			byte(value);
			byte(value >> 8);
			byte(value >> 16);
			byte(value >> 24);
		}

		void qword(i64 value)
		{
			byte(value);
			byte(value >> 8);
			byte(value >> 16);
			byte(value >> 24);
			byte(value >> 32);
			byte(value >> 40);
			byte(value >> 48);
			byte(value >> 56);
		}

		void opcode(u8 byte0)
		{
			byte(byte0);
		}

		void dopcode(u16 shrt)
		{
			byte(shrt >> 8);
			byte(shrt);
		}

		void opcode(u8 byte0, u8 byte1)
		{
			byte(byte0);
			byte(byte1);
		}

		void opcode(u8 byte0, u8 byte1, u8 byte2)
		{
			byte(byte0);
			byte(byte1);
			byte(byte2);
		}

		/*
		 * REX prefix byte:
		 * +----------------------+
		 * | 0100 | w | r | x | b |
		 * +----------------------+
		 * w: 64 bit operand size (required for instructions that default to 32 bit in 64 bit mode)
		 * r: additional 4th bit for register number formed using modrm.reg (required for r8-r15)
		 * x: additional 4th bit for register number formed using sib.index (required for r8-r15)
		 * b: additional 4th bit for register number formed using sib.base or modrm.r/m (required for r8-r15)
		 */
		void rex(bool w, bool r, bool x, bool b)
		{
			auto flags = (w << 3) | (r << 2) | (x << 1) | b;

			if(flags)
				byte(0x40 | flags);
		}

		void force_rex(bool w, bool r, bool x, bool b) {
			auto flags = (w << 3) | (r << 2) | (x << 1) | b;
			byte(0x40 | flags);
		}

		/**
		 *
		 * @param mod
		 * @param reg this is often used as an extension to opcodes (OPERATION /reg)
		 * @param rm
		 */
		void modrm(u8 mod, u8 reg, u8 rm)
		{
			byte((mod << 6) | ((reg & 0b111) << 3) | (rm & 0b111));
		}

		void sib(RegOp base, RegOp index, u8 scale)
		{
			byte((internal::log2(scale) << 6) | ((index & 0b111) << 3) | (base & 0b111));
		}

		void sib64(RegOp base, u8 scale, u8 index, i32 offset)
		{
			byte((scale & 0b11) << 6 | (index & 0b111) << 3 | (base & 0b111));
			dword(offset);
		}

		void prefixes(OperandSize size, RegOp reg, RegMemOp rm)
		{
			if(size == WORD)
			{
				// operand-size override
				byte(0x66);
			}

			auto w = size == QWORD;
			auto r = reg == NONE ? 0 : isExtended(reg);
			auto x = rm.isReg() || rm.mem().index == NONE ? false : isExtended(rm.mem().index);
			auto b = rm.isReg() ? isExtended(rm.reg()) : rm.mem().base == NONE ? false : isExtended(rm.mem().base);

			// todo this might not be exactly right with respect to `rm`
			if(size == BYTE && rm.isReg() && (reg > RBX || rm.reg() > RBX)) {
				force_rex(w, r, x, b);
			} else {
				rex(w, r, x, b);
			}
		}

		void operands(RegOp reg, RegMemOp rm)
		{
			if(rm.isReg())
				operands(reg, rm.reg());
			else
				operands(reg, rm.mem());
		}

		void operands(RegOp reg, RegOp rm)
		{
			modrm(0b11, reg & 0b111, rm & 0b111);
		}

		void operands(RegOp reg, MemOp rm)
		{
			if(rm.index == NONE)
			{
				if(rm.offset == 0)
				{
					if((rm.base & 0b111) == 0b100)
					{
						modrm(0b00, reg, 0b100);
						sib(rm.base, RegOp(0b100), 1);
					}
					else if((rm.base & 0b111) == 0b101)
					{
						modrm(0b01, reg, 0b101);
						byte(0);
					}
					else
						modrm(0b00, reg, rm.base);
				}
				else if(internal::fitsInto<i8>(rm.offset))
				{
					if((rm.base & 0b111) == 0b100)
					{
						modrm(0b01, reg, 0b100);
						sib(rm.base, RegOp(0b100), 1);
						byte(rm.offset);
					}
					else
					{
						modrm(0b01, reg, rm.base);
						byte(rm.offset);
					}
				}
				else
				{
					if((rm.base & 0b111) == 0b100)
					{
						modrm(0b10, reg, 0b100);
						sib(rm.base, RegOp(0b100), 1);
						dword(rm.offset);
					}
					else
					{
						modrm(0b10, reg, rm.base);
						dword(rm.offset);
					}
				}
			}
			else
			{
				if(rm.offset == 0)
				{
					if((rm.base & 0b111) == 0b101)
					{
						modrm(0b01, reg, 0b100);
						sib(rm.base, rm.index, rm.scale);
						byte(0);
					}
					else
					{
						modrm(0b00, reg, 0b100);
						sib(rm.base, rm.index, rm.scale);
					}
				}
				else if(internal::fitsInto<i8>(rm.offset))
				{
					modrm(0b01, reg, 0b100);
					sib(rm.base, rm.index, rm.scale);
					byte(rm.offset);
				}
				else
				{
					modrm(0b10, reg, 0b100);
					sib(rm.base, rm.index, rm.scale);
					dword(rm.offset);
				}
			}
		}

	public:
		std::vector<u8> build();

		void byte(u8 value, u32 idx)
		{
			_buf[idx] = value;
		}

		void quad(u32 value, u32 idx) {
			byte(value      , idx    );
			byte(value >>  8, idx + 1);
			byte(value >> 16, idx + 2);
			byte(value >> 24, idx + 3);
		}

		u32 offset() const
		{
			return _buf.size();
		}

		void ret()
		{
			opcode(0xC3);
		}

		void mov(RegMemOp src, RegMemOp dst, OperandSize size) {
			if((src.isReg() && dst.isReg()) ||
			   (src.isReg() && dst.isMem())) {
				mov(src.reg(), dst, size);
			} else if(src.isMem() && dst.isReg()) {
				mov(src, dst.reg(), size);
			} else if(src.isXMM() && dst.isXMM()) {
				movf(src.xmm(), dst.xmm(), size);
			} else if(src.isMem() && dst.isXMM() && size == DWORD) {
				movss_from_mem(src.mem(), dst.xmm());
			} else if(src.isXMM() && dst.isMem() && size == DWORD) {
				// todo size restriction only because we haven't figured out how movd works for double
				movd(src.xmm(), dst.mem(), DWORD);
			} else if(src.isXMM() && dst.isMem() && size == QWORD) {
				movq(src.xmm(), dst.mem(), QWORD);
			} else if(src.isMem() && dst.isXMM() && size == QWORD) {
				movq(src.mem(), dst.xmm(), QWORD);
			} else {
				throw NotImplementedException();
			}
		}

		void push(RegOp src) {
			rex(false, false, false, isExtended(src));
			opcode((u8)(0x50 | (src & 0b111)));
		}

		void pop(RegOp dst) {
			rex(false, false, false, isExtended(dst));
			opcode((u8)(0x58 | (dst & 0b111)));
		}

		void mov(RegOp src, RegOp dst, OperandSize size)
		{
			if(src == dst) {
				return;
			}
			mov(src, RegMemOp(dst), size);
		}

		void mov(RegOp src, RegMemOp dst, OperandSize size)
		{
			if(size == BYTE && dst.isReg() && (src > RBX || dst.reg() > RBX)) {
				force_rex(false, isExtended(src), false, isExtended(dst.reg()));
				opcode(0x88);
				operands(src, dst);
			} else {
				prefixes(size, src, dst);
				opcode(size == BYTE ? 0x88 : 0x89);
				operands(src, dst);
			}
		}

		void mov(RegMemOp src, RegOp dst, OperandSize size)
		{
			prefixes(size, dst, src);
			opcode(size == BYTE ? 0x8A : 0x8B);
			operands(dst, src);
		}

		void movsx(RegMemOp src, RegOp dst, OperandSize size) {
			prefixes(QWORD, dst, src);

			if(size == BYTE) {
				opcode(0x0F, 0xBE);
			} else if(size == WORD) {
				opcode(0x0F, 0xBF);
			} else {
				throw std::runtime_error("movsx is only supported for sizes BYTE and WORD: " + std::to_string(size));
			}

			operands(dst, src);
		}

		void movsxd(RegMemOp src, RegOp dst, OperandSize size) {
			prefixes(QWORD, dst, src);
			opcode(0x63);
			operands(dst, src);
		}

		void movimm(i64 imm, RegOp dst)
		{
			if(imm == 0)
			{
				// xor reg, reg

				if(isExtended(dst))
					rex(true, true, false, true);

				opcode(0x31);
				modrm(0b11, dst & 0b111, dst & 0b111);
			}
			else if(internal::fitsInto<i32>(imm))
			{
				// mov imm32 with sign-ext
				rex(true, false, false, isExtended(dst));
				opcode(0xC7);
				modrm(0b11, 0b000, dst & 0b111);
				dword(imm);
			}
			else
			{
				// mov imm64
				rex(true, false, false, isExtended(dst));
				opcode(0xB8 | (dst & 0b111));
				qword(imm);
			}
		}

		void movf(XMMOp src, XMMOp dst, OperandSize size = QWORD) {
			opcode(0xF3);
			rex(false, isExtended(dst), false, isExtended(src));
			opcode(0x0F, 0x7E);
			modrm(0b11, dst, src);
		}

		/**
		 * OperandSize == DWORD
		 *
		 * @param src
		 * @param dst
		 */
		void movss(MemOp src, XMMOp dst) {
			opcode(0xF3);
			rex(false, isExtended(dst), false, isExtended(src.base));
			opcode(0x0F, 0x10);

			operands((RegOp)dst, src);
		}

		/**
		 * OperandSize == DWORD
		 *
		 * @param src
		 * @param dst
		 */
		void movss(XMMOp src, MemOp dst) {
			opcode(0xF3);
			rex(false, isExtended(src), false, isExtended(dst.base));
			opcode(0x0F, 0x11);

			operands((RegOp)src, dst);
		}

		/**
		 * OperandSize == DWORD
		 *
		 * @param src
		 * @param dst
		 */
		void movss_from_mem(MemOp src, XMMOp dst) {
			opcode(0xF3);
			rex(false, isExtended(dst), false, isExtended(src.base));
			opcode(0x0F, 0x10);

			operands((RegOp)dst, src);
		}

		void movd(RegOp src, XMMOp dst, OperandSize size) {
			// todo use size doenst work for size=double rex.w? (source: https://www.felixcloutier.com/x86/MOVD:MOVQ.html)
			opcode(0x66);
			rex(size == QWORD, isExtended(dst), false, isExtended(src));
			opcode(0x0F, 0x6E);

			modrm(0b11, dst, src);
		}

		void movd(XMMOp src, MemOp dst, OperandSize size) {
			opcode(0x66);
			rex(false, isExtended(src), false, isExtended(dst.base));
			opcode(0x0F, 0xD6);

			operands((RegOp)src, dst);
		}

		/**
		 * Moves a XMM operand to memory
		 *
		 * @param src
		 * @param dst
		 * @param size
		 */
		void movq(XMMOp src, MemOp dst, OperandSize size) {
			opcode(0x66);
			rex(false, isExtended(src), false, isExtended(dst.base));
			opcode(0x0F, 0xD6);

			operands((RegOp)src, dst);
		}

		/**
		 * Moves a memory location to an XMM register
		 *
		 * @param src
		 * @param dst
		 * @param size
		 */
		void movq(MemOp src, XMMOp dst, OperandSize size) {
			opcode(0xF3);
			rex(false, isExtended(dst), false, isExtended(src.base));
			opcode(0x0F, 0x7E);

			operands((RegOp)dst, src);
		}

		void add(RegOp src, RegOp dst, OperandSize size = QWORD)
		{
			prefixes(size, src, dst);
			opcode(0x01);
			operands(src, dst);
		}

		void add(MemOp src, RegOp dst, OperandSize size = QWORD)
		{
			prefixes(size, dst, src);
			opcode(0x03);
			operands(dst, src);
		}

		void add(RegOp toThis, i16 that)
		{
			rex(true, isExtended(toThis), false, false);
			opcode(0x81);
			modrm(0b11, 0, toThis & 0b111);
			dword(that);
		}

		// todo untested
		void add(i16 src, RegOp dst)
		{
			rex(true, isExtended(dst), false, isExtended(RSP));
			opcode(0x03);
			modrm(0b10, dst & 0b111, 0b100);
			sib64(RSP, 0, 0b100, src * 8);
		}

		void addf(XMMOp src, XMMOp dst, OperandSize size = QWORD) {
			if(size == DWORD) {
				opcode(0xF3);
			} else if(size == QWORD) {
				opcode(0xF2);
			}

			rex(false, isExtended(dst), false, isExtended(dst));
			opcode(0x0F, 0x58);

			modrm(0b11, dst, src);
		}

		void mulf(XMMOp src, XMMOp dst, OperandSize size = QWORD) {
			if(size == DWORD) {
				opcode(0xF3);
			} else if(size == QWORD) {
				opcode(0xF2);
			}

			rex(false, isExtended(dst), false, isExtended(src));
			opcode(0x0F, 0x59);

			modrm(0b11, dst, src);
		}

		void divf(XMMOp srcA, RegMemOp srcB, OperandSize size) {
			if(size == DWORD) {
				opcode(0xF3);
			} else if(size == QWORD) {
				opcode(0xF2);
			} else {
				throw std::runtime_error("no such float for divf");
			}

			rex(false, isExtended(srcA), false, false);
			opcode(0x0F, 0x5E);

			if(srcB.isXMM()) {
				modrm(0b11, srcA, srcB.xmm());
			} else {
				throw std::runtime_error("divf for srcB in mem not yet implemented");
			}
		}

		void sub(RegOp src, RegOp dst, OperandSize size = QWORD)
		{
			prefixes(size, src, dst);
			opcode(0x29);
			operands(src, dst);
		}

		void sub(RegMemOp src, RegOp dst, OperandSize size = QWORD)
		{
			prefixes(size, dst, src);
			opcode(0x2B);
			operands(dst, src);
		}

		void sub(RegOp fromThis, i16 that)
		{
			rex(true, isExtended(fromThis), false, false);
			opcode(0x81);
			modrm(0b11, 5, fromThis & 0b111);
			dword(that);
		}

		void imul(RegOp src, RegOp dst)
		{
			rex(true, isExtended(dst), false, isExtended(src));
			opcode(0x0F, 0xAF);
			operands(dst, src);
		}

		void imul(RegOp reg, RegMemOp rm) {
			rex(true, isExtended(reg), false, false);
			opcode(0x0F, 0xAF);
			operands(reg, rm);
		}

		// todo this is always 64b
		void idiv(RegOp divider, OperandSize size = QWORD)
		{
			prefixes(size, NONE, divider);
			opcode(0xF7);
			modrm(0b11, 7, divider);
		}

		void idiv(RegMemOp divider, OperandSize size = QWORD) {
			prefixes(size, NONE, divider);
//			rex(true, false, false, isExtended(divider.base));
			opcode(0xF7);
			operands(NONE, divider);
		}

		u32 jmp_riprel()
		{
			opcode(0xE9);
			auto offptr = offset();
			dword(0);
			return offptr;
		}

		u32 jmp_nz_riprel()
		{
			opcode(0x0F, 0x85);
			auto offptr = offset();
			dword(0);
			return offptr;
		}

		void call(RegOp through)
		{
			opcode(0xFF);
			modrm(0b11, 0b010, through & 0b111);
		}

		void call(RegOp base, i32 offset)
		{
			opcode(0xFF);
			modrm(0b10, 2, base & 0b111);
			dword(offset);
		}

		void call(RegOp base, RegOp index) {
			if(base != RBP) {
				throw std::runtime_error("only rbp as base is supported by now");
			}

			opcode(0xFF);
			modrm(0b01, 2 /* opcode extension */, 0b100 /* SIB */);
			sib(base, index, 8);
			byte(0);
		}

		void cmp(RegOp a, RegOp b, OperandSize size = QWORD)
		{
			prefixes(size, a, b);
			opcode(0x3B);
			operands(a, b);
		}

		// todo untested
		void cmp(RegOp a, RegMemOp b, OperandSize size = QWORD)
		{
			prefixes(size, a, b);
			opcode(0x3B);
			operands(a, b);
		}

		// todo untested
		void cmp(RegMemOp a, RegOp b, OperandSize size = QWORD)
		{
			prefixes(size, b, a);
			rex(true, isExtended(b), false, isExtended(RSP));
			opcode(0x39);
			operands(b, a);
		}

		// todo untested
		void lor(RegOp src, RegOp dst, OperandSize size = QWORD)
		{
			prefixes(size, src, dst);
			opcode(0x09);
			operands(src, dst);
		}

		// todo untested
		void lor(MemOp src, RegOp dst, OperandSize size = QWORD)
		{
			prefixes(size, dst, src);
			opcode(0x0B);
			operands(dst, src);
		}

		// todo untested
		void lor(RegOp withThis, i16 that)
		{
			rex(true, isExtended(withThis), false, false);
			opcode(0x81);
			modrm(0b11, 1, withThis & 0b111);
			dword(that);
		}

		// todo untested
		void land(RegOp src, RegOp dst, OperandSize size = QWORD)
		{
			prefixes(size, src, dst);
			opcode(0x21);
			operands(src, dst);
		}

		// todo untested
		void land(MemOp src, RegOp dst, OperandSize size = QWORD)
		{
			prefixes(size, dst, src);
			opcode(0x23);
			operands(dst, src);
		}

		// todo untested
		void land(RegOp withThis, i16 that)
		{
			rex(true, isExtended(withThis), false, false);
			opcode(0x81);
			modrm(0b11, 4, withThis & 0b111);
			dword(that);
		}

		void shr(RegOp reg, u8 count)
		{
			rex(true, isExtended(reg), false, false);
			opcode(0xC1); // 5 ib
			modrm(0b11, 5, reg & 0b111);
			byte(count);
		}

		void andimm(RegOp reg, u8 b)
		{
			rex(true, isExtended(reg), false, false);
			opcode(0x83);
			modrm(0b11, 4, reg);
			byte(b);
		}

		void set(internal::Comparison on, RegOp dst)
		{
			rex(false, false, false, isExtended(dst));
			dopcode((u16) on);
			modrm(0b11, 0, dst & 0b111);
		}

		void set(internal::Comparison on, i32 dst)
		{
			dopcode((u16) on);
			modrm(0b10, 0, RSP);
			sib64(RSP, 0, RSP, dst * 8);
		}

		void test(RegOp src)
		{
			prefixes(BYTE, src, src);
//			rex(false, false, false, isExtended(src));
			opcode(0xF6);
			modrm(0b11, 0, src & 0b111);
			byte(1);
		}

		void test(i32 dst)
		{
			// REX is not needed for this instruction
//			rex(false, false, false, isExtended(RSP));
			opcode(0xF6);
			modrm(0b10, 0, RSP);
			sib64(RSP, 0, RSP, dst * 8);
			byte(1);
		}

		void lahf()
		{
			opcode(0x9F);
		}

		void not_(RegOp dst)
		{
			rex(true, false, false, isExtended(dst));
			opcode(0xF7);
			modrm(0b11, 2, dst);
		}

		void not_(i32 dst)
		{
			// todo this is always a 64b operation and shouldn't be
			rex(true, false, false, false);
			opcode(0xF7);
			modrm(0b10, 0, 0b100);
			sib64(RSP, 0, RSP, dst * 8);
		}

		// todo this is always a 64b operation and shouldn't be
		void neg(RegOp dst, OperandSize size = QWORD)
		{
			prefixes(size, dst, dst);
			opcode(size == BYTE ? 0xF6 : 0xF7);
			modrm(0b11, 3, dst);
		}

		// todo this is always a 64b operation and shouldn't be
		void neg(i32 dst)
		{
			rex(true, false, false, isExtended(RSP));
			opcode(0xF7);
			modrm(0b10, 3, 0b100);
			sib64(RSP, 0, RSP, dst * 8);
		}

		void nop()
		{
			opcode(0x90);
		}

		void cqo()
		{
			rex(true, false, false, false);
			opcode(0x99);
		}
	};
}

#include <utility>

#include <catch2/catch.hpp>

#include <types.hpp>
#include <jit/CodeBuilder.hpp>

#include "StringMakerExtensions.hpp"

using namespace am2017s;
using namespace jit;

struct CodePiece
{
	CodePiece(std::vector<u8> data)
		: data(std::move(data))
	{}

	std::vector<u8> data;
};

static
bool operator == (CodePiece const& first, CodePiece const& second)
{
	return first.data == second.data;
}

static
std::string toHexString(u8 byte)
{
	static constexpr char const* const chars = "0123456789ABCDEF";

	std::string result(2, '?');
	result[0] = chars[byte >> 4];
	result[1] = chars[byte & 0xF];
	return result;
}

namespace Catch
{
	template <>
	struct StringMaker<CodePiece>
	{
		static
		std::string convert(CodePiece const& piece)
		{
			std::string result = "{";

			if(!piece.data.empty())
			{
				result += toHexString(piece.data[0]);

				if(piece.data.size() > 1)
				{
					for(auto it = ++piece.data.begin(); it != piece.data.end(); ++it)
					{
						result += ' ';
						result += toHexString(*it);
					}
				}
			}

			return result += "}";
		}
	};
}

static
CodePiece encodeStore(RegOp src, MemOp dst)
{
	CodeBuilder builder;
	builder.mov(src, dst, QWORD);

	auto code = builder.build();
	// remove ud2
	code.erase(code.end() - 2, code.end());

	return CodePiece(std::move(code));
}

TEST_CASE("CodeBuilder - memory operands", "[jit]")
{
	SECTION("mov [base], reg")
	{
		REQUIRE(encodeStore(RSP, MemOp(RAX)) == CodePiece({0x48, 0x89, 0x20}));
		REQUIRE(encodeStore(RSP, MemOp(R15)) == CodePiece({0x49, 0x89, 0x27}));
		REQUIRE(encodeStore(RSP, MemOp(RSP)) == CodePiece({0x48, 0x89, 0x24, 0x24}));
		REQUIRE(encodeStore(RSP, MemOp(RBP)) == CodePiece({0x48, 0x89, 0x65, 0x00}));

		REQUIRE(encodeStore(R15, MemOp(RAX)) == CodePiece({0x4c, 0x89, 0x38}));
		REQUIRE(encodeStore(R15, MemOp(R15)) == CodePiece({0x4d, 0x89, 0x3f}));
		REQUIRE(encodeStore(R15, MemOp(RSP)) == CodePiece({0x4c, 0x89, 0x3c, 0x24}));
		REQUIRE(encodeStore(R15, MemOp(RBP)) == CodePiece({0x4c, 0x89, 0x7d, 0x00}));
	}

	SECTION("mov [base + offset], reg")
	{
		REQUIRE(encodeStore(RSP, MemOp(RAX, 42)) == CodePiece({0x48, 0x89, 0x60, 0x2a}));
		REQUIRE(encodeStore(RSP, MemOp(R15, 42)) == CodePiece({0x49, 0x89, 0x67, 0x2a}));
		REQUIRE(encodeStore(RSP, MemOp(RSP, 42)) == CodePiece({0x48, 0x89, 0x64, 0x24, 0x2a}));

		REQUIRE(encodeStore(R15, MemOp(RAX, 42)) == CodePiece({0x4c, 0x89, 0x78, 0x2a}));
		REQUIRE(encodeStore(R15, MemOp(R15, 42)) == CodePiece({0x4d, 0x89, 0x7f, 0x2a}));
		REQUIRE(encodeStore(R15, MemOp(RSP, 42)) == CodePiece({0x4c, 0x89, 0x7c, 0x24, 0x2a}));

		REQUIRE(encodeStore(RSP, MemOp(RAX, 13371337)) == CodePiece({0x48, 0x89, 0xa0, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(RSP, MemOp(R15, 13371337)) == CodePiece({0x49, 0x89, 0xa7, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(RSP, MemOp(RSP, 13371337)) == CodePiece({0x48, 0x89, 0xa4, 0x24, 0xc9, 0x07, 0xcc, 0x00}));

		REQUIRE(encodeStore(R15, MemOp(RAX, 13371337)) == CodePiece({0x4c, 0x89, 0xb8, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(R15, MemOp(R15, 13371337)) == CodePiece({0x4d, 0x89, 0xbf, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(R15, MemOp(RSP, 13371337)) == CodePiece({0x4c, 0x89, 0xbc, 0x24, 0xc9, 0x07, 0xcc, 0x00}));
	}

	SECTION("mov [base + index * scale], reg")
	{
		REQUIRE(encodeStore(RSP, MemOp(RAX, RAX, 1)) == CodePiece({0x48, 0x89, 0x24, 0x00}));
		REQUIRE(encodeStore(RSP, MemOp(R15, RAX, 2)) == CodePiece({0x49, 0x89, 0x24, 0x47}));
		REQUIRE(encodeStore(RSP, MemOp(RBP, RAX, 8)) == CodePiece({0x48, 0x89, 0x64, 0xc5, 0x00}));
		REQUIRE(encodeStore(R15, MemOp(RAX, RAX, 1)) == CodePiece({0x4c, 0x89, 0x3c, 0x00}));
		REQUIRE(encodeStore(R15, MemOp(R15, RAX, 2)) == CodePiece({0x4d, 0x89, 0x3c, 0x47}));
		REQUIRE(encodeStore(R15, MemOp(RBP, RAX, 8)) == CodePiece({0x4c, 0x89, 0x7c, 0xc5, 0x00}));

		REQUIRE(encodeStore(RSP, MemOp(RAX, RBP, 1)) == CodePiece({0x48, 0x89, 0x24, 0x28}));
		REQUIRE(encodeStore(RSP, MemOp(R15, RBP, 2)) == CodePiece({0x49, 0x89, 0x24, 0x6f}));
		REQUIRE(encodeStore(RSP, MemOp(RBP, RBP, 8)) == CodePiece({0x48, 0x89, 0x64, 0xed, 0x00}));
		REQUIRE(encodeStore(R15, MemOp(RAX, RBP, 1)) == CodePiece({0x4c, 0x89, 0x3c, 0x28}));
		REQUIRE(encodeStore(R15, MemOp(R15, RBP, 2)) == CodePiece({0x4d, 0x89, 0x3c, 0x6f}));
		REQUIRE(encodeStore(R15, MemOp(RBP, RBP, 8)) == CodePiece({0x4c, 0x89, 0x7c, 0xed, 0x00}));

		REQUIRE(encodeStore(RSP, MemOp(RAX, R15, 1)) == CodePiece({0x4a, 0x89, 0x24, 0x38}));
		REQUIRE(encodeStore(RSP, MemOp(R15, R15, 2)) == CodePiece({0x4b, 0x89, 0x24, 0x7f}));
		REQUIRE(encodeStore(RSP, MemOp(RBP, R15, 8)) == CodePiece({0x4a, 0x89, 0x64, 0xfd, 0x00}));
		REQUIRE(encodeStore(R15, MemOp(RAX, R15, 1)) == CodePiece({0x4e, 0x89, 0x3c, 0x38}));
		REQUIRE(encodeStore(R15, MemOp(R15, R15, 2)) == CodePiece({0x4f, 0x89, 0x3c, 0x7f}));
		REQUIRE(encodeStore(R15, MemOp(RBP, R15, 8)) == CodePiece({0x4e, 0x89, 0x7c, 0xfd, 0x00}));
	}

	SECTION("mov [base + index * scale + offset], reg")
	{
		REQUIRE(encodeStore(RSP, MemOp(RAX, RAX, 1, 42)) == CodePiece({0x48, 0x89, 0x64, 0x00, 0x2a}));
		REQUIRE(encodeStore(RSP, MemOp(R15, RAX, 2, 42)) == CodePiece({0x49, 0x89, 0x64, 0x47, 0x2a}));
		REQUIRE(encodeStore(R15, MemOp(RAX, RAX, 4, 42)) == CodePiece({0x4c, 0x89, 0x7c, 0x80, 0x2a}));
		REQUIRE(encodeStore(R15, MemOp(R15, RAX, 8, 42)) == CodePiece({0x4d, 0x89, 0x7c, 0xc7, 0x2a}));
		REQUIRE(encodeStore(RSP, MemOp(RAX, RBP, 1, 42)) == CodePiece({0x48, 0x89, 0x64, 0x28, 0x2a}));
		REQUIRE(encodeStore(RSP, MemOp(R15, RBP, 2, 42)) == CodePiece({0x49, 0x89, 0x64, 0x6f, 0x2a}));
		REQUIRE(encodeStore(R15, MemOp(RAX, RBP, 4, 42)) == CodePiece({0x4c, 0x89, 0x7c, 0xa8, 0x2a}));
		REQUIRE(encodeStore(R15, MemOp(R15, RBP, 8, 42)) == CodePiece({0x4d, 0x89, 0x7c, 0xef, 0x2a}));
		REQUIRE(encodeStore(RSP, MemOp(RAX, R15, 1, 42)) == CodePiece({0x4a, 0x89, 0x64, 0x38, 0x2a}));
		REQUIRE(encodeStore(RSP, MemOp(R15, R15, 2, 42)) == CodePiece({0x4b, 0x89, 0x64, 0x7f, 0x2a}));
		REQUIRE(encodeStore(R15, MemOp(RAX, R15, 4, 42)) == CodePiece({0x4e, 0x89, 0x7c, 0xb8, 0x2a}));
		REQUIRE(encodeStore(R15, MemOp(R15, R15, 8, 42)) == CodePiece({0x4f, 0x89, 0x7c, 0xff, 0x2a}));

		REQUIRE(encodeStore(RSP, MemOp(RAX, RAX, 1, 13371337)) == CodePiece({0x48, 0x89, 0xa4, 0x00, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(RSP, MemOp(R15, RAX, 2, 13371337)) == CodePiece({0x49, 0x89, 0xa4, 0x47, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(R15, MemOp(RAX, RAX, 4, 13371337)) == CodePiece({0x4c, 0x89, 0xbc, 0x80, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(R15, MemOp(R15, RAX, 8, 13371337)) == CodePiece({0x4d, 0x89, 0xbc, 0xc7, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(RSP, MemOp(RAX, RBP, 1, 13371337)) == CodePiece({0x48, 0x89, 0xa4, 0x28, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(RSP, MemOp(R15, RBP, 2, 13371337)) == CodePiece({0x49, 0x89, 0xa4, 0x6f, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(R15, MemOp(RAX, RBP, 4, 13371337)) == CodePiece({0x4c, 0x89, 0xbc, 0xa8, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(R15, MemOp(R15, RBP, 8, 13371337)) == CodePiece({0x4d, 0x89, 0xbc, 0xef, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(RSP, MemOp(RAX, R15, 1, 13371337)) == CodePiece({0x4a, 0x89, 0xa4, 0x38, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(RSP, MemOp(R15, R15, 2, 13371337)) == CodePiece({0x4b, 0x89, 0xa4, 0x7f, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(R15, MemOp(RAX, R15, 4, 13371337)) == CodePiece({0x4e, 0x89, 0xbc, 0xb8, 0xc9, 0x07, 0xcc, 0x00}));
		REQUIRE(encodeStore(R15, MemOp(R15, R15, 8, 13371337)) == CodePiece({0x4f, 0x89, 0xbc, 0xff, 0xc9, 0x07, 0xcc, 0x00}));
	}
}

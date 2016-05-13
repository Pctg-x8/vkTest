#pragma once

#include <cstdio>
#include <tuple>
#include <memory>
#include <string>

namespace BinaryLoader
{
	using Data = std::pair<std::unique_ptr<unsigned char[]>, size_t>;

	auto load(const std::wstring& path)
	{
		FILE* fp;
		if (_wfopen_s(&fp, path.c_str(), L"rb") != 0) throw std::runtime_error("File not found");

		fseek(fp, 0, SEEK_END);
		auto size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		auto buf = std::make_unique<unsigned char[]>(size);
		fread(buf.get(), sizeof(unsigned char), size, fp);
		fclose(fp);
		return Data(std::move(buf), size);
	}
}

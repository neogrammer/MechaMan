#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "Rect.h"

namespace MechaMan
{
	// Reads files that ship inside the app package: compiled shaders, images and so on.
	// File names are relative to the package folder, for example L"Assets\\Player.png".
	namespace PackageFiles
	{
		// An image decoded into memory: 4 bytes per pixel (red, green, blue, opacity), top row
		// first, with the color bytes already multiplied by the opacity.
		struct Image
		{
			UINT					width = 0;
			UINT					height = 0;
			std::vector<uint8_t>	pixels;
		};

		// Where a packaged file is on disk.
		std::wstring GetPath(const std::wstring& fileName);

		bool Exists(const std::wstring& fileName);

		// The whole file as bytes. Throws if the file cannot be read.
		std::vector<uint8_t> ReadBytes(const std::wstring& fileName);

		// An image file (PNG, BMP, JPEG, GIF), decoded. Throws if it cannot be read or decoded.
		Image ReadImage(const std::wstring& fileName);
	}

	extern const RectF c_view;
}

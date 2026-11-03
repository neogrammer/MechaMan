#include "pch.h"
#include "SpriteSheet.h"

#include "PackageFiles.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

using namespace MechaMan;

namespace
{
	// The .cfg is UTF-8 text; package file names are wide.
	std::wstring Widen(const std::string& text)
	{
		if (text.empty())
		{
			return std::wstring();
		}
		const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
		std::wstring wide(static_cast<size_t>(length), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
		return wide;
	}

	std::string Narrow(const std::wstring& text)
	{
		if (text.empty())
		{
			return std::string();
		}
		const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
		std::string narrow(static_cast<size_t>(length), '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), length, nullptr, nullptr);
		return narrow;
	}

	// The folder part of a package-relative file name, separator included:
	// L"Assets\\" for L"Assets\\alucard.cfg".
	std::wstring Folder(const std::wstring& file)
	{
		const size_t slash = file.find_last_of(L"\\/");
		return slash == std::wstring::npos ? std::wstring() : file.substr(0, slash + 1);
	}

	// Folders of '.' and '..' worked out, so L"Assets\\cfg\\..\\Sprite sheets\\bat.png" becomes
	// L"Assets\\Sprite sheets\\bat.png". A '..' with nothing left to climb out of is dropped:
	// the package root is as high as a path can go.
	std::wstring Normalize(const std::wstring& path)
	{
		std::vector<std::wstring> parts;
		size_t start = 0;
		while (start <= path.size())
		{
			size_t end = path.find(L'\\', start);
			if (end == std::wstring::npos)
			{
				end = path.size();
			}

			const std::wstring part = path.substr(start, end - start);
			if (part == L"..")
			{
				if (!parts.empty())
				{
					parts.pop_back();
				}
			}
			else if (!part.empty() && part != L".")
			{
				parts.push_back(part);
			}
			start = end + 1;
		}

		std::wstring result;
		for (const std::wstring& part : parts)
		{
			result += result.empty() ? part : L"\\" + part;
		}
		return result;
	}

	// The file name without folder or extension: "alucard" for L"Assets\\alucard.cfg".
	std::string Stem(const std::wstring& file)
	{
		std::wstring name = file.substr(Folder(file).size());
		const size_t dot = name.find_last_of(L'.');
		if (dot != std::wstring::npos && dot > 0)
		{
			name.resize(dot);
		}
		return Narrow(name);
	}

	// Writes a problem to the debugger's Output window. A line number of 0 means the whole file.
	void Log(const std::wstring& configFile, int lineNumber, const std::string& what)
	{
		std::wstring message = L"SpriteSheet: " + configFile;
		if (lineNumber > 0)
		{
			message += L" line " + std::to_wstring(lineNumber);
		}
		message += L": " + Widen(what) + L"\n";
		OutputDebugStringW(message.c_str());
	}

	void TrimEnd(std::string& text)
	{
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
		{
			text.pop_back();
		}
	}
}

bool SpriteSheet::Load(const std::wstring& configFile)
{
	animations.clear();
	imageFile.clear();
	facesLeft = false;
	texture = nullptr;

	if (!PackageFiles::Exists(configFile))
	{
		Log(configFile, 0, "is not in the package. Is it in the project with Content set to Yes for All Configurations?");
		return false;
	}

	std::vector<uint8_t> bytes;
	try
	{
		bytes = PackageFiles::ReadBytes(configFile);
	}
	catch (Platform::Exception^)
	{
		Log(configFile, 0, "could not be read");
		return false;
	}
	std::istringstream in(std::string(bytes.begin(), bytes.end()));

	AnimationData* current = nullptr;	// the animation the following lines belong to
	int cellWidth = 0;
	int cellHeight = 0;

	std::string line;
	int lineNumber = 0;

	auto fail = [&](const char* what)
	{
		Log(configFile, lineNumber, what);
		return false;
	};

	while (std::getline(in, line))
	{
		++lineNumber;

		// A file saved as UTF-8 by some editors starts with a byte order mark.
		if (lineNumber == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
		{
			line.erase(0, 3);
		}

		std::istringstream words(line);
		std::string keyword;

		if (!(words >> keyword) || keyword[0] == '#')
		{
			continue;
		}

		if (keyword == "image")
		{
			std::string file;
			std::getline(words >> std::ws, file);
			TrimEnd(file);	// the file is read as bytes, so Windows line endings leave a '\r' here
			std::replace(file.begin(), file.end(), '/', '\\');
			imageFile = Normalize(Folder(configFile) + Widen(file));

			if (!PackageFiles::Exists(imageFile))
			{
				return fail("the image is not in the package. Is it in the project as content?");
			}
		}
		else if (keyword == "facing")
		{
			std::string side;
			words >> side;
			facesLeft = side == "left";
		}
		else if (keyword == "anim")
		{
			AnimationData data;
			int count = 0;
			int rowY = 0;

			if (!(words >> data.name >> count >> cellWidth >> cellHeight >> rowY))
			{
				return fail("expected anim <name> <frames> <w> <h> <rowY> [secondsPerFrame]");
			}

			words >> data.secondsPerFrame;	// optional

			// Also optional: the column of the first frame, so one row can hold more than one
			// animation (Alucard's dash is the last three frames of his leap).
			int first = 0;
			if (words >> first && first < 0)
			{
				return fail("the first frame can not be negative");
			}

			for (int i = 0; i < count; ++i)
			{
				data.frames.push_back(Rect{ (first + i) * cellWidth, rowY, cellWidth, cellHeight });
			}

			data.hits.assign(data.frames.size(), std::nullopt);
			data.hands.assign(data.frames.size(), std::nullopt);
			data.hurt = ToAnchorRelative(Rect{ 0, 0, cellWidth, cellHeight }, cellWidth, cellHeight);

			const std::string name = data.name;
			current = &animations[name];
			*current = std::move(data);
		}
		else if (!current)
		{
			return fail("this line has to come after an anim line");
		}
		else if (keyword == "loop")
		{
			current->loop = true;
		}
		else if (keyword == "once")
		{
			current->loop = false;
		}
		else if (keyword == "next")
		{
			if (!(words >> current->next))
			{
				return fail("expected next <animation>");
			}
		}
		else if (keyword == "hurt")
		{
			int x = 0, y = 0, w = 0, h = 0;

			if (!(words >> x >> y >> w >> h))
			{
				return fail("expected hurt <x> <y> <w> <h>");
			}

			current->hurt = ToAnchorRelative(Rect{ x, y, w, h }, cellWidth, cellHeight);
		}
		else if (keyword == "hit")
		{
			int frame = 0, x = 0, y = 0, w = 0, h = 0;

			if (!(words >> frame >> x >> y >> w >> h))
			{
				return fail("expected hit <frame> <x> <y> <w> <h>");
			}

			if (frame < 0 || frame >= static_cast<int>(current->hits.size()))
			{
				return fail("hit frame number is out of range for this animation");
			}

			current->hits[static_cast<size_t>(frame)] = ToAnchorRelative(Rect{ x, y, w, h }, cellWidth, cellHeight);
		}
		else if (keyword == "hand")
		{
			int frame = 0, x = 0, y = 0;
			float degrees = 0.0f;

			if (!(words >> frame >> x >> y >> degrees))
			{
				return fail("expected hand <frame> <x> <y> <angle> [strike]");
			}

			if (frame < 0 || frame >= static_cast<int>(current->hands.size()))
			{
				return fail("hand frame number is out of range for this animation");
			}

			std::string extra;
			HandPose pose;
			pose.at = { static_cast<float>(x - cellWidth / 2) + 0.5f, static_cast<float>(y - cellHeight) + 0.5f };	// the middle of the pixel
			pose.angle = DirectX::XMConvertToRadians(degrees);
			pose.strike = (words >> extra) && extra == "strike";
			current->hands[static_cast<size_t>(frame)] = pose;
		}
		else
		{
			Log(configFile, lineNumber, "unknown keyword '" + keyword + "' ignored");
		}
	}

	if (imageFile.empty())
	{
		Log(configFile, 0, "has no image line");
		return false;
	}

	if (animations.empty())
	{
		Log(configFile, 0, "has no animations");
		return false;
	}

	for (const auto& [name, data] : animations)
	{
		if (!data.next.empty() && !Has(data.next))
		{
			Log(configFile, 0, "animation '" + name + "' names next '" + data.next + "', which does not exist");
		}
	}

	return true;
}

void SpriteSheet::CreateDeviceDependentResources(SpriteRenderer& sprites)
{
	// GetTexture hands back the same texture to every sheet that uses the same image.
	if (!imageFile.empty())
	{
		texture = sprites.GetTexture(imageFile);
	}
}

const SpriteSheet* SpriteLibrary::Load(const std::wstring& configFile)
{
	const std::string key = Stem(configFile);
	SpriteSheet& sheet = m_sheets[key];

	if (!sheet.Load(configFile))
	{
		m_sheets.erase(key);
		return nullptr;
	}

	if (m_sprites != nullptr)
	{
		sheet.CreateDeviceDependentResources(*m_sprites);
	}

	return &sheet;
}

void SpriteLibrary::CreateDeviceDependentResources(SpriteRenderer& sprites)
{
	m_sprites = &sprites;
	for (auto& entry : m_sheets)
	{
		entry.second.CreateDeviceDependentResources(sprites);
	}
}

void SpriteLibrary::ReleaseDeviceDependentResources()
{
	m_sprites = nullptr;
	for (auto& entry : m_sheets)
	{
		entry.second.ReleaseDeviceDependentResources();
	}
}

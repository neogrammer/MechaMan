#include "pch.h"
#include "Stage.h"

#include "PackageFiles.h"

#include <sstream>

using namespace MechaMan;

namespace
{
	void Log(const std::wstring& file, int line, const std::string& message)
	{
		const std::wstring text = L"Stage: " + file + (line > 0 ? L" line " + std::to_wstring(line) : L"") + L": " +
			std::wstring(message.begin(), message.end()) + L"\n";
		OutputDebugStringW(text.c_str());
	}
}

bool Stage::Load(const std::wstring& file)
{
	if (!PackageFiles::Exists(file))
	{
		Log(file, 0, "the file is not in the package. Is it in the project as content?");
		return false;
	}

	std::vector<uint8_t> bytes;
	try
	{
		bytes = PackageFiles::ReadBytes(file);
	}
	catch (...)
	{
		Log(file, 0, "the file could not be read");
		return false;
	}

	*this = Stage();

	std::istringstream in(std::string(bytes.begin(), bytes.end()));
	std::string line;
	int number = 0;

	while (std::getline(in, line))
	{
		++number;

		const size_t hash = line.find('#');
		if (hash != std::string::npos)
		{
			line.resize(hash);
		}

		std::istringstream words(line);
		std::string keyword;
		if (!(words >> keyword))
		{
			continue;	// blank, or only a comment
		}

		bool ok = true;

		if (keyword == "size")
		{
			ok = static_cast<bool>(words >> size.x >> size.y);
		}
		else if (keyword == "checkpoint")
		{
			DirectX::XMFLOAT2 at;
			ok = static_cast<bool>(words >> at.x >> at.y);
			if (ok)
			{
				checkpoints.push_back(at);
			}
		}
		else if (keyword == "solid")
		{
			RectF box;
			ok = static_cast<bool>(words >> box.x >> box.y >> box.width >> box.height) && box.width > 0.0f && box.height > 0.0f;
			if (ok)
			{
				solids.push_back(box);
			}
		}
		else if (keyword == "ramp")
		{
			Ramp ramp;
			ok = static_cast<bool>(words >> ramp.x0 >> ramp.y0 >> ramp.x1 >> ramp.y1) && ramp.x1 > ramp.x0;
			if (ok)
			{
				ramps.push_back(ramp);
			}
		}
		else if (keyword == "enemy")
		{
			EnemySpawn spawn;
			ok = static_cast<bool>(words >> spawn.kind >> spawn.at.x >> spawn.at.y);
			if (ok)
			{
				enemies.push_back(spawn);
			}
		}
		else
		{
			Log(file, number, "unknown keyword '" + keyword + "'");
			return false;
		}

		if (!ok)
		{
			Log(file, number, "expected " + keyword + " followed by its numbers (see Stage.h)");
			return false;
		}
	}

	if (checkpoints.empty())
	{
		Log(file, 0, "has no checkpoint, so there is nowhere to start");
		return false;
	}

	return true;
}

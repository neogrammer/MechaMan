#include "pch.h"
#include "PackageFiles.h"

#include "..\Common\DirectXHelper.h"

#include <cstdio>
#include <wincodec.h>
#include "SpriteRenderer.h"

// The Windows image decoder (WIC).
#pragma comment(lib, "windowscodecs.lib")

using namespace MechaMan;
using namespace Microsoft::WRL;
namespace
{


	// Says which file was the problem in the debugger's Output window, then throws.
	[[noreturn]] void ThrowFileError(HRESULT hr, const wchar_t* problem, const std::wstring& path)
	{
		std::wstring message = L"PackageFiles: ";
		message += problem;
		message += L" ";
		message += path;
		message += L"\n";
		OutputDebugStringW(message.c_str());

		throw Platform::Exception::CreateException(FAILED(hr) ? hr : E_FAIL);
	}
}

std::wstring PackageFiles::GetPath(const std::wstring& fileName)
{
	std::wstring path = Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data();
	path += L"\\";
	path += fileName;
	return path;
}

bool PackageFiles::Exists(const std::wstring& fileName)
{
	WIN32_FILE_ATTRIBUTE_DATA attributes;
	if (!GetFileAttributesExW(GetPath(fileName).c_str(), GetFileExInfoStandard, &attributes))
	{
		return false;
	}
	return (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::vector<uint8_t> PackageFiles::ReadBytes(const std::wstring& fileName)
{
	std::wstring path = GetPath(fileName);

	FILE* file = nullptr;
	if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || file == nullptr)
	{
		ThrowFileError(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), L"cannot open", path);
	}

	std::vector<uint8_t> bytes;
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (size > 0)
	{
		bytes.resize(static_cast<size_t>(size));
		bytes.resize(fread(bytes.data(), 1, bytes.size(), file));
	}
	fclose(file);

	if (size <= 0 || bytes.size() != static_cast<size_t>(size))
	{
		ThrowFileError(E_FAIL, L"cannot read", path);
	}
	return bytes;
}

PackageFiles::Image PackageFiles::ReadImage(const std::wstring& fileName)
{
	std::wstring path = GetPath(fileName);

	ComPtr<IWICImagingFactory> factory;
	DX::ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));

	// Open the file and take its first (usually only) picture.
	ComPtr<IWICBitmapDecoder> decoder;
	HRESULT hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
	if (FAILED(hr))
	{
		ThrowFileError(hr, L"cannot open this as an image:", path);
	}

	ComPtr<IWICBitmapFrameDecode> frame;
	hr = decoder->GetFrame(0, &frame);

	// Whatever the file holds, convert it to 32 bits per pixel with the colors multiplied by
	// the opacity. Blue-green-red order is the one every Windows image decoder can convert to.
	ComPtr<IWICFormatConverter> converter;
	if (SUCCEEDED(hr))
	{
		hr = factory->CreateFormatConverter(&converter);
	}
	if (SUCCEEDED(hr))
	{
		hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
	}

	Image image;
	if (SUCCEEDED(hr))
	{
		hr = converter->GetSize(&image.width, &image.height);
	}
	if (SUCCEEDED(hr) && (image.width == 0 || image.height == 0 || image.width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || image.height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION))
	{
		// Bigger than a texture can be.
		hr = E_INVALIDARG;
	}
	if (SUCCEEDED(hr))
	{
		const UINT stride = image.width * 4;
		image.pixels.resize(static_cast<size_t>(stride) * image.height);
		hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(image.pixels.size()), image.pixels.data());
	}
	if (FAILED(hr))
	{
		ThrowFileError(hr, L"cannot decode the image", path);
	}

	// Textures are red-green-blue, so swap the red and blue of every pixel.
	for (size_t i = 0; i < image.pixels.size(); i += 4)
	{
		const uint8_t blue = image.pixels[i];
		image.pixels[i] = image.pixels[i + 2];
		image.pixels[i + 2] = blue;
	}

	return image;
}

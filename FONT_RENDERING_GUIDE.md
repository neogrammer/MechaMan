# DirectX 12 Font Rendering Guide - MechaMan

## Overview
Your project uses **DirectXTK 12** for font rendering with UWP (Universal Windows Platform). The infrastructure is already set up in `SplashState` to render text to your DirectX 12 command list.

## Key Components

### 1. **SpriteFont** (`DirectX::SpriteFont`)
- The main class for rendering text
- Loaded from a `.spritefont` binary file (pre-compiled sprite font)
- Located in: `fonts/myfile.spritefont`

### 2. **SpriteBatch** (`DirectX::SpriteBatch`)
- Groups rendering commands together
- Batches text drawing calls for better performance
- Requires a descriptor heap and render target state

### 3. **DescriptorHeap** (`DirectX::DescriptorHeap`)
- GPU resource descriptor management
- Holds descriptor handles for font textures
- Required by SpriteBatch and SpriteFont

## How It Works in Your Code

### Step 1: Load Fonts (Done in `SplashState::LoadFonts()`)

```cpp
// 1. Define descriptor slots
enum Descriptors {
	MyFont,
	Count  // Total number of descriptors
};

// 2. Create descriptor heap
auto resourceDescriptors = std::make_unique<DirectX::DescriptorHeap>(
	device,
	Descriptors::Count
);

// 3. Load font data from file
const std::vector<uint8_t> fontData = PackageFiles::ReadBytes(fontPath);

// 4. Create ResourceUploadBatch to upload font to GPU
DirectX::ResourceUploadBatch resourceUpload(device);
resourceUpload.Begin();

// 5. Create SpriteFont with descriptor handles
auto font = std::make_unique<DirectX::SpriteFont>(
	device,
	resourceUpload,
	fontData.data(),
	fontData.size(),
	resourceDescriptors->GetCpuHandle(Descriptors::MyFont),  // CPU handle
	resourceDescriptors->GetGpuHandle(Descriptors::MyFont)   // GPU handle
);

// 6. Wait for GPU upload to finish
resourceUpload.End(commandQueue).get();

// 7. Create SpriteBatch for rendering text
DirectX::ResourceUploadBatch spriteBatchUpload(device);
spriteBatchUpload.Begin();

auto spriteBatch = std::make_unique<DirectX::SpriteBatch>(
	device,
	spriteBatchUpload,
	pd  // PipelineStateDescription
);

spriteBatchUpload.End(commandQueue).get();
```

### Step 2: Render Text (Done in `SplashState::Render()`)

```cpp
void SplashState::Render(ID3D12GraphicsCommandList* commandList)
{
	// ... existing sprite rendering code ...

	// Check that all font resources are ready
	if (m_resourceDescriptors != nullptr && m_spriteBatch != nullptr && m_font != nullptr)
	{
		// Step 1: Set descriptor heaps on the command list
		ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap() };
		commandList->SetDescriptorHeaps(_countof(heaps), heaps);

		// Step 2: Begin sprite batch (starts rendering group)
		m_spriteBatch->Begin(commandList);

		// Step 3: Draw text
		m_font->DrawString(
			m_spriteBatch.get(),              // SpriteBatch pointer
			L"Hello, DirectX 12!",            // Wide string (Unicode text)
			DirectX::XMFLOAT2(10.0f, 10.0f),  // Screen position (pixels)
			DirectX::Colors::White            // Text color (XMVECTOR)
		);

		// Step 4: End sprite batch (flushes rendering)
		m_spriteBatch->End();
	}
}
```

## Common Text Colors

Use these from `DirectX::Colors` namespace:

```cpp
DirectX::Colors::White         // XMVECTOR(1, 1, 1, 1)
DirectX::Colors::Black         // XMVECTOR(0, 0, 0, 1)
DirectX::Colors::Red           // XMVECTOR(1, 0, 0, 1)
DirectX::Colors::Green         // XMVECTOR(0, 1, 0, 1)
DirectX::Colors::Blue          // XMVECTOR(0, 0, 1, 1)
DirectX::Colors::Yellow        // XMVECTOR(1, 1, 0, 1)
DirectX::Colors::Cyan          // XMVECTOR(0, 1, 1, 1)
DirectX::Colors::Magenta       // XMVECTOR(1, 0, 1, 1)

// Or create custom colors with XMVECTOR constructor
DirectX::XMVECTOR customColor = DirectX::XMVectorSet(0.5f, 0.7f, 0.2f, 1.0f);
```

## Advanced Text Rendering Options

### 1. Text Alignment
```cpp
// Right-aligned text
float textWidth = m_font->MeasureString(L"Hello");
m_font->DrawString(
	m_spriteBatch.get(),
	L"Hello",
	DirectX::XMFLOAT2(640.0f - textWidth, 100.0f),  // Position from right
	DirectX::Colors::White
);
```

### 2. Multiple Text Calls
```cpp
m_spriteBatch->Begin(commandList);

m_font->DrawString(
	m_spriteBatch.get(),
	L"Line 1: Score = 1000",
	DirectX::XMFLOAT2(10.0f, 10.0f),
	DirectX::Colors::White
);

m_font->DrawString(
	m_spriteBatch.get(),
	L"Line 2: Health = 50%",
	DirectX::XMFLOAT2(10.0f, 50.0f),
	DirectX::Colors::Yellow
);

m_font->DrawString(
	m_spriteBatch.get(),
	L"Line 3: Ammo = 120",
	DirectX::XMFLOAT2(10.0f, 90.0f),
	DirectX::Colors::Green
);

m_spriteBatch->End();
```

### 3. Text with Rotation and Scale
```cpp
// For rotated/scaled text, use SpriteBatch directly with advanced options
RECT sourceRect = {};  // Font texture source (usually empty for DrawString)

m_font->DrawString(
	m_spriteBatch.get(),
	L"Text",
	DirectX::XMFLOAT2(centerX, centerY),
	DirectX::Colors::White,
	0.0f,                           // Rotation (radians)
	DirectX::XMFLOAT2(0.0f, 0.0f),  // Origin point
	1.5f                            // Scale (1.0 = normal)
);
```

## Data Flow Diagram

```
1. Font File (myfile.spritefont)
		↓
2. ResourceUploadBatch uploads to GPU
		↓
3. SpriteFont object created with GPU descriptors
		↓
4. In Render():
   a. SetDescriptorHeaps() → bind descriptors
   b. spriteBatch->Begin() → start recording
   c. font->DrawString() → queue text
   d. spriteBatch->End() → flush to GPU
		↓
5. GPU renders text to back buffer
```

## Key Points to Remember

### ✅ DO:
- Load fonts in `CreateDeviceDependentResources()`
- Call `SetDescriptorHeaps()` before `spriteBatch->Begin()`
- Use wide strings (prefix with `L""`)
- Check for `nullptr` before rendering
- Call `spriteBatch->End()` to flush rendering

### ❌ DON'T:
- Try to render without a `SpriteBatch`
- Forget to set descriptor heaps
- Use narrow strings without conversion
- Render fonts during device lost state
- Leave descriptor heaps unset between frames

## Debugging Tips

### Font not appearing?
1. Check `fonts/myfile.spritefont` exists
2. Verify `PackageFiles::Exists(fontPath)` returns true
3. Ensure descriptor heap is properly initialized
4. Check that `spriteBatch->Begin(commandList)` is called before drawing
5. Verify the text position isn't off-screen

### To add debug text:
```cpp
// Add to Render() to debug
std::wstring statusText = L"FPS: " + std::to_wstring((int)fps);
m_font->DrawString(
	m_spriteBatch.get(),
	statusText.c_str(),
	DirectX::XMFLOAT2(10.0f, 720.0f - 30.0f),  // Bottom-left
	DirectX::Colors::White
);
```

## Memory Management

Font resources are properly cleaned up in `ReleaseDeviceDependentResources()`:

```cpp
void SplashState::ReleaseDeviceDependentResources()
{
	m_font.reset();              // SpriteFont destroyed
	m_spriteBatch.reset();       // SpriteBatch destroyed
	m_resourceDescriptors.reset(); // DescriptorHeap destroyed
}
```

This is called when the GPU device is lost or the app is suspended, ensuring proper resource cleanup.

## Creating Custom Fonts

To use a different font:

1. **Generate .spritefont file**:
   - Use DirectXTK's `MakeSpriteFont` tool
   - Input: TrueType font (.ttf) or bitmap
   - Output: Binary `.spritefont` file

2. **Place in `fonts/` folder**:
   ```
   fonts/
   ├── myfile.spritefont  (existing)
   └── newfont.spritefont (your custom font)
   ```

3. **Load in code**:
   ```cpp
   const std::wstring fontPath = L"fonts\\newfont.spritefont";
   ```

## Performance Considerations

- **Batch calls**: Multiple `DrawString()` calls within one `spriteBatch->Begin/End()` are efficiently batched
- **GPU upload**: Font texture is uploaded once during `CreateDeviceDependentResources()`
- **Memory**: Font data stays in GPU memory, only used during rendering
- **CPU cost**: Text measurement and rendering calculations are minimal

## References

- DirectXTK12 Documentation: https://github.com/microsoft/DirectXTK12
- SpriteFont API: `<SpriteFont.h>`
- SpriteBatch API: `<SpriteBatch.h>`
- DescriptorHeap: `<DescriptorHeap.h>`

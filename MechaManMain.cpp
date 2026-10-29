#include "pch.h"
#include "MechaManMain.h"
#include "Common\DirectXHelper.h"
#include "Content\SplashState.h"
#include "Content\SpriteDemoState.h"
#include "Content\CidSprite.h"
using namespace MechaMan;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

// The DirectX 12 Application template is documented at https://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x409

// Loads and initializes application assets when the application is loaded.
MechaManMain::MechaManMain()
{
	// The states the game starts in. The requests are queued here and the states are entered on the first Update.
	//m_states.Push(std::make_unique<GameplayState>(m_states));

	// first set the sprite renderer to CidSprite static variable 
	CidSprite::SetSpriteRenderer(&m_graphics.sprites);

	// The game starts on the splash screen. Pushed here, once: CreateRenderers runs again
	// after a device loss, and pushing there would stack a second splash screen.
	m_states.Push(std::make_unique<SplashState>(m_states, m_graphics, m_input));

	// A demonstration of the sprite renderer, drawn over the state above. Remove this line to take it out.
	

	// TODO: Change the timer settings if you want something other than the default variable timestep mode.
	// e.g. for 60 FPS fixed timestep update logic, call:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/
}

// Creates everything that needs the Direct3D device: the frame's command list, the sprite renderer and the states' GPU resources.
// Called at startup, and again after the device has been lost and re-created.
void MechaManMain::CreateRenderers(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	m_deviceResources = deviceResources;

	// The game is drawn in the shape of c_view (16:9, like an Xbox). In a window of any other
	// shape it keeps that shape, with black bars filling the rest. This comes before the
	// renderers, which read the viewport when they are created.
	deviceResources->SetAspectRatio(c_view.width / c_view.height);

	// CreateCommandList returns the list open; close it so Render() can Reset it.
	DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, deviceResources->GetCommandAllocator(), nullptr, IID_PPV_ARGS(&m_commandList)));
	NAME_D3D12_OBJECT(m_commandList);
	DX::ThrowIfFailed(m_commandList->Close());

	// The renderers come first: states load their textures through them.
	m_graphics.sprites.CreateDeviceDependentResources(deviceResources);
	m_graphics.text.CreateDeviceDependentResources(deviceResources, L"fonts\\myfile.spritefont");
	m_states.CreateDeviceDependentResources(deviceResources);
}

// Updates the application state once per frame.
void MechaManMain::Update()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// Update the game states.
	m_timer.Tick([&]()
	{
		// Read the keyboard and controllers first. This is inside Tick because, with a fixed
		// time step, Tick can run more than one update in a frame (or none), and each press
		// has to show up in exactly one update.
		m_input.Update();

		m_states.Update(m_timer);
	});
}

// Renders the current frame according to the current application state.
// Returns true if the frame was rendered and is ready to be displayed.
bool MechaManMain::Render()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// Don't try to render anything before the first Update, or while a state that would be drawn is still loading.
	if (m_timer.GetFrameCount() == 0 || !m_states.IsReadyToRender())
	{
		return false;
	}

	auto commandAllocator = m_deviceResources->GetCommandAllocator();
	DX::ThrowIfFailed(commandAllocator->Reset());

	// The command list can be reset anytime after ExecuteCommandList() is called.
	// No pipeline state is set here: each renderer sets its own before it draws.
	DX::ThrowIfFailed(m_commandList->Reset(commandAllocator, nullptr));

	// Begin the frame.
	{
		// Set the viewport and scissor rectangle: the game's part of the window, which is all
		// of it unless the window is a different shape from the game.
		D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
		D3D12_RECT scissorRect = {
			static_cast<LONG>(viewport.TopLeftX),
			static_cast<LONG>(viewport.TopLeftY),
			static_cast<LONG>(viewport.TopLeftX + viewport.Width),
			static_cast<LONG>(viewport.TopLeftY + viewport.Height) };
		m_commandList->RSSetViewports(1, &viewport);
		m_commandList->RSSetScissorRects(1, &scissorRect);

		// Indicate this resource will be in use as a render target.
		CD3DX12_RESOURCE_BARRIER renderTargetResourceBarrier =
			CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_commandList->ResourceBarrier(1, &renderTargetResourceBarrier);

		// Clear the back buffer and the depth buffer, then draw to them.
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView = m_deviceResources->GetRenderTargetView();
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = m_deviceResources->GetDepthStencilView();

		// Black everywhere first: whatever is outside the game's part stays black, as the bars.
		// Then the game's part gets the background color the states draw over.
		m_commandList->ClearRenderTargetView(renderTargetView, DirectX::Colors::Black, 0, nullptr);
		m_commandList->ClearRenderTargetView(renderTargetView, DirectX::Colors::CornflowerBlue, 1, &scissorRect);
		m_commandList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		m_commandList->OMSetRenderTargets(1, &renderTargetView, false, &depthStencilView);

		// Let the sprite renderer know a new frame has begun.
		m_graphics.sprites.BeginFrame();
	}

	// Render the game states.
	m_states.Render(m_commandList.Get());

	// End the frame.
	{
		// Indicate that the render target will now be used to present when the command list is done executing.
		CD3DX12_RESOURCE_BARRIER presentResourceBarrier =
			CD3DX12_RESOURCE_BARRIER::Transition(m_deviceResources->GetRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		m_commandList->ResourceBarrier(1, &presentResourceBarrier);

		DX::ThrowIfFailed(m_commandList->Close());

		// Execute the command list.
		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_deviceResources->GetCommandQueue()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	}

	return true;
}

// Updates application state when the window's size changes (e.g. device orientation change)
void MechaManMain::OnWindowSizeChanged()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	m_graphics.text.CreateWindowSizeDependentResources();
	m_states.CreateWindowSizeDependentResources();
}

// Notifies the app that it is being suspended.
void MechaManMain::OnSuspending()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// Process lifetime management may terminate suspended apps at any time, so it is
	// good practice to save any state that will allow the app to restart where it left off.

	m_states.OnSuspending();
	m_input.OnSuspending();

	// If your application uses video memory allocations that are easy to re-create,
	// consider releasing that memory to make it available to other applications.
}

// Notifes the app that it is no longer suspended.
void MechaManMain::OnResuming()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	m_input.OnResuming();
	m_states.OnResuming();
}

// Notifies renderers that device resources need to be released.
void MechaManMain::OnDeviceRemoved()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// The states keep their game data and drop everything that was created from the old device.
	// Nothing may hold on to the old device, or the new one cannot be created.
	m_states.ReleaseDeviceDependentResources();
	m_graphics.text.ReleaseDeviceDependentResources();
	m_graphics.sprites.ReleaseDeviceDependentResources();
	m_commandList = nullptr;
	m_deviceResources = nullptr;
}

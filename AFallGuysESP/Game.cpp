//
// Game.cpp
//
#include "pch.h"
#include "Game.h"
#include "thread"
#include "future"
#include "RemoteType.h"
#include "SDK.h"
#include "ctime"
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <vector>
#include <deque>
#include <chrono>
#include <set>
#include <unordered_set>
#include <iterator>
#include <future>
#include <windows.h>
#include <random>
#include <optional>
#include "Resource.h"
#include "const.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;
using namespace FG;
using namespace std;

std::string nameAppendDistance(std::string name, float distance)
{
	auto pad = [](string x,int padlen) 
	{
		int differ = std::max<int>(0, int(padlen - x.size()));
		int lefty = 0;
		string res = x;
		while (differ-- >= 0)
		{
			if (lefty++ % 2)
				res = res + " ";
			else
			{
				res = " " + res;
			}
		}
		return res;
	};
	string distanceM = to_string(int(distance)) + "m";
	int padlen = static_cast<int>(std::max(name.size(), distanceM.size()));
	return pad(name, padlen) + "\n" + pad(distanceM, padlen);
}

Game::Game() noexcept :
    m_window(nullptr),
    m_outputWidth(1),
    m_outputHeight(1),
    m_featureLevel(D3D_FEATURE_LEVEL_11_1)
{
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    m_window = window;
    m_outputWidth = std::max(width, 8);
    m_outputHeight = std::max(height, 8);

    CreateDevice();
    CreateResources();
}

bool Game::updateWindow()
{
	HWND victim = FindWindowA(0, FGWINDOW);
	if (victim)
	{
		RECT victimRect;
		RECT myRect;
		GetWindowRect(victim, &victimRect);
		GetWindowRect(m_window, &myRect);
		HWND aboveVictim = GetTopWindow(0);

		if (myRect != victimRect)
		{
			SetWindowPos(m_window, NULL, victimRect.left, victimRect.top,
				victimRect.right - victimRect.left, victimRect.bottom - victimRect.top, SWP_SHOWWINDOW);
		}

		if (aboveVictim != m_window)
		{
			SetWindowPos(m_window, HWND_TOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
		}
		ShowWindow(m_window, SW_SHOW);
		return true;
	}
	else
	{
		ShowWindow(m_window, SW_HIDE);
		return false;
	}
	return false;
}

// Executes the basic game loop.
void Game::Manager()
{
	deviceLock.lock();
	Clear();
	deviceLock.unlock();
	Present();
	ShowWindow(m_window, SW_SHOW);
	{
		while (1)
		{
			bool flag = false;
			try {
				getVictimHandle();
				flag = true;
			}
			catch (const std::invalid_argument&) {
				Sleep(5000);
				deviceLock.lock();
				Clear();
				deviceLock.unlock();
				Present();
			}
			if (flag)
			{
				break;
			}
		}
	}

	while (1)
	{
		auto updateStartTime = chrono::system_clock::now();
		deviceLock.lock();
		try
		{
			Clear();
			{
				std::tie(toRender_text, toRender_lines) = Update();
				// Get Objects
			}
			struct {
				bool operator()(screenSpaceString& a, screenSpaceString& b) const
				{
					return a.content < b.content;
				}
			} customSSS;
			std::sort(toRender_text.begin(), toRender_text.end(), customSSS);
		}
		catch (std::exception & e)
		{
			std::string what = e.what() + '\n';
			OutputDebugStringA(what.c_str());
			toRender_text.clear();
		}

		Render();
		deviceLock.unlock();

		Present();
		static unsigned long tick = 0;
		tick++;

		std::this_thread::sleep_for(2ms);

		if ((tick % 1427) == 0)
		{
			DWORD exitcode = 0;
			GetExitCodeProcess(getVictimHandle(), &exitcode);
			if (exitcode != STILL_ACTIVE)
			{
				ShowWindow(m_window, SW_HIDE);
				while (1)
				{
					bool flag = false;
					try {
						getVictimHandle(true);
						flag = true;
					}
					catch (const std::invalid_argument&) {
						Sleep(8000);
						deviceLock.lock();
						Clear();
						deviceLock.unlock();
						Present();
					}
					if (flag)
					{
						Sleep(10000);
						break;
					}
				}
				ShowWindow(m_window, SW_SHOW);
			}
		}

		if ((tick % 2) == 0)
		{
			updateWindow();
		}
	}
}
// Updates the world.

struct GameObjectRef
{
	GameObjectRef(GameObject* _target, size_t _lifeCycle) :target(_target), lifeCycle(_lifeCycle) {};
	GameObject* target;
	mutable size_t lifeCycle;
};
class GameObjectRefComparator
{
public: using is_transparent = std::true_type;

public: bool operator()(const GameObject* left, const GameObjectRef& right) const
{
	return left < right.target;
}

public: bool operator()(const GameObjectRef& left, const GameObject* right) const
{
	return left.target < right;
}

public: bool operator()(const GameObjectRef& left, const GameObjectRef& right) const
{
	return left.target < right.target;
}
};
std::set<GameObjectRef, GameObjectRefComparator> blackListedGameObjects;

std::tuple<
std::vector<screenSpaceString>,
std::vector<screenSpaceLine>
> Game::Update()
{
	try
	{
		std::tuple<
			std::vector<screenSpaceString>,
			std::vector<screenSpaceLine>
		> res;

		for (auto it = blackListedGameObjects.begin(); it != blackListedGameObjects.end();)
		{
			if (it->lifeCycle-- <= 0)
			{
				it = blackListedGameObjects.erase(it);
			}
			else
			{
				it++;
			}
		}

		auto mgr = GetGameObjectManager();

		auto camera = mgr->GetObjectByName("Main Camera Brain", true);
		auto camerapos = camera->GetTransform().GetPosition();
		if (!camera.address())
			return res;

		auto CameraComponent = camera->GetBasicComponent("Camera").cast<Camera>();

		struct NotableGameObject {
			GameObject*				ptr;
			GameObject				local;
			std::string				name;
		};

		std::vector<NotableGameObject> gameObjects;
		auto CacheRemoteGameObjectsCallback = [&](RemoteType<GameObject> obj) {
			auto ptr = obj.address();
			auto cit = blackListedGameObjects.find({ ptr ,0 });
			if (cit == blackListedGameObjects.end())
			{
				auto local = obj.get();
				if (local.active > 0)
				{
					auto name = local.GetName();
					gameObjects.push_back({ obj.address(), local, name });
				}
			}
			return true;
		};
		auto ESPTransform = [&](Transform& transform, std::string name) {
			auto pos = transform.GetPosition();
			auto sspos = CameraComponent->ToScreen(pos);
			auto distance = FVector(pos - camerapos).Dot(pos - camerapos);
			if (distance > 10000)
				return;

			std::get<0>(res).push_back({
				sspos.x, sspos.y,  1000.0f / distance,
				Colors::YellowGreen,
				name,
				0
				});
		};
		auto ESPGameObject = [&](GameObject& obj, std::string name) {
			auto transform = obj.GetTransform();
			ESPTransform(transform, name);
		};
		auto DieGameObject = [&](GameObject* ptr){
			blackListedGameObjects.emplace(ptr, rand() % 50 + 140);
		};
		auto isNotableTransform = [&](Transform& transform)
		{
			if (!transform.transformAccess.pTransformData.address())
			{
				return false;
			}
			auto rootname = transform.GetRoot().parent->GetName();
			std::transform(rootname.begin(), rootname.end(), rootname.begin(), std::tolower);
			if (rootname.find("fallguy [") == 0)
			{
				return false;
			}
			if (rootname.find("camera") != string::npos || rootname.find("systems/managers") != string::npos
				|| rootname.find("background") != string::npos || rootname.find("sfx") != string::npos
				|| rootname.find("light") != string::npos || rootname.find("navigation") != string::npos
				|| rootname.find("manager") != string::npos
				|| rootname.find("vfx") != string::npos)
			{
				return false;
			}
			return true;
		};
		auto isNotableGameObject = [&](std::string name)
		{
			if (name.find("VFX_") != string::npos || name.find("be in here") != string::npos
				|| name.find("for example") != string::npos)
			{
				return false;;
			}
			return true;
		};
		mgr->EnumObjects(true, CacheRemoteGameObjectsCallback);
		mgr->EnumObjects(false, CacheRemoteGameObjectsCallback);

		for (auto obj : gameObjects)
		{
			if (obj.name.find("FallGuy [") == 0)
			{
				auto controller = obj.local.GetComponent("::FallGuysCharacterController");
				if (!controller.address())
				{
					DieGameObject(obj.ptr);
					continue;
				}
				auto il2CppController = controller->il2CppObj;
				static int isLocalOffset = il2CppController->klass->GetMemberOffset("<IsLocalPlayer>k__BackingField");
				bool isLocal = RemoteType<bool>((size_t)il2CppController.address() + isLocalOffset);
				std::string name = isLocal ? "Me" : "Pawn";
				ESPGameObject(obj.local, name);
				continue;
			}
			auto transform = obj.local.GetTransform();
			if (!isNotableTransform(transform) || !isNotableGameObject(obj.name))
			{
				DieGameObject(obj.ptr);
				continue;
			}
			auto controller = obj.local.GetComponent("::FakeDoorController");
			if (controller.address())
			{
				auto rootname = transform.GetRoot().parent->GetName();
				auto il2CppController = controller->il2CppObj;
				auto il2CppControllerKlass = il2CppController->klass;
				static int _fakeDoorOffset = il2CppControllerKlass->GetMemberOffset("_fakeDoor");
				static int _doorPositionOffset = il2CppControllerKlass->GetMemberOffset("_doorPosition");
				auto fake = *RemoteType<Il2CppGameObject*>((size_t)il2CppController.address() + _fakeDoorOffset)->obj;
				if (!fake.active)
				{
					Transform _transf = *RemoteType<Il2CppWrap<Transform>*>((size_t)il2CppController.address() + _doorPositionOffset)->obj;
					ESPTransform(_transf, "Real");
				}
				continue;
			}
			controller = obj.local.GetComponent("::MatchFallTile");
			if (controller.address())
			{
				auto rootname = transform.GetRoot().parent->GetName();
				OutputDebugStringA((rootname + ": " + obj.name + "\n").c_str());
				auto il2CppController = controller->il2CppObj;
				Il2CppGameObject tile = *RemoteType<Il2CppGameObject*>((size_t)il2CppController.address() + 0x50);
				GameObject _tile = tile.obj;
				ESPGameObject(_tile, _tile.GetName());
				continue;
			}
			controller = obj.local.GetComponent("::MusicalSquaresTile");
			if (controller.address())
			{
				auto rootname = transform.GetRoot().parent->GetName();
				OutputDebugStringA((rootname + ": " + obj.name + "\n").c_str());
				auto il2CppController = controller->il2CppObj;
				static int _isRealOffset = il2CppController->klass->GetMemberOffset("_isReal");
				bool isReal = RemoteType<bool>((size_t)il2CppController.address() + _isRealOffset);
				if (isReal)
				{
					ESPTransform(transform, obj.name);
				}
				continue;
			}
			//ESPGameObject(obj.local, obj.name);
			DieGameObject(obj.ptr);
			continue;
		}

		return res;
	}
	catch (...)
	{

	}
	return {};
}

// Draws the scene.
void Game::Render()
{
	RECT desktop;
	const HWND hDesktop = GetDesktopWindow();
	GetWindowRect(hDesktop, &desktop);

	float resizeFactor = 1.0f;

	{ // DrawTexts
		m_spriteBatch->Begin(SpriteSortMode_Deferred, m_states->NonPremultiplied());
		static deque<size_t> fpsQueue;

		auto sigmod = [](float x)->float
		{
			return 0.5f + 1.0f / (1.0f + std::powf(2.718f, (-x * 6.0f + 3.0f))) * 1.0f;
		};

		Vector2 origin;
		std::wstring output;
		std::string lastStr = "";

		auto stripNonAscii = [](string& str) {
			str.erase(remove_if(str.begin(), str.end(), [](char c) {
				return !((c == 0x20)|| c=='\n' || (c >= 0x30 && c <= 0x39) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
				}), str.end());
		};

		for (auto sss : toRender_text)
		{
			stripNonAscii(sss.content);
			if (lastStr != sss.content.c_str())
			{
				output = m2wconventer.from_bytes(sss.content.c_str());
				origin = m_font->MeasureString(output.c_str()) / 2.f;
			}

			sss.x = (sss.x - 0.5f) * resizeFactor + 0.5f;
			sss.y = (sss.y - 0.5f) * resizeFactor + 0.5f;

			if (!(sss.x > 0 && sss.x < 1 && sss.y > 0 && sss.y < 1))
			{
				if (!sss.important)
					continue;
				sss.x = fmaxf(fminf(0.985f, sss.x), 0.015f);
				sss.y = fmaxf(fminf(0.985f, sss.y), 0.015f);
			}
			m_font->DrawString(m_spriteBatch.get(), output.c_str(),
				{ sss.x * m_outputWidth , sss.y * m_outputHeight }, sss.col, 0.f, origin,
				0.5f * sigmod( sss.scale) );
		}

		auto ct = chrono::duration_cast<chrono::milliseconds>(
			chrono::system_clock::now().time_since_epoch()
			).count();
		fpsQueue.push_back(ct);
		if (fpsQueue.size() > 30)
		{
			auto st = fpsQueue.front();
			wchar_t fpsStr[8];
			swprintf_s(fpsStr, L"%.1f", (30.0f / float(ct - st) * 1000.0f));
			fpsQueue.pop_front();

			m_font->DrawString(m_spriteBatch.get(), fpsStr,
				Vector2{ 3.f,3.f }, Colors::YellowGreen, 0.f, { 0.f,0.f }, 0.5);
		}

		m_spriteBatch->End();
	}

	{ // matchstick
		m_d3dContext->OMSetBlendState(m_states->NonPremultiplied(), nullptr, 0xFFFFFFFF);
		m_d3dContext->OMSetDepthStencilState(m_states->DepthNone(), 0);
		m_d3dContext->RSSetState(m_states->CullNone());

		m_effect->Apply(m_d3dContext.Get());
		m_d3dContext->IASetInputLayout(m_inputLayout.Get());
		m_lineBatch->Begin();
		for (auto& line : toRender_lines)
		{
			float x1 = line.pos.m128_f32[0];
			float y1 = line.pos.m128_f32[1];
			float x2 = line.pos.m128_f32[2];
			float y2 = line.pos.m128_f32[3];

			y1 = ((y1 - 0.5f) / float(m_outputHeight) *
				m_outputWidth) + 0.5f;
			x1 = (x1 - 0.5f) * resizeFactor + 0.5f;
			y1 = (y1 - 0.5f) * resizeFactor + 0.5f;

			y2 = ((y2 - 0.5f) / float(m_outputHeight) *
				m_outputWidth) + 0.5f;
			x2 = (x2 - 0.5f) * resizeFactor + 0.5f;
			y2 = (y2 - 0.5f) * resizeFactor + 0.5f;

			VertexPositionColor v1(Vector3(x1 * m_outputWidth, y1 * m_outputHeight, 0.f), line.col);
			VertexPositionColor v2(Vector3(x2 * m_outputWidth, y2 * m_outputHeight, 1.f), line.col);
			m_lineBatch->DrawLine(v1, v2);
		}
		m_lineBatch->End();
	}
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    // Clear the views.
	float clearColor[] = { 0.f, 0.f, 0.f, 0.0f };
	m_d3dContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
    m_d3dContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    m_d3dContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

    // Set the viewport.
    CD3D11_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(m_outputWidth), static_cast<float>(m_outputHeight));
    m_d3dContext->RSSetViewports(1, &viewport);
}

// Presents the back buffer contents to the screen.
void Game::Present()
{
	HRESULT hr = m_swapChain->Present(0, 0);
	DX::ThrowIfFailed(m_visual->SetContent(m_swapChain.Get()));
	DX::ThrowIfFailed(m_dcompDevice->Commit());
}

// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
}

void Game::OnResuming()
{

}

void Game::OnWindowSizeChanged(int width, int height)
{
	if (width != m_outputWidth || height != m_outputHeight)
	{
		m_outputWidth = std::max(width, 8);
		m_outputHeight = std::max(height, 8);

		CreateResources();
	}
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const
{
    width = 64;
    height = 64;
}

void Game::exit()
{
	deviceLock.lock();
	PostQuitMessage(0);
}

// These are the resources that depend on the device.
void Game::CreateDevice()
{
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    static const D3D_FEATURE_LEVEL featureLevels [] =
    {
        // TODO: Modify for supported Direct3D feature levels
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };

    // Create the DX11 API device object, and get a corresponding context.
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    DX::ThrowIfFailed(D3D11CreateDevice(
        nullptr,                            // specify nullptr to use the default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        featureLevels,
        _countof(featureLevels),
        D3D11_SDK_VERSION,
        device.ReleaseAndGetAddressOf(),    // returns the Direct3D device created
        &m_featureLevel,                    // returns feature level of device created
        context.ReleaseAndGetAddressOf()    // returns the device immediate context
        ));
#ifndef NDEBUG
    ComPtr<ID3D11Debug> d3dDebug;
    if (SUCCEEDED(device.As(&d3dDebug)))
    {
        ComPtr<ID3D11InfoQueue> d3dInfoQueue;
        if (SUCCEEDED(d3dDebug.As(&d3dInfoQueue)))
        {
#ifdef _DEBUG
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
#endif
            D3D11_MESSAGE_ID hide [] =
            {
                D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
                // TODO: Add more message IDs here as needed.
            };
            D3D11_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            d3dInfoQueue->AddStorageFilterEntries(&filter);
        }
    }
#endif

    DX::ThrowIfFailed(device.As(&m_d3dDevice));
    DX::ThrowIfFailed(context.As(&m_d3dContext));


	HRSRC spriteFONTRC = FindResource(NULL, MAKEINTRESOURCE(IDR_SPRITEFONT1), L"SPRITEFONT");
	HGLOBAL hSpriteFONT = LoadResource(NULL, spriteFONTRC);
	void* spriteFONTData = LockResource(hSpriteFONT);
	unsigned int spriteFONTSize = SizeofResource(NULL, spriteFONTRC);
	const uint8_t* mspriteFONTData = (uint8_t*)malloc(spriteFONTSize);
	memcpy((void*)mspriteFONTData, spriteFONTData, spriteFONTSize);
	UnlockResource(hSpriteFONT);

	m_spriteBatch = std::make_unique<SpriteBatch>(m_d3dContext.Get());
	m_states = std::make_unique<CommonStates>(m_d3dDevice.Get());

	void const* shaderByteCode;
	size_t byteCodeLength;

	m_effect = std::make_unique<BasicEffect>(m_d3dDevice.Get());
	m_effect->SetVertexColorEnabled(true);
	m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

	DX::ThrowIfFailed(
		m_d3dDevice->CreateInputLayout(DirectX::VertexPositionColor::InputElements,
			DirectX::VertexPositionColor::InputElementCount,
			shaderByteCode, byteCodeLength,
			m_inputLayout.ReleaseAndGetAddressOf()));

	m_lineBatch = std::make_unique<PrimitiveBatch<DirectX::VertexPositionColor>>(m_d3dContext.Get());

	m_font = std::make_unique<SpriteFont>(m_d3dDevice.Get(),
		mspriteFONTData, spriteFONTSize, false);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateResources()
{
	std::lock_guard<std::mutex> lock(deviceLock);
    // Clear the previous window size specific context.
    ID3D11RenderTargetView* nullViews [] = { nullptr };
    m_d3dContext->OMSetRenderTargets(_countof(nullViews), nullViews, nullptr);
    m_renderTargetView.Reset();
    m_depthStencilView.Reset();
    m_d3dContext->Flush();

    UINT backBufferWidth = static_cast<UINT>(m_outputWidth);
    UINT backBufferHeight = static_cast<UINT>(m_outputHeight);
    DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    UINT backBufferCount = 2;

    // If the swap chain already exists, resize it, otherwise create one.
    if (m_swapChain)
    {
		m_d3dContext->ClearState();

        HRESULT hr = m_swapChain->ResizeBuffers(backBufferCount, backBufferWidth, backBufferHeight, backBufferFormat, 0);

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            // If the device was removed for any reason, a new device and swap chain will need to be created.
            OnDeviceLost();

            // Everything is set up now. Do not continue execution of this method. OnDeviceLost will reenter this method 
            // and correctly set up the new device.
            return;
        }
        else
        {
            DX::ThrowIfFailed(hr);
        }
    }
    else
    {
        // First, retrieve the underlying DXGI Device from the D3D Device.
		ComPtr<IDXGIDevice> dxgiDevice;
		DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

        // Identify the physical adapter (GPU or card) this device is running on.
        ComPtr<IDXGIAdapter> dxgiAdapter;
        DX::ThrowIfFailed(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()));

        // And obtain the factory object that created it.
		ComPtr<IDXGIFactory2> dxFactory;
		DX::ThrowIfFailed(CreateDXGIFactory2(
			0,//DXGI_CREATE_FACTORY_DEBUG,
			__uuidof(dxFactory),
			reinterpret_cast<void **>(dxFactory.GetAddressOf())));

        // Create a descriptor for the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = backBufferFormat;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		swapChainDesc.BufferCount = backBufferCount;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
			//| DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;

		DX::ThrowIfFailed(dxFactory->CreateSwapChainForComposition(dxgiDevice.Get(),
			&swapChainDesc,
			nullptr, // Don¡¯t restrict
			m_swapChain.GetAddressOf()));

		DX::ThrowIfFailed(DCompositionCreateDevice(
			dxgiDevice.Get(),
			__uuidof(m_dcompDevice),
			reinterpret_cast<void **>(m_dcompDevice.GetAddressOf())));

		DX::ThrowIfFailed(m_dcompDevice->CreateTargetForHwnd(m_window,
			true, // Top most
			m_compTarget.GetAddressOf()));

		DX::ThrowIfFailed(m_dcompDevice->CreateVisual(m_visual.GetAddressOf()));
		DX::ThrowIfFailed(m_compTarget->SetRoot(m_visual.Get()));

		/*DX::ThrowIfFailed(
			dxFactory->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER));*/
    }

    // Obtain the backbuffer for this window which will be the final 3D rendertarget.
    ComPtr<ID3D11Texture2D> backBuffer;
    DX::ThrowIfFailed(m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())));

    // Create a view interface on the rendertarget to use on bind.
    DX::ThrowIfFailed(m_d3dDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.ReleaseAndGetAddressOf()));

    // Allocate a 2-D surface as the depth/stencil buffer and
    // create a DepthStencil view on this surface to use on bind.
    CD3D11_TEXTURE2D_DESC depthStencilDesc(depthBufferFormat, backBufferWidth, backBufferHeight, 1, 1, D3D11_BIND_DEPTH_STENCIL);

    ComPtr<ID3D11Texture2D> depthStencil;
    DX::ThrowIfFailed(m_d3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, depthStencil.GetAddressOf()));

    CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
    DX::ThrowIfFailed(m_d3dDevice->CreateDepthStencilView(depthStencil.Get(), &depthStencilViewDesc, m_depthStencilView.ReleaseAndGetAddressOf()));

	Matrix proj = Matrix::CreateScale(2.f / float(backBufferWidth),
		-2.f / float(backBufferHeight), 1.f)
		* Matrix::CreateTranslation(-1.f, 1.f, 0.f);
	m_effect->SetProjection(proj);
}

void Game::OnDeviceLost()
{
	m_states.reset();

	m_font.reset();
	m_spriteBatch.reset();

	m_visual.Reset();
	m_compTarget.Reset();
	m_dcompDevice.Reset();
    m_depthStencilView.Reset();
    m_renderTargetView.Reset();
    m_swapChain.Reset();
    m_d3dContext.Reset();
	m_d3dDevice.Reset();

    CreateDevice();
    CreateResources();
}


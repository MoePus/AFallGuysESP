#pragma once
#include <iostream>
#include <mutex>
#include <vector>
#include <future>
#include <DirectXMath.h>

struct screenSpaceString {
	float x;
	float y;
	float scale;
	DirectX::XMVECTOR col;
	std::string content;
	bool important;
};

struct worldSpaceString {
	__m128 pos;
	DirectX::XMVECTOR col;
	std::string content;
};

struct screenSpaceLine {
	__m128 pos;
	DirectX::XMVECTOR col;
};
class Game
{
public:
    Game() noexcept;

    // Initialization and management
    void Initialize(HWND window, int width, int height);

	bool updateWindow();
    // Basic game loop
    void Manager();

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowSizeChanged(int width, int height);

    // Properties
    void GetDefaultSize( int& width, int& height ) const;
	void exit();
	SIZE getSize()
	{
		return { m_outputWidth ,m_outputHeight };
	}
private:
    // Device resources.
    HWND                                            m_window;
    int                                             m_outputWidth;
    int                                             m_outputHeight;

    D3D_FEATURE_LEVEL                               m_featureLevel;
    Microsoft::WRL::ComPtr<ID3D11Device1>           m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1>    m_d3dContext;

    Microsoft::WRL::ComPtr<IDXGISwapChain1>         m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  m_depthStencilView;
	Microsoft::WRL::ComPtr<IDCompositionDevice>		m_dcompDevice;
	Microsoft::WRL::ComPtr<IDCompositionTarget>		m_compTarget;
	Microsoft::WRL::ComPtr<IDCompositionVisual>		m_visual;

	std::unique_ptr<DirectX::SpriteFont> m_font;
	std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch;
	std::unique_ptr<DirectX::BasicEffect> m_effect;
	std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_lineBatch;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
	std::unique_ptr<DirectX::CommonStates> m_states;

	std::mutex deviceLock;
	std::vector<screenSpaceString> toRender_text;
	std::vector<screenSpaceLine> toRender_lines;
	std::wstring_convert<std::codecvt_utf8<wchar_t>> m2wconventer;

	std::tuple<
		std::vector<screenSpaceString>,
		std::vector<screenSpaceLine>
	> Update();

	bool gameAlive;
	void Render();

	void Clear();
	void Present();

	void CreateDevice();
	void CreateResources();

	void OnDeviceLost();
};

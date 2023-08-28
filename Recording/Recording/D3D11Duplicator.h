#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>
#include <atlbase.h>

class D3D11Duplicator
{
public:
	D3D11Duplicator();

	virtual ~D3D11Duplicator();

	virtual int Init(int monitor_index);

	virtual int Init(int monitor_index, ID3D11Device* device);

	virtual void Release();

	virtual void* GetDuplicate();

	virtual ID3D11Device* GetDevice() { return m_device; };

protected:
	int m_monitorIndex;

	CComPtr<ID3D11Device>		m_device;
	CComPtr<ID3D11DeviceContext> m_context;

	CComPtr<IDXGIDevice> m_dxgidevice;
	CComPtr<IDXGIAdapter> m_dxgiadapter;
	CComPtr<IDXGIOutputDuplication> m_dxgiduplicator;
	CComPtr<ID3D11Texture2D> m_texture;
	int m_textureWidth = 0;
	int m_textureHeight = 0;
	int m_textureFormat = 0;

	D3D_FEATURE_LEVEL m_featureLevel;
};
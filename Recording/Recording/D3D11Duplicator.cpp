#include "D3D11Duplicator.h"
#include <iostream>

D3D11Duplicator::D3D11Duplicator()
{

}

D3D11Duplicator::~D3D11Duplicator()
{
	Release();
}

int D3D11Duplicator::Init(int monitor_index)
{
	Release();

	HRESULT hr;

	static D3D_FEATURE_LEVEL D3D11_features[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};

	CComPtr<ID3D11Device> dev;
	hr = D3D11CreateDevice(NULL
		, D3D_DRIVER_TYPE_HARDWARE
		, NULL
		, 0
		, D3D11_features
		, sizeof(D3D11_features)/sizeof(D3D11_features[0])
		, D3D11_SDK_VERSION
		, &dev
		, &m_featureLevel
		, &m_context);
	if (FAILED(hr)) {
		return FALSE;
	}

	return Init(monitor_index, dev);
}


int D3D11Duplicator::Init(int monitor_index, ID3D11Device* device)
{
	if (m_device)
		Release();

	m_device = device;
	m_monitorIndex = monitor_index;

	HRESULT hr;
	hr = m_device.QueryInterface(&m_dxgidevice);
	if (FAILED(hr))
		return -1;

	hr = m_dxgidevice->GetAdapter(&m_dxgiadapter);
	if (FAILED(hr))
		return -1;

	CComPtr<IDXGIOutput> dxgioutput;
	hr = m_dxgiadapter->EnumOutputs(monitor_index, &dxgioutput);
	if (FAILED(hr)) {
		if (hr == DXGI_ERROR_NOT_FOUND)
			return 0;

		printf("IDXGIAdapter1 failed to get output(0x%x)\n", hr);
		return -1;
	}

	CComPtr<IDXGIOutput1> dxgioutput1;
	hr = dxgioutput->QueryInterface(&dxgioutput1);
	if (FAILED(hr))
		return -1;

	hr = dxgioutput1->DuplicateOutput(m_device, &m_dxgiduplicator);
	if (FAILED(hr))
	{
		printf("Failed to DuplicateOutput(0x%x)\n", hr);
		return -1;
	}

	return 0;
}

void D3D11Duplicator::Release()
{
	m_texture.Release();

	m_dxgiadapter.Release();
	m_dxgidevice.Release();
	m_context.Release();
	m_device.Release();
}

void* D3D11Duplicator::GetDuplicate()
{
	if (!m_dxgiduplicator)
		return NULL;

	HRESULT hr;

	DXGI_OUTDUPL_FRAME_INFO info;
	CComPtr<IDXGIResource> dxgires;
	hr = m_dxgiduplicator->AcquireNextFrame(0, &info, &dxgires);
	if (hr == DXGI_ERROR_ACCESS_LOST)
		return NULL;

	if (hr == DXGI_ERROR_WAIT_TIMEOUT)
		return NULL;


	if (FAILED(hr)) {
		printf("IDXGIOutputDuplication AcquireNextFrame failed(0x%x)\n", hr);
		return NULL;
	}

	CComPtr<ID3D11Texture2D> texure;
	hr = dxgires->QueryInterface(&texure);
	if (FAILED(hr)) {
		m_dxgiduplicator->ReleaseFrame();
		return NULL;
	}

	D3D11_TEXTURE2D_DESC desc;
	texure->GetDesc(&desc);

	if (!m_texture ||
		m_textureWidth != desc.Width ||
		m_textureHeight != desc.Height ||
		m_textureFormat != desc.Format)
	{
		D3D11_TEXTURE2D_DESC texDesc = { 0 };
		texDesc.Width = desc.Width;
		texDesc.Height = desc.Height;
		texDesc.MipLevels = 1;
		texDesc.Format = desc.Format;
		texDesc.SampleDesc.Count = 1;
		texDesc.ArraySize = 1;
		texDesc.Usage = D3D11_USAGE_STAGING; //D3D11_USAGE_DYNAMIC
		texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

		m_texture.Release();
		hr = m_device->CreateTexture2D(&texDesc, NULL, &m_texture);
		if (FAILED(hr)) {
			printf("Could not create the staging texture (%lx)\n", (long)hr);
			return NULL;
		}
	}

	m_context->CopyResource(m_texture, texure);

	// 获取纹理数据
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	hr = m_context->Map(m_texture, 0, D3D11_MAP_READ, 0, &mappedResource);
	if (FAILED(hr))
	{
		std::cout << "Failed to map staging texture." << std::endl;
		return nullptr;
	}	

	m_dxgiduplicator->ReleaseFrame();

	//save_d3d11texture_to_image(m_context, m_texture, "d:\\1.jpg");
	int linesize[4];
	for (int i = 0; i < 4; i++)
		linesize[i] = mappedResource.RowPitch;
	
	//纹理数据
	return mappedResource.pData;
}

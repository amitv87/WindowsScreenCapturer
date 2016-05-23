// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <stdio.h>
#include "comdef.h"
#include <sstream>
#include <assert.h>
#include "wincodec.h"
#include "D3DCommon.h"
#include "D3D11.h"
#include "DXGI.h"
#include "DXGI1_2.h"
//#include <SDKDDKVer.h>
#include <fstream>
#include <iostream>
#include <ctime>
#include <chrono>
using std::ofstream;
using namespace std::chrono;

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

long long getTS(){
	return duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
}

int main(int argc, char* argv[])
{
	ID3D11Device* d3ddevice = nullptr;
	ID3D11DeviceContext* d3dcontext = nullptr;
	D3D_FEATURE_LEVEL feature_level;
	D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG || D3D11_CREATE_DEVICE_SINGLETHREADED, nullptr, 0, D3D11_SDK_VERSION, &d3ddevice, &feature_level, &d3dcontext);
	IDXGIDevice* device = nullptr;
	d3ddevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&device);
	IDXGIAdapter* adapter = nullptr;
	device->GetParent(__uuidof(IDXGIAdapter), (void**)&adapter);
	/*
	IDXGIFactory1* factory = nullptr;
	adapter->GetParent(__uuidof(IDXGIFactory1), (void**)&factory);
	IDXGIAdapter1* adapter1 = nullptr;
	factory->EnumAdapters1(0, &adapter1);
	*/
	IDXGIOutput* output = nullptr;
	for (int i = 0;; i++)
	{
		if (adapter->EnumOutputs(i, &output) == DXGI_ERROR_NOT_FOUND)
		{
			std::cout << "No output detected." << std::endl;
			return -1;
		}
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);
		if (desc.AttachedToDesktop) break;
	}
	IDXGIOutput1* output1 = nullptr;
	output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);
	IUnknown* unknown = d3ddevice;
	IDXGIOutputDuplication* output_duplication = nullptr;
	_com_error err(output1->DuplicateOutput(unknown, &output_duplication));
	std::wcout << err.ErrorMessage() << __LINE__ << std::endl;

	int size = 0;
	byte *bytes = new byte[size];

	int index = 0, cur_index = 0;
	HRESULT hr;

	long long ts1 = 0, ts2 = 0;
	int targetFramInterval = 1000 / 30;


	if (argc > 1){
		std::cout << "target frame rate: " << argv[1] << std::endl;
		int targetFrameRate = atoi(argv[1]);
		if (targetFrameRate > 0)
			targetFramInterval = 1000 / targetFrameRate;
	}

	bool save_img = false;
	if (argc > 2){
		save_img = true;
		std::cout << "save image enabled: " << std::endl;
		system("mkdir img_dxgi");
		system("del img_dxgi\\*.argb");
	}

	float elapsedTime = 0, cur_elapsedTime = 0;
	while (true)
	{
		ts1 = getTS();

		if (cur_index > 0){
			float cur_frameRate = cur_index * 1000 / cur_elapsedTime;
			float avg_frameRate = index * 1000 / elapsedTime;
			std::cout << "\r" << "avg_frameRate: " << avg_frameRate << ", cur_frameRate: " << cur_frameRate << std::flush;
		}

		DXGI_OUTDUPL_FRAME_INFO frame_info = { 0 };
		IDXGIResource* resource = nullptr;
		while (frame_info.AccumulatedFrames == 0)
		{

			frame_info = { 0 };
			if (resource != nullptr)
			{
				resource->Release();
				output_duplication->ReleaseFrame();
			}
			resource = nullptr;
			err = _com_error(output_duplication->AcquireNextFrame(INFINITE, &frame_info, &resource));
			if (err.Error() == DXGI_ERROR_ACCESS_LOST)
			{
				output_duplication->Release();
				output1->DuplicateOutput(unknown, &output_duplication);
			}
		}

		ID3D11Texture2D* texture = nullptr;
		resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texture);
		assert(resource != nullptr);

		ID3D11Texture2D* textureBuf;
		D3D11_TEXTURE2D_DESC textureDesc;
		//ComPtr<ID3D11Texture2D> tempTexture = this->c_PreviewTexture;
		D3D11_TEXTURE2D_DESC tempDesc;

		if (texture != nullptr) {
			texture->GetDesc(&tempDesc);

			//std::wcout << "width: " << tempDesc.Width << ", height: " << tempDesc.Height << ", org_fmt: " << tempDesc.Format << ", mip_lvl: " << tempDesc.MipLevels << std::endl;

			ZeroMemory(&textureDesc, sizeof(textureDesc));

			textureDesc.MiscFlags = 0;
			textureDesc.MipLevels = 1;
			textureDesc.ArraySize = 1;
			textureDesc.BindFlags = 0;
			textureDesc.Width = tempDesc.Width;
			textureDesc.Height = tempDesc.Height;
			textureDesc.Format = tempDesc.Format;
			textureDesc.ArraySize = tempDesc.ArraySize;
			textureDesc.SampleDesc = tempDesc.SampleDesc;
			textureDesc.Usage = D3D11_USAGE_STAGING;
			textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

			//textureDesc.SampleDesc.Count = 1;
			//textureDesc.SampleDesc.Quality = 0;
			//textureDesc.Format = DXGI_FORMAT_NV12; // NV12

			d3ddevice->CreateTexture2D(&textureDesc, NULL, &textureBuf);
			d3dcontext->CopyResource(textureBuf, texture);
			D3D11_MAPPED_SUBRESOURCE  mapResource;
			hr = d3dcontext->Map(textureBuf, 0, D3D11_MAP_READ, NULL, &mapResource);

			/*IDXGISurface* CopySurface = nullptr;
			hr = textureBuf->QueryInterface(__uuidof(IDXGISurface), (void **)&CopySurface);
			DXGI_MAPPED_RECT mapResource;
			hr = CopySurface->Map(&mapResource, DXGI_MAP_READ);*/

			if (FAILED(hr)) {
				std::wcout << "d3dcontext->Map error: " << hr << std::endl;
			}
			else {

				int tsize = mapResource.DepthPitch;
				if (tsize != size) {
					size = tsize;
					if (bytes != nullptr)
						delete[] bytes;
					bytes = new byte[size];
				}

				//std::wcout << "size: " << size << ", ts: " << getTS() << std::endl;

				memcpy(bytes, mapResource.pData, size);
				mapResource = { 0 };

				if (save_img){
					char file[100] = "";
					sprintf_s(file, "img_dxgi\\raw%d.argb", index);
					ofstream fout;
					fout.open(file, std::ios::binary);
					fout.write((char*)bytes, size);
					fout.close();
				}
			}
			//CopySurface->Release();
			textureBuf->Release();
		}

		texture->Release();
		resource->Release();
		output_duplication->ReleaseFrame();
		index += 1;
		cur_index += 1;
		ts2 = getTS();
		int sleep_duration = ts2 - ts1;
		sleep_duration = sleep_duration > targetFramInterval ? 0 : targetFramInterval - sleep_duration;
		Sleep(sleep_duration);

		elapsedTime += getTS() - ts1;
		cur_elapsedTime += getTS() - ts1;
		if (cur_elapsedTime > 1000){
			cur_elapsedTime = 0;
			cur_index = 0;
		}
	}

	return 0;
}

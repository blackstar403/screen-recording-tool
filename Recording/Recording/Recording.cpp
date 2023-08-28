// Recording.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "D3D11Duplicator.h"
#include "EncodeMux.h"


int main(int argc, char** argv)
{
	// 初始化COM
	HRESULT hr = CoInitialize(nullptr);
	if (FAILED(hr))
	{
		std::cout << "Failed to initialize COM." << std::endl;
		return 1;
	}

	D3D11Duplicator* d3d11_duplicator = new D3D11Duplicator();
	d3d11_duplicator->Init(0);

	EncodeMux* encode_mux = new EncodeMux();
	encode_mux->Init(argc, argv);
	encode_mux->Start();
	//while (true)
	//{
	//	auto data = d3d11_duplicator->GetDuplicate();
	//	encode_mux->EncodeAndMux(data);
	//	Sleep(1);
	//}

	encode_mux->EncodeAndMux(NULL);

	encode_mux->Stop();

}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件

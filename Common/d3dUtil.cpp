
#include "d3dUtil.h"
#include <comdef.h>
#include <fstream>

using Microsoft::WRL::ComPtr;

DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
	ErrorCode(hr),
	FunctionName(functionName),
	Filename(filename),
	LineNumber(lineNumber)
{
}

bool d3dUtil::IsKeyDown(int vkeyCode)
{
	return (GetAsyncKeyState(vkeyCode) & 0x8000) != 0;
}

ComPtr<ID3DBlob> d3dUtil::LoadBinary(const std::wstring& filename)
{
	std::ifstream fin(filename, std::ios::binary);

	fin.seekg(0, std::ios_base::end);
	std::ifstream::pos_type size = (int)fin.tellg();
	fin.seekg(0, std::ios_base::beg);

	ComPtr<ID3DBlob> blob;
	ThrowIfFailed(D3DCreateBlob(size, blob.GetAddressOf()));

	fin.read((char*)blob->GetBufferPointer(), size);
	fin.close();

	return blob;
}

// ������ �ڴ�->�ϴ���->����������(GPU)
Microsoft::WRL::ComPtr<ID3D12Resource> d3dUtil::CreateDefaultBuffer(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	const void* initData, // �������ݶ��㻺����/����������
	UINT64 byteSize, // ���ݴ�С
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
	ComPtr<ID3D12Resource> defaultBuffer;

	// Create the actual default buffer resource. ����ʵ�ʵ�Ĭ�ϻ�������Դ
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // Ĭ�϶�
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

	// In order to copy CPU memory data into our default buffer, we need to create
	// an intermediate upload heap. Ϊ�˽�CPU�ڴ��е����ݸ��Ƶ�Ĭ�ϻ�����,����Ҫ����һ�������н�λ�õ��ϴ���
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // �ϴ���
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

	// Describe the data we want to copy into the default buffer. ����ϣ�����Ƶ�Ĭ�ϻ������е�����
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData; // ָ��ĳ��ϵͳ�ڴ���ָ��,�����г�ʼ�����������õ�����
	subResourceData.RowPitch = byteSize; // ���ڻ���������,�˲���Ϊ��Ҫ�������ݵ��ֽ���
	subResourceData.SlicePitch = subResourceData.RowPitch; // ���ڻ���������,�˲���Ϊ��Ҫ�������ݵ��ֽ���

	// �����ݸ��Ƶ�Ĭ�ϻ�����������(Schedule)  
	// �������� UpdateSubresources �ὫCPU�ڴ��е����ݸ��Ƶ�λ���н�λ�õ��ϴ���
	// Ȼ����� ID3D12CommandList::CopySubresourceRegion ���ϴ��ѵ����ݸ��Ƶ� mBuffer
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	// ע��: ��������������,������������uploadBuffer
	// ��Ϊ�����б��еĸ��Ʋ�������δִ��
	// �����ߵ�֪������ɺ�,�����ͷ�uploadBuffer
	return defaultBuffer;
}

ComPtr<ID3DBlob> d3dUtil::CompileShader(
	const std::wstring& filename,	// .hlsl�ļ�·��
	const D3D_SHADER_MACRO* defines,// ����nullptr
	const std::string& entrypoint,	// ��ɫ������ں�����
	const std::string& target)		// ָ����ɫ�����ͺͰ汾��string
{
	UINT compileFlags = 0; // ������ڵ���ģʽ,��ʹ�õ��Ա�־
#if defined(DEBUG) || defined(_DEBUG)  
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION; //�õ���ģʽ��������ɫ��,�����Ż��׶�
#endif

	HRESULT hr = S_OK;

	// ID3DBlob��������һ����ͨ���ڴ��,��������
	// GetBufferPointer(),��������void*���͵�ָ��,ʹ��ǰҪת������.
	// GetBufferSize(),�������ݴ�С
	ComPtr<ID3DBlob> byteCode = nullptr; // �������õ���ɫ�������ֽ���
	ComPtr<ID3DBlob> errors; // ������뱨��,���汨�����ַ���

	// para1: .hlslԴ�����ļ�
	// para2: ���鲻ʹ��,����Ϊnullptr
	// para3: ���鲻ʹ��
	// para4: ��ɫ������ں�����
	// para5: ָ��������ɫ�����ͺͰ汾���ַ���
	// para6: ָʾ����ɫ������Ӧ����α���ı�־
	// para7: ���鲻ʹ��
	// para8: ����һ��ָ�� ID3DBlob ���ݽṹ��ָ��,�������ű���õ���ɫ�������ֽ���
	// para8: ����һ��ָ�� ID3DBlob ���ݽṹ��ָ��,����������,����ᴢ�汨�����ַ���
	hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

	// ��������Ϣ��������Դ���
	if (errors != nullptr)
		OutputDebugStringA((char*)errors->GetBufferPointer());

	ThrowIfFailed(hr);

	return byteCode;
}

std::wstring DxException::ToString()const
{
	// Get the string description of the error code.
	_com_error err(ErrorCode);
	std::wstring msg = err.ErrorMessage();

	return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
}


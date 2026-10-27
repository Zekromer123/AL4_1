#include "2d/SpriteCommon.h"
#include "base/DirectXCommon.h"
#include "base/DXCManager.h"
#include "base/TextureManager.h"
#include "base/Log.h"
#include <algorithm>
#include <cassert>

SpriteCommon* SpriteCommon::GetInstance() {
	static SpriteCommon instance;
	return &instance;
}

void SpriteCommon::Initialize(DirectXCommon* dxCommon, DXCManager* dxc) {
	dxCommon_ = dxCommon;
	CreateRootSignature();
	for (int i = 0; i < static_cast<int>(BlendMode::Count); ++i) {
		CreatePso(dxc, static_cast<BlendMode>(i));
	}
}

void SpriteCommon::Finalize() {
	for (auto& pso : pipelineStates_) {
		pso.Reset();
	}
	rootSignature_.Reset();
	dxCommon_ = nullptr;
}

void SpriteCommon::PreDraw(ID3D12GraphicsCommandList* cmdList, BlendMode blendMode) {
	cmdList->SetGraphicsRootSignature(rootSignature_.Get());
	int idx = std::clamp(static_cast<int>(blendMode), 0, static_cast<int>(BlendMode::Count) - 1);
	cmdList->SetPipelineState(pipelineStates_[idx].Get());
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	TextureManager::GetInstance()->SetDescriptorHeaps(cmdList);
}

void SpriteCommon::PostDraw() {
}

D3D12_BLEND_DESC SpriteCommon::MakeBlendDesc(BlendMode mode) {
	D3D12_BLEND_DESC blendDesc{};
	D3D12_RENDER_TARGET_BLEND_DESC& rt = blendDesc.RenderTarget[0];
	rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	switch (mode) {
	case BlendMode::AlphaBlend:
		rt.BlendEnable    = TRUE;
		rt.SrcBlend       = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend      = D3D12_BLEND_INV_SRC_ALPHA;
		rt.BlendOp        = D3D12_BLEND_OP_ADD;
		rt.SrcBlendAlpha  = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_ZERO;
		rt.BlendOpAlpha   = D3D12_BLEND_OP_ADD;
		break;
	case BlendMode::Add:
		rt.BlendEnable    = TRUE;
		rt.SrcBlend       = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend      = D3D12_BLEND_ONE;
		rt.BlendOp        = D3D12_BLEND_OP_ADD;
		rt.SrcBlendAlpha  = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_ZERO;
		rt.BlendOpAlpha   = D3D12_BLEND_OP_ADD;
		break;
	case BlendMode::Multiply:
		rt.BlendEnable    = TRUE;
		rt.SrcBlend       = D3D12_BLEND_ZERO;
		rt.DestBlend      = D3D12_BLEND_SRC_COLOR;
		rt.BlendOp        = D3D12_BLEND_OP_ADD;
		rt.SrcBlendAlpha  = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_ZERO;
		rt.BlendOpAlpha   = D3D12_BLEND_OP_ADD;
		break;
	default: // Opaque
		rt.BlendEnable = FALSE;
		break;
	}
	return blendDesc;
}

void SpriteCommon::CreateRootSignature() {
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // t0
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Root parameters:
	// [0] TransformationMatrix (b0, Vertex)
	// [1] Material (b0, Pixel)
	// [2] Texture SRV (t0, Pixel)
	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootSigDesc.pParameters = rootParameters;
	rootSigDesc.NumParameters = _countof(rootParameters);
	rootSigDesc.pStaticSamplers = staticSamplers;
	rootSigDesc.NumStaticSamplers = _countof(staticSamplers);
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr) || !signatureBlob) {
		if (errorBlob) {
			Log::GetInstance()->logOutput(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
			errorBlob->Release();
		}
		assert(false);
		return;
	}
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
	signatureBlob->Release();
}

void SpriteCommon::CreatePso(DXCManager* dxc, BlendMode mode) {
	// 2D Sprite Input Layout: POSITION (float4) + TEXCOORD (float2)
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	IDxcBlob* vsBlob = dxc->CompileShader(L"shaders/Sprite.VS.hlsl", L"vs_6_0", dxc->GetDxcUtils(), dxc->GetDxcCompiler(), dxc->GetIncludeHandler());
	assert(vsBlob != nullptr);
	IDxcBlob* psBlob = dxc->CompileShader(L"shaders/Sprite.PS.hlsl", L"ps_6_0", dxc->GetDxcUtils(), dxc->GetDxcCompiler(), dxc->GetIncludeHandler());
	assert(psBlob != nullptr);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoDesc.BlendState = MakeBlendDesc(mode);
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStates_[static_cast<int>(mode)]));
	assert(SUCCEEDED(hr));

	vsBlob->Release();
	psBlob->Release();
}

#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "base/BlendMode.h"

class DirectXCommon;
class DXCManager;

class SpriteCommon {
public:
	static SpriteCommon* GetInstance();

	void Initialize(DirectXCommon* dxCommon, DXCManager* dxc);
	void Finalize();

	/// Pre-draw setup for 2D sprites (binds RootSignature, PSO, topology, and SRV heaps)
	void PreDraw(ID3D12GraphicsCommandList* cmdList, BlendMode blendMode = BlendMode::AlphaBlend);
	void PostDraw();

	ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
	DirectXCommon* GetDirectXCommon() const { return dxCommon_; }

private:
	SpriteCommon() = default;
	~SpriteCommon() = default;
	SpriteCommon(const SpriteCommon&) = delete;
	SpriteCommon& operator=(const SpriteCommon&) = delete;

	void CreateRootSignature();
	void CreatePso(DXCManager* dxc, BlendMode mode);
	D3D12_BLEND_DESC MakeBlendDesc(BlendMode mode);

	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates_[static_cast<int>(BlendMode::Count)];
};

// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// Custom mesh component with direct GPU buffer management via FPrimitiveSceneProxy.

#include "DirectProxyMeshComponent.h"
#include "PrimitiveViewRelevance.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "PrimitiveSceneProxy.h"
#include "Materials/Material.h"
#include "DynamicMeshBuilder.h"
#include "MaterialDomain.h"
#include "StaticMeshResources.h"
#include "SceneManagement.h"

// Data sent to the render thread for dynamic updates
struct FDirectProxyDynamicData
{
	TArray<FVector3f> Positions;
	TArray<FVector3f> Normals;
};

// ============================================================================
// Custom Buffer Classes
// ============================================================================

class FDirectProxyPositionBuffer : public FVertexBuffer
{
public:
	int32 NumVertices = 0;
	FShaderResourceViewRHIRef SRV;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		if (NumVertices > 0)
		{
			const FRHIBufferCreateDesc Desc =
				FRHIBufferCreateDesc::CreateVertex<FVector3f>(TEXT("DirectProxyPositionBuffer"), NumVertices)
				.AddUsage(EBufferUsageFlags::Dynamic | EBufferUsageFlags::ShaderResource)
				.DetermineInitialState();
			VertexBufferRHI = RHICmdList.CreateBuffer(Desc);
			SRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI,
				FRHIViewDesc::CreateBufferSRV()
				.SetType(FRHIViewDesc::EBufferType::Typed)
				.SetFormat(PF_R32_FLOAT));
		}
	}

	void UpdateData(FRHICommandListBase& RHICmdList, const TArray<FVector3f>& Data)
	{
		if (!IsValidRef(VertexBufferRHI) || Data.Num() == 0)
		{
			return;
		}
		void* Buffer = RHICmdList.LockBuffer(VertexBufferRHI, 0, Data.Num() * sizeof(FVector3f), RLM_WriteOnly);
		FMemory::Memcpy(Buffer, Data.GetData(), Data.Num() * sizeof(FVector3f));
		RHICmdList.UnlockBuffer(VertexBufferRHI);
	}
};

class FDirectProxyTangentBuffer : public FVertexBuffer
{
public:
	int32 NumVertices = 0;
	FShaderResourceViewRHIRef SRV;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		if (NumVertices > 0)
		{
			// 2 x FPackedNormal per vertex (tangent + normal)
			const FRHIBufferCreateDesc Desc =
				FRHIBufferCreateDesc::CreateVertex(TEXT("DirectProxyTangentBuffer"), NumVertices * 2 * sizeof(FPackedNormal))
				.SetStride(sizeof(FPackedNormal))
				.AddUsage(EBufferUsageFlags::Dynamic | EBufferUsageFlags::ShaderResource)
				.DetermineInitialState();
			VertexBufferRHI = RHICmdList.CreateBuffer(Desc);
			SRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI,
				FRHIViewDesc::CreateBufferSRV()
				.SetType(FRHIViewDesc::EBufferType::Typed)
				.SetFormat(PF_R8G8B8A8_SNORM));
		}
	}

	void UpdateData(FRHICommandListBase& RHICmdList, const TArray<FVector3f>& Normals)
	{
		if (!IsValidRef(VertexBufferRHI) || Normals.Num() == 0)
		{
			return;
		}
		const int32 NumVerts = Normals.Num();
		void* Buffer = RHICmdList.LockBuffer(VertexBufferRHI, 0, NumVerts * 2 * sizeof(FPackedNormal), RLM_WriteOnly);
		FPackedNormal* TangentData = static_cast<FPackedNormal*>(Buffer);

		for (int32 i = 0; i < NumVerts; i++)
		{
			const FVector3f& Normal = Normals[i];
			FVector3f Tangent;
			if (FMath::Abs(Normal.Z) < 0.999f)
			{
				Tangent = FVector3f::CrossProduct(FVector3f(0, 0, 1), Normal).GetSafeNormal();
			}
			else
			{
				Tangent = FVector3f::CrossProduct(FVector3f(1, 0, 0), Normal).GetSafeNormal();
			}

			TangentData[i * 2 + 0] = FPackedNormal(Tangent);
			TangentData[i * 2 + 1] = FPackedNormal(Normal);
			TangentData[i * 2 + 1].Vector.W = 127;
		}

		RHICmdList.UnlockBuffer(VertexBufferRHI);
	}
};

class FDirectProxyTexCoordBuffer : public FVertexBuffer
{
public:
	int32 NumVertices = 0;
	FShaderResourceViewRHIRef SRV;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		if (NumVertices > 0)
		{
			const FRHIBufferCreateDesc Desc =
				FRHIBufferCreateDesc::CreateVertex<FVector2f>(TEXT("DirectProxyTexCoordBuffer"), NumVertices)
				.AddUsage(EBufferUsageFlags::Static | EBufferUsageFlags::ShaderResource)
				.DetermineInitialState();
			VertexBufferRHI = RHICmdList.CreateBuffer(Desc);
			SRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI,
				FRHIViewDesc::CreateBufferSRV()
				.SetType(FRHIViewDesc::EBufferType::Typed)
				.SetFormat(PF_G32R32F));
		}
	}

	void SetData(FRHICommandListBase& RHICmdList, const TArray<FVector2f>& Data)
	{
		if (!IsValidRef(VertexBufferRHI) || Data.Num() == 0)
		{
			return;
		}
		void* Buffer = RHICmdList.LockBuffer(VertexBufferRHI, 0, Data.Num() * sizeof(FVector2f), RLM_WriteOnly);
		FMemory::Memcpy(Buffer, Data.GetData(), Data.Num() * sizeof(FVector2f));
		RHICmdList.UnlockBuffer(VertexBufferRHI);
	}
};

class FDirectProxyColorBuffer : public FVertexBuffer
{
public:
	int32 NumVertices = 0;
	FShaderResourceViewRHIRef SRV;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		if (NumVertices > 0)
		{
			const FRHIBufferCreateDesc Desc =
				FRHIBufferCreateDesc::CreateVertex<FColor>(TEXT("DirectProxyColorBuffer"), NumVertices)
				.AddUsage(EBufferUsageFlags::Static | EBufferUsageFlags::ShaderResource)
				.DetermineInitialState();
			VertexBufferRHI = RHICmdList.CreateBuffer(Desc);
			SRV = RHICmdList.CreateShaderResourceView(VertexBufferRHI,
				FRHIViewDesc::CreateBufferSRV()
				.SetType(FRHIViewDesc::EBufferType::Typed)
				.SetFormat(PF_R8G8B8A8));

			void* Buffer = RHICmdList.LockBuffer(VertexBufferRHI, 0, NumVertices * sizeof(FColor), RLM_WriteOnly);
			FMemory::Memset(Buffer, 0xFF, NumVertices * sizeof(FColor));
			RHICmdList.UnlockBuffer(VertexBufferRHI);
		}
	}
};

class FDirectProxyIndexBuffer : public FIndexBuffer
{
public:
	int32 NumIndices = 0;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		if (NumIndices > 0)
		{
			const FRHIBufferCreateDesc Desc =
				FRHIBufferCreateDesc::CreateIndex<uint32>(TEXT("DirectProxyIndexBuffer"), NumIndices)
				.AddUsage(EBufferUsageFlags::Static)
				.DetermineInitialState();
			IndexBufferRHI = RHICmdList.CreateBuffer(Desc);
		}
	}

	void SetData(FRHICommandListBase& RHICmdList, const TArray<uint32>& Data)
	{
		if (!IsValidRef(IndexBufferRHI) || Data.Num() == 0)
		{
			return;
		}
		void* Buffer = RHICmdList.LockBuffer(IndexBufferRHI, 0, Data.Num() * sizeof(uint32), RLM_WriteOnly);
		FMemory::Memcpy(Buffer, Data.GetData(), Data.Num() * sizeof(uint32));
		RHICmdList.UnlockBuffer(IndexBufferRHI);
	}
};

// ============================================================================
// Scene Proxy
// ============================================================================

class FDirectProxyMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	FDirectProxyMeshSceneProxy(UDirectProxyMeshComponent* Component,
		const TArray<uint32>& InIndices,
		const TArray<FVector2f>& InTexCoords,
		const TArray<FVector3f>& InPositions,
		const TArray<FVector3f>& InNormals,
		UMaterialInterface* InMaterial)
		: FPrimitiveSceneProxy(Component)
		, VertexFactory(GetScene().GetFeatureLevel(), "FDirectProxyMeshVertexFactory")
		, Material(InMaterial)
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetShaderPlatform()))
	{
		const int32 NumVerts = InPositions.Num();
		const int32 NumIdx = InIndices.Num();

		PositionBuffer.NumVertices = NumVerts;
		TangentBuffer.NumVertices = NumVerts;
		TexCoordBuffer.NumVertices = NumVerts;
		ColorBuffer.NumVertices = NumVerts;
		IndexBuffer.NumIndices = NumIdx;

		CachedPositions = InPositions;
		CachedNormals = InNormals;
		CachedTexCoords = InTexCoords;
		CachedIndices = InIndices;
	}

	virtual ~FDirectProxyMeshSceneProxy()
	{
		PositionBuffer.ReleaseResource();
		TangentBuffer.ReleaseResource();
		TexCoordBuffer.ReleaseResource();
		ColorBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override
	{
		PositionBuffer.InitResource(RHICmdList);
		TangentBuffer.InitResource(RHICmdList);
		TexCoordBuffer.InitResource(RHICmdList);
		ColorBuffer.InitResource(RHICmdList);
		IndexBuffer.InitResource(RHICmdList);

		PositionBuffer.UpdateData(RHICmdList, CachedPositions);
		TangentBuffer.UpdateData(RHICmdList, CachedNormals);
		TexCoordBuffer.SetData(RHICmdList, CachedTexCoords);
		IndexBuffer.SetData(RHICmdList, CachedIndices);

		CachedPositions.Empty();
		CachedNormals.Empty();
		CachedTexCoords.Empty();
		CachedIndices.Empty();

		FLocalVertexFactory::FDataType Data;
		Data.PositionComponent = FVertexStreamComponent(&PositionBuffer, 0, sizeof(FVector3f), VET_Float3);
		Data.PositionComponentSRV = PositionBuffer.SRV;
		Data.TangentBasisComponents[0] = FVertexStreamComponent(&TangentBuffer, 0, 2 * sizeof(FPackedNormal), VET_PackedNormal);
		Data.TangentBasisComponents[1] = FVertexStreamComponent(&TangentBuffer, sizeof(FPackedNormal), 2 * sizeof(FPackedNormal), VET_PackedNormal);
		Data.TangentsSRV = TangentBuffer.SRV;
		Data.ColorComponent = FVertexStreamComponent(&ColorBuffer, 0, sizeof(FColor), VET_Color);
		Data.ColorComponentsSRV = ColorBuffer.SRV;
		Data.TextureCoordinates.Add(FVertexStreamComponent(&TexCoordBuffer, 0, sizeof(FVector2f), VET_Float2));
		Data.TextureCoordinatesSRV = TexCoordBuffer.SRV;
		Data.NumTexCoords = 1;
		Data.LightMapCoordinateComponent = FVertexStreamComponent(&TexCoordBuffer, 0, sizeof(FVector2f), VET_Float2);
		Data.LightMapCoordinateIndex = 0;

		VertexFactory.SetData(RHICmdList, Data);
		VertexFactory.InitResource(RHICmdList);
	}

	void UpdateDynamicData_RenderThread(FRHICommandListBase& RHICmdList, FDirectProxyDynamicData&& NewData)
	{
		PositionBuffer.UpdateData(RHICmdList, NewData.Positions);
		TangentBuffer.UpdateData(RHICmdList, NewData.Normals);
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		if (PositionBuffer.NumVertices == 0 || IndexBuffer.NumIndices == 0)
		{
			return;
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				FMeshBatch& Mesh = Collector.AllocateMesh();
				Mesh.VertexFactory = &VertexFactory;
				Mesh.Type = PT_TriangleList;

				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = &IndexBuffer;
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = IndexBuffer.NumIndices / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = PositionBuffer.NumVertices - 1;

				Mesh.MaterialRenderProxy = Material->GetRenderProxy();
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.bDisableBackfaceCulling = false;
				Mesh.CastShadow = true;
				Mesh.bUseAsOccluder = false;

				Collector.AddMesh(ViewIndex, Mesh);
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override { return sizeof(*this); }
	SIZE_T GetTypeHash() const override { static size_t UniquePointer; return reinterpret_cast<size_t>(&UniquePointer); }
	virtual bool CanBeOccluded() const override { return !MaterialRelevance.bDisableDepthTest; }

private:
	FDirectProxyPositionBuffer PositionBuffer;
	FDirectProxyTangentBuffer TangentBuffer;
	FDirectProxyTexCoordBuffer TexCoordBuffer;
	FDirectProxyColorBuffer ColorBuffer;
	FDirectProxyIndexBuffer IndexBuffer;
	FLocalVertexFactory VertexFactory;

	UMaterialInterface* Material;
	FMaterialRelevance MaterialRelevance;

	TArray<FVector3f> CachedPositions;
	TArray<FVector3f> CachedNormals;
	TArray<FVector2f> CachedTexCoords;
	TArray<uint32> CachedIndices;
};

// ============================================================================
// UDirectProxyMeshComponent Implementation
// ============================================================================

UDirectProxyMeshComponent::UDirectProxyMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	LocalBounds = FBox(ForceInit);
}

void UDirectProxyMeshComponent::SetStaticTopology(TArray<uint32>&& InIndices, TArray<FVector2f>&& InTexCoords, int32 InNumVertices)
{
	Indices = MoveTemp(InIndices);
	TexCoords = MoveTemp(InTexCoords);
	NumVertices = InNumVertices;

	// Pre-size dynamic buffers to avoid per-frame allocations
	Positions.SetNumUninitialized(NumVertices);
	Normals.SetNumUninitialized(NumVertices);

	MarkRenderStateDirty();
}

void UDirectProxyMeshComponent::SetFixedBounds(const FBox& InBounds)
{
	LocalBounds = InBounds;
	UpdateBounds();
}

void UDirectProxyMeshComponent::UpdateDynamicData(const TArray<FVector3f>& InPositions, const TArray<FVector3f>& InNormals)
{
	// Copy into persistent buffers (avoids per-frame allocation)
	FMemory::Memcpy(Positions.GetData(), InPositions.GetData(), InPositions.Num() * sizeof(FVector3f));
	FMemory::Memcpy(Normals.GetData(), InNormals.GetData(), InNormals.Num() * sizeof(FVector3f));

	if (SceneProxy)
	{
		FDirectProxyDynamicData NewData;
		NewData.Positions = Positions;
		NewData.Normals = Normals;

		FDirectProxyMeshSceneProxy* Proxy = static_cast<FDirectProxyMeshSceneProxy*>(SceneProxy);
		ENQUEUE_RENDER_COMMAND(UpdateDirectProxyMeshData)(
			[Proxy, DynData = MoveTemp(NewData)](FRHICommandListImmediate& RHICmdList) mutable
			{
				Proxy->UpdateDynamicData_RenderThread(RHICmdList, MoveTemp(DynData));
			}
		);
	}
}

FPrimitiveSceneProxy* UDirectProxyMeshComponent::CreateSceneProxy()
{
	if (!HasValidMeshData() || Positions.Num() == 0)
	{
		return nullptr;
	}

	UMaterialInterface* Mat = GetMaterial(0);
	if (!Mat)
	{
		Mat = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	return new FDirectProxyMeshSceneProxy(this, Indices, TexCoords, Positions, Normals, Mat);
}

FBoxSphereBounds UDirectProxyMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (LocalBounds.IsValid)
	{
		return FBoxSphereBounds(LocalBounds.TransformBy(LocalToWorld));
	}
	return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
}

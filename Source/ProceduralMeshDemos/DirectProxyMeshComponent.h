// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// A custom mesh component that manages GPU buffers directly via FPrimitiveSceneProxy,
// enabling efficient per-frame vertex updates with a single CPU-to-GPU copy.

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "DirectProxyMeshComponent.generated.h"

UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class PROCEDURALMESHDEMOS_API UDirectProxyMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	UDirectProxyMeshComponent();

	// Called once when topology changes. Triggers proxy recreation.
	void SetStaticTopology(TArray<uint32>&& InIndices, TArray<FVector2f>&& InTexCoords, int32 InNumVertices);

	// Called per frame to update positions and normals without recreating the proxy.
	// Data is copied internally; callers may reuse their buffers.
	void UpdateDynamicData(const TArray<FVector3f>& InPositions, const TArray<FVector3f>& InNormals);

	// Set fixed bounds to avoid per-frame O(N) bounds recalculation.
	void SetFixedBounds(const FBox& InBounds);

	// UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual int32 GetNumMaterials() const override { return 1; }
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	bool HasValidMeshData() const { return NumVertices > 0 && Indices.Num() > 0; }

private:
	// Static topology (set once, triggers proxy recreation)
	TArray<uint32> Indices;
	TArray<FVector2f> TexCoords;
	int32 NumVertices = 0;

	// Dynamic data (updated per frame, kept for proxy recreation)
	TArray<FVector3f> Positions;
	TArray<FVector3f> Normals;

	// Cached bounds
	FBox LocalBounds;
};

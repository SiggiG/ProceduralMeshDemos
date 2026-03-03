// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// Example heightfield grid animated with sine and cosine waves
// Uses a custom FPrimitiveSceneProxy for direct GPU buffer updates (single copy path)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeightFieldDirectProxyActor.generated.h"

class UDirectProxyMeshComponent;

UCLASS()
class PROCEDURALMESHDEMOS_API AHeightFieldDirectProxyActor : public AActor
{
	GENERATED_BODY()

public:
	AHeightFieldDirectProxyActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	FVector Size = FVector(1000.0f, 1000.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float ScaleFactor = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 LengthSections = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 WidthSections = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	UMaterialInterface* Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	bool AnimateMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float AnimationSpeedX = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float AnimationSpeedY = 4.5f;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	UDirectProxyMeshComponent* MeshComponent;

	float CurrentAnimationFrameX = 0.0f;
	float CurrentAnimationFrameY = 0.0f;

private:
	void GenerateMesh();
	void FillPositionsAndNormals(TArray<FVector3f>& OutPositions, TArray<FVector3f>& OutNormals,
		const FVector2D& SectionSize);

	// Persistent buffers reused each frame to avoid per-frame allocations
	TArray<FVector3f> Positions;
	TArray<FVector3f> Normals;

	bool bMeshCreated = false;
	bool bRequiresMeshRebuild = false;
};

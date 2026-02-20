// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// A ProceduralMeshComponent subclass that prevents mesh data from being serialized

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "RuntimeProceduralMeshComponent.generated.h"

/**
 * A UProceduralMeshComponent subclass that clears mesh section and collision data
 * before serialization. This prevents generated mesh data and cooked physics data
 * from bloating Blueprint and level assets.
 * The owning actor is expected to regenerate the mesh in PostLoad / OnConstruction.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class PROCEDURALMESHDEMOS_API URuntimeProceduralMeshComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	URuntimeProceduralMeshComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UPrimitiveComponent Interface
	virtual UBodySetup* GetBodySetup() override;
	//~ End UPrimitiveComponent Interface

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface
};

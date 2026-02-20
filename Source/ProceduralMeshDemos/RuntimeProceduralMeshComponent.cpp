// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// A ProceduralMeshComponent subclass that prevents mesh data from being serialized

#include "RuntimeProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"

URuntimeProceduralMeshComponent::URuntimeProceduralMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Disable complex-as-simple collision so no physics trimesh is cooked.
	// This avoids large cooked collision data being serialized into assets.
	bUseComplexAsSimpleCollision = false;
}

UBodySetup* URuntimeProceduralMeshComponent::GetBodySetup()
{
	UBodySetup* BodySetup = Super::GetBodySetup();

	// Mark the BodySetup as Transient so it is never serialized into the asset.
	// UProceduralMeshComponent::CreateBodySetupHelper() creates it with RF_Public |
	// RF_ArchetypeObject when IsTemplate() is true, which causes it to be written
	// as a separate named export in the package — making the asset very large.
	// Setting RF_Transient overrides that and keeps it out of the saved package.
	if (BodySetup && !BodySetup->HasAnyFlags(RF_Transient))
	{
		BodySetup->SetFlags(RF_Transient);
		// Also clear the public flag so it is not treated as a package export
		BodySetup->ClearFlags(RF_Public);
	}

	return BodySetup;
}

void URuntimeProceduralMeshComponent::Serialize(FArchive& Ar)
{
	// Only intercept persistent saves to disk (level saves, asset saves).
	// Skip during transactions (undo/redo), duplications (construction script re-run),
	// and other non-persistent archives to avoid clearing the in-memory mesh state.
	if (Ar.IsSaving() && Ar.IsPersistent() && !Ar.IsTransacting())
	{
		// Save the current mesh sections so we can restore them after serialization.
		// This keeps the in-memory visual state intact after a save; without the restore
		// the viewport mesh would disappear until the next OnConstruction call.
		const int32 NumSections = GetNumSections();
		TArray<FProcMeshSection> SavedSections;
		SavedSections.Reserve(NumSections);
		for (int32 i = 0; i < NumSections; ++i)
		{
			if (FProcMeshSection* Section = GetProcMeshSection(i))
			{
				SavedSections.Add(*Section);
			}
		}

		// Clear mesh and collision data so nothing large is written to the asset.
		ClearAllMeshSections();
		ClearCollisionConvexMeshes();

		if (ProcMeshBodySetup)
		{
			ProcMeshBodySetup->InvalidatePhysicsData();
			ProcMeshBodySetup->AggGeom.EmptyElements();
		}

		Super::Serialize(Ar);

		// Restore mesh sections so the viewport continues to show the mesh.
		for (int32 i = 0; i < SavedSections.Num(); ++i)
		{
			SetProcMeshSection(i, SavedSections[i]);
		}

		return;
	}

	Super::Serialize(Ar);
}

#include "AEPathHeatmapRendererComponent.h"

#include "AdaptiveEnvLog.h"
#include "AdaptiveEnvSettings.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "CanvasItem.h"
#include "Components/MeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace AEPathHeatmapRendererPrivate
{
	/* Reports whether one material parameter list contains an exact configured name. */
	bool ContainsParameterName(
		const TConstArrayView<FMaterialParameterInfo> Parameters,
		const FName RequiredName)
	{
		return Parameters.ContainsByPredicate(
			[RequiredName](const FMaterialParameterInfo& Parameter)
			{
				return Parameter.Name == RequiredName;
			});
	}
}

/* Create a visual consumer whose scheduling remains owned by the World Subsystem. */
UAEPathHeatmapRendererComponent::UAEPathHeatmapRendererComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

/* Register this renderer with the current World Subsystem. */
void UAEPathHeatmapRendererComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem =
			World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->RegisterPathHeatmapRenderer(this);
		}
	}
}

/* Disable the material contract and unregister before component teardown. */
void UAEPathHeatmapRendererComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	DisableMaterialOutput();
	ReleaseMeshMaterialBindings();
	BoundLandscape.Reset();
	BoundMaterialOutputCount = 0;
	if (UWorld* World = GetWorld())
	{
		if (UAEAdaptiveEnvWorldSubsystem* Subsystem =
			World->GetSubsystem<UAEAdaptiveEnvWorldSubsystem>())
		{
			Subsystem->UnregisterPathHeatmapRenderer(this);
		}
	}
	PendingCommands.Reset();
	PendingCommandPositions.Reset();
	PathHeatmapRenderTarget = nullptr;
	Super::EndPlay(EndPlayReason);
}

/* Create the linear Render Target and publish the material-side binding. */
bool UAEPathHeatmapRendererComponent::InitializeVisualOutput(
	const FIntPoint& GridDimensions,
	const FBox2D& GridWorldBounds)
{
	check(IsInGameThread());
	if (GridDimensions.X <= 0 || GridDimensions.Y <= 0
		|| !GridWorldBounds.bIsValid)
	{
		DisableMaterialOutput();
		return false;
	}

	// Allocate one texture texel per shared runtime Cell.
	const FLinearColor NeutralVisualValue(128.0f / 255.0f, 128.0f / 255.0f, 0.0f, 0.0f);
	TextureDimensions = GridDimensions;
	PathHeatmapRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	if (!IsValid(PathHeatmapRenderTarget))
	{
		return false;
	}
	PathHeatmapRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	PathHeatmapRenderTarget->ClearColor = NeutralVisualValue;
	PathHeatmapRenderTarget->bAutoGenerateMips = false;
	PathHeatmapRenderTarget->InitAutoFormat(TextureDimensions.X, TextureDimensions.Y);
	PathHeatmapRenderTarget->UpdateResourceImmediate(true);
	UKismetRenderingLibrary::ClearRenderTarget2D(this, PathHeatmapRenderTarget, NeutralVisualValue);

	// Encode the stable World XY to texture UV transform.
	const FVector2D WorldSize = GridWorldBounds.GetSize();
	if (WorldSize.X <= UE_DOUBLE_SMALL_NUMBER || WorldSize.Y <= UE_DOUBLE_SMALL_NUMBER)
	{
		PathHeatmapRenderTarget = nullptr;
		return false;
	}
	GridTransform = FLinearColor(
		GridWorldBounds.Min.X,
		GridWorldBounds.Min.Y,
		1.0 / WorldSize.X,
		1.0 / WorldSize.Y);
	PendingCommands.Reset();
	PendingCommandPositions.Reset();
	BindMaterialOutputs();
	return true;
}

/* Rebind configured outputs to the current Render Target without changing its contents. */
bool UAEPathHeatmapRendererComponent::RefreshMaterialBindings()
{
	check(IsInGameThread());
	if (!IsValid(PathHeatmapRenderTarget))
	{
		UE_LOG(
			LogAdaptiveEnv,
			Warning,
			TEXT("M6 material outputs cannot refresh before Render Target initialization. World=%s Owner=%s"),
			*GetNameSafe(GetWorld()),
			*GetNameSafe(GetOwner()));
		return false;
	}
	return BindMaterialOutputs() > 0;
}

/* Keep only the newest command for each Cell while preserving bounded storage. */
void UAEPathHeatmapRendererComponent::EnqueueVisualCommands(
	const TConstArrayView<FAEPathHeatmapVisualCommand> Commands)
{
	for (const FAEPathHeatmapVisualCommand& Command : Commands)
	{
		if (Command.CellIndex < 0
			|| Command.Coordinate.X < 0 || Command.Coordinate.Y < 0
			|| Command.Coordinate.X >= TextureDimensions.X
			|| Command.Coordinate.Y >= TextureDimensions.Y)
		{
			continue;
		}
		if (const int32* ExistingPosition = PendingCommandPositions.Find(Command.CellIndex))
		{
			FAEPathHeatmapVisualCommand& Existing = PendingCommands[*ExistingPosition];
			if (Command.PathVisualRevision >= Existing.PathVisualRevision)
			{
				Existing = Command;
			}
		}
		else
		{
			const int32 NewPosition = PendingCommands.Add(Command);
			PendingCommandPositions.Add(Command.CellIndex, NewPosition);
		}
	}
}

/* Draw a bounded stable prefix of one-texel updates into the persistent Render Target. */
void UAEPathHeatmapRendererComponent::ApplyVisualBudget(
	const int32 MaxCommands)
{
	check(IsInGameThread());
	if (!IsValid(PathHeatmapRenderTarget)
		|| MaxCommands <= 0 || PendingCommands.IsEmpty())
	{
		return;
	}

	// Open one Canvas pass so every applied command updates the same texture resource.
	UCanvas* Canvas = nullptr;
	FVector2D RenderTargetSize;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(
		this,
		PathHeatmapRenderTarget,
		Canvas,
		RenderTargetSize,
		Context);
	if (Canvas == nullptr || Canvas->Canvas == nullptr)
	{
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
		return;
	}

	const int32 ApplyCount = FMath::Min(MaxCommands, PendingCommands.Num());
	for (int32 CommandIndex = 0; CommandIndex < ApplyCount; ++CommandIndex)
	{
		const FAEPathHeatmapVisualCommand& Command = PendingCommands[CommandIndex];
		// Preserve the shared Grid XY orientation so material UVs need no hidden axis correction.
		const int32 PixelY = Command.Coordinate.Y;
		const FLinearColor EncodedValue(
			static_cast<float>(Command.EncodedValue.R) / 255.0f,
			static_cast<float>(Command.EncodedValue.G) / 255.0f,
			static_cast<float>(Command.EncodedValue.B) / 255.0f,
			static_cast<float>(Command.EncodedValue.A) / 255.0f);
		FCanvasTileItem Tile(
			FVector2D(Command.Coordinate.X, PixelY),
			FVector2D(1.0, 1.0),
			EncodedValue);
		Tile.BlendMode = SE_BLEND_Opaque;
		Canvas->Canvas->DrawItem(Tile);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);

	// Remove the applied stable prefix and rebuild lookup positions.
	PendingCommands.RemoveAt(0, ApplyCount, EAllowShrinking::No);
	RebuildPendingCommandLookup();
}

/* Clear transient visual state and turn off material sampling. */
void UAEPathHeatmapRendererComponent::ResetVisualOutput()
{
	check(IsInGameThread());
	PendingCommands.Reset();
	PendingCommandPositions.Reset();
	if (IsValid(PathHeatmapRenderTarget))
	{
		const FLinearColor NeutralVisualValue(128.0f / 255.0f, 128.0f / 255.0f, 0.0f, 0.0f);
		UKismetRenderingLibrary::ClearRenderTarget2D(
			this,
			PathHeatmapRenderTarget,
			NeutralVisualValue);
	}
	DisableMaterialOutput();
}

/* Replace all material outputs while preserving the shared Render Target contents. */
int32 UAEPathHeatmapRendererComponent::BindMaterialOutputs()
{
	check(IsInGameThread());
	DisableMaterialOutput();
	ReleaseMeshMaterialBindings();
	BoundLandscape.Reset();
	BoundMaterialOutputCount = 0;

	if (BindLandscapeMaterialParameters())
	{
		++BoundMaterialOutputCount;
	}
	int32 RejectedMeshBindingCount = 0;
	BoundMaterialOutputCount += BindMeshMaterialParameters(RejectedMeshBindingCount);

	if (BoundMaterialOutputCount == 0)
	{
		UE_LOG(
			LogAdaptiveEnv,
			Warning,
			TEXT("M6 Render Target is ready but no material output was bound. World=%s Owner=%s MeshRejected=%d"),
			*GetNameSafe(GetWorld()),
			*GetNameSafe(GetOwner()),
			RejectedMeshBindingCount);
	}
	else if (RejectedMeshBindingCount > 0)
	{
		UE_LOG(
			LogAdaptiveEnv,
			Warning,
			TEXT("M6 material binding partially succeeded. World=%s Owner=%s Bound=%d MeshRejected=%d"),
			*GetNameSafe(GetWorld()),
			*GetNameSafe(GetOwner()),
			BoundMaterialOutputCount,
			RejectedMeshBindingCount);
	}
	return BoundMaterialOutputCount;
}

/* Push the documented M6 texture and spatial constants into the optional Landscape material. */
bool UAEPathHeatmapRendererComponent::BindLandscapeMaterialParameters()
{
	if (!IsValid(TargetLandscape) || !IsValid(PathHeatmapRenderTarget))
	{
		return false;
	}
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	TargetLandscape->SetLandscapeMaterialTextureParameterValue(
		Settings->M6PathTextureParameterName,
		PathHeatmapRenderTarget);
	TargetLandscape->SetLandscapeMaterialVectorParameterValue(
		Settings->M6GridTransformParameterName,
		GridTransform);
	TargetLandscape->SetLandscapeMaterialScalarParameterValue(
		Settings->M6EnabledParameterName,
		1.0f);
	BoundLandscape = TargetLandscape;
	return true;
}

/* Create one owned MID for every valid unique Mesh material-slot binding. */
int32 UAEPathHeatmapRendererComponent::BindMeshMaterialParameters(
	int32& OutRejectedBindingCount)
{
	OutRejectedBindingCount = 0;
	int32 BoundCount = 0;
	for (const FAEPathMeshMaterialBinding& Binding : TargetMeshMaterials)
	{
		UMeshComponent* MeshComponent = Binding.MeshComponent.Get();
		const bool bDuplicate = RuntimeMeshMaterialBindings.ContainsByPredicate(
			[MeshComponent, &Binding](const FRuntimeMeshMaterialBinding& RuntimeBinding)
			{
				return RuntimeBinding.MeshComponent.Get() == MeshComponent
					&& RuntimeBinding.MaterialSlotIndex == Binding.MaterialSlotIndex;
			});
		if (!IsValid(MeshComponent)
			|| MeshComponent->GetWorld() != GetWorld()
			|| Binding.MaterialSlotIndex < 0
			|| Binding.MaterialSlotIndex >= MeshComponent->GetNumMaterials()
			|| bDuplicate)
		{
			++OutRejectedBindingCount;
			continue;
		}

		UMaterialInterface* OriginalMaterial = MeshComponent->GetMaterial(Binding.MaterialSlotIndex);
		if (!IsValid(OriginalMaterial)
			|| !SupportsRequiredMeshMaterialParameters(*OriginalMaterial))
		{
			++OutRejectedBindingCount;
			continue;
		}

		UMaterialInstanceDynamic* DynamicMaterial = MeshComponent->CreateDynamicMaterialInstance(
			Binding.MaterialSlotIndex,
			OriginalMaterial);
		if (!IsValid(DynamicMaterial))
		{
			++OutRejectedBindingCount;
			continue;
		}

		ApplyMeshMaterialParameters(*DynamicMaterial);
		FRuntimeMeshMaterialBinding& RuntimeBinding = RuntimeMeshMaterialBindings.AddDefaulted_GetRef();
		RuntimeBinding.MeshComponent = MeshComponent;
		RuntimeBinding.MaterialSlotIndex = Binding.MaterialSlotIndex;
		RuntimeBinding.OriginalMaterial = OriginalMaterial;
		RuntimeBinding.DynamicMaterial = DynamicMaterial;
		++BoundCount;
	}
	return BoundCount;
}

/* Apply the shared texture, World XY transform, and enable gate to one Mesh MID. */
void UAEPathHeatmapRendererComponent::ApplyMeshMaterialParameters(
	UMaterialInstanceDynamic& Material) const
{
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	Material.SetTextureParameterValue(Settings->M6PathTextureParameterName, PathHeatmapRenderTarget);
	Material.SetVectorParameterValue(Settings->M6GridTransformParameterName, GridTransform);
	Material.SetScalarParameterValue(Settings->M6EnabledParameterName, 1.0f);
}

/* Require exact runtime parameter names before replacing a user Mesh material slot. */
bool UAEPathHeatmapRendererComponent::SupportsRequiredMeshMaterialParameters(
	const UMaterialInterface& Material) const
{
	const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
	TArray<FMaterialParameterInfo> Parameters;
	TArray<FGuid> ParameterIds;

	Material.GetAllTextureParameterInfo(Parameters, ParameterIds);
	const bool bHasTexture = AEPathHeatmapRendererPrivate::ContainsParameterName(
		Parameters,
		Settings->M6PathTextureParameterName);
	Parameters.Reset();
	ParameterIds.Reset();
	Material.GetAllVectorParameterInfo(Parameters, ParameterIds);
	const bool bHasTransform = AEPathHeatmapRendererPrivate::ContainsParameterName(
		Parameters,
		Settings->M6GridTransformParameterName);
	Parameters.Reset();
	ParameterIds.Reset();
	Material.GetAllScalarParameterInfo(Parameters, ParameterIds);
	const bool bHasEnabled = AEPathHeatmapRendererPrivate::ContainsParameterName(
		Parameters,
		Settings->M6EnabledParameterName);
	return bHasTexture && bHasTransform && bHasEnabled;
}

/* Disable material sampling without clearing authoritative M6 state. */
void UAEPathHeatmapRendererComponent::DisableMaterialOutput()
{
	if (ALandscapeProxy* Landscape = BoundLandscape.Get())
	{
		const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
		Landscape->SetLandscapeMaterialScalarParameterValue(
			Settings->M6EnabledParameterName,
			0.0f);
	}
	const FName EnabledParameterName = GetDefault<UAdaptiveEnvSettings>()->M6EnabledParameterName;
	for (const FRuntimeMeshMaterialBinding& Binding : RuntimeMeshMaterialBindings)
	{
		if (UMaterialInstanceDynamic* DynamicMaterial = Binding.DynamicMaterial.Get())
		{
			DynamicMaterial->SetScalarParameterValue(EnabledParameterName, 0.0f);
		}
	}
}

/* Restore only slots that still contain a material instance owned by this Renderer. */
void UAEPathHeatmapRendererComponent::ReleaseMeshMaterialBindings()
{
	for (const FRuntimeMeshMaterialBinding& Binding : RuntimeMeshMaterialBindings)
	{
		UMeshComponent* MeshComponent = Binding.MeshComponent.Get();
		UMaterialInstanceDynamic* DynamicMaterial = Binding.DynamicMaterial.Get();
		UMaterialInterface* OriginalMaterial = Binding.OriginalMaterial.Get();
		if (IsValid(MeshComponent)
			&& IsValid(DynamicMaterial)
			&& IsValid(OriginalMaterial)
			&& Binding.MaterialSlotIndex >= 0
			&& Binding.MaterialSlotIndex < MeshComponent->GetNumMaterials()
			&& MeshComponent->GetMaterial(Binding.MaterialSlotIndex) == DynamicMaterial)
		{
			MeshComponent->SetMaterial(Binding.MaterialSlotIndex, OriginalMaterial);
		}
	}
	RuntimeMeshMaterialBindings.Reset();
}

/* Restore command positions after stable-prefix removal. */
void UAEPathHeatmapRendererComponent::RebuildPendingCommandLookup()
{
	PendingCommandPositions.Reset();
	for (int32 Position = 0; Position < PendingCommands.Num(); ++Position)
	{
		PendingCommandPositions.Add(PendingCommands[Position].CellIndex, Position);
	}
}

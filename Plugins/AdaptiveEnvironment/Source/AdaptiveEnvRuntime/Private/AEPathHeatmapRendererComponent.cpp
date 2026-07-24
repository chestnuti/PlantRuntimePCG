#include "AEPathHeatmapRendererComponent.h"

#include "AdaptiveEnvLog.h"
#include "AdaptiveEnvSettings.h"
#include "AdaptiveEnvWorldSubsystem.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "LandscapeProxy.h"

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
	return BindLandscapeMaterialParameters();
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

/* Clear transient visual state and turn off Landscape sampling. */
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

/* Push the documented M6 texture and spatial constants into the Landscape material. */
bool UAEPathHeatmapRendererComponent::BindLandscapeMaterialParameters()
{
	if (!IsValid(TargetLandscape) || !IsValid(PathHeatmapRenderTarget))
	{
		UE_LOG(
			LogAdaptiveEnv,
			Warning,
			TEXT("M6 visual output has no valid Landscape binding. Owner=%s"),
			*GetNameSafe(GetOwner()));
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
	return true;
}

/* Disable material sampling without clearing authoritative M6 state. */
void UAEPathHeatmapRendererComponent::DisableMaterialOutput()
{
	if (IsValid(TargetLandscape))
	{
		const UAdaptiveEnvSettings* Settings = GetDefault<UAdaptiveEnvSettings>();
		TargetLandscape->SetLandscapeMaterialScalarParameterValue(
			Settings->M6EnabledParameterName,
			0.0f);
	}
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

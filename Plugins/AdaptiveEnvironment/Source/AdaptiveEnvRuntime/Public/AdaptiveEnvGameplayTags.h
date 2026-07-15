#pragma once

#include "NativeGameplayTags.h"

namespace AdaptiveEnvGameplayTags
{
	/* Identifies normal agent movement. */
	ADAPTIVEENVRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Behaviour_Move);
	/* Identifies low-speed dwelling. */
	ADAPTIVEENVRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Behaviour_Dwell);
	/* Identifies fast agent movement. */
	ADAPTIVEENVRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Behaviour_Sprint);
	/* Identifies a resource collection event. */
	ADAPTIVEENVRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Behaviour_Collect);
	/* Identifies local combat activity. */
	ADAPTIVEENVRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Behaviour_Combat);
}

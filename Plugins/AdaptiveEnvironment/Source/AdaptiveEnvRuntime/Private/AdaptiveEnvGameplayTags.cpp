#include "AdaptiveEnvGameplayTags.h"

namespace AdaptiveEnvGameplayTags
{
	// Define native tags once so runtime and tests share stable identifiers.
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Behaviour_Move, "Behaviour.Move", "Normal movement.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Behaviour_Dwell, "Behaviour.Dwell", "Low-speed dwelling.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Behaviour_Sprint, "Behaviour.Sprint", "Fast movement.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Behaviour_Collect, "Behaviour.Collect", "Resource collection.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Behaviour_Combat, "Behaviour.Combat", "Local combat activity.");
}

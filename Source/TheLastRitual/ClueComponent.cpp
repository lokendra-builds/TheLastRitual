#include "ClueComponent.h"

#include "Engine/World.h"
#include "GhostGuideSubsystem.h"

bool UClueComponent::MarkFound()
{
	if (bFound)
	{
		return false;
	}

	UWorld* World = GetWorld();
	UGhostGuideSubsystem* Guide = World ? World->GetSubsystem<UGhostGuideSubsystem>() : nullptr;
	if (!Guide)
	{
		return false;
	}

	bFound = true;
	Guide->RecordClue(this);
	return true;
}

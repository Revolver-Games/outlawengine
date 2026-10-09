/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include <ScriptMotion/O3DE/ScriptMotionConfiguration.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

namespace Wanted::ScriptMotion::O3DE
{
    void ScriptMotionConfiguration::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<ScriptMotionConfiguration>()
                ->Version(2)
                ->Field("SkeletonPath", &ScriptMotionConfiguration::m_skeletonPath)
                ->Field("ClipPath", &ScriptMotionConfiguration::m_clipPath)
                ->Field("UseActorBindPose", &ScriptMotionConfiguration::m_useActorBindPose)
                ->Field("ActorSkeletonName", &ScriptMotionConfiguration::m_actorSkeletonName)
                ->Field("PlayOnActivation", &ScriptMotionConfiguration::m_playOnActivation)
                ->Field("Loop", &ScriptMotionConfiguration::m_loop)
                ->Field("PlaybackSpeed", &ScriptMotionConfiguration::m_playbackSpeed)
                ->Field("BlendInSeconds", &ScriptMotionConfiguration::m_blendInSeconds)
                ->Field("SampleRate", &ScriptMotionConfiguration::m_sampleRate);

            if (AZ::EditContext* edit = serialize->GetEditContext())
            {
                edit->Class<ScriptMotionConfiguration>("ScriptMotion settings", "Source JSON and native motion playback")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ScriptMotionConfiguration::m_useActorBindPose,
                        "Use Actor bind pose", "Read the exact joints and bind transforms from the loaded Actor; ignore Skeleton JSON")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ScriptMotionConfiguration::m_actorSkeletonName,
                        "Actor skeleton name", "Must match the clip's skeleton field when Use Actor bind pose is enabled")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ScriptMotionConfiguration::m_skeletonPath,
                        "Skeleton JSON", "FileIO path to validated skeleton JSON; unused when Use Actor bind pose is enabled")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ScriptMotionConfiguration::m_clipPath,
                        "Animation JSON", "FileIO path to ScriptMotion clip JSON")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ScriptMotionConfiguration::m_playOnActivation,
                        "Play on activation", "Start when the game entity's Actor is ready")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ScriptMotionConfiguration::m_loop,
                        "Loop", "Repeat this motion")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ScriptMotionConfiguration::m_playbackSpeed,
                        "Playback speed", "Zero pauses; reverse playback is not supported")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 10.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ScriptMotionConfiguration::m_blendInSeconds,
                        "Blend in", "Native EMotionFX blend-in duration in seconds")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 10.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &ScriptMotionConfiguration::m_sampleRate,
                        "Bake samples per second", "Native motion sampling, 30 to 240 Hz; smooth curves are approximated")
                    ->Attribute(AZ::Edit::Attributes::Min, 30.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 240.0f);
            }
        }
    }
}

/*
 * Copyright (c) 2026 Wanted Engine contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#include <AzTest/AzTest.h>
#include <AzTest/Utils.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <Tests/SystemComponentFixture.h>
#include <EMotionFX/Source/Actor.h>
#include <EMotionFX/Source/ActorInstance.h>
#include <EMotionFX/Source/Motion.h>
#include <EMotionFX/Source/MotionData/MotionData.h>
#include <EMotionFX/Source/MotionEventTable.h>
#include <EMotionFX/Source/MotionEventTrack.h>
#include <EMotionFX/Source/MotionInstance.h>
#include <EMotionFX/Source/MotionSystem.h>
#include <EMotionFX/Source/Pose.h>
#include <EMotionFX/Source/Skeleton.h>
#include <EMotionFX/Source/TransformData.h>
#include <O3DE/ScriptMotionPlayback.h>

namespace Wanted::ScriptMotion::O3DE
{
    // Real EMotion FX Actor/MotionSystem and FileIO; no stand-in engine API.
    // These tests require the O3DE SDK and are NOT part of the portable test executable.
    class ScriptMotionPlaybackTests : public EMotionFX::SystemComponentFixture
    {
    protected:
        void SetUp() override
        {
            EMotionFX::SystemComponentFixture::SetUp();
            m_directory = AZStd::make_unique<AZ::Test::ScopedAutoTempDirectory>();
            m_configuration.m_clipPath = m_directory->Resolve("wave.json").c_str();
            m_configuration.m_skeletonPath = m_directory->Resolve("rig.json").c_str();
            m_configuration.m_actorSkeletonName = "native_test";
            m_configuration.m_blendInSeconds = 0;
            m_configuration.m_loop = false;
            Write(m_configuration.m_skeletonPath, R"({"formatVersion":1,"name":"native_test","bones":[
                {"name":"root","parent":null}, {"name":"hand","parent":"root","translation":[1,0,0]}]})");
            Write(m_configuration.m_clipPath, R"({"formatVersion":1,"name":"wave","skeleton":"native_test",
                "duration":2,"tracks":[{"bone":"hand","keys":[{"time":0,"translation":[1,0,0]},
                {"time":2,"translation":[1,0,2]}]}],"events":[{"time":1,"name":"greeting"}]})");
            m_actor = AZStd::make_unique<EMotionFX::Actor>("ScriptMotion test");
            m_actor->AddNode(0, "root");
            m_actor->AddNode(1, "hand", 0);
            m_actor->GetSkeleton()->UpdateNodeIndexValues(0);
            m_actor->ResizeTransformData();
            m_actor->GetBindPose()->SetLocalSpaceTransform(0, EMotionFX::Transform::CreateIdentity());
            auto hand = EMotionFX::Transform::CreateIdentity();
            hand.m_position = AZ::Vector3(1, 0, 0);
            m_actor->GetBindPose()->SetLocalSpaceTransform(1, hand);
            m_actor->PostCreateInit(false, false);
            m_instance = EMotionFX::ActorInstance::Create(m_actor.get());
            m_playback = AZStd::make_unique<ScriptMotionPlayback>();
            m_playback->SetActor(m_instance);
        }

        void TearDown() override
        {
            m_playback.reset();
            if (m_instance)
            {
                m_instance->Destroy();
                m_instance = nullptr;
            }
            m_actor.reset();
            m_directory.reset();
            EMotionFX::SystemComponentFixture::TearDown();
        }

        void Write(const AZStd::string& path, AZStd::string_view text)
        {
            AZ::IO::FileIOStream file(path.c_str(), AZ::IO::OpenMode::ModeWrite);
            ASSERT_TRUE(file.IsOpen());
            ASSERT_EQ(file.Write(text.size(), text.data()), text.size());
        }

        EMotionFX::MotionInstance* Live()
        {
            return m_instance->GetMotionSystem()->GetMotionInstance(0);
        }

        ScriptMotionConfiguration m_configuration;
        AZStd::unique_ptr<AZ::Test::ScopedAutoTempDirectory> m_directory;
        AZStd::unique_ptr<EMotionFX::Actor> m_actor;
        EMotionFX::ActorInstance* m_instance = nullptr;
        AZStd::unique_ptr<ScriptMotionPlayback> m_playback;
    };

    TEST_F(ScriptMotionPlaybackTests, TextClipProducesNativeJointPoseAndEventTrack)
    {
        ASSERT_TRUE(m_playback->Play(m_configuration)) << m_playback->GetLastError().c_str();
        ASSERT_EQ(m_instance->GetMotionSystem()->GetNumMotionInstances(), 1);
        EXPECT_TRUE(Live()->GetMotion()->GetMotionData()->VerifyIntegrity());
        EXPECT_EQ(Live()->GetMotion()->GetMotionData()->GetNumJoints(), 2);
        ASSERT_TRUE(m_playback->Seek(1));
        const auto& transform = m_instance->GetTransformData()->GetCurrentPose()->GetLocalSpaceTransform(1);
        EXPECT_TRUE(transform.m_position.IsClose(AZ::Vector3(1, 0, 1), 0.0001f));
        auto* track = Live()->GetMotion()->GetEventTable()->FindTrackByName("ScriptMotion");
        ASSERT_NE(track, nullptr);
        EXPECT_EQ(track->GetNumEvents(), 1);
    }

    TEST_F(ScriptMotionPlaybackTests, InvalidReloadPreservesLiveMotionAndPlayhead)
    {
        ASSERT_TRUE(m_playback->Play(m_configuration));
        ASSERT_TRUE(m_playback->Seek(0.75f));
        auto* previous = Live();
        Write(m_configuration.m_clipPath, "{broken");
        EXPECT_FALSE(m_playback->Play(m_configuration));
        EXPECT_EQ(Live(), previous);
        EXPECT_FLOAT_EQ(Live()->GetCurrentTime(), 0.75f);
        EXPECT_FALSE(m_playback->GetLastError().empty());
    }

    TEST_F(ScriptMotionPlaybackTests, CapturedActorRigNeedsNoSkeletonFile)
    {
        m_configuration.m_useActorBindPose = true;
        m_configuration.m_skeletonPath.clear();
        ASSERT_TRUE(m_playback->Play(m_configuration)) << m_playback->GetLastError().c_str();
        ASSERT_TRUE(m_playback->Seek(1.5f));
        EXPECT_FLOAT_EQ(Live()->GetDuration(), 2);
        m_configuration.m_actorSkeletonName = "wrong_rig";
        EXPECT_FALSE(m_playback->Play(m_configuration));
        EXPECT_FLOAT_EQ(Live()->GetCurrentTime(), 1.5f);
    }

    TEST_F(ScriptMotionPlaybackTests, PausedStartHasVisibleWeightAndSeekClearsFinishedState)
    {
        m_configuration.m_playbackSpeed = 0;
        m_configuration.m_blendInSeconds = 1;
        ASSERT_TRUE(m_playback->Play(m_configuration));
        EXPECT_FLOAT_EQ(Live()->GetWeight(), 1);
        ASSERT_TRUE(m_playback->Seek(1));
        EXPECT_FLOAT_EQ(Live()->GetLastCurrentTime(), 1); // no skipped event interval
        Live()->SetNumCurrentLoops(1);
        Live()->SetIsFrozen(true);
        ASSERT_TRUE(m_playback->Seek(0.5f));
        EXPECT_FALSE(Live()->GetIsFrozen());
        EXPECT_FALSE(Live()->GetHasEnded());
        ASSERT_TRUE(m_playback->SetPlaybackSpeed(1));
        m_instance->UpdateTransformations(0.1f, true, true);
        EXPECT_NEAR(Live()->GetCurrentTime(), 0.6f, 0.0001f);
    }

    TEST_F(ScriptMotionPlaybackTests, StopReloadAndActorDetachReleaseOwnedInstances)
    {
        for (int i = 0; i < 8; ++i)
        {
            ASSERT_TRUE(m_playback->Play(m_configuration));
            ASSERT_TRUE(m_playback->Play(m_configuration));
            EXPECT_EQ(m_instance->GetMotionSystem()->GetNumMotionInstances(), 1);
            m_playback->Stop();
            m_playback->Stop();
            EXPECT_EQ(m_instance->GetMotionSystem()->GetNumMotionInstances(), 0);
        }
        ASSERT_TRUE(m_playback->Play(m_configuration));
        m_playback->SetActor(nullptr);
        EXPECT_EQ(m_instance->GetMotionSystem()->GetNumMotionInstances(), 0);
        EXPECT_FALSE(m_playback->Seek(0));
        EXPECT_FALSE(m_playback->Play(m_configuration));
        m_playback->SetActor(m_instance);
        EXPECT_TRUE(m_playback->Play(m_configuration));
    }

    TEST_F(ScriptMotionPlaybackTests, ExternallyRemovedMotionIsNeverDereferenced)
    {
        ASSERT_TRUE(m_playback->Play(m_configuration));
        m_instance->GetMotionSystem()->RemoveMotionInstance(Live());
        EXPECT_FLOAT_EQ(m_playback->GetDuration(), 0);
        EXPECT_FALSE(m_playback->SetPlaybackSpeed(1));
        m_playback->Stop();
        EXPECT_TRUE(m_playback->Play(m_configuration));
    }
}

AZ_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);

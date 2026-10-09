/*
 * Copyright (c) Contributors to the Wanted Engine Project.
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include <cstdint>
#include <string_view>

namespace Wanted
{
    // A small executable mission, not the future MissionScript language.
    // No engine types are required: the exact transition rules can be tested headlessly.
    class FirstErrand
    {
    public:
        enum class Stage : std::uint32_t
        {
            NotStarted = 0,
            MeetAda = 1,
            FindSatchel = 2,
            ReturnToAda = 3,
            Complete = 4
        };

        enum class Transition
        {
            Ignored,
            ObjectiveChanged,
            Completed
        };

        bool Start()
        {
            if (m_stage != Stage::NotStarted)
            {
                return false;
            }
            m_stage = Stage::MeetAda;
            return true;
        }

        void Reset()
        {
            m_stage = Stage::NotStarted;
        }

        Transition Interact(std::string_view target)
        {
            if (m_stage == Stage::MeetAda && target == "ada_mercer")
            {
                m_stage = Stage::FindSatchel;
                return Transition::ObjectiveChanged;
            }
            if (m_stage == Stage::FindSatchel && target == "mail_satchel")
            {
                m_stage = Stage::ReturnToAda;
                return Transition::ObjectiveChanged;
            }
            if (m_stage == Stage::ReturnToAda && target == "ada_mercer")
            {
                m_stage = Stage::Complete;
                return Transition::Completed;
            }
            return Transition::Ignored;
        }

        Stage GetStage() const
        {
            return m_stage;
        }

        std::string_view GetObjective() const
        {
            switch (m_stage)
            {
            case Stage::NotStarted: return "No delivery accepted.";
            case Stage::MeetAda: return "Speak to Ada Mercer at Mercy Crossing station.";
            case Stage::FindSatchel: return "Recover the mail satchel beside the water tower.";
            case Stage::ReturnToAda: return "Return the mail satchel to Ada Mercer.";
            case Stage::Complete: return "Unclaimed Post completed.";
            }
            return "Unknown mission state.";
        }

    private:
        Stage m_stage = Stage::NotStarted;
    };
} // namespace Wanted

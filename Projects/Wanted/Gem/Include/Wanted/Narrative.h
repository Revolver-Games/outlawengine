/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <set>

namespace Wanted::Narrative
{
    template<class T> struct Result
    {
        T value{};
        std::string error;
        [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
    };
    enum class Stage { NotStarted, Active, Complete, Failed };
    struct Objective
    {
        std::string id, text, event, target, requiresFlag, setsFlag;
        int count = 1;
    };
    struct Choice
    {
        std::string id, text, next, requiresFlag, setsFlag, relationship;
        int reputationDelta = 0;
    };
    struct Dialogue { std::string id, speaker, text, requiresFlag; std::vector<Choice> choices; };
    struct Definition
    {
        std::string id, chapter, requiresFlag, completedFlag, cutscene;
        int revision = 1, reward = 0;
        std::vector<std::string> flags, relationships;
        std::vector<Objective> objectives;
        std::vector<Dialogue> dialogues;
    };
    struct MissionScriptState { Stage stage = Stage::NotStarted; std::size_t objective = 0; int count = 0; };
    struct StoryScriptState
    {
        std::set<std::string> flags, decisions;
        std::map<std::string, int> reputation;
        std::string dialogue;
        int credits = 0; // Local save-owned reward balance, not an external economy transaction.
    };
    struct Change
    {
        bool accepted = false, objectiveChanged = false, completed = false;
        int creditsAdded = 0;
        std::string cutscene;
    };
    [[nodiscard]] Result<Definition> ParseDefinition(std::string_view json);

    // Single-player trusted host events; no distance/inventory/network authority is inferred.
    // The original FirstErrand implementation remains a separate supported entry point.
    class Session
    {
    public:
        [[nodiscard]] std::string Load(std::string_view definitionJson);
        [[nodiscard]] Result<Change> Start();
        [[nodiscard]] Result<Change> Event(std::string_view kind, std::string_view target);
        [[nodiscard]] Result<Change> BeginDialogue(std::string_view id);
        [[nodiscard]] Result<Change> Choose(std::string_view id);
        [[nodiscard]] Result<Change> CloseDialogue();
        [[nodiscard]] Result<Change> Fail();
        [[nodiscard]] std::string ObjectiveText() const;
        [[nodiscard]] std::vector<Choice> Choices() const;
        [[nodiscard]] std::string DialogueJson() const;
        [[nodiscard]] std::string Save() const;
        [[nodiscard]] std::string Restore(std::string_view checkpointJson);
        [[nodiscard]] const MissionScriptState& Mission() const { return m_mission; }
        [[nodiscard]] const StoryScriptState& Story() const { return m_story; }
        [[nodiscard]] const Definition& Content() const { return m_definition; }
    private:
        struct Command { std::string action, kind, target; };
        [[nodiscard]] bool Ready() const { return m_loaded && m_journal.size() < 4096; }
        [[nodiscard]] bool Allowed(const std::string& flag) const { return flag.empty() || m_story.flags.contains(flag); }
        [[nodiscard]] const Dialogue* Node(std::string_view id) const;
        Change AdvanceMission(std::string_view kind, std::string_view target);
        Definition m_definition;
        MissionScriptState m_mission;
        StoryScriptState m_story;
        std::vector<Command> m_journal;
        bool m_loaded = false;
    };
}

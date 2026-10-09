/* Copyright (c) 2026 Revolver Games contributors.
 * SPDX-License-Identifier: Apache-2.0 OR MIT */
#include <Wanted/Narrative.h>
#define JSON_NOEXCEPTION 1
#include <ThirdParty/nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace Wanted::Narrative
{
    namespace
    {
        using Json = nlohmann::json;
        bool Name(std::string_view s)
        {
            return !s.empty() && s.size() <= 128 && std::all_of(s.begin(), s.end(), [](unsigned char c)
            { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; });
        }
        bool EventName(std::string_view s)
        { return s == "interact" || s == "collect" || s == "enter" || s == "dialogue" || s == "signal"; }
        bool Fields(const Json& j, std::initializer_list<std::string_view> allowed)
        {
            if (!j.is_object()) { return false; }
            for (auto it=j.begin(); it!=j.end(); ++it)
            { if (std::find(allowed.begin(), allowed.end(), it.key()) == allowed.end()) { return false; } }
            return true;
        }
        bool String(const Json& j, const char* field, std::string& out, bool optional = false)
        {
            const auto it=j.find(field);
            if (it==j.end()) { return optional; }
            if (!it->is_string()) { return false; }
            out=it->get<std::string>();
            return out.size() <= 2048 && out.find('\0') == std::string::npos;
        }
        bool Integer(const Json& j, const char* field, int& out, int low, int high, bool optional = false)
        {
            const auto it=j.find(field);
            if (it==j.end()) { return optional; }
            if (!it->is_number_integer()) { return false; }
            const double n=it->get<double>();
            if (!std::isfinite(n) || n < low || n > high) { return false; }
            out=static_cast<int>(n); return true;
        }
        Result<Json> Document(std::string_view text)
        {
            if (text.empty() || text.size() > 1024*1024) { return {{}, "Document must be 1 byte to 1 MiB"}; }
            std::size_t depth=0; bool quoted=false, escaped=false;
            for (const char c:text)
            {
                if (quoted)
                { if (escaped) { escaped=false; } else if (c=='\\') { escaped=true; } else if(c=='"') { quoted=false; } }
                else if(c=='"') { quoted=true; }
                else if(c=='[' || c=='{') { if(++depth>16) { return {{}, "JSON nesting exceeds 16"}; } }
                else if(c==']' || c=='}') { if(!depth) { return {{}, "Malformed JSON nesting"}; } --depth; }
            }
            bool duplicate=false;
            std::vector<std::unordered_set<std::string>> keys;
            auto callback=[&](int, Json::parse_event_t e, Json& value)
            {
                if(e==Json::parse_event_t::object_start) { keys.emplace_back(); }
                else if(e==Json::parse_event_t::key && !keys.empty())
                { if(!keys.back().insert(value.get<std::string>()).second) { duplicate=true; } }
                else if(e==Json::parse_event_t::object_end && !keys.empty()) { keys.pop_back(); }
                return true;
            };
            auto result=Json::parse(text.begin(),text.end(),callback,false);
            if(result.is_discarded() || !result.is_object() || duplicate) { return {{}, "Invalid JSON or duplicate object key"}; }
            return {std::move(result),{}};
        }
        bool Array(const Json& j, const char* field, std::size_t maximum)
        { return j.contains(field) && j[field].is_array() && j[field].size()<=maximum; }
        bool Names(const Json& j, const char* field, std::size_t maximum, std::vector<std::string>& out)
        {
            if(!Array(j,field,maximum)) { return false; }
            std::set<std::string> unique;
            for(const auto& value:j[field])
            {
                if(!value.is_string()) { return false; }
                auto name=value.get<std::string>();
                if(!Name(name) || !unique.insert(name).second) { return false; }
                out.push_back(std::move(name));
            }
            return true;
        }
    }
    Result<Definition> ParseDefinition(std::string_view text)
    {
        auto document=Document(text);
        if(!document) { return {{},document.error}; }
        const auto& j=document.value;
        Definition d; int version=0;
        if(!Fields(j,{"formatVersion","id","chapter","revision","flags","relationships","mission","dialogue"})
            || !Integer(j,"formatVersion",version,1,1) || !String(j,"id",d.id) || !Name(d.id)
            || !String(j,"chapter",d.chapter) || !Name(d.chapter) || !Integer(j,"revision",d.revision,1,1000000000)
            || !Names(j,"flags",128,d.flags) || !Names(j,"relationships",32,d.relationships))
        { return {{},"Invalid narrative header/declared variables"}; }
        const auto declared=[&](const std::string& flag)
        { return flag.empty() || std::find(d.flags.begin(),d.flags.end(),flag)!=d.flags.end(); };
        if(!j.contains("mission") || !Fields(j["mission"],{"requires","completedFlag","cutscene","reward","objectives"}))
        { return {{},"Invalid mission fields"}; }
        const auto& m=j["mission"];
        if(!String(m,"requires",d.requiresFlag,true) || !String(m,"completedFlag",d.completedFlag,true)
            || !String(m,"cutscene",d.cutscene,true) || (!d.cutscene.empty() && !Name(d.cutscene))
            || !declared(d.requiresFlag) || !declared(d.completedFlag) || !Integer(m,"reward",d.reward,0,1000000)
            || !Array(m,"objectives",32) || m["objectives"].empty()) { return {{},"Invalid mission settings"}; }
        std::set<std::string> objectives;
        for(const auto& value:m["objectives"])
        {
            Objective o;
            if(!Fields(value,{"id","text","event","target","count","requires","sets"}) || !String(value,"id",o.id)
                || !Name(o.id) || !objectives.insert(o.id).second || !String(value,"text",o.text) || o.text.empty()
                || !String(value,"event",o.event) || !EventName(o.event) || !String(value,"target",o.target) || !Name(o.target)
                || !Integer(value,"count",o.count,1,32,true) || !String(value,"requires",o.requiresFlag,true)
                || !String(value,"sets",o.setsFlag,true) || !declared(o.requiresFlag) || !declared(o.setsFlag))
            { return {{},"Invalid mission objective"}; }
            d.objectives.push_back(std::move(o));
        }
        if(!Array(j,"dialogue",128)) { return {{},"Invalid dialogue array"}; }
        std::set<std::string> nodes, choices;
        for(const auto& value:j["dialogue"])
        {
            Dialogue node;
            if(!Fields(value,{"id","speaker","text","requires","choices"}) || !String(value,"id",node.id)
                || !Name(node.id) || !nodes.insert(node.id).second || !String(value,"speaker",node.speaker) || node.speaker.empty()
                || !String(value,"text",node.text) || node.text.empty() || !String(value,"requires",node.requiresFlag,true)
                || !declared(node.requiresFlag) || !Array(value,"choices",16)) { return {{},"Invalid dialogue node"}; }
            for(const auto& item:value["choices"])
            {
                Choice c;
                if(!Fields(item,{"id","text","next","requires","sets","relationship","reputationDelta"})
                    || !String(item,"id",c.id) || !Name(c.id) || !choices.insert(c.id).second || choices.size()>512
                    || !String(item,"text",c.text) || c.text.empty() || !String(item,"next",c.next,true)
                    || (!c.next.empty() && !Name(c.next)) || !String(item,"requires",c.requiresFlag,true)
                    || !String(item,"sets",c.setsFlag,true) || !declared(c.requiresFlag) || !declared(c.setsFlag)
                    || !String(item,"relationship",c.relationship,true) || !Integer(item,"reputationDelta",c.reputationDelta,-20,20,true)
                    || (c.relationship.empty() && c.reputationDelta!=0)
                    || (!c.relationship.empty() && std::find(d.relationships.begin(),d.relationships.end(),c.relationship)==d.relationships.end()))
                { return {{},"Invalid dialogue choice or undeclared relationship"}; }
                node.choices.push_back(std::move(c));
            }
            d.dialogues.push_back(std::move(node));
        }
        for(const auto& node:d.dialogues)
        { for(const auto& choice:node.choices) { if(!choice.next.empty() && !nodes.contains(choice.next)) { return {{},"Choice points to missing dialogue"}; } } }
        std::set<std::string> dialogueObjectives;
        for(const auto& objective:d.objectives)
        {
            if(objective.event=="dialogue" && (!choices.contains(objective.target) || objective.count!=1
                || !dialogueObjectives.insert(objective.target).second))
            { return {{},"Dialogue objectives require one unique, existing choice"}; }
        }
        return {std::move(d),{}};
    }

    std::string Session::Load(std::string_view text)
    {
        auto d=ParseDefinition(text);
        if(!d) { return d.error; }
        m_definition=std::move(d.value); m_mission={}; m_story={}; m_journal.clear(); m_loaded=true;
        for(const auto& name:m_definition.relationships) { m_story.reputation.emplace(name,0); }
        return {};
    }
    const Dialogue* Session::Node(std::string_view id) const
    { for(const auto& node:m_definition.dialogues) { if(node.id==id) { return &node; } } return nullptr; }
    Result<Change> Session::Start()
    {
        if(!Ready()) { return {{},"No content loaded or command journal is full"}; }
        if(m_mission.stage!=Stage::NotStarted || !Allowed(m_definition.requiresFlag)) { return {}; }
        m_journal.push_back({"start",{}, {}}); m_mission.stage=Stage::Active;
        return {{true,true,false,0,{}},{}};
    }
    Change Session::AdvanceMission(std::string_view kind,std::string_view target)
    {
        if(m_mission.stage!=Stage::Active) { return {}; }
        const auto& objective=m_definition.objectives[m_mission.objective];
        if(objective.event!=kind || objective.target!=target || !Allowed(objective.requiresFlag)) { return {}; }
        Change change; change.accepted=true;
        if(++m_mission.count<objective.count) { return change; }
        if(!objective.setsFlag.empty()) { m_story.flags.insert(objective.setsFlag); }
        ++m_mission.objective; m_mission.count=0; change.objectiveChanged=true;
        if(m_mission.objective==m_definition.objectives.size())
        {
            m_mission.stage=Stage::Complete; change.completed=true;
            change.creditsAdded=m_definition.reward; m_story.credits+=m_definition.reward;
            if(!m_definition.completedFlag.empty()) { m_story.flags.insert(m_definition.completedFlag); }
            change.cutscene=m_definition.cutscene;
        }
        return change;
    }
    Result<Change> Session::Event(std::string_view kind,std::string_view target)
    {
        if(!Ready()) { return {{},"No content loaded or command journal is full"}; }
        // Dialogue objectives can only come from an actual accepted choice.
        if(!EventName(kind) || kind=="dialogue" || !Name(target)) { return {{},"Invalid host event; use Choose for dialogue"}; }
        auto change=AdvanceMission(kind,target);
        if(change.accepted) { m_journal.push_back({"event",std::string(kind),std::string(target)}); }
        return {std::move(change),{}};
    }
    Result<Change> Session::BeginDialogue(std::string_view id)
    {
        if(!Ready()) { return {{},"No content loaded or command journal is full"}; }
        const auto* node=Node(id);
        if(!node || !Allowed(node->requiresFlag)) { return {{},"Dialogue is missing or locked"}; }
        if(!m_story.dialogue.empty()) { return {{},"Close the active dialogue first"}; }
        m_story.dialogue=std::string(id); m_journal.push_back({"begin",{},std::string(id)});
        return {{true,false,false,0,{}},{}};
    }
    std::vector<Choice> Session::Choices() const
    {
        std::vector<Choice> visible;
        if(const auto* node=Node(m_story.dialogue))
        {
            for(const auto& c:node->choices)
            {
                bool objectiveAllowed=true;
                for(std::size_t i=0;i<m_definition.objectives.size();++i)
                {
                    const auto& objective=m_definition.objectives[i];
                    if(objective.event=="dialogue" && objective.target==c.id
                        && (m_mission.stage!=Stage::Active || m_mission.objective!=i || !Allowed(objective.requiresFlag)))
                    { objectiveAllowed=false; }
                }
                if(objectiveAllowed && Allowed(c.requiresFlag) && !m_story.decisions.contains(c.id)) { visible.push_back(c); }
            }
        }
        return visible;
    }
    Result<Change> Session::Choose(std::string_view id)
    {
        if(!Ready()) { return {{},"No content loaded or command journal is full"}; }
        const auto choices=Choices();
        auto found=std::find_if(choices.begin(),choices.end(),[&](const auto& c){return c.id==id;});
        if(found==choices.end()) { return {{},"Choice missing, locked or already selected"}; }
        // Validate the next-node gate against the post-choice flags before changing state.
        const auto* next=Node(found->next);
        if(next && !Allowed(next->requiresFlag) && next->requiresFlag!=found->setsFlag)
        { return {{},"Next dialogue node is locked"}; }
        m_journal.push_back({"choose",{},std::string(id)});
        if(!found->setsFlag.empty()) { m_story.flags.insert(found->setsFlag); }
        if(!found->relationship.empty())
        {
            auto& value=m_story.reputation[found->relationship];
            value=std::clamp(value+found->reputationDelta,-100,100);
        }
        m_story.decisions.insert(found->id); m_story.dialogue=found->next;
        auto change=AdvanceMission("dialogue",found->id); change.accepted=true;
        return {std::move(change),{}};
    }
    Result<Change> Session::CloseDialogue()
    {
        if(!Ready()) { return {{},"No content loaded or command journal is full"}; }
        if(m_story.dialogue.empty()) { return {}; }
        m_story.dialogue.clear(); m_journal.push_back({"close",{}, {}});
        return {{true,false,false,0,{}},{}};
    }
    Result<Change> Session::Fail()
    {
        if(!Ready()) { return {{},"No content loaded or command journal is full"}; }
        if(m_mission.stage!=Stage::Active) { return {}; }
        m_mission.stage=Stage::Failed; m_story.dialogue.clear(); m_journal.push_back({"fail",{}, {}});
        return {{true,true,false,0,{}},{}};
    }
    std::string Session::ObjectiveText() const
    {
        if(m_mission.stage==Stage::Complete) { return "Mission complete."; }
        if(m_mission.stage==Stage::Failed) { return "Mission failed."; }
        if(m_mission.stage!=Stage::Active) { return "No active mission."; }
        return m_definition.objectives[m_mission.objective].text;
    }
    std::string Session::DialogueJson() const
    {
        const auto* node=Node(m_story.dialogue);
        if(!node) { return "{}"; }
        Json choices=Json::array();
        for(const auto& choice:Choices()) { choices.push_back({{"id",choice.id},{"text",choice.text}}); }
        return Json{{"id",node->id},{"speaker",node->speaker},{"text",node->text},{"choices",choices}}.dump();
    }
    std::string Session::Save() const
    {
        if(!m_loaded) { return {}; }
        Json commands=Json::array();
        for(const auto& command:m_journal) { commands.push_back({{"action",command.action},{"kind",command.kind},{"target",command.target}}); }
        return Json{{"formatVersion",1},{"content",m_definition.id},{"revision",m_definition.revision},{"commands",commands}}.dump();
    }
    std::string Session::Restore(std::string_view text)
    {
        if(!m_loaded) { return "Load matching content before restoring"; }
        auto parsed=Document(text);
        if(!parsed) { return parsed.error; }
        const auto& j=parsed.value;
        int version=0,revision=0; std::string content;
        if(!Fields(j,{"formatVersion","content","revision","commands"}) || !Integer(j,"formatVersion",version,1,1)
            || !String(j,"content",content) || content!=m_definition.id || !Integer(j,"revision",revision,1,1000000000)
            || revision!=m_definition.revision || !Array(j,"commands",4096)) { return "Checkpoint content/revision mismatch"; }
        Session copy;
        copy.m_definition=m_definition; copy.m_loaded=true;
        for(const auto& name:m_definition.relationships) { copy.m_story.reputation.emplace(name,0); }
        for(const auto& item:j["commands"])
        {
            Command c;
            if(!Fields(item,{"action","kind","target"}) || !String(item,"action",c.action)
                || !String(item,"kind",c.kind) || !String(item,"target",c.target)) { return "Invalid checkpoint command"; }
            Result<Change> result;
            if(c.action=="event") { result=copy.Event(c.kind,c.target); }
            else if(!c.kind.empty()) { return "Unexpected command kind"; }
            else if(c.action=="begin") { result=copy.BeginDialogue(c.target); }
            else if(c.action=="choose") { result=copy.Choose(c.target); }
            else if(!c.target.empty()) { return "Unexpected command target"; }
            else if(c.action=="start") { result=copy.Start(); }
            else if(c.action=="close") { result=copy.CloseDialogue(); }
            else if(c.action=="fail") { result=copy.Fail(); }
            else { return "Unknown checkpoint command"; }
            if(!result || !result.value.accepted) { return "Invalid checkpoint transition: "+c.action; }
        }
        *this=std::move(copy); // No callbacks/cues escape replay; replace the whole local state atomically.
        return {};
    }
}

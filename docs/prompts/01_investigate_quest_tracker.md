Use the repository rules and skills.
Investigate whether a complete safe in-game quest tracker can be built in this GWToolboxpp fork. Do not modify production code.
Use investigate-repository, inspect-gwca-state, and plan-toolbox-feature.
Inspect QuestModule, ActiveQuestWidget, CompletionWindow, relevant GWCA QuestManager/StoC/UI APIs, character/account identification, map callbacks, persistence, registration, and build/test workflow.
Determine reliability of current character/account, quest log, selected quest, objective text/flags, quest added/removed/abandoned/rewarded, permanent completion, mission/bonus completion, character switching, and reconstruction while Toolbox was offline.
Never treat disappearance as completion.
Create docs/quest-tracker/repository-investigation.md, data-availability.md, proposed-architecture.md, and plans/quest-tracker-mvp.md.
Plan phases: 1 read-only list/objectives; 2 per-character persistence/history; 3 export/manual corrections/Codex compatibility.
Cite concrete files and symbols. End with STOP. Do not implement.

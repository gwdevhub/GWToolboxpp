# Governance rendezés és visszatérés a bétához

Állapotfelvétel: 2026-09-30. Ez az operatív rendezés követési lapja; nem build-, játékbeli vagy kiadási PASS.

## Ellenőrzött GitHub állapot a rendezés indulásakor

| Repo | Alapág / SHA | Nyitott PR |
| --- | --- | --- |
| vinogitz/GWToolboxpp | master / 99b7b612a33228aa6e4b4efd718d01bc5536bf30 | 0 a vizsgálatkor |
| vinogitz/guildwarscodex | main / 03a0ae0f86b217eab523e66649429fd722b9ff08 | 0 a vizsgálatkor |

A két `docs/contracts/quest_progress_contract_v1.md` tartalma a vizsgálatkor azonos volt. A Windows gépen lévő repo-, branch-, worktree- és fájlállapot innen nem lett ellenőrizve.

## A rendezési szelet

A `chore/ai-governance-consolidation` branch külön létezik a két repóban. A PR-ek a fenti fork alapágakra készülnek; nincs közöttük Contract- vagy runtime-függőség. A rendezés az angol technikai governance-ra épít, központi nyelvi szabállyal, repo/branch láthatósággal, Cursor belépési paranccsal és review handoff routinggal. A jelenlegi emberi kérést tekintjük e dokumentációs szelet jóváhagyott scope-jának.

Dokumentációs önellenőrzés: routing, relatív linkek, változott fájlok és a nyelvi szabály egyezése. Teljes Flutter/C++ build nem szükséges ehhez a szelethez. Független review és emberi merge-döntés külön szükséges; az önellenőrzés ezeket nem helyettesíti.

## Az AI munkafolyamat operatív ellenőrzése

- [ ] Mindkét governance PR független review-ja és emberi merge-döntése lezárult.
- [ ] Cursor mindkét lokális repóban kiírta a folder/branch/HEAD/upstream/ahead-behind/tree táblát.
- [ ] Mindkét tiszta lokális alapág tartalmazza az elfogadott governance commitot; eltérés vagy saját munka esetén megőrzési terv készült.
- [ ] A lokális branchek és worktree-k leltára elkészült; az aktív/saját munka és az összevont történet megkülönböztethető. A leltár nem törlési engedély.
- [ ] Egy read-only `start-task` próba új branch nélkül működött.
- [ ] A handoff magyar magyarázatot, pontos SHA-kat, ellenőrzési eredményt és következő felelőst mutat.
- [ ] Nincs nyitott lényegi szabályellentmondás az érintett feladathoz.

E pontok teljesülése a munkafolyamat operatív készültségét igazolja. Nem igazolja automatikusan a termék vagy a béta készültségét.

## Béta: a következő feladat sorrendje

1. Az operatív ellenőrzés és a lokális leltár után egy közös, csak olvasási bétaállapot-felmérés készül, pontos producer/consumer SHA-val és célzott kiadási platformmal.
2. A meglévő build-, teszt-, export/import-, re-import-, backup/restore- és live bizonyítékot a megfelelő commitokra és környezetre kell visszavezetni. Korábbi bizonyíték nem új tesztfutás.
3. Egy listában jelenjenek meg a valódi béta blokkolók, a választható exportbővítések és a későbbre halasztott funkciók. Az account-skill és cartography korábbi tervezése nem indul automatikusan.
4. A Product Owner kijelöli a következő szeletet és a kiadási célt; ezután készül TaskSpec, implementáció, célzott validáció és független review.

A meglévő [public-beta-gate.md](../quest-tracker/verification/public-beta-gate.md) 2026-09-26-i build és manuális bridge bizonyítékot rögzít. A PR #15 skill-point változása későbbi; a korábbi DLL bizonyítéka nem igazolja ezt az új kódot. A PR #15/R1 handoff 1268 sikeres tesztet jelentett, új játékbeli SP-küszöb próbát nem.

A következő lokális feladat: mindkét repo read-only leltára és a governance alapágra jutásának ellenőrzése. Branch-törlés, reset, runtime-módosítás vagy kiadás nincs e feladatban.

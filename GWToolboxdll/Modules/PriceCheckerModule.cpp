#include "stdafx.h"

#include <Defines.h>
#include <Logger.h>
#include <ImGuiAddons.h>

#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/ItemMgr.h>

#include <GWCA/Constants/Constants.h>

#include <Modules/InventoryManager.h>
#include <Modules/PriceCheckerModule.h>
#include <Modules/Resources.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <Timer.h>
#include <Utils/TextUtils.h>
#include <Utils/ToolboxUtils.h>

namespace pricechecker_api {
    struct SellEntry {
        double p = 0.0; // 价格（金币）
    };

    struct PricesResponse {
        std::unordered_map<std::string, SellEntry> sell;
    };
}

namespace {

    constexpr glz::opts json_opts{.error_on_unknown_keys = false};

    bool fetching_prices;
    const char* trader_quotes_url = "https://kamadan.gwtoolbox.com/trader_quotes";

    // No live pre-searing trader-quote service exists - this is presearing.com's own public, community-maintained price sheet.
    const char* presearing_sheet_id = "1u8-n_EJe9Nfl1twUHExLeuuYNT0Szo_7ss4HmmNytqk";

    constexpr clock_t request_interval = 1000 * 60 * 5;
    clock_t last_request_time = -request_interval;
    bool last_request_was_presearing = false;
    std::unordered_map<std::string, uint32_t> prices_by_identifier;

    PriceCheckerModule::Settings settings;

    // -------------------------------------------------------------------------
    // 价格查找表
    // -------------------------------------------------------------------------

    struct PriceInfo {
        const char* name;
        const char* id;
    };

    static const std::unordered_map<uint32_t, PriceInfo> price_info_by_unique_mod_struct = {
        {0x25300423, {"协调符文", "08038225300423"}},
        {0x25300425, {"生命符文", "08038225300425"}},
        {0x27e802c2, {"次级活力符文", "08038227e802c2"}},
        {0x21e80001, {"幻术师 次级快速施法符文", "08038321e80001"}},
        {0x21e80101, {"幻术师 次级幻象魔法符文", "08038321e80101"}},
        {0x21e80201, {"幻术师 次级支配魔法符文", "08038321e80201"}},
        {0x21e80301, {"幻术师 次级灵感魔法符文", "08038321e80301"}},
        {0x21e80401, {"死灵法师 次级鲜血魔法符文", "08038421e80401"}},
        {0x21e80501, {"死灵法师 次级死亡魔法符文", "08038421e80501"}},
        {0x21e80601, {"死灵法师 次级灵魂汲取符文", "08038421e80601"}},
        {0x21e80701, {"死灵法师 次级诅咒符文", "08038421e80701"}},
        {0x21e80801, {"元素使 次级空气魔法符文", "08038521e80801"}},
        {0x21e80901, {"元素使 次级大地魔法符文", "08038521e80901"}},
        {0x21e80a01, {"元素使 次级火焰魔法符文", "08038521e80a01"}},
        {0x21e80b01, {"元素使 次级水系魔法符文", "08038521e80b01"}},
        {0x21e80c01, {"元素使 次级能量储存符文", "08038521e80c01"}},
        {0x21e80d01, {"僧侣 次级治疗祈祷符文", "08038621e80d01"}},
        {0x21e80e01, {"僧侣 次级惩戒祈祷符文", "08038621e80e01"}},
        {0x21e80f01, {"僧侣 次级防护祈祷符文", "08038621e80f01"}},
        {0x21e81001, {"僧侣 次级神恩符文", "08038621e81001"}},
        {0x21e81101, {"战士 次级力量符文", "08038721e81101"}},
        {0x21e81201, {"战士 次级斧术精通符文", "08038721e81201"}},
        {0x21e81301, {"战士 次级锤术精通符文", "08038721e81301"}},
        {0x21e81401, {"战士 次级剑术精通符文", "08038721e81401"}},
        {0x21e81501, {"战士 次级战术符文", "08038721e81501"}},
        {0x27e802ea, {"战士 次级吸收符文", "08038727e802ea"}},
        {0x21e81601, {"游侠 次级野兽支配符文", "08038821e81601"}},
        {0x21e81701, {"游侠 次级专精符文", "08038821e81701"}},
        {0x21e81801, {"游侠 次级野外生存符文", "08038821e81801"}},
        {0x21e81901, {"游侠 次级射手精通符文", "08038821e81901"}},
        {0x21e80002, {"幻术师 高级快速施法符文", "080e1c21e80002"}},
        {0x21e80102, {"幻术师 高级幻象魔法符文", "080e1c21e80102"}},
        {0x21e80202, {"幻术师 高级支配魔法符文", "080e1c21e80202"}},
        {0x21e80302, {"幻术师 高级灵感魔法符文", "080e1c21e80302"}},
        {0x21e80003, {"幻术师 超级快速施法符文", "0815ad21e80003"}},
        {0x21e80103, {"幻术师 超级幻象魔法符文", "0815ad21e80103"}},
        {0x21e80203, {"幻术师 超级支配魔法符文", "0815ad21e80203"}},
        {0x21e80303, {"幻术师 超级灵感魔法符文", "0815ad21e80303"}},
        {0x25300427, {"恢复符文", "0815ae25300427"}},
        {0x25300429, {"复原符文", "0815ae25300429"}},
        {0x2530042b, {"清晰符文", "0815ae2530042b"}},
        {0x2530042d, {"净化符文", "0815ae2530042d"}},
        {0x27e902c2, {"高级活力符文", "0815ae27e902c2"}},
        {0x27ea02c2, {"超级活力符文", "0815af27ea02c2"}},
        {0x21e80402, {"死灵法师 高级鲜血魔法符文", "0815b021e80402"}},
        {0x21e80502, {"死灵法师 高级死亡魔法符文", "0815b021e80502"}},
        {0x21e80602, {"死灵法师 高级灵魂汲取符文", "0815b021e80602"}},
        {0x21e80702, {"死灵法师 高级诅咒符文", "0815b021e80702"}},
        {0x21e80403, {"死灵法师 超级鲜血魔法符文", "0815b121e80403"}},
        {0x21e80503, {"死灵法师 超级死亡魔法符文", "0815b121e80503"}},
        {0x21e80603, {"死灵法师 超级灵魂汲取符文", "0815b121e80603"}},
        {0x21e80703, {"死灵法师 超级诅咒符文", "0815b121e80703"}},
        {0x21e80802, {"元素使 高级空气魔法符文", "0815b221e80802"}},
        {0x21e80902, {"元素使 高级大地魔法符文", "0815b221e80902"}},
        {0x21e80a02, {"元素使 高级火焰魔法符文", "0815b221e80a02"}},
        {0x21e80b02, {"元素使 高级水系魔法符文", "0815b221e80b02"}},
        {0x21e80c02, {"元素使 高级能量储存符文", "0815b221e80c02"}},
        {0x21e80803, {"元素使 超级空气魔法符文", "0815b321e80803"}},
        {0x21e80903, {"元素使 超级大地魔法符文", "0815b321e80903"}},
        {0x21e80a03, {"元素使 超级火焰魔法符文", "0815b321e80a03"}},
        {0x21e80b03, {"元素使 超级水系魔法符文", "0815b321e80b03"}},
        {0x21e80c03, {"元素使 超级能量储存符文", "0815b321e80c03"}},
        {0x21e80d02, {"僧侣 高级治疗祈祷符文", "0815b421e80d02"}},
        {0x21e80e02, {"僧侣 高级惩戒祈祷符文", "0815b421e80e02"}},
        {0x21e80f02, {"僧侣 高级防护祈祷符文", "0815b421e80f02"}},
        {0x21e81002, {"僧侣 高级神恩符文", "0815b421e81002"}},
        {0x21e80d03, {"僧侣 超级治疗祈祷符文", "0815b521e80d03"}},
        {0x21e80e03, {"僧侣 超级惩戒祈祷符文", "0815b521e80e03"}},
        {0x21e80f03, {"僧侣 超级防护祈祷符文", "0815b521e80f03"}},
        {0x21e81003, {"僧侣 超级神恩符文", "0815b521e81003"}},
        {0x21e81102, {"战士 高级力量符文", "0815b621e81102"}},
        {0x21e81202, {"战士 高级斧术精通符文", "0815b621e81202"}},
        {0x21e81302, {"战士 高级锤术精通符文", "0815b621e81302"}},
        {0x21e81402, {"战士 高级剑术精通符文", "0815b621e81402"}},
        {0x21e81502, {"战士 高级战术符文", "0815b621e81502"}},
        {0x27e902ea, {"战士 高级吸收符文", "0815b627e902ea"}},
        {0x21e81103, {"战士 超级力量符文", "0815b721e81103"}},
        {0x21e81203, {"战士 超级斧术精通符文", "0815b721e81203"}},
        {0x21e81303, {"战士 超级锤术精通符文", "0815b721e81303"}},
        {0x21e81403, {"战士 超级剑术精通符文", "0815b721e81403"}},
        {0x21e81503, {"战士 超级战术符文", "0815b721e81503"}},
        {0x27ea02ea, {"战士 超级吸收符文", "0815b727ea02ea"}},
        {0x21e81602, {"游侠 高级野兽支配符文", "0815b821e81602"}},
        {0x21e81702, {"游侠 高级专精符文", "0815b821e81702"}},
        {0x21e81802, {"游侠 高级野外生存符文", "0815b821e81802"}},
        {0x21e81902, {"游侠 高级射手精通符文", "0815b821e81902"}},
        {0x21e81603, {"游侠 超级野兽支配符文", "0815b921e81603"}},
        {0x21e81703, {"游侠 超级专精符文", "0815b921e81703"}},
        {0x21e81803, {"游侠 超级野外生存符文", "0815b921e81803"}},
        {0x21e81903, {"游侠 超级射手精通符文", "0815b921e81903"}},
        {0x21e81d01, {"刺客 次级匕首精通符文", "0818b421e81d01"}},
        {0x21e81e01, {"刺客 次级暗杀术符文", "0818b421e81e01"}},
        {0x21e81f01, {"刺客 次级影术符文", "0818b421e81f01"}},
        {0x21e82301, {"刺客 次级致命攻击符文", "0818b421e82301"}},
        {0x21e81d02, {"刺客 高级匕首精通符文", "0818b521e81d02"}},
        {0x21e81e02, {"刺客 高级暗杀术符文", "0818b521e81e02"}},
        {0x21e81f02, {"刺客 高级影术符文", "0818b521e81f02"}},
        {0x21e82302, {"刺客 高级致命攻击符文", "0818b521e82302"}},
        {0x21e81d03, {"刺客 超级匕首精通符文", "0818b621e81d03"}},
        {0x21e81e03, {"刺客 超级暗杀术符文", "0818b621e81e03"}},
        {0x21e81f03, {"刺客 超级影术符文", "0818b621e81f03"}},
        {0x21e82303, {"刺客 超级致命攻击符文", "0818b621e82303"}},
        {0x21e82001, {"祭祀 次级通灵符文", "0818b721e82001"}},
        {0x21e82101, {"祭祀 次级复原魔法符文", "0818b721e82101"}},
        {0x21e82201, {"祭祀 次级引导魔法符文", "0818b721e82201"}},
        {0x21e82401, {"祭祀 次级召唤力量符文", "0818b721e82401"}},
        {0x21e82002, {"祭祀 高级通灵符文", "0818b821e82002"}},
        {0x21e82102, {"祭祀 高级复原魔法符文", "0818b821e82102"}},
        {0x21e82202, {"祭祀 高级引导魔法符文", "0818b821e82202"}},
        {0x21e82402, {"祭祀 高级召唤力量符文", "0818b821e82402"}},
        {0x21e82003, {"祭祀 超级通灵符文", "0818b921e82003"}},
        {0x21e82103, {"祭祀 超级复原魔法符文", "0818b921e82103"}},
        {0x21e82203, {"祭祀 超级引导魔法符文", "0818b921e82203"}},
        {0x21e82403, {"祭祀 超级召唤力量符文", "0818b921e82403"}},
        {0x21e82901, {"神唤使 次级镰刀精通符文", "083cb921e82901"}},
        {0x21e82a01, {"神唤使 次级风之祈祷符文", "083cb921e82a01"}},
        {0x21e82b01, {"神唤使 次级地之祈祷符文", "083cb921e82b01"}},
        {0x21e82c01, {"神唤使 次级秘法符文", "083cb921e82c01"}},
        {0x21e82902, {"神唤使 高级镰刀精通符文", "083cba21e82902"}},
        {0x21e82a02, {"神唤使 高级风之祈祷符文", "083cba21e82a02"}},
        {0x21e82b02, {"神唤使 高级地之祈祷符文", "083cba21e82b02"}},
        {0x21e82c02, {"神唤使 高级秘法符文", "083cba21e82c02"}},
        {0x21e82903, {"神唤使 超级镰刀精通符文", "083cbb21e82903"}},
        {0x21e82a03, {"神唤使 超级风之祈祷符文", "083cbb21e82a03"}},
        {0x21e82b03, {"神唤使 超级地之祈祷符文", "083cbb21e82b03"}},
        {0x21e82c03, {"神唤使 超级秘法符文", "083cbb21e82c03"}},
        {0x21e82501, {"圣言者 次级矛术精通符文", "083cbc21e82501"}},
        {0x21e82601, {"圣言者 次级命令符文", "083cbc21e82601"}},
        {0x21e82701, {"圣言者 次级激励符文", "083cbc21e82701"}},
        {0x21e82801, {"圣言者 次级领导符文", "083cbc21e82801"}},
        {0x21e82502, {"圣言者 高级矛术精通符文", "083cbd21e82502"}},
        {0x21e82602, {"圣言者 高级命令符文", "083cbd21e82602"}},
        {0x21e82702, {"圣言者 高级激励符文", "083cbd21e82702"}},
        {0x21e82802, {"圣言者 高级领导符文", "083cbd21e82802"}},
        {0x21e82503, {"圣言者 超级矛术精通符文", "083cbe21e82503"}},
        {0x21e82603, {"圣言者 超级命令符文", "083cbe21e82603"}},
        {0x21e82703, {"圣言者 超级激励符文", "083cbe21e82703"}},
        {0x21e82803, {"圣言者 超级领导符文", "083cbe21e82803"}},
        {0xa53003bc, {"先锋纹章 [刺客]", "084ab4a53003bc"}},
        {0xa53003be, {"渗透者纹章 [刺客]", "084ab5a53003be"}},
        {0xa53003c0, {"破坏者纹章 [刺客]", "084ab6a53003c0"}},
        {0xa53003c2, {"夜行者纹章 [刺客]", "084ab7a53003c2"}},
        {0xa53003c4, {"技师纹章 [幻术师]", "084ab8a53003c4"}},
        {0xa53003c6, {"天才纹章 [幻术师]", "084ab9a53003c6"}},
        {0xa53003c8, {"大师纹章 [幻术师]", "084abaa53003c8"}},
        {0x253003ca, {"光辉纹章", "084abb253003ca"}},
        {0x253003cc, {"幸存者纹章", "084abc253003cc"}},
        {0xa53003ce, {"坚韧纹章", "084abda53003ce"}},
        {0xa53003d0, {"斗士纹章", "084abea53003d0"}},
        {0xa53003d2, {"祝福纹章", "084abfa53003d2"}},
        {0xa53003d4, {"使者纹章", "084ac0a53003d4"}},
        {0xa53003d6, {"哨兵纹章", "084ac1a53003d6"}},
        {0x27e802a9, {"血染纹章 [死灵法师]", "084ac227e802a9"}},
        {0x253003d8, {"折磨者纹章 [死灵法师]", "084ac3253003d8"}},
        {0xa53003da, {"葬仪纹章 [死灵法师]", "084ac4a53003da"}},
        {0xa53003dc, {"骨织纹章 [死灵法师]", "084ac5a53003dc"}},
        {0xa53003de, {"仆从大师纹章 [死灵法师]", "084ac6a53003de"}},
        {0xa53003e0, {"枯萎纹章 [死灵法师]", "084ac7a53003e0"}},
        {0xa53003e2, {"棱镜纹章 [元素使]", "084ac8a53003e2"}},
        {0xa53003e4, {"水法纹章 [元素使]", "084ac9a53003e4"}},
        {0xa53003e6, {"土法纹章 [元素使]", "084acaa53003e6"}},
        {0xa53003e8, {"火法纹章 [元素使]", "084acba53003e8"}},
        {0xa53003ea, {"风法纹章 [元素使]", "084acca53003ea"}},
        {0xa53003ec, {"流浪者纹章 [僧侣]", "084acda53003ec"}},
        {0xa53003ee, {"信徒纹章 [僧侣]", "084acea53003ee"}},
        {0xa53003f0, {"隐士纹章 [僧侣]", "084acfa53003f0"}},
        {0xa53003f2, {"骑士纹章 [战士]", "084ad0a53003f2"}},
        {0x27e802b6, {"中尉纹章 [战士]", "084ad127e802b6"}},
        {0x27e802b7, {"石拳纹章 [战士]", "084ad227e802b7"}},
        {0xa53003f4, {"无畏纹章 [战士]", "084ad3a53003f4"}},
        {0xa53003f6, {"哨卫纹章 [战士]", "084ad4a53003f6"}},
        {0xa53003f8, {"霜缚纹章 [游侠]", "084ad5a53003f8"}},
        {0xa53003fa, {"地缚纹章 [游侠]", "084ad6a53003fa"}},
        {0xa53003fc, {"焰缚纹章 [游侠]", "084ad7a53003fc"}},
        {0xa53003fe, {"风暴缚纹章 [游侠]", "084ad8a53003fe"}},
        {0xa5300400, {"野兽大师纹章 [游侠]", "084ad9a5300400"}},
        {0xa5300402, {"侦察兵纹章 [游侠]", "084adaa5300402"}},
        {0xa5300404, {"风行者纹章 [神唤使]", "084adba5300404"}},
        {0xa5300406, {"遗弃纹章 [神唤使]", "084adca5300406"}},
        {0xa5300408, {"萨满纹章 [祭祀]", "084adda5300408"}},
        {0xa530040a, {"鬼锻纹章 [祭祀]", "084adea530040a"}},
        {0xa530040c, {"秘法纹章 [祭祀]", "084adfa530040c"}},
        {0xa530040e, {"百夫长纹章 [圣言者]", "084ae0a530040e"}},
        {0x24d00201, {"染料瓶 [蓝色]", "0a009224d00201"}},
        {0x24d00301, {"染料瓶 [绿色]", "0a009224d00301"}},
        {0x24d00401, {"染料瓶 [紫色]", "0a009224d00401"}},
        {0x24d00501, {"染料瓶 [红色]", "0a009224d00501"}},
        {0x24d00601, {"染料瓶 [黄色]", "0a009224d00601"}},
        {0x24d00701, {"染料瓶 [棕色]", "0a009224d00701"}},
        {0x24d00801, {"染料瓶 [橙色]", "0a009224d00801"}},
        {0x24d00901, {"染料瓶 [银色]", "0a009224d00901"}},
        {0x24d00a01, {"染料瓶 [黑色]", "0a009224d00a01"}},
        {0x24d00c01, {"染料瓶 [白色]", "0a009224d00c01"}},
        {0x24d00d01, {"染料瓶 [粉色]", "0a009224d00d01"}},
        {0x000b0399, {"骨头", "0b0399"}},
        {0x000b039a, {"木炭块", "0b039a"}},
        {0x000b039b, {"怪兽爪", "0b039b"}},
        {0x000b039d, {"布卷", "0b039d"}},
        {0x000b039e, {"亚麻布卷", "0b039e"}},
        {0x000b039f, {"锦缎布卷", "0b039f"}},
        {0x000b03a0, {"丝绸布卷", "0b03a0"}},
        {0x000b03a1, {"闪闪发光的灰尘堆", "0b03a1"}},
        {0x000b03a2, {"鬼灵精华", "0b03a2"}},
        {0x000b03a3, {"怪兽眼", "0b03a3"}},
        {0x000b03a4, {"怪兽牙", "0b03a4"}},
        {0x000b03a5, {"羽毛", "0b03a5"}},
        {0x000b03a6, {"植物纤维", "0b03a6"}},
        {0x000b03a7, {"钻石", "0b03a7"}},
        {0x000b03a8, {"黑曜石宝石", "0b03a8"}},
        {0x000b03a9, {"红宝石", "0b03a9"}},
        {0x000b03aa, {"蓝宝石", "0b03aa"}},
        {0x000b03ab, {"钢化玻璃瓶", "0b03ab"}},
        {0x000b03ac, {"鞣制皮革方块", "0b03ac"}},
        {0x000b03ad, {"毛皮方块", "0b03ad"}},
        {0x000b03ae, {"皮革方块", "0b03ae"}},
        {0x000b03af, {"伊洛纳皮革方块", "0b03af"}},
        {0x000b03b0, {"墨水瓶", "0b03b0"}},
        {0x000b03b1, {"黑曜石碎片", "0b03b1"}},
        {0x000b03b2, {"木板", "0b03b2"}},
        {0x000b03b4, {"铁锭", "0b03b4"}},
        {0x000b03b5, {"钢锭", "0b03b5"}},
        {0x000b03b6, {"戴尔迪摩钢锭", "0b03b6"}},
        {0x000b03b7, {"羊皮纸卷", "0b03b7"}},
        {0x000b03b8, {"犊皮纸卷", "0b03b8"}},
        {0x000b03b9, {"鳞片", "0b03b9"}},
        {0x000b03ba, {"几丁质碎片", "0b03ba"}},
        {0x000b03bb, {"花岗岩板", "0b03bb"}},
        {0x000b03bc, {"灵木木板", "0b03bc"}},
        {0x000b1984, {"琥珀块", "0b1984"}},
        {0x000b1985, {"翡翠碎片", "0b1985"}},
    };

    // -------------------------------------------------------------------------
    // Pre-searing price sheet (presearing.com's community-maintained Google Sheet)
    // -------------------------------------------------------------------------

    struct PresearingSheetItem {
        const char* tab_name;      // Google Sheets tab ("sheet=" query param)
        const char* category;      // the sheet's grouping column, carried forward over blank cells
        const char* item_name;     // the sheet's "Item Name" column, verbatim
        const char* id;            // matches an id in price_info_by_unique_mod_struct above
    };

    // Hand-mapped onto the mod-struct ids above, since the sheet only has free-text item names; rows it flags as pre-searing-unusable or with no price of their own are omitted.
    const std::vector<PresearingSheetItem> presearing_sheet_items = {
        {"Runes", "Common", "Attunement", "08038225300423"},
        {"Runes", "Common", "Minor Vigor", "08038227e802c2"},
        {"Runes", "Common", "Vitae", "08038225300425"},
        {"Runes", "Elementalist", "Air Magic", "08038521e80801"},
        {"Runes", "Elementalist", "Earth Magic", "08038521e80901"},
        {"Runes", "Elementalist", "Energy Storage", "08038521e80c01"},
        {"Runes", "Elementalist", "Fire Magic", "08038521e80a01"},
        {"Runes", "Elementalist", "Water Magic", "08038521e80b01"},
        {"Runes", "Mesmer", "Domination", "08038321e80201"},
        {"Runes", "Mesmer", "Fast Casting", "08038321e80001"},
        {"Runes", "Mesmer", "Illusion", "08038321e80101"},
        {"Runes", "Mesmer", "Inspiration", "08038321e80301"},
        {"Runes", "Monk", "Divine Favor", "08038621e81001"},
        {"Runes", "Monk", "Healing Prayers", "08038621e80d01"},
        {"Runes", "Monk", "Protection Prayers", "08038621e80f01"},
        {"Runes", "Monk", "Smiting", "08038621e80e01"},
        {"Runes", "Necromancer", "Blood Magic", "08038421e80401"},
        {"Runes", "Necromancer", "Curses", "08038421e80701"},
        {"Runes", "Necromancer", "Death Magic", "08038421e80501"},
        {"Runes", "Necromancer", "Soul Reaping", "08038421e80601"},
        {"Runes", "Ranger", "Beast Mastery", "08038821e81601"},
        {"Runes", "Ranger", "Expertise", "08038821e81701"},
        {"Runes", "Ranger", "Marksmanship", "08038821e81901"},
        {"Runes", "Ranger", "Wilderness Survival", "08038821e81801"},
        {"Runes", "Warrior", "Absorption", "08038727e802ea"},
        {"Runes", "Warrior", "Axe Mastery", "08038721e81201"},
        {"Runes", "Warrior", "Hammer Mastery", "08038721e81301"},
        {"Runes", "Warrior", "Strength", "08038721e81101"},
        {"Runes", "Warrior", "Swordsmanship", "08038721e81401"},
        {"Runes", "Warrior", "Tactics", "08038721e81501"},

        {"Insignias", "Common", "Blessed", "084abfa53003d2"},
        {"Insignias", "Common", "Brawler's", "084abea53003d0"},
        {"Insignias", "Common", "Radiant", "084abb253003ca"},
        {"Insignias", "Common", "Sentry's", "084ac1a53003d6"},
        {"Insignias", "Common", "Stalwart", "084abda53003ce"},
        {"Insignias", "Common", "Survivor", "084abc253003cc"},
        {"Insignias", "Elementalist", "Aeromancer", "084acca53003ea"},
        {"Insignias", "Elementalist", "Geomancer", "084acaa53003e6"},
        {"Insignias", "Elementalist", "Hydromancer", "084ac9a53003e4"},
        {"Insignias", "Elementalist", "Prismatic", "084ac8a53003e2"},
        {"Insignias", "Elementalist", "Pyromancer", "084acba53003e8"},
        {"Insignias", "Mesmer", "Artificer's", "084ab8a53003c4"},
        {"Insignias", "Mesmer", "Prodigy's", "084ab9a53003c6"},
        {"Insignias", "Mesmer", "Virtuoso's", "084abaa53003c8"},
        {"Insignias", "Monk", "Anchorite's", "084acfa53003f0"},
        {"Insignias", "Monk", "Disciple's", "084acea53003ee"},
        {"Insignias", "Monk", "Wanderer's", "084acda53003ec"},
        {"Insignias", "Necromancer", "Blighter's", "084ac7a53003e0"},
        {"Insignias", "Necromancer", "Bonelace", "084ac5a53003dc"},
        {"Insignias", "Necromancer", "Minion Master's", "084ac6a53003de"},
        {"Insignias", "Necromancer", "Tormentor's", "084ac3253003d8"},
        {"Insignias", "Necromancer", "Undertaker's", "084ac4a53003da"},
        {"Insignias", "Ranger", "Beastmaster's", "084ad9a5300400"},
        {"Insignias", "Ranger", "Earthbound", "084ad6a53003fa"},
        {"Insignias", "Ranger", "Frostbound", "084ad5a53003f8"},
        {"Insignias", "Ranger", "Pyrebound", "084ad7a53003fc"},
        {"Insignias", "Ranger", "Scout's", "084adaa5300402"},
        {"Insignias", "Ranger", "Stormbound", "084ad8a53003fe"},
        {"Insignias", "Warrior", "Dreadnought", "084ad3a53003f4"},
        {"Insignias", "Warrior", "Knight's", "084ad0a53003f2"},
        {"Insignias", "Warrior", "Sentinel's", "084ad4a53003f6"},
    };

    const char* presearing_sheet_tabs[] = {"Runes", "Insignias"};

    int MaterialSlotToModelID(GW::Constants::MaterialSlot mat)
    {
        const auto info = GW::Items::GetMaterialInfo(mat);
        return info ? info->model_id : 0;
    }

    GW::Constants::MaterialSlot GetItemMaterialSlot(const GW::Item* item)
    {
        if (!item) return static_cast<GW::Constants::MaterialSlot>(0xff);
        const auto mod = static_cast<const InventoryManager::Item*>(item)->GetModifier(0x2508);
        if (!mod) return static_cast<GW::Constants::MaterialSlot>(0xff);
        return static_cast<GW::Constants::MaterialSlot>(mod->arg1());
    }

    uint32_t GetPriceById(const char* id)
    {
        const auto& prices = PriceCheckerModule::FetchPrices();
        const auto found = prices.find(id);
        return found != prices.end() ? found->second : 0u;
    }

    bool ParsePriceJson(const std::string& prices_json_str)
    {
        pricechecker_api::PricesResponse prices{};
        if (auto ec = glz::read<json_opts>(prices, prices_json_str); ec) return false;

        prices_by_identifier.clear();
        for (const auto& [key, entry] : prices.sell) {
            prices_by_identifier[key] = static_cast<uint32_t>(entry.p);
        }
        return !prices_by_identifier.empty();
    }

    // Minimal RFC4180-ish CSV line splitter - handles quoted fields, commas inside quotes, and "" as an escaped quote.
    std::vector<std::string> ParseCsvLine(const std::string& line)
    {
        std::vector<std::string> fields;
        std::string field;
        bool in_quotes = false;
        for (size_t i = 0; i < line.size(); i++) {
            const char c = line[i];
            if (in_quotes) {
                if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    i++;
                } else if (c == '"') {
                    in_quotes = false;
                } else {
                    field += c;
                }
            } else if (c == '"') {
                in_quotes = true;
            } else if (c == ',') {
                fields.push_back(std::move(field));
                field.clear();
            } else {
                field += c;
            }
        }
        fields.push_back(std::move(field));
        return fields;
    }

    // Parses "500g"/"1k"/"1.5k"; other units (e.g. presearing.com's unconfirmed "BD") are left unparsed rather than guessed at.
    bool ParseGoldAmount(const std::string& cell, double& out_gold)
    {
        const auto trimmed = TextUtils::trim(cell);
        if (trimmed.empty()) return false;

        const auto unit = static_cast<char>(std::tolower(static_cast<unsigned char>(trimmed.back())));
        if (unit != 'g' && unit != 'k') return false;

        const auto number_part = trimmed.substr(0, trimmed.size() - 1);
        if (number_part.empty()) return false;

        char* end = nullptr;
        const double value = std::strtod(number_part.c_str(), &end);
        if (end != number_part.c_str() + number_part.size()) return false;

        out_gold = unit == 'k' ? value * 1000.0 : value;
        return true;
    }

    // Merges recognised rows into prices_by_identifier; only touches ids owned by this tab, leaving other tabs' cached prices intact.
    bool ParsePresearingSheetCsv(const char* tab_name, const std::string& csv)
    {
        const auto lines = TextUtils::Split(csv, "\n");
        if (lines.empty()) return false;

        const auto header = ParseCsvLine(lines.front());
        int item_name_col = -1, low_col = -1, high_col = -1;
        constexpr int category_col = 0;
        for (size_t i = 0; i < header.size(); i++) {
            const auto cell = TextUtils::trim(header[i]);
            if (cell == "Item Name") item_name_col = static_cast<int>(i);
            else if (cell.starts_with("Price Low")) low_col = static_cast<int>(i); // rightmost column wins - latest date
            else if (cell.starts_with("Price High")) high_col = static_cast<int>(i);
        }
        if (item_name_col < 0 || low_col < 0 || high_col < 0) return false;

        size_t found_count = 0;
        const std::string tab_name_str = tab_name;
        std::string current_category;
        for (size_t i = 1; i < lines.size(); i++) {
            const auto fields = ParseCsvLine(lines[i]);
            const auto max_col = std::max({item_name_col, low_col, high_col});
            if (fields.size() <= static_cast<size_t>(max_col)) continue;

            if (const auto category_cell = TextUtils::trim(fields[category_col]); !category_cell.empty()) {
                current_category = category_cell;
            }

            const auto item_name = TextUtils::trim(fields[item_name_col]);
            if (item_name.empty()) continue; // extra description row for the item above, not a distinct item

            const auto entry = std::ranges::find_if(presearing_sheet_items, [&](const PresearingSheetItem& item) {
                return item.tab_name == tab_name_str && item.category == current_category && item.item_name == item_name;
            });
            if (entry == presearing_sheet_items.end()) continue;

            double low = 0.0, high = 0.0;
            const bool has_low = ParseGoldAmount(fields[low_col], low);
            const bool has_high = ParseGoldAmount(fields[high_col], high);
            if (!has_low && !has_high) continue;

            const double price = has_low && has_high ? (low + high) / 2.0 : (has_low ? low : high);
            prices_by_identifier[entry->id] = static_cast<uint32_t>(std::lround(price));
            found_count++;
        }
        return found_count > 0;
    }

    void SignalItemDescriptionUpdated()
    {
        GW::GameThread::Enqueue([] {
            const auto item = GW::Items::GetHoveredItem();
            if (!item) return;
            GW::UI::SendFrameUIMessage(GW::UI::GetChildFrame(GW::UI::GetRootFrame(), 0xffffffff), GW::UI::UIMessage::kItemUpdated, item);
        });
    }

} // namespace

void PriceCheckerModule::Initialize()
{
    ToolboxModule::Initialize();
    SettingsRegistry::Register(this, settings);
    FetchPrices();
}

void PriceCheckerModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void PriceCheckerModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

void PriceCheckerModule::Terminate()
{
    ToolboxModule::Terminate();
}

void PriceCheckerModule::DrawSettingsInternal()
{
    ImGui::CheckboxWithHelp("在梅兰朵契约中显示商人价格而非交易者价格", &settings.show_merchant_price_for_melandrus_accord_instead, "在梅兰朵契约中，显示材料的固定商人价格而非实时交易者价格");
}

const std::unordered_map<std::string, uint32_t>& PriceCheckerModule::FetchPrices()
{
    const bool is_presearing = GW::Map::IsPreSearing();
    if (is_presearing != last_request_was_presearing) {
        // Crossing the pre/post-searing boundary invalidates the cached quotes - force a refetch from the right source.
        last_request_time = -request_interval;
    }
    if (TIMER_DIFF(last_request_time) > request_interval) {
        last_request_time = TIMER_INIT();
        last_request_was_presearing = is_presearing;
        if (is_presearing) {
            for (const char* tab_name : presearing_sheet_tabs) {
                // Same CSV export a published Google Sheet's "File > Share > Publish to web" produces - no key/auth needed.
                const auto csv_url = std::format("https://docs.google.com/spreadsheets/d/{}/gviz/tq?tqx=out:csv&sheet={}", presearing_sheet_id, tab_name);
                Resources::Download(csv_url, [tab_name](bool success, const std::string& response, void*) {
                    if (!success) {
                        last_request_time -= request_interval;
                        return;
                    }
                    ParsePresearingSheetCsv(tab_name, response);
                    SignalItemDescriptionUpdated();
                });
            }
        } else {
            Resources::Download(trader_quotes_url, [](bool success, const std::string& response, void*) {
                if (!success) {
                    last_request_time -= request_interval;
                    return;
                }
                ParsePriceJson(response);
                SignalItemDescriptionUpdated();
            });
        }
    }
    return prices_by_identifier;
}

uint32_t PriceCheckerModule::GetTraderSellPrice(const GW::Item* item)
{
    return GetPriceByItem(item);
}

uint32_t PriceCheckerModule::GetTraderSellPrice(const GW::Constants::MaterialSlot material)
{
    if (GW::PlayerMgr::IsMelandrusAccord() && settings.show_merchant_price_for_melandrus_accord_instead) return GetMerchantSellPrice(material);
    GW::Item item;
    memset(&item, 0, sizeof(item));
    item.type = GW::Constants::ItemType::Materials_Zcoins;
    item.model_id = MaterialSlotToModelID(material);
    return GetTraderSellPrice(&item);
}

uint32_t PriceCheckerModule::GetMerchantSellPrice(const GW::Constants::MaterialSlot material)
{
    using namespace GW::Constants;
    if (material == MaterialSlot::IronIngot) return 5;
    if (material == MaterialSlot::WoodPlank) return 4;
    if (material <= MaterialSlot::Feather) return 3;
    switch (material) {
        case MaterialSlot::Sapphire:
        case MaterialSlot::Ruby:
        case MaterialSlot::Diamond:
        case MaterialSlot::OnyxGemstone:
            return 250;
        case MaterialSlot::AmberChunk:
        case MaterialSlot::JadeiteShard:
        case MaterialSlot::GlobofEctoplasm:
        case MaterialSlot::MonstrousEye:
        case MaterialSlot::MonstrousClaw:
        case MaterialSlot::MonstrousFang:
        case MaterialSlot::ObsidianShard:
            return 100;
        case MaterialSlot::RollofParchment:
        case MaterialSlot::RollofVellum:
        case MaterialSlot::TemperedGlassVial:
        case MaterialSlot::VialofInk:
        case MaterialSlot::SpiritwoodPlank:
            return 20;
        case MaterialSlot::BoltofDamask:
        case MaterialSlot::BoltofLinen:
        case MaterialSlot::BoltofSilk:
            return 15;
        case MaterialSlot::FurSquare:
            return 10;
    }
    return 30;
}

uint32_t PriceCheckerModule::GetPriceByItem(const GW::Item* item, std::string* item_name_out, unsigned int mod_start_index)
{
    if (item->type == GW::Constants::ItemType::Materials_Zcoins) {
        uint32_t mod = (std::to_underlying(item->type) << 16) | (item->model_id & 0xffff);
        const auto found = price_info_by_unique_mod_struct.find(mod);
        if (found == price_info_by_unique_mod_struct.end()) return 0;
        if (GW::PlayerMgr::IsMelandrusAccord() && settings.show_merchant_price_for_melandrus_accord_instead) return GetMerchantSellPrice(GetItemMaterialSlot(item));
        return GetPriceById(found->second.id);
    }

    size_t found_count = 0;
    for (size_t i = 0; i < item->mod_struct_size; i++) {
        const auto mod = item->mod_struct[i].mod;
        const auto found = price_info_by_unique_mod_struct.find(mod);
        if (found == price_info_by_unique_mod_struct.end()) continue;
        if (found_count == mod_start_index) {
            if (item_name_out) *item_name_out = found->second.name;
            return GetPriceById(found->second.id);
        }
        found_count++;
    }
    return 0;
}

#pragma once
#include <GWCA/Constants/Constants.h>

namespace CompletionWindow_Constants {
    using namespace GW::Constants;

    constexpr std::array campaign_names = {"核心", "英雄之路", "盟与敌", "英雄世界", "北方之眼", "地监"};

    const char* CampaignName(const Campaign camp) { return campaign_names[std::to_underlying(camp)]; }


    // GW 将这些图标作为文件哈希数组加载一次，然后保存在内存中 - 我无法在 RDATA 中可靠地找到它们的位置，因此必须在此处定义它们 :(
    enum class WorldMapIcon : uint32_t {
        None = 0,
        Kryta_Mission = 0x2ac23,
        Kryta_CompletePrimary = 0x2ac27,
        Kryta_CompleteSecondary = 0x2ac29,
        Kryta_City = 0x2ac2f,
        Kryta_Outpost = 0x2ac2d,
        Kryta_Arena = 0x2ac2b,

        Cantha_Mission = 0x2ac35,
        Cantha_CompletePrimary = 0x2ac39,
        Cantha_CompleteExpert = 0x2ac3b,
        Cantha_CompleteMaster = 0x2ac3d,
        Cantha_City = 0x2ac43,
        Cantha_Outpost = 0x2ac41,
        Cantha_ChallengeMission = 0x2ac33,
        Cantha_LuxonsOwned = 0x2ac31, // 注意：库兹柯的纹理是路克森纹理加蓝色覆盖层；证明路克森才是凯珊的真正拥有者！

        Elona_Mission = 0x38048,
        Elona_CompletePrimary = 0x3804c,
        Elona_CompleteExpert = 0x3804e,
        Elona_CompleteMaster = 0x38050,
        Elona_City = 0x322ad,
        Elona_Outpost = 0x322ab,
        Elona_ChallengeMission = 0x38046,

        RealmOfTorment_Outpost = 0x322af,
        RealmOfTorment_City = 0x322b1,
        RealmOfTorment_Mission = 0x38058,
        RealmOfTorment_ChallengeMission = 0x38056,

        RealmOfTorment_EliteArea = 0x43466,

        EyeOfTheNorth_Dungeon = 0x50830,
        EyeOfTheNorth_DungeonComplete = 0x50830, // TODO
        EyeOfTheNorth_Mission = 0x50831,
        EyeOfTheNorth_MissionComplete = 0x50831, // TODO
        EyeOfTheNorth_HardMode_Dungeon = 0x50832,
        EyeOfTheNorth_HardMode_DungeonComplete = 0x50832, // TODO
        EyeOfTheNorth_HardMode_Mission = 0x50833,
        EyeOfTheNorth_HardMode_MissionComplete = 0x50833, // TODO

        HardMode = 0x45563,
        HardMode_CompletePrimary = 0x45567,
        HardMode_CompleteExpert = 0x45569,
        HardMode_CompleteMaster = 0x4556b,
        HardMode_CompleteAll = 0x4556d // 金色头盔
    };

    // 节日帽子编码名称（注意：这些是游戏内部编码字符串，不可翻译）
    const wchar_t* encoded_festival_hat_names[] = {
        // 万圣节
        L"\x8102\x5C2B\xB7F4\xC976\x5CE1", // 夏尔帽子
        L"\x8101\x5EBF\xE584\x8367\x5830", // 愤怒南瓜王冠
        L"\x8102\x6E5B\x9CE4\xF433\x7437", // 毛茸茸耳朵
        L"\x8102\x34F7\xD06F\x9B03\x1D89", // 木乃伊面具
        L"\x8102\x34F6\x9A67\xE2B0\x19A6", // 狼人面具
        L"\x245D\xCD3B\xC2E1\x2AC3",       // 南瓜王冠
        L"\x8102\x7F39\xEDE6\xF160\x552",  // 死神兜帽
        L"\x8102\x34F8\xE9FE\x9F4D\x4B4A", // 稻草人面具
        L"\x8102\x5C2A\xBF3B\x9294\x717F", // 骷髅脸绘
        L"\x8102\x6E5C\xE744\xE51D\x549D", // 幽灵眼镜
        L"\x8102\x7F3A\xD2B3\xE31C\x8F1",  // 三角帽
        L"\x8101\x5EC0\xD174\xBB32\x1697", // 邪恶帽子
        L"\x8102\x4997\x8F3A\xB679\x3196", // 僵尸脸绘
        // 冬幕节
        L"\x8102\x6E5D",                   // 恶魔之角
        L"\x8102\x6E16",                   // 神圣光环
        L"\x8103\x1E6\xCA8E\xFEBE\x13F1",  // 节日冬帽
        L"\x8101\x60FF\xD57C\xEE89\x1B58", // 弗利兹王冠
        L"\x8101\x6100\xE654\xF2D6\x4A8",  // 格伦斯巨角
        L"\x8102\x34F3\xA758\x90C4\x16B6", // 格伦奇帽
        L"\x2481\x821F\xD73C\x4CD2",       // 格伦斯之角
        L"\x8102\x34F1\xDF5B\xCBEB\x51B3", // 冰王冠
        L"\x8102\x5EA2\xF571\xCFCB\x35A6", // 冰晶冠
        L"\x8101\x60FE\xDE91\x9A0C\x158B", // 小丑帽
        L"\x8102\x34F4\xB9A7\x8ED1\x23ED", // 鲁迪面具
        L"\x8102\x5EA3\xBF84\xA60F\xC33",  // 雪花晶冠
        L"\x8103\x1E9\xDD16\x9987\x490C",  // 时尚黑围巾
        L"\x8103\x1E7\xDD75\xA264\x4968",  // 时尚红条纹围巾
        L"\x8103\x1E8\xDBFA\xB129\x3687",  // 时尚白条纹围巾
        L"\x8101\x6101\xCAC9\xC13F\x5086", // 时尚圣诞帽
        L"\x8102\x34F2\xBED1\xD377\x12A0", // 花环王冠
        L"\x2482\x81E2\xEE52\x471F",       // 圣诞帽
        // 龙节
        L"\x8102\x216A\xF512\xCE87\x46F",  // 恶魔面具
        L"\x8101\x151F\x918F\xB47E\x36CA", // 龙面具
        L"\x8102\x4669\xAC2E\xFF9A\x29A4", // 抓握面具
        L"\x8102\x5A95\xB979\xA70E\x1639", // 帝国龙面具
        L"\x8101\x66FE\xE888\xCFB3\x1F77", // 狮子面具
        L"\x8102\x7699\x94D7\xAEA5\x2431", // 欢乐龙面具
        L"\x8102\x6901\x8F3A\xC47B\x4E1A", // 邪恶龙面具
        L"\x8101\x3DE\xE3E9\xAFAA\x152"    // 天狗面具
    };
    constexpr size_t wintersday_index = 13;      // 冬幕节帽子在 encoded_festival_hat_names 数组中的起始索引
    constexpr size_t dragon_festival_index = 31; // 龙节帽子在 encoded_festival_hat_names 数组中的起始索引

    // 护甲编码名称（注意：这些是游戏内部编码字符串，不可翻译）
    // 此数组按 HallOfMonumentsModule::Detail 枚举中护甲的顺序作为键
    // 即索引 0 为精英凯珊护甲
    const wchar_t* encoded_armor_names[] = {
        L"\x108\x107"
        "Elite Canthan Armor\x1", // 精英凯珊护甲
        L"\x108\x107"
        "Elite Exotic Armor\x1", // 精英异域护甲
        L"\x108\x107"
        "Elite Kurzick Armor\x1", // 精英库兹柯护甲
        L"\x108\x107"
        "Elite Luxon Armor\x1", // 精英路克森护甲
        L"\x108\x107"
        "Imperial Ascended Armor\x1", // 帝国飞升护甲
        L"\x108\x107"
        "Ancient Armor\x1", // 远古护甲
        L"\x108\x107"
        "Elite Sunspear Armor\x1", // 精英日灼护甲
        L"\x108\x107"
        "Vabbian Armor\x1", // 法比护甲
        L"\x108\x107"
        "Primeval Armor\x1", // 原初护甲
        L"\x108\x107"
        "Asuran Armor\x1", // 阿苏拉护甲
        L"\x108\x107"
        "Norn Armor\x1", // 诺恩护甲
        L"\x108\x107"
        "Silver Eagle Armor\x1", // 银鹰护甲
        L"\x108\x107"
        "Monument Armor\x1", // 纪念碑护甲
        L"\x108\x107"
        "Obsidian Armor\x1", // 黑曜石护甲
        L"\x108\x107"
        "Granite Citadel Elite Armor\x1", // 花岗岩城堡精英护甲
        L"\x108\x107"
        "Granite Citadel Exclusive Armor\x1", // 花岗岩城堡专属护甲
        L"\x108\x107"
        "Granite Citadel Ascended Armor\x1", // 花岗岩城堡飞升护甲
        L"\x108\x107"
        "Marhan's Grotto Elite Armor\x1", // 马汗洞穴精英护甲
        L"\x108\x107"
        "Marhan's Grotto Exclusive Armor\x1", // 马汗洞穴专属护甲
        L"\x108\x107"
        "Marhan's Grotto Ascended Armor\x1", // 马汗洞穴飞升护甲
    };
    static_assert(_countof(encoded_armor_names) == static_cast<size_t>(ResilienceDetail::Count));

    // 武器编码名称（注意：这些是游戏内部编码字符串，不可翻译）
    // 此数组按 HallOfMonumentsModule::Detail 枚举中武器的顺序作为键
    // 即索引 0 为毁灭者斧
    const wchar_t* encoded_weapon_names[] = {
        L"\x8101\x7776\xCCA9\xBAA8\x10E0", // 毁灭者斧
        L"\x8101\x7777\xA0D7\x9027\x2458", // 毁灭者弓
        L"\x8101\x7778\xB879\xDFF6\x3310", // 毁灭者匕首
        L"\x8101\x7779\x83CC\xCECC\x5CA4", // 毁灭者副手
        L"\x8101\x777A\xDC41\x9DBE\x663A", // 毁灭者锤
        L"\x8101\x777B\xB050\xBB40\x245B", // 毁灭者魔杖
        L"\x8101\x777C\xFFD7\xE16E\x4BEE", // 毁灭者镰刀
        L"\x8101\x777D\xCA7A\xB9E2\x3BDD", // 毁灭者盾
        L"\x8101\x777E\xB3DD\x830E\x4CA1", // 毁灭者矛
        L"\x8101\x777F\xCCF0\xA1E7\x2A5E", // 毁灭者法杖
        L"\x8101\x7780\x8DAB\xA3C4\x48B1", // 毁灭者剑

        L"\x108\x107"
        "Tormented Axe\x1", // 痛苦斧
        L"\x108\x107"
        "Tormented Bow\x1", // 痛苦弓
        L"\x108\x107"
        "Tormented Daggers\x1", // 痛苦匕首
        L"\x108\x107"
        "Tormented Focus\x1", // 痛苦副手
        L"\x108\x107"
        "Tormented Hammer\x1", // 痛苦锤
        L"\x108\x107"
        "Tormented Scepter\x1", // 痛苦权杖
        L"\x108\x107"
        "Tormented Scythe\x1",            // 痛苦镰刀
        L"\x8102\x45E0\xDC95\xF3B4\x404", // 痛苦盾
        L"\x108\x107"
        "Tormented Spear\x1",             // 痛苦矛
        L"\x8102\x45E2\xA1E4\xBB9E\x2A8", // 痛苦法杖
        L"\x108\x107"
        "Tormented Sword\x1", // 痛苦剑

        L"\x8102\xEDD\xD560\xED5C\x2578",  // 压迫者斧
        L"\x8102\xEDE\x945E\x98D8\x4698",  // 压迫者弓
        L"\x8102\x2C72\xCC78\xA2B4\x5F85", // 压迫者匕首
        L"\x8102\x6B5C\x9773\xD778\x3567", // 压迫者副手
        L"\x108\x107"
        "Oppressor's Hammer\x1",           // 压迫者锤
        L"\x8102\x6B5E\x9964\xCAF9\x700D", // 压迫者权杖
        L"\x8102\x6B5F\x8E3F\x8145\x1956", // 压迫者镰刀
        L"\x8102\x6B60\xFC25\xD943\x329F", // 压迫者盾
        L"\x8102\x6B61\xC1EA\xD1AF\x4F8",  // 压迫者矛
        L"\x8102\x6B62\xB5BE\xA6EE\x2937", // 压迫者法杖
        L"\x8102\x6B63\x9222\xF8D1\x5715", // 压迫者剑
    };
    static_assert(_countof(encoded_weapon_names) == static_cast<size_t>(ValorDetail::Count));

    // 迷你宠物编码名称（注意：这些是游戏内部编码字符串，不可翻译）
    // 警告：不要尝试重新排序此列表；这些键用于在追踪器中跨存档识别迷你宠物。
    const wchar_t* encoded_minipet_names[] = {

        L"\x8101\x730C",                   // 艾特克斯
        L"\x8102\x4509",                   // 深渊恶魔
        L"\x8101\x682F",                   // 阿苏拉
        L"\x8102\x450A",                   // 啊啊啊啊啊啊啊黑兽
        L"\x8102\x2176\xA5D1\x8A87\x6C96", // 黑恐鸟雏鸟
        L"\x8101\x3E8\xB1EC\xA471\xA12",   // 骨龙
        L"\x8102\x5387\x8E7B\xC70D\x6A66", // 棕色兔子
        L"\x8101\x3F2\xA392\x9F0A\x2FD5",  // 燃烧泰坦
        L"\x8102\x4515",                   // 洞穴蜘蛛
        L"\x8102\x3F68\xB9DD\x9EEE\x73A7", // 天界狗
        L"\x8102\x3F62\xF4D2\xB3A7\x50D9", // 天界龙
        L"\x8102\x3F64\x91D1\xFF08\x1406", // 天界马
        L"\x8102\x3F66\xC842\x9CB3\xF11",  // 天界猴
        L"\x8102\x3F5F\xEAB3\x9E25\x22C9", // 天界牛
        L"\x8102\x3F5D\x82A5\xCB19\x49F7", // 天界猪
        L"\x8102\x3F61\xD886\xC8C6\x70BD", // 天界兔
        L"\x8102\x3F5E\xA749\x8783\x5EE0", // 天界鼠
        L"\x8102\x3F67\xA65A\xEF3A\x7E4F", // 天界鸡
        L"\x8102\x3F65\xF85B\xEFA1\x5929", // 天界羊
        L"\x8102\x3F63\xBF56\xE485\x37B7", // 天界蛇
        L"\x8102\x3F60\xB396\xABEF\x3CA6", // 天界虎
        L"\x8102\x3272",                   // 角龙
        L"\x8101\x3E9\xDD98\xBEEE\x5ABA",  // 夏尔萨满
        L"\x8102\x4514",                   // 触云猿
        L"\x8101\x76DD",                   // 血肉毁灭者
        L"\x8102\x5945",                   // 德雷德蛮兵
        L"\x8103\x6F9",                    // 巡礼者荀饶
        L"\x8101\x7303",                   // 精灵
        L"\x8101\x730B",                   // 火焰小鬼
        L"\x8102\x450E",                   // 森林牛头怪
        L"\x8102\x450B",                   // 弗利兹
        L"\x8101\x3E7\xFB88\xF384\x7D78",  // 真菌野猪
        L"\x8102\x122E",                   // 格劳尔
        L"\x8101\x2EA1\xAECB\x8321\x55B2", // 灰巨人
        L"\x8101\x66FD\xB774\x8AC7\x4878", // 闪电疾驰
        L"\x8102\x7446",                   // 公会领主
        L"\x8101\x7300",                   // 格温
        L"\x8102\x5385\xB5F4\xDD41\x6DE",  // 格温娃娃
        L"\x8101\x7308",                   // 哈比游侠
        L"\x8101\x7307",                   // 赫克特战士
        L"\x8101\x3EC\xC067\xCE23\x645C",  // 九头蛇
        L"\x8102\x450C",                   // 伊鲁坎吉
        L"\x8101\xF80\xE0F2\xABE6\x2AB1",  // 岛屿守护者
        L"\x8101\x3ED\xF9DD\xCFE6\x144F",  // 翡翠战甲
        L"\x8101\x7309",                   // 巨兽
        L"\x8101\x3F3\x870C\xA10D\xB74",   // 丛林巨魔
        L"\x8101\xF7C\xB5D7\xEC6E\x4FF3",  // 卡纳赛
        L"\x8101\x3EE\xFEE1\x8908\xA80",   // 麒麟
        L"\x8101\x7305",                   // 科斯
        L"\x8101\x9Bc",                    // 库纳旺
        L"\x8103\xAF7\xCFC2\x99A2\x3DDC",  // 军团士兵
        L"\x8101\x7302",                   // 巫妖
        L"\x8101\xF81\x9D3D\xF28A\x2F4E",  // 长毛雪人
        L"\x8101\xF7D\xBCAD\xF3B9\xC22",   // 纳迦唤雨者
        L"\x8101\x3EB\xFBB9\xF538\x2C8",   // 死灵骑士
        L"\x8102\x4510",                   // 诺恩熊
        L"\x8102\x450D",                   // 疯王索恩
        L"\x8102\x5CC5",                   // 疯王卫兵
        L"\x8101\x39EF\xC406\xC4C7\x7D88", // 马利克斯
        L"\x8101\x7306",                   // 曼德拉草小鬼
        L"\x8103\x6F8",                    // 大臣雷子
        L"\x8102\x450F",                   // 穆萨特
        L"\x8101\xF7E\x95BD\xB2B4\x51E7",  // 鬼面
        L"\x8102\x4511",                   // 软泥怪
        L"\x8101\x7304",                   // 帕拉瓦·乔科
        L"\x8101\xf7f\x8670\x899e\x3c46",   // 熊猫
        L"\x8101\x66FC\xC207\xBD26\x40CB", // 猪
        L"\x8101\x3C78",                   // 北极熊
        L"\x8101\x3EF\xE477\xD632\x3AC",   // 鲁里克王子
        L"\x8102\x4512",                   // 迅猛龙
        L"\x8102\x4513",                   // 咆哮以太
        L"\x8101\x3F0\x98B5\xB78C\x1EBD",  // 史洛
        L"\x8101\x6BB6",                   // 史洛肯刺客
        L"\x8101\x3F4\x9D9B\xEDB3\x3EA7",  // 攻城龟
        L"\x8101\x3F1\xCAA4\xC7B1\x77B8",  // 神庙守卫
        L"\x8101\x730D",                   // 荆棘狼
        L"\x8101\x5EE0",                   // 瓦蕾什
        L"\x8101\x6BB7",                   // 薇茹
        L"\x8101\x7301",                   // 水精灵
        L"\x8101\x3EA\xB3F6\xFFBA\x1293",  // 鞭尾吞噬者
        L"\x8102\x4516",                   // 白兔
        L"\x8101\x730A",                   // 风骑士
        L"\x8102\x5944",                   // 疯狂之语
        L"\x8102\x5389\xD54E\xE94E\x5120", // 亚金顿
        L"\x8101\x6BB8",                   // 泽德·暗蹄

        L"\x8102\x5946", // 恐网德莱德
        L"\x8102\x5947", // 憎恶
        L"\x8102\x5948", // 克雷特·尼奥斯
        L"\x8102\x5949", // 沙漠狮鹫
        L"\x8102\x594A", // 克维尔杜尔夫
        L"\x8102\x594B", // 克查尔·斯莱
        L"\x8102\x594C", // 约拉
        L"\x8102\x594D", // 流石元素
        L"\x8102\x594E", // 年兽
        L"\x8102\x594F", // 达格纳·石板
        L"\x8102\x5950", // 火焰巨灵
        L"\x8102\x5952", // 詹西尔之眼

        L"\x8102\x5CC6", // 惩戒爬行者
        L"\x8102\x5E49", // 杜姆

        L"\x8102\x6505", // 先知
        L"\x8102\x6506", // 攻城吞噬者
        L"\x8102\x6507", // 碎片狼
        L"\x8102\x6508", // 火龙
        L"\x8102\x6509", // 山巅巨人牧者
        L"\x8102\x650A", // 奥菲尔·纳瓦利
        L"\x8102\x650B", // 钴蓝蝎尾狮
        L"\x8102\x650C", // 天灾蝠鲼
        L"\x8102\x650D", // 文塔里
        L"\x8102\x650E", // 奥拉
        L"\x8102\x650F", // 糖果匠马利
        L"\x8102\x6510", // 朱·哈努库
        L"\x8102\x6511", // 阿德尔伯恩国王
        L"\x8102\x6512", // M.O.X

        L"\x8102\x6799", // 萨尔玛
        L"\x8102\x679A", // 莉维亚
        L"\x8102\x679B", // 伊文尼亚
        L"\x8102\x679C", // 忏悔者以赛亚
        L"\x8102\x679D", // 忏悔者多里安
        L"\x8102\x679E", // 和平卫士执法者

        L"\x8102\x7526", // 大祭司张
        L"\x8102\x7527", // 幽灵祭司
        L"\x8102\x7528", // 裂隙守望者

        L"\x8103\xA3B\xEEC0\xD3AD\x6648", // 世界知名竞速甲虫
        L"\x8101\x6730",                  // 幽灵英雄（需要）
    };

    // file_id => GWW 文件名映射，用于图像
    struct ItemUpgradeInfo {
        uint32_t file_id = 0;
        const char* wiki_filename;
        const char* completion_category;
    };

    const char* completion_category_weapon_upgrades = "武器升级";
    const char* completion_category_runes_insignias = "符文 & 纹章";
    const char* completion_category_inscriptions = "铭文";
    std::vector<ItemUpgradeInfo> item_upgrades_by_file_id = {
        // 战士
        {0x40e8b, "Knight's Insignia.png", completion_category_runes_insignias},
        {0x40e8c, "Lieutenant's Insignia.png", completion_category_runes_insignias},
        {0x40e8d, "Stonefist Insignia.png", completion_category_runes_insignias},
        {0x40e8e, "Dreadnought Insignia.png", completion_category_runes_insignias},
        {0x40e8f, "Sentinel's Insignia.png", completion_category_runes_insignias},
        {0x5c8b, "Rune Warrior Minor.png", completion_category_runes_insignias},
        {0x2514b, "Rune Warrior Major.png", completion_category_runes_insignias},
        {0x2514c, "Rune Warrior Sup.png", completion_category_runes_insignias},

        // 游侠
        {0x40e90, "Frostbound Insignia.png", completion_category_runes_insignias},
        {0x40e92, "Pyrebound Insignia.png", completion_category_runes_insignias},
        {0x40e93, "Stormbound Insignia.png", completion_category_runes_insignias},
        {0x40e95, "Scout's Insignia.png", completion_category_runes_insignias},
        {0x40e91, "Earthbound Insignia.png", completion_category_runes_insignias},
        {0x40e94, "Beastmaster's Insignia.png", completion_category_runes_insignias},
        {0x5c90, "Rune Ranger Minor.png", completion_category_runes_insignias},
        {0x25151, "Rune Ranger Major.png", completion_category_runes_insignias},
        {0x25152, "Rune Ranger Sup.png", completion_category_runes_insignias},

        // 僧侣
        {0x40e88, "Wanderer's Insignia.png", completion_category_runes_insignias},
        {0x40e89, "Disciple's Insignia.png", completion_category_runes_insignias},
        {0x40e8a, "Anchorite's Insignia.png", completion_category_runes_insignias},
        {0x5c86, "Rune Monk Minor.png", completion_category_runes_insignias},
        {0x25145, "Rune Monk Major.png", completion_category_runes_insignias},
        {0x25146, "Rune Monk Sup.png", completion_category_runes_insignias},

        // 死灵法师
        {0x40e7d, "Bloodstained Insignia.png", completion_category_runes_insignias},
        {0x40e7e, "Tormentor's Insignia.png", completion_category_runes_insignias},
        {0x40e80, "Bonelace Insignia.png", completion_category_runes_insignias},
        {0x40e81, "Minion Master's Insignia.png", completion_category_runes_insignias},
        {0x40e82, "Blighter's Insignia.png", completion_category_runes_insignias},
        {0x40e7f, "Undertaker's Insignia.png", completion_category_runes_insignias},
        {0x5c7c, "Rune Necromancer Minor.png", completion_category_runes_insignias},
        {0x25139, "Rune Necromancer Major.png", completion_category_runes_insignias},
        {0x2513a, "Rune Necromancer Sup.png", completion_category_runes_insignias},

        // 幻术师
        {0x40e75, "Virtuoso's Insignia.png", completion_category_runes_insignias},
        {0x40e73, "Artificer's Insignia.png", completion_category_runes_insignias},
        {0x40e74, "Prodigy's Insignia.png", completion_category_runes_insignias},
        {0x5c77, "Rune Mesmer Minor.png", completion_category_runes_insignias},
        {0x25133, "Rune Mesmer Major.png", completion_category_runes_insignias},
        {0x25134, "Rune Mesmer Sup.png", completion_category_runes_insignias},

        // 元素使
        {0x40e84, "Hydromancer Insignia.png", completion_category_runes_insignias},
        {0x40e85, "Geomancer Insignia.png", completion_category_runes_insignias},
        {0x40e86, "Pyromancer Insignia.png", completion_category_runes_insignias},
        {0x40e87, "Aeromancer Insignia.png", completion_category_runes_insignias},
        {0x40e82, "Blighter's Insignia.png", completion_category_runes_insignias},
        {0x5c81, "Rune Elementalist Minor.png", completion_category_runes_insignias},
        {0x2513f, "Rune Elementalist Major.png", completion_category_runes_insignias},
        {0x25140, "Rune Elementalist Sup.png", completion_category_runes_insignias},

        // 刺客
        {0x40e6f, "Vanguard's Insignia.png", completion_category_runes_insignias},
        {0x40e70, "Infiltrator's Insignia.png", completion_category_runes_insignias},
        {0x40e71, "Saboteur's Insignia.png", completion_category_runes_insignias},
        {0x40e72, "Nightstalker's Insignia.png", completion_category_runes_insignias},
        {0x283ea, "Rune Assassin Minor.png", completion_category_runes_insignias},
        {0x283eb, "Rune Assassin Major.png", completion_category_runes_insignias},
        {0x283ec, "Rune Assassin Sup.png", completion_category_runes_insignias},

        // 祭祀
        {0x40e98, "Shaman's Insignia.png", completion_category_runes_insignias},
        {0x40e99, "Ghost Forge Insignia.png", completion_category_runes_insignias},
        {0x40e9a, "Mystic's Insignia.png", completion_category_runes_insignias},
        {0x283f1, "Rune Ritualist Minor.png", completion_category_runes_insignias},
        {0x283f2, "Rune Ritualist Major.png", completion_category_runes_insignias},
        {0x283f3, "Rune Ritualist Sup.png", completion_category_runes_insignias},

        // 神唤使
        {0x40e96, "Windwalker Insignia.png", completion_category_runes_insignias},
        {0x40e97, "Forsaken Insignia.png", completion_category_runes_insignias},
        {0x3244d, "Rune Dervish Minor.png", completion_category_runes_insignias},
        {0x32452, "Rune Dervish Major.png", completion_category_runes_insignias},
        {0x32453, "Rune Dervish Sup.png", completion_category_runes_insignias},

        // 圣言者
        {0x40e9b, "Centurion's Insignia.png", completion_category_runes_insignias},
        {0x32454, "Rune Paragon Minor.png", completion_category_runes_insignias},
        {0x32455, "Rune Paragon Major.png", completion_category_runes_insignias},
        {0x32456, "Rune Paragon Sup.png", completion_category_runes_insignias},

        // 通用
        {0x40e77, "Survivor Insignia.png", completion_category_runes_insignias},
        {0x40e76, "Radiant Insignia.png", completion_category_runes_insignias},
        {0x40e78, "Stalwart Insignia.png", completion_category_runes_insignias},
        {0x40e79, "Brawler's Insignia.png", completion_category_runes_insignias},
        {0x40e79, "Blessed Insignia.png", completion_category_runes_insignias},
        {0x40e7b, "Herald's Insignia.png", completion_category_runes_insignias},
        {0x40e7c, "Sentry's Insignia.png", completion_category_runes_insignias},
        {0x40e7a, "Blessed Insignia.png", completion_category_runes_insignias},
        {0x2512c, "Rune All Minor.png", completion_category_runes_insignias},
        {0x2512d, "Rune All Major.png", completion_category_runes_insignias},
        {0x2512e, "Rune All Sup.png", completion_category_runes_insignias},

        // 铭文
        {0x32442, "Inscription martial weapons.png", completion_category_inscriptions},
        {0x32441, "Inscription spellcasting weapons.png", completion_category_inscriptions},
        {0x32444, "Inscription weapons.png", completion_category_inscriptions},
        {0x32443, "Inscription focus items or shields.png", completion_category_inscriptions},

        // 武器
        {0x16602, "Axe Grip.png", completion_category_weapon_upgrades},
        {0x4da0, "Axe Haft.png", completion_category_weapon_upgrades},
        {0x16605, "Bow Grip.png", completion_category_weapon_upgrades},
        {0x16607, "Bow String.png", completion_category_weapon_upgrades},
        {0x16606, "Hammer Grip.png", completion_category_weapon_upgrades},
        {0x4da1, "Hammer Haft.png", completion_category_weapon_upgrades},
        {0x16608, "Sword Pommel.png", completion_category_weapon_upgrades},
        {0x4e2f, "Sword Hilt.png", completion_category_weapon_upgrades},
        {0x32459, "Focus Core.png", completion_category_weapon_upgrades},
        {0x3245c, "Wand Wrapping.png", completion_category_weapon_upgrades},
        {0x32460, "Shield Handle.png", completion_category_weapon_upgrades},
        {0x283bb, "Staff Head.png", completion_category_weapon_upgrades},
        {0x283bc, "Staff Wrapping.png", completion_category_weapon_upgrades},
        {0x3245d, "Scythe Grip.png", completion_category_weapon_upgrades},
        {0x32447, "Scythe Snathe.png", completion_category_weapon_upgrades},
        {0x32461, "Spear Grip.png", completion_category_weapon_upgrades},
        {0x32448, "Spearhead.png", completion_category_weapon_upgrades},
        {0x283e5, "Dagger Tang.png", completion_category_weapon_upgrades},
    };
}

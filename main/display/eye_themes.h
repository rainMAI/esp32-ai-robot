#ifndef EYE_THEMES_H
#define EYE_THEMES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 眼睛主题配置结构体
typedef struct {
    const char* name;           // 主题名称
    const uint16_t* sclera;     // 眼白数据指针
    const uint16_t* iris;       // 虹膜数据指针
    int width;                  // 眼睛宽度
    int height;                 // 眼睛高度
    int iris_min;               // 虹膜最小缩放值
    int iris_max;               // 虹膜最大缩放值
} EyeThemeConfig;

// 原有的大尺寸主题 (375x375) - 这些在 eyes_data.h 中定义,无需 extern 声明
// extern const uint16_t sclera_xingkong[];
// extern const uint16_t sclera_shuimu[];
// extern const uint16_t sclera_keji[];
// extern const uint16_t iris_xingkong[];

// 方案三:使用命名空间保护的独立主题文件
// 龙眼主题数据完全独立,不会与其他主题冲突
// 每个主题都有自己的命名空间,可以添加无限多个主题

// 新的 graphics 主题 (180x180 或 200x200)
// 暂时无法直接使用,存在符号冲突
// 解决方案:参考 dragon_theme_data.h 的实现方式
// 为每个主题创建带命名空间的独立文件
// extern const uint16_t sclera_cat[180][180];        // catEye
// extern const uint16_t sclera_default[200][200];    // defaultEye
// extern const uint16_t sclera_doe[180][180];        // doeEye
// extern const uint16_t sclera_dragon[180][180];     // dragonEye (已迁移到DragonTheme)
// extern const uint16_t sclera_goat[180][180];       // goatEye
// extern const uint16_t logo[200][200];              // logo
// extern const uint16_t sclera_nauga[180][180];      // naugaEye
// extern const uint16_t sclera_newt[180][180];       // newtEye
// extern const uint16_t sclera_noSclera[180][180];   // noScleraEye
// extern const uint16_t sclera_owl[180][180];        // owlEye
// extern const uint16_t sclera_terminator[180][180]; // terminatorEye

// 眼睛主题枚举
typedef enum {
    // 原有大尺寸主题 (375x375)
    EYE_THEME_XINGKONG = 0,  // 星空主题
    EYE_THEME_SHUIMU = 1,    // 水墨主题
    EYE_THEME_KEJI = 2,      // 科技主题
    EYE_THEME_DRAGON = 3,    // 龙眼主题 (使用DragonTheme命名空间)
    // Graphics 主题 (已转换为375x375)
    EYE_THEME_CAT = 4,       // 猫眼主题 (使用CatTheme命名空间)
    EYE_THEME_DEFAULT = 5,   // 默认主题 (使用DefaultTheme命名空间)
    EYE_THEME_DOE = 6,       // 鹿眼主题 (使用DoeTheme命名空间)
    EYE_THEME_GOAT = 7,      // 山羊眼主题 (使用GoatTheme命名空间)
    EYE_THEME_NAUGA = 8,     // nauga眼主题 (使用NaugaTheme命名空间)
    EYE_THEME_NEWT = 9,      // 蝾螈眼主题 (使用NewtTheme命名空间)
    EYE_THEME_NOSCLERA = 10, // 无眼白主题 (使用NoscleraTheme命名空间)
    EYE_THEME_OWL = 11,      // 猫头鹰眼主题 (使用OwlTheme命名空间)
    EYE_THEME_TERMINATOR = 12 // 终结者眼主题 (使用TerminatorTheme命名空间)
} EyeTheme;

// 获取主题配置
const EyeThemeConfig* GetEyeThemeConfig(EyeTheme theme);

// 获取主题名称
const char* GetEyeThemeName(EyeTheme theme);

#ifdef __cplusplus
}
#endif

#endif // EYE_THEMES_H

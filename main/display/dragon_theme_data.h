#ifndef DRAGON_THEME_DATA_H
#define DRAGON_THEME_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 使用命名空间避免符号冲突
// 龙眼主题 - 完全独立的主题数据
// 尺寸已从原始160x160调整为375x375以匹配其他主题

namespace DragonTheme {

// 龙眼参数常量 (调整为375x375)
constexpr int IRIS_MIN = 180;  // 使用与xingkong相同的范围
constexpr int IRIS_MAX = 280;
constexpr int SCLERA_WIDTH = 375;
constexpr int SCLERA_HEIGHT = 375;

// 眼白数据 (375x375 = 140625 pixels)
// 从原始160x160龙眼数据通过最近邻插值放大
// 使用独特的名称避免链接冲突
extern const uint16_t* const dragon_sclera;

// 为了兼容性,提供别名
inline const uint16_t* const sclera() { return dragon_sclera; }

// 虹膜数据 (256x64 = 16384 pixels)
// 从原始80x512龙眼虹膜调整而来
extern const uint16_t* const dragon_iris;

} // namespace DragonTheme

#ifdef __cplusplus
}
#endif

#endif // DRAGON_THEME_DATA_H

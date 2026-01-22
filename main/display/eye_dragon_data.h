#ifndef EYE_DRAGON_DATA_H
#define EYE_DRAGON_DATA_H

// 龙眼主题数据 - 从 dragonEye.h 提取并重命名变量
// 避免与其他 graphics 文件冲突

#define DRAGON_IRIS_MIN  80
#define DRAGON_IRIS_MAX 400
#define DRAGON_SCLERA_WIDTH  160
#define DRAGON_SCLERA_HEIGHT 160

// 这里我们使用宏技巧来重命名变量
// 包含原始文件但重命名关键变量
#define sclera sclera_dragon_internal
#define iris iris_dragon_internal
#define upper upper_dragon_internal
#define lower lower_dragon_internal
#define polar polar_dragon_internal
#define IRIS_MIN DRAGON_IRIS_MIN
#define IRIS_MAX DRAGON_IRIS_MAX
#define SCLERA_WIDTH DRAGON_SCLERA_WIDTH
#define SCLERA_HEIGHT DRAGON_SCLERA_HEIGHT
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128
#define IRIS_WIDTH 160
#define IRIS_HEIGHT 160
#define IRIS_MAP_WIDTH 512
#define IRIS_MAP_HEIGHT 80

// 包含原始数据
#include "graphics/dragonEye.h"

// 取消重定义
#undef sclera
#undef iris
#undef upper
#undef lower
#undef polar
#undef IRIS_MIN
#undef IRIS_MAX
#undef SCLERA_WIDTH
#undef SCLERA_HEIGHT
#undef SCREEN_WIDTH
#undef SCREEN_HEIGHT
#undef IRIS_WIDTH
#undef IRIS_HEIGHT
#undef IRIS_MAP_WIDTH
#undef IRIS_MAP_HEIGHT

// 导出统一的接口
extern const uint16_t sclera_dragon_internal[160][160];

#endif // EYE_DRAGON_DATA_H

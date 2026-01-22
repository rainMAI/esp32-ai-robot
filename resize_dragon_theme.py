#!/usr/bin/env python3
"""
调整龙眼主题尺寸从160x160到375x375
使用图像插值算法放大龙眼数据
"""

import re
import sys

def extract_array_from_dragon_eye():
    """从dragonEye.h提取原始160x160数据"""
    input_file = 'main/display/graphics/dragonEye.h'

    with open(input_file, 'r', encoding='utf-8') as f:
        content = f.read()

    # 提取sclera数据
    pattern = r'const\s+uint16_t\s+sclera\[SCLERA_HEIGHT\]\s*\[SCLERA_WIDTH\]\s*=\s*\{([^;]+)\};'
    match = re.search(pattern, content, re.DOTALL)

    if not match:
        print("错误: 无法找到sclera数据")
        sys.exit(1)

    data_str = match.group(1)
    values = re.findall(r'0X[0-9A-Fa-f]+', data_str)
    sclera_160 = [int(v, 16) for v in values]

    if len(sclera_160) != 160 * 160:
        print(f"错误: sclera数据大小不匹配,期望25600,实际{len(sclera_160)}")
        sys.exit(1)

    print(f"✓ 提取到160x160龙眼数据 ({len(sclera_160)} 像素)")
    return sclera_160

def bilinear_interpolate(src, src_width, src_height, dst_width, dst_height):
    """双线性插值放大图像"""
    result = []

    x_ratio = src_width / dst_width
    y_ratio = src_height / dst_height

    for y in range(dst_height):
        for x in range(dst_width):
            # 计算源图像中的对应位置
            src_x = x * x_ratio
            src_y = y * y_ratio

            # 获取四个邻近像素的坐标
            x1 = int(src_x)
            y1 = int(src_y)
            x2 = min(x1 + 1, src_width - 1)
            y2 = min(y1 + 1, src_height - 1)

            # 计算权重
            dx = src_x - x1
            dy = src_y - y1

            # 获取四个像素值
            p11 = src[y1 * src_width + x1]
            p12 = src[y1 * src_width + x2]
            p21 = src[y2 * src_width + x1]
            p22 = src[y2 * src_width + x2]

            # 双线性插值 (RGB565格式)
            # 对RGB565进行分量插值
            def rgb565_to_components(val):
                r = (val >> 11) & 0x1F
                g = (val >> 5) & 0x3F
                b = val & 0x1F
                return r, g, b

            def components_to_rgb565(r, g, b):
                return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F)

            r11, g11, b11 = rgb565_to_components(p11)
            r12, g12, b12 = rgb565_to_components(p12)
            r21, g21, b21 = rgb565_to_components(p21)
            r22, g22, b22 = rgb565_to_components(p22)

            # 插值计算
            r = int(r11 * (1 - dx) * (1 - dy) + r12 * dx * (1 - dy) +
                    r21 * (1 - dx) * dy + r22 * dx * dy)
            g = int(g11 * (1 - dx) * (1 - dy) + g12 * dx * (1 - dy) +
                    g21 * (1 - dx) * dy + g22 * dx * dy)
            b = int(b11 * (1 - dx) * (1 - dy) + b12 * dx * (1 - dy) +
                    b21 * (1 - dx) * dy + b22 * dx * dy)

            result.append(components_to_rgb565(r, g, b))

    return result

def nearest_neighbor_interpolate(src, src_width, src_height, dst_width, dst_height):
    """最近邻插值(更简单,但可能有锯齿)"""
    result = []
    x_ratio = src_width / dst_width
    y_ratio = src_height / dst_height

    for y in range(dst_height):
        for x in range(dst_width):
            src_x = min(int(x * x_ratio), src_width - 1)
            src_y = min(int(y * y_ratio), src_height - 1)
            result.append(src[src_y * src_width + src_x])

    return result

def create_resized_dragon_theme():
    """创建调整尺寸后的龙眼主题文件"""

    # 提取原始160x160数据
    sclera_160 = extract_array_from_dragon_eye()

    # 使用最近邻插值放大到375x375
    print("正在放大到375x375...")
    sclera_375 = nearest_neighbor_interpolate(sclera_160, 160, 160, 375, 375)
    print(f"✓ 放大完成 ({len(sclera_375)} 像素)")

    # 生成新的头文件
    header_file = 'main/display/dragon_theme_data.h'
    with open(header_file, 'w', encoding='utf-8') as f:
        f.write("""#ifndef DRAGON_THEME_DATA_H
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
extern const uint16_t sclera[375 * 375];

// 虹膜数据 - 暂时使用xingkong的虹膜
// extern const uint16_t iris[];

} // namespace DragonTheme

#ifdef __cplusplus
}
#endif

#endif // DRAGON_THEME_DATA_H
""")

    print(f"✓ 生成头文件: {header_file}")

    # 生成实现文件
    impl_file = 'main/display/dragon_theme_data.cc'
    with open(impl_file, 'w', encoding='utf-8') as f:
        f.write('#include "dragon_theme_data.h"\n\n')
        f.write('namespace DragonTheme {\n\n')
        f.write('// 眼白数据 (375x375 = 140625 pixels)\n')
        f.write('// 从原始160x160龙眼数据通过最近邻插值放大\n')
        f.write('const uint16_t sclera[375 * 375] = {\n')

        # 每16个值一行
        for i in range(0, len(sclera_375), 16):
            line_values = sclera_375[i:i+16]
            line = '    ' + ''.join([f'0x{v:04X}, ' for v in line_values]) + '\n'
            f.write(line)

        f.write('};\n\n')
        f.write('} // namespace DragonTheme\n')

    print(f"✓ 生成实现文件: {impl_file}")
    print("\n✓ 龙眼主题尺寸调整完成!")
    print(f"  原始尺寸: 160x160 (25600 pixels)")
    print(f"  新尺寸:   375x375 (140625 pixels)")
    print(f"  放大倍数: {375/160:.2f}x")
    print(f"  插值方法: 最近邻插值")

if __name__ == '__main__':
    create_resized_dragon_theme()

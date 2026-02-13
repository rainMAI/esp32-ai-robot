#!/usr/bin/env python3
"""
提取龙眼完整主题数据
使用命名空间避免符号冲突
"""

import re
import sys

def extract_array_data(content, array_name):
    """从C代码中提取数组数据"""
    # 匹配 const uint16_t/uint8_t array_name[...][...] = { ... };
    # 首先找到数组声明
    decl_pattern = rf'const\s+(uint16_t|uint8_t)\s+{array_name}\s*\[(.*?)\]\s*\[(.*?)\]'
    decl_match = re.search(decl_pattern, content)

    if not decl_match:
        print(f"警告: 未找到数组 {array_name} 的声明")
        return None, None

    data_type = decl_match.group(1)
    dim1 = decl_match.group(2)
    dim2 = decl_match.group(3)

    # 检查是否是宏定义
    if dim1 in ['SCLERA_HEIGHT', 'SCLERA_WIDTH', 'SCREEN_HEIGHT', 'SCREEN_WIDTH',
                'IRIS_MAP_HEIGHT', 'IRIS_MAP_WIDTH']:
        # 使用默认值
        size_map = {
            'SCLERA_HEIGHT': 160, 'SCLERA_WIDTH': 160,
            'SCREEN_HEIGHT': 128, 'SCREEN_WIDTH': 128,
            'IRIS_MAP_HEIGHT': 80, 'IRIS_MAP_WIDTH': 512
        }
        height = size_map.get(dim1, 0)
        width = size_map.get(dim2, 0)
    else:
        height = int(dim1) if dim1.isdigit() else 0
        width = int(dim2) if dim2.isdigit() else 0

    # 现在提取数据
    pattern = rf'const\s+(?:uint16_t|uint8_t)\s+{array_name}\s*\[[^\]]*\]\s*\[[^\]]*\]\s*=\s*\{{([^;]+)\}};'
    match = re.search(pattern, content, re.DOTALL)

    if not match:
        print(f"警告: 未找到数组 {array_name} 的数据")
        return None, None

    data_str = match.group(1)

    # 提取所有数值
    values = re.findall(r'0X[0-9A-Fa-f]+|\d+', data_str)

    # 转换为整数列表
    if data_type == 'uint16_t':
        values = [int(v, 16) if v.startswith('0X') or v.startswith('0x') else int(v) for v in values]
    else:  # uint8_t
        values = [int(v, 16) if v.startswith('0X') or v.startswith('0x') else int(v) for v in values]

    return values, (height, width)

def convert_to_one_line_c_array(values, data_type='uint16_t'):
    """将Python列表转换为C语言单行数组初始化格式"""
    if data_type == 'uint16_t':
        return ''.join([f'0x{v:04X}, ' for v in values])
    else:
        return ''.join([f'{v}, ' for v in values])

def main():
    input_file = 'main/display/graphics/dragonEye.h'
    output_file = 'main/display/dragon_theme_data.h'

    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"错误: 找不到文件 {input_file}")
        sys.exit(1)

    # 提取各个数组
    print("提取龙眼数据...")

    # 1. sclera (眼白)
    sclera_values, sclera_size = extract_array_data(content, 'sclera')
    if sclera_values:
        print(f"  sclera: {len(sclera_values)} 个像素, 尺寸: {sclera_size}")

    # 2. iris (虹膜)
    iris_values, iris_size = extract_array_data(content, 'iris')
    if iris_values:
        print(f"  iris: {len(iris_values)} 个像素, 尺寸: {iris_size}")

    # 3. polar (极坐标映射)
    polar_values, polar_size = extract_array_data(content, 'polar')
    if polar_values:
        print(f"  polar: {len(polar_values)} 个像素, 尺寸: {polar_size}")

    # 4. upper (上眼睑)
    upper_values, upper_size = extract_array_data(content, 'upper')
    if upper_values:
        print(f"  upper: {len(upper_values)} 个像素, 尺寸: {upper_size}")

    # 5. lower (下眼睑)
    lower_values, lower_size = extract_array_data(content, 'lower')
    if lower_values:
        print(f"  lower: {len(lower_values)} 个像素, 尺寸: {lower_size}")

    # 生成新的头文件
    print(f"\n生成文件: {output_file}")

    with open(output_file, 'w', encoding='utf-8') as f:
        f.write("""#ifndef DRAGON_THEME_DATA_H
#define DRAGON_THEME_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 使用命名空间避免符号冲突
// 龙眼主题 - 完全独立的主题数据
// 尺寸: 160x160 (眼白), 80x512 (虹膜映射), 128x128 (眼睑)

namespace DragonTheme {

// 龙眼参数常量
constexpr int IRIS_MIN = 80;
constexpr int IRIS_MAX = 400;
constexpr int SCLERA_WIDTH = 160;
constexpr int SCLERA_HEIGHT = 160;
constexpr int IRIS_MAP_WIDTH = 512;
constexpr int IRIS_MAP_HEIGHT = 80;
constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 128;

// 眼白数据 (160x160)
extern const uint16_t sclera[160 * 160];

// 虹膜数据 (80x512)
extern const uint16_t iris[80 * 512];

// 极坐标映射数据 (160x160)
extern const uint16_t polar[160 * 160];

// 上眼睑数据 (128x128)
extern const uint8_t upper[128 * 128];

// 下眼睑数据 (128x128)
extern const uint8_t lower[128 * 128];

} // namespace DragonTheme

#ifdef __cplusplus
}
#endif

#endif // DRAGON_THEME_DATA_H
""")

    # 生成实现文件
    impl_file = 'main/display/dragon_theme_data.cc'
    print(f"生成文件: {impl_file}")

    with open(impl_file, 'w', encoding='utf-8') as f:
        f.write("""#include "dragon_theme_data.h"

namespace DragonTheme {

// 眼白数据 (160x160 = 25600 pixels)
const uint16_t sclera[160 * 160] = {
""")

        if sclera_values:
            # 每16个值一行
            for i in range(0, len(sclera_values), 16):
                line_values = sclera_values[i:i+16]
                line = '    ' + ''.join([f'0x{v:04X}, ' for v in line_values]) + '\n'
                f.write(line)

        f.write("};\n\n")

        # iris data
        f.write("""// 虹膜数据 (80x512 = 40960 pixels)
const uint16_t iris[80 * 512] = {
""")

        if iris_values:
            for i in range(0, len(iris_values), 16):
                line_values = iris_values[i:i+16]
                line = '    ' + ''.join([f'0x{v:04X}, ' for v in line_values]) + '\n'
                f.write(line)

        f.write("};\n\n")

        # polar data
        f.write("""// 极坐标映射数据 (160x160 = 25600 pixels)
const uint16_t polar[160 * 160] = {
""")

        if polar_values:
            for i in range(0, len(polar_values), 16):
                line_values = polar_values[i:i+16]
                line = '    ' + ''.join([f'0x{v:04X}, ' for v in line_values]) + '\n'
                f.write(line)

        f.write("};\n\n")

        # upper eyelid data
        f.write("""// 上眼睑数据 (128x128 = 16384 pixels)
const uint8_t upper[128 * 128] = {
""")

        if upper_values:
            for i in range(0, len(upper_values), 16):
                line_values = upper_values[i:i+16]
                line = '    ' + ''.join([f'{v}, ' for v in line_values]) + '\n'
                f.write(line)

        f.write("};\n\n")

        # lower eyelid data
        f.write("""// 下眼睑数据 (128x128 = 16384 pixels)
const uint8_t lower[128 * 128] = {
""")

        if lower_values:
            for i in range(0, len(lower_values), 16):
                line_values = lower_values[i:i+16]
                line = '    ' + ''.join([f'{v}, ' for v in line_values]) + '\n'
                f.write(line)

        f.write("};\n\n")

        f.write("""} // namespace DragonTheme
""")

    print("\n✓ 龙眼主题数据提取完成!")
    print(f"  - {output_file}")
    print(f"  - {impl_file}")
    print("\n使用方法:")
    print("  在 eye_themes.cc 中:")
    print("    #include \"dragon_theme_data.h\"")
    print("    using namespace DragonTheme;")
    print("    .sclera = DragonTheme::sclera,")

if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""
简化版:批量转换graphics文件夹中的眼睛主题
只转换sclera(眼白),虹膜统一使用xingkong
"""

import re
import sys
import os

# 需要处理的主题文件列表
THEME_FILES = [
    ('catEye.h', 180, 180),
    ('defaultEye.h', 200, 200),
    ('doeEye.h', 180, 180),
    ('goatEye.h', 180, 180),
    ('naugaEye.h', 180, 180),
    ('newtEye.h', 180, 180),
    ('noScleraEye.h', 180, 180),
    ('owlEye.h', 180, 180),
    ('terminatorEye.h', 180, 180)
]

def extract_sclera_data(file_path, expected_width, expected_height):
    """从主题文件提取sclera数据"""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 提取sclera数组
    pattern = r'const\s+uint16_t\s+sclera\[(.*?)\]\s*\[(.*?)\]\s*=\s*\{([^;]+)\};'
    match = re.search(pattern, content, re.DOTALL)

    if not match:
        return None

    values_str = match.group(3)
    values = re.findall(r'0X[0-9A-Fa-f]+', values_str)
    sclera_data = [int(v, 16) for v in values]

    print(f"  ✓ 提取sclera: {len(sclera_data)} 像素")
    return sclera_data

def resize_sclera(sclera_data, src_w, src_h, dst_w=375, dst_h=375):
    """调整sclera尺寸到375x375"""
    result = []
    x_ratio = src_w / dst_w
    y_ratio = src_h / dst_h

    for y in range(dst_h):
        for x in range(dst_w):
            src_x = min(int(x * x_ratio), src_w - 1)
            src_y = min(int(y * y_ratio), src_h - 1)
            result.append(sclera_data[src_y * src_w + src_x])

    return result

def generate_theme_file(theme_name, sclera_data):
    """生成主题头文件和实现文件"""

    # 命名空间名称
    namespace = theme_name.capitalize() + 'Theme'

    # 生成头文件
    header_content = f"""#ifndef {theme_name.upper()}_THEME_DATA_H
#define {theme_name.upper()}_THEME_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {{
#endif

// {theme_name}主题 - 从graphics文件夹转换
// 使用命名空间避免符号冲突
// 尺寸调整为375x375以匹配其他主题

namespace {namespace} {{

// {theme_name}参数常量 (调整为375x375)
constexpr int IRIS_MIN = 180;
constexpr int IRIS_MAX = 280;
constexpr int SCLERA_WIDTH = 375;
constexpr int SCLERA_HEIGHT = 375;

// 眼白数据 (375x375 = 140625 pixels)
extern const uint16_t* const {theme_name}_sclera;

}} // namespace {namespace}

#ifdef __cplusplus
}}
#endif

#endif // {theme_name.upper()}_THEME_DATA_H
"""

    header_file = f'main/display/{theme_name}_theme_data.h'
    with open(header_file, 'w', encoding='utf-8') as f:
        f.write(header_content)
    print(f"  ✓ 生成头文件: {header_file}")

    # 生成实现文件
    impl_content = f"""#include "{theme_name}_theme_data.h"

namespace {namespace} {{

// 眼白数据 (375x375 = 140625 pixels)
// 从原始尺寸调整为375x375
static const uint16_t {theme_name}_sclera_data[375 * 375] = {{
"""

    # 添加sclera数据 (每16个值一行)
    for i in range(0, len(sclera_data), 16):
        line_values = sclera_data[i:i+16]
        line = '    ' + ''.join([f'0x{{v:04X}}, ' for v in line_values]) + '\n'
        impl_content += line

    impl_content += "};\n\n"
    impl_content += f"// 导出数据指针\n"
    impl_content += f"const uint16_t* const {theme_name}_sclera = {theme_name}_sclera_data;\n"
    impl_content += "\n} // namespace " + namespace + "\n"

    impl_file = f'main/display/{theme_name}_theme_data.cc'
    with open(impl_file, 'w', encoding='utf-8') as f:
        f.write(impl_content)
    print(f"  ✓ 生成实现文件: {impl_file}")

    return namespace

def main():
    graphics_dir = 'main/display/graphics'

    print("开始批量转换graphics主题...")
    print("=" * 60)

    generated_themes = []

    for theme_file, width, height in THEME_FILES:
        file_path = os.path.join(graphics_dir, theme_file)

        if not os.path.exists(file_path):
            print(f"\n⚠️  文件不存在,跳过: {theme_file}")
            continue

        theme_name = theme_file.replace('Eye.h', '').replace('.h', '')
        print(f"\n处理: {theme_file} ({width}x{height})")

        # 提取数据
        sclera_data = extract_sclera_data(file_path, width, height)

        if not sclera_data:
            print(f"  ⚠️  未找到sclera数据,跳过")
            continue

        # 调整尺寸到375x375
        print(f"  调整尺寸: {width}x{height} -> 375x375")
        sclera_data = resize_sclera(sclera_data, width, height)

        # 生成文件
        namespace = generate_theme_file(theme_name, sclera_data)
        generated_themes.append((theme_name, namespace))

    print("\n" + "=" * 60)
    print(f"✓ 完成! 共生成 {len(generated_themes)} 个主题")

    # 生成CMakeLists.txt添加项
    print("\n\n需要添加到 main/CMakeLists.txt 的源文件:")
    for theme_name, _ in generated_themes:
        print(f'    "display/{theme_name}_theme_data.cc"')

    # 生成枚举
    print("\n\n需要添加到 eye_themes.h 的枚举:")
    for i, (theme_name, _) in enumerate(generated_themes, start=4):  # 从4开始
        print(f'    EYE_THEME_{theme_name.upper()} = {i},  // {theme_name}主题')

    # 生成配置
    print("\n\n需要添加到 eye_themes.cc 的配置:")
    for theme_name, namespace in generated_themes:
        config = f"""    {{
        .name = "{theme_name}",
        .sclera = {namespace}::{theme_name}_sclera,
        .iris = iris_xingkong,  // 使用星空虹膜
        .width = 375,
        .height = 375,
        .iris_min = {namespace}::IRIS_MIN,
        .iris_max = {namespace}::IRIS_MAX
    }},"""
        print(config)

if __name__ == '__main__':
    main()

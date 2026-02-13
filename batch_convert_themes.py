#!/usr/bin/env python3
"""
批量转换graphics文件夹中的眼睛主题
使用命名空间避免符号冲突,统一调整为375x375尺寸
"""

import re
import sys
import os

# 需要处理的主题文件列表
THEME_FILES = [
    'catEye.h',
    'defaultEye.h',
    'doeEye.h',
    'goatEye.h',
    'naugaEye.h',
    'newtEye.h',
    'noScleraEye.h',
    'owlEye.h',
    'terminatorEye.h'
]

# 宏定义映射(从dragonEye.h推断)
MACRO_MAP = {
    'IRIS_MIN': 80,
    'IRIS_MAX': 400,
    'SCLERA_WIDTH': 160,
    'SCLERA_HEIGHT': 160,
    'SCREEN_WIDTH': 128,
    'SCREEN_HEIGHT': 128,
    'IRIS_MAP_WIDTH': 512,
    'IRIS_MAP_HEIGHT': 80
}

def extract_theme_data(file_path):
    """从主题文件提取数据"""
    theme_name = os.path.basename(file_path).replace('Eye.h', '').replace('.h', '')

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    data = {
        'name': theme_name,
        'sclera': None,
        'iris': None,
        'polar': None,
        'upper': None,
        'lower': None,
        'sclera_size': (0, 0),
        'iris_size': (0, 0)
    }

    # 提取sclera数据
    sclera_pattern = r'const\s+uint16_t\s+sclera\[(.*?)\]\s*\[(.*?)\]\s*=\s*\{([^}]+)\};'
    match = re.search(sclera_pattern, content, re.DOTALL)
    if match:
        dim1 = match.group(1)
        dim2 = match.group(2)
        # 解析宏定义
        height = MACRO_MAP.get(dim1, int(dim1) if dim1.isdigit() else 160)
        width = MACRO_MAP.get(dim2, int(dim2) if dim2.isdigit() else 160)
        data['sclera_size'] = (height, width)

        values_str = match.group(3)
        values = re.findall(r'0X[0-9A-Fa-f]+', values_str)
        data['sclera'] = [int(v, 16) for v in values]
        print(f"  ✓ 提取sclera: {height}x{width} ({len(values)} 像素)")

    # 提取iris数据
    iris_pattern = r'const\s+uint16_t\s+iris\[(.*?)\]\s*\[(.*?)\]\s*=\s*\{([^}]+)\};'
    match = re.search(iris_pattern, content, re.DOTALL)
    if match:
        dim1 = match.group(1)
        dim2 = match.group(2)
        height = MACRO_MAP.get(dim1, int(dim1) if dim1.isdigit() else 80)
        width = MACRO_MAP.get(dim2, int(dim2) if dim2.isdigit() else 512)
        data['iris_size'] = (height, width)

        values_str = match.group(3)
        values = re.findall(r'0X[0-9A-Fa-f]+', values_str)
        data['iris'] = [int(v, 16) for v in values]
        print(f"  ✓ 提取iris: {height}x{width} ({len(values)} 像素)")

    # 提取polar数据(如果有)
    polar_pattern = r'const\s+uint16_t\s+polar\[(.*?)\]\s*\[(.*?)\]\s*=\s*\{([^}]+)\};'
    match = re.search(polar_pattern, content, re.DOTALL)
    if match:
        values_str = match.group(3)
        values = re.findall(r'0X[0-9A-Fa-f]+', values_str)
        data['polar'] = [int(v, 16) for v in values]
        print(f"  ✓ 提取polar: {len(values)} 像素")

    return data

def resize_sclera(sclera_data, src_w, src_h, dst_w=375, dst_h=375):
    """调整sclera尺寸"""
    result = []
    x_ratio = src_w / dst_w
    y_ratio = src_h / dst_h

    for y in range(dst_h):
        for x in range(dst_w):
            src_x = min(int(x * x_ratio), src_w - 1)
            src_y = min(int(y * y_ratio), src_h - 1)
            result.append(sclera_data[src_y * src_w + src_x])

    return result

def resize_iris(iris_data, src_w, src_h, dst_w=256, dst_h=64):
    """调整iris尺寸"""
    result = []
    x_ratio = src_w / dst_w
    y_ratio = src_h / dst_h

    for y in range(dst_h):
        for x in range(dst_w):
            src_x = min(int(x * x_ratio), src_w - 1)
            src_y = min(int(y * y_ratio), src_h - 1)
            result.append(iris_data[src_y * src_w + src_x])

    return result

def generate_theme_file(theme_name, theme_data):
    """生成主题头文件和实现文件"""

    # 命名空间名称 (首字母大写)
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
constexpr int IRIS_MIN = 180;  // 使用与xingkong相同的范围
constexpr int IRIS_MAX = 280;
constexpr int SCLERA_WIDTH = 375;
constexpr int SCLERA_HEIGHT = 375;

// 眼白数据 (375x375 = 140625 pixels)
extern const uint16_t* const {theme_name}_sclera;

// 虹膜数据 (256x64 = 16384 pixels)
extern const uint16_t* const {theme_name}_iris;

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
// 从原始{theme_data['sclera_size'][0]}x{theme_data['sclera_size'][1]}调整
static const uint16_t {theme_name}_sclera_data[375 * 375] = {{
"""

    # 添加sclera数据 (每16个值一行)
    for i in range(0, len(theme_data['sclera']), 16):
        line_values = theme_data['sclera'][i:i+16]
        line = '    ' + ''.join([f'0x{{v:04X}}, ' for v in line_values]) + '\n'
        impl_content += line

    impl_content += "};\n\n"

    # 添加iris数据
    if theme_data['iris']:
        impl_content += f"// 虹膜数据 (256x64 = 16384 pixels)\n"
        impl_content += f"// 从原始{theme_data['iris_size'][0]}x{theme_data['iris_size'][1]}调整\n"
        impl_content += f"static const uint16_t {theme_name}_iris_data[256 * 64] = {{\n"

        for i in range(0, len(theme_data['iris']), 16):
            line_values = theme_data['iris'][i:i+16]
            line = '    ' + ''.join([f'0x{v:04X}, ' for v in line_values]) + '\n'
            impl_content += line

        impl_content += "};\n\n"

    # 导出指针
    impl_content += f"// 导出数据指针\n"
    impl_content += f"const uint16_t* const {theme_name}_sclera = {theme_name}_sclera_data;\n"

    if theme_data['iris']:
        impl_content += f"const uint16_t* const {theme_name}_iris = {theme_name}_iris_data;\n"

    impl_content += "\n} // namespace " + namespace + "\n"

    impl_file = f'main/display/{theme_name}_theme_data.cc'
    with open(impl_file, 'w', encoding='utf-8') as f:
        f.write(impl_content)
    print(f"  ✓ 生成实现文件: {impl_file}")

    return {
        'header': header_file,
        'impl': impl_file,
        'namespace': namespace,
        'sclera_var': f'{namespace}::{theme_name}_sclera',
        'iris_var': f'{namespace}::{theme_name}_iris' if theme_data['iris'] else None
    }

def main():
    graphics_dir = 'main/display/graphics'

    print("开始批量转换graphics主题...")
    print("=" * 60)

    generated_themes = []

    for theme_file in THEME_FILES:
        file_path = os.path.join(graphics_dir, theme_file)

        if not os.path.exists(file_path):
            print(f"⚠️  文件不存在,跳过: {theme_file}")
            continue

        print(f"\n处理: {theme_file}")

        # 提取数据
        theme_data = extract_theme_data(file_path)

        if not theme_data['sclera']:
            print(f"  ⚠️  未找到sclera数据,跳过")
            continue

        # 调整尺寸
        print(f"  调整sclera尺寸: {theme_data['sclera_size']} -> 375x375")
        theme_data['sclera'] = resize_sclera(
            theme_data['sclera'],
            theme_data['sclera_size'][1],  # width
            theme_data['sclera_size'][0]   # height
        )

        if theme_data['iris']:
            print(f"  调整iris尺寸: {theme_data['iris_size']} -> 256x64")
            theme_data['iris'] = resize_iris(
                theme_data['iris'],
                theme_data['iris_size'][1],  # width
                theme_data['iris_size'][0]   # height
            )

        # 生成文件
        theme_info = generate_theme_file(theme_data['name'], theme_data)
        generated_themes.append(theme_info)

    print("\n" + "=" * 60)
    print(f"✓ 完成! 共生成 {len(generated_themes)} 个主题")

    # 生成CMakeLists.txt添加项
    print("\n需要添加到 CMakeLists.txt 的源文件:")
    for theme in generated_themes:
        print(f'  "display/{os.path.basename(theme["impl"])}",')

    # 生成枚举和配置
    print("\n需要添加到 eye_themes.h 的枚举:")
    for i, theme in enumerate(generated_themes, start=4):  # 从4开始(dragon是3)
        theme_name = theme['header'].split('/')[-1].replace('_theme_data.h', '')
        print(f'  EYE_THEME_{theme_name.upper()} = {i},')

    print("\n需要添加到 eye_themes.cc 的配置:")
    for theme in generated_themes:
        theme_name = theme['header'].split('/')[-1].replace('_theme_data.h', '')
        iris_line = f'        .iris = {theme["iris_var"]},' if theme['iris_var'] else '        .iris = iris_xingkong,  // 使用默认虹膜'
        print(f"""  {{
        .name = "{theme_name}",
        .sclera = {theme['sclera_var']},
{iris_line}
        .width = 375,
        .height = 375,
        .iris_min = {theme['namespace']}::IRIS_MIN,
        .iris_max = {theme['namespace']}::IRIS_MAX
    }},""")

if __name__ == '__main__':
    main()

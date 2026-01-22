#!/usr/bin/env python3
"""
PNG 序列帧转 C 数组工具
将 After Effects 导出的 PNG 序列转换为 ESP32 可用的 C 数组

使用方法:
    python png_sequence_to_array.py <png_folder> <output_name> [options]

参数:
    png_folder: PNG 序列文件夹路径
    output_name: 输出的数组名称（如 "animation_happy"）

选项:
    --fps FPS: 帧率（默认 12）
    --start N: 起始帧编号（默认 0）
    --width W: 图片宽度（默认 240）
    --height H: 图片高度（默认 240）
    --rgb565: 转换为 RGB565 格式（默认）
    --no-alpha: 忽略 Alpha 通道

示例:
    # 转换 happy 文件夹中的所有 PNG
    python png_sequence_to_array.py ./happy anim_happy --fps 12

    # 转换指定范围的帧
    python png_sequence_to_array.py ./surprised anim_surprised --fps 15 --start 0
"""

import os
import sys
from pathlib import Path
from PIL import Image
import argparse

def rgb888_to_rgb565(r, g, b):
    """将 RGB888 转换为 RGB565"""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def process_png_sequence(folder_path, output_name, width=240, height=240,
                         fps=12, start_frame=0, use_alpha=True, rgb565=True):
    """
    处理 PNG 序列并生成 C 数组

    Args:
        folder_path: PNG 文件夹路径
        output_name: 输出数组名称
        width: 目标宽度
        height: 目标高度
        fps: 帧率
        start_frame: 起始帧编号
        use_alpha: 是否使用 Alpha 通道
        rgb565: 是否转换为 RGB565 格式
    """
    folder = Path(folder_path)

    # 查找所有 PNG 文件并排序
    png_files = sorted(folder.glob("*.png"),
                      key=lambda x: int(x.stem.split('_')[-1]) if x.stem.split('_')[-1].isdigit() else 0)

    if not png_files:
        print(f"错误: 在 {folder} 中未找到 PNG 文件")
        return None

    print(f"找到 {len(png_files)} 个 PNG 文件")
    print(f"分辨率: {width}x{height}")
    print(f"帧率: {fps} FPS")
    print(f"格式: {'RGB565' if rgb565 else 'RGB888'}")

    frames_data = []
    total_frames = len(png_files)

    for idx, png_file in enumerate(png_files):
        try:
            img = Image.open(png_file)

            # 调整大小
            if img.size != (width, height):
                img = img.resize((width, height), Image.Resampling.LANCZOS)

            # 转换为 RGB
            if img.mode != 'RGB':
                img = img.convert('RGB')

            pixels = img.load()

            # 处理每一帧
            frame_pixels = []
            for y in range(height):
                for x in range(width):
                    r, g, b = pixels[x, y]

                    if rgb565:
                        # RGB565 格式
                        pixel = rgb888_to_rgb565(r, g, b)
                        frame_pixels.append(f"0x{pixel:04X}")
                    else:
                        # RGB888 格式
                        pixel = (r << 16) | (g << 8) | b
                        frame_pixels.append(f"0x{pixel:06X}")

            frames_data.append(frame_pixels)
            print(f"处理帧 {idx + 1}/{total_frames}: {png_file.name}")

        except Exception as e:
            print(f"处理 {png_file} 时出错: {e}")
            continue

    if not frames_data:
        print("错误: 没有成功处理任何帧")
        return None

    # 生成 C 头文件内容
    header_content = generate_c_header(output_name, frames_data,
                                      width, height, fps, total_frames, rgb565)

    return header_content

def generate_c_header(name, frames_data, width, height, fps, frame_count, rgb565):
    """生成 C 头文件内容"""

    pixel_size = 2 if rgb565 else 3  # RGB565 = 2字节, RGB888 = 3字节
    bytes_per_frame = width * height * pixel_size
    total_bytes = bytes_per_frame * frame_count

    # 计算大约的 Flash 占用
    kb_size = total_bytes / 1024

    header = f"""/**
 * 自动生成的动画数据
 * 来源: PNG 序列转换
 *
 * 动画信息:
 * - 帧数: {frame_count}
 * - 分辨率: {width}x{height}
 * - 帧率: {fps} FPS
 * - 总时长: {frame_count/fps:.2f} 秒
 * - 数据大小: {kb_size:.1f} KB
 * - 格式: {'RGB565' if rgb565 else 'RGB888'}
 *
 * 生成时间: 自动生成，请勿手动编辑
 */

#ifndef ANIM_{name.upper()}_H
#define ANIM_{name.upper()}_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {{
#endif

// 动画配置
#define ANIM_{name.upper()}_FRAME_COUNT     {frame_count}
#define ANIM_{name.upper()}_WIDTH           {width}
#define ANIM_{name.upper()}_HEIGHT          {height}
#define ANIM_{name.upper()}_FPS             {fps}
#define ANIM_{name.upper()}_DURATION_MS     {(frame_count * 1000 / fps)}
#define ANIM_{name.upper()}_BYTES_PER_FRAME {bytes_per_frame}
#define ANIM_{name.upper()}_TOTAL_BYTES     {total_bytes}

"""

    # 生成帧数据数组
    for frame_idx, frame in enumerate(frames_data):
        header += f"// 帧 {frame_idx}\n"
        header += f"const uint16_t anim_{name}_frame_{frame_idx}[{width * height}] = {{\n"
        header += "    "

        line_pixels = 0
        for pixel in frame:
            header += f"{pixel}, "
            line_pixels += 1
            if line_pixels >= 16:  # 每行 16 个像素
                header += "\n    "
                line_pixels = 0

        # 移除最后的逗号和空格
        header = header.rstrip(", ")
        header += "\n};\n\n"

    # 生成帧指针数组
    header += f"// 帧指针数组（便于遍历）\n"
    header += f"const uint16_t* anim_{name}_frames[{frame_count}] = {{\n"
    for frame_idx in range(frame_count):
        header += f"    anim_{name}_frame_{frame_idx},\n"
    header += "};\n\n"

    # 生成动画结构体
    header += f"// 动画元数据结构体\n"
    header += f"typedef struct {{\n"
    header += f"    const uint16_t** frames;        // 帧数据指针数组\n"
    header += f"    uint16_t frame_count;           // 总帧数\n"
    header += f"    uint16_t width;                 // 宽度\n"
    header += f"    uint16_t height;                // 高度\n"
    header += f"    uint8_t fps;                    // 帧率\n"
    header += f"    uint32_t duration_ms;           // 总时长（毫秒）\n"
    header += f"}} anim_{name}_t;\n\n"

    header += f"// 动画实例\n"
    header += f"const anim_{name}_t anim_{name} = {{\n"
    header += f"    .frames = anim_{name}_frames,\n"
    header += f"    .frame_count = {frame_count},\n"
    header += f"    .width = {width},\n"
    header += f"    .height = {height},\n"
    header += f"    .fps = {fps},\n"
    header += f"    .duration_ms = {frame_count * 1000 / fps},\n"
    header += f"}};\n\n"

    header += "#ifdef __cplusplus\n}\n"
    header += "#endif\n"
    header += "#endif // ANIM_{name.upper()}_H\n"

    return header

def main():
    parser = argparse.ArgumentParser(
        description='将 PNG 序列转换为 ESP32 动画数组',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )

    parser.add_argument('folder', help='PNG 序列文件夹路径')
    parser.add_argument('name', help='输出数组名称（如 anim_happy）')
    parser.add_argument('--fps', type=int, default=12, help='帧率（默认 12）')
    parser.add_argument('--start', type=int, default=0, help='起始帧编号（默认 0）')
    parser.add_argument('--width', type=int, default=240, help='宽度（默认 240）')
    parser.add_argument('--height', type=int, default=240, help='高度（默认 240）')
    parser.add_argument('--rgb888', action='store_true', help='使用 RGB888 格式（默认 RGB565）')
    parser.add_argument('--output', '-o', help='输出文件路径（默认：main/display/anim_<name>.h）')

    args = parser.parse_args()

    # 检查文件夹是否存在
    if not os.path.isdir(args.folder):
        print(f"错误: 文件夹 '{args.folder}' 不存在")
        sys.exit(1)

    # 处理 PNG 序列
    header_content = process_png_sequence(
        args.folder,
        args.name,
        width=args.width,
        height=args.height,
        fps=args.fps,
        start_frame=args.start,
        rgb565=not args.rgb888
    )

    if header_content is None:
        sys.exit(1)

    # 确定输出路径
    if args.output:
        output_path = args.output
    else:
        output_dir = Path(__file__).parent.parent / "main" / "display" / "animations"
        output_dir.mkdir(parents=True, exist_ok=True)
        output_path = output_dir / f"anim_{args.name}.h"

    # 写入文件
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(header_content)

    print(f"\n✅ 成功生成动画文件: {output_path}")
    print(f"\n📊 使用说明:")
    print(f"   1. 在代码中包含: #include \"anim_{args.name}.h\"")
    print(f"   2. 使用动画: const anim_{args.name}_t* anim = &anim_{args.name};")
    print(f"   3. 播放帧: anim->frames[frame_index]")

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Web面相分析服务器 - 提供Web界面的面相分析服务
"""

from flask import Flask, request, jsonify, send_file, send_from_directory, Response, stream_with_context
import base64
import sys
import os
import json
import traceback
from datetime import datetime
import uuid
import time
import re

# 添加项目路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# 设置 LLM API Key
if 'LLM_API_KEY' not in os.environ:
    os.environ['LLM_API_KEY'] = 'sk-6dda6739fdc244379ac1109fdc9734ce'

if 'LLM_BASE_URL' not in os.environ:
    os.environ['LLM_BASE_URL'] = 'https://api.deepseek.com'

if 'LLM_MODEL' not in os.environ:
    os.environ['LLM_MODEL'] = 'deepseek-chat'

# 导入面相检测模块
try:
    from vision.detector import detect_face_features
    DETECTION_AVAILABLE = True
    print("✅ 面相检测模块加载成功")
except ImportError as e:
    DETECTION_AVAILABLE = False
    print(f"❌ 面相检测模块加载失败: {e}")

app = Flask(__name__)
# ============================================================================
# Reminder TTS Integration
# ============================================================================

# Import reminder TTS routes
try:
    from reminder_tts_routes import register_reminder_routes
    REMINDER_TTS_AVAILABLE = True
    print("[OK] Reminder TTS module loaded")
except ImportError as e:
    REMINDER_TTS_AVAILABLE = False
    print(f"[ERROR] Failed to load Reminder TTS module: {e}")

# Register reminder TTS routes
if REMINDER_TTS_AVAILABLE:
    register_reminder_routes(app)
    print("[OK] Reminder TTS routes registered")
    print("   Endpoint: http://0.0.0.0:8081/api/text_to_opus")


# ============================================================================
# Device & Reminder Management System
# ============================================================================

# Register device management routes
try:
    from routes.device_routes import register_device_routes
    register_device_routes(app)
    print("[OK] Device management routes registered")
except ImportError as e:
    print(f"[ERROR] Failed to load device routes: {e}")
    import traceback
    traceback.print_exc()

# Register reminder CRUD routes
try:
    from routes.reminder_routes import register_reminder_routes
    register_reminder_routes(app)
    print("[OK] Reminder CRUD routes registered")
except ImportError as e:
    print(f"[ERROR] Failed to load reminder CRUD routes: {e}")
    import traceback
    traceback.print_exc()

# Register sync routes
try:
    from routes.sync_routes import register_sync_routes
    register_sync_routes(app)
    print("[OK] Device sync routes registered")
except ImportError as e:
    print(f"[ERROR] Failed to load sync routes: {e}")
    import traceback
    traceback.print_exc()

print("[OK] Reminder Management System loaded")
print("     Web interface: http://192.140.190.183:8081/web-reminder/")
print("     API endpoints:")
print("       - Devices:  POST/GET/PUT/DELETE /api/devices/...")
print("       - Reminders: POST/GET/PUT/DELETE /api/reminders/...")
print("       - Sync:      GET/POST /api/sync/...")


# 配置
SERVER_CONFIG = {
    'host': '0.0.0.0',
    'port': 8081,  # 使用8081端口，避免冲突
    'debug': False
}

# Web分析历史存储配置
WEB_HISTORY_CONFIG = {
    'base_dir': 'web_history',
    'photos_dir': 'photos',
    'reports_dir': 'reports',
    'metadata_dir': 'metadata',
    'index_file': 'index.json'
}


def init_history_structure():
    """初始化Web分析历史存储结构"""
    base_dir = WEB_HISTORY_CONFIG['base_dir']

    dirs = [
        base_dir,
        os.path.join(base_dir, WEB_HISTORY_CONFIG['photos_dir']),
        os.path.join(base_dir, WEB_HISTORY_CONFIG['reports_dir']),
        os.path.join(base_dir, WEB_HISTORY_CONFIG['metadata_dir'])
    ]

    for dir_path in dirs:
        os.makedirs(dir_path, exist_ok=True)

    # 初始化索引文件
    index_path = os.path.join(base_dir, WEB_HISTORY_CONFIG['index_file'])
    if not os.path.exists(index_path):
        with open(index_path, 'w', encoding='utf-8') as f:
            json.dump({"analyses": [], "total_count": 0}, f, indent=2, ensure_ascii=False)

    return base_dir


def generate_analysis_id():
    """生成唯一分析ID"""
    now = datetime.now()
    timestamp = now.strftime('%Y%m%d_%H%M%S')
    unique_id = str(uuid.uuid4())[:8]
    return f"web_{timestamp}_{unique_id}"


def save_web_analysis(analysis_id, image_data, result, gender, question):
    """保存Web分析结果"""
    base_dir = WEB_HISTORY_CONFIG['base_dir']
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    # 1. 保存照片
    photo_filename = f"{analysis_id}.jpg"
    photo_path = os.path.join(base_dir, WEB_HISTORY_CONFIG['photos_dir'], photo_filename)
    with open(photo_path, 'wb') as f:
        f.write(image_data)

    # 2. 保存报告
    report = result.get("enhanced_report", result.get("report", ""))
    report_filename = f"{analysis_id}_report.txt"
    report_path = os.path.join(base_dir, WEB_HISTORY_CONFIG['reports_dir'], report_filename)
    with open(report_path, 'w', encoding='utf-8') as f:
        f.write(report)

    # 3. 保存元数据
    detection = result.get("detection", {})
    features = detection.get("features", {})
    measurements = detection.get("measurements", {})

    metadata = {
        "analysis_id": analysis_id,
        "timestamp": timestamp,
        "source": "web",
        "gender": gender,
        "question": question,
        "photo_path": os.path.join(WEB_HISTORY_CONFIG['photos_dir'], photo_filename),
        "report_path": os.path.join(WEB_HISTORY_CONFIG['reports_dir'], report_filename),
        "features": features,
        "measurements": measurements,
        "report_length": len(report),
        "photo_size": len(image_data)
    }

    metadata_filename = f"{analysis_id}_metadata.json"
    metadata_path = os.path.join(base_dir, WEB_HISTORY_CONFIG['metadata_dir'], metadata_filename)
    with open(metadata_path, 'w', encoding='utf-8') as f:
        json.dump(metadata, f, indent=2, ensure_ascii=False)

    # 4. 更新索引
    update_index(metadata)

    return {
        "photo_path": photo_path,
        "report_path": report_path,
        "metadata_path": metadata_path
    }


def update_index(metadata):
    """更新全局索引"""
    index_path = os.path.join(WEB_HISTORY_CONFIG['base_dir'], WEB_HISTORY_CONFIG['index_file'])

    try:
        with open(index_path, 'r', encoding='utf-8') as f:
            index = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        index = {"analyses": [], "total_count": 0}

    index["analyses"].append({
        "analysis_id": metadata["analysis_id"],
        "timestamp": metadata["timestamp"],
        "source": metadata["source"],
        "gender": metadata["gender"],
        "photo_path": metadata["photo_path"],
        "report_path": metadata["report_path"],
        "features": metadata["features"]
    })
    index["total_count"] = len(index["analyses"])

    with open(index_path, 'w', encoding='utf-8') as f:
        json.dump(index, f, indent=2, ensure_ascii=False)


@app.route('/')
def index():
    """主页 - 上传界面"""
    return send_file('web_face_analysis.html')


@app.route('/api/analyze', methods=['POST'])
def analyze():
    """
    Web面相分析API
    接收照片上传，使用LLM生成详细报告
    """
    try:
        # 检查文件
        if 'photo' not in request.files:
            return jsonify({
                "success": False,
                "error": "未找到照片文件"
            }), 400

        photo_file = request.files['photo']
        image_data = photo_file.read()

        if len(image_data) == 0:
            return jsonify({
                "success": False,
                "error": "照片文件为空"
            }), 400

        # 获取参数
        gender = request.form.get('gender', 'male')
        question = request.form.get('question', '请分析此人的面相，包括性格特点和近期运势')

        print(f"\n{'='*60}")
        print(f"收到Web分析请求")
        print(f"性别: {gender}")
        print(f"问题: {question}")
        print(f"照片大小: {len(image_data)} 字节")
        print(f"{'='*60}\n")

        # 转换为Base64
        photo_base64 = base64.b64encode(image_data).decode('utf-8')

        # 调用面相分析（使用LLM）
        if not DETECTION_AVAILABLE:
            return jsonify({
                "success": False,
                "error": "面相检测模块未可用"
            }), 500

        # 开始性能计时
        timing_start = time.time()
        print("开始面相分析（使用LLM）...", flush=True)

        result = detect_face_features(
            photo_base64,
            gender=gender,
            use_llm=True  # Web端使用LLM生成详细报告
        )

        # 记录分析时间
        timing_analysis = time.time()
        analysis_duration = timing_analysis - timing_start
        print(f"⏱️ 面相分析耗时: {analysis_duration:.2f} 秒")

        if not result.get("success"):
            error_msg = result.get("error", "分析失败")
            print(f"❌ 分析失败: {error_msg}")
            return jsonify({
                "success": False,
                "error": error_msg
            }), 500

        # 获取报告（保留原始markdown格式，前端会处理）
        report = result.get("enhanced_report", result.get("report", ""))

        # 保存分析历史
        try:
            analysis_id = generate_analysis_id()
            saved_paths = save_web_analysis(analysis_id, image_data, result, gender, question)
            print(f"✅ 分析历史已保存 (ID: {analysis_id})")
        except Exception as e:
            print(f"⚠️  保存分析历史失败: {e}")
            traceback.print_exc()

        # 记录总时间
        timing_total = time.time() - timing_start
        print(f"✅ 分析完成")
        print(f"报告长度: {len(report)} 字符")
        print(f"总耗时: {timing_total:.2f} 秒 (面相分析: {analysis_duration:.2f}s)")

        return jsonify({
            "success": True,
            "report": report,
            "analysis_id": analysis_id
        })

    except Exception as e:
        print(f"❌ 处理请求时发生错误: {str(e)}")
        traceback.print_exc()
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


@app.route('/api/analyze-stream', methods=['POST'])
def analyze_stream():
    """
    流式面相分析API - 使用Server-Sent Events (SSE)
    实时返回分析进度和生成的报告内容（真正的流式LLM）
    """
    try:
        # 检查文件
        if 'photo' not in request.files:
            return jsonify({
                "success": False,
                "error": "未找到照片文件"
            }), 400

        photo_file = request.files['photo']
        image_data = photo_file.read()

        if len(image_data) == 0:
            return jsonify({
                "success": False,
                "error": "照片文件为空"
            }), 400

        # 获取参数
        gender = request.form.get('gender', 'male')
        question = request.form.get('question', '请分析此人的面相，包括性格特点和近期运势')

        print(f"\n{'='*60}")
        print(f"收到Web流式分析请求")
        print(f"性别: {gender}")
        print(f"问题: {question}")
        print(f"照片大小: {len(image_data)} 字节")
        print(f"{'='*60}\n")

        # 转换为Base64
        photo_base64 = base64.b64encode(image_data).decode('utf-8')

        if not DETECTION_AVAILABLE:
            return jsonify({
                "success": False,
                "error": "面相检测模块未可用"
            }), 500

        def generate():
            """SSE流式生成器"""
            try:
                # 发送开始消息
                yield f"data: {json.dumps({'type': 'start', 'message': '开始面相分析...'}, ensure_ascii=False)}\n\n"

                # 步骤1: 人脸检测
                yield f"data: {json.dumps({'type': 'progress', 'message': '正在进行人脸特征检测...', 'step': 1}, ensure_ascii=False)}\n\n"

                timing_detect = time.time()

                # 1. 先进行人脸检测（不使用LLM，不生成报告）
                result = detect_face_features(
                    photo_base64,
                    gender=gender,
                    use_enhanced_report=False,  # 不生成报告，只做检测
                    use_llm=False  # 不使用LLM
                )

                timing_detect_done = time.time()
                detect_duration = timing_detect_done - timing_detect
                yield f"data: {json.dumps({'type': 'progress', 'message': f'人脸检测完成，正在生成详细报告...', 'step': 2, 'timing': f'{detect_duration:.2f}s'}, ensure_ascii=False)}\n\n"
                print(f"⏱️ 人脸检测耗时: {detect_duration:.2f} 秒")

                if not result.get("success"):
                    error_msg = result.get("error", "分析失败")
                    yield f"data: {json.dumps({'type': 'error', 'error': error_msg}, ensure_ascii=False)}\n\n"
                    return

                # 2. 使用LLM流式生成报告
                yield f"data: {json.dumps({'type': 'progress', 'message': f'AI正在生成报告，请稍候...', 'step': 3}, ensure_ascii=False)}\n\n"

                timing_llm = time.time()

                try:
                    from vision.llm_client import get_llm_client
                    from vision.report_generator import FaceReportGenerator

                    llm_client = get_llm_client()

                    if llm_client and llm_client.is_available():
                        # 检查result的类型和结构
                        print(f"[DEBUG] result type: {type(result)}")
                        print(f"[DEBUG] result keys: {result.keys() if isinstance(result, dict) else 'N/A'}")

                        detection_data = result.get("detection", {})
                        match_data = result.get("match", {})

                        print(f"[DEBUG] detection_data type: {type(detection_data)}")
                        print(f"[DEBUG] match_data type: {type(match_data)}")

                        # 类型检查
                        if not isinstance(detection_data, dict):
                            yield f"data: {json.dumps({'type': 'error', 'error': f'detection_data类型错误: {type(detection_data)}, 期望dict'}, ensure_ascii=False)}\n\n"
                            return

                        if not isinstance(match_data, dict):
                            yield f"data: {json.dumps({'type': 'error', 'error': f'match_data类型错误: {type(match_data)}, 期望dict'}, ensure_ascii=False)}\n\n"
                            return

                        # 判断是否是儿童
                        measurements = detection_data.get("measurements", {})
                        forehead_ratio = measurements.get("forehead", {}).get("ratio", 0.3)
                        face_ratio = measurements.get("face", {}).get("shape_ratio", 1.3)
                        is_child = (forehead_ratio >= 0.32 and face_ratio < 1.35) or forehead_ratio >= 0.35

                        # 使用LLM流式生成
                        report_parts = []

                        # 先发送报告标题
                        import datetime
                        lines = []
                        if is_child:
                            lines.append("# 🌟 儿童面相分析报告")
                        else:
                            lines.append("# 🌟 面相分析报告")
                        lines.append("")
                        lines.append(f"> 分析时间: {datetime.datetime.now().strftime('%Y年%m月%d日 %H:%M')}")
                        lines.append("")
                        lines.append("---")
                        lines.append("")
                        lines_text = '\n'.join(lines)
                        yield f"data: {json.dumps({'type': 'content', 'content': lines_text, 'done': False}, ensure_ascii=False)}\n\n"

                        # 实时流式生成报告内容
                        for chunk in llm_client.generate_face_report_streaming(
                            detection_data,
                            match_data,
                            gender,
                            is_child
                        ):
                            # 立即发送每个chunk
                            yield f"data: {json.dumps({'type': 'content', 'content': chunk, 'done': False}, ensure_ascii=False)}\n\n"
                            report_parts.append(chunk)

                    else:
                        # LLM不可用，使用模板
                        yield f"data: {json.dumps({'type': 'error', 'error': 'LLM服务不可用'}, ensure_ascii=False)}\n\n"
                        return

                except Exception as llm_error:
                    print(f"❌ LLM生成出错: {llm_error}")
                    traceback.print_exc()
                    yield f"data: {json.dumps({'type': 'error', 'error': f'LLM生成失败: {str(llm_error)}'}, ensure_ascii=False)}\n\n"
                    return

                timing_llm_done = time.time()
                llm_duration = timing_llm_done - timing_llm

                # 3. 组装完整结果
                full_report = '\n'.join(lines) + ''.join(report_parts)

                # 4. 保存分析历史
                total_time = time.time() - timing_detect
                analysis_id = generate_analysis_id()

                # 构建完整的结果对象（用于保存）
                result_with_report = result.copy()
                result_with_report["enhanced_report"] = full_report

                try:
                    save_web_analysis(analysis_id, image_data, result_with_report, gender, question)
                    print(f"✅ 分析历史已保存 (ID: {analysis_id})")
                except Exception as e:
                    print(f"⚠️  保存分析历史失败: {e}")

                # 发送完成消息
                yield f"data: {json.dumps({'type': 'complete', 'analysis_id': analysis_id, 'timing': f'{total_time:.2f}s', 'llm_timing': f'{llm_duration:.2f}s', 'done': True}, ensure_ascii=False)}\n\n"

                print(f"✅ 流式分析完成")
                print(f"  人脸检测耗时: {timing_detect_done - timing_detect:.2f}s")
                print(f"  LLM生成耗时: {llm_duration:.2f}s")
                print(f"  总耗时: {total_time:.2f}s")

            except Exception as e:
                print(f"❌ 流式分析错误: {str(e)}")
                traceback.print_exc()
                yield f"data: {json.dumps({'type': 'error', 'error': str(e)}, ensure_ascii=False)}\n\n"

        return Response(
            stream_with_context(generate()),
            mimetype='text/event-stream',
            headers={
                'Cache-Control': 'no-cache',
                'X-Accel-Buffering': 'no'
            }
        )

    except Exception as e:
        print(f"❌ 处理流式请求时发生错误: {str(e)}")
        traceback.print_exc()
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


@app.route('/api/history', methods=['GET'])
def get_history():
    """获取分析历史列表"""
    try:
        index_path = os.path.join(WEB_HISTORY_CONFIG['base_dir'], WEB_HISTORY_CONFIG['index_file'])

        with open(index_path, 'r', encoding='utf-8') as f:
            index = json.load(f)

        return jsonify({
            "success": True,
            "analyses": index["analyses"],
            "total_count": index["total_count"]
        })
    except Exception as e:
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


@app.route('/api/report/<analysis_id>', methods=['GET'])
def get_report(analysis_id):
    """获取指定分析的报告"""
    try:
        base_dir = WEB_HISTORY_CONFIG['base_dir']
        report_path = os.path.join(base_dir, WEB_HISTORY_CONFIG['reports_dir'], f"{analysis_id}_report.txt")

        if not os.path.exists(report_path):
            return jsonify({
                "success": False,
                "error": "报告不存在"
            }), 404

        with open(report_path, 'r', encoding='utf-8') as f:
            report = f.read()

        return jsonify({
            "success": True,
            "report": report
        })
    except Exception as e:
        return jsonify({
            "success": False,
            "error": str(e)
        }), 500


@app.route('/health', methods=['GET'])
def health_check():
    """健康检查"""
    return jsonify({
        "status": "ok",
        "service": "web-face-analysis-server",
        "version": "1.0",
        "detection_available": DETECTION_AVAILABLE
    })


@app.route('/vision/explain', methods=['POST'])
def vision_explain():
    """
    ESP32设备面相分析接口
    兼容 xiaozhi.me 的 /vision/explain 接口
    """
    try:
        # 获取请求数据
        data = request.get_json(force=True, silent=True) or {}

        # 获取图片和问题
        image_data = data.get('image')
        question = data.get('question', '请分析这个人的面相')

        if not image_data:
            return jsonify({
                'success': False,
                'error': 'Missing image parameter'
            }), 400

        print(f"\n{'='*60}")
        print(f"ESP32面相分析请求")
        print(f"问题: {question}")
        print(f"{'='*60}\n")

        # 如果是base64编码的图片
        if isinstance(image_data, str) and image_data.startswith('data:image'):
            if ',' in image_data:
                image_data = image_data.split(',', 1)[1]

        # 解码图片
        import base64
        image_bytes = base64.b64decode(image_data)

        # 检测面相特征
        if DETECTION_AVAILABLE:
            from vision.detector import detect_face_features
            detection_result = detect_face_features(image_bytes)
        else:
            return jsonify({
                'success': False,
                'error': '面相检测模块未加载'
            }), 500

        if not detection_result:
            return jsonify({
                'success': False,
                'error': '面相检测失败'
            }), 500

        # 获取性别和是否为儿童
        gender = detection_result.get('gender', 'unknown')
        is_child = detection_result.get('is_child', False)

        # 生成报告
        report_parts = []

        # 使用LLM生成报告
        try:
            from vision.llm_client import get_llm_client
            llm_client = get_llm_client()

            if llm_client and llm_client.is_available():
                print("✅ 使用LLM生成报告")

                # 构建prompt
                prompt = f"根据以下面相检测数据生成报告：\n{str(detection_result)}\n用户问题: {question}"
                system_prompt = "你是一位专业的面相分析师，擅长从五官特征分析一个人的性格特点和运势走向。"

                # 生成报告
                llm_report = llm_client.generate(
                    prompt=prompt,
                    system_prompt=system_prompt,
                    temperature=0.7,
                    max_tokens=3000
                )

                if llm_report:
                    report_parts.append(llm_report)
                else:
                    # LLM失败，使用模板
                    print("⚠️ LLM生成失败，使用模板")
                    report_parts.append(f"性别: {gender}\n{str(detection_result)}")
            else:
                print("⚠️ LLM不可用，使用基础模板")
                report_parts.append(f"性别: {gender}\n{str(detection_result)}")

        except Exception as llm_error:
            print(f"⚠️ LLM生成出错: {llm_error}")
            report_parts.append(f"性别: {gender}\n{str(detection_result)}")

        # 组合报告
        full_report = '\n'.join(report_parts)

        print(f"✅ 面相分析完成")

        # 返回结果（兼容xiaozhi.me格式）
        return jsonify({
            'success': True,
            'report': full_report,
            'analysis': detection_result,
            'gender': gender,
            'is_child': is_child
        })

    except Exception as e:
        print(f"❌ Vision explain失败: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


# ============================================================================
# Web Reminder Interface - 日程管理Web界面
# ============================================================================

@app.route('/web-reminder')
@app.route('/web-reminder/')
def web_reminder_index():
    """日程管理Web界面"""
    try:
        return send_from_directory('static/web-reminder', 'index.html')
    except FileNotFoundError:
        return "Web Reminder interface not found", 404


@app.route('/web-reminder/<path:filename>')
def web_reminder_static(filename):
    """日程管理静态文件"""
    try:
        return send_from_directory('static/web-reminder', filename)
    except FileNotFoundError:
        return f"File not found: {filename}", 404


def main():
    # 启动时检查数据库配置
    try:
        from startup_db_check import check_database_config
        success, errors, warnings = check_database_config()
        if not success:
            print("[ERROR] 数据库配置检查失败，请修复后再启动！")
            sys.exit(1)
    except Exception as e:
        print(f"[WARNING] 数据库检查失败: {e}")
    
    """启动Web服务器"""
    # 初始化存储结构
    base_dir = init_history_structure()

    print("=" * 60)
    print("Web面相分析服务器")
    print("=" * 60)
    print(f"监听地址: {SERVER_CONFIG['host']}:{SERVER_CONFIG['port']}")
    print(f"Web界面: http://localhost:{SERVER_CONFIG['port']}")
    print(f"健康检查: http://localhost:{SERVER_CONFIG['port']}/health")
    print()
    print("分析历史存储:")
    print(f"  基础目录: {base_dir}/")
    print(f"  - photos/     用户上传的照片")
    print(f"  - reports/    LLM生成的详细报告")
    print(f"  - metadata/   分析元数据")
    print(f"  - index.json  全局索引")
    print()
    print("等待访问...")
    print("=" * 60)
    print()

    # ============================================================================
    # Chat Messages & Daily Reports System
    # ============================================================================

    # 注册对话记录路由
    try:
        from routes.chat_routes import register_chat_routes
        register_chat_routes(app)
        print("[OK] Chat messages routes registered")
    except ImportError as e:
        print(f"[ERROR] Failed to load chat routes: {e}")
        import traceback
        traceback.print_exc()

    # 注册报告路由
    try:
        from routes.report_routes import register_report_routes
        register_report_routes(app)
        print("[OK] Report routes registered")
    except ImportError as e:
        print(f"[ERROR] Failed to load report routes: {e}")
        import traceback
        traceback.print_exc()

    # 启动报告定时调度器
    try:
        from utils.report_scheduler import get_report_scheduler
        scheduler = get_report_scheduler()
        scheduler.start()
        print("[OK] Daily report scheduler started (23:00 daily)")
    except Exception as e:
        print(f"[ERROR] Failed to start report scheduler: {e}")
        import traceback
        traceback.print_exc()

    # 启动Flask服务器
    app.run(
        host=SERVER_CONFIG['host'],
        port=SERVER_CONFIG['port'],
        debug=SERVER_CONFIG['debug']
    )


if __name__ == "__main__":
    main()

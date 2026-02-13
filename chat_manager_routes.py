"""
Chat Manager Routes - 对话管理和报告查看API（修复版）
"""
from flask import Blueprint, request, jsonify, Response, send_file
from services.chat_service import ChatService
from services.report_service import ReportService
from datetime import datetime, timedelta
import json

chat_manager_bp = Blueprint('chat_manager', __name__)
chat_service = ChatService()
report_service = ReportService()


@chat_manager_bp.route('/api/devices', methods=['GET'])
def get_devices():
    """
    获取设备列表（包含提醒数量）

    Response:
        {
            "success": true,
            "devices": [
                {
                    "id": 1,
                    "mac_address": "aa:bb:cc:dd:ee:ff",
                    "device_name": "我的设备",
                    "is_online": true,
                    "reminder_count": 5
                }
            ]
        }
    """
    try:
        from database.connection import get_db

        db = get_db()
        rows = db.execute(
            """SELECT id, mac_address, device_name, is_online, last_online_at
               FROM devices
               ORDER BY last_online_at DESC""",
            fetch_all=True
        )

        devices = []
        for row in rows:
            device_id = row[0]

            # 查询该设备的活跃提醒数量
            reminder_count_result = db.execute(
                "SELECT COUNT(*) FROM reminders WHERE device_id = ? AND status = 'active'",
                (device_id,),
                fetch_one=True
            )
            reminder_count = reminder_count_result[0] if reminder_count_result else 0

            devices.append({
                'id': device_id,
                'mac_address': row[1],
                'device_name': row[2] or f"设备{device_id}",
                'is_online': bool(row[3]),  # 添加在线状态
                'last_online_at': row[4],
                'reminder_count': reminder_count  # 添加提醒数量
            })

        return jsonify({
            'success': True,
            'devices': devices
        })

    except Exception as e:
        print(f"[ChatManager] Error getting devices: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500


@chat_manager_bp.route('/api/chats/<device_mac>', methods=['GET'])
def get_chats(device_mac: str):
    """
    获取指定设备的对话记录

    Query Parameters:
        date: 日期筛选 (YYYY-MM-DD)
        limit: 返回数量限制 (默认100)
        offset: 偏移量 (默认0)
    """
    try:
        date = request.args.get('date')
        limit = int(request.args.get('limit', 100))
        offset = int(request.args.get('offset', 0))

        # 如果指定了日期，获取该日期的对话
        if date:
            chats = chat_service.get_daily_chats(device_mac, date)
        else:
            # 获取最近的对话
            chats = chat_service.get_recent_chats(device_mac, limit, offset)

        return jsonify({
            'success': True,
            'device_mac': device_mac,
            'date': date,
            'count': len(chats),
            'chats': chats
        })

    except Exception as e:
        print(f"[ChatManager] Error getting chats: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500


@chat_manager_bp.route('/api/chats/stats', methods=['GET'])
def get_chat_stats():
    """
    获取对话统计信息

    Query Parameters:
        date: 日期 (YYYY-MM-DD)
        device_mac: 设备MAC地址
    """
    try:
        from database.connection import get_db

        date = request.args.get('date')
        device_mac = request.args.get('device_mac')

        db = get_db()

        # 总对话数
        total_result = db.execute(
            "SELECT COUNT(*) FROM chat_messages",
            fetch_one=True
        )
        total = total_result[0] if total_result else 0

        # 今日对话数
        if date:
            start_dt = datetime.strptime(date, "%Y-%m-%d")
            end_dt = start_dt + timedelta(days=1)
            start_ts = int(start_dt.timestamp() * 1000)
            end_ts = int(end_dt.timestamp() * 1000)

            today_result = db.execute(
                """SELECT COUNT(*) FROM chat_messages
                   WHERE server_timestamp >= ? AND server_timestamp < ?""",
                (start_ts, end_ts),
                fetch_one=True
            )
            today = today_result[0] if today_result else 0
        else:
            today = 0

        # 设备数量
        devices_result = db.execute(
            "SELECT COUNT(*) FROM devices",
            fetch_one=True
        )
        devices = devices_result[0] if devices_result else 0

        return jsonify({
            'success': True,
            'total': total,
            'today': today,
            'devices': devices
        })

    except Exception as e:
        print(f"[ChatManager] Error getting stats: {e}")
        return jsonify({'success': False, 'error': str(e)}), 500


@chat_manager_bp.route('/api/reports/list', methods=['GET'])
def list_reports():
    """
    获取报告列表

    Query Parameters:
        device_mac: 设备MAC地址
        limit: 返回数量限制
    """
    try:
        from database.connection import get_db

        device_mac = request.args.get('device_mac')
        limit = int(request.args.get('limit', 30))

        db = get_db()

        if device_mac:
            # 获取设备ID
            device = db.execute(
                "SELECT id FROM devices WHERE mac_address = ?",
                (device_mac,),
                fetch_one=True
            )

            if not device:
                return jsonify({'success': False, 'error': 'Device not found'}), 404

            device_id = device[0]

            # 获取该设备的报告列表
            rows = db.execute(
                """SELECT id, device_id, report_date, chat_count,
                          generation_status, created_at
                   FROM ai_reports
                   WHERE device_id = ?
                   ORDER BY report_date DESC
                   LIMIT ?""",
                (device_id, limit),
                fetch_all=True
            )
        else:
            # 获取所有报告
            rows = db.execute(
                """SELECT id, device_id, report_date, chat_count,
                          generation_status, created_at
                   FROM ai_reports
                   ORDER BY report_date DESC
                   LIMIT ?""",
                (limit,),
                fetch_all=True
            )

        reports = []
        for row in rows:
            reports.append({
                'id': row[0],
                'device_id': row[1],
                'report_date': row[2],
                'chat_count': row[3],
                'generation_status': row[4],
                'created_at': row[5]
            })

        return jsonify({
            'success': True,
            'count': len(reports),
            'reports': reports
        })

    except Exception as e:
        print(f"[ChatManager] Error listing reports: {e}")
        return jsonify({'success': False, 'error': str(e)}), 500


@chat_manager_bp.route('/api/manager', methods=['GET'])
def get_manager_page():
    """返回对话管理页面"""
    return send_file('static/chat-manager.html')


def register_chat_manager_routes(app):
    """注册对话管理路由"""
    app.register_blueprint(chat_manager_bp)
    print("[OK] Chat Manager routes registered:")
    print("     - GET  /api/devices                        # 获取设备列表 ✨")
    print("     - GET  /api/chats/<device_mac>              # 获取对话记录")
    print("     - GET  /api/chats/stats                     # 获取统计信息")
    print("     - GET  /api/reports/list                    # 获取报告列表")
    print("     - GET  /api/manager                         # 管理页面")

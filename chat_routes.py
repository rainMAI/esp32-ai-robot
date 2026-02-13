"""
Chat Routes - 对话记录API路由
"""
from flask import Blueprint, request, jsonify, Response
from services.chat_service import ChatService

chat_bp = Blueprint('chat', __name__)
chat_service = ChatService()


def validate_mac_address(mac: str) -> bool:
    """验证MAC地址格式"""
    import re
    patterns = [
        r'^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$',
        r'^([0-9A-Fa-f]{4}\.){2}([0-9A-Fa-f]{4})$'
    ]
    return any(re.match(pattern, mac) for pattern in patterns)


@chat_bp.route('/api/chats/batch', methods=['POST'])
def batch_create_chats():
    """
    批量上传对话记录

    Headers:
        Device-Id: MAC地址 (aa:bb:cc:dd:ee:ff)

    Request Body:
        {
            "messages": [
                {"user_text": "今天天气怎么样", "ai_text": "今天晴天，气温25度"}
            ]
        }

    Response:
        {"success": true, "created_count": 5, "device_id": 1}
    """
    try:
        # 获取设备ID
        mac_address = request.headers.get('Device-Id')
        if not mac_address:
            return jsonify({'success': False, 'error': 'Missing Device-Id header'}), 400

        if not validate_mac_address(mac_address):
            return jsonify({'success': False, 'error': 'Invalid MAC address format'}), 400

        # 获取请求数据
        data = request.get_json()
        if not data or 'messages' not in data:
            return jsonify({'success': False, 'error': 'Missing messages field'}), 400

        messages = data['messages']
        if not isinstance(messages, list):
            return jsonify({'success': False, 'error': 'Messages must be an array'}), 400

        # 批量创建
        result = chat_service.batch_create(mac_address, messages)

        if result['success']:
            return jsonify(result), 201
        else:
            return jsonify(result), 404 if 'Device not found' in result.get('error', '') else 500

    except Exception as e:
        print(f"[ChatRoutes] Error in batch_create_chats: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500


@chat_bp.route('/api/chats/<device_mac>/<date>', methods=['GET'])
def get_chats_by_date(device_mac: str, date: str):
    """
    查询指定日期的对话记录

    Response:
        {
            "device_mac": "aa:bb:cc:dd:ee:ff",
            "date": "2026-01-17",
            "chats": [...]
        }
    """
    try:
        chats = chat_service.get_daily_chats(device_mac, date)
        return jsonify({
            'device_mac': device_mac,
            'date': date,
            'chats': chats,
            'count': len(chats)
        }), 200

    except Exception as e:
        print(f"[ChatRoutes] Error in get_chats_by_date: {e}")
        return jsonify({'success': False, 'error': str(e)}), 500


def register_chat_routes(app):
    """注册对话记录路由"""
    app.register_blueprint(chat_bp)
    print("[OK] Chat routes registered:")
    print("     - POST /api/chats/batch")
    print("     - GET  /api/chats/<device_mac>/<date>")

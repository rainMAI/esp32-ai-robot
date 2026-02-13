"""
User Device Routes - 用户设备管理API（完整版）
- 设备排他性绑定
- 设备解绑功能
- 设备列表查询
"""
from flask import Blueprint, request, jsonify
from functools import wraps
import re

user_device_bp = Blueprint('user_device', __name__)


def require_auth(f):
    """认证装饰器"""
    @wraps(f)
    def decorated_function(*args, **kwargs):
        from services.auth_service import AuthService
        token = request.headers.get('Authorization', '').replace('Bearer ', '')

        if not token:
            return jsonify({'success': False, 'error': 'Missing token'}), 401

        auth_service = AuthService()
        result = auth_service.verify_token(token)

        if not result['success']:
            return jsonify({'success': False, 'error': result.get('error', 'Invalid token')}), 401

        request.user = result['user']
        return f(*args, **kwargs)

    return decorated_function


@user_device_bp.route('/api/user/devices/add', methods=['POST'])
@require_auth
def add_user_device():
    """
    为当前用户添加设备（带排他性检查）

    Request Body:
        {
            "device_name": "string",
            "mac_address": "string"
        }

    Response:
        {"success": bool, "error": str, "device": dict}
    """
    try:
        from database.connection import get_db

        data = request.get_json()
        if not data:
            return jsonify({'success': False, 'error': 'Missing request body'}), 400

        device_name = data.get('device_name', '').strip()
        mac_address = data.get('mac_address', '').strip()

        if not device_name or not mac_address:
            return jsonify({'success': False, 'error': '设备名称和MAC地址不能为空'}), 400

        # 验证MAC地址格式
        mac_pattern = r'^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$'
        if not re.match(mac_pattern, mac_address):
            return jsonify({'success': False, 'error': 'MAC地址格式不正确，应为: AA:BB:CC:DD:EE:FF'}), 400

        # 标准化MAC地址
        mac_clean = re.sub(r'[:.-]', '', mac_address)
        mac_normalized = ':'.join([mac_clean[i:i+2] for i in range(0, 12, 2)]).lower()

        user_id = request.user['id']
        db = get_db()

        # 检查MAC地址是否已存在
        existing_mac = db.execute(
            "SELECT id, device_name, owner_id FROM devices WHERE mac_address = ?",
            (mac_normalized,),
            fetch_one=True
        )

        if existing_mac:
            # MAC地址已存在，返回提示信息
            device_id, db_device_name, owner_id = existing_mac
            return jsonify({
                'success': True,
                'message': f'该MAC地址已存在（设备名称：{db_device_name}）',
                'device': {
                    'id': device_id,
                    'mac_address': mac_normalized,
                    'device_name': db_device_name,
                    'is_owned': owner_id == user_id
                }
            })

        # 检查设备名称是否已存在
        existing_name = db.execute(
            "SELECT id, mac_address, owner_id FROM devices WHERE device_name = ?",
            (device_name,),
            fetch_one=True
        )

        if existing_name:
            # 设备名称已存在，返回提示信息
            device_id, existing_mac, owner_id = existing_name
            return jsonify({
                'success': True,
                'message': f'该设备名称已存在（MAC地址：{existing_mac}）',
                'device': {
                    'id': device_id,
                    'mac_address': existing_mac,
                    'device_name': device_name,
                    'is_owned': owner_id == user_id
                }
            })

        # 设备不存在，创建新设备并绑定
        device_id = db.execute(
            """INSERT INTO devices (mac_address, device_name, owner_id, created_at, updated_at)
               VALUES (?, ?, ?, ?, ?)""",
            (mac_normalized, device_name, user_id,
             __import__('datetime').datetime.now().isoformat(),
             __import__('datetime').datetime.now().isoformat())
        )

        # 添加到user_devices（使用INSERT OR IGNORE避免重复和外键错误）
        db.execute(
            """INSERT OR IGNORE INTO user_devices (user_id, device_id, device_name, created_at)
               VALUES (?, ?, ?, ?)""",
            (user_id, device_id, device_name,
             __import__('datetime').datetime.now().isoformat())
        )

        return jsonify({
            'success': True,
            'message': '设备添加成功',
            'device': {
                'id': device_id,
                'mac_address': mac_normalized,
                'device_name': device_name,
                'is_owned': True
            }
        })

    except Exception as e:
        print(f"[UserDeviceRoutes] Error in add_user_device: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500


@user_device_bp.route('/api/user/devices/unbind', methods=['POST'])
@require_auth
def unbind_device():
    """
    删除设备（从数据库中完全删除）

    Request Body:
        {
            "device_id": int
        }

    Response:
        {"success": bool, "error": str}
    """
    try:
        from database.connection import get_db

        data = request.get_json()
        if not data:
            return jsonify({'success': False, 'error': 'Missing request body'}), 400

        device_id = data.get('device_id')

        if not device_id:
            return jsonify({'success': False, 'error': '设备ID不能为空'}), 400

        user_id = request.user['id']
        db = get_db()

        # 检查设备是否属于当前用户
        device = db.execute(
            "SELECT id, owner_id, device_name FROM devices WHERE id = ?",
            (device_id,),
            fetch_one=True
        )

        if not device:
            return jsonify({'success': False, 'error': '设备不存在'}), 404

        _, device_owner_id, device_name = device

        if device_owner_id != user_id:
            return jsonify({'success': False, 'error': '该设备不属于您，无权删除'}), 403

        # 删除user_devices关联
        db.execute(
            "DELETE FROM user_devices WHERE device_id = ?",
            (device_id,)
        )

        # 删除设备相关的提醒
        db.execute(
            "DELETE FROM reminders WHERE device_id = ?",
            (device_id,)
        )

        # 删除设备记录
        db.execute(
            "DELETE FROM devices WHERE id = ?",
            (device_id,)
        )

        return jsonify({'success': True, 'message': f'设备 "{device_name}" 已删除'})

    except Exception as e:
        print(f"[UserDeviceRoutes] Error in unbind_device: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500


@user_device_bp.route('/api/user/devices', methods=['GET'])
@require_auth
def get_user_devices():
    """
    获取当前用户的设备列表（修复版：返回所有可用设备）

    包括：
    1. 用户拥有的设备（owner_id = user_id）
    2. 无主的设备（owner_id IS NULL）

    Response:
        {
            "success": bool,
            "devices": [
                {
                    "id": int,
                    "mac_address": "string",
                    "device_name": "string",
                    "is_primary": bool,
                    "is_owned": bool
                }
            ]
        }
    """
    try:
        from database.connection import get_db

        user_id = request.user['id']
        db = get_db()

        # 只查询用户拥有的设备
        rows = db.execute(
            """SELECT d.id, d.mac_address, d.device_name,
                      d.owner_id,
                      1 as is_owned,
                      COALESCE(ud.is_primary, 0) as is_primary
               FROM devices d
               LEFT JOIN user_devices ud ON d.id = ud.device_id AND ud.user_id = ?
               WHERE d.owner_id = ?
               ORDER BY d.id""",
            (user_id, user_id),
            fetch_all=True
        )

        devices = []
        for row in rows:
            device_id, mac_address, device_name, owner_id, is_owned, is_primary = row
            devices.append({
                'id': device_id,
                'mac_address': mac_address,
                'device_name': device_name,
                'is_primary': bool(is_primary),
                'is_owned': bool(is_owned)
            })

        print(f"[UserDeviceRoutes] Found {len(devices)} devices for user {user_id}")

        return jsonify({
            'success': True,
            'devices': devices
        })

    except Exception as e:
        print(f"[UserDeviceRoutes] Error in get_user_devices: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500


@user_device_bp.route('/api/user/devices/set-primary', methods=['POST'])
@require_auth
def set_primary_device():
    """
    设置主设备

    Request Body:
        {
            "device_id": int
        }

    Response:
        {"success": bool, "error": str}
    """
    try:
        from database.connection import get_db

        data = request.get_json()
        if not data:
            return jsonify({'success': False, 'error': 'Missing request body'}), 400

        device_id = data.get('device_id')

        if not device_id:
            return jsonify({'success': False, 'error': '设备ID不能为空'}), 400

        user_id = request.user['id']
        db = get_db()

        # 验证设备属于当前用户
        device = db.execute(
            "SELECT id FROM user_devices WHERE user_id = ? AND device_id = ?",
            (user_id, device_id),
            fetch_one=True
        )

        if not device:
            return jsonify({'success': False, 'error': '设备不存在或不属于您'}), 404

        # 取消该用户的所有主设备标记
        db.execute(
            "UPDATE user_devices SET is_primary = 0 WHERE user_id = ?",
            (user_id,)
        )

        # 设置新的主设备
        db.execute(
            "UPDATE user_devices SET is_primary = 1 WHERE user_id = ? AND device_id = ?",
            (user_id, device_id)
        )

        return jsonify({'success': True, 'message': '主设备设置成功'})

    except Exception as e:
        print(f"[UserDeviceRoutes] Error in set_primary_device: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({'success': False, 'error': str(e)}), 500


def register_user_device_routes(app):
    """注册用户设备路由"""
    app.register_blueprint(user_device_bp)
    print("[OK] User Device routes registered:")
    print("     - POST /api/user/devices/add")
    print("     - POST /api/user/devices/unbind")
    print("     - GET  /api/user/devices")
    print("     - POST /api/user/devices/set-primary")

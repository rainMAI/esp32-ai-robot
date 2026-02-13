"""
Reminder CRUD API Routes
"""
from flask import Blueprint, request, jsonify
from datetime import datetime
import json
import time
import re

from database.connection import get_db

reminder_bp = Blueprint('reminders', __name__, url_prefix='/api/reminders')


def normalize_mac_address(mac_address: str) -> str:
    """Normalize MAC address to lowercase without separators"""
    if not mac_address:
        return mac_address
    # Remove all non-hex characters and convert to lowercase
    cleaned = re.sub(r'[^a-fA-F0-9]', '', mac_address)
    # Insert colons in the right places: aa:bb:cc:dd:ee:ff
    if len(cleaned) == 12:
        return ':'.join([cleaned[i:i+2] for i in range(0, 12, 2)]).lower()
    return cleaned.lower()


def get_device_id_by_mac(mac_address: str):
    """Get device ID from MAC address"""
    db = get_db()
    normalized = normalize_mac_address(mac_address)
    result = db.execute(
        "SELECT id FROM devices WHERE mac_address = ?",
        (normalized,),
        fetch_one=True
    )
    return result[0] if result else None


@reminder_bp.route('/', methods=['POST'])
def create_reminder():
    """
    Create a new reminder
    Request Body:
        {
            "device_mac": "aa:bb:cc:dd:ee:ff",  # or device_id
            "content": "Take medicine",
            "reminder_type": "once",  # 'once' or 'daily'
            "scheduled_time": "08:00",  # HH:MM format
            "scheduled_timestamp": 1736346000,  # Optional: Unix timestamp for one-time
            "skip_holidays": false
        }
    """
    try:
        data = request.get_json()
        if not data:
            return jsonify({
                'success': False,
                'error': 'Request body is required'
            }), 400

        # Get device ID
        device_id = data.get('device_id')
        if not device_id:
            mac_address = data.get('device_mac')
            if not mac_address:
                return jsonify({
                    'success': False,
                    'error': 'device_mac or device_id is required'
                }), 400
            device_id = get_device_id_by_mac(mac_address)
            if not device_id:
                return jsonify({
                    'success': False,
                    'error': 'Device not found'
                }), 404

        # Validate required fields
        content = data.get('content')
        if not content:
            return jsonify({
                'success': False,
                'error': 'content is required'
            }), 400

        reminder_type = data.get('reminder_type', 'once')
        if reminder_type not in ['once', 'daily']:
            return jsonify({
                'success': False,
                'error': 'reminder_type must be "once" or "daily"'
            }), 400

        scheduled_time = data.get('scheduled_time')
        if not scheduled_time:
            return jsonify({
                'success': False,
                'error': 'scheduled_time is required (HH:MM format)'
            }), 400

        # Validate time format
        try:
            time.strptime(scheduled_time, '%H:%M')
        except ValueError:
            return jsonify({
                'success': False,
                'error': 'scheduled_time must be in HH:MM format'
            }), 400

        # For one-time reminders, calculate timestamp
        scheduled_timestamp = data.get('scheduled_timestamp')
        if reminder_type == 'once' and not scheduled_timestamp:
            # If not provided, calculate from scheduled_time for TODAY
            # Parse HH:MM and calculate today's timestamp
            try:
                hours, minutes = map(int, scheduled_time.split(':'))
                today_midnight = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
                scheduled_timestamp = int(today_midnight.timestamp()) + hours * 3600 + minutes * 60

                # Check if the calculated time is already in the past
                current_ts = int(time.time())
                if scheduled_timestamp < current_ts:
                    # If it's already past today, schedule for tomorrow
                    scheduled_timestamp += 86400  # Add 24 hours
            except Exception as e:
                # Fallback: 1 hour from now
                scheduled_timestamp = int(time.time()) + 3600

        skip_holidays = data.get('skip_holidays', False)

        db = get_db()
        now = datetime.now().isoformat()

        # Insert reminder
        db.execute(
            """INSERT INTO reminders
               (device_id, content, reminder_type, scheduled_timestamp, scheduled_time, skip_holidays, created_at)
               VALUES (?, ?, ?, ?, ?, ?, ?)""",
            (device_id, content, reminder_type, scheduled_timestamp, scheduled_time, skip_holidays, now)
        )

        reminder_id = db.execute("SELECT last_insert_rowid()", fetch_one=True)[0]

        return jsonify({
            'success': True,
            'reminder_id': reminder_id,
            'message': 'Reminder created successfully',
            'next_trigger_at': scheduled_timestamp if reminder_type == 'once' else None
        })

    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@reminder_bp.route('/', methods=['GET'])
def list_reminders():
    """
    List reminders with filters
    Query Params:
        device_id: int (optional)
        device_mac: str (optional)
        status: 'active' | 'completed' | 'cancelled' | 'all' (default: 'active')
        limit: int (default: 50)
        offset: int (default: 0)
    """
    try:
        db = get_db()

        # Get filters
        device_id = request.args.get('device_id', type=int)
        status_filter = request.args.get('status', 'active')
        limit = request.args.get('limit', 50, type=int)
        offset = request.args.get('offset', 0, type=int)

        # Get device_id from mac if provided
        if not device_id:
            mac_address = request.args.get('device_mac')
            if mac_address:
                device_id = get_device_id_by_mac(mac_address)

        # Build query
        query = """
            SELECT id, device_id, content, reminder_type, scheduled_timestamp,
                   scheduled_time, status, skip_holidays, created_at, updated_at
            FROM reminders
            WHERE 1=1
        """
        params = []

        if device_id:
            query += " AND device_id = ?"
            params.append(device_id)

        if status_filter != 'all':
            query += " AND status = ?"
            params.append(status_filter)

        query += " ORDER BY created_at DESC LIMIT ? OFFSET ?"
        params.extend([limit, offset])

        rows = db.execute(query, tuple(params), fetch_all=True)

        # Filter expired one-time reminders and mark them as completed
        current_time = int(time.time())
        filtered_reminders = []
        expired_reminder_ids = []

        for row in rows:
            reminder_id = row[0]
            reminder_type = row[3]  # reminder_type
            scheduled_timestamp = row[4]  # scheduled_timestamp
            status = row[6]  # status

            # Only check active reminders
            if status == 'active' and reminder_type == 'once' and scheduled_timestamp:
                if scheduled_timestamp < current_time:
                    # Expired one-time reminder
                    expired_reminder_ids.append(reminder_id)
                    continue  # Don't include in response

            filtered_reminders.append(row)

        # Mark expired reminders as completed in database
        if expired_reminder_ids:
            now = datetime.now().isoformat()
            for rid in expired_reminder_ids:
                db.execute(
                    "UPDATE reminders SET status = 'completed', completed_at = ? WHERE id = ?",
                    (now, rid)
                )
            print(f"[ListReminders] Marked {len(expired_reminder_ids)} expired reminder(s) as completed")

        # Use filtered reminders
        rows = filtered_reminders

        # Get total count
        count_query = "SELECT COUNT(*) FROM reminders WHERE 1=1"
        count_params = []
        if device_id:
            count_query += " AND device_id = ?"
            count_params.append(device_id)
        if status_filter != 'all':
            count_query += " AND status = ?"
            count_params.append(status_filter)

        total_count = db.execute(count_query, tuple(count_params), fetch_one=True)[0]

        # Get active count
        active_count = 0
        if device_id:
            active_row = db.execute(
                "SELECT COUNT(*) FROM reminders WHERE device_id = ? AND status = 'active'",
                (device_id,),
                fetch_one=True
            )
            active_count = active_row[0] if active_row else 0

        reminders = []
        for row in rows:
            reminders.append({
                'id': row[0],
                'device_id': row[1],
                'content': row[2],
                'reminder_type': row[3],
                'scheduled_timestamp': row[4],
                'scheduled_time': row[5],
                'status': row[6],
                'skip_holidays': bool(row[7]),
                'created_at': row[8],
                'updated_at': row[9]
            })

        return jsonify({
            'success': True,
            'reminders': reminders,
            'total_count': total_count,
            'active_count': active_count,
            'limit': limit,
            'offset': offset
        })

    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@reminder_bp.route('/<int:reminder_id>', methods=['GET'])
def get_reminder(reminder_id: int):
    """Get reminder details"""
    try:
        db = get_db()

        row = db.execute(
            """SELECT id, device_id, remote_id, content, reminder_type,
                      scheduled_timestamp, scheduled_time, status,
                      skip_holidays, completed_at, created_at, updated_at, notes
               FROM reminders WHERE id = ?""",
            (reminder_id,),
            fetch_one=True
        )

        if not row:
            return jsonify({
                'success': False,
                'error': 'Reminder not found'
            }), 404

        reminder = {
            'id': row[0],
            'device_id': row[1],
            'remote_id': row[2],
            'content': row[3],
            'reminder_type': row[4],
            'scheduled_timestamp': row[5],
            'scheduled_time': row[6],
            'status': row[7],
            'skip_holidays': bool(row[8]),
            'completed_at': row[9],
            'created_at': row[10],
            'updated_at': row[11],
            'notes': row[12]
        }

        # Get device info
        device_row = db.execute(
            "SELECT device_name, mac_address FROM devices WHERE id = ?",
            (row[1],),
            fetch_one=True
        )

        if device_row:
            reminder['device_name'] = device_row[0]
            reminder['device_mac'] = device_row[1]

        return jsonify({
            'success': True,
            'reminder': reminder
        })

    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@reminder_bp.route('/<int:reminder_id>', methods=['PUT'])
def update_reminder(reminder_id: int):
    """
    Update reminder
    Request Body:
        {
            "content": "New content",
            "scheduled_time": "09:00",
            "skip_holidays": true
        }
    """
    try:
        data = request.get_json() or {}
        if not data:
            return jsonify({
                'success': False,
                'error': 'Request body is required'
            }), 400

        db = get_db()

        # Check if reminder exists
        existing = db.execute(
            "SELECT id FROM reminders WHERE id = ?",
            (reminder_id,),
            fetch_one=True
        )

        if not existing:
            return jsonify({
                'success': False,
                'error': 'Reminder not found'
            }), 404

        # Build update query dynamically
        update_fields = []
        params = []

        if 'content' in data:
            update_fields.append("content = ?")
            params.append(data['content'])

        if 'scheduled_time' in data:
            # Validate time format
            try:
                time.strptime(data['scheduled_time'], '%H:%M')
            except ValueError:
                return jsonify({
                    'success': False,
                    'error': 'scheduled_time must be in HH:MM format'
                }), 400
            update_fields.append("scheduled_time = ?")
            params.append(data['scheduled_time'])

            # Also update scheduled_timestamp to today's date with the new time
            hours, minutes = map(int, data['scheduled_time'].split(':'))
            today_midnight = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
            new_timestamp = int(today_midnight.timestamp()) + hours * 3600 + minutes * 60
            update_fields.append("scheduled_timestamp = ?")
            params.append(new_timestamp)

        if 'skip_holidays' in data:
            update_fields.append("skip_holidays = ?")
            params.append(data['skip_holidays'])

        if 'status' in data:
            if data['status'] not in ['active', 'completed', 'cancelled']:
                return jsonify({
                    'success': False,
                    'error': 'status must be "active", "completed", or "cancelled"'
                }), 400
            update_fields.append("status = ?")
            params.append(data['status'])

        if not update_fields:
            return jsonify({
                'success': False,
                'error': 'No valid fields to update'
            }), 400

        # Add updated_at
        update_fields.append("updated_at = ?")
        params.append(datetime.now().isoformat())

        # Add reminder_id
        params.append(reminder_id)

        query = f"UPDATE reminders SET {', '.join(update_fields)} WHERE id = ?"
        db.execute(query, tuple(params))

        return jsonify({
            'success': True,
            'reminder_id': reminder_id,
            'updated_at': params[-2]  # The second-to-last param is updated_at
        })

    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@reminder_bp.route('/<int:reminder_id>', methods=['DELETE'])
def delete_reminder(reminder_id: int):
    """Delete a reminder"""
    try:
        db = get_db()

        # Check if reminder exists
        existing = db.execute(
            "SELECT id FROM reminders WHERE id = ?",
            (reminder_id,),
            fetch_one=True
        )

        if not existing:
            return jsonify({
                'success': False,
                'error': 'Reminder not found'
            }), 404

        # Delete reminder
        db.execute("DELETE FROM reminders WHERE id = ?", (reminder_id,))

        return jsonify({
            'success': True,
            'message': 'Reminder deleted successfully'
        })

    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@reminder_bp.route('/<int:reminder_id>/complete', methods=['POST'])
def complete_reminder(reminder_id: int):
    """
    Mark a reminder as completed
    Request Body:
        {
            "notes": "Taken on time"
        }
    """
    try:
        data = request.get_json() or {}
        notes = data.get('notes')

        db = get_db()

        # Check if reminder exists
        existing = db.execute(
            "SELECT id FROM reminders WHERE id = ?",
            (reminder_id,),
            fetch_one=True
        )

        if not existing:
            return jsonify({
                'success': False,
                'error': 'Reminder not found'
            }), 404

        # Update reminder
        now = datetime.now().isoformat()
        db.execute(
            """UPDATE reminders
               SET status = 'completed', completed_at = ?, updated_at = ?, notes = ?
               WHERE id = ?""",
            (now, now, notes, reminder_id)
        )

        return jsonify({
            'success': True,
            'reminder_id': reminder_id,
            'completed_at': now
        })

    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


@reminder_bp.route('/stats', methods=['GET'])
def get_reminder_stats():
    """
    获取提醒统计信息

    返回当前用户所有设备的提醒统计

    Response:
        {
            "success": true,
            "active": int,           # 活跃提醒数量
            "completed_today": int   # 今日完成的提醒数量
        }
    """
    try:
        from database.connection import get_db
        db = get_db()

        # 获取当前时间（今天0点）
        from datetime import datetime, timedelta
        today_midnight = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
        today_timestamp = int(today_midnight.timestamp())

        # 查询活跃提醒数量（status = 'active'）
        active_result = db.execute(
            "SELECT COUNT(*) FROM reminders WHERE status = 'active'",
            fetch_one=True
        )
        active_count = active_result[0] if active_result else 0

        # 查询今日完成的提醒数量（status = 'completed' AND completed_at >= today）
        completed_result = db.execute(
            "SELECT COUNT(*) FROM reminders WHERE status = 'completed' AND completed_at IS NOT NULL",
            fetch_one=True
        )

        # 由于SQLite的日期比较，我们需要过滤今天的完成记录
        # 如果completed_at是ISO格式字符串，需要转换
        completed_today = 0
        if completed_result:
            # 获取所有今日完成的提醒记录
            rows = db.execute(
                """SELECT completed_at FROM reminders
                   WHERE status = 'completed' AND completed_at IS NOT NULL""",
                fetch_all=True
            )

            for row in rows:
                completed_at_str = row[0]
                if completed_at_str:
                    # 解析ISO格式字符串并比较
                    try:
                        completed_at = datetime.fromisoformat(completed_at_str)
                        if completed_at >= today_midnight:
                            completed_today += 1
                    except:
                        pass

        print(f"[ReminderStats] active={active_count}, completed_today={completed_today}")

        return jsonify({
            'success': True,
            'active': active_count,
            'completed_today': completed_today
        })

    except Exception as e:
        print(f"[ReminderStats] Error: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500


def register_reminder_routes(app):
    """Register reminder routes with Flask app"""
    app.register_blueprint(reminder_bp)
    print("[OK] Reminder CRUD routes registered")
    print("     Endpoints:")
    print("       POST   /api/reminders")
    print("       GET    /api/reminders")
    print("       GET    /api/reminders/stats        # 获取统计信息 ✨")
    print("       GET    /api/reminders/<id>")
    print("       PUT    /api/reminders/<id>")
    print("       DELETE /api/reminders/<id>")
    print("       POST   /api/reminders/<id>/complete")

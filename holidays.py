"""
Chinese Holiday Data (2026)
MVP: Placeholder data, Phase 2 will have complete dataset
"""
from typing import Optional

# Chinese holidays for 2026
# Format: "MM-DD": {"name": "Holiday Name", "type": "national|traditional", "observed": bool}
HOLIDAYS_2026 = {
    "01-01": {"name": "元旦", "type": "national", "observed": True},
    "02-17": {"name": "春节", "type": "traditional", "observed": True},
    "02-18": {"name": "春节", "type": "traditional", "observed": True},
    "02-19": {"name": "春节", "type": "traditional", "observed": True},
    "04-05": {"name": "清明节", "type": "traditional", "observed": True},
    "05-01": {"name": "劳动节", "type": "national", "observed": True},
    "06-19": {"name": "端午节", "type": "traditional", "observed": True},
    "09-25": {"name": "中秋节", "type": "traditional", "observed": True},
    "10-01": {"name": "国庆节", "type": "national", "observed": True},
    "10-02": {"name": "国庆节", "type": "national", "observed": True},
    "10-03": {"name": "国庆节", "type": "national", "observed": True},
}


def is_holiday(date_str: str) -> bool:
    """
    Check if a date is a holiday
    Args:
        date_str: Date in "MM-DD" format
    Returns:
        True if it's a holiday
    """
    return date_str in HOLIDAYS_2026


def get_holiday_name(date_str: str) -> Optional[str]:
    """
    Get holiday name for a date
    Args:
        date_str: Date in "MM-DD" format
    Returns:
        Holiday name or None
    """
    holiday = HOLIDAYS_2026.get(date_str)
    return holiday["name"] if holiday else None


def get_holidays_for_year(year: int) -> dict:
    """
    Get all holidays for a year
    Args:
        year: Year (e.g., 2026)
    Returns:
        Dictionary of holidays
    """
    if year == 2026:
        return HOLIDAYS_2026
    # TODO: Add more years in Phase 2
    return {}


if __name__ == "__main__":
    # Test holiday functions
    print("=== Chinese Holiday Test (2026) ===")

    test_dates = ["01-01", "02-18", "12-25"]
    for date in test_dates:
        is_hol = is_holiday(date)
        name = get_holiday_name(date)
        if is_hol:
            print(f"{date}: {name} ✓")
        else:
            print(f"{date}: Not a holiday")

    print(f"\nTotal holidays in 2026: {len(HOLIDAYS_2026)}")

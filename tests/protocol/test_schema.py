"""3MAP envelope smoke test (no hardware)."""

from pathlib import Path
import json

schema_path = Path(__file__).resolve().parents[2] / "protocol" / "3map" / "schema.json"


def test_schema_exists():
    assert schema_path.is_file()
    data = json.loads(schema_path.read_text(encoding="utf-8"))
    assert data["title"] == "3MAP envelope"

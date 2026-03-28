#!/usr/bin/env python3
"""Dump SentriFace logs in human-friendly formats.

Supports:
- current text format: "1234 I/module: message"
- planned binary format described in docs/logging/binary_logging_pipeline_spec.md
"""

from __future__ import annotations

import argparse
import collections
import gzip
import io
import json
import os
import re
import struct
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import BinaryIO, Iterable, Iterator, List


TEXT_PATTERN = re.compile(
    r"^(?:(?P<wall>\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}\.\d{3})\s+)?"
    r"(?:\+)?(?P<mono>\d+)(?:ms)?\s+"
    r"(?P<level>[A-Z])/(?P<module>[^:]+):\s(?P<message>.*)$"
)
FILE_MAGIC = b"FLOG"
FILE_HEADER = struct.Struct("<4sHH")
RECORD_HEADER = struct.Struct("<IIHBBHHH")


@dataclass
class LogRecord:
    mono_ms: int
    wall_sec: int
    wall_msec: int
    level: str
    module: str
    message: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Dump SentriFace text/binary logs")
    parser.add_argument("path", help="Log file path")
    parser.add_argument(
        "--input-format",
        choices=("auto", "text", "binary"),
        default="auto",
        help="Input log format",
    )
    parser.add_argument(
        "--output-format",
        choices=("brief", "elapsed", "full", "json", "logcat", "threadtime"),
        default="logcat",
        help="Rendered output style",
    )
    parser.add_argument(
        "--fields",
        default="wall,mono,level,module,message",
        help="Comma-separated fields for text output",
    )
    parser.add_argument(
        "--level",
        default="D,I,W,E",
        help="Allowed levels, e.g. I,W,E",
    )
    parser.add_argument(
        "--modules",
        default="",
        help="Comma-separated module filter",
    )
    parser.add_argument(
        "--grep",
        default="",
        help="Substring filter applied to the rendered message payload",
    )
    parser.add_argument(
        "--grep-regex",
        default="",
        help="Regex filter applied to the rendered message payload",
    )
    parser.add_argument(
        "--ignore-case",
        action="store_true",
        help="Use case-insensitive matching for --grep/--grep-regex",
    )
    parser.add_argument(
        "--tail",
        type=int,
        default=0,
        help="Only render the last N matched records",
    )
    parser.add_argument(
        "--count",
        action="store_true",
        help="Print only the number of matched records",
    )
    parser.add_argument(
        "--stats",
        action="store_true",
        help="Print summary statistics instead of individual records",
    )
    parser.add_argument(
        "--stats-format",
        choices=("text", "json"),
        default="text",
        help="Output format for --stats",
    )
    parser.add_argument(
        "--stats-top",
        type=int,
        default=0,
        help="Limit module/message-key summary to top N entries (0 = all)",
    )
    parser.add_argument(
        "--follow",
        action="store_true",
        help="Follow appended text log records (text/plain files only)",
    )
    parser.add_argument(
        "--follow-interval-ms",
        type=int,
        default=500,
        help="Polling interval for --follow",
    )
    parser.add_argument(
        "--color",
        choices=("auto", "always", "never"),
        default="auto",
        help="ANSI color mode",
    )
    parser.add_argument(
        "--show-fields",
        action="store_true",
        help="Prefix full/logcat-style output with selected field names",
    )
    return parser.parse_args()


def read_input_bytes(path: str) -> bytes:
    if path.endswith(".gz"):
        with gzip.open(path, "rb") as f:
            return f.read()
    if path.endswith(".zst"):
        try:
            completed = subprocess.run(
                ["zstd", "-dc", path],
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
        except FileNotFoundError as exc:
            raise ValueError("zstd command not found for .zst input") from exc
        except subprocess.CalledProcessError as exc:
            raise ValueError(
                f"zstd decompression failed: {exc.stderr.decode('utf-8', errors='replace').strip()}"
            ) from exc
        return completed.stdout
    with open(path, "rb") as f:
        return f.read()


def detect_binary(path: str) -> bool:
    return read_input_bytes(path)[:4] == FILE_MAGIC


def parse_text(path: str) -> Iterator[LogRecord]:
    with open_input_text(path) as f:
        for line in f:
            line = line.rstrip("\n")
            match = TEXT_PATTERN.match(line)
            if not match:
                continue
            wall_sec, wall_msec = parse_text_wall(match.group("wall"))
            yield LogRecord(
                mono_ms=int(match.group("mono")),
                wall_sec=wall_sec,
                wall_msec=wall_msec,
                level=match.group("level"),
                module=match.group("module"),
                message=match.group("message"),
            )


def follow_text(path: str,
                start_at_end: bool = True,
                poll_interval_ms: int = 500) -> Iterator[LogRecord]:
    with open(path, "r", encoding="utf-8") as f:
        if start_at_end:
            f.seek(0, os.SEEK_END)

        while True:
            line = f.readline()
            if not line:
                time.sleep(max(poll_interval_ms, 10) / 1000.0)
                continue
            line = line.rstrip("\n")
            match = TEXT_PATTERN.match(line)
            if not match:
                continue
            wall_sec, wall_msec = parse_text_wall(match.group("wall"))
            yield LogRecord(
                mono_ms=int(match.group("mono")),
                wall_sec=wall_sec,
                wall_msec=wall_msec,
                level=match.group("level"),
                module=match.group("module"),
                message=match.group("message"),
            )


def parse_text_wall(wall_text: str | None) -> tuple[int, int]:
    if not wall_text:
        return 0, 0
    current_year = datetime.now().year
    try:
        dt = datetime.strptime(f"{current_year}-{wall_text}", "%Y-%m-%d %H:%M:%S.%f")
    except ValueError:
        return 0, 0
    return int(dt.timestamp()), dt.microsecond // 1000


def parse_binary(path: str) -> Iterator[LogRecord]:
    with open_input_binary(path) as f:
        yield from parse_binary_stream(f)


def open_input_binary(path: str) -> BinaryIO:
    return io.BytesIO(read_input_bytes(path))


def open_input_text(path: str):
    data = read_input_bytes(path)
    return io.StringIO(data.decode("utf-8", errors="replace"))


def parse_binary_stream(f: BinaryIO) -> Iterator[LogRecord]:
    header = f.read(FILE_HEADER.size)
    if len(header) != FILE_HEADER.size:
        return
    magic, version, _reserved = FILE_HEADER.unpack(header)
    if magic != FILE_MAGIC:
        raise ValueError("invalid binary log magic")
    if version != 1:
        raise ValueError(f"unsupported binary log version: {version}")

    while True:
        raw = f.read(RECORD_HEADER.size)
        if not raw:
            return
        if len(raw) != RECORD_HEADER.size:
            raise ValueError("truncated binary record header")
        mono_ms, wall_sec, wall_msec, level_raw, _flags, module_len, message_len, _reserved = (
            RECORD_HEADER.unpack(raw)
        )
        module_bytes = f.read(module_len)
        message_bytes = f.read(message_len)
        if len(module_bytes) != module_len or len(message_bytes) != message_len:
            raise ValueError("truncated binary record payload")
        yield LogRecord(
            mono_ms=mono_ms,
            wall_sec=wall_sec,
            wall_msec=wall_msec,
            level=chr(level_raw),
            module=module_bytes.decode("utf-8", errors="replace"),
            message=message_bytes.decode("utf-8", errors="replace"),
        )


def should_color(mode: str) -> bool:
    if mode == "always":
        return True
    if mode == "never":
        return False
    return sys.stdout.isatty() and os.environ.get("TERM", "") != "dumb"


def level_color_code(level: str) -> str:
    codes = {
        "E": "38;5;196",
        "W": "38;5;220",
        "I": "38;5;45",
        "D": "38;5;244",
    }
    return codes.get(level, "0")


def paint(text: str, code: str, enable: bool) -> str:
    if not enable:
        return text
    return f"\033[{code}m{text}\033[0m"


def colorize(level: str, text: str, enable: bool) -> str:
    return paint(text, level_color_code(level), enable)


def format_wall(record: LogRecord) -> str:
    if record.wall_sec == 0:
        return "-"
    dt = datetime.fromtimestamp(record.wall_sec, tz=timezone.utc).astimezone()
    return dt.strftime("%m-%d %H:%M:%S") + f".{record.wall_msec:03d}"


def format_wall_time_only(record: LogRecord) -> str:
    if record.wall_sec == 0:
        return "--:--:--.---"
    dt = datetime.fromtimestamp(record.wall_sec, tz=timezone.utc).astimezone()
    return dt.strftime("%H:%M:%S") + f".{record.wall_msec:03d}"


def render_text(record: LogRecord, fields: List[str], show_fields: bool) -> str:
    parts: List[str] = []
    for field in fields:
        if field == "wall":
            value = format_wall(record)
        elif field == "mono":
            value = f"+{record.mono_ms}ms"
        elif field == "level":
            value = record.level
        elif field == "module":
            value = record.module
        elif field == "message":
            value = record.message
        else:
            continue
        if show_fields:
            parts.append(f"{field}={value}")
        else:
            parts.append(value)
    return " ".join(part for part in parts if part)


def render_record(record: LogRecord, output_format: str, fields: List[str], show_fields: bool) -> str:
    if output_format == "json":
        return json.dumps(
            {
                "wall": format_wall(record),
                "mono_ms": record.mono_ms,
                "level": record.level,
                "module": record.module,
                "message": record.message,
            },
            ensure_ascii=False,
        )
    if output_format == "brief":
        return f"{record.level}/{record.module}: {record.message}"
    if output_format == "elapsed":
        return f"+{record.mono_ms}ms {record.level}/{record.module}: {record.message}"
    if output_format == "logcat":
        return f"{format_wall(record)} {record.level}/{record.module}: {record.message}"
    if output_format == "threadtime":
        return (
            f"{format_wall_time_only(record)} +{record.mono_ms:>6}ms "
            f"{record.level}/{record.module}: {record.message}"
        )
    if output_format == "full":
        return render_text(record, fields, show_fields)
    raise ValueError(f"unsupported output format: {output_format}")


def render_with_color(record: LogRecord,
                      output_format: str,
                      fields: List[str],
                      show_fields: bool,
                      enable_color: bool) -> str:
    if output_format == "json":
        return render_record(record, output_format, fields, show_fields)

    if output_format == "brief":
        return (
            f"{paint(record.level, level_color_code(record.level), enable_color)}/"
            f"{paint(record.module, '38;5;81', enable_color)}: {record.message}"
        )
    if output_format == "elapsed":
        return (
            f"{paint(f'+{record.mono_ms}ms', '38;5;242', enable_color)} "
            f"{paint(record.level, level_color_code(record.level), enable_color)}/"
            f"{paint(record.module, '38;5;81', enable_color)}: {record.message}"
        )
    if output_format == "logcat":
        return (
            f"{paint(format_wall(record), '38;5;242', enable_color)} "
            f"{paint(record.level, level_color_code(record.level), enable_color)}/"
            f"{paint(record.module, '38;5;81', enable_color)}: {record.message}"
        )
    if output_format == "threadtime":
        return (
            f"{paint(format_wall_time_only(record), '38;5;242', enable_color)} "
            f"{paint(f'+{record.mono_ms:>6}ms', '38;5;242', enable_color)} "
            f"{paint(record.level, level_color_code(record.level), enable_color)}/"
            f"{paint(record.module, '38;5;81', enable_color)}: {record.message}"
        )
    if output_format == "full":
        return colorize(
            record.level,
            render_record(record, output_format, fields, show_fields),
            enable_color,
        )
    raise ValueError(f"unsupported output format: {output_format}")


def filter_records(records: Iterable[LogRecord], levels: set[str], modules: set[str]) -> Iterator[LogRecord]:
    for record in records:
        if levels and record.level not in levels:
            continue
        if modules and record.module not in modules:
            continue
        yield record


def grep_records(records: Iterable[LogRecord],
                 grep_text: str,
                 grep_pattern: str,
                 ignore_case: bool) -> Iterator[LogRecord]:
    regex = None
    if grep_pattern:
        flags = re.IGNORECASE if ignore_case else 0
        regex = re.compile(grep_pattern, flags)

    grep_cmp = grep_text.lower() if ignore_case else grep_text

    for record in records:
        haystack = record.message.lower() if ignore_case else record.message
        if grep_cmp and grep_cmp not in haystack:
            continue
        if regex is not None and regex.search(record.message) is None:
            continue
        yield record


def message_key(record: LogRecord) -> str:
    text = record.message.strip()
    if not text:
        return "-"
    return text.split()[0]


def trim_counter(counter: collections.Counter[str], top_n: int) -> dict[str, int]:
    items = sorted(counter.items(), key=lambda item: (-item[1], item[0]))
    if top_n > 0:
        items = items[:top_n]
    return dict(items)


def render_stats(records: List[LogRecord], stats_format: str, stats_top: int) -> str:
    level_counts = collections.Counter(record.level for record in records)
    module_counts = collections.Counter(record.module for record in records)
    message_counts = collections.Counter(message_key(record) for record in records)
    trimmed_module_counts = trim_counter(module_counts, stats_top)
    trimmed_message_counts = trim_counter(message_counts, stats_top)
    first_wall = format_wall(records[0]) if records else "-"
    last_wall = format_wall(records[-1]) if records else "-"
    first_mono = records[0].mono_ms if records else 0
    last_mono = records[-1].mono_ms if records else 0
    if stats_format == "json":
        return json.dumps(
            {
                "records": len(records),
                "first_wall": first_wall,
                "last_wall": last_wall,
                "first_mono_ms": first_mono,
                "last_mono_ms": last_mono,
                "levels": dict(sorted(level_counts.items())),
                "modules": trimmed_module_counts,
                "message_keys": trimmed_message_counts,
            },
            ensure_ascii=False,
        )
    lines = [
        f"records={len(records)}",
        f"first_wall={first_wall}",
        f"last_wall={last_wall}",
        f"first_mono_ms={first_mono}",
        f"last_mono_ms={last_mono}",
        "levels="
        + ",".join(f"{level}:{level_counts[level]}" for level in sorted(level_counts)),
        "modules="
        + ",".join(
            f"{module}:{count}" for module, count in trimmed_module_counts.items()
        ),
        "message_keys="
        + ",".join(
            f"{key}:{count}" for key, count in trimmed_message_counts.items()
        ),
    ]
    return "\n".join(lines)


def main() -> int:
    args = parse_args()
    input_format = args.input_format
    if input_format == "auto":
        input_format = "binary" if detect_binary(args.path) else "text"

    if args.follow and input_format != "text":
        print("logcat_dump.py: --follow currently supports text logs only", file=sys.stderr)
        return 1
    if args.follow and (args.path.endswith(".gz") or args.path.endswith(".zst")):
        print("logcat_dump.py: --follow does not support compressed inputs", file=sys.stderr)
        return 1

    if args.follow:
        records = follow_text(args.path, start_at_end=True, poll_interval_ms=args.follow_interval_ms)
    elif input_format == "binary":
        records = parse_binary(args.path)
    else:
        records = parse_text(args.path)

    level_filter = {value.strip() for value in args.level.split(",") if value.strip()}
    module_filter = {value.strip() for value in args.modules.split(",") if value.strip()}
    fields = [value.strip() for value in args.fields.split(",") if value.strip()]
    use_color = should_color(args.color)

    try:
        filtered_records = grep_records(
            filter_records(records, level_filter, module_filter),
            args.grep,
            args.grep_regex,
            args.ignore_case,
        )

        if args.follow:
            for record in filtered_records:
                rendered = render_with_color(
                    record,
                    args.output_format,
                    fields,
                    args.show_fields,
                    use_color,
                )
                print(rendered, flush=True)
            return 0

        matched_records = list(filtered_records)
        if args.tail > 0:
            matched_records = matched_records[-args.tail:]
        if args.count:
            print(len(matched_records))
            return 0
        if args.stats:
            print(render_stats(matched_records, args.stats_format, args.stats_top))
            return 0

        for record in matched_records:
            rendered = render_with_color(
                record,
                args.output_format,
                fields,
                args.show_fields,
                use_color,
            )
            print(rendered)
    except ValueError as exc:
        print(f"logcat_dump.py: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

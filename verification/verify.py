#!/usr/bin/env python3

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

from scheduler.config import EXPECTED as SCHEDULER_EXPECTED
from scheduler.config import GENERATOR_OPTIONS as SCHEDULER_OPTIONS
from scheduler.generate_tessla import generate as generate_scheduler


ROOT_DIR = Path(__file__).resolve().parent
BUILD_DIR = ROOT_DIR / "build"

DEFAULT_TESSLA_JAR = Path.home() / "Desktop" / "tessla.jar"

OUTPUT_PATTERN = re.compile(
    r"^[^:]+:\s+([A-Za-z_][A-Za-z0-9_]*)\s*="
)


@dataclass(frozen=True)
class VerificationModule:
    name: str
    directory: Path
    generator: Callable[..., str]
    generator_options: dict[str, object]
    expected: dict[str, set[str]]


MODULES = {
    "scheduler": VerificationModule(
        name="scheduler",
        directory=ROOT_DIR / "scheduler",
        generator=generate_scheduler,
        generator_options=SCHEDULER_OPTIONS,
        expected=SCHEDULER_EXPECTED,
    ),
}


def select_modules(
    requested: list[str],
) -> list[VerificationModule]:
    if not requested:
        return [
            MODULES[name]
            for name in sorted(MODULES)
        ]

    selected: list[VerificationModule] = []

    for name in requested:
        module = MODULES.get(name)

        if module is None:
            available = ", ".join(sorted(MODULES))

            raise ValueError(
                f"Unknown module '{name}'. "
                f"Available modules: {available}"
            )

        selected.append(module)

    return selected


def generated_spec_path(module: VerificationModule) -> Path:
    return BUILD_DIR / f"{module.name}.tessla"


def generate_specification(
    module: VerificationModule,
    mode: str,
    overrides: dict[str, object] | None = None,
) -> Path:
    options = dict(module.generator_options)
    options["mode"] = mode

    if overrides:
        options.update(overrides)

    specification = module.generator(**options)

    if not isinstance(specification, str):
        raise TypeError(
            f"{module.name} generator must return str"
        )

    output_path = generated_spec_path(module)

    output_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    output_path.write_text(
        specification,
        encoding="utf-8",
    )

    return output_path


def extract_output_streams(output: str) -> set[str]:
    streams: set[str] = set()

    for line in output.splitlines():
        match = OUTPUT_PATTERN.match(line)

        if match is not None:
            streams.add(match.group(1))

    return streams


def run_tessla(
    tessla_jar: Path,
    specification: Path,
    trace: Path,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            "java",
            "-jar",
            str(tessla_jar),
            "interpreter",
            str(specification),
            str(trace),
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )


def format_streams(streams: set[str]) -> str:
    if not streams:
        return "no violations"

    return ", ".join(sorted(streams))


def command_list(
    _args: argparse.Namespace,
) -> int:
    for name in sorted(MODULES):
        module = MODULES[name]
        test_dir = module.directory / "test"
        test_count = len(list(test_dir.glob("*.input")))

        print(f"{name}: {test_count} test(s)")

    return 0


def command_generate(
    args: argparse.Namespace,
) -> int:
    try:
        modules = select_modules(args.modules)
    except ValueError as error:
        print(f"[ERROR] {error}", file=sys.stderr)
        return 1

    overrides: dict[str, object] = {}

    if args.max_tasks is not None:
        overrides["max_tasks"] = args.max_tasks

    if args.quantum is not None:
        overrides["quantum_ticks"] = args.quantum

    failures = 0

    for module in modules:
        try:
            output_path = generate_specification(
                module=module,
                mode=args.mode,
                overrides=overrides,
            )

            print(
                f"[GENERATED] {module.name}: "
                f"{output_path.relative_to(ROOT_DIR)}"
            )

        except Exception as error:
            print(
                f"[ERROR] {module.name}: {error}",
                file=sys.stderr,
            )
            failures += 1

    return 1 if failures else 0


def run_module_tests(
    module: VerificationModule,
    tessla_jar: Path,
    verbose: bool,
) -> tuple[int, int]:
    print(f"\n=== {module.name} ===")

    specification = generate_specification(
        module=module,
        mode="violations",
    )

    test_dir = module.directory / "test"

    existing_tests = {
        path.name
        for path in test_dir.glob("*.input")
    }

    configured_tests = set(module.expected)

    passed = 0
    failed = 0

    for test_name in sorted(
        existing_tests - configured_tests
    ):
        print(
            f"[FAIL] {test_name}: "
            "no expected result configured"
        )
        failed += 1

    for test_name in sorted(
        configured_tests - existing_tests
    ):
        print(
            f"[FAIL] {test_name}: "
            "test file not found"
        )
        failed += 1

    for test_name in sorted(
        existing_tests & configured_tests
    ):
        trace_path = test_dir / test_name
        expected_streams = module.expected[test_name]

        result = run_tessla(
            tessla_jar=tessla_jar,
            specification=specification,
            trace=trace_path,
        )

        if result.returncode != 0:
            print(
                f"[FAIL] {test_name}: "
                "TeSSLa interpreter error"
            )

            if result.stdout:
                for line in result.stdout.splitlines():
                    print(f"  {line}")

            failed += 1
            continue

        actual_streams = extract_output_streams(
            result.stdout
        )

        if actual_streams == expected_streams:
            print(f"[PASS] {test_name}")
            passed += 1

            if verbose and result.stdout:
                for line in result.stdout.splitlines():
                    print(f"  {line}")

            continue

        print(f"[FAIL] {test_name}")
        print(
            f"  Expected: "
            f"{format_streams(expected_streams)}"
        )
        print(
            f"  Actual:   "
            f"{format_streams(actual_streams)}"
        )

        if result.stdout:
            print("  Output:")

            for line in result.stdout.splitlines():
                print(f"    {line}")

        failed += 1

    print(f"Passed: {passed}")
    print(f"Failed: {failed}")

    return passed, failed


def command_test(
    args: argparse.Namespace,
) -> int:
    if not args.tessla_jar.is_file():
        print(
            f"[ERROR] TeSSLa JAR not found: "
            f"{args.tessla_jar}",
            file=sys.stderr,
        )
        return 1

    try:
        modules = select_modules(args.modules)
    except ValueError as error:
        print(f"[ERROR] {error}", file=sys.stderr)
        return 1

    total_passed = 0
    total_failed = 0

    for module in modules:
        try:
            passed, failed = run_module_tests(
                module=module,
                tessla_jar=args.tessla_jar,
                verbose=args.verbose,
            )

            total_passed += passed
            total_failed += failed

        except Exception as error:
            print(
                f"\n[ERROR] {module.name}: {error}",
                file=sys.stderr,
            )
            total_failed += 1

    print("\n=== Overall result ===")
    print(f"Passed: {total_passed}")
    print(f"Failed: {total_failed}")

    return 1 if total_failed else 0


def command_clean(
    _args: argparse.Namespace,
) -> int:
    if not BUILD_DIR.exists():
        print("Nothing to clean.")
        return 0

    removed = 0

    for path in BUILD_DIR.glob("*.tessla"):
        path.unlink()
        removed += 1

    try:
        BUILD_DIR.rmdir()
    except OSError:
        pass

    print(f"Removed {removed} generated file(s).")

    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Generate and test TeSSLa verification modules."
        )
    )

    subparsers = parser.add_subparsers(
        dest="command",
        required=True,
    )

    list_parser = subparsers.add_parser(
        "list",
        help="List verification modules.",
    )

    list_parser.set_defaults(
        handler=command_list,
    )

    generate_parser = subparsers.add_parser(
        "generate",
        help="Generate one or all TeSSLa specifications.",
    )

    generate_parser.add_argument(
        "modules",
        nargs="*",
        help="Modules to generate. Default: all.",
    )

    generate_parser.add_argument(
        "--mode",
        choices=["violations", "checks"],
        default="violations",
    )

    generate_parser.add_argument(
        "--max-tasks",
        type=int,
        help=(
            "Override the configured task count "
            "for manual generation."
        ),
    )

    generate_parser.add_argument(
        "--quantum",
        type=int,
        help=(
            "Override the configured quantum "
            "for manual generation."
        ),
    )

    generate_parser.set_defaults(
        handler=command_generate,
    )

    test_parser = subparsers.add_parser(
        "test",
        help=(
            "Generate specifications using module test "
            "configuration and run tests."
        ),
    )

    test_parser.add_argument(
        "modules",
        nargs="*",
        help="Modules to test. Default: all.",
    )

    test_parser.add_argument(
        "--tessla-jar",
        type=Path,
        default=Path(
            os.environ.get(
                "TESSLA_JAR",
                DEFAULT_TESSLA_JAR,
            )
        ),
    )

    test_parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print output from passing tests.",
    )

    test_parser.set_defaults(
        handler=command_test,
    )

    clean_parser = subparsers.add_parser(
        "clean",
        help="Remove generated specifications.",
    )

    clean_parser.set_defaults(
        handler=command_clean,
    )

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if hasattr(args, "max_tasks"):
        if args.max_tasks is not None and args.max_tasks <= 0:
            parser.error("--max-tasks must be greater than 0")

    if hasattr(args, "quantum"):
        if args.quantum is not None and args.quantum <= 0:
            parser.error("--quantum must be greater than 0")

    return args.handler(args)


if __name__ == "__main__":
    raise SystemExit(main())
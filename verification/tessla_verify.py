"""Generate, compile, and test the project's TeSSLa verification monitors."""

import argparse
import os
import re
import signal
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

from scheduler.config import EXPECTED as SCHEDULER_EXPECTED
from scheduler.config import GENERATOR_OPTIONS as SCHEDULER_OPTIONS
from scheduler.generate_tessla import generate as generate_scheduler

from delay.config import EXPECTED as DELAY_EXPECTED
from delay.config import GENERATOR_OPTIONS as DELAY_OPTIONS
from delay.generate_tessla import generate as generate_delay

from integrity.config import EXPECTED as INTEGRITY_EXPECTED
from integrity.config import GENERATOR_OPTIONS as INTEGRITY_OPTIONS
from integrity.generate_tessla import generate as generate_integrity

from semaphore.config import EXPECTED as SEMAPHORE_EXPECTED
from semaphore.config import GENERATOR_OPTIONS as SEMAPHORE_OPTIONS
from semaphore.generate_tessla import generate as generate_semaphore

from mutex.config import EXPECTED as MUTEX_EXPECTED
from mutex.config import GENERATOR_OPTIONS as MUTEX_OPTIONS
from mutex.generate_tessla import generate as generate_mutex

from queue.config import EXPECTED as QUEUE_EXPECTED
from queue.config import GENERATOR_OPTIONS as QUEUE_OPTIONS
from queue.generate_tessla import generate as generate_queue

ROOT_DIR = Path(__file__).resolve().parent
BUILD_DIR = ROOT_DIR / "build"

OUTPUT_PATTERN = re.compile(r"^[^:]+:\s+([A-Za-z_][A-Za-z0-9_]*)\s*=")

INPUT_PATTERN = re.compile(r"^in\s+([A-Za-z_][A-Za-z0-9_]*)\s*:")

DEFINITION_PATTERN = re.compile(r"^def\s+([A-Za-z_][A-Za-z0-9_]*)")

OUTPUT_DECLARATION_PATTERN = re.compile(r"^out\s+([A-Za-z_][A-Za-z0-9_]*)\s*$")


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
    "delay": VerificationModule(
        name="delay",
        directory=ROOT_DIR / "delay",
        generator=generate_delay,
        generator_options=DELAY_OPTIONS,
        expected=DELAY_EXPECTED,
    ),
    "integrity": VerificationModule(
        name="integrity",
        directory=ROOT_DIR / "integrity",
        generator=generate_integrity,
        generator_options=INTEGRITY_OPTIONS,
        expected=INTEGRITY_EXPECTED,
    ),
    "semaphore": VerificationModule(
        name="semaphore",
        directory=ROOT_DIR / "semaphore",
        generator=generate_semaphore,
        generator_options=SEMAPHORE_OPTIONS,
        expected=SEMAPHORE_EXPECTED,
    ),
    "mutex": VerificationModule(
        name="mutex",
        directory=ROOT_DIR / "mutex",
        generator=generate_mutex,
        generator_options=MUTEX_OPTIONS,
        expected=MUTEX_EXPECTED,
    ),
    "queue": VerificationModule(
        name="queue",
        directory=ROOT_DIR / "queue",
        generator=generate_queue,
        generator_options=QUEUE_OPTIONS,
        expected=QUEUE_EXPECTED,
    ),
}


GENERATOR_ARGUMENTS = {
    "max_tasks": (
        "max_tasks",
        "--max-tasks",
    ),
    "quantum_ticks": (
        "quantum",
        "--quantum",
    ),
    "max_semaphores": (
        "max_semaphores",
        "--max-semaphores",
    ),
    "max_mutexes": (
        "max_mutexes",
        "--max-mutexes",
    ),
    "queue_capacities": (
        "queues",
        "--queue",
    ),
}


def select_modules(
    requested: list[str],
) -> list[VerificationModule]:
    if not requested:
        return [MODULES[name] for name in sorted(MODULES)]

    selected: list[VerificationModule] = []

    for name in requested:
        module = MODULES.get(name)

        if module is None:
            available = ", ".join(sorted(MODULES))

            raise ValueError(f"Unknown module '{name}'. " f"Available modules: {available}")

        selected.append(module)

    return selected


def generated_spec_path(
    module: VerificationModule,
) -> Path:
    return BUILD_DIR / f"{module.name}.tessla"


def validate_generation_arguments(
    modules: list[VerificationModule],
    args: argparse.Namespace,
) -> None:
    required_by: dict[str, list[str]] = {}

    for module in modules:
        for option_name in module.generator_options:
            argument = GENERATOR_ARGUMENTS.get(option_name)

            if argument is None:
                raise ValueError(f"module '{module.name}' has " f"unsupported generator option " f"'{option_name}'")

            attribute, flag = argument

            if getattr(args, attribute) is None:
                required_by.setdefault(
                    flag,
                    [],
                ).append(module.name)

    if not required_by:
        return

    missing = ", ".join(f"{flag} ({'/'.join(module_names)})" for flag, module_names in required_by.items())

    raise ValueError("missing required generation option(s): " + missing)


def generate_specification(
    module: VerificationModule,
    mode: str,
    overrides: dict[str, object] | None = None,
) -> Path:
    options = dict(module.generator_options)
    options["mode"] = mode

    if overrides:
        options.update({name: value for name, value in overrides.items() if name in module.generator_options})

    specification = module.generator(**options)

    if not isinstance(specification, str):
        raise TypeError(f"{module.name} generator must return str")

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


def combine_specifications(
    specifications: list[tuple[str, str]],
) -> str:
    inputs: dict[str, str] = {}
    definitions: dict[str, str] = {}
    outputs: set[str] = set()

    body_lines: list[str] = []
    output_lines: list[str] = []

    for module_name, specification in specifications:
        body_lines.append("")

        for line in specification.splitlines():
            input_match = INPUT_PATTERN.match(line)

            if input_match is not None:
                stream_name = input_match.group(1)
                previous = inputs.get(stream_name)

                if previous is None:
                    inputs[stream_name] = line

                elif previous != line:
                    raise ValueError(f"conflicting input " f"'{stream_name}' in module " f"'{module_name}'")

                continue

            definition_match = DEFINITION_PATTERN.match(line)

            if definition_match is not None:
                definition_name = definition_match.group(1)

                previous_module = definitions.get(definition_name)

                if previous_module is not None:
                    raise ValueError(
                        f"duplicate definition "
                        f"'{definition_name}' in modules "
                        f"'{previous_module}' and "
                        f"'{module_name}'"
                    )

                definitions[definition_name] = module_name

            output_match = OUTPUT_DECLARATION_PATTERN.match(line)

            if output_match is not None:
                output_name = output_match.group(1)

                if output_name in outputs:
                    continue

                outputs.add(output_name)
                output_lines.append(line)
                continue

            body_lines.append(line)

    sections = [
        "\n".join(inputs.values()),
        "\n".join(body_lines).strip(),
        "\n".join(output_lines),
    ]

    return "\n\n".join(section for section in sections if section) + "\n"


def write_combined_specification(
    modules: list[VerificationModule],
    mode: str,
    overrides: dict[str, object],
) -> Path:
    specifications: list[tuple[str, str]] = []

    for module in modules:
        options = dict(module.generator_options)
        options["mode"] = mode

        options.update({name: value for name, value in overrides.items() if name in module.generator_options})

        specification = module.generator(**options)

        if not isinstance(specification, str):
            raise TypeError(f"{module.name} generator must return str")

        specifications.append(
            (
                module.name,
                specification,
            )
        )

    combined = combine_specifications(specifications)
    output_path = BUILD_DIR / "combined.tessla"

    output_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    output_path.write_text(
        combined,
        encoding="utf-8",
    )

    return output_path


def extract_output_streams(
    output: str,
) -> set[str]:
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


def rust_monitor_path(
    specification: Path,
) -> Path:
    return specification.with_name(f"{specification.stem}-monitor")


def compile_rust_monitor(
    tessla_jar: Path,
    specification: Path,
) -> tuple[Path, subprocess.CompletedProcess[str]]:
    monitor = rust_monitor_path(specification)

    compiler_environment = os.environ.copy()

    existing_rustflags = compiler_environment.get(
        "RUSTFLAGS",
        "",
    ).strip()

    compiler_environment["RUSTFLAGS"] = f"{existing_rustflags} -Awarnings".strip()

    command = [
        "java",
        "-jar",
        str(tessla_jar),
        "compile-rust",
        "-b",
        str(monitor),
        str(specification),
    ]

    print(
        f"[COMPILING] " f"{specification.relative_to(ROOT_DIR)} -> " f"{monitor.relative_to(ROOT_DIR)}",
        flush=True,
    )

    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
        start_new_session=(os.name == "posix"),
        env=compiler_environment,
    )

    output: list[str] = []
    suppress_warning = False

    try:
        if process.stdout is None:
            raise RuntimeError("failed to capture Rust compiler output")

        for line in process.stdout:
            stripped = line.lstrip()

            progress = stripped.startswith(
                (
                    "Updating ",
                    "Locking ",
                    "Downloading ",
                    "Downloaded ",
                    "Compiling ",
                    "Checking ",
                    "Finished ",
                    "Running ",
                )
            )

            error = stripped.startswith("error:") or stripped.startswith("error[")

            if stripped.startswith("warning:") or stripped.startswith("warning["):
                suppress_warning = True
                continue

            if suppress_warning:
                if not progress and not error:
                    continue

                suppress_warning = False

            output.append(line)

            print(
                f"  {line}",
                end="",
                flush=True,
            )

        returncode = process.wait()

    except KeyboardInterrupt:
        if process.poll() is None:
            if os.name == "posix":
                os.killpg(
                    process.pid,
                    signal.SIGINT,
                )
            else:
                process.terminate()

            try:
                process.wait(timeout=5)

            except subprocess.TimeoutExpired:
                if os.name == "posix":
                    os.killpg(
                        process.pid,
                        signal.SIGKILL,
                    )
                else:
                    process.kill()

                process.wait()

        raise

    finally:
        if process.stdout is not None:
            process.stdout.close()

    result = subprocess.CompletedProcess(
        args=command,
        returncode=returncode,
        stdout="".join(output),
    )

    return monitor, result


def run_rust_monitor(
    monitor: Path,
    trace: Path,
) -> subprocess.CompletedProcess[str]:
    with trace.open(
        "r",
        encoding="utf-8",
    ) as trace_input:
        return subprocess.run(
            [str(monitor)],
            stdin=trace_input,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )


def report_compile_failure(
    name: str,
    result: subprocess.CompletedProcess[str],
) -> None:
    print(
        f"[ERROR] {name}: " f"Rust monitor compilation failed",
        file=sys.stderr,
    )

    if result.stdout:
        for line in result.stdout.splitlines():
            print(
                f"  {line}",
                file=sys.stderr,
            )


def format_streams(
    streams: set[str],
) -> str:
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
        print(
            f"[ERROR] {error}",
            file=sys.stderr,
        )
        return 1

    try:
        validate_generation_arguments(
            modules,
            args,
        )

    except ValueError as error:
        print(
            f"[ERROR] {error}",
            file=sys.stderr,
        )
        return 1

    overrides: dict[str, object] = {}

    if args.max_tasks is not None:
        overrides["max_tasks"] = args.max_tasks

    if args.quantum is not None:
        overrides["quantum_ticks"] = args.quantum

    if args.max_semaphores is not None:
        overrides["max_semaphores"] = args.max_semaphores

    if args.max_mutexes is not None:
        overrides["max_mutexes"] = args.max_mutexes

    if args.queues is not None:
        queue_capacities = dict(args.queues)

        if len(queue_capacities) != len(args.queues):
            print(
                "[ERROR] duplicate queue id",
                file=sys.stderr,
            )
            return 1

        overrides["queue_capacities"] = queue_capacities

    failures = 0

    if args.combined:
        try:
            output_path = write_combined_specification(
                modules=modules,
                mode=args.mode,
                overrides=overrides,
            )

            print("[GENERATED] combined: " f"{output_path.relative_to(ROOT_DIR)}")

            if args.rust:
                monitor, result = compile_rust_monitor(
                    args.tessla_jar,
                    output_path,
                )

                if result.returncode != 0 or not monitor.is_file():
                    report_compile_failure(
                        "combined",
                        result,
                    )
                    return 1

                print("[COMPILED] combined: " f"{monitor.relative_to(ROOT_DIR)}")

            return 0

        except Exception as error:
            print(
                f"[ERROR] combined: {error}",
                file=sys.stderr,
            )
            return 1

    for module in modules:
        try:
            output_path = generate_specification(
                module=module,
                mode=args.mode,
                overrides=overrides,
            )

            print(f"[GENERATED] {module.name}: " f"{output_path.relative_to(ROOT_DIR)}")

            if args.rust:
                monitor, result = compile_rust_monitor(
                    args.tessla_jar,
                    output_path,
                )

                if result.returncode != 0 or not monitor.is_file():
                    report_compile_failure(
                        module.name,
                        result,
                    )
                    failures += 1
                    continue

                print(f"[COMPILED] {module.name}: " f"{monitor.relative_to(ROOT_DIR)}")

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
    use_rust: bool,
) -> tuple[int, int]:
    print(f"\n=== {module.name} ===")

    specification = generate_specification(
        module=module,
        mode="violations",
    )

    monitor: Path | None = None

    if use_rust:
        monitor, compile_result = compile_rust_monitor(
            tessla_jar,
            specification,
        )

        if compile_result.returncode != 0 or not monitor.is_file():
            details = compile_result.stdout.strip()

            message = f"{module.name} Rust monitor " f"compilation failed"

            if details:
                message += f"\n{details}"

            raise RuntimeError(message)

        print(f"[COMPILED] " f"{monitor.relative_to(ROOT_DIR)}")

    test_dir = module.directory / "test"

    existing_tests = {path.name for path in test_dir.glob("*.input")}

    configured_tests = set(module.expected)

    passed = 0
    failed = 0

    for test_name in sorted(existing_tests - configured_tests):
        print(f"[FAIL] {test_name}: " f"no expected result configured")
        failed += 1

    for test_name in sorted(configured_tests - existing_tests):
        print(f"[FAIL] {test_name}: " f"test file not found")
        failed += 1

    for test_name in sorted(existing_tests & configured_tests):
        trace_path = test_dir / test_name
        expected_streams = module.expected[test_name]

        if monitor is None:
            result = run_tessla(
                tessla_jar=tessla_jar,
                specification=specification,
                trace=trace_path,
            )
        else:
            result = run_rust_monitor(
                monitor=monitor,
                trace=trace_path,
            )

        if result.returncode != 0:
            backend = "Rust monitor" if monitor is not None else "TeSSLa interpreter"

            print(f"[FAIL] {test_name}: " f"{backend} error")

            if result.stdout:
                for line in result.stdout.splitlines():
                    print(f"  {line}")

            failed += 1
            continue

        actual_streams = extract_output_streams(result.stdout)

        if actual_streams == expected_streams:
            print(f"[PASS] {test_name}")
            passed += 1

            if verbose and result.stdout:
                for line in result.stdout.splitlines():
                    print(f"  {line}")

            continue

        print(f"[FAIL] {test_name}")

        print(f"  Expected: " f"{format_streams(expected_streams)}")

        print(f"  Actual:   " f"{format_streams(actual_streams)}")

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
    if args.tessla_jar is None:
        print(
            "[ERROR] TeSSLa JAR not specified. " "Use --tessla-jar PATH or set TESSLA_JAR.",
            file=sys.stderr,
        )
        return 1

    if not args.tessla_jar.is_file():
        print(
            f"[ERROR] TeSSLa JAR not found: " f"{args.tessla_jar}",
            file=sys.stderr,
        )
        return 1

    try:
        modules = select_modules(args.modules)
    except ValueError as error:
        print(
            f"[ERROR] {error}",
            file=sys.stderr,
        )
        return 1

    backend = "Rust" if args.rust else "TeSSLa interpreter"

    print(f"[BACKEND] {backend}")

    total_passed = 0
    total_failed = 0

    for module in modules:
        try:
            passed, failed = run_module_tests(
                module=module,
                tessla_jar=args.tessla_jar,
                verbose=args.verbose,
                use_rust=args.rust,
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

    for pattern in (
        "*.tessla",
        "*-monitor",
    ):
        for path in BUILD_DIR.glob(pattern):
            if path.is_file():
                path.unlink()
                removed += 1

    try:
        BUILD_DIR.rmdir()
    except OSError:
        pass

    print(f"Removed {removed} generated file(s).")

    return 0


def parse_queue(
    value: str,
) -> tuple[int, int]:
    try:
        queue_id_text, capacity_text = value.split(":", 1)

        queue_id = int(queue_id_text)
        capacity = int(capacity_text)

    except ValueError as error:
        raise argparse.ArgumentTypeError("queue must use QUEUE_ID:CAPACITY") from error

    if queue_id < 0:
        raise argparse.ArgumentTypeError("queue id must be non-negative")

    if capacity <= 0:
        raise argparse.ArgumentTypeError("queue capacity must be greater than zero")

    return queue_id, capacity


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=("Generate and test TeSSLa " "verification modules."))

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
        help=("Generate one or all " "TeSSLa specifications."),
    )

    generate_parser.add_argument(
        "modules",
        nargs="*",
        help="Modules to generate. Default: all.",
    )

    generate_parser.add_argument(
        "--mode",
        choices=[
            "violations",
            "checks",
        ],
        default="violations",
    )

    generate_parser.add_argument(
        "--combined",
        action="store_true",
        help=("Generate one combined specification " "for all selected modules."),
    )

    generate_parser.add_argument(
        "--max-tasks",
        type=int,
        help=("Required when scheduler, delay, semaphore, " "mutex, or queue is selected."),
    )

    generate_parser.add_argument(
        "--quantum",
        type=int,
        help=("Required when the scheduler module " "is selected."),
    )

    generate_parser.add_argument(
        "--max-semaphores",
        type=int,
        help=("Required when the semaphore module " "is selected."),
    )

    generate_parser.add_argument(
        "--max-mutexes",
        type=int,
        help=("Required when the mutex module " "is selected."),
    )

    generate_parser.add_argument(
        "--queue",
        dest="queues",
        action="append",
        type=parse_queue,
        metavar="QUEUE_ID:CAPACITY",
        help=("Required when queue is selected. " "Repeat for multiple queues."),
    )

    generate_parser.add_argument(
        "--rust",
        action="store_true",
        help=("Compile each generated specification " "to a Rust monitor."),
    )

    generate_parser.add_argument(
        "--tessla-jar",
        type=Path,
        default=(Path(os.environ["TESSLA_JAR"]) if "TESSLA_JAR" in os.environ else None),
        help=("TeSSLa JAR used by --rust. " "Defaults to TESSLA_JAR."),
    )

    generate_parser.set_defaults(
        handler=command_generate,
    )

    test_parser = subparsers.add_parser(
        "test",
        help=("Generate specifications using module " "test configuration and run tests."),
    )

    test_parser.add_argument(
        "modules",
        nargs="*",
        help="Modules to test. Default: all.",
    )

    test_parser.add_argument(
        "--tessla-jar",
        type=Path,
        default=(Path(os.environ["TESSLA_JAR"]) if "TESSLA_JAR" in os.environ else None),
        help="Path to tessla.jar. Defaults to TESSLA_JAR.",
    )

    test_parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print output from passing tests.",
    )

    test_parser.add_argument(
        "--rust",
        action="store_true",
        help=("Compile one Rust monitor per module " "and reuse it for all tests."),
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

    if getattr(args, "rust", False):
        if args.tessla_jar is None:
            parser.error("TeSSLa JAR not specified. " "Use --tessla-jar PATH or set TESSLA_JAR.")

        if not args.tessla_jar.is_file():
            parser.error(f"TeSSLa JAR not found: {args.tessla_jar}")

    if hasattr(args, "max_tasks"):
        if args.max_tasks is not None and args.max_tasks <= 0:
            parser.error("--max-tasks must be greater than 0")

    if hasattr(args, "quantum"):
        if args.quantum is not None and args.quantum <= 0:
            parser.error("--quantum must be greater than 0")

    if hasattr(args, "max_semaphores"):
        if args.max_semaphores is not None and args.max_semaphores <= 0:
            parser.error("--max-semaphores must be " "greater than 0")

    if hasattr(args, "max_mutexes"):
        if args.max_mutexes is not None and args.max_mutexes <= 0:
            parser.error("--max-mutexes must be greater than 0")

    try:
        return args.handler(args)

    except KeyboardInterrupt:
        print(
            "\n[CANCELLED] interrupted by user",
            file=sys.stderr,
        )
        return 130


if __name__ == "__main__":
    raise SystemExit(main())

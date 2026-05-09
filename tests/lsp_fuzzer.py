#!/usr/bin/env python3
import argparse
import collections
import json
import os
import random
import select
import string
import subprocess
import sys
import tempfile
import threading
import time
import traceback


def _send_message(proc, msg_dict):
    payload = json.dumps(msg_dict, ensure_ascii=False)
    payload_bytes = payload.encode("utf-8")
    header = f"Content-Length: {len(payload_bytes)}\r\n\r\n".encode("utf-8")
    try:
        proc.stdin.write(header + payload_bytes)
        proc.stdin.flush()
    except (BrokenPipeError, OSError) as exc:
        raise RuntimeError(f"stdin write failed (server dead?): {exc}") from exc


class RpcTransport:
    def __init__(self, proc):
        self._proc = proc
        self._buf = bytearray()

    def _fill_buffer(self, deadline):
        while time.monotonic() < deadline:
            remain = max(0.0, deadline - time.monotonic())
            ready, _, _ = select.select([self._proc.stdout], [], [], min(remain, 0.1))
            if not ready:
                continue

            try:
                chunk = os.read(self._proc.stdout.fileno(), 65536)
            except OSError:
                return False

            if not chunk:
                return False

            self._buf.extend(chunk)
            return True
        return True

    def read_message(self, timeout):
        deadline = time.monotonic() + timeout
        search_start = 0

        while time.monotonic() < deadline:
            hdr_end = self._buf.find(b"\r\n\r\n", search_start)
            if hdr_end < 0:
                search_start = max(0, len(self._buf) - 3)
                if not self._fill_buffer(deadline):
                    if len(self._buf) == 0:
                        return None, "EOF"

                    if not self._buf[:15].lower().startswith(b"content-length:"):
                        self._buf.clear()
                        return None, "error: garbage on EOF"

                    return None, None
                continue

            hdr_str = self._buf[:hdr_end].decode("utf-8", errors="replace")
            cl = None
            for line in hdr_str.split("\r\n"):
                if line.lower().startswith("content-length:"):
                    try:
                        cl = int(line.split(":", 1)[1].strip())
                    except ValueError:
                        self._buf = self._buf[hdr_end + 4 :]
                        return None, f"error: bad Content-Length: {line!r}"

            if cl is None:
                self._buf = self._buf[hdr_end + 4 :]
                search_start = 0
                continue

            if cl < 0:
                self._buf = self._buf[hdr_end + 4 :]
                search_start = 0
                continue

            body_start = hdr_end + 4
            while len(self._buf) - body_start < cl:
                if time.monotonic() >= deadline:
                    return None, None

                if not self._fill_buffer(deadline):
                    self._buf.clear()
                    return None, "error: EOF reading body"

            body_bytes = bytes(self._buf[body_start : body_start + cl])
            self._buf = self._buf[body_start + cl :]
            try:
                return json.loads(body_bytes), None
            except json.JSONDecodeError as exc:
                return None, f"error: JSON parse: {exc}"

        return None, None

    def drain_pending(self, timeout):
        msgs = []
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            msg, err = self.read_message(min(timeout, 1.0))
            if msg is None:
                break

            msgs.append(msg)

        return msgs


DCC_KEYWORDS = [
    "module",
    "import",
    "pub",
    "fn",
    "var",
    "let",
    "mut",
    "struct",
    "union",
    "enum",
    "using",
    "return",
    "if",
    "else",
    "while",
    "for",
    "in",
    "match",
    "defer",
    "static_if",
    "static_match",
    "sizeof",
    "alignof",
    "offsetof",
    "compiles",
    "as",
    "cast",
    "true",
    "false",
    "null",
    "void",
    "bool",
    "char",
    "i8",
    "i16",
    "i32",
    "i64",
    "u8",
    "u16",
    "u32",
    "u64",
    "f32",
    "f64",
    "const",
    "volatile",
    "restrict",
    "type",
]

DCC_OPERATORS = [
    "+",
    "-",
    "*",
    "/",
    "%",
    "=",
    "==",
    "!=",
    "<",
    ">",
    "<=",
    ">=",
    "&&",
    "||",
    "!",
    "&",
    "|",
    "^",
    "~",
    "<<",
    ">>",
    "+=",
    "-=",
    "*=",
    "/=",
    "%=",
    "&=",
    "|=",
    "^=",
    "<<=",
    ">>=",
    "->",
    ".",
    "..",
    "...",
    "::",
    "=>",
    "@",
    "#",
    "$",
]

DCC_DELIMITERS = ["(", ")", "[", "]", "{", "}", ",", ";", ":", "\n"]


class TextMutator:
    def _rand_keyword(self):
        return self.rng.choice(DCC_KEYWORDS)

    def _rand_op(self):
        return self.rng.choice(DCC_OPERATORS)

    def _rand_delim(self):
        return self.rng.choice(DCC_DELIMITERS)

    def _rand_ascii(self):
        return chr(self.rng.randint(0x20, 0x7E))

    def _rand_unicode(self):
        r = self.rng.randint(0x80, 0x10FFFF)
        if 0xD800 <= r <= 0xDFFF:
            r = 0x80
        try:
            return chr(r)
        except ValueError:
            return "\u00a9"

    def _rand_null(self):
        return "\x00"

    def _gen_deep_nesting(self):
        depth = self.rng.randint(5, 50)
        pieces = ["fn f() {\n"]
        for _ in range(depth):
            t = self.rng.random()
            if t < 0.33:
                pieces.append("{\n")
            elif t < 0.66:
                pieces.append("(\n")
            else:
                pieces.append("[\n")

        for _ in range(self.rng.randint(0, depth // 2)):
            t = self.rng.random()
            if t < 0.33:
                pieces.append("}\n")
            elif t < 0.66:
                pieces.append(")\n")
            else:
                pieces.append("]\n")

        pieces.append("}\n")
        return "".join(pieces)

    def _gen_random_bytes(self):
        length = self.rng.randint(16, min(4096, self.max_bytes))
        pieces = []
        for _ in range(length):
            r = self.rng.randint(0, 255)
            if r < 32:
                pieces.append(chr(r))
            elif r < 128:
                pieces.append(chr(r))
            elif r < 200:
                pieces.append(chr(r))
            else:
                pieces.append(chr(0xC0 + self.rng.randint(0, 31)))

        return "".join(pieces)

    def _gen_keyword_soup(self):
        count = self.rng.randint(4, 20)
        pieces = []
        for _ in range(count):
            t = self.rng.random()
            if t < 0.15:
                pieces.append(self._rand_keyword())
            elif t < 0.45:
                pieces.append(self._rand_op())
            elif t < 0.75:
                pieces.append(self._rand_delim())
            elif t < 0.95:
                pieces.append(self._rand_ascii())
            else:
                pieces.append(self._rand_unicode())

            if self.rng.random() < 0.15:
                pieces.append(" ")

        return "".join(pieces)

    def _gen_unterminated(self):
        kind = self.rng.choice(
            ["string", "char", "block_comment", "line_comment", "raw_string"]
        )
        prefix = {
            "string": '"',
            "char": "'",
            "block_comment": "/*",
            "line_comment": "//",
            "raw_string": 'r#"',
        }[kind]

        fill_len = self.rng.randint(16, min(2048, self.max_bytes))
        pieces = [prefix]
        for _ in range(fill_len):
            pieces.append(self._rand_ascii())
            if self.rng.random() < 0.05:
                pieces.append(self._rand_unicode())
            if self.rng.random() < 0.03:
                pieces.append("\x00")

        return "".join(pieces)

    def _gen_large_number(self):
        num_digits = self.rng.randint(20, min(500, self.max_bytes // 2))
        pieces = []
        if self.rng.random() < 0.5:
            pieces.append("-")
        if self.rng.random() < 0.3:
            pieces.append("0x")
        elif self.rng.random() < 0.3:
            pieces.append("0b")
        elif self.rng.random() < 0.3:
            pieces.append("0o")

        digits = string.digits + "abcdefABCDEF_."
        for _ in range(num_digits):
            pieces.append(self.rng.choice(digits))

        return "".join(pieces)

    def _gen_unicode_chaos(self):
        count = self.rng.randint(16, min(512, self.max_bytes // 2))
        pieces = []
        for _ in range(count):
            pieces.append(self._rand_unicode())

        return "".join(pieces)

    def _gen_repeated_braces(self):
        count = self.rng.randint(10, min(200, self.max_bytes // 2))
        brace_pool = ["{", "}", "(", ")", "[", "]", "{{", "}}", "[[]]", "{;}", "(())"]
        pieces = []
        for _ in range(count):
            pieces.append(self.rng.choice(brace_pool))

        return "".join(pieces)

    def _gen_semi_valid(self):
        pieces = ["module test;\n", "import std;\n\n"]
        func_count = self.rng.randint(1, 3)
        for _ in range(func_count):
            ret = self.rng.choice(["void", "i32", "bool", self._rand_unicode()])
            name = "".join(
                self.rng.choice(string.ascii_letters)
                for _ in range(self.rng.randint(1, 12))
            )

            pieces.append(f"fn {name}(")
            param_count = self.rng.randint(0, 3)
            for j in range(param_count):
                if j > 0:
                    pieces.append(", ")

                ptype = self.rng.choice(["i32", "bool", "char", self._rand_unicode()])
                pname = "".join(
                    self.rng.choice(string.ascii_letters)
                    for _ in range(self.rng.randint(0, 6))
                )
                pieces.append(f"{ptype} {pname}")

            pieces.append(") {\n")
            stmt_count = self.rng.randint(0, 4)
            for _ in range(stmt_count):
                t = self.rng.random()
                if t < 0.5:
                    pieces.append("    return ")
                    if self.rng.random() < 0.5:
                        pieces.append(str(self.rng.randint(0, 9999)))
                    else:
                        pieces.append("true" if self.rng.random() < 0.5 else "false")

                    pieces.append(";\n")
                elif t < 0.8:
                    pieces.append("    var x")
                    pieces.append(str(self.rng.randint(0, 9)))
                    pieces.append(" = ")
                    pieces.append(str(self.rng.randint(0, 100)))
                    pieces.append(";\n")
                else:
                    pieces.append(self._rand_unicode() * self.rng.randint(1, 6))
                    pieces.append("\n")

            if self.rng.random() < 0.3:
                pieces.append("}\n")
            else:
                pieces.append("\n")

        return "".join(pieces)

    SAFE_STRATEGIES = [
        ("deep_nesting", _gen_deep_nesting),
        ("random_bytes", _gen_random_bytes),
        ("unterminated", _gen_unterminated),
        ("large_number", _gen_large_number),
        ("repeated_braces", _gen_repeated_braces),
    ]

    STRESS_STRATEGIES = [
        ("keyword_soup", _gen_keyword_soup),
        ("unicode_chaos", _gen_unicode_chaos),
        ("semi_valid", _gen_semi_valid),
    ]

    def __init__(self, seed, max_bytes, stress=False):
        self.rng = random.Random(seed)
        self.max_bytes = max_bytes
        self.stress = stress
        self.last_strategy = None
        self.last_text = None

    def generate(self):
        pool = self.SAFE_STRATEGIES
        if self.stress:
            pool = self.SAFE_STRATEGIES + self.STRESS_STRATEGIES

        name, fn = self.rng.choice(pool)
        text = fn(self)
        self.last_strategy = name
        self.last_text = text
        return name, text


class _TimeoutError(Exception):
    def __init__(self, method):
        super().__init__(method)
        self.method = method


class _ServerDied(Exception):
    def __init__(self, reason):
        super().__init__(str(reason))
        self.reason = reason


class LspFuzzer:
    def __init__(self, args):
        self.server_path = args.server
        self.iterations = args.iterations
        self.seed = args.seed
        self.timeout = args.timeout
        self.verbose = args.verbose
        self.workspace_root = args.workspace_root
        self.uri = args.uri
        self.restart_on_timeout = args.restart_on_timeout
        self.drain_timeout = args.drain_timeout
        self.mutator = TextMutator(
            self.seed, args.max_bytes, stress=args.stress_size is not None
        )
        self.req_id = 0
        self.proc = None
        self.transport = None
        self.last_diagnostics = {}
        self._stderr_chunks = collections.deque(maxlen=512)
        self._stderr_lock = threading.Lock()
        self._stderr_crash_marker = None

    def _next_id(self):
        self.req_id += 1
        return self.req_id

    def _log(self, msg):
        if self.verbose:
            print(f"[{time.strftime('%H:%M:%S')}] {msg}", file=sys.stderr, flush=True)

    def _start_stderr_drain(self):
        proc = self.proc
        if proc is None or proc.stderr is None:
            return

        crash_markers = [
            "AddressSanitizer",
            "LeakSanitizer",
            "SIGSEGV",
            "SIGABRT",
            "SIGILL",
            "SIGFPE",
            "SIGBUS",
            "double free",
            "use-after-free",
            "buffer overflow",
            "stack-buffer-overflow",
            "heap-buffer-overflow",
            "heap-use-after-free",
            "stack-use-after-return",
            "global-buffer-overflow",
            "DEADLYSIGNAL",
        ]

        def _reader():
            while True:
                try:
                    chunk = os.read(proc.stderr.fileno(), 65536)
                except OSError:
                    break

                if not chunk:
                    break

                text = chunk.decode("utf-8", errors="replace")
                with self._stderr_lock:
                    self._stderr_chunks.append(text)
                    if self._stderr_crash_marker is None:
                        for marker in crash_markers:
                            if marker in text:
                                self._stderr_crash_marker = marker
                                break

        threading.Thread(target=_reader, daemon=True).start()

    def _stderr_tail(self):
        with self._stderr_lock:
            return "".join(self._stderr_chunks)

    def _spawn_server(self):
        try:
            self.proc = subprocess.Popen(
                [self.server_path],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                bufsize=0,
                cwd=self.workspace_root,
            )
        except FileNotFoundError:
            self._fail(f"server binary not found: {self.server_path}")
        except Exception as exc:
            self._fail(f"failed to spawn server: {exc}")

        self.transport = RpcTransport(self.proc)
        self._start_stderr_drain()

    def _fail(self, msg):
        if self._stderr_crash_marker is not None:
            print(f"\n*** {self._stderr_crash_marker} DETECTED ***", file=sys.stderr)
            print(self._stderr_tail()[-8000:], file=sys.stderr)

        print(f"FAIL: {msg}", file=sys.stderr, flush=True)
        sys.exit(1)

    def _check_alive(self):
        if self.proc is None:
            self._fail("server process is None")

        poll = self.proc.poll()
        if poll is not None:
            self._fail(f"server died with exit code {poll}")

    def _expect_response(self, method, params):
        rid = self._next_id()
        req = {"jsonrpc": "2.0", "method": method, "params": params, "id": rid}
        _send_message(self.proc, req)

        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            if self.proc.poll() is not None:
                raise _ServerDied(self.proc.returncode)

            remain = max(0.1, deadline - time.monotonic())
            msg, err = self.transport.read_message(remain)
            if msg is None:
                if err and err.startswith("error:"):
                    self._log(f"  transport error: {err}")
                if err == "EOF":
                    raise _ServerDied("EOF on stdout")

                continue

            if "method" in msg and "id" not in msg:
                if msg["method"] == "textDocument/publishDiagnostics":
                    p = msg.get("params", {})
                    self.last_diagnostics[p.get("uri", "")] = p.get("diagnostics", [])
                    if self.verbose:
                        self._log(f"  diag: {len(p.get('diagnostics',[]))} diags")

                continue

            if "id" in msg:
                if msg["id"] == rid:
                    return msg.get("result"), msg.get("error")

                if self.verbose:
                    self._log(f"  stale response id={msg['id']} (expect {rid})")

                continue

            if self.verbose:
                self._log(f"  unexpected msg: {str(msg)[:100]}")

        raise _TimeoutError(method)

    def _restart_server(self):
        if self.proc:
            try:
                self.proc.kill()
                self.proc.wait(timeout=3)
            except Exception:
                pass

        for attempt in range(3):
            self._spawn_server()

            _send_message(
                self.proc,
                {
                    "jsonrpc": "2.0",
                    "method": "initialize",
                    "params": {
                        "processId": os.getpid(),
                        "rootUri": f"file://{self.workspace_root}",
                        "capabilities": {},
                    },
                    "id": 1,
                },
            )

            deadline = time.monotonic() + 15.0
            ok = False
            while time.monotonic() < deadline:
                msg, err = self.transport.read_message(5.0)
                if msg and "id" in msg and msg["id"] == 1:
                    if msg.get("error"):
                        self._log(f"  restart init error: {msg['error']}")
                    else:
                        ok = True
                    break

                if err == "EOF" or self.proc.poll() is not None:
                    break
            if ok:
                break

            try:
                self.proc.kill()
                self.proc.wait(timeout=2)
            except Exception:
                pass

            time.sleep(0.5)
        else:
            self._fail("could not re-initialize server after 3 attempts")

        _send_message(
            self.proc, {"jsonrpc": "2.0", "method": "initialized", "params": {}}
        )

        _send_message(
            self.proc,
            {
                "jsonrpc": "2.0",
                "method": "textDocument/didOpen",
                "params": {
                    "textDocument": {
                        "uri": self.uri,
                        "languageId": "dc",
                        "version": 0,
                        "text": "module test;\nimport std;\n\nfn main() {\n    return 0;\n}\n",
                    }
                },
            },
        )

        time.sleep(0.1)
        for m in self.transport.drain_pending(self.drain_timeout):
            if m.get("method") == "textDocument/publishDiagnostics":
                p = m.get("params", {})
                self.last_diagnostics[p.get("uri", "")] = p.get("diagnostics", [])

        self.req_id = 0
        self._log(f"  server restarted (PID {self.proc.pid})")

    def _write_failing_input(self, strategy, text):
        try:
            fd, path = tempfile.mkstemp(
                prefix="dcc_fuzz_fail_", suffix=".dc", text=True
            )

            with os.fdopen(fd, "w", encoding="utf-8", errors="replace") as f:
                f.write(f"// seed = {self.seed}  strategy = {strategy}\n")
                f.write(f"// text_length={len(text)}\n")
                f.write(text)

            print(f"  failing input written to {path}", file=sys.stderr)
            return path
        except Exception as exc:
            print(f"  could not write failing input: {exc}", file=sys.stderr)
            return None

    def _check_stderr(self):
        if self._stderr_crash_marker is not None:
            print(f"\n*** {self._stderr_crash_marker} DETECTED ***", file=sys.stderr)
            print(self._stderr_tail()[-8000:], file=sys.stderr)
            return True

        return False

    def run(self):
        self._log(
            f"Starting: server = {self.server_path}  iterations = {self.iterations}  "
            f"seed = {self.seed}  timeout = {self.timeout}s  max_bytes = {self.mutator.max_bytes}"
        )

        self._spawn_server()
        self._log(f"Server PID: {self.proc.pid}")

        try:
            self._log("initialize...")
            init_params = {
                "processId": os.getpid(),
                "rootUri": f"file://{self.workspace_root}",
                "capabilities": {},
                "workspaceFolders": [
                    {"uri": f"file://{self.workspace_root}", "name": "workspace"}
                ],
            }

            result, error = self._expect_response("initialize", init_params)
            if error:
                self._fail(f"initialize error: {error}")

            self._log("initialize OK")

            _send_message(
                self.proc, {"jsonrpc": "2.0", "method": "initialized", "params": {}}
            )

            self._log(f"didOpen {self.uri}")
            _send_message(
                self.proc,
                {
                    "jsonrpc": "2.0",
                    "method": "textDocument/didOpen",
                    "params": {
                        "textDocument": {
                            "uri": self.uri,
                            "languageId": "dc",
                            "version": 0,
                            "text": "module test;\nimport std;\n\nfn main() {\n    return 0;\n}\n",
                        }
                    },
                },
            )

            time.sleep(0.1)
            for m in self.transport.drain_pending(max(self.drain_timeout, 0.5)):
                if m.get("method") == "textDocument/publishDiagnostics":
                    p = m.get("params", {})
                    self.last_diagnostics[p.get("uri", "")] = p.get("diagnostics", [])

            pass_count = 0
            fail_count = 0
            for iteration in range(1, self.iterations + 1):
                self._current_iter = iteration

                if self.verbose or iteration % 100 == 0:
                    self._log(
                        f"iter {iteration}/{self.iterations}"
                        + (f"  fails={fail_count}" if fail_count else "")
                    )

                strategy, text = self.mutator.generate()
                self._current_strategy = strategy
                self._current_text_len = len(text)

                utf8_len = len(text.encode("utf-8", errors="replace"))
                if utf8_len > self.mutator.max_bytes:
                    while len(text) > 0:
                        text = text[: len(text) * 3 // 4]
                        if (
                            len(text.encode("utf-8", errors="replace"))
                            <= self.mutator.max_bytes
                        ):
                            break

                if self.verbose:
                    self._log(f"  strategy={strategy}  len={len(text)}")

                try:
                    _send_message(
                        self.proc,
                        {
                            "jsonrpc": "2.0",
                            "method": "textDocument/didChange",
                            "params": {
                                "textDocument": {"uri": self.uri, "version": iteration},
                                "contentChanges": [{"text": text}],
                            },
                        },
                    )
                except RuntimeError:
                    self._log(f"  server dead before didChange")
                    self._restart_server()
                    fail_count += 1
                    continue

                if self.proc.poll() is None:
                    for m in self.transport.drain_pending(self.drain_timeout):
                        if m.get("method") == "textDocument/publishDiagnostics":
                            p = m.get("params", {})
                            self.last_diagnostics[p.get("uri", "")] = p.get(
                                "diagnostics", []
                            )

                queries = []

                if self.mutator.rng.random() < 0.8:
                    qline = self.mutator.rng.randint(0, 1000)
                    qchar = self.mutator.rng.randint(0, 500)
                    queries.append(
                        (
                            "textDocument/hover",
                            {
                                "textDocument": {"uri": self.uri},
                                "position": {"line": qline, "character": qchar},
                            },
                        )
                    )

                if self.mutator.rng.random() < 0.5:
                    queries.append(
                        (
                            "textDocument/semanticTokens/full",
                            {
                                "textDocument": {"uri": self.uri},
                            },
                        )
                    )

                if self.mutator.rng.random() < 0.4:
                    qline = self.mutator.rng.randint(0, 1000)
                    qchar = self.mutator.rng.randint(0, 500)
                    queries.append(
                        (
                            "textDocument/documentHighlight",
                            {
                                "textDocument": {"uri": self.uri},
                                "position": {"line": qline, "character": qchar},
                            },
                        )
                    )

                if self.mutator.rng.random() < 0.2:
                    diags = self.last_diagnostics.get(self.uri, [])
                    queries.append(
                        (
                            "textDocument/codeAction",
                            {
                                "textDocument": {"uri": self.uri},
                                "range": {
                                    "start": {"line": 0, "character": 0},
                                    "end": {"line": 1000, "character": 500},
                                },
                                "context": {"diagnostics": diags},
                            },
                        )
                    )

                for method, params in queries:
                    try:
                        result, error = self._expect_response(method, params)
                        if error and self.verbose:
                            self._log(f"  {method} error: {error}")
                    except _TimeoutError as exc:
                        failing_path = self._write_failing_input(strategy, text)
                        detail = (
                            f"timeout waiting for response to {exc.method}\n"
                            f"  seed = {self.seed}  iteration = {iteration}  strategy = {strategy}  text_len = {len(text)}"
                        )

                        if failing_path:
                            detail += f"\n  input={failing_path}"

                        if self.restart_on_timeout:
                            self._log("  " + detail.replace("\n", " | "))
                            fail_count += 1
                            self._restart_server()
                            break

                        self._fail(detail)
                    except _ServerDied:
                        if self.restart_on_timeout:
                            self._log(f"  server died; restarting")
                            fail_count += 1
                            self._restart_server()
                            break

                        failing_path = self._write_failing_input(strategy, text)
                        detail = (
                            f"server died during {method}\n"
                            f"  seed={self.seed}  iteration={iteration}  strategy={strategy}  text_len={len(text)}"
                        )

                        if failing_path:
                            detail += f"\n  input={failing_path}"

                        self._fail(detail)
                else:
                    pass_count += 1

            self._log("shutdown...")
            try:
                _send_message(
                    self.proc,
                    {
                        "jsonrpc": "2.0",
                        "method": "shutdown",
                        "params": {},
                        "id": self._next_id(),
                    },
                )
                self.transport.read_message(min(self.timeout, 5.0))
            except Exception:
                pass

            try:
                _send_message(
                    self.proc, {"jsonrpc": "2.0", "method": "exit", "params": {}}
                )
            except Exception:
                pass

            try:
                self.proc.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()

            crashed = self._check_stderr()
            rc = self.proc.returncode if self.proc.poll() is not None else 0

            if crashed:
                self._fail("server crashed")

            if fail_count > 0:
                print(
                    f"\nPASS/FAIL: {pass_count} passed, {fail_count} timeouts/restarts "
                    f"(seed = {self.seed}, timeout = {self.timeout}s, max_bytes = {self.mutator.max_bytes})",
                    file=sys.stderr,
                )
                sys.exit(1)
            else:
                print(
                    f"\nPASS: all {pass_count} iterations completed "
                    f"(seed = {self.seed}, timeout = {self.timeout}s)",
                    file=sys.stderr,
                )

        except SystemExit:
            raise
        except Exception:
            traceback.print_exc()
            if self.proc and self.proc.poll() is None:
                self.proc.kill()
                self.proc.wait()
            sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="LSP JSON-RPC fuzzer for dccd")
    parser.add_argument(
        "--server", default="./build/bin/dccd", help="Path to dccd binary"
    )
    parser.add_argument(
        "--iterations", type=int, default=1000, help="Number of fuzz iterations"
    )
    parser.add_argument("--seed", type=int, default=None, help="Random seed")
    parser.add_argument(
        "--timeout", type=float, default=10.0, help="Per-request timeout in seconds"
    )
    parser.add_argument(
        "--verbose", action="store_true", help="Verbose logging to stderr"
    )
    parser.add_argument(
        "--workspace-root", default=os.getcwd(), help="Workspace root directory"
    )
    parser.add_argument(
        "--uri",
        default="file:///tmp/dcc_fuzz_test.dc",
        help="URI for the test document",
    )
    parser.add_argument(
        "--max-bytes", type=int, default=8192, help="Maximum generated-text UTF-8 size"
    )
    parser.add_argument(
        "--stress-size",
        type=int,
        default=None,
        help="Override --max-bytes for stress tests",
    )
    parser.add_argument(
        "--restart-on-timeout",
        action="store_true",
        help="Restart dccd and continue after a timeout instead of failing immediately",
    )
    parser.add_argument(
        "--drain-timeout",
        type=float,
        default=0.05,
        help="Per-iteration notification drain timeout in seconds",
    )

    args = parser.parse_args()

    if args.seed is None:
        args.seed = random.randint(0, 2**31 - 1)

    if args.stress_size is not None:
        args.max_bytes = args.stress_size

    args.workspace_root = os.path.abspath(args.workspace_root)

    fuzzer = LspFuzzer(args)
    fuzzer.run()


if __name__ == "__main__":
    main()

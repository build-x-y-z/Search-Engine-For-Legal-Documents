from __future__ import annotations

import subprocess
from pathlib import Path

from flask import Flask, render_template, request


BASE_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = BASE_DIR.parent

app = Flask(__name__)


def find_executable() -> Path | None:
    candidates = [
        PROJECT_ROOT / "search_engine.exe",
        PROJECT_ROOT / "search_engine",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def extract_results(raw_output: str) -> str:
    lines = [line.rstrip() for line in raw_output.splitlines()]
    cleaned: list[str] = []

    for line in lines:
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("Enter query:"):
            remainder = stripped[len("Enter query:") :].strip()
            if remainder:
                cleaned.append(remainder)
            continue
        cleaned.append(stripped)

    for marker in ("Results:", "Suggestions:", "No results"):
        if marker in cleaned:
            index = len(cleaned) - 1 - cleaned[::-1].index(marker)
            block = cleaned[index:]
            return "\n".join(block)

    return "\n".join(cleaned)


@app.route("/", methods=["GET", "POST"])
def index():
    query = ""
    results = None
    error = None

    if request.method == "POST":
        query = request.form.get("query", "").strip()

        if query:
            executable = find_executable()
            if not executable:
                error = (
                    "Search engine executable not found. Build `search_engine` "
                    "or `search_engine.exe` in the project root first."
                )
            else:
                try:
                    completed = subprocess.run(
                        [str(executable)],
                        input=f"{query}\nquit\n",
                        text=True,
                        capture_output=True,
                        cwd=PROJECT_ROOT,
                        timeout=30,
                        check=False,
                    )
                    results = extract_results(completed.stdout)
                    if completed.returncode != 0 and completed.stderr.strip():
                        error = completed.stderr.strip()
                except subprocess.TimeoutExpired:
                    error = "The search engine timed out while processing the query."
                except OSError as exc:
                    error = f"Failed to run the search engine: {exc}"

    return render_template(
        "index.html",
        query=query,
        results=results,
        error=error,
    )


if __name__ == "__main__":
    app.run(debug=True)

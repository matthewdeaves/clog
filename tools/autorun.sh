#!/bin/bash
#
# autorun.sh — Unattended speckit.implement loop for Claude Code
#
# Runs Claude Code in a loop, executing tasks from the active feature's
# tasks.md. Designed for overnight or unattended implementation runs.
#
# Usage:
#   ./tools/autorun.sh              # Run from project root
#   ./tools/autorun.sh --dry-run    # Show what would happen without running Claude
#   ./tools/autorun.sh --resume     # Skip build check, resume after manual fix
#   ./tools/autorun.sh --notify     # Send desktop notification on completion
#

set -euo pipefail

# Allow running from within a Claude Code session
unset CLAUDECODE 2>/dev/null || true

# Detach from terminal stdin to prevent SIGTTIN when backgrounded.
if [ -t 0 ]; then
    exec < /dev/null
fi

# --- Configuration ---
MAX_ITERATIONS=50
TASKS_FILE="specs/001-clog-library/tasks.md"
COOLDOWN_SECONDS=15
RATE_LIMIT_WAIT_INITIAL=300  # 5 minutes first backoff
RATE_LIMIT_WAIT_MAX=3600     # 1 hour max backoff
STUCK_THRESHOLD=3            # Stop after N iterations with no progress
BUILD_CHECK_AFTER=true       # Verify build compiles after each iteration
ITERATION_TIMEOUT=1800       # 30 minutes per iteration (clog tasks are small)
DRY_RUN=false
RESUME=false
NOTIFY=false

# --- Parse arguments ---
for arg in "$@"; do
    case "$arg" in
        --dry-run)  DRY_RUN=true ;;
        --resume)   RESUME=true ;;
        --notify)   NOTIFY=true ;;
        --help|-h)
            echo "Usage: $0 [--dry-run] [--resume] [--notify]"
            echo "  --dry-run   Show status without running Claude"
            echo "  --resume    Skip initial build check (after manual fix)"
            echo "  --notify    Send desktop notification on completion (Linux)"
            exit 0
            ;;
        *)
            echo "Unknown argument: $arg"
            exit 1
            ;;
    esac
done

# --- Ensure we're in project root ---
if [ ! -f "$TASKS_FILE" ]; then
    echo "ERROR: $TASKS_FILE not found. Run from project root."
    exit 1
fi

PROJECT_ROOT="$(pwd)"

# --- Setup logging ---
TIMESTAMP="$(date +%Y%m%d-%H%M%S)"
LOG_DIR="logs/autorun-${TIMESTAMP}"
mkdir -p "$LOG_DIR"
SUMMARY_LOG="$LOG_DIR/summary.log"

log() {
    local msg="[$(date '+%Y-%m-%d %H:%M:%S')] $*"
    echo "$msg"
    echo "$msg" >> "$SUMMARY_LOG"
}

# --- Task counting helpers ---
count_done() {
    local n
    n=$(grep -c '^\- \[x\]' "$TASKS_FILE" 2>/dev/null) || true
    echo "${n:-0}"
}

count_remaining() {
    local n
    n=$(grep -c '^\- \[ \]' "$TASKS_FILE" 2>/dev/null) || true
    echo "${n:-0}"
}

current_phase() {
    grep -B15 '^\- \[ \]' "$TASKS_FILE" | grep '^## Phase' | head -1 || echo "Unknown"
}

next_incomplete_task() {
    grep '^\- \[ \]' "$TASKS_FILE" | head -1 | sed 's/- \[ \] //'
}

# --- Build verification ---
check_posix_build() {
    log "Verifying POSIX build..."
    mkdir -p build
    if (cd build && cmake .. 2>&1 && make 2>&1) > "$LOG_DIR/build-check-latest.log" 2>&1; then
        log "POSIX build OK"
        return 0
    else
        log "POSIX BUILD BROKEN — see $LOG_DIR/build-check-latest.log"
        return 1
    fi
}

# --- Cross-build verification ---
check_all_builds() {
    local label="$1"
    local any_failed=false

    # Always check POSIX
    log "  Checking POSIX build..."
    mkdir -p build
    if ! (cd build && cmake .. 2>&1 && make 2>&1) > "$LOG_DIR/build-${label}-posix.log" 2>&1; then
        log "  WARNING: POSIX build failed — see $LOG_DIR/build-${label}-posix.log"
        any_failed=true
    else
        log "  POSIX build OK"
    fi

    # Check 68k cross-build if directory exists
    if [ -d "build-68k" ]; then
        log "  Checking build-68k..."
        if ! (cd build-68k && make 2>&1) > "$LOG_DIR/build-${label}-68k.log" 2>&1; then
            log "  WARNING: 68k build failed — see $LOG_DIR/build-${label}-68k.log"
            any_failed=true
        else
            log "  68k build OK"
        fi
    fi

    # Check PPC cross-build if directory exists
    if [ -d "build-ppc" ]; then
        log "  Checking build-ppc..."
        if ! (cd build-ppc && make 2>&1) > "$LOG_DIR/build-${label}-ppc.log" 2>&1; then
            log "  WARNING: PPC build failed — see $LOG_DIR/build-${label}-ppc.log"
            any_failed=true
        else
            log "  PPC build OK"
        fi
    fi

    if [ "$any_failed" = true ]; then
        return 1
    fi
    return 0
}

# --- Notification ---
send_notification() {
    local title="$1"
    local body="$2"

    if [ "$NOTIFY" = true ]; then
        if command -v notify-send >/dev/null 2>&1; then
            notify-send "$title" "$body" 2>/dev/null || true
        fi
    fi
}

update_status_file() {
    local status="$1"
    local status_dir="logs"
    mkdir -p "$status_dir"
    echo "$status" > "$status_dir/autorun-latest-status.txt"
}

# --- Git safety snapshot ---
git_snapshot() {
    local label="$1"
    local snap_file="$LOG_DIR/git-${label}.txt"
    {
        echo "=== git log -1 ==="
        git log --oneline -1 2>/dev/null || true
        echo ""
        echo "=== git diff --stat ==="
        git diff --stat 2>/dev/null || true
        echo ""
        echo "=== git diff ==="
        git diff 2>/dev/null || true
    } > "$snap_file"
    git log --oneline -1 >> "$SUMMARY_LOG" 2>/dev/null || true
}

# --- The prompt sent to Claude each iteration ---
build_prompt() {
    local done_count="$1"
    local remaining="$2"
    local next_task="$3"

    cat <<PROMPT
You are running in unattended automation mode.

## Your job

Run /speckit.implement to execute tasks from specs/001-clog-library/tasks.md. Implement as many tasks as you can in this session, working through them in phase order.

## Current progress

- Tasks completed: ${done_count}
- Tasks remaining: ${remaining}
- Next incomplete task: ${next_task}

## Rules for this session

1. Run /speckit.implement — it will read the task list and work through them in order.
2. Implement tasks phase by phase. Complete all tasks in a phase before moving to the next.
3. After each task, verify the POSIX build compiles (mkdir -p build && cd build && cmake .. && make).
4. Mark completed tasks as done by changing \`- [ ]\` to \`- [x]\` in tasks.md.
5. If the build fails, fix the issue before moving on.
6. Commit after completing each phase (or more frequently if changes are large).
7. Do NOT skip tasks or work out of order.
8. For cross-compile verification tasks, use the Retro68 toolchain files:
   - 68k: cmake .. -DCMAKE_TOOLCHAIN_FILE=~/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
   - PPC: cmake .. -DCMAKE_TOOLCHAIN_FILE=~/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
9. **Feedback-first rule**: When testing reveals unexpected platform behavior (crash, wrong output, or any result that contradicts assumptions in research.md or spec.md), run \`/speckit.feedback\` BEFORE implementing a fix. This creates an R-entry in research.md, updates spec artifacts, and generates remediation tasks. For straightforward tasks where requirements are already documented, implement directly.

## When finished

If ALL tasks in tasks.md are marked \`[x]\`, output the exact string: ALL_TASKS_COMPLETE
Otherwise, implement as far as you can and exit cleanly. The next session will pick up where you left off.
PROMPT
}

# --- Main ---
log "=========================================="
log "Autorun — Starting"
log "Project: $PROJECT_ROOT"
log "Tasks file: $TASKS_FILE"
log "Log dir: $LOG_DIR"
log "Max iterations: $MAX_ITERATIONS"
log "Iteration timeout: ${ITERATION_TIMEOUT}s"
log "=========================================="

DONE_BEFORE="$(count_done)"
REMAINING_BEFORE="$(count_remaining)"
log "Initial state: $DONE_BEFORE done, $REMAINING_BEFORE remaining"
log "Current phase: $(current_phase)"

if [ "$REMAINING_BEFORE" -eq 0 ]; then
    log "All tasks already complete. Nothing to do."
    update_status_file "COMPLETE: All tasks done"
    send_notification "Autorun" "All tasks already complete."
    exit 0
fi

# Dry run — just list all remaining tasks and exit
if [ "$DRY_RUN" = true ]; then
    log ""
    log "Remaining tasks:"
    grep '^\- \[ \]' "$TASKS_FILE" | while IFS= read -r line; do
        log "  $line"
    done
    log ""
    log "Would run up to $MAX_ITERATIONS iterations to complete $REMAINING_BEFORE tasks."
    exit 0
fi

# Initial build check (skip with --resume)
if [ "$RESUME" = false ]; then
    if [ -d "build" ]; then
        log "Running initial build check..."
        if ! check_posix_build; then
            log "FATAL: Build is already broken before starting. Fix manually and use --resume."
            update_status_file "FAILED: Build broken at start"
            send_notification "Autorun FAILED" "Build broken before start."
            exit 1
        fi
    else
        log "No build directory yet — skipping initial build check."
    fi
fi

# Track state across iterations
LAST_DONE_COUNT="$DONE_BEFORE"
STUCK_COUNT=0
RATE_LIMIT_CONSECUTIVE=0
ITERATIONS_USED=0

for i in $(seq 1 "$MAX_ITERATIONS"); do
    ITERATIONS_USED="$i"
    DONE_NOW="$(count_done)"
    REMAINING_NOW="$(count_remaining)"
    NEXT_TASK="$(next_incomplete_task)"

    log "------------------------------------------"
    log "Iteration $i/$MAX_ITERATIONS"
    log "Progress: $DONE_NOW/$((DONE_NOW + REMAINING_NOW)) done, $REMAINING_NOW remaining"
    log "Phase: $(current_phase)"
    log "Next: $NEXT_TASK"

    update_status_file "RUNNING: Iteration $i, $REMAINING_NOW remaining"

    # All done?
    if [ "$REMAINING_NOW" -eq 0 ]; then
        log "ALL TASKS COMPLETE"
        break
    fi

    # Stuck detection
    if [ "$DONE_NOW" -eq "$LAST_DONE_COUNT" ] && [ "$i" -gt 1 ] && [ "$RATE_LIMIT_CONSECUTIVE" -eq 0 ]; then
        STUCK_COUNT=$((STUCK_COUNT + 1))
        log "WARNING: No progress after successful run (stuck count: $STUCK_COUNT/$STUCK_THRESHOLD)"
        if [ "$STUCK_COUNT" -ge "$STUCK_THRESHOLD" ]; then
            log "STUCK: Same task failed $STUCK_THRESHOLD times. Stopping."
            log "Last attempted task: $NEXT_TASK"
            log "Review logs in $LOG_DIR for errors."
            update_status_file "STUCK: $NEXT_TASK failed $STUCK_THRESHOLD times"
            send_notification "Autorun STUCK" "Task failed $STUCK_THRESHOLD times: $NEXT_TASK"
            exit 1
        fi
    elif [ "$DONE_NOW" -gt "$LAST_DONE_COUNT" ]; then
        STUCK_COUNT=0
    fi
    LAST_DONE_COUNT="$DONE_NOW"

    # Snapshot git state before
    git_snapshot "before-iter-$(printf '%03d' "$i")"

    # Build the prompt
    PROMPT="$(build_prompt "$DONE_NOW" "$REMAINING_NOW" "$NEXT_TASK")"

    # Run Claude with per-iteration timeout and line-buffered output
    ITER_LOG="$LOG_DIR/iter-$(printf '%03d' "$i")-$(date +%H%M%S).log"
    log "Running Claude... (log: $ITER_LOG)"

    CLAUDE_EXIT=0
    if command -v stdbuf >/dev/null 2>&1; then
        timeout "$ITERATION_TIMEOUT" stdbuf -oL claude -p "$PROMPT" \
            --dangerously-skip-permissions \
            < /dev/null 2>&1 | tee "$ITER_LOG" || CLAUDE_EXIT=$?
    else
        timeout "$ITERATION_TIMEOUT" claude -p "$PROMPT" \
            --dangerously-skip-permissions \
            < /dev/null 2>&1 | tee "$ITER_LOG" || CLAUDE_EXIT=$?
    fi

    # Check for timeout (exit code 124 from timeout command)
    if [ "$CLAUDE_EXIT" -eq 124 ]; then
        log "TIMEOUT: Iteration $i killed after $((ITERATION_TIMEOUT / 60)) minutes"
        git_snapshot "after-iter-$(printf '%03d' "$i")"
        log "Cooling down ${COOLDOWN_SECONDS}s before next iteration..."
        sleep "$COOLDOWN_SECONDS"
        continue
    fi

    # Snapshot git state after
    git_snapshot "after-iter-$(printf '%03d' "$i")"

    # Check for completion signal
    if grep -q "ALL_TASKS_COMPLETE" "$ITER_LOG"; then
        log "ALL TASKS COMPLETE (reported by Claude)"
        break
    fi

    # Check for rate limiting or errors
    if grep -qiE "rate.limit|usage.limit|capacity|overloaded|529|quota|too many|throttl" "$ITER_LOG" || [ "$CLAUDE_EXIT" -ne 0 ]; then
        RATE_LIMIT_CONSECUTIVE=$((RATE_LIMIT_CONSECUTIVE + 1))

        # Exponential backoff: 5min, 10min, 20min, 40min, capped at 60min
        BACKOFF=$((RATE_LIMIT_WAIT_INITIAL * (2 ** (RATE_LIMIT_CONSECUTIVE - 1))))
        if [ "$BACKOFF" -gt "$RATE_LIMIT_WAIT_MAX" ]; then
            BACKOFF=$RATE_LIMIT_WAIT_MAX
        fi

        BACKOFF_MIN=$((BACKOFF / 60))
        RESUME_TIME="$(date -d "+${BACKOFF} seconds" '+%H:%M:%S' 2>/dev/null || date -v+${BACKOFF}S '+%H:%M:%S' 2>/dev/null || echo "unknown")"
        log "Rate limit or error (exit code: $CLAUDE_EXIT, consecutive: $RATE_LIMIT_CONSECUTIVE)"
        log "Backing off ${BACKOFF_MIN}m (resuming ~${RESUME_TIME})..."
        sleep "$BACKOFF"
    else
        # Success — reset consecutive rate limit counter
        RATE_LIMIT_CONSECUTIVE=0

        # Cross-build verification after successful iteration
        if [ "$BUILD_CHECK_AFTER" = true ]; then
            log "Running cross-build verification..."
            if ! check_all_builds "iter-$(printf '%03d' "$i")"; then
                log "WARNING: Some builds failed after iteration $i."
                log "Continuing — next iteration should fix it."
            fi
        fi

        log "Iteration $i complete. Cooling down ${COOLDOWN_SECONDS}s..."
        sleep "$COOLDOWN_SECONDS"
    fi
done

# --- Final summary ---
DONE_FINAL="$(count_done)"
REMAINING_FINAL="$(count_remaining)"
ELAPSED_TASKS=$((DONE_FINAL - DONE_BEFORE))

log "=========================================="
log "Autorun — Complete"
log "=========================================="
log "Tasks completed this run: $ELAPSED_TASKS"
log "Total done: $DONE_FINAL"
log "Remaining: $REMAINING_FINAL"
log "Iterations used: $ITERATIONS_USED"
log "Logs: $LOG_DIR"

if [ "$REMAINING_FINAL" -eq 0 ]; then
    log "SUCCESS: All tasks implemented."
    log "=========================================="
    update_status_file "SUCCESS: All $DONE_FINAL tasks complete"
    send_notification "Autorun SUCCESS" "All $DONE_FINAL tasks complete in $ITERATIONS_USED iterations."
    exit 0
else
    log "INCOMPLETE: $REMAINING_FINAL tasks remain."
    log "=========================================="
    update_status_file "INCOMPLETE: $REMAINING_FINAL tasks remain ($DONE_FINAL done)"
    send_notification "Autorun INCOMPLETE" "$REMAINING_FINAL tasks remain. $ELAPSED_TASKS completed this run."
    exit 2
fi

#!/bin/bash

# Enable strict error checking across environments
set -u
set -o pipefail

# Global Configurations & Ecosystem Constants
BIN="aquachemd"
REPO="https://api.github.com/repos/AqualinkD/${BIN}"
INSTALLED_BINARY="/usr/local/bin/${BIN}"

# Systemd Journal Message ID (Shared with C code)
SD_MSG_ID="c3b9b418e24440939b4bfae6dfbc1122" 

# Inverse Boolean Setup
TRUE=0
FALSE=1

# Initialize output log destination (overridden by specific logic files)
OUTPUT=""

# Centralized Logger Engine
log_to_journal() {
  local priority="$1"
  shift
  local msg="$*"

  # Map systemd numeric priorities to syslog facilities for the logger fallback
  local syslog_pri="local0.notice"
  if [ "$priority" -eq 3 ]; then
    syslog_pri="local0.err"
  elif [ "$priority" -eq 6 ]; then
    syslog_pri="local0.info"
  fi

  # Output straight to console stdout/stderr
  if [ "$priority" -eq 3 ]; then
    echo "Error: $msg" >&2
  else
    echo "$msg"
  fi

  # Log to systemd journal database if available
  if command -v systemd-journal-send &>/dev/null; then
    systemd-journal-send \
      MESSAGE_ID="$SD_MSG_ID" \
      SYSLOG_IDENTIFIER="${BIN}" \
      PRIORITY="$priority" \
      MESSAGE="$msg"
  else
    logger -p "$syslog_pri" -t ${BIN} "Upgrade/Install: $msg"
  fi

  # Log to file if OUTPUT variable has been populated
  if [[ -n "${OUTPUT:-}" ]]; then
    if [ "$priority" -eq 3 ]; then
      echo "$(date): ERROR: $msg" >> "$OUTPUT" 2>/dev/null || true
    else
      echo "$(date): $msg" >> "$OUTPUT" 2>/dev/null || true
    fi
  fi
}

log() {
  log_to_journal 5 "$*"
}

logerr() {
  log_to_journal 3 "$*"
}

check_tool() {
  local cmd=$1
  if ! command -v "$cmd" &>/dev/null; then
    log "Command '$cmd' could not be found!"
    return "$FALSE"
  fi
  return "$TRUE"
}

check_root_privileges() {
  if [[ $EUID -ne 0 ]]; then
     logerr "This script must be run as root" 
     exit 1
  fi
}
# ==============================================================================
# LOCAL ASSET PLACEMENT & ENVIRONMENT SPECIFIC LOGIC
# ==============================================================================
CONTEXT="LOCAL"

BUILD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PARENT_COMMAND=$(ps -o comm= $PPID 2>/dev/null) 

SERVICE="aquachemd"
CFG="${BIN}.conf"
SRV="${BIN}.service"
DEF="${BIN}"
MDNS="${BIN}.service"

BINLocation="/usr/local/bin"
CFGLocation="/etc"
SRVLocation="/etc/systemd/system"
DEFLocation="/etc/default"
WEBLocation="/var/www/${BIN}"
MDNSLocation="/etc/avahi/services"

LOG_SYSTEMD=1   # 1=false, 0=true

_frommake=$FALSE
_ignorearch=$FALSE
_nosystemd=$FALSE

printHelp() {
  echo "Usage: $0 [options]"
  echo "  clean | delete        Remove the installation"
  echo "  from-make             Explicitly use the compiled custom build"
  echo "  ignorearch            Skip architecture checking"
  echo "  --arch <arch>         Force a specific architecture (armhf | arm64)"
  echo "  --logfile <file>      Log outputs to a file"
}

clean() {
  log "Deleting installation..."
  systemctl disable $SERVICE > /dev/null 2>&1
  rm -f "$BINLocation/$BIN" "$SRVLocation/$SRV" "$CFGLocation/$CFG" "$DEFLocation/$DEF" "/etc/cron.d/aquachemd"
  rm -rf "$WEBLocation"
  systemctl daemon-reload
  exit 0
}

# Parse Entry Parameters
while [[ $# -gt 0 ]]; do
  case "$1" in
    --logfile)  shift; OUTPUT="$1" ;;
    --arch)
      shift
      if [[ "$1" == "arm64" || "$1" == "armhf" ]]; then
        _ignorearch=$TRUE
        TARGET_BIN="$BUILD/$1/$BIN"
      else
        log "Error: --arch requires armhf or arm64"
        exit 1
      fi
      ;;
    from-make)  _frommake=$TRUE ;;
    ignorearch) _ignorearch=$TRUE ;;
    nosystemd)  _nosystemd=$TRUE ;;
    help|-help|--help|-h) printHelp; exit 0 ;;
    clean|delete) clean ;;
    *) echo "Unknown argument: $1"; printHelp; exit 1 ;;
  esac
  shift
done

if ! tty > /dev/null 2>&1 || [ "${1:-}" = "syslog" ]; then
  LOG_SYSTEMD=0
fi

check_root_privileges

# Architecture Tracking and Source Mapping Decision Engine
if [ -f "$BUILD/$BIN" ] && { [ "$PARENT_COMMAND" = "make" ] || [ "$_frommake" -eq $TRUE ] || [ "$_ignorearch" -eq $TRUE ]; }; then
  TARGET_BIN="$BUILD/$BIN"
  log "Using custom-built binary at $TARGET_BIN"
else
  if [ -f "$BUILD/$BIN" ] ; then
    TARGET_BIN="$BUILD/$BIN"
    log "Found and prioritizing custom binary at $TARGET_BIN"
  else
    command -v dpkg >/dev/null 2>&1 || { log "Error: 'dpkg' missing. Can't auto-detect architecture."; exit 1; }
    ARCH=$(dpkg --print-architecture)
    
    if [ -f "$BUILD/$ARCH/$BIN" ]; then
      TARGET_BIN="$BUILD/$ARCH/$BIN"
      log "Detected architecture $ARCH. Using pre-built binary: $TARGET_BIN"
    else
      log "Error: Cannot find an appropriate binary for architecture '$ARCH' in $BUILD"
      exit 1
    fi
  fi
fi

# Validation Matrix (Systemd and Active Cron Schedulers)
command -v systemctl >/dev/null 2>&1 || { log "Error: systemctl missing. Systemd required." >&2; exit 1; }

SERVICE_EXISTS=1
if [ "$_nosystemd" -eq $FALSE ]; then
  systemctl stop $SERVICE > /dev/null 2>&1
  SERVICE_EXISTS=$?
fi

if systemctl is-active --quiet cron.service; then
  if [ ! -d "/etc/cron.d" ]; then log "Warning: /etc/cron.d does not exist. Scheduler may fail."; fi
else
 log "Warning: Cron service is inactive. Scheduler will not work."
fi

# File deployment Execution Steps
cp "$TARGET_BIN" "$BINLocation/$BIN"
cp "$BUILD/$SRV" "$SRVLocation/$SRV"

if [ -f "$CFGLocation/$CFG" ]; then log "Config exists, skipping asset copy: $CFGLocation/$CFG"; else cp "$BUILD/$CFG" "$CFGLocation/$CFG"; fi

if [ -f "$MDNSLocation/$MDNS" ]; then
  log "mDNS template exists, skipping asset copy: $MDNSLocation/$MDNS"
else
  if [ -d "$MDNSLocation" ] && [ -f "$BUILD/$MDNS.avahi" ]; then 
    cp "$BUILD/$MDNS.avahi" "$MDNSLocation/$MDNS"
  fi
fi

# Web Assets Optimization Loop
mkdir -p "$WEBLocation"
if [ -f "$WEBLocation/config.json" ]; then
  log "Web config exists. Merging newer UI files safely..."
  if command -v rsync &>/dev/null; then
    rsync -avq --exclude='config.json' "$BUILD/../web/" "$WEBLocation/"
  else
    find "$BUILD/../web" -maxdepth 1 ! -name 'config.json' ! -name 'web' -exec cp -R {} "$WEBLocation/" \;
  fi
else
  cp -r "$BUILD/../web/"* "$WEBLocation/"
fi

if [ "$_nosystemd" -eq $TRUE ]; then exit 0; fi

systemctl enable $SERVICE
systemctl daemon-reload

if [ $SERVICE_EXISTS -eq 0 ]; then
  log "Starting daemon $SERVICE"
  systemctl start $SERVICE
else
  log "Please edit $CFGLocation/$CFG, then run: sudo systemctl start $SERVICE"
fi

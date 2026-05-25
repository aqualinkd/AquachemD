#!/bin/bash

# Enable strict mode
set -u          # Treat unset variables as an error and exit immediately
set -o pipefail # If a command in a pipeline fails, the whole pipeline fails

#
# run from curl or local will give different results.
#  curl -fsSL https://raw.githubusercontent.com/aquachemd/AquachemD/master/release/remote_install.sh | sudo bash -s -- latest
#  ./upgrade.sh latest
#
# To get good / bad exit code from both curl and bash, use below. It will exit current term so be careful.
# curl -fsSL "https://raw.githubusercontent.com/aquachemd/AquachemD/master/release/remote_install.sh" | ( sudo bash && exit 0 ) || exit $?

BIN="aquachemd"

REQUIRED_SPACE_MB=18 # For complete tarball need 17MB, use 18 ( will get reset to 2MB if using new install and release tar6)

TRUE=0
FALSE=1

REPO="https://api.github.com/repos/AqualinkD/${BIN}"
#REPO="https://api.github.com/repos/sfeakes/AquachemD"

INSTALLED_BINARY="/usr/local/bin/${BIN}"

# Can't use $0 since this script is usually piped into bash
SELF="remote_install.sh"

REL_VERSION=""
DEV_VERSION=""
INSTALLED_VERSION=""

TEMP_INSTALL="/tmp/${BIN}"
OUTPUT="/var/log/${BIN}_upgrade.log"

UNTAR_CMD="tar xz --strip-components=1 --directory=$TEMP_INSTALL"

FROM_CURL=$FALSE
SYSTEMD_LOG=$FALSE

USE_RELEASE_PKG=$TRUE

# Initialize global variable for trap cleanup
temp_file=""

if [ "$USE_RELEASE_PKG" -eq $TRUE ]; then
  REQUIRED_SPACE_MB=2
  UNTAR_CMD="tar xz --directory=$TEMP_INSTALL"
fi

# We can get called from no path, so find external commands
if command -v "systemd-cat" &>/dev/null; then
  SYSTEMD_LOG=$TRUE
fi

# Guaranteed cleanup on exit, regardless of how the script terminates (success, error, or Ctrl+C)
cleanup_on_exit() {
  rm -rf "$TEMP_INSTALL"
  if [[ -n "$temp_file" && -f "$temp_file" ]]; then
    rm -f "$temp_file"
  fi
}
trap cleanup_on_exit EXIT

# Define shared message ID (Keep it identical to what the C code looks for)
SD_MSG_ID="c3b9b418e24440939b4bfae6dfbc1122" 

# Centralized Logger Engine
log_to_journal() {
  local priority="$1"
  shift
  local msg="$*"

  # Map systemd numeric priorities to syslog facilities for the logger fallback
  # 3 = error, 5 = notice, 6 = info
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
    # Fallback to standard legacy syslog tool
    logger -p "$syslog_pri" -t ${BIN} "Upgrade: $msg"
  fi

  # Always append to the local persistent backup file
  # Checking if OUTPUT is set and writable before writing
  if [[ -n "${OUTPUT:-}" ]]; then
    if [ "$priority" -eq 3 ]; then
      echo "$(date): ERROR: $msg" >> "$OUTPUT" 2>/dev/null || true
    else
      echo "$(date): $msg" >> "$OUTPUT" 2>/dev/null || true
    fi
  fi
}

# Standard info / notice logging (Priority 5 or 6)
log() {
  log_to_journal 5 "$*"
}

# Error logging (Priority 3)
logerr() {
  log_to_journal 3 "$*"
}

function check_tool() {
  cmd=$1
  if ! command -v "$cmd" &>/dev/null
  then
    log "Command '$cmd' could not be found!"
    return "$FALSE"
  fi

  return "$TRUE"
}

function latest_release_version {
  REL_VERSION=$(curl -fsSL "$REPO/releases/latest" | grep -Po '"tag_name": "\K.*?(?=")')
  if [[ "$REL_VERSION" != "" ]]; then
    return "$TRUE"
  else
    return "$FALSE"
  fi
}

function latest_development_version {
  DEV_VERSION=$(curl -fsSL -H "Accept: application/vnd.github.raw" "$REPO/contents/source/version.h" | grep AQUALINKD_VERSION | cut -d '"' -f 2)
  if [[ "$DEV_VERSION" != "" ]]; then
    return "$TRUE"
  else
    return "$FALSE"
  fi
}

function installed_version {
  if [ -f "$INSTALLED_BINARY" ]; then
    if check_tool strings &&
       check_tool grep &&
       check_tool awk &&
       check_tool tr; then
      INSTALLED_VERSION=$(strings "$INSTALLED_BINARY" | grep sw_version | awk -v RS="," -v FS=":" '/sw_version/{print $2;exit;}' | tr -d ' "' )
    fi
  else
    log "AquachemD is not installed"
  fi

  if [[ "$INSTALLED_VERSION" != "" ]]; then
    return "$TRUE"
  else
    return "$FALSE"
  fi
}

function check_system_arch {
  ARCH=$(dpkg --print-architecture)

  case $ARCH in 
    arm64 |\
    armhf)
      return "$TRUE"
    ;;
    *)
      logerr "System arch is $ARCH, this is not supported by AquachemD"
      return "$FALSE";
    ;;
  esac
}

function check_can_upgrade {
  output=""
  # Check we have needed commands.
  if ! command -v curl &>/dev/null; then output+="Command 'curl' not found, please check it's installed and in path\n"; fi
  if ! command -v dpkg &>/dev/null; then output+="Command 'dpkg' not found, please check it's installed and in path\n"; fi
  if ! command -v systemctl &>/dev/null; then output+="Command 'systemctl' not found, please check it's installed and in path\n"; fi
  
  # Check root is rw
  if mount | grep " / " | grep -q "(ro,"; then
    if mount / -o remount,rw &>/dev/null; then
      mount / -o remount,ro &>/dev/null
    else
      output+="Root filesystem is readonly & failed to remount read write, can't upgrade";
    fi
  fi

  # Check we can get the latest version
  if ! latest_release_version; then output+="Couldn't find latest version on github"; fi

  if command -v dpkg &>/dev/null; then
    if ! check_system_arch; then output+="System Architecture not supported!"; fi
  fi

  # Check free diskspace
  mkdir -p "$TEMP_INSTALL"
  free_space_mb=$(df -mP "$TEMP_INSTALL" 2>/dev/null | awk 'NR==2{print $4}' )

  if [ -z "$free_space_mb" ] || ! [[ "$free_space_mb" =~ ^[0-9]+$ ]]; then
    output+="Could not retrieve free space for directory: $TEMP_INSTALL"
  else
    if [ "$free_space_mb" -lt "$REQUIRED_SPACE_MB" ]; then
      output+="Not enough disk space on directory: $TEMP_INSTALL! (Required $REQUIRED_SPACE_MB MB)"
    fi
  fi

  if [[ "$output" == "" ]] && [[ "$REL_VERSION" != "" ]]; then
    return "$TRUE"
  elif [[ "$output" != "" ]]; then
    logerr "$output";
    return "$FALSE"
  fi

  return "$TRUE"
}

function download_latest_release {
  mkdir -p "$TEMP_INSTALL"
  
  if [ "$USE_RELEASE_PKG" -eq $TRUE ]; then
    tar_url=$(curl -fsSL "$REPO/releases/latest" | grep -Po '"browser_download_url": "\K.*?(?=")' | grep ${BIN}-release.tar.gz)
  else
    tar_url=$(curl -fsSL "$REPO/releases/latest" | grep -Po '"tarball_url": "\K.*?(?=")')
  fi

  if [[ "$tar_url" == "" ]]; then return "$FALSE"; fi

  curl -fsSL "$tar_url" | $UNTAR_CMD
  if [ $? -ne 0 ]; then return "$FALSE"; fi

  return "$TRUE";
}

function download_latest_development {
  mkdir -p "$TEMP_INSTALL"
  tar_url="$REPO/tarball/master"
  curl -fsSL "$tar_url" | tar xz --strip-components=1 --directory="$TEMP_INSTALL"
  if [ $? -ne 0 ]; then return "$FALSE"; fi

  return "$TRUE";
}

function download_version {
  if [ "$USE_RELEASE_PKG" -eq $TRUE ]; then
    tar_url=$(curl -fsSL "$REPO/releases" | awk 'match($0,/.*"browser_download_url": "(.*\/'"${BIN}"'-release\.tar\.gz)".*/)' | grep "$1/" | awk -F '"' '{print $4}' )
  else
    tar_url=$(curl -fsSL "$REPO/releases" | awk 'match($0,/.*"tarball_url": "(.*\/tarball\/.*)".*/)' | grep "$1\"" | awk -F '"' '{print $4}')
  fi

  if [[ ! -n "$tar_url" ]]; then
    return "$FALSE"
  fi

  mkdir -p "$TEMP_INSTALL"

  curl -fsSL "$tar_url" | $UNTAR_CMD

  if [ $? -ne 0 ]; then return "$FALSE"; fi

  return "$TRUE";
}

function get_all_versions {
  if [ "$USE_RELEASE_PKG" -eq $TRUE ]; then
    curl -fsSL "$REPO/releases" | awk 'match($0,/.*"browser_download_url": "(.*\/'"${BIN}"'-release\.tar\.gz)".*/)' | awk -F '/' '{split($(NF-1), a, "\""); print a[1]}'
  else
    curl -fsSL "$REPO/releases" | awk 'match($0,/.*"tarball_url": "(.*\/tarball\/.*)".*/)' | awk -F '/' '{split($NF,a,"\""); print a[1]}'
  fi
}

function run_install_script {
  if [ ! -f "$TEMP_INSTALL/release/install.sh" ]; then
    logerr "Can not find install script $TEMP_INSTALL/release/install.sh"
    return "$FALSE"
  fi

  log "Installing AquachemD $1"

  # Can't run in background as it'll cleanup / delete files before install.
  temp_file=$(mktemp)
  nohup "$TEMP_INSTALL/release/install.sh" "--logfile" "$temp_file" >> "$OUTPUT" 2>&1
  cat "$temp_file"
  rm -f "$temp_file"
  temp_file="" # Clear the variable so the trap doesn't try to delete it again
}

function remove_install {
  curl -fsSL -H "Accept: application/vnd.github.raw" "$REPO/contents/release/install.sh" | sudo bash -s -- clean
}

####################################################
#
#  Main Wrapper
#  

main() {
  # See if we are called from curl or local dir.
  if ! tty > /dev/null 2>&1; then
    script=$(basename "${0:-}")
    if [ "$script" == "bash" ] || [ "$script" == "" ]; then 
      FROM_CURL=$TRUE
    fi
  fi

  if [[ $EUID -ne 0 ]]; then
     logerr "This script must be run as root" 
     exit 1
  fi

  echo "$SELF - $(date) " 2>/dev/null > "$OUTPUT"

  if check_can_upgrade; then
    installed_version
    if [[ "$INSTALLED_VERSION" != "" ]]; then
      log "Current AquachemD installation $INSTALLED_VERSION"
      log "System OK to install AquachemD latest release $REL_VERSION"
    else
      log "System OK to install AquachemD $REL_VERSION"
    fi
  else
    logerr "Can not upgrade, Please fix error(s)!"
    exit $FALSE;
  fi

  # Default to "latest" safely to prevent set -u from throwing an error if no arg is passed
  case ${1:-latest} in
    check|checkupgradable)
      exit $TRUE
    ;;
    development)
      if ! latest_development_version; then logerr "getting development version"; exit "$FALSE"; fi
      if ! download_latest_development; then logerr "downloading latest development"; exit "$FALSE"; fi
      run_install_script "$DEV_VERSION"
    ;;
    clean|delete|remove)
      if ! remove_install; then logerr "Removing install"; exit "$FALSE"; fi
      log "Removed install"
    ;;
    list|versions)
      get_all_versions
    ;;
    v*)
      if ! download_version "$1"; then logerr "downloading version $1"; exit "$FALSE"; fi
      run_install_script "$1"
    ;;
    -h|help|h)
      echo "AquachemD Installation script"
      echo "$SELF               <- download and install latest AquachemD version"
      echo "$SELF latest        <- download and install latest AquachemD version"
      if [ "$USE_RELEASE_PKG" -eq $FALSE ]; then
        echo "$SELF development   <- download and install latest AquachemD development version"
      fi
      echo "$SELF clean         <- Remove AquachemD"
      echo "$SELF list          <- List available versions to install"
      echo "$SELF v1.0.0        <- install AquachemD v1.0.0 (use list option to see available versions)"
    ;;
    latest|*)
      if ! download_latest_release; then logerr "downloading latest"; exit "$FALSE"; fi
      run_install_script "$REL_VERSION"
    ;;
  esac
}

# The single execution trigger that forces Bash to parse the entire file before running
main "$@"


# List all versions
# curl -fsSL https://api.github.com/repos/sfeakes/aquachemd/releases | awk 'match($0,/.*"html_url": "(.*\/releases\/tag\/.*)".*/)'
# curl -fsSL "https://api.github.com/repos/sfeakes/AquachemD/releases" | awk 'match($0,/.*"tarball_url": "(.*\/tarball\/.*)".*/)' | awk -F '"' '{print $4}'
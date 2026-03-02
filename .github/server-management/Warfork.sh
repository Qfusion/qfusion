#!/bin/bash

# Combined Environment Setup and Server Management Script
# Run with sudo for initial setup, then use server functions

set -e

# Set default values for environment variables
DEBUG="${DEBUG:-false}"
VALIDATE_SERVER_FILES="${VALIDATE_SERVER_FILES:-false}"
WF_CUSTOM_CONFIGS_DIR="${WF_CUSTOM_CONFIGS_DIR:-/var/wf}"
WF_PARAMS="${WF_PARAMS:-+set dedicated 1 +set net_port 44400}"
STEAM_BRANCH="${STEAM_BRANCH:-public}"

# Server configuration
steam_dir="$HOME/Steam"
server_dir="$HOME/server"
server_installed_lock_file="$server_dir/installed.lock"
wf_dir="$server_dir/basewf"
wf_custom_configs_dir="$WF_CUSTOM_CONFIGS_DIR"


provision() {
    echo "Starting environment setup..."

    # Check if running as root for system setup
    if [[ $EUID -ne 0 ]]; then
       echo "Environment setup needs to be run with sudo for system package installation"
       echo "Usage: sudo $0 setup"
       exit 1
    fi

    # Update package lists
    echo "Updating package lists..."
    apt-get update

    # Install required packages
    echo "Installing required packages..."
    apt-get install -y --no-install-recommends \
        lib32gcc-s1 \
        lib32stdc++6 \
        wget \
        ca-certificates \
        rsync \
        unzip \
        tmux \
        jq \
        bc \
        binutils \
        util-linux \
        python3 \
        curl \
        file \
        tar \
        bzip2 \
        gzip \
        bsdmainutils \
        libcurl4 \
        libcurl3-gnutls \
        locales

    useradd wf || echo "wf User already exists."
    sudo -u wf mkdir -p /var/wf
}

setup_environment() {
    echo "Starting environment setup..."

    # Configure locales
    echo "Configuring locales..."
    sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen
    dpkg-reconfigure --frontend=noninteractive locales

    # Clean up package cache
    echo "Cleaning up package cache..."
    rm -rf /var/lib/apt/lists/*

    mkdir $HOME/wf

    # Create Steam directories
    echo "Creating Steam directories..."
    mkdir -p $HOME/Steam
    mkdir -p $HOME/server
    mkdir -p $HOME/.steam

    echo "Downloading and installing SteamCMD..."
    cd $HOME/Steam
    wget -qO- https://steamcdn-a.akamaihd.net/client/installer/steamcmd_linux.tar.gz | tar zxf -

    echo "Running SteamCMD initial setup..."
    $HOME/Steam/steamcmd.sh +quit

    echo "Creating Steam SDK symlinks..."
    ln -sf $HOME/Steam/linux64 $HOME/.steam/sdk64
    ln -sf $HOME/Steam/linux32 $HOME/.steam/sdk32

    echo "Creating server directories..."
    mkdir -p /var/wf/{maps,progs/gametypes,configs}
    touch /var/wf/motd.txt

    echo "Environment setup completed successfully!"
    echo ""
    echo "Steam and server directories created in $HOME/"
    echo "SteamCMD is ready to use at $HOME/Steam/steamcmd.sh"
    echo ""
    echo "Usage:"
    echo "$0 install          # Install server"
    echo "$0 update           # Update server"
    echo "$0 start            # Start server"
    echo "$0 run              # Install/update and start server"
    echo ""
    echo "Environment variables:"
    echo "- DEBUG=true                    # Enable verbose output"
    echo "- VALIDATE_SERVER_FILES=true    # Validate files on updates"
    echo "- WF_CUSTOM_CONFIGS_DIR=/path   # Custom configs directory (default: /var/wf)"
    echo "- WF_PARAMS='--param value'     # Additional server parameters"
    echo "- STEAM_BRANCH=beta             # Steam branch (default: public)"
}

get_app_update_args() {
    if [ "${STEAM_BRANCH}" = "beta" ]; then
        echo "1136510 -beta beta"
    else
        echo "1136510"
    fi
}

install() {
    echo '> Installing server ...'

    if [ "${DEBUG}" = "true" ]; then
        set -x
    fi

    local app_args
    app_args=$(get_app_update_args)

    $steam_dir/steamcmd.sh \
        +force_install_dir $server_dir \
        +login anonymous \
        +app_update $app_args validate \
        +quit

    if [ "${DEBUG}" = "true" ]; then
        set +x
    fi

    echo '> Done'
    touch $server_installed_lock_file
}

sync_custom_files() {
    echo "> Checking for custom files at \"$wf_custom_configs_dir\" ..."

    if [ -d "$wf_custom_configs_dir" ]; then
        echo "> Found custom files. Syncing with \"${wf_dir}\" ..."

        if [ "${DEBUG}" = "true" ]; then
            set -x
        fi

        cp -asf "$wf_custom_configs_dir"/* "$wf_dir" # Copy custom files as soft links
        find "$wf_dir" -xtype l -delete              # Find and delete broken soft links

        if [ "${DEBUG}" = "true" ]; then
            set +x
        fi

        echo '> Done'
    else
        echo '> No custom files found'
    fi
}

get_session_name() {
    if [ -n "${WF_SESSION_NAME:-}" ]; then
        echo "$WF_SESSION_NAME"
        return
    fi
    local port
    port=$(echo "$WF_PARAMS" | sed -n 's/.*net_port \([0-9]*\).*/\1/p')
    echo "wf-${port:-44400}"
}

start() {
    echo '> Starting server ...'

    local session_name
    session_name=$(get_session_name)
    local log_file="$server_dir/${session_name}.log"
    local run_script="$server_dir/${session_name}.sh"

    if tmux has-session -t "$session_name" 2>/dev/null; then
        echo "> Session '$session_name' is already running. Use restart to restart it."
        exit 1
    fi

    # Write a launcher script so WF_PARAMS is never subject to tmux shell re-quoting
    printf '#!/bin/bash\ncd %q\nexec ./wf_server.x86_64 %s 2>&1 | tee -a %q\n' \
        "$wf_dir/.." "$WF_PARAMS" "$log_file" > "$run_script"
    chmod +x "$run_script"

    if [ "${DEBUG}" = "true" ]; then
        set -x
    fi

    tmux new-session -d -s "$session_name" "$run_script"

    echo "> Server started in tmux session '$session_name'"
    echo "> Log: $log_file"
    echo "> Attach: tmux attach -t $session_name"
}

update() {
    if [ "${VALIDATE_SERVER_FILES-"false"}" = "true" ]; then
        echo '> Validating server files and checking for server update ...'
    else
        echo '> Checking for server update ...'
    fi

    local app_args
    app_args=$(get_app_update_args)

    if [ "${DEBUG}" = "true" ]; then
        set -x
    fi

    if [ "${VALIDATE_SERVER_FILES-"false"}" = "true" ]; then
        $steam_dir/steamcmd.sh \
            +force_install_dir $server_dir \
            +login anonymous \
            +app_update $app_args validate \
            +quit
    else
        $steam_dir/steamcmd.sh \
            +force_install_dir $server_dir \
            +login anonymous \
            +app_update $app_args \
            +quit
    fi

    if [ "${DEBUG}" = "true" ]; then
        set +x
    fi

    echo '> Done'
}

install_or_update() {
    if [ -f "$server_installed_lock_file" ]; then
        update
    else
        install
    fi
}

run_server() {
    # Check if environment is set up, if not, set it up first
    if [ ! -f "$steam_dir/steamcmd.sh" ]; then
        echo "SteamCMD not found. Setting up environment first..."
        setup_environment
        echo "Environment setup complete. Continuing with server setup..."
    fi

    if [ "${DEBUG}" = "true" ]; then
        set -x
    fi

    install_or_update
    sync_custom_files
    start
}

stop() {
    echo '> Stopping server ...'

    local session_name
    session_name=$(get_session_name)

    if tmux has-session -t "$session_name" 2>/dev/null; then
        tmux kill-session -t "$session_name"
        echo '> Done'
    else
        echo "> No session '$session_name' found"
    fi
}

status() {
    local session_name
    session_name=$(get_session_name)
    if tmux has-session -t "$session_name" 2>/dev/null; then
        echo "> Session '$session_name' is running"
        return 0
    else
        echo "> Session '$session_name' is not running"
        return 1
    fi
}

restart() {
    stop || true
    sleep 2
    start
}

# Main execution logic
case "${1:-}" in
    "provision")
        provision
        ;;
    "install")
        install
        ;;
    "update")
        update
        ;;
    "start")
        start
        ;;
    "restart")
        restart
        ;;
    "status")
        status
        ;;
    "stop")
        stop
        ;;
    "run"|"")
        # Check if we need sudo for setup
        if [ ! -f "$steam_dir/steamcmd.sh" ] && [[ $EUID -ne 0 ]]; then
            echo "Environment not set up and not running as root."
            echo "Please run: sudo $0 run"
            exit 1
        fi
        run_server
        ;;
    *)
        echo "Usage: $0 {setup|install|update|start|restart|stop|status|run}"
        echo ""
        echo "Commands:"
        echo "  setup     - Install system packages and set up environment (requires sudo)"
        echo "  install   - Install game server"
        echo "  update    - Update game server"
        echo "  start     - Start game server (detached tmux session)"
        echo "  restart   - Restart game server"
        echo "  stop      - Stop game server"
        echo "  status    - Exit 0 if session running, 1 if not"
        echo "  run       - Install/update and start server (default)"
        echo ""
        echo "Environment variables:"
        echo "  DEBUG=true                    # Enable verbose output"
        echo "  VALIDATE_SERVER_FILES=true    # Validate files on updates"
        echo "  WF_CUSTOM_CONFIGS_DIR=/path   # Custom configs directory (default: /var/wf)"
        echo "  WF_PARAMS='--param value'     # Additional server parameters"
        echo "  STEAM_BRANCH=beta             # Steam branch (default: public)"
        echo "  WF_SESSION_NAME=wf-custom     # Override tmux session name"
        exit 1
        ;;
esac

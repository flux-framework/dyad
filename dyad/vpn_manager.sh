#!/bin/bash

# Comprehensive VPN Management Script

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if VPN is connected
is_vpn_connected() {
    if sudo wg show protonvpn &>/dev/null; then
        return 0
    else
        return 1
    fi
}

# Function to get current IP
get_current_ip() {
    curl -s --max-time 10 ifconfig.me
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to install dependencies
install_dependencies() {
    print_status "Checking and installing dependencies..."
    
    # Update package list
    sudo apt update >/dev/null 2>&1
    
    # Install required packages
    local packages=("wireguard" "curl" "iproute2")
    local missing_packages=()
    
    for package in "${packages[@]}"; do
        if ! dpkg -l | grep -q "^ii  $package "; then
            missing_packages+=("$package")
        fi
    done
    
    if [ ${#missing_packages[@]} -gt 0 ]; then
        print_status "Installing missing packages: ${missing_packages[*]}"
        sudo apt install -y "${missing_packages[@]}" >/dev/null 2>&1
        if [ $? -eq 0 ]; then
            print_success "Dependencies installed successfully"
        else
            print_error "Failed to install dependencies"
            exit 1
        fi
    else
        print_success "All dependencies are already installed"
    fi
}

# Function to connect to VPN
connect_vpn() {
    print_status "Connecting to ProtonVPN..."
    
    # Check if config file exists
    if [ ! -f "/etc/wireguard/protonvpn.conf" ]; then
        print_error "VPN configuration file not found at /etc/wireguard/protonvpn.conf"
        print_status "Please ensure you have copied your ProtonVPN config file to this location"
        exit 1
    fi
    
    # Connect to VPN
    sudo wg-quick up protonvpn
    
    if [ $? -eq 0 ]; then
        print_success "VPN connected successfully!"
        
        # Wait a moment for connection to establish
        sleep 2
        
        # Show connection details
        print_status "Connection details:"
        sudo wg show protonvpn
        
        # Show new IP
        print_status "Your new IP address:"
        local new_ip=$(get_current_ip)
        if [ -n "$new_ip" ]; then
            echo "$new_ip"
        else
            print_warning "Could not retrieve new IP address"
        fi
    else
        print_error "Failed to connect to VPN"
        exit 1
    fi
}

# Function to disconnect from VPN
disconnect_vpn() {
    print_status "Disconnecting from ProtonVPN..."
    
    sudo wg-quick down protonvpn
    
    if [ $? -eq 0 ]; then
        print_success "VPN disconnected successfully!"
        
        # Show original IP
        print_status "Your IP address is now:"
        local current_ip=$(get_current_ip)
        if [ -n "$current_ip" ]; then
            echo "$current_ip"
        else
            print_warning "Could not retrieve current IP address"
        fi
    else
        print_error "Failed to disconnect from VPN"
        exit 1
    fi
}

# Function to show VPN status
show_status() {
    if is_vpn_connected; then
        print_success "VPN is currently CONNECTED"
        print_status "Connection details:"
        sudo wg show protonvpn
        print_status "Current IP address:"
        local current_ip=$(get_current_ip)
        if [ -n "$current_ip" ]; then
            echo "$current_ip"
        else
            print_warning "Could not retrieve current IP address"
        fi
    else
        print_warning "VPN is currently DISCONNECTED"
        print_status "Your IP address:"
        local current_ip=$(get_current_ip)
        if [ -n "$current_ip" ]; then
            echo "$current_ip"
        else
            print_warning "Could not retrieve current IP address"
        fi
    fi
}

# Function to test connection speed
test_speed() {
    if command_exists "speedtest-cli"; then
        print_status "Testing connection speed..."
        speedtest-cli --simple
    else
        print_warning "speedtest-cli not found. Installing..."
        sudo apt install -y speedtest-cli >/dev/null 2>&1
        if [ $? -eq 0 ]; then
            print_status "Testing connection speed..."
            speedtest-cli --simple
        else
            print_error "Failed to install speedtest-cli"
        fi
    fi
}

# Main script logic
case "${1:-help}" in
    "connect"|"up")
        install_dependencies
        if is_vpn_connected; then
            print_warning "VPN is already connected"
            show_status
        else
            connect_vpn
        fi
        ;;
    "disconnect"|"down")
        if is_vpn_connected; then
            disconnect_vpn
        else
            print_warning "VPN is not connected"
        fi
        ;;
    "status")
        show_status
        ;;
    "toggle")
        if is_vpn_connected; then
            disconnect_vpn
        else
            install_dependencies
            connect_vpn
        fi
        ;;
    "speed")
        test_speed
        ;;
    "help"|"-h"|"--help")
        echo "VPN Management Script"
        echo "Usage: $0 [command]"
        echo ""
        echo "Commands:"
        echo "  connect    - Connect to VPN"
        echo "  disconnect - Disconnect from VPN"
        echo "  status     - Show VPN connection status"
        echo "  toggle     - Toggle VPN connection (connect if disconnected, disconnect if connected)"
        echo "  speed      - Test connection speed"
        echo "  help       - Show this help message"
        echo ""
        echo "Examples:"
        echo "  $0 connect"
        echo "  $0 status"
        echo "  $0 toggle"
        ;;
    *)
        print_error "Unknown command: $1"
        echo "Use '$0 help' for available commands"
        exit 1
        ;;
esac
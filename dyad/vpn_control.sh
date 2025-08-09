#!/bin/bash

# Simple WireGuard VPN Control Script

case "$1" in
    "connect"|"up")
        echo "Connecting to ProtonVPN..."
        sudo wg-quick up protonvpn
        if [ $? -eq 0 ]; then
            echo "VPN connected successfully!"
            echo "Your new IP address is:"
            curl -s ifconfig.me
        else
            echo "Failed to connect to VPN."
        fi
        ;;
    "disconnect"|"down")
        echo "Disconnecting from ProtonVPN..."
        sudo wg-quick down protonvpn
        if [ $? -eq 0 ]; then
            echo "VPN disconnected successfully!"
            echo "Your IP address is now:"
            curl -s ifconfig.me
        else
            echo "Failed to disconnect from VPN."
        fi
        ;;
    "status")
        echo "VPN connection status:"
        sudo wg show protonvpn
        ;;
    *)
        echo "Usage: $0 {connect|disconnect|status}"
        echo "  connect   - Connect to ProtonVPN"
        echo "  disconnect - Disconnect from ProtonVPN"
        echo "  status    - Show VPN connection status"
        ;;
esac
#!/bin/bash

# ProtonVPN Connection Script
echo "Setting up ProtonVPN connection..."
echo "Connecting to NL-FREE-114 server..."

# Connect to ProtonVPN using the configuration file
sudo openvpn --config /home/johnycash/Documents/vpnfile-NL-FREE-114.conf

# Check if connection was successful
if [ $? -eq 0 ]; then
    echo "VPN connection established successfully!"
    echo "Your IP address is now:"
    curl -s ifconfig.me
else
    echo "Failed to establish VPN connection."
    echo "Please check:"
    echo "1. Your ProtonVPN credentials"
    echo "2. The configuration file path"
    echo "3. Your internet connection"
fi
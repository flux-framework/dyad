#!/bin/bash

# ProtonVPN OpenVPN Connection Script
echo "Setting up ProtonVPN OpenVPN connection..."
echo "Connecting to NL-FREE-114 server..."

# Check if OpenVPN is installed
if ! command -v openvpn &> /dev/null; then
    echo "OpenVPN is not installed. Installing..."
    sudo apt update && sudo apt install openvpn -y
fi

# Connect to ProtonVPN using the OpenVPN configuration file
echo "When prompted, enter your ProtonVPN username and password:"
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
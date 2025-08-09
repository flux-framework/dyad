#!/bin/bash

echo "Setting up ProtonVPN..."
echo "Please enter your ProtonVPN credentials when prompted."

# Login to ProtonVPN
read -p "Enter your ProtonVPN username (email): " username
protonvpn-cli login "$username"

# Check if login was successful
if [ $? -eq 0 ]; then
    echo "Login successful. Connecting to the fastest server..."
    protonvpn-cli connect --fastest
else
    echo "Login failed. Please check your credentials and try again."
    exit 1
fi

# Check connection status
echo "Checking connection status..."
protonvpn-cli status
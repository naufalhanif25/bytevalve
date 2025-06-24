#!/bin/bash

# Define the program name
NAME="ByteValve"

# Define the program version
VERSION="0.0.1"

# Define default installation path
DEF_PATH="/usr/local/bin"

# Detect the operating system
OS=$(uname)

if [ "$OS" == "Linux" ]; then
    BIN_NAME="bytevalve"
else
    echo -e "\e[31mUnsupported OS: $OS\e[0m"
    
    exit 1
fi

# Show welcome message
echo -e "Welcome to $NAME $VERSION Installer\e[32m"

# Print ASCII art logo
cat << "EOF"
        ___       __     _   __     __                 
       / _ )__ __/ /____| | / /__ _/ /  _____          
      / _  / // / __/ -_) |/ / _ `/ / |/ / -_)         
     /____/\_, /\__/\__/|___/\_,_/_/|___/\__/     
          /___/                                        
EOF

echo -e "\e[0m"

# Check if ByteValve is already installed
INSTALLED=$(ls "$DEF_PATH" | grep bytevalve)

# Set the operation mode
if [ "$INSTALLED" == "" ]; then
    OPS="install"
    OPSING="installing"
    OPSED="installed"
else
    OPS="update"
    OPSING="updating"
    OPSED="updated"
fi

# Notify user if ByteValve is already installed
if [ "$OPS" == "update" ]; then
    echo -e "\e[31mByteValve already installed in $DEF_PATH\e[0m\n"
fi

# Prompt the user for confirmation
read -p "Are you sure you want to $OPS $NAME [y/n]? " INPUT

# Validate user input
while [[ ! "$INPUT" =~ ^[yYnN]$ ]]; do
    echo -e "\nThe '$INPUT' option is not recognized. Try again\n"
    read -p "Are you sure you want to $OPS $NAME [y/n]? " INPUT
done

# Proceed if user confirms with y or Y
if [[ "$INPUT" =~ ^[yY]$ ]]; then
    # Ask user to specify the installation path or use the default path
    echo ""
    read -p "Enter installation path (default is $DEF_PATH): " IN_PATH

    if [ "$IN_PATH" == "" ]; then
        IN_PATH=$DEF_PATH
    fi

    echo -e "\n\e[33m[ ${OPSING^} $NAME... ]\e[0m\n"

    # Copy the binary to the path
    echo -e "\e[36m>\e[0m Copying the $NAME binary..."

    sudo cp "$BIN_NAME" "$IN_PATH/$BIN_NAME"
    sleep 1
    
    # Make the binary executable
    echo -e "\e[36m>\e[0m Making the $NAME binary executable..."
    
    sudo chmod +x "$IN_PATH/$BIN_NAME"
    sleep 1

    # Show success message
    echo -e "\n\e[32m$NAME $VERSION successfully ${OPSED}!\e[0m"
    echo -e "\nYou can type '$BIN_NAME -v' or '$BIN_NAME --version' to check the version."
    echo -e "\nSee the GitHub page at \e[36mhttps://github.com/naufalhanif25/bytevalve.git\e[0m"
    
    exit 0
else
    # Show abort message
    if [ "$OPS" == "install" ]; then
        echo -e "\n\e[31mInstallation aborted\e[0m"
    else
        echo -e "\n\e[31mUpdate aborted\e[0m"
    fi
    
    exit 1
fi

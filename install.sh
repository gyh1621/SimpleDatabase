# detect os: https://unix.stackexchange.com/a/6348
if type lsb_release >/dev/null 2>&1; then
    # linuxbase.org
    OS=$(lsb_release -si)
    VER=$(lsb_release -sr)
elif [ -f /etc/debian_version ]; then
    # Older Debian/Ubuntu/etc.
    OS=Debian
    VER=$(cat /etc/debian_version)
else
    # Fall back to uname, e.g. "Linux <version>", also works for BSD, etc.
    OS=$(uname -s)
    VER=$(uname -r)
fi
echo $OS:$VER

if ! [ "$OS" == "Ubuntu" ] || ! [ "$VER" == "18.04" ]; then
    echo "Not running on Ubuntu 18.04. GCC 5.5.0 will not be installed."
    exit 0
fi

# install GCC 5.5.0 on Ubuntu 18.04
if hash gcc && gcc --version | grep "5.5.0"; then
    echo "GCC 5.5.0 already installed."
    exit 0
fi
sudo apt update && sudo apt install -y gcc-5 g++-5
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 10
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 20
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 10
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 20
sudo update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 30
sudo update-alternatives --set cc /usr/bin/gcc
sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 30
sudo update-alternatives --set c++ /usr/bin/g++
if gcc --version | grep "5.5.0"; then
    echo "GCC 5.5.0 installed successfully."
    exit 0
else
    echo "ERROR: Install GCC 5.5.0 failed."
    exit 1
fi

FROM mcr.microsoft.com/devcontainers/base:bookworm

# PlatformIO needs Python. The udev rules give the container user permission
# to open the USB-serial adapters (CP210x/CH9102/CH340) used to flash and
# monitor the board.
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3 \
        python3-pip \
        udev \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir --break-system-packages platformio \
    && curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules \
        -o /etc/udev/rules.d/99-platformio-udev.rules \
    && usermod -aG dialout,plugdev vscode

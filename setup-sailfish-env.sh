#!/usr/bin/env bash
# ==============================================================================
# Sailfish OS SDK Setup Script for CachyOS / Arch Linux
# Target: Jolla Phone 2026 (aarch64) & Sailfish Emulator
# ==============================================================================

set -euo pipefail

SDK_INSTALL_DIR="$HOME/SailfishOS"
DOWNLOADS_DIR="$HOME/Downloads"
INSTALLER_URL="https://releases.sailfishos.org/sdk/installers/3.13.5/SailfishSDK-3.13.5-linux64-online.run"
INSTALLER_FILE="$DOWNLOADS_DIR/SailfishSDK-3.13.5-linux64-online.run"

echo "=========================================================="
echo "⛵ Настройка на Sailfish OS среда за разработка (CachyOS)"
echo "   Целева платформа: Jolla Phone 2026 (aarch64)"
echo "=========================================================="
echo ""

# 1. Проверка на Docker
echo "🔍 1. Проверка на Docker..."
if ! command -v docker >/dev/null 2>&1; then
    echo "❌ Грешка: Docker не е инсталиран. Инсталирайте го с:"
    echo "   sudo pacman -S docker"
    exit 1
fi

if ! docker ps >/dev/null 2>&1; then
    echo "❌ Грешка: Docker daemon не работи или текущият потребител няма права."
    echo "   Изпълнете:"
    echo "   sudo systemctl enable --now docker"
    echo "   sudo usermod -aG docker $USER"
    echo "   (след което се превпишете в системата)"
    exit 1
fi
echo "   ✅ Docker е активен и достъпен."

# 2. Проверка и настройка на docker-buildx (критично за Arch/CachyOS)
echo "🔍 2. Проверка за Docker Buildx CLI плъгин..."
if ! docker buildx version >/dev/null 2>&1; then
    echo "   ⚠️ docker-buildx липсва. Изтегляне на плъгина в ~/.docker/cli-plugins/..."
    mkdir -p ~/.docker/cli-plugins
    curl -sL https://github.com/docker/buildx/releases/download/v0.37.1/buildx-v0.37.1.linux-amd64 -o ~/.docker/cli-plugins/docker-buildx
    chmod +x ~/.docker/cli-plugins/docker-buildx
fi
echo "   ✅ Docker buildx е наличен: $(docker buildx version | head -n 1)"

# 3. Проверка на VirtualBox (за емулатора)
echo "🔍 3. Проверка за VirtualBox (за емулатор)..."
if command -v vboxmanage >/dev/null 2>&1; then
    echo "   ✅ VirtualBox е инсталиран: $(vboxmanage --version)"
    if lsmod | grep -q vboxdrv; then
        echo "   ✅ VirtualBox ядрените модули са заредени."
    else
        echo "   ⚠️ VirtualBox модулите не са заредени. Може да се наложи:"
        echo "      sudo modprobe vboxdrv vboxnetadp vboxnetflt"
    fi
else
    echo "   ℹ️ VirtualBox не е инсталиран. Емулаторът няма да може да се стартира локално,"
    echo "      но крос-компилацията с Docker ще работи без проблем."
fi

# 4. Сваляне на Sailfish SDK Online Installer
echo "📥 4. Подготовка на инсталатора на Sailfish SDK..."
mkdir -p "$DOWNLOADS_DIR"
if [ ! -f "$INSTALLER_FILE" ]; then
    echo "   Сваляне на $INSTALLER_FILE..."
    curl -Lo "$INSTALLER_FILE" --progress-bar "$INSTALLER_URL"
    chmod +x "$INSTALLER_FILE"
else
    echo "   ✅ Инсталаторът вече е наличен в $INSTALLER_FILE"
    chmod +x "$INSTALLER_FILE"
fi

# 5. Инсталиране на Sailfish SDK (ако още не е инсталиран)
if [ ! -x "$SDK_INSTALL_DIR/bin/sfdk" ]; then
    echo "🚀 5. Инсталиране на Sailfish SDK с Docker Build Engine..."
    echo "   (Това може да отнеме няколко минути, в зависимост от интернет връзката)"
    QT_QPA_PLATFORM=minimal "$INSTALLER_FILE" non-interactive=1 accept-licenses=1 build-engine-type=docker
    echo "   ✅ Sailfish SDK е инсталиран успешно в $SDK_INSTALL_DIR."
else
    echo "   ✅ Sailfish SDK вече е инсталиран в $SDK_INSTALL_DIR."
fi

# 6. Добавяне на sfdk в PATH
echo "🔧 6. Конфигуриране на PATH в шела..."
EXPORT_LINE="export PATH=\"$SDK_INSTALL_DIR/bin:\$PATH\""

for RC in "$HOME/.bashrc" "$HOME/.zshrc"; do
    if [ -f "$RC" ]; then
        if ! grep -q "SailfishOS/bin" "$RC"; then
            echo "" >> "$RC"
            echo "# Sailfish OS SDK" >> "$RC"
            echo "$EXPORT_LINE" >> "$RC"
            echo "   Добавен PATH в $RC"
        fi
    fi
done

export PATH="$SDK_INSTALL_DIR/bin:$PATH"

# 7. Инсталиране на Build Targets
echo ""
echo "🎯 7. Управление на Build Targets през sfdk:"
echo "----------------------------------------------------------"
echo "Текущи targets:"
sfdk tools list || true

echo ""
echo "Препоръчителни стъпки за Jolla Phone 2026 и Емулатор:"
echo "1) За инсталиране на target за Jolla Phone (aarch64):"
echo "   sfdk tools install SailfishOS-latest-aarch64"
echo "   sfdk config --global --push target SailfishOS-latest-aarch64"
echo ""
echo "2) За инсталиране на емулатор (VirtualBox):"
echo "   sfdk emulator install SailfishOS-latest"
echo ""
echo "3) За компилиране на Car Hotspot:"
echo "   cd /home/psyhlo/Projects/car-hotspot"
echo "   ./build-hotspot.sh package"
echo "=========================================================="
echo "🎉 Средата е готова за работа!"

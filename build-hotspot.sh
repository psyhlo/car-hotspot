#!/usr/bin/env bash
# ==============================================================================
# Sailfish OS Build & Packaging Helper for Car Hotspot
# Target: Jolla Phone 2026 (aarch64) & Local Emulator
# ==============================================================================

set -euo pipefail

# Ensure Sailfish SDK bin is in PATH
SFDK_DIR="$HOME/SailfishOS/bin"
if [ -d "$SFDK_DIR" ]; then
    export PATH="$SFDK_DIR:$PATH"
fi

if ! command -v sfdk >/dev/null 2>&1; then
    echo "❌ Грешка: 'sfdk' не беше намерен в PATH или в $HOME/SailfishOS/bin."
    echo "   Уверете се, че Sailfish SDK е инсталиран успешно."
    exit 1
fi

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

SSH_SDK_KEY="$HOME/SailfishOS/vmshare/ssh/private_keys/sdk"
DEVICE_IP="${DEVICE_IP:-192.168.2.15}"

usage() {
    cat <<EOF
Използване: $0 [опция]

Опции за устройство (Jolla Phone 2026):
  run-device        Компилира, инсталира и пуска приложението на телефона директно
  deploy-device     Компилира и инсталира пакета на телефона (без стартиране)
  device-shell      Отваря интерактивен SSH шел на телефона (или изпълнява команда)
  device-logs       Следи системния дневник (journalctl) на телефона в реално време

Опции за емулатор:
  emulator-start    Стартира Sailfish OS емулатора
  emulator-stop     Спира Sailfish OS емулатора
  run-emulator      Компилира, инсталира и пуска приложението в емулатора

Основни опции за компилация:
  build             Компилира проекта за текущия активен target
  package           Компилира и създава RPM пакет (в директория RPMS/)
  clean             Изчиства генерираните build файлове
  targets           Показва списък с инсталираните build targets
  set-aarch64       Задава target по подразбиране за Jolla Phone (aarch64)
  set-emulator      Задава target за емулатор (i486 / x86_64)
  ide               Стартира Sailfish Qt Creator IDE
  help              Показва това съобщение

EOF
}

COMMAND="${1:-build}"

case "$COMMAND" in
    build)
        echo "🔨 Компилиране на harbour-carhotspot..."
        sfdk build
        echo "✅ Компилацията завърши успешно."
        ;;
    package)
        echo "📦 Генериране на RPM пакет..."
        sfdk package
        echo "✅ Готово! Проверете генерираните .rpm файлове:"
        ls -la RPMS/*.rpm 2>/dev/null || ls -la *.rpm 2>/dev/null || true
        ;;
    clean)
        echo "🧹 Изчистване на временни файлове..."
        sfdk qmake -- -r
        sfdk make clean || true
        rm -rf RPMS/
        rm -f *.o moc_* harbour-carhotspot
        ;;
    targets)
        echo "🎯 Инсталирани build targets:"
        sfdk tools list
        ;;
    set-aarch64)
        TARGET="$(sfdk tools list | grep -oE "[a-zA-Z0-9._-]*aarch64[a-zA-Z0-9._-]*" | head -n 1)"
        if [ -z "$TARGET" ]; then
            echo "⚠️  Не е намерен aarch64 таргет. Можете да го инсталирате с:"
            echo "   sfdk tools install SailfishOS-latest-aarch64"
        else
            echo "🎯 Задаване на активен таргет: $TARGET"
            sfdk config --global --push target "$TARGET"
        fi
        ;;
    set-emulator)
        TARGET="$(sfdk tools list | grep -oE "[a-zA-Z0-9._-]*(i486|x86_64)[a-zA-Z0-9._-]*" | head -n 1)"
        if [ -z "$TARGET" ]; then
            echo "⚠️  Не е намерен таргет за емулатор."
        else
            echo "🎯 Задаване на таргет за емулатор: $TARGET"
            sfdk config --global --push target "$TARGET"
        fi
        ;;
    run-device|device-run)
        echo "📱 Проверка на връзката с Jolla Phone на $DEVICE_IP..."
        if ! ping -c 1 -W 2 "$DEVICE_IP" >/dev/null 2>&1; then
            echo "❌ Грешка: Няма връзка с телефон на адрес $DEVICE_IP."
            echo "   Уверете се, че USB кабелът е свързан и в Settings > Developer tools е пуснат Remote connection."
            exit 1
        fi

        echo "🎯 Конфигуриране на aarch64 таргет..."
        sfdk config target=SailfishOS-5.1.0.11-aarch64
        if [ -f harbour-carhotspot ] && file harbour-carhotspot | grep -q -i "80386"; then
            echo "🧹 Изчистване на стари x86 обектни файлове..."
            rm -f *.o moc_* harbour-carhotspot
        fi

        echo "🔨 Компилиране и пакетиране за Jolla Phone (aarch64)..."
        sfdk qmake
        sfdk make
        sfdk package

        RPM_FILE="$(ls -1t RPMS/harbour-carhotspot-[0-9]*.aarch64.rpm 2>/dev/null | head -n 1)"
        if [ -z "$RPM_FILE" ]; then
            echo "❌ Грешка: Не беше намерен aarch64 RPM пакет."
            exit 1
        fi

        echo "🚀 Прехвърляне и инсталиране на телефона ($RPM_FILE)..."
        SSH_OPTS="-o BatchMode=yes -o StrictHostKeyChecking=no"
        if [ -f "$SSH_SDK_KEY" ]; then
            SSH_OPTS="$SSH_OPTS -i $SSH_SDK_KEY"
        fi

        scp $SSH_OPTS "$RPM_FILE" "defaultuser@$DEVICE_IP:/tmp/app.rpm"
        ssh $SSH_OPTS "root@$DEVICE_IP" "pkcon -y install-local /tmp/app.rpm"

        echo "🎉 Стартиране на Car Hotspot на екрана на телефона..."
        ssh $SSH_OPTS "defaultuser@$DEVICE_IP" "killall harbour-carhotspot 2>/dev/null || true; nohup invoker --type=silica-qt5 -d 5 /usr/bin/harbour-carhotspot >/dev/null 2>&1 &"
        echo "✅ Приложението стартира на телефона!"
        ;;
    deploy-device|device-deploy)
        echo "📱 Проверка на връзката с Jolla Phone на $DEVICE_IP..."
        if ! ping -c 1 -W 2 "$DEVICE_IP" >/dev/null 2>&1; then
            echo "❌ Грешка: Няма връзка с телефон на адрес $DEVICE_IP."
            exit 1
        fi

        sfdk config target=SailfishOS-5.1.0.11-aarch64
        if [ -f harbour-carhotspot ] && file harbour-carhotspot | grep -q -i "80386"; then
            rm -f *.o moc_* harbour-carhotspot
        fi
        sfdk qmake
        sfdk make
        sfdk package

        RPM_FILE="$(ls -1t RPMS/harbour-carhotspot-[0-9]*.aarch64.rpm 2>/dev/null | head -n 1)"
        SSH_OPTS="-o BatchMode=yes -o StrictHostKeyChecking=no"
        if [ -f "$SSH_SDK_KEY" ]; then
            SSH_OPTS="$SSH_OPTS -i $SSH_SDK_KEY"
        fi

        echo "🚀 Прехвърляне и инсталиране на телефона ($RPM_FILE)..."
        scp $SSH_OPTS "$RPM_FILE" "defaultuser@$DEVICE_IP:/tmp/app.rpm"
        ssh $SSH_OPTS "root@$DEVICE_IP" "pkcon -y install-local /tmp/app.rpm"
        echo "✅ Пакетът е инсталиран успешно на телефона."
        ;;
    device-shell)
        SSH_OPTS="-o StrictHostKeyChecking=no"
        if [ -f "$SSH_SDK_KEY" ]; then
            SSH_OPTS="$SSH_OPTS -i $SSH_SDK_KEY"
        fi
        shift || true
        ssh $SSH_OPTS "defaultuser@$DEVICE_IP" "$@"
        ;;
    device-logs)
        SSH_OPTS="-o StrictHostKeyChecking=no"
        if [ -f "$SSH_SDK_KEY" ]; then
            SSH_OPTS="$SSH_OPTS -i $SSH_SDK_KEY"
        fi
        ssh $SSH_OPTS "defaultuser@$DEVICE_IP" "journalctl -f | grep harbour-carhotspot"
        ;;
    emulator-start)
        echo "📱 Стартиране на емулатора..."
        sfdk emulator start
        ;;
    emulator-stop)
        echo "📱 Спиране на емулатора..."
        sfdk emulator stop
        ;;
    run-emulator)
        echo "📱 Проверка на емулатора..."
        if ! sfdk emulator status 2>&1 | grep -q "running: yes"; then
            echo "   Стартиране на Sailfish OS емулатора..."
            sfdk emulator start
            sleep 5
        fi

        echo "🔨 Компилиране за i486 емулатор..."
        sfdk config target=SailfishOS-5.1.0.11-i486
        rm -f *.o moc_* harbour-carhotspot
        sfdk qmake
        sfdk make
        sfdk package

        RPM_FILE="$(ls -1t RPMS/harbour-carhotspot-[0-9]*.i486.rpm 2>/dev/null | head -n 1)"
        if [ -z "$RPM_FILE" ]; then
            echo "❌ Грешка: Не беше намерен i486 RPM пакет."
            exit 1
        fi

        echo "🚀 Прехвърляне и инсталиране в емулатора ($RPM_FILE)..."
        scp -P 2223 -i "$SSH_SDK_KEY" -o StrictHostKeyChecking=no "$RPM_FILE" defaultuser@127.0.0.1:/tmp/app.rpm
        ssh -p 2223 -i "$SSH_SDK_KEY" -o StrictHostKeyChecking=no defaultuser@127.0.0.1 "sudo pkcon -y install-local /tmp/app.rpm"

        echo "🎉 Стартиране на Car Hotspot на екрана на емулатора..."
        ssh -p 2223 -i "$SSH_SDK_KEY" -o StrictHostKeyChecking=no defaultuser@127.0.0.1 "killall harbour-carhotspot 2>/dev/null || true; nohup invoker --type=silica-qt5 -d 5 /usr/bin/harbour-carhotspot >/dev/null 2>&1 &"
        echo "✅ Приложението стартира в емулатора!"
        ;;
    ide)
        echo "💻 Стартиране на Sailfish Qt Creator..."
        IDE_BIN=""
        for candidate in "$HOME/SailfishOS/bin/qtcreator.sh" "$HOME/SailfishOS/bin/sailfish-qtcreator" "$HOME/SailfishOS/bin/qtcreator"; do
            if [ -x "$candidate" ]; then
                IDE_BIN="$candidate"
                break
            fi
        done

        if [ -n "$IDE_BIN" ]; then
            nohup "$IDE_BIN" "$PROJECT_DIR/harbour-carhotspot.pro" >/dev/null 2>&1 &
            echo "✅ Sailfish Qt Creator стартира във фонов режим."
        else
            echo "❌ Qt Creator не е намерен в $HOME/SailfishOS/bin."
        fi
        ;;
    help|--help|-h)
        usage
        ;;
    *)
        echo "❌ Непозната команда: $COMMAND"
        usage
        exit 1
        ;;
esac

#!/usr/bin/env bash
# ==============================================================================
# Sailfish OS Build & Packaging Helper for Car Hotspot
# Target: Jolla Phone 2026 (aarch64) & Local Emulator
# ==============================================================================

set -euo pipefail

# Ensure Sailfish SDK bin is in PATH
for p in "$HOME/SailfishOS/bin" "/e/SailfishOS/bin" "/c/SailfishOS/bin" "E:/SailfishOS/bin" "C:/SailfishOS/bin"; do
    if [ -d "$p" ]; then
        export PATH="$p:$PATH"
    fi
done

if ! command -v sfdk >/dev/null 2>&1; then
    echo "❌ Грешка: 'sfdk' не беше намерен в PATH или в SailfishOS/bin."
    echo "   Уверете се, че Sailfish SDK е инсталиран успешно."
    exit 1
fi

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

SSH_SDK_KEY=""
for k in "$HOME/SailfishOS/vmshare/ssh/private_keys/sdk" \
         "/e/SailfishOS/vmshare/ssh/private_keys/sdk" \
         "/c/SailfishOS/vmshare/ssh/private_keys/sdk" \
         "$HOME/SailfishSDK/vmshare/ssh/private_keys/sdk"; do
    if [ -f "$k" ]; then
        SSH_SDK_KEY="$k"
        break
    fi
done
if [ -z "$SSH_SDK_KEY" ]; then
    echo "⚠️  ʟ警告: Не е намерен SDK SSH ключ; генерирай го с 'merssh generatesshkeys'."
fi
DEVICE_IP="${DEVICE_IP:-192.168.2.15}"

# On Windows 'ping -c' needs administrative privileges, so probe the SSH port
# instead: a "Permission denied" answer still proves the device is reachable.
device_reachable() {
    local out
    out="$(SSH_AUTH_SOCK="" ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=none \
                -o ConnectTimeout=5 -o StrictHostKeyChecking=no "defaultuser@$DEVICE_IP" 2>&1)"
    case "$out" in
        *"Permission denied"*|*"no authentication"*|*"No supported authentication"*) return 0 ;;
        *) echo "$out"; return 1 ;;
    esac
}

device_ssh_opts() {
    local opts="-F /dev/null -o BatchMode=yes -o IdentitiesOnly=yes -o StrictHostKeyChecking=no"
    if [ -f "$SSH_SDK_KEY" ]; then
        opts="$opts -i $SSH_SDK_KEY"
    fi
    echo "$opts"
}

# An SSH session on Sailfish gets neither XDG_RUNTIME_DIR nor WAYLAND_DISPLAY,
# but systemd --user and Silica apps need them. There is no sudo on Sailfish -
# the privileged path is devel-su, which prompts for the Developer-mode password.
DEVICE_USER_ENV='XDG_RUNTIME_DIR=/run/user/$(id -u) DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$(id -u)/dbus/user_bus_socket'
DEVICE_UI_ENV="WAYLAND_DISPLAY=/run/display/wayland-0 $DEVICE_USER_ENV"

device_run() {
    SSH_AUTH_SOCK="" ssh $(device_ssh_opts) "defaultuser@$DEVICE_IP" "$1" 2>&1 | tr -d '\r'
}

device_root_run() {
    if [ -n "${DEVICE_ROOT_PASSWORD:-}" ]; then
        printf '%s\n%s\n' "$DEVICE_ROOT_PASSWORD" "$DEVICE_ROOT_PASSWORD" | SSH_AUTH_SOCK="" ssh -tt $(device_ssh_opts) -o BatchMode=no \
            "defaultuser@$DEVICE_IP" "devel-su $1" 2>&1 | tr -d '\r' | grep -vF -- "$DEVICE_ROOT_PASSWORD"
    else
        SSH_AUTH_SOCK="" ssh -tt $(device_ssh_opts) "defaultuser@$DEVICE_IP" "devel-su $1" 2>&1 | tr -d '\r'
    fi
}

# sfdk reuses leftover .o files, and rpmbuild happily packages a binary of the
# wrong architecture (it cannot even strip it). Drop the objects when they do not
# match the active target.
clean_stale_objects() {
    local arch expect
    arch="$(sfdk config 2>/dev/null | grep -oE 'aarch64|armv7hl|i486' | head -n 1)"
    [ -z "$arch" ] && return 0
    case "$arch" in
        aarch64) expect="aarch64" ;;
        armv7hl) expect="ARM" ;;
        i486)    expect="80386" ;;
        *)       return 0 ;;
    esac
    if [ -f harbour-carhotspot ] && ! file harbour-carhotspot 2>/dev/null | grep -q "$expect"; then
        echo "🧹 Таргетът е $arch, а съществуващият бинар е от друга архитектура - изчистване..."
        rm -f *.o moc_* harbour-carhotspot
    fi
}

# Inside a Git working tree sfdk derives the package version from git tags and
# appends a branch/timestamp/hash suffix (e.g. 0.4.1+main.20261027...hash).
# For Harbour/OpenRepos releases we want the plain version from the RPM spec.
SFDK_OPTS="-c no-fix-version"

usage() {
    cat <<EOF
Използване: $0 [опция]

Опции за устройство (Jolla Phone 2026):
  setup-device      Поставя SDK публичния ключ на телефона (поиска паролата от Developer mode)
  run-device        Компилира, инсталира и пуска приложението на телефона директно
  deploy-device     Компилира и инсталира пакета на телефона (без стартиране)
  refresh-icons     Рестартира home screen-а (lipstick), за да се покаже обновена иконка
  device-shell      Отваря интерактивен SSH шел на телефона (или изпълнява команда)
  device-logs       Следи дневника на потребителската сесия (journalctl --user) на телефона

Опции за емулатор:
  emulator-start    Стартира Sailfish OS емулатора
  emulator-stop     Спира Sailfish OS емулатора
  run-emulator      Компилира, инсталира и пуска приложението в емулатора

Основни опции за компилация:
  build             Компилира проекта за текущия активен target
  check             Jolla Harbour/rpmlint проверка на RPM пакетите за текущия target
  build-all         Пакети за качване в RPMS/: aarch64 (5.1) + armv7hl (таргет 5.0,
                    работи на 4.x / 5.0 / 5.1+ / 5.2); вариантът само за 5.1 отива
                    в RPMS-sfos5.1/, а пакетите за 4.x/5.0 - в RPMS-sfos5.0/
  build-sfos50      Компилира armv7hl RPM, съвместим със Sailfish OS 4.x / 5.0 (glibc 2.30)
  package           Компилира и създава RPM пакет (в директория RPMS/)
  clean             Изчиства генерираните build файлове
  targets           Показва списък с инсталираните build targets
  set-aarch64       Задава target за 64-битов ARM (aarch64)
  set-armv7hl       Задава target за 32-битов ARM (armv7hl)
  set-emulator      Задава target за емулатор (i486 / x86_64)
  ide               Стартира Sailfish Qt Creator IDE
  help              Показва това съобщение

EOF
}

COMMAND="${1:-build}"

case "$COMMAND" in
    build)
        echo "🔨 Компилиране на harbour-carhotspot..."
        clean_stale_objects
        sfdk $SFDK_OPTS build
        echo "✅ Компилацията завърши успешно."
        ;;
    package)
        echo "📦 Генериране на RPM пакет..."
        clean_stale_objects
        sfdk $SFDK_OPTS package
        echo "✅ Готово! Проверете генерираните .rpm файлове:"
        ls -la RPMS/*.rpm 2>/dev/null || ls -la *.rpm 2>/dev/null || true
        ;;
    check|harbour-check)
        # Validate every package in RPMS/ with a matching target: checking an
        # aarch64 package with the armv7hl target produces a false rpmlint
        # objdump-failed error. The armv7hl package is validated against the 5.0
        # target when it is installed, because that is what build-all builds.
        FOUND=0
        for RPM in RPMS/harbour-carhotspot-*.rpm; do
            [ -f "$RPM" ] || continue
            FOUND=1
            ARCH="$(basename "$RPM" | grep -oE 'aarch64|armv7hl|i486' | head -n 1)"
            if [ "$ARCH" = "armv7hl" ] && sfdk tools list 2>/dev/null | grep -q "SailfishOS-5.0.0.62-armv7hl"; then
                TARGET="SailfishOS-5.0.0.62-armv7hl"
            else
                TARGET="SailfishOS-5.1.0.11-$ARCH"
            fi
            echo "🔍 Jolla Harbour проверка на $RPM (таргет $TARGET)..."
            sfdk $SFDK_OPTS -c target="$TARGET" -c device= check -- "$RPM" || true
        done

        if [ "$FOUND" = "0" ]; then
            echo "❌ Грешка: Няма RPM пакети в RPMS/. Пуснете 'build-all' първо."
            exit 1
        fi
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
            sfdk config target="$TARGET"
        fi
        ;;
    set-armv7hl)
        TARGET="$(sfdk tools list | grep -oE "[a-zA-Z0-9._-]*armv7hl[a-zA-Z0-9._-]*" | head -n 1)"
        if [ -z "$TARGET" ]; then
            echo "⚠️  Не е намерен armv7hl таргет."
        else
            echo "🎯 Задаване на активен таргет: $TARGET"
            sfdk config target="$TARGET"
        fi
        ;;
    build-all)
        # RPMS/ ends up with exactly the packages to upload to OpenRepos/Harbour:
        #   * aarch64  - built with the 5.1 target (64-bit ARM, Jolla Phone 2026)
        #   * armv7hl  - built with the 5.0 target (glibc 2.30): a binary linked
        #                against an old glibc also runs on newer OS releases, so
        #                this single package covers SFOS 4.x, 5.0, 5.1 and 5.2.
        echo "🔨 Компилиране за aarch64 (таргет 5.1)..."
        rm -f Makefile *.o moc_* harbour-carhotspot
        sfdk $SFDK_OPTS -c target=SailfishOS-5.1.0.11-aarch64 -c device= build

        echo "🔨 Компилиране за armv7hl с таргет 5.1 (само за RPMS-sfos5.1/)..."
        rm -f Makefile *.o moc_* harbour-carhotspot
        sfdk $SFDK_OPTS -c target=SailfishOS-5.1.0.11-armv7hl -c device= build
        mkdir -p RPMS-sfos5.1
        mv -f RPMS/harbour-carhotspot-*.armv7hl.rpm RPMS-sfos5.1/ 2>/dev/null || true

        if sfdk tools list | grep -q "SailfishOS-5.0.0.62-armv7hl"; then
            echo "🔨 Компилиране за armv7hl с таргет 5.0 (glibc 2.30, за 4.x/5.0/5.1+/5.2)..."
            rm -f Makefile *.o moc_* harbour-carhotspot
            sfdk $SFDK_OPTS -c target=SailfishOS-5.0.0.62-armv7hl -c device= build
            mkdir -p RPMS-sfos5.0
            for RPM in RPMS/harbour-carhotspot-*.armv7hl.rpm; do
                [ -f "$RPM" ] || continue
                BASE="$(basename "$RPM")"
                ALIAS="${BASE%.armv7hl.rpm}.sfos5.0.armv7hl.rpm"
                cp -a "$RPM" "RPMS-sfos5.0/$BASE"
                # NB: the alias is only for manual installs (devel-su pkcon install-local);
                # Harbour rejects file names that are not name-version-release.arch.rpm.
                cp -a "$RPM" "RPMS-sfos5.0/$ALIAS"
            done
        else
            echo "⚠️  Таргетът SailfishOS-5.0.0.62-armv7hl не е инсталиран -"
            echo "   armv7hl пакетът ще е само за Sailfish OS 5.1+. Инсталирай го с:"
            echo "   sfdk tools install SailfishOS-5.0.0.62-armv7hl"
        fi

        rm -f Makefile *.o moc_* harbour-carhotspot
        echo "✅ Готово! Пакети за качване в RPMS/:"
        ls -lh RPMS/*.rpm 2>/dev/null || true
        echo "📦 Допълнителни пакети:"
        ls -lh RPMS-sfos5.1/*.rpm RPMS-sfos5.0/*.rpm 2>/dev/null || true
        ;;
    build-sfos50)
        echo "🔨 Компилиране на armv7hl пакет за Sailfish OS 4.x / 5.0 (glibc 2.30)..."
        if ! sfdk tools list | grep -q "SailfishOS-5.0.0.62-armv7hl"; then
            echo "📥 Инсталиране на SDK таргет SailfishOS-5.0.0.62-armv7hl..."
            sfdk tools install SailfishOS-5.0.0.62-armv7hl
        fi
        mkdir -p RPMS-sfos5.0
        rm -f Makefile *.o moc_* harbour-carhotspot
        sfdk $SFDK_OPTS -c target=SailfishOS-5.0.0.62-armv7hl -c device= build
        rm -f Makefile *.o moc_* harbour-carhotspot
        for RPM in RPMS/harbour-carhotspot-*.armv7hl.rpm; do
            [ -f "$RPM" ] || continue
            BASE="$(basename "$RPM")"
            ALIAS="${BASE%.armv7hl.rpm}.sfos5.0.armv7hl.rpm"
            cp -a "$RPM" "RPMS-sfos5.0/$BASE"
            cp -a "$RPM" "RPMS-sfos5.0/$ALIAS"
        done
        echo "✅ Готово! Пакети за Sailfish OS 4.x / 5.0 в RPMS-sfos5.0/:"
        ls -lh RPMS-sfos5.0/*.rpm
        echo "   RPMS/ съдържа същия пакет (работи и на 5.1+/5.2)."
        ;;
    set-emulator)
        TARGET="$(sfdk tools list | grep -oE "[a-zA-Z0-9._-]*(i486|x86_64)[a-zA-Z0-9._-]*" | head -n 1)"
        if [ -z "$TARGET" ]; then
            echo "⚠️  Не е намерен таргет за емулатор."
        else
            echo "🎯 Задаване на таргет за емулатор: $TARGET"
            sfdk config target="$TARGET"
        fi
        ;;
    setup-device|device-setup)
        echo "📱 Проверка на SSH достъпа до $DEVICE_IP..."
        if ! device_reachable; then
            echo "❌ Грешка: Няма SSH достъп до телефона на адрес $DEVICE_IP."
            echo "   Свържи USB кабела и включи Settings > System > Developer mode."
            exit 1
        fi

        PUB="${SSH_SDK_KEY}.pub"
        if [ ! -f "$PUB" ]; then
            echo "❌ Грешка: Няма SDK публичен ключ ($PUB). Генерирай го с: merssh generatesshkeys"
            exit 1
        fi

        if device_run "echo KEY_ALREADY_OK" | grep -q "KEY_ALREADY_OK"; then
            echo "✅ Ключовият достъп вече работи - нищо не е нужно."
            exit 0
        fi

        echo "🔑 Поставяне на SDK публичния ключ (въведи паролата от Developer mode)..."
        SSH_AUTH_SOCK="" ssh -F /dev/null -o StrictHostKeyChecking=no -o PreferredAuthentications=password -o PubkeyAuthentication=no \
            "defaultuser@$DEVICE_IP" 'mkdir -p ~/.ssh && chmod 700 ~/.ssh && touch ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys && cat >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys && echo KEY_PUSHED' < "$PUB" | tr -d '\r'

        if device_run "echo KEY_LOGIN_OK" | grep -q "KEY_LOGIN_OK"; then
            echo "✅ Готово! Ключовият достъп е настроен."
            device_run "uname -m; grep -m1 PRETTY /etc/os-release" | sed 's/^/   /'
        else
            echo "❌ Ключът не беше приет. Провери паролата и Developer mode."
            exit 1
        fi
        ;;
    run-device|device-run)
        echo "📱 Проверка на SSH достъпа до Jolla Phone на $DEVICE_IP..."
        if ! device_reachable; then
            echo "❌ Грешка: Няма SSH достъп до телефон на адрес $DEVICE_IP."
            echo "   Уверете се, че USB кабелът е свързан и Developer mode е включен"
            echo "   (ако ключът още не е поставен: ./build-hotspot.sh setup-device)."
            exit 1
        fi

        echo "🎯 Конфигуриране на aarch64 таргет..."
        sfdk config target=SailfishOS-5.1.0.11-aarch64
        # Always start from a clean tree: leftover .o files from another target
        # (armv7hl/i486) get linked into the aarch64 RPM and rpmbuild cannot
        # strip them, producing a silently wrong package.
        echo "🧹 Изчистване на стари обектни файлове от други таргети..."
        rm -f *.o moc_* harbour-carhotspot

        echo "🔨 Компилиране и пакетиране за Jolla Phone (aarch64)..."
        sfdk $SFDK_OPTS qmake
        sfdk $SFDK_OPTS make
        sfdk $SFDK_OPTS package

        RPM_FILE="$(ls -1t RPMS/harbour-carhotspot-[0-9]*.aarch64.rpm 2>/dev/null | head -n 1)"
        if [ -z "$RPM_FILE" ]; then
            echo "❌ Грешка: Не беше намерен aarch64 RPM пакет."
            exit 1
        fi

        echo "🚀 Прехвърляне и инсталиране на телефона ($RPM_FILE)..."
        SSH_AUTH_SOCK="" scp $(device_ssh_opts) "$RPM_FILE" "defaultuser@$DEVICE_IP:/tmp/app.rpm"
        # Same-version reinstall needs a remove first (pkcon refuses 'already installed')
        device_root_run "pkcon -y install-local /tmp/app.rpm 2>&1 | tail -4 || (pkcon -y remove harbour-carhotspot && pkcon -y install-local /tmp/app.rpm)" | tail -6
        device_run "rpm -q harbour-carhotspot" | sed 's/^/   /'

        echo "🔄 Рестартиране на фоновия сървис..."
        device_run "$DEVICE_USER_ENV systemctl --user restart harbour-carhotspot.service 2>&1; $DEVICE_USER_ENV systemctl --user is-active harbour-carhotspot.service" | sed 's/^/   /'

        echo "🎉 Опит за стартиране на UI на екрана..."
        device_run "$DEVICE_UI_ENV nohup invoker --type=silica-qt5 --id=harbour-carhotspot >/dev/null 2>&1 & sleep 3; pgrep -f 'harbour-carhotspot$' | head -3"
        echo "   Ако приложението не се появи, отвори го от стартовия екран."
        ;;
    deploy-device|device-deploy)
        echo "📱 Проверка на SSH достъпа до Jolla Phone на $DEVICE_IP..."
        if ! device_reachable; then
            echo "❌ Грешка: Няма SSH достъп до телефон на адрес $DEVICE_IP."
            echo "   (ако ключът още не е поставен: ./build-hotspot.sh setup-device)"
            exit 1
        fi

        sfdk config target=SailfishOS-5.1.0.11-aarch64
        rm -f *.o moc_* harbour-carhotspot
        sfdk $SFDK_OPTS qmake
        sfdk $SFDK_OPTS make
        sfdk $SFDK_OPTS package

        RPM_FILE="$(ls -1t RPMS/harbour-carhotspot-[0-9]*.aarch64.rpm 2>/dev/null | head -n 1)"
        if [ -z "$RPM_FILE" ]; then
            echo "❌ Грешка: Не беше намерен aarch64 RPM пакет."
            exit 1
        fi

        echo "🚀 Прехвърляне и инсталиране на телефона ($RPM_FILE)..."
        SSH_AUTH_SOCK="" scp $(device_ssh_opts) "$RPM_FILE" "defaultuser@$DEVICE_IP:/tmp/app.rpm"
        device_root_run "pkcon -y install-local /tmp/app.rpm 2>&1 | tail -4 || (pkcon -y remove harbour-carhotspot && pkcon -y install-local /tmp/app.rpm)" | tail -6
        device_run "$DEVICE_USER_ENV systemctl --user restart harbour-carhotspot.service 2>&1" >/dev/null
        echo "✅ Пакетът е инсталиран и фоновият сървис е рестартиран: $(device_run "$DEVICE_USER_ENV systemctl --user is-active harbour-carhotspot.service")"
        ;;
    refresh-icons)
        echo "📱 Проверка на SSH достъпа до $DEVICE_IP..."
        if ! device_reachable; then
            echo "❌ Грешка: Няма SSH достъп до телефон на адрес $DEVICE_IP."
            exit 1
        fi

        echo "🔄 Рестартиране на home screen (lipstick) на телефона..."
        device_run "$DEVICE_USER_ENV systemctl --user restart lipstick" || {
            echo "⚠️  Неуспешен рестарт на lipstick. Опитвам през killall..."
            device_run "killall lipstick 2>/dev/null || true"
        }
        echo "✅ Home screen-ът е рестартиран. Новата иконка трябва да се покаже до няколко секунди."
        echo "   Ако иконката все още е стара, рестартирайте телефона: ssh $DEVICE_IP reboot"
        ;;
    device-shell)
        shift || true
        SSH_AUTH_SOCK="" ssh $(device_ssh_opts) "defaultuser@$DEVICE_IP" "$@"
        ;;
    device-logs)
        device_run "$DEVICE_USER_ENV journalctl --user -f 2>/dev/null | grep -i harbour-carhotspot"
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
        sfdk $SFDK_OPTS qmake
        sfdk $SFDK_OPTS make
        sfdk $SFDK_OPTS package

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

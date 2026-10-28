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
    # devel-su on Sailfish is an interactive root shell: it asks for the Developer-mode
    # password on the tty and then reads commands from stdin. "devel-su <command>" does NOT
    # work (it tries to exec the argument and hangs), and "pkcon install-local" hangs even as
    # root, so the password and the command are piped into the shell and local installs use
    # rpm directly.
    printf '%s\n%s\nexit\n' "${DEVICE_ROOT_PASSWORD:-}" "$1" | SSH_AUTH_SOCK="" ssh -tt $(device_ssh_opts) -o BatchMode=no \
        "defaultuser@$DEVICE_IP" devel-su 2>&1 | tr -d '\r' | grep -avF -- "${DEVICE_ROOT_PASSWORD:-}"
}

# The Developer-mode password is required for the privileged install path. Take it from the
# environment, otherwise ask for it once (the prompt is hidden).
ask_root_password() {
    if [ -n "${DEVICE_ROOT_PASSWORD:-}" ]; then
        return 0
    fi
    if [ -t 0 ]; then
        read -rs -p "Парола за Developer mode (devel-su): " DEVICE_ROOT_PASSWORD && echo
        export DEVICE_ROOT_PASSWORD
        [ -n "$DEVICE_ROOT_PASSWORD" ] && return 0
    fi
    echo "❌ Нужна е паролата за Developer mode."
    echo "   Вариант 1: пуснете командата в терминал и я въведете, когато попитаме."
    echo "   Вариант 2: export DEVICE_ROOT_PASSWORD=<парола> преди командата."
    return 1
}

# sfdk reuses leftover .o files, and rpmbuild happily packages a binary of the
# wrong architecture (it cannot even strip it). Drop the objects when they do not
# match the active target.
#
# The rpm spec macro %with_service selects the distribution flavour:
#   0 = Jolla Harbour build (no background service)
#   1 = OpenRepos build (sandboxed systemd user unit + "User services" permission)
set_service_flavour() {
    sed -i "s/^%define with_service .*/%define with_service $1/" rpm/harbour-carhotspot.spec
}

# OpenRepos needs one file per (OS release, architecture) pair. Two of them would
# otherwise produce the very same file name, so the Release tag carries the OS release.
set_release() {
    sed -i "s/^Release:    .*/Release:    $1/" rpm/harbour-carhotspot.spec
}

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
  build-openrepos   Пакети за OpenRepos с фонов сървис (with_service=1) -> RPMS-openrepos/
  deploy-openrepos  Инсталира OpenRepos варианта на телефона и стартира сервиса (поисква Developer mode парола)
  shots           Заснема екрани за магазина (shot режим) в screenshots/harbour/ (поисква Developer mode парола)
  build-emulator    i486 пакет за SDK емулатора -> RPMS-emulator/ (не е за магазина)
  deploy-emulator   Инсталира i486 пакета в SDK емулатора (root SSH, порт 2223)
  flavour           Показва активния flavour в spec-а (with_service)
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
        rm -rf RPMS/ RPMS-openrepos/ RPMS-emulator/ RPMS-sfos5.0/ RPMS-sfos5.1/
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
        # RPMS/ ends up with exactly the two files to upload to Jolla Harbour:
        #   harbour-carhotspot-<ver>-1.armv7hl.rpm   32-bit ARM (Xperia X, F2C, XA2, ...)
        #   harbour-carhotspot-<ver>-1.aarch64.rpm   64-bit ARM (Jolla Phone 2026)
        # Both are built with the current store target (5.1). Harbour accepts only files
        # named name-version-release.arch.rpm, so no aliases go into RPMS/.
        echo "🔨 Компилиране за aarch64 (таргет 5.1)..."
        rm -f Makefile *.o moc_* harbour-carhotspot
        sfdk $SFDK_OPTS -c target=SailfishOS-5.1.0.11-aarch64 -c device= build || exit 1

        echo "🔨 Компилиране за armv7hl - 32-bit (таргет 5.1)..."
        rm -f Makefile *.o moc_* harbour-carhotspot
        sfdk $SFDK_OPTS -c target=SailfishOS-5.1.0.11-armv7hl -c device= build || exit 1
        mkdir -p RPMS-sfos5.1
        cp -f RPMS/harbour-carhotspot-*.armv7hl.rpm RPMS-sfos5.1/ 2>/dev/null || true

        if sfdk tools list 2>/dev/null | grep -q "SailfishOS-5.0.0.62-armv7hl"; then
            echo "🔨 Компилиране за armv7hl с таргет 5.0 (glibc 2.30, за 4.x/5.0)..."
            rm -f Makefile *.o moc_* harbour-carhotspot
            if sfdk $SFDK_OPTS -c target=SailfishOS-5.0.0.62-armv7hl -c device= build; then
                mkdir -p RPMS-sfos5.0
                for RPM in RPMS/harbour-carhotspot-*.armv7hl.rpm; do
                    [ -f "$RPM" ] || continue
                    BASE="$(basename "$RPM")"
                    ALIAS="${BASE%.armv7hl.rpm}.sfos5.0.armv7hl.rpm"
                    cp -a "$RPM" "RPMS-sfos5.0/$BASE"
                    # NB: the alias is only for manual installs; Harbour rejects file names
                    # that are not name-version-release.arch.rpm.
                    cp -a "$RPM" "RPMS-sfos5.0/$ALIAS"
                done
                # The 5.0 build reused the RPMS/ name - put the store-target build back.
                cp -f RPMS-sfos5.1/harbour-carhotspot-*.armv7hl.rpm RPMS/ 2>/dev/null || true
            fi
        else
            echo "⚠️  Таргетът SailfishOS-5.0.0.62-armv7hl не е инсталиран -"
            echo "   armv7hl пакетът е само за Sailfish OS 5.1+. Инсталирай го с:"
            echo "   sfdk tools install SailfishOS-5.0.0.62-armv7hl"
        fi

        # An emulator (i486) package must never end up next to the store packages.
        rm -f RPMS/harbour-carhotspot-*.i486.rpm
        rm -f Makefile *.o moc_* harbour-carhotspot
        echo "✅ Готово! Двата файла за Harbour в RPMS/:"
        ls -lh RPMS/*.rpm 2>/dev/null || true
        echo "📦 Допълнителни (не се качват в Harbour):"
        ls -lh RPMS-sfos5.1/*.rpm RPMS-sfos5.0/*.rpm RPMS-emulator/*.rpm 2>/dev/null || true
        echo "   За OpenRepos варианта с фонов сървис: ./build-hotspot.sh build-openrepos"
        echo "   (build-openrepos ползва RPMS/ като работна директория на sfdk - след него"
        echo "    пуснете отново build-all, ако ви трябват Harbour пакетите в RPMS/.)"
        ;;
    build-openrepos|openrepos)
        # OpenRepos flavour: ships the systemd *user* service, so the car automation keeps
        # working after a reboot and with the app closed. Jolla Harbour does not accept
        # background services -> upload these packages to OpenRepos only.
        #
        # Four upload files, one per OS release and architecture:
        #   SailfishOS 5.1 32-bit (armv7hl)  Release 1
        #   SailfishOS 5.1 64-bit (aarch64)  Release 1
        #   SailfishOS 5.0 32-bit (armv7hl)  Release 1sfos50
        #   SailfishOS 5.0 64-bit (aarch64)  Release 1sfos50
        # A package built against the older target (glibc 2.30) also installs on newer
        # releases, but OpenRepos lists compatibility per uploaded file, so build each pair
        # explicitly and keep the file names distinct.
        echo "🔨 Компилране на OpenRepos варианти (with_service=1) за 5.0/5.1 и 32/64 бит..."
        trap 'set_service_flavour 0; set_release 1' EXIT
        set_service_flavour 1
        set_release 1
        mkdir -p RPMS-openrepos

        for COMBO in \
            "SailfishOS-5.1.0.11-aarch64 aarch64 1" \
            "SailfishOS-5.1.0.11-armv7hl armv7hl 1" \
            "SailfishOS-5.0.0.62-aarch64 aarch64 1sfos50" \
            "SailfishOS-5.0.0.62-armv7hl armv7hl 1sfos50"; do
            set -- $COMBO
            TARGET="$1"; ARCH="$2"; REL="$3"

            if ! sfdk tools target list | grep -q "$TARGET"; then
                echo "📥 Инсталиране на SDK таргет $TARGET..."
                sfdk tools target install "$TARGET" || { echo "❌ Таргетът $TARGET не може да се инсталира"; continue; }
            fi

            echo "🔨 $TARGET (Release $REL)..."
            rm -f Makefile *.o moc_* harbour-carhotspot
            set_release "$REL"
            if sfdk $SFDK_OPTS -c target="$TARGET" -c device= build; then
                cp -f RPMS/harbour-carhotspot-*."$ARCH".rpm RPMS-openrepos/ 2>/dev/null || true
            else
                echo "❌ Неудачно компилиране за $TARGET"
            fi
        done

        set_service_flavour 0
        set_release 1
        trap - EXIT
        rm -f Makefile *.o moc_* harbour-carhotspot
        echo "✅ OpenRepos пакети (4 файла) в RPMS-openrepos/:"
        ls -lh RPMS-openrepos/*.rpm 2>/dev/null || true
        echo "⚠️  RPMS/ е работната директория на sfdk. За Harbour пакетите: ./build-hotspot.sh build-all"
        echo "   Инсталиране на телефона:  ./build-hotspot.sh deploy-openrepos"
        echo "   Инсталиране в емулатора:  ./build-hotspot.sh build-emulator && ./build-hotspot.sh deploy-emulator"
        
        ;;
    build-emulator|emu-build)
        # i486 build for the SDK emulator (SailfishOS-*-i486 target), OpenRepos flavour.
        TARGET="$(sfdk tools list | grep -oE 'SailfishOS-[0-9.]+-i486' | tail -n 1)"
        if [ -z "$TARGET" ]; then
            echo "❌ Няма i486 таргет. Инсталирай: sfdk tools install SailfishOS-latest-i486"
            exit 1
        fi
        set_service_flavour 1
        trap 'set_service_flavour 0' EXIT
        rm -f Makefile *.o moc_* harbour-carhotspot
        sfdk $SFDK_OPTS -c target="$TARGET" -c device= build
        # i486 is the SDK emulator only - it is not a store package, so it lives apart.
        mkdir -p RPMS-emulator
        cp -f RPMS/harbour-carhotspot-*.i486.rpm RPMS-emulator/ 2>/dev/null || true
        rm -f RPMS/harbour-carhotspot-*.i486.rpm RPMS-openrepos/harbour-carhotspot-*.i486.rpm
        set_service_flavour 0
        trap - EXIT
        echo "✅ i486 пакет за емулатора: RPMS-emulator/$(basename "$(ls -1t RPMS-emulator/*.i486.rpm 2>/dev/null | head -n 1)")"
        ;;
    deploy-emulator|emu-deploy)
        # Install into the SDK emulator. The emulator allows root SSH with the SDK key,
        # so no Developer-mode password is needed here (unlike the phone).
        RPM_FILE="$(ls -1t RPMS-emulator/harbour-carhotspot-[0-9]*.i486.rpm RPMS-openrepos/harbour-carhotspot-[0-9]*.i486.rpm RPMS/harbour-carhotspot-[0-9]*.i486.rpm 2>/dev/null | head -n 1)"
        if [ -z "$RPM_FILE" ]; then
            echo "❌ Няма i486 пакет. Пуснете: ./build-hotspot.sh build-emulator"
            exit 1
        fi
        EMU_PORT="${EMU_PORT:-2223}"
        SSH_AUTH_SOCK="" scp -F /dev/null -P "$EMU_PORT" -o BatchMode=yes -o IdentitiesOnly=yes \
            -o StrictHostKeyChecking=no -i "$SSH_SDK_KEY" "$RPM_FILE" "root@127.0.0.1:/tmp/carhotspot-emu.rpm"
        SSH_AUTH_SOCK="" ssh -F /dev/null -p "$EMU_PORT" -o BatchMode=yes -o IdentitiesOnly=yes \
            -o StrictHostKeyChecking=no -i "$SSH_SDK_KEY" root@127.0.0.1 \
            "rpm -Uvh --replacepkgs /tmp/carhotspot-emu.rpm 2>&1 | tail -3; ls -l /usr/lib/systemd/user/harbour-carhotspot.service; ls -l /home/defaultuser/.config/harbour-carhotspot/harbour-carhotspot/carhotspot.ini 2>&1" | sed 's/^/   /'
        echo "🔌 Стартиране на сервиса в емулатора..."
        SSH_AUTH_SOCK="" ssh -F /dev/null -p "$EMU_PORT" -o BatchMode=yes -o IdentitiesOnly=yes \
            -o StrictHostKeyChecking=no -i "$SSH_SDK_KEY" defaultuser@127.0.0.1 \
            "XDG_RUNTIME_DIR=/run/user/\$(id -u) DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/\$(id -u)/dbus/user_bus_socket systemctl --user enable harbour-carhotspot.service; systemctl --user restart harbour-carhotspot.service; XDG_RUNTIME_DIR=/run/user/\$(id -u) systemctl --user --no-pager status harbour-carhotspot.service | head -n 6" | sed 's/^/   /'
        echo "✅ Инсталирано в емулатора. UI (ако емулаторът има applauncherd):"
        echo "   systemd-run --user --unit=carhotspot-ui -p Environment=WAYLAND_DISPLAY=/run/display/wayland-0 /usr/bin/harbour-carhotspot"
        ;;
    shots|screenshots)
        # Store screenshots. The UI is grabbed from inside the app (shot mode in main.cpp),
        # which only produces real pixels while the app is the foreground app on an unlocked
        # screen - so this command arms the shot mode, waits for you to open the app from the
        # launcher, triggers the capture and copies the PNGs to screenshots/harbour/.
        INI="/home/defaultuser/.config/harbour-carhotspot/harbour-carhotspot/carhotspot.ini"
        echo "📸 Проверка на SSH достъпа до Jolla Phone на $DEVICE_IP..."
        if ! device_reachable; then
            echo "❌ Грешка: Няма SSH достъп до телефона на адрес $DEVICE_IP."
            exit 1
        fi
        TS="$(date +%s)"
        DEVDIR="/home/defaultuser/.cache/harbour-carhotspot/harbour-carhotspot/shots-$TS"

        echo "🎯 Подготвям shot режима..."
        device_run "sed -i '/^devShot/d' $INI; printf 'devShotEnabled=1\n' >> $INI; mkdir -p $DEVDIR" | sed 's/^/   /'

        echo "👉 Отключете телефона и натиснете иконката на Car Hotspot (чакам до 90 сек.)..."
        FOUND=0
        for i in $(seq 1 45); do
            if device_run "pgrep -x harbour-carhots" | grep -q harbour; then
                FOUND=1
                break
            fi
            sleep 2
        done
        if [ "$FOUND" != "1" ]; then
            echo "❌ Грешка: UI-то не стартира. Натиснете иконката и пуснете отново."
            device_run "sed -i '/^devShot/d' $INI"
            exit 1
        fi
        echo "   UI-то е на екрана - заснемам..."
        device_run "printf 'devShotDir=$DEVDIR\ndevShotTrigger=$TS\n' >> $INI" | sed 's/^/   /'
        sleep 26

        echo "   Отчет (colors>1 = реално съдържание):"
        device_run "sed -n 's/^devShotStatus=//p' $INI | tail -8" | sed 's/^/   /'
        device_run "ls -la $DEVDIR | tail -6" | sed 's/^/   /'
        device_run "sed -i '/^devShot/d' $INI" | sed 's/^/   /'

        mkdir -p screenshots/harbour
        SSH_AUTH_SOCK="" scp $(device_ssh_opts) "defaultuser@$DEVICE_IP:$DEVDIR/*.png" screenshots/harbour/ 2>/dev/null | tail -3

        # Harbour wants at least 1080 px width; the device panel is 1032 px.
        if command -v ffmpeg >/dev/null 2>&1; then
            for f in screenshots/harbour/shot-*.png; do
                [ -f "$f" ] || continue
                SIZE=$(ffmpeg -hide_banner -i "$f" -v error -map 0:v:0 -f null - 2>&1 | grep -oE '[0-9]+x[0-9]+' | head -n 1)
                W="${SIZE%x*}"
                if [ -n "$W" ] && [ "$W" -lt 1080 ] 2>/dev/null; then
                    ffmpeg -hide_banner -loglevel error -y -i "$f" -vf "scale=1080:-2:flags=lanczos" "$f" && echo "   $(basename "$f"): ${SIZE} -> 1080px"
                fi
            done
        else
            echo "   (ffmpeg не е намерен - PNG-овете остават с размерите на екрана)"
        fi
        ls -la screenshots/harbour/ | tail -8
        echo "✅ Файловете са в screenshots/harbour/"
        ;;
    deploy-openrepos|openrepos-device)
        # Install the OpenRepos flavour (with the background user service) on the phone and
        # start the service. Needs the Developer-mode password for devel-su.
        echo "📱 Проверка на SSH достъпа до Jolla Phone на $DEVICE_IP..."
        if ! device_reachable; then
            echo "❌ Грешка: Няма SSH достъп до телефона на адрес $DEVICE_IP."
            echo "   (ако ключът още не е поставен: ./build-hotspot.sh setup-device)"
            exit 1
        fi
        ask_root_password || exit 1

        # Prefer the 5.1 build; the *_sfos50 packages are the OpenRepos uploads for older OS.
        RPM_FILE="$(ls -1t RPMS-openrepos/harbour-carhotspot-[0-9]*.aarch64.rpm 2>/dev/null | grep -v sfos50 | head -n 1)"
        if [ -z "$RPM_FILE" ]; then
            RPM_FILE="$(ls -1t RPMS-openrepos/harbour-carhotspot-[0-9]*.aarch64.rpm 2>/dev/null | head -n 1)"
        fi
        if [ -z "$RPM_FILE" ]; then
            echo "❌ Грешка: Няма OpenRepos пакет. Пуснете: ./build-hotspot.sh build-openrepos"
            exit 1
        fi

        echo "🚀 Прехвърляне на $RPM_FILE към $DEVICE_IP..."
        SSH_AUTH_SOCK="" scp $(device_ssh_opts) "$RPM_FILE" "defaultuser@$DEVICE_IP:/tmp/carhotspot-openrepos.rpm" || exit 1

        echo "📦 Инсталиране на пакета (rpm през devel-su)..."
        device_root_run "rpm -Uvh --replacepkgs /tmp/carhotspot-openrepos.rpm 2>&1 | tail -4" | tail -8
        device_run "rpm -q harbour-carhotspot" | sed 's/^/   /'
        device_run "ls -l /usr/lib/systemd/user/harbour-carhotspot.service" | sed 's/^/   /'

        echo "🔌 Активиране и стартиране на потребителския сервис..."
        device_run "$DEVICE_USER_ENV systemctl --user enable harbour-carhotspot.service 2>&1 | tail -2" | sed 's/^/   /'
        device_run "$DEVICE_USER_ENV systemctl --user restart harbour-carhotspot.service" | sed 's/^/   /'
        sleep 4
        echo "   Състояние на сервиса:"
        device_run "$DEVICE_USER_ENV systemctl --user status harbour-carhotspot.service --no-pager | head -n 8" | sed 's/^/   /'

        echo "🎉 Стартиране на UI-то..."
        # A leftover UI instance from another flavour would occupy the transient unit name.
        device_run "$DEVICE_USER_ENV systemctl --user stop carhotspot-ui.service 2>/dev/null; true" | sed 's/^/   /'
        device_run "$DEVICE_USER_ENV systemd-run --user --unit=carhotspot-ui --property=Environment=WAYLAND_DISPLAY=/run/display/wayland-0 /usr/bin/sailjail -p harbour-carhotspot.desktop -- /usr/bin/harbour-carhotspot 2>&1 | tail -2" | sed 's/^/   /'
        sleep 10
        echo "   UI:" $(device_run "$DEVICE_USER_ENV systemctl --user is-active carhotspot-ui.service")
        echo "✅ OpenRepos варианта е инсталирана. Дневник: ./build-hotspot.sh device-logs"
        ;;
    flavour)
        echo "🍡 Активен flavour в spec-а:"
        grep -n "^%define with_service" rpm/harbour-carhotspot.spec | head -n 1
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
        # Same-version reinstall: --replacepkgs lets rpm accept the identical NVR
        device_root_run "rpm -Uvh --replacepkgs /tmp/app.rpm 2>&1 | tail -4" | tail -6
        device_run "rpm -q harbour-carhotspot" | sed 's/^/   /'

        echo "🔄 Рестартиране на фоновия сървис (съществува само при OpenRepos варианта)..."
        device_run "$DEVICE_USER_ENV systemctl --user restart harbour-carhotspot.service 2>&1; $DEVICE_USER_ENV systemctl --user is-active harbour-carhotspot.service" | sed 's/^/   /'

        echo "🎉 Стартиране на UI на екрана..."
        # Start inside the sandbox exactly like lipstick does; fall back to plain invoker
        # if the sailjail profile cannot be used from this SSH session.
        device_run "$DEVICE_UI_ENV nohup /usr/bin/sailjail -p harbour-carhotspot.desktop -- /usr/bin/harbour-carhotspot >/dev/null 2>&1 & sleep 3; pgrep -f 'harbour-carhotspot$' | head -3" | sed 's/^/   /'
        if ! device_run "pgrep -f 'harbour-carhotspot$'" | grep -q harbour-carhotspot; then
            device_run "$DEVICE_UI_ENV nohup invoker --type=silica-qt5 /usr/bin/harbour-carhotspot >/dev/null 2>&1 & sleep 3; pgrep -f 'harbour-carhotspot$' | head -3" | sed 's/^/   /'
        fi
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
        device_root_run "rpm -Uvh --replacepkgs /tmp/app.rpm 2>&1 | tail -4" | tail -6
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

        RPM_FILE="$(ls -1t RPMS-emulator/harbour-carhotspot-[0-9]*.i486.rpm RPMS/harbour-carhotspot-[0-9]*.i486.rpm 2>/dev/null | head -n 1)"
        if [ -z "$RPM_FILE" ]; then
            echo "❌ Грешка: Не беше намерен i486 RPM пакет."
            exit 1
        fi

        echo "🚀 Прехвърляне и инсталиране в емулатора ($RPM_FILE)..."
        # The emulator allows root SSH with the SDK key; there is no sudo on Sailfish and
        # pkcon needs an authentication agent, so rpm is used directly.
        scp -P 2223 -i "$SSH_SDK_KEY" -o StrictHostKeyChecking=no "$RPM_FILE" root@127.0.0.1:/tmp/app.rpm
        ssh -p 2223 -i "$SSH_SDK_KEY" -o StrictHostKeyChecking=no root@127.0.0.1 "rpm -Uvh --replacepkgs /tmp/app.rpm 2>&1 | tail -3"

        echo "🎉 Стартиране на Car Hotspot на екрана на емулатора..."
        # A transient user unit survives the SSH session; a plain nohup child does not.
        ssh -p 2223 -i "$SSH_SDK_KEY" -o StrictHostKeyChecking=no defaultuser@127.0.0.1 \
            "XDG_RUNTIME_DIR=/run/user/\$(id -u) DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/\$(id -u)/dbus/user_bus_socket systemd-run --user --unit=carhotspot-ui --property=Environment=WAYLAND_DISPLAY=/run/display/wayland-0 /usr/bin/harbour-carhotspot 2>&1 | tail -2"
        echo "✅ Приложението стартира в емулатора (спрете го с: systemctl --user stop carhotspot-ui)"
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

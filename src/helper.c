#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // Elevate privileges to root (SUID binary: owner root, chmod 4755)
    if (setuid(0) != 0 || setgid(0) != 0) {
        perror("harbour-carhotspot-helper: setuid/setgid failed");
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: %s {wifi-off|wifi-on|tethering-off|tethering-on|cellular-on|cellular-off|bluetooth-on|bluetooth-off|bluetooth-restart}\n", argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "bluetooth-restart") == 0) {
        // Deep recovery of the Sailfish OS Bluetooth subsystem
        system("/usr/bin/dbus-send --system --dest=net.connman /net/connman/technology/bluetooth net.connman.Technology.SetProperty string:Powered variant:boolean:false 2>/dev/null || true");
        system("/usr/bin/systemctl restart bluetooth.service 2>/dev/null || true");
        system("/usr/sbin/rfkill unblock bluetooth 2>/dev/null || /usr/bin/rfkill unblock bluetooth 2>/dev/null || true");
        system("/usr/bin/dbus-send --system --dest=net.connman /net/connman/technology/bluetooth net.connman.Technology.SetProperty string:Powered variant:boolean:true 2>/dev/null || true");
        return 0;
    } else if (strcmp(cmd, "bluetooth-on") == 0) {
        system("/usr/sbin/rfkill unblock bluetooth 2>/dev/null || /usr/bin/rfkill unblock bluetooth 2>/dev/null || true");
        system("/usr/bin/dbus-send --system --dest=net.connman /net/connman/technology/bluetooth net.connman.Technology.SetProperty string:Powered variant:boolean:true 2>/dev/null || true");
        return 0;
    } else if (strcmp(cmd, "bluetooth-off") == 0) {
        system("/usr/bin/dbus-send --system --dest=net.connman /net/connman/technology/bluetooth net.connman.Technology.SetProperty string:Powered variant:boolean:false 2>/dev/null || true");
        return 0;
    } else if (strcmp(cmd, "wifi-off") == 0) {
        system("/usr/bin/dbus-send --system --dest=net.connman /net/connman/technology/wifi net.connman.Technology.SetProperty string:Powered variant:boolean:false 2>/dev/null || true");
        return 0;
    } else if (strcmp(cmd, "wifi-on") == 0) {
        system("/usr/bin/dbus-send --system --dest=net.connman /net/connman/technology/wifi net.connman.Technology.SetProperty string:Powered variant:boolean:true 2>/dev/null || true");
        return 0;
    } else if (strcmp(cmd, "tethering-on") == 0) {
        system("/usr/bin/dbus-send --system --dest=net.connman /net/connman/technology/wifi net.connman.Technology.SetProperty string:Tethering variant:boolean:true 2>/dev/null || true");
        return 0;
    } else if (strcmp(cmd, "tethering-off") == 0) {
        system("/usr/bin/dbus-send --system --dest=net.connman /net/connman/technology/wifi net.connman.Technology.SetProperty string:Tethering variant:boolean:false 2>/dev/null || true");
        return 0;
    } else if (strcmp(cmd, "cellular-on") == 0) {
        system("/usr/bin/dbus-send --system --dest=net.connman /net/connman/technology/cellular net.connman.Technology.SetProperty string:Powered variant:boolean:true 2>/dev/null || true");
        return 0;
    } else if (strcmp(cmd, "cellular-off") == 0) {
        system("/usr/bin/dbus-send --system --dest=net.connman /net/connman/technology/cellular net.connman.Technology.SetProperty string:Powered variant:boolean:false 2>/dev/null || true");
        return 0;
    }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    return 1;
}

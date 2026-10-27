#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Add the translation entries that are still missing from the .ts files.

Missing strings are inserted into the correct <context> block of every
translation file, using terminology consistent with the existing entries
(see tools/l10n/*.py and the .ts files themselves).
"""

import glob
import os
import re
import sys
from xml.sax.saxutils import escape

# (context, source english)
STRINGS = [
    ("AppController", "Hotspot problem"),
    ("ToolsPage", "Recovering Bluetooth subsystem..."),
    ("ToolsPage", "Restarting Bluetooth..."),
    ("MainPage", "Selected Car"),
    ("MainPage", "Selected Cars (%1)"),
]

TRANSLATIONS = {
    "ar": {
        "Hotspot problem": "مشكلة في نقطة الاتصال",
        "Recovering Bluetooth subsystem...": "جارٍ استعادة نظام البلوتوث الفرعي...",
        "Restarting Bluetooth...": "جارٍ إعادة تشغيل البلوتوث...",
        "Selected Car": "السيارة المحددة",
        "Selected Cars (%1)": "السيارات المحددة (%1)",
    },
    "bg": {
        "Hotspot problem": "Проблем с хотспота",
        "Recovering Bluetooth subsystem...": "Възстановяване на Bluetooth подсистемата...",
        "Restarting Bluetooth...": "Рестартиране на Bluetooth...",
        "Selected Car": "Избрана кола",
        "Selected Cars (%1)": "Избрани коли (%1)",
    },
    "ca": {
        "Hotspot problem": "Problema del punt d'accés",
        "Recovering Bluetooth subsystem...": "Recuperant el subsistema Bluetooth...",
        "Restarting Bluetooth...": "Reiniciant el Bluetooth...",
        "Selected Car": "Cotxe seleccionat",
        "Selected Cars (%1)": "Cotxes seleccionats (%1)",
    },
    "cs": {
        "Hotspot problem": "Problém s hotspotem",
        "Recovering Bluetooth subsystem...": "Obnovování subsystému Bluetooth...",
        "Restarting Bluetooth...": "Restartování Bluetooth...",
        "Selected Car": "Vybrané auto",
        "Selected Cars (%1)": "Vybraná auta (%1)",
    },
    "da": {
        "Hotspot problem": "Problem med hotspot",
        "Recovering Bluetooth subsystem...": "Gendanner Bluetooth-delsystem...",
        "Restarting Bluetooth...": "Genstarter Bluetooth...",
        "Selected Car": "Valgt bil",
        "Selected Cars (%1)": "Valgte biler (%1)",
    },
    "de": {
        "Hotspot problem": "Hotspot-Problem",
        "Recovering Bluetooth subsystem...": "Bluetooth-Subsystem wird wiederhergestellt...",
        "Restarting Bluetooth...": "Bluetooth wird neu gestartet...",
        "Selected Car": "Ausgewähltes Auto",
        "Selected Cars (%1)": "Ausgewählte Autos (%1)",
    },
    "el": {
        "Hotspot problem": "Πρόβλημα Hotspot",
        "Recovering Bluetooth subsystem...": "Ανάκτηση υποσυστήματος Bluetooth...",
        "Restarting Bluetooth...": "Επανεκκίνηση Bluetooth...",
        "Selected Car": "Επιλεγμένο αυτοκίνητο",
        "Selected Cars (%1)": "Επιλεγμένα αυτοκίνητα (%1)",
    },
    "en": {
        "Hotspot problem": "Hotspot problem",
        "Recovering Bluetooth subsystem...": "Recovering Bluetooth subsystem...",
        "Restarting Bluetooth...": "Restarting Bluetooth...",
        "Selected Car": "Selected Car",
        "Selected Cars (%1)": "Selected Cars (%1)",
    },
    "es": {
        "Hotspot problem": "Problema del punto de acceso",
        "Recovering Bluetooth subsystem...": "Recuperando el subsistema Bluetooth...",
        "Restarting Bluetooth...": "Reiniciando Bluetooth...",
        "Selected Car": "Coche seleccionado",
        "Selected Cars (%1)": "Coches seleccionados (%1)",
    },
    "et": {
        "Hotspot problem": "Kuumkoha probleem",
        "Recovering Bluetooth subsystem...": "Bluetoothi alamsüsteemi taastamine...",
        "Restarting Bluetooth...": "Bluetoothi taaskäivitus...",
        "Selected Car": "Valitud auto",
        "Selected Cars (%1)": "Valitud autod (%1)",
    },
    "eu": {
        "Hotspot problem": "Hotspotaren arazoa",
        "Recovering Bluetooth subsystem...": "Bluetooth azpisistema berreskuratzen...",
        "Restarting Bluetooth...": "Bluetooth berrabiarazten...",
        "Selected Car": "Hautatako autoa",
        "Selected Cars (%1)": "Hautatako autoak (%1)",
    },
    "fi": {
        "Hotspot problem": "Yhteyspisteen ongelma",
        "Recovering Bluetooth subsystem...": "Palautetaan Bluetooth-alajärjestelmää...",
        "Restarting Bluetooth...": "Käynnistetään Bluetooth uudelleen...",
        "Selected Car": "Valittu auto",
        "Selected Cars (%1)": "Valitut autot (%1)",
    },
    "fr": {
        "Hotspot problem": "Problème du point d'accès",
        "Recovering Bluetooth subsystem...": "Récupération du sous-système Bluetooth...",
        "Restarting Bluetooth...": "Redémarrage du Bluetooth...",
        "Selected Car": "Voiture sélectionnée",
        "Selected Cars (%1)": "Voitures sélectionnées (%1)",
    },
    "hu": {
        "Hotspot problem": "Hotspot-probléma",
        "Recovering Bluetooth subsystem...": "Bluetooth alrendszer helyreállítása...",
        "Restarting Bluetooth...": "Bluetooth újraindítása...",
        "Selected Car": "Kiválasztott autó",
        "Selected Cars (%1)": "Kiválasztott autók (%1)",
    },
    "it": {
        "Hotspot problem": "Problema hotspot",
        "Recovering Bluetooth subsystem...": "Ripristino sottosistema Bluetooth...",
        "Restarting Bluetooth...": "Riavvio di Bluetooth...",
        "Selected Car": "Auto selezionata",
        "Selected Cars (%1)": "Auto selezionate (%1)",
    },
    "ja": {
        "Hotspot problem": "テザリングの問題",
        "Recovering Bluetooth subsystem...": "Bluetoothサブシステムを復旧中...",
        "Restarting Bluetooth...": "Bluetoothを再起動中...",
        "Selected Car": "選択した車",
        "Selected Cars (%1)": "選択した車 (%1)",
    },
    "lt": {
        "Hotspot problem": "Prieigos taško problema",
        "Recovering Bluetooth subsystem...": "Atkuriama Bluetooth posistemė...",
        "Restarting Bluetooth...": "Iš naujo paleidžiama Bluetooth...",
        "Selected Car": "Pasirinktas automobilis",
        "Selected Cars (%1)": "Pasirinkti automobiliai (%1)",
    },
    "lv": {
        "Hotspot problem": "Tīklāja problēma",
        "Recovering Bluetooth subsystem...": "Atjauno Bluetooth apakšsistēmu...",
        "Restarting Bluetooth...": "Pārstartē Bluetooth...",
        "Selected Car": "Izvēlētais auto",
        "Selected Cars (%1)": "Izvēlētie auto (%1)",
    },
    "nb_NO": {
        "Hotspot problem": "Problem med hotspot",
        "Recovering Bluetooth subsystem...": "Gjenoppretter Bluetooth-delsystem...",
        "Restarting Bluetooth...": "Starter Bluetooth på nytt...",
        "Selected Car": "Valgt bil",
        "Selected Cars (%1)": "Valgte biler (%1)",
    },
    "nl": {
        "Hotspot problem": "Hotspot-probleem",
        "Recovering Bluetooth subsystem...": "Bluetooth-subsysteem herstellen...",
        "Restarting Bluetooth...": "Bluetooth herstarten...",
        "Selected Car": "Geselecteerde auto",
        "Selected Cars (%1)": "Geselecteerde auto's (%1)",
    },
    "no": {
        "Hotspot problem": "Problem med hotspot",
        "Recovering Bluetooth subsystem...": "Gjenoppretter Bluetooth-delsystem...",
        "Restarting Bluetooth...": "Starter Bluetooth på nytt...",
        "Selected Car": "Valgt bil",
        "Selected Cars (%1)": "Valgte biler (%1)",
    },
    "pl": {
        "Hotspot problem": "Problem hotspotu",
        "Recovering Bluetooth subsystem...": "Przywracanie podsystemu Bluetooth...",
        "Restarting Bluetooth...": "Restartowanie Bluetooth...",
        "Selected Car": "Wybrany samochód",
        "Selected Cars (%1)": "Wybrane samochody (%1)",
    },
    "pt": {
        "Hotspot problem": "Problema do hotspot",
        "Recovering Bluetooth subsystem...": "A recuperar o subsistema Bluetooth...",
        "Restarting Bluetooth...": "A reiniciar o Bluetooth...",
        "Selected Car": "Carro selecionado",
        "Selected Cars (%1)": "Carros selecionados (%1)",
    },
    "pt_BR": {
        "Hotspot problem": "Problema do hotspot",
        "Recovering Bluetooth subsystem...": "Recuperando o subsistema Bluetooth...",
        "Restarting Bluetooth...": "Reiniciando o Bluetooth...",
        "Selected Car": "Carro selecionado",
        "Selected Cars (%1)": "Carros selecionados (%1)",
    },
    "ro": {
        "Hotspot problem": "Problemă hotspot",
        "Recovering Bluetooth subsystem...": "Recuperare subsistem Bluetooth...",
        "Restarting Bluetooth...": "Repornire Bluetooth...",
        "Selected Car": "Mașină selectată",
        "Selected Cars (%1)": "Mașini selectate (%1)",
    },
    "ru": {
        "Hotspot problem": "Проблема точки доступа",
        "Recovering Bluetooth subsystem...": "Восстановление подсистемы Bluetooth...",
        "Restarting Bluetooth...": "Перезапуск Bluetooth...",
        "Selected Car": "Выбранный автомобиль",
        "Selected Cars (%1)": "Выбранные автомобили (%1)",
    },
    "sk": {
        "Hotspot problem": "Problém s hotspotom",
        "Recovering Bluetooth subsystem...": "Obnovovanie subsystému Bluetooth...",
        "Restarting Bluetooth...": "Reštartovanie Bluetooth...",
        "Selected Car": "Vybrané auto",
        "Selected Cars (%1)": "Vybrané autá (%1)",
    },
    "sl": {
        "Hotspot problem": "Težava z dostopno točko",
        "Recovering Bluetooth subsystem...": "Obnovovanje podsistema Bluetooth...",
        "Restarting Bluetooth...": "Znova zaganjanje Bluetooth...",
        "Selected Car": "Izbrani avtomobil",
        "Selected Cars (%1)": "Izbrani avtomobili (%1)",
    },
    "sr": {
        "Hotspot problem": "Проблем са хотспотом",
        "Recovering Bluetooth subsystem...": "Враћам Bluetooth подсистем...",
        "Restarting Bluetooth...": "Поново покретам Bluetooth...",
        "Selected Car": "Изабрано ауто",
        "Selected Cars (%1)": "Изабрана аута (%1)",
    },
    "sv": {
        "Hotspot problem": "Hotspotproblem",
        "Recovering Bluetooth subsystem...": "Återställer Bluetooth-delsystem...",
        "Restarting Bluetooth...": "Startar om Bluetooth...",
        "Selected Car": "Vald bil",
        "Selected Cars (%1)": "Valda bilar (%1)",
    },
    "tr": {
        "Hotspot problem": "Hotspot sorunu",
        "Recovering Bluetooth subsystem...": "Bluetooth alt sistemi onarılıyor...",
        "Restarting Bluetooth...": "Bluetooth yeniden başlatılıyor...",
        "Selected Car": "Seçili araç",
        "Selected Cars (%1)": "Seçili araçlar (%1)",
    },
    "uk": {
        "Hotspot problem": "Проблема точки доступу",
        "Recovering Bluetooth subsystem...": "Відновлення підсистеми Bluetooth...",
        "Restarting Bluetooth...": "Перезапуск Bluetooth...",
        "Selected Car": "Вибраний автомобіль",
        "Selected Cars (%1)": "Вибрані автомобілі (%1)",
    },
    "zh_CN": {
        "Hotspot problem": "热点问题",
        "Recovering Bluetooth subsystem...": "正在恢复蓝牙子系统...",
        "Restarting Bluetooth...": "正在重启蓝牙...",
        "Selected Car": "已选车辆",
        "Selected Cars (%1)": "已选车辆 (%1)",
    },
    "zh_TW": {
        "Hotspot problem": "熱點問題",
        "Recovering Bluetooth subsystem...": "正在復原藍牙子系統...",
        "Restarting Bluetooth...": "正在重啟藍牙...",
        "Selected Car": "已選車輛",
        "Selected Cars (%1)": "已選車輛 (%1)",
    },
}


def context_block(text: str, name: str):
    """Return (start, end, inner) of a <context> block with the given <name>."""
    for m in re.finditer(r"<context>\s*<name>%s</name>.*?</context>" % re.escape(name), text, re.S):
        return m.start(), m.end()
    return None


def add_missing(path: str) -> int:
    lang = os.path.basename(path)[len("harbour-carhotspot-"):-len(".ts")]
    table = TRANSLATIONS.get(lang)
    if table is None:
        print("!! no translation table for language %r (%s)" % (lang, path))
        return 0

    text = open(path, encoding="utf-8").read()
    added = 0

    for ctx, source in STRINGS:
        esc_source = escape(source)
        translation = table[source]

        block = context_block(text, ctx)
        if block is None:
            print("!! no context %r in %s" % (ctx, path))
            continue

        start, end = block
        inner = text[start:end]
        if re.search(r"<source>%s</source>" % re.escape(esc_source), inner):
            continue

        message = (
            "    <message>\n"
            "        <source>%s</source>\n"
            "        <translation>%s</translation>\n"
            "    </message>\n"
        ) % (esc_source, escape(translation))

        insert_at = inner.rfind("</context>")
        text = text[: start + insert_at] + message + text[start + insert_at:]
        added += 1

    if added:
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
    return added


def main() -> int:
    project = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    files = sorted(glob.glob(os.path.join(project, "translations", "*.ts")))
    if not files:
        print("No .ts files found")
        return 1

    total = 0
    for f in files:
        n = add_missing(f)
        total += n
        print("%-40s +%d entries" % (os.path.basename(f), n))
    print("total inserted:", total)
    return 0


if __name__ == "__main__":
    sys.exit(main())

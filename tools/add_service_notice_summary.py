#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Adds the notification summary used by the Harbour build notice to every .ts file."""

import io
import os

SOURCE = "Background automation needs the OpenRepos version"

T = {
    "ar": "الأتمتة في الخلفية تتطلب نسخة OpenRepos",
    "bg": "Автоматизация във фонов режим изисква версията за OpenRepos",
    "ca": "L'automatització en segon pla requereix la versió d'OpenRepos",
    "cs": "Automatizace na pozadí vyžaduje verzi pro OpenRepos",
    "da": "Baggrundsautomatisering kræver OpenRepos-versionen",
    "de": "Hintergrund-Automatisierung erfordert die OpenRepos-Version",
    "el": "Ο αυτοματισμός στο παρασκήνιο απαιτεί την έκδοση OpenRepos",
    "en": SOURCE,
    "es": "La automatización en segundo plano requiere la versión de OpenRepos",
    "et": "Tausta automatiseerimine nõuab OpenReposi versiooni",
    "eu": "Atzeko automatizazioak OpenRepos bertsioa eskatzen du",
    "fi": "Tausta-automaatio vaatii OpenRepos-version",
    "fr": "L'automatisation en arrière-plan nécessite la version OpenRepos",
    "hu": "A háttér automatizálás az OpenRepos-verziót igényli",
    "it": "L'automazione in background richiede la versione OpenRepos",
    "ja": "バックグラウンドの自動機能には OpenRepos 版が必要です",
    "lt": "Fono automatizacijai reikia OpenRepos versijos",
    "lv": "Fona automatizācijai nepieciešama OpenRepos versija",
    "nb_NO": "Bakgrundsautomatisering krever OpenRepos-versjonen",
    "nl": "Automatisering op de achtergrond vereist de OpenRepos-versie",
    "no": "Bakgrundsautomatisering krever OpenRepos-versjonen",
    "pl": "Automatyzacja w tle wymaga wersji OpenRepos",
    "pt": "A automação em segundo plano requer a versão OpenRepos",
    "pt_BR": "A automação em segundo plano requer a versão OpenRepos",
    "ro": "Automatizarea în fundal necesită versiunea OpenRepos",
    "ru": "Фоновая автоматизация требует версию для OpenRepos",
    "sk": "Automatizácia na pozadí vyžaduje verziu pre OpenRepos",
    "sl": "Avtomatizacija v ozadju potrebuje različico OpenRepos",
    "sr": "Аутоматизација у позадини захтева OpenRepos верзију",
    "sv": "Bakgrundsautomation kräver OpenRepos-versionen",
    "tr": "Arka plan otomasyonu OpenRepos sürümünü gerektirir",
    "uk": "Фоновна автоматизація потребує версії для OpenRepos",
    "zh_CN": "后台自动化需要 OpenRepos 版本",
    "zh_TW": "背景自動化需要 OpenRepos 版本",
}


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ts_dir = os.path.join(root, "translations")
    updated = []
    for name in sorted(os.listdir(ts_dir)):
        if not name.endswith(".ts"):
            continue
        lang = name[len("harbour-carhotspot-"):-len(".ts")]
        if lang not in T:
            print("no translation for:", lang)
            continue
        path = os.path.join(ts_dir, name)
        with io.open(path, encoding="utf-8") as fh:
            content = fh.read()
        if SOURCE in content:
            continue
        marker = "<name>AppController</name>"
        start = content.index(marker)
        end = content.index("</context>", start)
        entry = ("    <message>\n        <source>%s</source>\n"
                 "        <translation>%s</translation>\n    </message>\n"
                 % (SOURCE, T[lang]))
        content = content[:end] + entry + content[end:]
        with io.open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(content)
        updated.append(lang)
    print("updated %d files" % len(updated))


if __name__ == "__main__":
    main()

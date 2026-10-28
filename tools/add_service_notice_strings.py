#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Adds the background-service / OpenRepos notice strings to every .ts file.

The five strings below were introduced together with the two distribution flavours
(Harbour build without a service, OpenRepos build with a sandboxed user service).
lupdate is not available outside the Sailfish SDK, so the entries are inserted
directly into the context blocks. Every language keeps the same application id, and
the literal systemctl command is left untranslated on purpose.
"""

import io
import os
import sys

S1 = "Background automation is available in the OpenRepos version"
S2 = ("This version monitors the car while the app is running (also in background). "
      "The OpenRepos version adds a user service that starts on boot and works after "
      "the app is closed.")
S3 = "Open OpenRepos page"
S4 = ("User service failed to start. Install it manually with: "
      "systemctl --user enable --now harbour-carhotspot.service")
S5 = "Open Car Hotspot"

SOURCES = {S1: "MainPage ToolsPage", S2: "AppController MainPage ToolsPage",
           S3: "MainPage ToolsPage", S4: "AppController", S5: "AppController"}

T = {
    "ar": [
        "الأتمتة في الخلفية متوفرة في نسخة OpenRepos",
        "يتابع هذا الإصدار السيارة طالما التطبيق يعمل (في الخلفية أيضًا). يضيف إصدار OpenRepos خدمة مستخدم تبدأ عند تشغيل الهاتف وتعمل بعد إغلاق التطبيق.",
        "فتح صفحة OpenRepos",
        "تعذّر بدء خدمة المستخدم. ثبّتها يدويًا بالأمر: systemctl --user enable --now harbour-carhotspot.service",
        "فتح Car Hotspot",
    ],
    "bg": [
        "Автоматизация във фонов режим е налична в версията за OpenRepos",
        "Тази версия следя колата, докато приложението работи (и във фонов режим). Версията за OpenRepos добавя потребителска услуга, която се стартира при включване на телефона и работи след затваряне на приложението.",
        "Отваряне на страницата в OpenRepos",
        "Потребителската услуга не можа да се стартира. Инсталирайте я ръчно с: systemctl --user enable --now harbour-carhotspot.service",
        "Отваряне на Car Hotspot",
    ],
    "ca": [
        "L'automatització en segon pla és disponible a la versió d'OpenRepos",
        "Aquesta versió supervisa el cotxe mentre l'aplicació s'executa (també en segon pla). La versió d'OpenRepos afegeix un servei d'usuari que s'inicia en arrencar el telèfon i funciona després de tancar l'aplicació.",
        "Obre la pàgina d'OpenRepos",
        "El servei d'usuari no s'ha pogut iniciar. Instal·leu-lo manualment amb: systemctl --user enable --now harbour-carhotspot.service",
        "Obre Car Hotspot",
    ],
    "cs": [
        "Automatizace na pozadí je dostupná v verzi pro OpenRepos",
        "Tato verze sleduje auto, zatímco aplikace běží (i na pozadí). Verze pro OpenRepos přidá uživatelskou službu, která se spouští při startu telefonu a funguje i po zavření aplikace.",
        "Otevřít stránku OpenRepos",
        "Uživatelskou službu nelze spustit. Nainstalujte ji ručně: systemctl --user enable --now harbour-carhotspot.service",
        "Otevřít Car Hotspot",
    ],
    "da": [
        "Baggrundsautomatisering er tilgængelig i OpenRepos-versionen",
        "Denne version overvåger bilen, mens appen kører (også i baggrunden). OpenRepos-versionen tilføjer en brugertjeneste, der starter ved opstart og virker, efter appen er lukket.",
        "Åbn OpenRepos-siden",
        "Brugertjenesten kunne ikke startes. Installér den manuelt med: systemctl --user enable --now harbour-carhotspot.service",
        "Åbn Car Hotspot",
    ],
    "de": [
        "Hintergrund-Automatisierung ist in der OpenRepos-Version verfügbar",
        "Diese Version überwacht das Auto, solange die App läuft (auch im Hintergrund). Die OpenRepos-Version ergänzt einen Nutzerdienst, der beim Start des Telefons aktiv wird und auch nach dem Schließen der App funktioniert.",
        "OpenRepos-Seite öffnen",
        "Nutzerdienst konnte nicht gestartet werden. Manuell installieren mit: systemctl --user enable --now harbour-carhotspot.service",
        "Car Hotspot öffnen",
    ],
    "el": [
        "Ο αυτοματισμός στο παρασκήνιο είναι διαθέσιμος στην έκδοση OpenRepos",
        "Αυτή η έκδοση παρακολουθεί το αυτοκίνητο όσο η εφαρμογή εκτελείται (και στο παρασκήνιο). Η έκδοση OpenRepos προσθέτει μια υπηρεσία χρήστη που ξεκινά με την εκκίνηση του τηλεφώνου και λειτουργεί μετά το κλείσιμο της εφαρμογής.",
        "Άνοιγμα σελίδας OpenRepos",
        "Η υπηρεσία χρήστη δεν μπόρεσε να ξεκινήσει. Εγκαταστήστε τη χειροκίνητα με: systemctl --user enable --now harbour-carhotspot.service",
        "Άνοιγμα Car Hotspot",
    ],
    "en": [S1, S2, S3, S4, S5],
    "es": [
        "La automatización en segundo plano está disponible en la versión de OpenRepos",
        "Esta versión monitoriza el coche mientras la app se ejecuta (también en segundo plano). La versión de OpenRepos añade un servicio de usuario que se inicia al arrancar el teléfono y funciona después de cerrar la app.",
        "Abrir la página de OpenRepos",
        "El servicio de usuario no pudo iniciarse. Instálalo manualmente con: systemctl --user enable --now harbour-carhotspot.service",
        "Abrir Car Hotspot",
    ],
    "et": [
        "Tausta automatiseerimine on saadaval OpenReposi versioonis",
        "See versioon jälgib autot, rakenduse töös olles (ka taustal). OpenReposi versioon lisab kasutajateenuse, mis käivitub telefoni käivitamisel ja töötab pärast rakenduse sulgemist.",
        "Ava OpenReposi leht",
        "Kasutajateenust ei õnnestunud käivitada. Paigalda käsitsi: systemctl --user enable --now harbour-carhotspot.service",
        "Ava Car Hotspot",
    ],
    "eu": [
        "Atzeko automatizazioa OpenRepos bertsioan dago erabilgarri",
        "Bertsio honek autoa zaintzen du aplikazioa martuan dagoen bitartean (atzekuan ere). OpenRepos bertsioak erabiltzaile-zerbitzu bat gehitzen du, telefonoa abiaraztean hasten dena eta aplikazioa itxi ondoren ere funtzionatzen duena.",
        "Ireki OpenRepos orria",
        "Erabiltzaile-zerbitzua ezin izan da abiarazi. Eskuarki instalatu: systemctl --user enable --now harbour-carhotspot.service",
        "Ireki Car Hotspot",
    ],
    "fi": [
        "Tausta-automaatio on saatavilla OpenRepos-versiossa",
        "Tämä versio valvoo autoa sovelluksen ollessa käynnissä (myös taustalla). OpenRepos-versio lisää käyttäjäpalvelun, joka käynnistyy puhelimen käynnistyessä ja toimii sovelluksen sulkemisen jälkeen.",
        "Avaa OpenRepos-sivu",
        "Käyttäjäpalvelua ei voitu käynnistää. Asenna se käsin: systemctl --user enable --now harbour-carhotspot.service",
        "Avaa Car Hotspot",
    ],
    "fr": [
        "L'automatisation en arrière-plan est disponible dans la version OpenRepos",
        "Cette version surveille la voiture tant que l'application s'exécute (y compris en arrière-plan). La version OpenRepos ajoute un service utilisateur qui démarre au démarrage du téléphone et fonctionne après la fermeture de l'application.",
        "Ouvrir la page OpenRepos",
        "Le service utilisateur n'a pas pu démarrer. Installer manuellement avec : systemctl --user enable --now harbour-carhotspot.service",
        "Ouvrir Car Hotspot",
    ],
    "hu": [
        "A háttér automatizálás az OpenRepos-verzióban érhető el",
        "Ez a verzió figyeli az autót, amíg az alkalmazás fut (a háttérben is). Az OpenRepos-verzió felhasználói szolgáltatást ad hozzá, amely a telefon indításakor elindul, és az alkalmazás bezárása után is működik.",
        "OpenRepos oldal megnyitása",
        "A felhasználói szolgáltatás nem indítható el. Telepítse kézzel: systemctl --user enable --now harbour-carhotspot.service",
        "Car Hotspot megnyitása",
    ],
    "it": [
        "L'automazione in background è disponibile nella versione OpenRepos",
        "Questa versione monitora l'auto mentre l'app è in esecuzione (anche in background). La versione OpenRepos aggiunge un servizio utente che si avvia all'accensione del telefono e funziona dopo la chiusura dell'app.",
        "Apri la pagina OpenRepos",
        "Impossibile avviare il servizio utente. Installalo manualmente con: systemctl --user enable --now harbour-carhotspot.service",
        "Apri Car Hotspot",
    ],
    "ja": [
        "バックグラウンドの自動機能は OpenRepos 版で利用できます",
        "この版はアプリが実行中（バックグラウンドを含む）の間、車を監視します。OpenRepos 版では、端末の起動時に開始し、アプリを閉じた後も動作するユーザーサービスが追加されます。",
        "OpenRepos のページを開く",
        "ユーザーサービスを開始できませんでした。次を手動で実行してインストールしてください: systemctl --user enable --now harbour-carhotspot.service",
        "Car Hotspot を開く",
    ],
    "lt": [
        "Fono automatizacija yra prieinama OpenRepos versijoje",
        "Ši versija stebi automobilį, kol programa veikia (taip pat fone). OpenRepos versija prideda vartotojo paslaugą, kuri paleidžiama įjungus telefoną ir veikia uždarius programą.",
        "Atidaryti OpenRepos puslapį",
        "Vartotojo paslaugos nepavyko paleisti. Įdiekite rankiniu būdu: systemctl --user enable --now harbour-carhotspot.service",
        "Atidaryti Car Hotspot",
    ],
    "lv": [
        "Fona automatizācija ir pieejama OpenRepos versijā",
        "Šī versija uzrauga auto, kamēr lietotne darbojas (arī fona režīmā). OpenRepos versija pievieno lietotāja servisu, kas startē līdz ar telefonu un darbojas pēc lietotnes aizvēršanas.",
        "Atvērt OpenRepos lapu",
        "Lietotāja servisu nevarēja startēt. Instalējiet manuāli: systemctl --user enable --now harbour-carhotspot.service",
        "Atvērt Car Hotspot",
    ],
    "nb_NO": [
        "Bakgrundsautomatisering er tilgjengelig i OpenRepos-versjonen",
        "Denne versjonen overvåker bilen mens appen kjører (også i bakgrunnen). OpenRepos-versjonen legger til en brukertjeneste som starter ved oppstart og fungerer etter at appen er lukket.",
        "Åpne OpenRepos-siden",
        "Brukertjenesten kunne ikke startes. Installer den manuelt med: systemctl --user enable --now harbour-carhotspot.service",
        "Åpne Car Hotspot",
    ],
    "nl": [
        "Automatisering op de achtergrond is beschikbaar in de OpenRepos-versie",
        "Deze versie bewaakt de auto zolang de app draait (ook op de achtergrond). De OpenRepos-versie voegt een gebruikersdienst toe die bij het opstarten actief wordt en werkt nadat de app is gesloten.",
        "OpenRepos-pagina openen",
        "Gebruikersdienst kon niet worden gestart. Installeer handmatig met: systemctl --user enable --now harbour-carhotspot.service",
        "Car Hotspot openen",
    ],
    "no": [
        "Bakgrundsautomatisering er tilgjengelig i OpenRepos-versjonen",
        "Denne versjonen overvåker bilen mens appen kjører (også i bakgrunnen). OpenRepos-versjonen legger til en brukertjeneste som starter ved oppstart og fungerer etter at appen er lukket.",
        "Åpne OpenRepos-siden",
        "Brukertjenesten kunne ikke startes. Installer den manuelt med: systemctl --user enable --now harbour-carhotspot.service",
        "Åpne Car Hotspot",
    ],
    "pl": [
        "Automatyzacja w tle jest dostępna w wersji OpenRepos",
        "Ta wersja nadzoruje samochód, gdy aplikacja działa (także w tle). Wersja OpenRepos dodaje usługę użytkownika, która uruchamia się przy starcie telefonu i działa po zamknięciu aplikacji.",
        "Otwórz stronę OpenRepos",
        "Nie udało się uruchomić usługi użytkownika. Zainstaluj ją ręcznie: systemctl --user enable --now harbour-carhotspot.service",
        "Otwórz Car Hotspot",
    ],
    "pt": [
        "A automação em segundo plano está disponível na versão OpenRepos",
        "Esta versão monitoriza o carro enquanto a aplicação está em execução (também em segundo plano). A versão OpenRepos adiciona um serviço de utilizador que inicia no arranque e funciona depois de a aplicação ser fechada.",
        "Abrir a página do OpenRepos",
        "O serviço de utilizador não pôde ser iniciado. Instale-o manualmente com: systemctl --user enable --now harbour-carhotspot.service",
        "Abrir Car Hotspot",
    ],
    "pt_BR": [
        "A automação em segundo plano está disponível na versão OpenRepos",
        "Esta versão monitora o carro enquanto o aplicativo está em execução (também em segundo plano). A versão OpenRepos adiciona um serviço de usuário que inicia na inicialização e funciona depois que o aplicativo é fechado.",
        "Abrir a página do OpenRepos",
        "O serviço de usuário não pôde ser iniciado. Instale-o manualmente com: systemctl --user enable --now harbour-carhotspot.service",
        "Abrir Car Hotspot",
    ],
    "ro": [
        "Automatizarea în fundal este disponibilă în versiunea OpenRepos",
        "Această versiune monitorizează mașina cât timp aplicația rulează (și în fundal). Versiunea OpenRepos adaugă un serviciu de utilizator care pornește la pornirea telefonului și funcționează după închiderea aplicației.",
        "Deschide pagina OpenRepos",
        "Serviciul de utilizator nu a putut fi pornit. Instalați-l manual cu: systemctl --user enable --now harbour-carhotspot.service",
        "Deschide Car Hotspot",
    ],
    "ru": [
        "Фоновая автоматизация доступна в версии для OpenRepos",
        "Эта версия следит за автомобилем, пока приложение запущено (в том числе в фоне). Версия для OpenRepos добавляет пользовательскую службу, которая запускается при загрузке телефона и работает после закрытия приложения.",
        "Открыть страницу в OpenRepos",
        "Не удалось запустить пользовательскую службу. Установите её вручную: systemctl --user enable --now harbour-carhotspot.service",
        "Открыть Car Hotspot",
    ],
    "sk": [
        "Automatizácia na pozadí je dostupná vo verzii pre OpenRepos",
        "Táto verzia sleduje auto, kým je aplikácia spustená (aj na pozadí). Verzia pre OpenRepos pridáva používateľskú službu, ktorá sa spúšťa pri starte telefónu a funguje po zatvorení aplikácie.",
        "Otvoriť stránku OpenRepos",
        "Používateľskú službu sa nepodarilo spustiť. Inštalujte ju ručne: systemctl --user enable --now harbour-carhotspot.service",
        "Otvoriť Car Hotspot",
    ],
    "sl": [
        "Avtomatizacija v ozadju je na voljo v različici OpenRepos",
        "Ta različica spremlja avto, medtem ko je aplikacija zagnana (tudi v ozadju). Različica OpenRepos doda uporabniško storitev, ki se zagn ob zagonu telefona in deluje po zaprtju aplikacije.",
        "Odpri stran OpenRepos",
        "Uporabniške storitve ni bilo mogoče zagnati. Namestite jo ročno: systemctl --user enable --now harbour-carhotspot.service",
        "Odpri Car Hotspot",
    ],
    "sr": [
        "Аутоматизација у позадини је доступна у OpenRepos верзији",
        "Ова верзија прати ауто док је апликација покренута (и у позадини). OpenRepos верзија додаје корисничку службу која се покреће при покретању телефона и ради након затварања апликације.",
        "Отвори OpenRepos страницу",
        "Корисничка служба се није покренула. Инсталирајте је ручно: systemctl --user enable --now harbour-carhotspot.service",
        "Отвори Car Hotspot",
    ],
    "sv": [
        "Bakgrundsautomation finns i OpenRepos-versionen",
        "Denna version övervakar bilen medan appen körs (även i bakgrunden). OpenRepos-versionen lägger till en tjänst som startar vid omstart och fungerar efter att appen stängts.",
        "Öppna OpenRepos-sidan",
        "Tjänsten kunde inte startas. Installera den manuellt med: systemctl --user enable --now harbour-carhotspot.service",
        "Öppna Car Hotspot",
    ],
    "tr": [
        "Arka plan otomasyonu OpenRepos sürümünde kullanılabilir",
        "Bu sürüm, uygulama çalışırken (arka planda dahil) aracı izler. OpenRepos sürümü, telefon açıldığında başlayan ve uygulama kapatıldıktan sonra da çalışan bir kullanıcı hizmeti ekler.",
        "OpenRepos sayfasını aç",
        "Kullanıcı hizmeti başlatılamadı. Elle kurun: systemctl --user enable --now harbour-carhotspot.service",
        "Car Hotspot'u aç",
    ],
    "uk": [
        "Фоновна автоматизація доступна у версії для OpenRepos",
        "Ця версія стежить за автомобілем, поки додаток працює (зокрема у фоні). Версія для OpenRepos додає користувацьку службу, яка запускається при старті телефону та працює після закриття додатка.",
        "Відкрити сторінку OpenRepos",
        "Не вдалося запустити користувацьку службу. Установіть вручну: systemctl --user enable --now harbour-carhotspot.service",
        "Відкрити Car Hotspot",
    ],
    "zh_CN": [
        "后台自动化在 OpenRepos 版本中可用",
        "此版本仅在应用运行时（含后台）监控车辆。OpenRepos 版本增加了随手机启动、并在应用关闭后仍工作的用户服务。",
        "打开 OpenRepos 页面",
        "用户服务启动失败。请手动安装：systemctl --user enable --now harbour-carhotspot.service",
        "打开 Car Hotspot",
    ],
    "zh_TW": [
        "背景自動化在 OpenRepos 版本中可用",
        "此版本僅在應用程式執行時（含背景）監控車輛。OpenRepos 版本新增隨手機啟動、且在應用程式關閉後仍可用的使用者服務。",
        "開啟 OpenRepos 頁面",
        "使用者服務啟動失敗。請手動安裝：systemctl --user enable --now harbour-carhotspot.service",
        "開啟 Car Hotspot",
    ],
}


def esc(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def insert(path, lang):
    with io.open(path, encoding="utf-8") as fh:
        content = fh.read()

    trs = T[lang]
    order = {S1: trs[0], S2: trs[1], S3: trs[2], S4: trs[3], S5: trs[4]}

    for source, contexts in SOURCES.items():
        for ctx in contexts.split():
            marker = '<name>%s</name>' % ctx
            start = content.index(marker)
            end = content.index("</context>", start)
            block = content[start:end]
            if esc(source) in block:
                continue
            entry = ("    <message>\n        <source>%s</source>\n"
                     "        <translation>%s</translation>\n    </message>\n"
                     % (esc(source), esc(order[source])))
            content = content[:end] + entry + content[end:]

    with io.open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(content)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ts_dir = os.path.join(root, "translations")
    done = []
    for name in sorted(os.listdir(ts_dir)):
        if not name.endswith(".ts"):
            continue
        lang = name[len("harbour-carhotspot-"):-len(".ts")]
        if lang not in T:
            print("no translations for language:", lang)
            continue
        insert(os.path.join(ts_dir, name), lang)
        done.append(lang)
    print("updated %d language files: %s" % (len(done), " ".join(done)))


if __name__ == "__main__":
    main()

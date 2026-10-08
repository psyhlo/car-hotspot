#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import glob
import os
import re
import xml.etree.ElementTree as ET

NEW_TRANSLATIONS = {
    'en': {
        'Tools & Diagnostics': 'Tools & Diagnostics',
        'Refresh Status': 'Refresh Status',
        'Bluetooth Subsystem Recovery': 'Bluetooth Subsystem Recovery',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.',
        'Restart Bluetooth Subsystem': 'Restart Bluetooth Subsystem',
        'Hotspot Manual Control': 'Hotspot Manual Control',
        'Current State: ': 'Current State: ',
        'Hotspot ACTIVE': 'Hotspot ACTIVE',
        'Hotspot INACTIVE': 'Hotspot INACTIVE',
        'Turn Hotspot OFF': 'Turn Hotspot OFF',
        'Turn Hotspot ON': 'Turn Hotspot ON',
        'System Diagnostics Log': 'System Diagnostics Log',
    },
    'de': {
        'Tools & Diagnostics': 'Werkzeuge & Diagnose',
        'Refresh Status': 'Status aktualisieren',
        'Bluetooth Subsystem Recovery': 'Bluetooth-Wiederherstellung',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Führt einen Low-Level-Reset des Bluetooth-Dienstes durch, hebt die rfkill-Sperre auf und initialisiert ConnMan neu.',
        'Restart Bluetooth Subsystem': 'Bluetooth-Subsystem neustarten',
        'Hotspot Manual Control': 'Manuelle Hotspot-Steuerung',
        'Current State: ': 'Aktueller Status: ',
        'Hotspot ACTIVE': 'Hotspot AKTIV',
        'Hotspot INACTIVE': 'Hotspot INAKTIV',
        'Turn Hotspot OFF': 'Hotspot AUS',
        'Turn Hotspot ON': 'Hotspot EIN',
        'System Diagnostics Log': 'Systemdiagnose-Protokoll',
    },
    'ru': {
        'Tools & Diagnostics': 'Инструменты и диагностика',
        'Refresh Status': 'Обновить статус',
        'Bluetooth Subsystem Recovery': 'Восстановление Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Выполняет низкоуровневый сброс демона Bluetooth, снимает блокировку rfkill и реинициализирует ConnMan.',
        'Restart Bluetooth Subsystem': 'Перезапустить службу Bluetooth',
        'Hotspot Manual Control': 'Ручное управление точкой доступа',
        'Current State: ': 'Текущее состояние: ',
        'Hotspot ACTIVE': 'Точка доступа АКТИВНА',
        'Hotspot INACTIVE': 'Точка доступа НЕАКТИВНА',
        'Turn Hotspot OFF': 'Выключить точку доступа',
        'Turn Hotspot ON': 'Включить точку доступа',
        'System Diagnostics Log': 'Журнал диагностики системы',
    },
    'fr': {
        'Tools & Diagnostics': 'Outils & Diagnostics',
        'Refresh Status': 'Actualiser l\'état',
        'Bluetooth Subsystem Recovery': 'Récupération du Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Effectue une réinitialisation de bas niveau du démon Bluetooth, débloque l\'état rfkill et réinitialise ConnMan.',
        'Restart Bluetooth Subsystem': 'Redémarrer le sous-système Bluetooth',
        'Hotspot Manual Control': 'Contrôle manuel du point d\'accès',
        'Current State: ': 'État actuel : ',
        'Hotspot ACTIVE': 'Point d\'accès ACTIF',
        'Hotspot INACTIVE': 'Point d\'accès INACTIF',
        'Turn Hotspot OFF': 'Désactiver le point d\'accès',
        'Turn Hotspot ON': 'Activer le point d\'accès',
        'System Diagnostics Log': 'Journal de diagnostic système',
    },
    'es': {
        'Tools & Diagnostics': 'Herramientas y diagnóstico',
        'Refresh Status': 'Actualizar estado',
        'Bluetooth Subsystem Recovery': 'Recuperación de Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Realiza un reinicio de bajo nivel del demonio Bluetooth, desbloquea el estado rfkill y reinicia ConnMan.',
        'Restart Bluetooth Subsystem': 'Reiniciar subsistema Bluetooth',
        'Hotspot Manual Control': 'Control manual del punto de acceso',
        'Current State: ': 'Estado actual: ',
        'Hotspot ACTIVE': 'Punto de acceso ACTIVO',
        'Hotspot INACTIVE': 'Punto de acceso INACTIVO',
        'Turn Hotspot OFF': 'Apagar punto de acceso',
        'Turn Hotspot ON': 'Encender punto de acceso',
        'System Diagnostics Log': 'Registro de diagnóstico del sistema',
    },
    'it': {
        'Tools & Diagnostics': 'Strumenti e diagnostica',
        'Refresh Status': 'Aggiorna stato',
        'Bluetooth Subsystem Recovery': 'Ripristino Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Esegue un riavvio di basso livello del demone Bluetooth, sblocca lo stato rfkill e reinizializza ConnMan.',
        'Restart Bluetooth Subsystem': 'Riavvia sottosistema Bluetooth',
        'Hotspot Manual Control': 'Controllo manuale hotspot',
        'Current State: ': 'Stato attuale: ',
        'Hotspot ACTIVE': 'Hotspot ATTIVO',
        'Hotspot INACTIVE': 'Hotspot INATTIVO',
        'Turn Hotspot OFF': 'Spegni Hotspot',
        'Turn Hotspot ON': 'Accendi Hotspot',
        'System Diagnostics Log': 'Registro diagnostica di sistema',
    },
    'pt': {
        'Tools & Diagnostics': 'Ferramentas e Diagnóstico',
        'Refresh Status': 'Atualizar estado',
        'Bluetooth Subsystem Recovery': 'Recuperação do Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Executa uma reinicialização de baixo nível do daemon Bluetooth, desbloqueia o rfkill e reinicializa o ConnMan.',
        'Restart Bluetooth Subsystem': 'Reiniciar subsistema Bluetooth',
        'Hotspot Manual Control': 'Controlo manual do hotspot',
        'Current State: ': 'Estado atual: ',
        'Hotspot ACTIVE': 'Hotspot ATIVO',
        'Hotspot INACTIVE': 'Hotspot INATIVO',
        'Turn Hotspot OFF': 'Desligar Hotspot',
        'Turn Hotspot ON': 'Ligar Hotspot',
        'System Diagnostics Log': 'Registo de diagnóstico do sistema',
    },
    'pt_BR': {
        'Tools & Diagnostics': 'Ferramentas e Diagnóstico',
        'Refresh Status': 'Atualizar estado',
        'Bluetooth Subsystem Recovery': 'Recuperação do Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Executa uma reinicialização de baixo nível do daemon Bluetooth, desbloqueia o rfkill e reinicializa o ConnMan.',
        'Restart Bluetooth Subsystem': 'Reiniciar subsistema Bluetooth',
        'Hotspot Manual Control': 'Controle manual do hotspot',
        'Current State: ': 'Estado atual: ',
        'Hotspot ACTIVE': 'Hotspot ATIVO',
        'Hotspot INACTIVE': 'Hotspot INATIVO',
        'Turn Hotspot OFF': 'Desligar Hotspot',
        'Turn Hotspot ON': 'Ligar Hotspot',
        'System Diagnostics Log': 'Registro de diagnóstico do sistema',
    },
    'fi': {
        'Tools & Diagnostics': 'Työkalut ja diagnostiikka',
        'Refresh Status': 'Päivitä tila',
        'Bluetooth Subsystem Recovery': 'Bluetooth-palautus',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Suorittaa matalan tason Bluetooth-palautuksen, poistaa rfkill-eston ja alustaa ConnManin uudelleen.',
        'Restart Bluetooth Subsystem': 'Käynnistä Bluetooth uudelleen',
        'Hotspot Manual Control': 'Yhteyspisteen manuaalinen ohjaus',
        'Current State: ': 'Nykyinen tila: ',
        'Hotspot ACTIVE': 'Yhteyspiste AKTIIVINEN',
        'Hotspot INACTIVE': 'Yhteyspiste EI AKTIIVINEN',
        'Turn Hotspot OFF': 'Sammuta yhteyspiste',
        'Turn Hotspot ON': 'Käynnistä yhteyspiste',
        'System Diagnostics Log': 'Järjestelmädiagnostiikan loki',
    },
    'sv': {
        'Tools & Diagnostics': 'Verktyg och diagnostik',
        'Refresh Status': 'Uppdatera status',
        'Bluetooth Subsystem Recovery': 'Återställning av Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Utför en lågnivååterställning av Bluetooth-demonen, avblockerar rfkill och återinitierar ConnMan.',
        'Restart Bluetooth Subsystem': 'Starta om Bluetooth-delsystem',
        'Hotspot Manual Control': 'Manuell hotspotkontroll',
        'Current State: ': 'Aktuellt tillstånd: ',
        'Hotspot ACTIVE': 'Hotspot AKTIV',
        'Hotspot INACTIVE': 'Hotspot INAKTIV',
        'Turn Hotspot OFF': 'Stäng av hotspot',
        'Turn Hotspot ON': 'Slå på hotspot',
        'System Diagnostics Log': 'Systemdiagnostiklogg',
    },
    'da': {
        'Tools & Diagnostics': 'Værktøjer og diagnostik',
        'Refresh Status': 'Opdater status',
        'Bluetooth Subsystem Recovery': 'Gendannelse af Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Udfører en lavniveau-nulstilling af Bluetooth, fjerner rfkill-blokering og genindlæser ConnMan.',
        'Restart Bluetooth Subsystem': 'Genstart Bluetooth-delsystem',
        'Hotspot Manual Control': 'Manuel hotspot-styring',
        'Current State: ': 'Aktuel tilstand: ',
        'Hotspot ACTIVE': 'Hotspot AKTIV',
        'Hotspot INACTIVE': 'Hotspot INAKTIV',
        'Turn Hotspot OFF': 'Sluk hotspot',
        'Turn Hotspot ON': 'Tænd hotspot',
        'System Diagnostics Log': 'Systemdiagnostiklog',
    },
    'nb_NO': {
        'Tools & Diagnostics': 'Verktøy og diagnostikk',
        'Refresh Status': 'Oppdater status',
        'Bluetooth Subsystem Recovery': 'Gjenoppretting av Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Utfører en lavnivå-tilbakestilling av Bluetooth, fjerner rfkill-blokkering og starter ConnMan på nytt.',
        'Restart Bluetooth Subsystem': 'Start Bluetooth-delsystem på nytt',
        'Hotspot Manual Control': 'Manuell hotspot-kontroll',
        'Current State: ': 'Nåværende tilstand: ',
        'Hotspot ACTIVE': 'Hotspot AKTIV',
        'Hotspot INACTIVE': 'Hotspot INAKTIV',
        'Turn Hotspot OFF': 'Slå AV hotspot',
        'Turn Hotspot ON': 'Slå PÅ hotspot',
        'System Diagnostics Log': 'Systemdiagnostikklogg',
    },
    'no': {
        'Tools & Diagnostics': 'Verktøy og diagnostikk',
        'Refresh Status': 'Oppdater status',
        'Bluetooth Subsystem Recovery': 'Gjenoppretting av Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Utfører en lavnivå-tilbakestilling av Bluetooth, fjerner rfkill-blokkering og starter ConnMan på nytt.',
        'Restart Bluetooth Subsystem': 'Start Bluetooth-delsystem på nytt',
        'Hotspot Manual Control': 'Manuell hotspot-kontroll',
        'Current State: ': 'Nåværende tilstand: ',
        'Hotspot ACTIVE': 'Hotspot AKTIV',
        'Hotspot INACTIVE': 'Hotspot INAKTIV',
        'Turn Hotspot OFF': 'Slå AV hotspot',
        'Turn Hotspot ON': 'Slå PÅ hotspot',
        'System Diagnostics Log': 'Systemdiagnostikklogg',
    },
    'nl': {
        'Tools & Diagnostics': 'Gereedschappen & Diagnostiek',
        'Refresh Status': 'Status vernieuwen',
        'Bluetooth Subsystem Recovery': 'Herstel Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Voert een reset op laag niveau uit van Bluetooth, deblokkeert rfkill en initialiseert ConnMan opnieuw.',
        'Restart Bluetooth Subsystem': 'Herstart Bluetooth-subsysteem',
        'Hotspot Manual Control': 'Handmatige hotspotbediening',
        'Current State: ': 'Huidige status: ',
        'Hotspot ACTIVE': 'Hotspot ACTIEF',
        'Hotspot INACTIVE': 'Hotspot INACTIEF',
        'Turn Hotspot OFF': 'Hotspot UIT',
        'Turn Hotspot ON': 'Hotspot AAN',
        'System Diagnostics Log': 'Systeemdiagnoselog',
    },
    'pl': {
        'Tools & Diagnostics': 'Narzędzia i diagnostyka',
        'Refresh Status': 'Odśwież status',
        'Bluetooth Subsystem Recovery': 'Naprawa Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Wykonuje niskopoziomowy reset demona Bluetooth, odblokowuje stan rfkill i ponownie inicjalizuje ConnMan.',
        'Restart Bluetooth Subsystem': 'Zrestartuj podsystem Bluetooth',
        'Hotspot Manual Control': 'Ręczne sterowanie hotspotem',
        'Current State: ': 'Aktualny stan: ',
        'Hotspot ACTIVE': 'Hotspot AKTYWNY',
        'Hotspot INACTIVE': 'Hotspot NIEAKTYWNY',
        'Turn Hotspot OFF': 'Wyłącz Hotspot',
        'Turn Hotspot ON': 'Włącz Hotspot',
        'System Diagnostics Log': 'Dziennik diagnostyczny systemu',
    },
    'cs': {
        'Tools & Diagnostics': 'Nástroje a diagnostika',
        'Refresh Status': 'Obnovit stav',
        'Bluetooth Subsystem Recovery': 'Obnova Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Provádí nízkoúrovňový reset Bluetooth, odblokuje rfkill a znovu inicializuje ConnMan.',
        'Restart Bluetooth Subsystem': 'Restartovat subsystém Bluetooth',
        'Hotspot Manual Control': 'Ruční ovládání hotspotu',
        'Current State: ': 'Aktuální stav: ',
        'Hotspot ACTIVE': 'Hotspot AKTIVNÍ',
        'Hotspot INACTIVE': 'Hotspot NEAKTIVNÍ',
        'Turn Hotspot OFF': 'Vypnout Hotspot',
        'Turn Hotspot ON': 'Zapnout Hotspot',
        'System Diagnostics Log': 'Protokol diagnostiky systému',
    },
    'sk': {
        'Tools & Diagnostics': 'Nástroje a diagnostika',
        'Refresh Status': 'Obnoviť stav',
        'Bluetooth Subsystem Recovery': 'Obnova Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Vykonáva nízkoúrovňový reset Bluetooth, odblokuje rfkill a znovu inicializuje ConnMan.',
        'Restart Bluetooth Subsystem': 'Reštartovať subsystém Bluetooth',
        'Hotspot Manual Control': 'Ručné ovládanie hotspotu',
        'Current State: ': 'Aktuálny stav: ',
        'Hotspot ACTIVE': 'Hotspot AKTÍVNY',
        'Hotspot INACTIVE': 'Hotspot NEAKTÍVNY',
        'Turn Hotspot OFF': 'Vypnúť Hotspot',
        'Turn Hotspot ON': 'Zapnúť Hotspot',
        'System Diagnostics Log': 'Denník diagnostiky systému',
    },
    'uk': {
        'Tools & Diagnostics': 'Інструменти та діагностика',
        'Refresh Status': 'Оновити стан',
        'Bluetooth Subsystem Recovery': 'Відновлення Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Виконує низькорівневе скидання демона Bluetooth, розблоковує rfkill та переініціалізує ConnMan.',
        'Restart Bluetooth Subsystem': 'Перезапустити підсистему Bluetooth',
        'Hotspot Manual Control': 'Ручне керування точкою доступу',
        'Current State: ': 'Поточний стан: ',
        'Hotspot ACTIVE': 'Точка доступу АКТИВНА',
        'Hotspot INACTIVE': 'Точка доступу НЕАКТИВНА',
        'Turn Hotspot OFF': 'Вимкнути точку доступу',
        'Turn Hotspot ON': 'Увімкнути точку доступу',
        'System Diagnostics Log': 'Журнал діагностики системи',
    },
    'zh_CN': {
        'Tools & Diagnostics': '工具与诊断',
        'Refresh Status': '刷新状态',
        'Bluetooth Subsystem Recovery': '蓝牙子系统恢复',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': '执行蓝牙守护程序的底层重置，解除 rfkill 锁定并重新初始化 ConnMan。',
        'Restart Bluetooth Subsystem': '重启蓝牙子系统',
        'Hotspot Manual Control': '热点手动控制',
        'Current State: ': '当前状态：',
        'Hotspot ACTIVE': '热点 已激活',
        'Hotspot INACTIVE': '热点 未激活',
        'Turn Hotspot OFF': '关闭热点',
        'Turn Hotspot ON': '开启热点',
        'System Diagnostics Log': '系统诊断日志',
    },
    'zh_TW': {
        'Tools & Diagnostics': '工具與診斷',
        'Refresh Status': '重新整理狀態',
        'Bluetooth Subsystem Recovery': '藍牙子系統復原',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': '執行藍牙守護程式的底層重設，解除 rfkill 鎖定並重新初始化 ConnMan。',
        'Restart Bluetooth Subsystem': '重啟藍牙子系統',
        'Hotspot Manual Control': '熱點手動控制',
        'Current State: ': '目前狀態：',
        'Hotspot ACTIVE': '熱點 已啟用',
        'Hotspot INACTIVE': '熱點 未啟用',
        'Turn Hotspot OFF': '關閉熱點',
        'Turn Hotspot ON': '開啟熱點',
        'System Diagnostics Log': '系統診斷日誌',
    },
    'ja': {
        'Tools & Diagnostics': 'ツールと診断',
        'Refresh Status': '状態を更新',
        'Bluetooth Subsystem Recovery': 'Bluetooth復旧',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Bluetoothデーモンを低レベルでリセットし、rfkillを解除してConnManを再初期化します。',
        'Restart Bluetooth Subsystem': 'Bluetoothサブシステムを再起動',
        'Hotspot Manual Control': 'テザリングの手動制御',
        'Current State: ': '現在の状態: ',
        'Hotspot ACTIVE': 'テザリング 有効',
        'Hotspot INACTIVE': 'テザリング 無効',
        'Turn Hotspot OFF': 'テザリングをオフにする',
        'Turn Hotspot ON': 'テザリングをオンにする',
        'System Diagnostics Log': 'システム診断ログ',
    },
    'ca': {
        'Tools & Diagnostics': 'Eines i diagnòstics',
        'Refresh Status': 'Actualitza l\'estat',
        'Bluetooth Subsystem Recovery': 'Recuperació del Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Realitza un reinici de baix nivell del dimoni Bluetooth, desbloqueja rfkill i reinicia ConnMan.',
        'Restart Bluetooth Subsystem': 'Reinicia el subsistema Bluetooth',
        'Hotspot Manual Control': 'Control manual del punt d\'accés',
        'Current State: ': 'Estat actual: ',
        'Hotspot ACTIVE': 'Punt d\'accés ACTIU',
        'Hotspot INACTIVE': 'Punt d\'accés INACTIU',
        'Turn Hotspot OFF': 'Apaga el punt d\'accés',
        'Turn Hotspot ON': 'Engega el punt d\'accés',
        'System Diagnostics Log': 'Registre de diagnòstic del sistema',
    },
    'hu': {
        'Tools & Diagnostics': 'Eszközök és diagnosztika',
        'Refresh Status': 'Állapot frissítése',
        'Bluetooth Subsystem Recovery': 'Bluetooth helyreállítása',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Alacsony szintű Bluetooth újraindítást hajt végre, feloldja az rfkill zárolást és újraindítja a ConnMan-t.',
        'Restart Bluetooth Subsystem': 'Bluetooth alrendszer újraindítása',
        'Hotspot Manual Control': 'Hotspot kézi vezérlése',
        'Current State: ': 'Jelenlegi állapot: ',
        'Hotspot ACTIVE': 'Hotspot AKTÍV',
        'Hotspot INACTIVE': 'Hotspot INAKTÍV',
        'Turn Hotspot OFF': 'Hotspot kikapcsolása',
        'Turn Hotspot ON': 'Hotspot bekapcsolása',
        'System Diagnostics Log': 'Rendszerdiagnosztikai napló',
    },
    'el': {
        'Tools & Diagnostics': 'Εργαλεία και διαγνωστικά',
        'Refresh Status': 'Ανανέωση κατάστασης',
        'Bluetooth Subsystem Recovery': 'Ανάκτηση Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Εκτελεί επαναφορά χαμηλού επιπέδου του Bluetooth, ξεμπλοκάρει το rfkill και επανεκκινεί το ConnMan.',
        'Restart Bluetooth Subsystem': 'Επανεκκίνηση υποσυστήματος Bluetooth',
        'Hotspot Manual Control': 'Χειροκίνητος έλεγχος Hotspot',
        'Current State: ': 'Τρέχουσα κατάσταση: ',
        'Hotspot ACTIVE': 'Hotspot ΕΝΕΡΓΟ',
        'Hotspot INACTIVE': 'Hotspot ΑΝΕΝΕΡΓΟ',
        'Turn Hotspot OFF': 'Απενεργοποίηση Hotspot',
        'Turn Hotspot ON': 'Ενεργοποίηση Hotspot',
        'System Diagnostics Log': 'Αρχείο καταγραφής διαγνωστικών',
    },
    'tr': {
        'Tools & Diagnostics': 'Araçlar ve Tanılama',
        'Refresh Status': 'Durumu Yenile',
        'Bluetooth Subsystem Recovery': 'Bluetooth Kurtarma',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Bluetooth arka plan programını alt düzeyde sıfırlar, rfkill durumunu kaldırır ve ConnMan\'i yeniden başlatır.',
        'Restart Bluetooth Subsystem': 'Bluetooth Alt Sistemini Yeniden Başlat',
        'Hotspot Manual Control': 'Hotspot Manuel Kontrolü',
        'Current State: ': 'Mevcut Durum: ',
        'Hotspot ACTIVE': 'Hotspot AKTİF',
        'Hotspot INACTIVE': 'Hotspot DEVRE DIŞI',
        'Turn Hotspot OFF': 'Hotspot\'ı Kapat',
        'Turn Hotspot ON': 'Hotspot\'ı Aç',
        'System Diagnostics Log': 'Sistem Tanılama Günlüğü',
    },
    'et': {
        'Tools & Diagnostics': 'Tööriistad ja diagnostika',
        'Refresh Status': 'Värskenda olekut',
        'Bluetooth Subsystem Recovery': 'Bluetoothi taastamine',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Teeb Bluetoothi madalatasemelise lähtestamise, vabastab rfkill-blokeeringu ja lähtestab ConnMani.',
        'Restart Bluetooth Subsystem': 'Taaskäivita Bluetoothi alamsüsteem',
        'Hotspot Manual Control': 'Kuumkoha käsitsi juhtimine',
        'Current State: ': 'Praegune olek: ',
        'Hotspot ACTIVE': 'Kuumkoht AKTIIVNE',
        'Hotspot INACTIVE': 'Kuumkoht MITTEAKTIIVNE',
        'Turn Hotspot OFF': 'Lülita kuumkoht VÄLJA',
        'Turn Hotspot ON': 'Lülita kuumkoht SISSE',
        'System Diagnostics Log': 'Süsteemi diagnostikalogi',
    },
    'lt': {
        'Tools & Diagnostics': 'Įrankiai ir diagnostika',
        'Refresh Status': 'Atnaujinti būseną',
        'Bluetooth Subsystem Recovery': 'Bluetooth atkūrimas',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Atlieka žemo lygio Bluetooth atstatymą, atblokuoja rfkill ir iš naujo inicijuoja ConnMan.',
        'Restart Bluetooth Subsystem': 'Iš naujo paleisti Bluetooth posistemę',
        'Hotspot Manual Control': 'Rankinis prieigos taško valdymas',
        'Current State: ': 'Dabartinė būsena: ',
        'Hotspot ACTIVE': 'Prieigos taškas AKTYVUS',
        'Hotspot INACTIVE': 'Prieigos taškas NEAKTYVUS',
        'Turn Hotspot OFF': 'Išjungti prieigos tašką',
        'Turn Hotspot ON': 'Įjungti prieigos tašką',
        'System Diagnostics Log': 'Sistemos diagnostikos žurnalas',
    },
    'lv': {
        'Tools & Diagnostics': 'Rīki un diagnostika',
        'Refresh Status': 'Atjaunot statusu',
        'Bluetooth Subsystem Recovery': 'Bluetooth atkopšana',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Veic zema līmeņa Bluetooth atiestatīšanu, atbloķē rfkill un no jauna inicializē ConnMan.',
        'Restart Bluetooth Subsystem': 'Pārstartēt Bluetooth apakšsistēmu',
        'Hotspot Manual Control': 'Tīklāja manuālā vadība',
        'Current State: ': 'Pašreizējais stāvoklis: ',
        'Hotspot ACTIVE': 'Tīklājs AKTĪVS',
        'Hotspot INACTIVE': 'Tīklājs NEAKTĪVS',
        'Turn Hotspot OFF': 'Izslēgt tīklāju',
        'Turn Hotspot ON': 'Ieslēgt tīklāju',
        'System Diagnostics Log': 'Sistēmas diagnostikas žurnāls',
    },
    'ro': {
        'Tools & Diagnostics': 'Instrumente și diagnosticare',
        'Refresh Status': 'Actualizare stare',
        'Bluetooth Subsystem Recovery': 'Recuperare Bluetooth',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Efectuează o resetare de nivel scăzut a Bluetooth, deblochează rfkill și reinițializează ConnMan.',
        'Restart Bluetooth Subsystem': 'Repornește subsistemul Bluetooth',
        'Hotspot Manual Control': 'Control manual hotspot',
        'Current State: ': 'Stare actuală: ',
        'Hotspot ACTIVE': 'Hotspot ACTIV',
        'Hotspot INACTIVE': 'Hotspot INACTIV',
        'Turn Hotspot OFF': 'Oprește Hotspot',
        'Turn Hotspot ON': 'Pornește Hotspot',
        'System Diagnostics Log': 'Jurnal diagnostic sistem',
    },
    'sl': {
        'Tools & Diagnostics': 'Orodja in diagnostika',
        'Refresh Status': 'Osveži stanje',
        'Bluetooth Subsystem Recovery': 'Obnovitev Bluetootha',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Izvede nizkonivojsko ponastavitev Bluetootha, odblokira rfkill in ponovno inicializira ConnMan.',
        'Restart Bluetooth Subsystem': 'Znova zaženi podsistem Bluetooth',
        'Hotspot Manual Control': 'Ročni nadzor dostopne točke',
        'Current State: ': 'Trenutno stanje: ',
        'Hotspot ACTIVE': 'Dostopna točka AKTIVNA',
        'Hotspot INACTIVE': 'Dostopna točka NEAKTIVNA',
        'Turn Hotspot OFF': 'Izklopi dostopno točko',
        'Turn Hotspot ON': 'Vklopi dostopno točko',
        'System Diagnostics Log': 'Dnevnik diagnostike sistema',
    },
    'sr': {
        'Tools & Diagnostics': 'Алатке и дијагностика',
        'Refresh Status': 'Освежи статус',
        'Bluetooth Subsystem Recovery': 'Опоравак Bluetooth-а',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Извршава ресетовање ниског нивоа за Bluetooth демон, одблокира rfkill и поново покреће ConnMan.',
        'Restart Bluetooth Subsystem': 'Поново покрени Bluetooth подсистем',
        'Hotspot Manual Control': 'Ручно управљање хотспотом',
        'Current State: ': 'Тренутно стање: ',
        'Hotspot ACTIVE': 'Хотспот је АКТИВАН',
        'Hotspot INACTIVE': 'Хотспот је НЕАКТИВАН',
        'Turn Hotspot OFF': 'Искључи хотспот',
        'Turn Hotspot ON': 'Укључи хотспот',
        'System Diagnostics Log': 'Дневник системске дијагностике',
    },
    'ar': {
        'Tools & Diagnostics': 'الأدوات والتشخيص',
        'Refresh Status': 'تحديث الحالة',
        'Bluetooth Subsystem Recovery': 'استعادة البلوتوث',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'يقوم بإعادة تعيين منخفضة المستوى للبلوتوث، وإلغاء حظر rfkill وإعادة تهيئة ConnMan.',
        'Restart Bluetooth Subsystem': 'إعادة تشغيل نظام البلوتوث الفرعي',
        'Hotspot Manual Control': 'التحكم اليدوي بنقطة الاتصال',
        'Current State: ': 'الحالة الحالية: ',
        'Hotspot ACTIVE': 'نقطة الاتصال نشطة',
        'Hotspot INACTIVE': 'نقطة الاتصال غير نشطة',
        'Turn Hotspot OFF': 'إيقاف نقطة الاتصال',
        'Turn Hotspot ON': 'تشغيل نقطة الاتصال',
        'System Diagnostics Log': 'سجل تشخيص النظام',
    },
    'eu': {
        'Tools & Diagnostics': 'Tresnak eta diagnostikoak',
        'Refresh Status': 'Eguneratu egoera',
        'Bluetooth Subsystem Recovery': 'Bluetooth-a berreskuratzea',
        'Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.': 'Bluetooth deabruaren maila baxuko berrezarpena egiten du, rfkill desblokeatzen du eta ConnMan berriz hasten du.',
        'Restart Bluetooth Subsystem': 'Berrabiarazi Bluetooth azpisistema',
        'Hotspot Manual Control': 'Hotspotaren eskuzko kontrola',
        'Current State: ': 'Uneko egoera: ',
        'Hotspot ACTIVE': 'Hotspota AKTIBOA',
        'Hotspot INACTIVE': 'Hotspota EZ-AKTIBOA',
        'Turn Hotspot OFF': 'Itzali hotspota',
        'Turn Hotspot ON': 'Piztu hotspota',
        'System Diagnostics Log': 'Sistemaren diagnostikoen erregistroa',
    },
}

def update_translations():
    ts_files = sorted(glob.glob('translations/*.ts'))
    print(f"Updating {len(ts_files)} translation files...")

    for fpath in ts_files:
        lang_match = re.search(r'harbour-carhotspot-(.*)\.ts', fpath)
        if not lang_match:
            continue
        lang = lang_match.group(1)
        if lang == 'bg':
            continue

        tree = ET.parse(fpath)
        root = tree.getroot()
        modified = False

        trans_map = NEW_TRANSLATIONS.get(lang, NEW_TRANSLATIONS.get('en', {}))

        # Check existing translations in file for Turn Hotspot ON/OFF if not explicitly in map
        existing_off = None
        existing_on = None
        for msg in root.findall('.//message'):
            src = msg.find('source')
            tr = msg.find('translation')
            if src is not None and tr is not None and tr.text and 'type' not in tr.attrib:
                if src.text == 'Turn Hotspot OFF' and not existing_off:
                    existing_off = tr.text
                elif src.text == 'Turn Hotspot ON' and not existing_on:
                    existing_on = tr.text

        for msg in root.findall('.//message'):
            tr = msg.find('translation')
            src = msg.find('source')
            if src is None or not src.text:
                continue
            src_text = src.text

            # If message is in MainPage and is Activity Log:, we can remove or mark
            if src_text == 'Activity Log:':
                parent_context = None
                for ctx in root.findall('context'):
                    if msg in ctx.findall('message'):
                        ctx.remove(msg)
                        modified = True
                        break
                continue

            if tr is not None and tr.attrib.get('type') == 'unfinished':
                if src_text in trans_map:
                    tr.text = trans_map[src_text]
                    del tr.attrib['type']
                    modified = True
                elif src_text == 'Turn Hotspot OFF' and existing_off:
                    tr.text = existing_off
                    del tr.attrib['type']
                    modified = True
                elif src_text == 'Turn Hotspot ON' and existing_on:
                    tr.text = existing_on
                    del tr.attrib['type']
                    modified = True
                else:
                    # fallback to English
                    tr.text = NEW_TRANSLATIONS['en'].get(src_text, src_text)
                    del tr.attrib['type']
                    modified = True

        if modified:
            tree.write(fpath, encoding='utf-8', xml_declaration=True)
            print(f"Updated {fpath}")

    print("All translation files updated.")

if __name__ == '__main__':
    update_translations()

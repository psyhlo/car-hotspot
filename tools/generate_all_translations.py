#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import xml.etree.ElementTree as ET
import subprocess

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
sys.path.insert(0, SCRIPT_DIR)

from l10n.group1 import GROUP1_TRANSLATIONS
from l10n.group2 import GROUP2_TRANSLATIONS
from l10n.group3 import GROUP3_TRANSLATIONS
from l10n.group4 import GROUP4_TRANSLATIONS

ALL_LANGUAGES = {}
ALL_LANGUAGES.update(GROUP1_TRANSLATIONS)
ALL_LANGUAGES.update(GROUP2_TRANSLATIONS)
ALL_LANGUAGES.update(GROUP3_TRANSLATIONS)
ALL_LANGUAGES.update(GROUP4_TRANSLATIONS)

TEMPLATE_FILE = os.path.join(PROJECT_DIR, "translations", "harbour-carhotspot-bg.ts")
TRANSLATIONS_DIR = os.path.join(PROJECT_DIR, "translations")

def generate():
    if not os.path.exists(TEMPLATE_FILE):
        print(f"Error: Template {TEMPLATE_FILE} not found!")
        sys.exit(1)

    tree = ET.parse(TEMPLATE_FILE)
    root = tree.getroot()

    print(f"Generating translations for {len(ALL_LANGUAGES)} languages...")

    for lang, trans_dict in ALL_LANGUAGES.items():
        lang_tree = ET.parse(TEMPLATE_FILE)
        lang_root = lang_tree.getroot()
        lang_root.attrib['language'] = lang

        for context in lang_root.findall('context'):
            for message in context.findall('message'):
                source_elem = message.find('source')
                if source_elem is None or source_elem.text is None:
                    continue
                source_text = source_elem.text
                trans_elem = message.find('translation')
                if trans_elem is None:
                    trans_elem = ET.SubElement(message, 'translation')

                if source_text in trans_dict:
                    trans_elem.text = trans_dict[source_text]
                    if 'type' in trans_elem.attrib:
                        del trans_elem.attrib['type']
                else:
                    # fallback to English
                    trans_elem.text = source_text

        out_path = os.path.join(TRANSLATIONS_DIR, f"harbour-carhotspot-{lang}.ts")
        lang_tree.write(out_path, encoding='utf-8', xml_declaration=True)
        print(f"  -> Generated {out_path}")

    print("\nAll .ts files generated successfully.")

if __name__ == '__main__':
    generate()

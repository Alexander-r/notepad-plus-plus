#!/usr/bin/env python3
"""Generate the menu and image tables from the remaining .rc files.

Notepad_plus.rc and a few others remain the source for the main menus and the image list. Dialogs are .ui
forms now. The output goes to PowerEditor/src/generated and is committed, so building needs no Python.
Run it again after changing a .rc file:

    python3 cmake/tools/rc2qt.py
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'PowerEditor', 'src')
OUT = os.path.join(SRC, 'generated')

# Features the user chose to remove from the port
SKIPPED_RC = {'pluginsAdmin.rc', 'regExtDlg.rc'}

CONTROL_KEYWORDS = {'CONTROL', 'LTEXT', 'RTEXT', 'CTEXT', 'EDITTEXT', 'COMBOBOX', 'LISTBOX', 'PUSHBUTTON', 'DEFPUSHBUTTON',
                    'GROUPBOX', 'CHECKBOX', 'AUTOCHECKBOX', 'AUTORADIOBUTTON', 'RADIOBUTTON', 'ICON', 'SCROLLBAR', 'AUTO3STATE', 'STATE3'}

# Control statement layouts: text-bearing ones are  text,id,x,y,w,h[,style[,exstyle]]; the others  id,x,y,w,h[,style[,exstyle]]
TEXTLESS = {'EDITTEXT', 'COMBOBOX', 'LISTBOX', 'SCROLLBAR'}


def strip_comments(text):
    out, i, n = [], 0, len(text)
    in_string = False
    while i < n:
        c = text[i]
        if in_string:
            out.append(c)
            if c == '"':
                in_string = False
            elif c == '\\' and i + 1 < n:
                i += 1
                out.append(text[i])
            i += 1
        elif c == '"':
            in_string = True
            out.append(c)
            i += 1
        elif text.startswith('//', i):
            while i < n and text[i] != '\n':
                i += 1
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def split_args(line):
    """Split on top-level commas, keeping string literals intact."""
    args, cur, in_string, i = [], [], False, 0
    while i < len(line):
        c = line[i]
        if in_string:
            cur.append(c)
            if c == '\\' and i + 1 < len(line):
                i += 1
                cur.append(line[i])
            elif c == '"':
                # "" is an escaped quote in .rc strings
                if i + 1 < len(line) and line[i + 1] == '"':
                    i += 1
                    cur.append('"')
                else:
                    in_string = False
        elif c == '"':
            in_string = True
            cur.append(c)
        elif c == ',':
            args.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    args.append(''.join(cur).strip())
    return args


def rc_string(token):
    """Convert an .rc string literal to the C++ wide string body."""
    if not (token.startswith('"') and token.endswith('"')):
        return '@' + token  # a macro naming a wide string, such as NOTEPAD_PLUS_VERSION
    return token[1:-1].replace('""', '\\"')


def wide(text):
    return text[1:] if text.startswith('@') else 'L"%s"' % text


def style_names(expr):
    """Style words of an expression, with those preceded by NOT marked with a leading '!'."""
    names = []
    for m in re.finditer(r'(NOT\s+)?([A-Z_][A-Z0-9_]*|0x[0-9a-fA-F]+|\d+)', expr or ''):
        names.append(('!' if m.group(1) else '') + m.group(2))
    return names


def number(token):
    return int(token, 0)


def control_from_statement(keyword, rest):
    args = split_args(rest)
    text = ''
    styles = []
    cls = ''
    if keyword == 'CONTROL':
        # text, id, class, style, x, y, w, h [,exstyle]
        text = rc_string(args[0])
        cid = args[1]
        cls = args[2].strip('"')
        styles = style_names(args[3])
        x, y, w, h = args[4:8]
    elif keyword in TEXTLESS:
        cid = args[0]
        x, y, w, h = args[1:5]
        styles = style_names(args[5]) if len(args) > 5 else []
    else:
        if keyword == 'ICON':
            text = ''
        else:
            text = rc_string(args[0])
        cid = args[1]
        x, y, w, h = args[2:6]
        styles = style_names(args[6]) if len(args) > 6 else []

    kind, flags = classify(keyword, cls, styles)
    return dict(id=cid, kind=kind, text=text, x=number(x), y=number(y), w=number(w), h=number(h), flags=flags)


COMMON_FLAGS = {'WS_GROUP': 'Group', 'WS_DISABLED': 'Disabled', '!WS_VISIBLE': 'Hidden', 'WS_VSCROLL': 'VScroll', 'WS_HSCROLL': 'HScroll', '!WS_TABSTOP': 'NoTabStop'}
STYLE_FLAGS = {
    'ES_MULTILINE': 'Multiline', 'ES_READONLY': 'ReadOnly', 'ES_PASSWORD': 'Password', 'ES_NUMBER': 'Number', 'ES_RIGHT': 'AlignRight', 'ES_CENTER': 'AlignCenter',
    'ES_WANTRETURN': 'WantReturn', 'ES_AUTOHSCROLL': 'AutoHScroll', 'ES_AUTOVSCROLL': 'AutoVScroll',
    'SS_NOTIFY': 'Notify', 'SS_RIGHT': 'AlignRight', 'SS_CENTER': 'AlignCenter', 'SS_ICON': 'ImageLabel', 'SS_BITMAP': 'ImageLabel', 'SS_SUNKEN': 'Sunken', 'SS_ETCHEDHORZ': 'HorizontalLine', 'SS_ETCHEDFRAME': 'Sunken', 'SS_BLACKFRAME': 'Sunken',
    'BS_MULTILINE': 'Multiline', 'BS_FLAT': 'Flat', 'BS_LEFTTEXT': 'LeftText', 'BS_RIGHT': 'AlignRight', 'BS_LEFT': 'AlignLeft', 'BS_PUSHLIKE': 'PushLike', 'BS_ICON': 'ImageLabel', 'BS_BITMAP': 'ImageLabel', 'BS_DEFPUSHBUTTON': 'Default', 'BS_SPLITBUTTON': 'SplitButton',
    'LBS_SORT': 'Sorted', 'LBS_MULTIPLESEL': 'MultiSelect', 'LBS_EXTENDEDSEL': 'ExtendedSelect', 'LBS_NOTIFY': 'Notify', 'LBS_OWNERDRAWFIXED': 'OwnerDraw', 'LBS_OWNERDRAWVARIABLE': 'OwnerDraw',
    'CBS_DROPDOWNLIST': 'DropDownList', 'CBS_SORT': 'Sorted', 'CBS_OWNERDRAWFIXED': 'OwnerDraw',
    'TBS_VERT': 'Vertical', 'TBS_NOTICKS': 'NoTicks',
    'UDS_ALIGNRIGHT': 'AlignRight', 'UDS_AUTOBUDDY': 'AutoBuddy', 'UDS_SETBUDDYINT': 'SetBuddyInt',
}


def flags_from(styles):
    flags = []
    for s in styles:
        f = COMMON_FLAGS.get(s) or (STYLE_FLAGS.get(s) if not s.startswith('!') else None)
        if f and f not in flags:
            flags.append(f)
    return flags


def classify(keyword, cls, styles):
    flags = flags_from(styles)
    s = set(styles)
    if keyword == 'CONTROL':
        c = cls.lower()
        if c == 'button':
            if s & {'BS_AUTOCHECKBOX', 'BS_CHECKBOX', 'BS_AUTO3STATE', 'BS_3STATE'}:
                if s & {'BS_AUTO3STATE', 'BS_3STATE'}:
                    flags.append('Tristate')
                return 'Check', flags
            if s & {'BS_AUTORADIOBUTTON', 'BS_RADIOBUTTON'}:
                return 'Radio', flags
            if 'BS_GROUPBOX' in s:
                return 'GroupBox', flags
            return 'Push', flags
        if c == 'static':
            return 'Label', flags
        if c == 'edit':
            return 'Edit', flags
        if c == 'combobox':
            return 'Combo', flags
        if c == 'listbox':
            return 'List', flags
        if c == 'msctls_trackbar32':
            return 'Slider', flags
        if c == 'msctls_updown32':
            return 'SpinButtons', flags
        if c == 'msctls_progress32':
            return 'Progress', flags
        if c == 'systabcontrol32':
            return 'TabBar', flags
        if c == 'syslistview32':
            return 'ListView', flags
        if c == 'systreeview32':
            return 'TreeView', flags
        if c == 'syslink':
            return 'Link', flags
        return 'Custom', flags
    if keyword in ('LTEXT', 'RTEXT', 'CTEXT', 'ICON'):
        if keyword == 'RTEXT':
            flags.append('AlignRight')
        elif keyword == 'CTEXT':
            flags.append('AlignCenter')
        elif keyword == 'ICON':
            flags.append('ImageLabel')
        return 'Label', flags
    if keyword == 'EDITTEXT':
        return 'Edit', flags
    if keyword == 'COMBOBOX':
        return 'Combo', flags
    if keyword == 'LISTBOX':
        return 'List', flags
    if keyword in ('PUSHBUTTON', 'DEFPUSHBUTTON'):
        if keyword == 'DEFPUSHBUTTON':
            flags.append('Default')
        return 'Push', flags
    if keyword == 'GROUPBOX':
        return 'GroupBox', flags
    if keyword in ('CHECKBOX', 'AUTOCHECKBOX'):
        return 'Check', flags
    if keyword in ('AUTO3STATE', 'STATE3'):
        return 'Check', flags + ['Tristate']
    if keyword in ('RADIOBUTTON', 'AUTORADIOBUTTON'):
        return 'Radio', flags
    if keyword == 'SCROLLBAR':
        return 'Slider', flags
    raise ValueError(keyword)


def active_lines(text):
    """Resolve preprocessor conditionals conservatively: every #if branch is excluded except #else branches of
    '#if 0', and #ifndef/#ifdef are treated as not defined. The .rc files only use these for optional content."""
    lines = []
    stack = []
    for raw in text.split('\n'):
        line = raw.strip()
        if line.startswith('#'):
            directive = line[1:].strip()
            if directive.startswith(('if ', 'ifdef')):
                stack.append(False)
            elif directive.startswith('ifndef'):
                stack.append(True)
            elif directive.startswith('else') or directive.startswith('elif'):
                if stack:
                    stack[-1] = not stack[-1]
            elif directive.startswith('endif'):
                if stack:
                    stack.pop()
            continue
        if all(stack):
            lines.append(raw)
    return lines


def parse_rc(path):
    text = strip_comments(open(path, encoding='utf-8', errors='replace', newline='').read().replace('\r\n', '\n').lstrip('﻿'))
    lines = active_lines(text)
    dialogs, menus, images = [], [], []
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        i += 1
        if not line:
            continue

        m = re.match(r'^(\w+)\s+(ICON|BITMAP|CURSOR)\b(?:\s+\w+)*\s+"([^"]+)"', line)
        if m:
            images.append(dict(id=m.group(1), type=m.group(2), file=m.group(3).replace('\\\\', '/').replace('\\', '/')))
            continue

        m = re.match(r'^(\w+)\s+MENU\b', line)
        if m:
            while not lines[i].strip().startswith(('BEGIN', '{')):
                i += 1
            i += 1
            items, i = parse_menu(lines, i)
            menus.append(dict(id=m.group(1), items=items))
            continue

        m = re.match(r'^(\w+)\s+DIALOG(EX)?\b(?:\s+(?:DISCARDABLE|MOVEABLE|PURE|LOADONCALL|PRELOAD|IMPURE))*\s*(.*)$', line)
        if not m:
            continue
        rect = [a.strip() for a in m.group(3).split(',')]
        dialog = dict(id=m.group(1), x=number(rect[0]), y=number(rect[1]), w=number(rect[2]), h=number(rect[3]), caption='', style=[], controls=[])
        # Dialog attributes until BEGIN
        while True:
            l = lines[i].strip()
            i += 1
            if l.startswith('BEGIN') or l == '{':
                break
            if l.startswith('CAPTION'):
                dialog['caption'] = rc_string(l[len('CAPTION'):].strip())
            elif l.startswith('STYLE'):
                expr = l[len('STYLE'):]
                # a style can continue on the next line
                while lines[i].strip().startswith('|') or expr.rstrip().endswith('|'):
                    expr += lines[i]
                    i += 1
                dialog['style'] = style_names(expr)
        # Statements until END. A statement may continue on following lines.
        statement = None
        while True:
            l = lines[i].strip()
            i += 1
            word = re.match(r'^[A-Z_0-9]+', l)
            word = word.group(0) if word else ''
            if word in CONTROL_KEYWORDS or l == 'END' or l == '}':
                if statement:
                    dialog['controls'].append(control_from_statement(*statement))
                    statement = None
                if l == 'END' or l == '}':
                    break
                statement = (word, l[len(word):].strip())
            elif l and statement:
                statement = (statement[0], statement[1] + ' ' + l)
        dialogs.append(dialog)
    return dialogs, menus, images


def parse_menu(lines, i):
    """Parse a MENU body after its BEGIN. Returns (items, next line index)."""
    items = []
    while True:
        l = lines[i].strip()
        i += 1
        if not l:
            continue
        if l == 'END' or l == '}':
            return items, i
        m = re.match(r'^POPUP\s+("(?:[^"]|"")*")', l)
        if m:
            while not lines[i].strip().startswith(('BEGIN', '{')):
                i += 1
            i += 1
            children, i = parse_menu(lines, i)
            items.append(dict(kind='popup', text=rc_string(m.group(1)), children=children))
            continue
        if re.match(r'^MENUITEM\s+SEPARATOR', l):
            items.append(dict(kind='separator'))
            continue
        m = re.match(r'^MENUITEM\s+(.*)$', l)
        if m:
            args = split_args(m.group(1))
            items.append(dict(kind='item', text=rc_string(args[0]), id=args[1] if len(args) > 1 else '0', options=[a.upper() for a in args[2:]]))
            continue
        raise ValueError('menu line: ' + l)


def find_rc_files():
    found = []
    for directory, _, files in os.walk(SRC):
        if os.path.relpath(directory, SRC).startswith('generated'):
            continue
        for f in files:
            if f.endswith('.rc') and f not in SKIPPED_RC:
                found.append(os.path.join(directory, f))
    return sorted(found)


def includes_of(path):
    result = []
    for line in open(path, encoding='utf-8', errors='replace'):
        m = re.match(r'\s*#\s*include\s+"([^"]+\.h)"', line)
        if m:
            result.append(m.group(1))
    return result


def emit_menu_items(items, name, out):
    """Emit a table per level and return the entries of this level."""
    rows = []
    for index, item in enumerate(items):
        if item['kind'] == 'separator':
            rows.append('{NppMenuEntry::Separator, 0, L"", nullptr, 0, 0}')
        elif item['kind'] == 'popup':
            child = '%s_%d' % (name, index)
            emit_menu_items(item['children'], child, out)
            rows.append('{NppMenuEntry::Popup, 0, %s, %s, %d, 0}' % (wide(item['text']), child, len(item['children'])))
        else:
            flags = []
            if 'CHECKED' in item['options']:
                flags.append('NppMenuEntry::Checked')
            if 'GRAYED' in item['options'] or 'INACTIVE' in item['options']:
                flags.append('NppMenuEntry::Disabled')
            rows.append('{NppMenuEntry::Item, %s, %s, nullptr, 0, %s}' % (item['id'], wide(item['text']), ' | '.join(flags) or '0'))
    out.append('static const NppMenuEntry %s[] =\n{\n\t%s\n};\n' % (name, ',\n\t'.join(rows) if rows else '{NppMenuEntry::Separator, 0, L"", nullptr, 0, 0}'))


def main():
    os.makedirs(OUT, exist_ok=True)
    all_dialogs, all_menus, all_images, headers = [], [], [], []
    for path in find_rc_files():
        dialogs, menus, images = parse_rc(path)
        directory = os.path.relpath(os.path.dirname(path), SRC).replace(os.sep, '/')
        for image in images:
            image['file'] = os.path.normpath(os.path.join(directory, image['file'])).replace(os.sep, '/')
        all_dialogs += dialogs
        all_menus += menus
        all_images += images
        for h in includes_of(path):
            if h not in headers and h.lower() != 'windows.h':
                headers.append(h)

    out = ['// Generated by cmake/tools/rc2qt.py from the .rc files. Do not edit.\n',
           '#include "NppMessages.h"\n#include "NppResources.h"\n']
    for h in headers:
        out.append('#include "%s"' % h)
    out.append('')
    out.append('#ifndef IDC_STATIC\n#define IDC_STATIC (-1)\n#endif\n')

    for m in all_menus:
        name = 'menu_' + m['id']
        emit_menu_items(m['items'], name, out)
    out.append('const NppMenuEntry* nppMenuTemplate(int menuId, int& count)\n{\n\tswitch (menuId)\n\t{')
    for m in all_menus:
        out.append('\t\tcase %s: count = %d; return menu_%s;' % (m['id'], len(m['items']), m['id']))
    out.append('\t\tdefault: count = 0; return nullptr;\n\t}\n}\n')

    # Images: first definition of an ID wins, like the resource compiler rejects duplicates
    seen = set()
    rows = []
    qrc_files = []
    missing = []
    for image in all_images:
        if image['id'] in seen:
            continue
        seen.add(image['id'])
        full = os.path.join(SRC, image['file'])
        if not os.path.exists(full):
            missing.append(image['file'])
            continue
        rows.append('{%s, ":/npp/%s"}' % (image['id'], image['file']))
        if image['file'] not in qrc_files:
            qrc_files.append(image['file'])
    out.append('static const NppImageResource imageTable[] =\n{\n\t%s\n};\n' % ',\n\t'.join(rows))
    out.append('const char* nppImageResourcePath(int resourceId)\n{\n\tfor (const NppImageResource& r : imageTable)\n\t{\n\t\tif (r.id == resourceId)\n\t\t\treturn r.path;\n\t}\n\treturn nullptr;\n}\n')

    open(os.path.join(OUT, 'NppResources_generated.cpp'), 'w', newline='').write('\n'.join(out).replace('\n', '\r\n') + '\r\n')

    qrc = ['<RCC>', '\t<qresource prefix="/npp">'] + ['\t\t<file alias="%s">../%s</file>' % (f, f) for f in qrc_files] + ['\t</qresource>', '</RCC>', '']
    open(os.path.join(OUT, 'NppResources.qrc'), 'w', newline='').write('\n'.join(qrc))

    print('menus: %d, images: %d' % (len(all_menus), len(rows)))
    if missing:
        print('missing image files:', missing[:10])
    return 0


if __name__ == '__main__':
    sys.exit(main())

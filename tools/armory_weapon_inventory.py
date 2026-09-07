#!/usr/bin/env python3
import argparse
import concurrent.futures
import html
import io
import re
import struct
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "GWToolboxdll/Windows/ArmoryWindow_Constants.h"
OUTPUT = Path(__file__).with_name("armory_weapons.xlsx")
ENTRY = re.compile(
    r'^\s*\{"(?P<label>(?:[^"\\]|\\.)*)",\s*(?P<model>0x[0-9a-fA-F]+),\s*'
    r'Profession::None,\s*ItemType::(?P<type>\w+),\s*Campaign::BonusMissionPack,\s*'
    r'(?P<tint>\d+),\s*(?P<interaction>0x[0-9a-fA-F]+)\},?'
)


def weapons():
    source = SOURCE.read_text(encoding="utf-8")
    start = source.index("    Armor weapons[] = {")
    end = source.index("    };", start)
    result = []
    for line in source[start:end].splitlines():
        match = ENTRY.match(line)
        if not match:
            continue
        entry = match.groupdict()
        entry["label"] = entry["label"].replace(r'\"', '"')
        entry["known"] = not re.fullmatch(r"0x[0-9a-fA-F]+", entry["label"])
        result.append(entry)
    if not result:
        raise RuntimeError("No Armory weapons found")
    return result


def icon(name):
    url = "https://wiki.guildwars.com/wiki/Special:FilePath/" + urllib.parse.quote(name + ".png")
    request = urllib.request.Request(url, headers={"User-Agent": "GWToolbox++ Armory inventory generator"})
    try:
        with urllib.request.urlopen(request, timeout=20) as response:
            data = response.read()
    except OSError:
        return None
    return data if data.startswith(b"\x89PNG\r\n\x1a\n") else None


def cell(reference, value):
    if value == "":
        return f'<c r="{reference}" t="inlineStr"><is><t/></is></c>'
    return f'<c r="{reference}" t="inlineStr"><is><t>{html.escape(value)}</t></is></c>'


def worksheet(entries, images):
    rows = [
        '<row r="1" ht="24" customHeight="1">'
        + cell("A1", "Model File ID")
        + cell("B1", "Interaction")
        + cell("C1", "Is Valid Weapon (Y/N)")
        + cell("D1", "Name")
        + cell("E1", "Icon")
        + "</row>"
    ]
    for index, entry in enumerate(entries, start=2):
        name = entry["label"] if entry["known"] else ""
        rows.append(
            f'<row r="{index}" ht="54" customHeight="1">'
            + cell(f"A{index}", entry["model"].upper())
            + cell(f"B{index}", entry["interaction"].upper())
            + cell(f"C{index}", "Y" if entry["known"] else "")
            + cell(f"D{index}", name)
            + cell(f"E{index}", "" if index not in images else "")
            + "</row>"
        )
    drawing = '<drawing r:id="rId1"/>' if images else ""
    return f'''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
  <sheetViews><sheetView workbookViewId="0"><pane ySplit="1" topLeftCell="A2" activePane="bottomLeft" state="frozen"/><selection pane="bottomLeft" activeCell="A2" sqref="A2"/></sheetView></sheetViews>
  <cols><col min="1" max="1" width="16" customWidth="1"/><col min="2" max="2" width="16" customWidth="1"/><col min="3" max="3" width="24" customWidth="1"/><col min="4" max="4" width="38" customWidth="1"/><col min="5" max="5" width="12" customWidth="1"/></cols>
  <sheetData>{''.join(rows)}</sheetData>
  <autoFilter ref="A1:E{len(entries) + 1}"/>
  {drawing}
</worksheet>'''


def drawing(images):
    anchors = []
    for image_index, row in enumerate(images, start=1):
        anchors.append(f'''<xdr:oneCellAnchor>
  <xdr:from><xdr:col>4</xdr:col><xdr:colOff>47625</xdr:colOff><xdr:row>{row - 1}</xdr:row><xdr:rowOff>19050</xdr:rowOff></xdr:from>
  <xdr:ext cx="609600" cy="609600"/>
  <xdr:pic><xdr:nvPicPr><xdr:cNvPr id="{image_index}" name="Icon {image_index}"/><xdr:cNvPicPr/></xdr:nvPicPr><xdr:blipFill><a:blip r:embed="rId{image_index}"/><a:stretch><a:fillRect/></a:stretch></xdr:blipFill><xdr:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="609600" cy="609600"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom></xdr:spPr></xdr:pic>
  <xdr:clientData/>
</xdr:oneCellAnchor>''')
    return f'''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<xdr:wsDr xmlns:xdr="http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing" xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">{''.join(anchors)}</xdr:wsDr>'''


def write(entries, icons, output):
    images = {row: data for row, data in icons.items() if data}
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        content_types = ['<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>', '<Default Extension="xml" ContentType="application/xml"/>', '<Default Extension="png" ContentType="image/png"/>', '<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>', '<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>']
        if images:
            content_types.append('<Override PartName="/xl/drawings/drawing1.xml" ContentType="application/vnd.openxmlformats-officedocument.drawing+xml"/>')
        archive.writestr("[Content_Types].xml", '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">' + ''.join(content_types) + "</Types>")
        archive.writestr("_rels/.rels", '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>')
        archive.writestr("xl/workbook.xml", '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="Weapons" sheetId="1" r:id="rId1"/></sheets></workbook>')
        archive.writestr("xl/_rels/workbook.xml.rels", '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/></Relationships>')
        archive.writestr("xl/worksheets/sheet1.xml", worksheet(entries, images))
        if images:
            archive.writestr("xl/worksheets/_rels/sheet1.xml.rels", '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing" Target="../drawings/drawing1.xml"/></Relationships>')
            archive.writestr("xl/drawings/drawing1.xml", drawing(images))
            relationships = []
            for image_index, (_, image) in enumerate(images.items(), start=1):
                archive.writestr(f"xl/media/image{image_index}.png", image)
                relationships.append(f'<Relationship Id="rId{image_index}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="../media/image{image_index}.png"/>')
            archive.writestr("xl/drawings/_rels/drawing1.xml.rels", '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">' + ''.join(relationships) + "</Relationships>")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--skip-icons", action="store_true")
    args = parser.parse_args()
    entries = weapons()
    icons = {}
    if not args.skip_icons:
        known_entries = [(row, entry["label"]) for row, entry in enumerate(entries, start=2) if entry["known"]]
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
            for row, image in zip((row for row, _ in known_entries), executor.map(lambda item: icon(item[1]), known_entries)):
                icons[row] = image
    write(entries, icons, args.output)
    print(f"Wrote {args.output} with {len(entries)} weapons and {sum(bool(value) for value in icons.values())} icons")


if __name__ == "__main__":
    main()

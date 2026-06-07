#!/usr/bin/env python3
"""Update CHANNELS.EPG_CHANNEL_ID in a simpleiptv database from an XMLTV file.

For every <channel> in the XMLTV file we read its `id` attribute and its
<display-name> text, then set EPG_CHANNEL_ID on the matching CHANNELS row,
matching by channel name (CHANNELS.NAME == <display-name>).
"""

import argparse
import sqlite3
import sys
import xml.etree.ElementTree as ET


def parse_xmltv(xml_path):
    """Return {display_name: channel_id} parsed from the XMLTV file.

    A <channel> may carry several <display-name> tags; each one is mapped to
    the channel id so any of them can match a row in the database.
    """
    name_to_id = {}
    # iterparse keeps memory low on large guides and clears elements as we go.
    for _, elem in ET.iterparse(xml_path, events=("end",)):
        if elem.tag != "channel":
            continue
        channel_id = elem.get("id")
        if channel_id:
            for dn in elem.findall("display-name"):
                name = (dn.text or "").strip()
                if name:
                    name_to_id[name] = channel_id
        elem.clear()
    return name_to_id


def update_database(db_path, name_to_id, dry_run=False):
    """Apply EPG_CHANNEL_ID updates, matching CHANNELS.NAME exactly.

    Returns (matched_rows, distinct_names_matched).
    """
    con = sqlite3.connect(db_path)
    try:
        cur = con.cursor()
        matched_rows = 0
        names_matched = 0
        for name, channel_id in name_to_id.items():
            cur.execute(
                "UPDATE CHANNELS SET EPG_CHANNEL_ID = ? WHERE NAME = ?",
                (channel_id, name),
            )
            if cur.rowcount > 0:
                matched_rows += cur.rowcount
                names_matched += 1
        if dry_run:
            con.rollback()
        else:
            con.commit()
        return matched_rows, names_matched
    finally:
        con.close()


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Update CHANNELS.EPG_CHANNEL_ID from an XMLTV file, matching by name."
    )
    parser.add_argument("xmltv", help="Path to the xmltv.xml file")
    parser.add_argument("database", help="Path to the simpleiptv sqlite database")
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Report what would change without writing to the database",
    )
    args = parser.parse_args(argv)

    name_to_id = parse_xmltv(args.xmltv)
    print(f"Parsed {len(name_to_id)} channel names from {args.xmltv}")

    matched_rows, names_matched = update_database(
        args.database, name_to_id, dry_run=args.dry_run
    )

    action = "Would update" if args.dry_run else "Updated"
    print(
        f"{action} {matched_rows} channel row(s) "
        f"({names_matched} of {len(name_to_id)} XMLTV names matched a channel)."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())

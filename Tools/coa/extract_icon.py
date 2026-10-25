"""Extract an icon group from a Windows executable into a .ico file.

Usage:
    python Tools/coa/extract_icon.py D:/WoW/Coa/Ascension.exe CoA/Resources/CoA64.ico [group-index]

The first RT_GROUP_ICON is used unless an index is given. The .ico gets every image of the group, as the .exe holds them.
"""
import struct
import sys

import pefile

RT_ICON = 3
RT_GROUP_ICON = 14


def resource_entries(pe, type_id):
    for entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if entry.id == type_id:
            for item in entry.directory.entries:
                for language in item.directory.entries:
                    yield item.id if item.id is not None else str(item.name), language.data.struct


def main(argv):
    exe, out = argv[1], argv[2]
    which = int(argv[3]) if len(argv) > 3 else 0
    pe = pefile.PE(exe)
    icons = {key: pe.get_data(data.OffsetToData, data.Size) for key, data in resource_entries(pe, RT_ICON)}
    groups = list(resource_entries(pe, RT_GROUP_ICON))
    _, group_struct = groups[which]
    group = pe.get_data(group_struct.OffsetToData, group_struct.Size)
    reserved, kind, count = struct.unpack_from('<HHH', group, 0)
    entries = [struct.unpack_from('<BBBBHHIH', group, 6 + i * 14) for i in range(count)]
    header = struct.pack('<HHH', reserved, kind, count)
    directory = b''
    images = b''
    offset = 6 + 16 * count

    for width, height, colors, res, planes, bits, size, icon_id in entries:
        image = icons[icon_id]
        directory += struct.pack('<BBBBHHII', width, height, colors, res, planes, bits, len(image), offset + len(images))
        images += image

    with open(out, 'wb') as f:
        f.write(header + directory + images)

    print(f'{count} images from group {which} of {len(groups)} -> {out}')


if __name__ == '__main__':
    main(sys.argv)

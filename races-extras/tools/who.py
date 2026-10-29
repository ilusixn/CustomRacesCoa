import sys
sys.path[:0] = [r'C:\CoA-Build\esteria_racials']
import estlib
BS = chr(92)
C = estlib.Client(estlib.OUR_DATA)
names = ['ChrRaces', 'CharBaseInfo', 'CharStartOutfit', 'CharSections', 'CharHairGeosets', 'CharacterFacialHairStyles', 'BarberShopStyle', 'NameGen', 'CreatureModelData', 'CreatureDisplayInfo']
tops = ['patch-ZZZ.MPQ', 'patch-ZZZA.MPQ', 'patch-ZZZC.MPQ', 'patch-ZZZD.MPQ']
for n in names:
    f = 'DBFilesClient' + BS + n + '.dbc'
    has = []
    for p in C.paths:
        m = C.mpq(p)
        if m is not None and m.has(f):
            has.append(p.split(BS)[-1])
    print(n, has[-6:])
print([p.split(BS)[-1] for p in C.paths][-12:])

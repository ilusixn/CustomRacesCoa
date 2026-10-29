#!/usr/bin/env python3
"""Make ANY creature .m2 (WotLK 3.3.5, v264) work as a player race body:
 - fills every missing player animation from the closest one it has (casts use attack anims when it has no cast anim)
 - adds weapon (1,2), shield (0) and back (12) attachment points from its spell-hand / chest-back points
Usage:  py -3 playerize_any_model.py  In.m2  Out.m2      (copy the .skin files next to Out.m2 with the new name: OutName00.skin ...)
Then change the body textures in the .m2 from creature type 11/12 to 1/8 (CharSections skin) if it should use race skin colours."""
import sys, anim_alias as A
d, rep = A.playerize(open(sys.argv[1], 'rb').read()); open(sys.argv[2], 'wb').write(d); print(rep)

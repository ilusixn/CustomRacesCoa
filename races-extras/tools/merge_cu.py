import sys
sys.path.insert(0, r'C:\CoA-Build\kv')
from merge_util import Merger
m = Merger(r'C:\CoA-Build\coa64\WoW\Common\ComponentCore\ComponentUtils.cpp')
# includes
m.replace('#include "WoW/Common/ComponentCore/ComponentUtils.h"\n', '#include "WoW/Common/ComponentCore/ComponentUtils.h"\n#include "WoW/Common/ComponentCore/RetailRace.h"\n', optional=True)
m.replace('int32_t ComponentGetNumVariations(st_race* lookup, int32_t raceID, int32_t sexID, CHARTEXTUREVARIATIONS variation) {\n',
          'int32_t ComponentGetNumVariations(st_race* lookup, int32_t raceID, int32_t sexID, CHARTEXTUREVARIATIONS variation) {\n    const int32_t retail = RetailRaceSectionCount(raceID, sexID, variation, false);\n\n    if (retail >= 0) {\n        return retail;\n    }\n\n')
m.replace('int32_t ComponentGetNumColors(st_race* lookup, int32_t raceID, int32_t sexID, CHARTEXTUREVARIATIONS variation, int32_t variationIndex) {\n',
          'int32_t ComponentGetNumColors(st_race* lookup, int32_t raceID, int32_t sexID, CHARTEXTUREVARIATIONS variation, int32_t variationIndex) {\n    const int32_t retail = RetailRaceSectionCount(raceID, sexID, variation, true);\n\n    if (retail >= 0) {\n        return retail;\n    }\n\n')
m.replace('bool ComponentValidateBase(st_race* lookup, int32_t raceID, int32_t sexID, CHARTEXTUREVARIATIONS variation, int32_t variationIndex, int32_t colorIndex) {\n',
          'bool ComponentValidateBase(st_race* lookup, int32_t raceID, int32_t sexID, CHARTEXTUREVARIATIONS variation, int32_t variationIndex, int32_t colorIndex) {\n    RetailRaceSectionArguments(raceID, sexID, &variationIndex, &colorIndex);\n')
m.replace('CharSectionsRec* ComponentGetSectionsRecord(st_race* lookup, int32_t raceID, int32_t sexID, CHARTEXTUREVARIATIONS variation, int32_t variationIndex, int32_t colorIndex, bool* found) {\n',
          'CharSectionsRec* ComponentGetSectionsRecord(st_race* lookup, int32_t raceID, int32_t sexID, CHARTEXTUREVARIATIONS variation, int32_t variationIndex, int32_t colorIndex, bool* found) {\n    RetailRaceSectionArguments(raceID, sexID, &variationIndex, &colorIndex);\n')
m.replace('bool ComponentValidateSkin(st_race* lookup, int32_t raceID, int32_t sexID, int32_t classID, int32_t colorIndex, COMPONENT_CONTEXT context) {\n',
          'bool ComponentValidateSkin(st_race* lookup, int32_t raceID, int32_t sexID, int32_t classID, int32_t colorIndex, COMPONENT_CONTEXT context) {\n    const int32_t retail = RetailRaceSectionCount(raceID, sexID, CHARTEXTUREVAR_SKIN, true);\n\n    if (retail >= 0) {\n        return colorIndex >= 0 && colorIndex < retail;\n    }\n\n')
m.save()

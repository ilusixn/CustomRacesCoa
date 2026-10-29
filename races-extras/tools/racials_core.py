import re, shutil
from pathlib import Path
SRC = Path(r'C:\CoA-Build\sources\shotboom\aetheria-server\source\src\server')
CORE = Path(r'C:\CoA-Build\core\src\server')
BAT_CREATURE, BAT_DISPLAY = 20725, 4732        # vanilla Bat (display 4732): the package's own bat (9501110 / 95110) is not in our data
for h, race in (('KyrianHover.h', 107), ('KyrianRacials.h', 107), ('VenthyrRacials.h', 101)):
    s = (SRC / 'shared' / h).read_text()
    s = re.sub(r'(constexpr std::uint8_t Race = )\d+;', r'\g<1>%d;' % race, s)
    s = s.replace('VampireBatDisplay = 95110', 'VampireBatDisplay = %d' % BAT_DISPLAY).replace('VampireBatCreature = 9501110', 'VampireBatCreature = %d' % BAT_CREATURE)
    (CORE / 'shared' / h).write_text(s)
for c in ('kyrian_hover.cpp', 'kyrian_racials.cpp', 'venthyr_racials.cpp'):
    shutil.copy(SRC / 'scripts' / 'Custom' / c, CORE / 'coa' / 'EsteriaRacials' / c)
# core hooks (race-hooks.patch)
p = CORE / 'game' / 'Entities' / 'Player' / 'Player.cpp'
s = p.read_text()
if 'KyrianRacials.h' not in s:
    s = s.replace('#include "Player.h"\n', '#include "Player.h"\n#include "KyrianRacials.h"\n', 1)
    old = '                if (HasAura(SPELL_DIVINE_PROTECTION))\n                    damage /= 2;\n'
    assert old in s
    s = s.replace(old, old + '\n                if (getRace(true) == KyrianRacials::Race && HasAura(KyrianRacials::AscendedWings))\n                    damage = CalculatePct(damage, 100 - KyrianRacials::FallReductionPercent);\n', 1)
    p.write_text(s)
p = CORE / 'game' / 'Spells' / 'Auras' / 'SpellAuraEffects.cpp'
s = p.read_text()
if 'VenthyrRacials.h' not in s:
    s = s.replace('#include "Vehicle.h"\n', '#include "Vehicle.h"\n#include "VenthyrRacials.h"\n', 1)
    old = '    uint32 heal = uint32(caster->SpellHealingBonusDone(caster, GetSpellInfo(), uint32(new_damage * gainMultiplier), DOT, GetEffIndex(), 0.0f, GetBase()->GetStackAmount()));\n'
    assert old in s
    new = ('    uint32 baseHeal = uint32(new_damage * gainMultiplier);\n    if (GetId() == VenthyrRacials::Feast)\n    {\n'
           '        // Preserve half-health points between actual damage ticks. At level one,\n        // four one-damage ticks must heal twice instead of rounding all four to zero.\n'
           '        double const exactHeal = double(new_damage) * gainMultiplier + m_venthyrFeastLeechRemainder;\n        baseHeal = uint32(exactHeal);\n        m_venthyrFeastLeechRemainder = exactHeal - baseHeal;\n    }\n\n'
           '    uint32 heal = uint32(caster->SpellHealingBonusDone(caster, GetSpellInfo(), baseHeal,\n        DOT, GetEffIndex(), 0.0f, GetBase()->GetStackAmount()));\n')
    s = s.replace(old, new, 1)
    p.write_text(s)
p = CORE / 'game' / 'Spells' / 'Auras' / 'SpellAuraEffects.h'
s = p.read_text()
if 'm_venthyrFeastLeechRemainder' not in s:
    old = '    uint32 m_tickNumber;\n'
    assert old in s
    s = s.replace(old, old + '    mutable double m_venthyrFeastLeechRemainder = 0.0;\n', 1)
    p.write_text(s)
# registration
p = CORE / 'coa' / 'EsteriaRacials' / 'CoaRacialSync.cpp'
s = p.read_text()
if 'AddSC_kyrian_racials' not in s:
    s = s.replace('    void AddSC_broken_racials();\n', '    void AddSC_broken_racials();\n    void AddSC_kyrian_hover();\n    void AddSC_kyrian_racials();\n    void AddSC_venthyr_racials();\n', 1)
    s = s.replace('    AddSC_broken_racials();\n}', '    AddSC_broken_racials();\n    AddSC_kyrian_hover();\n    AddSC_kyrian_racials();\n    AddSC_venthyr_racials();\n}', 1)
    p.write_text(s)
# racials table: Aetheria's own set replaces the borrowed ones
p = CORE / 'shared' / 'CoaCustomRacials.h'
s = p.read_text()
s = s.replace('    {107, {110540, 110542, 110500, 110501, 14001075}},', '    {107, {111104, 111105, 111106, 111107, 111108, 111110}},   // Kyrian: Aetheria racials (kyrian_racials.cpp)')
s = s.replace('    {101, {110040, 110041, 110920, 110921, 14001015}},', '    {101, {111200, 111201, 111202, 111203, 111204, 111206, 111209}},   // Venthyr: Aetheria racials (venthyr_racials.cpp)')
p.write_text(s)
print('core patched')

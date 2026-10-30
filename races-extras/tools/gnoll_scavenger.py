import re
p = r'C:\CoA-Build\core\src\server\shared\CoaCustomRacials.h'
s = open(p, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'
a = '{83, {110920, 110921, 110922, 110923, 110924}},'
assert s.count(a) == 1
s = s.replace(a, '{83, {110920, 110921, 110922, 110923, 110924, 14000832}},   // 14000832: Scavenger, the Cannibalize copy')
m = re.search(r'CoaManagedRacialSpells =\s*\{\{(.*?)\}\};', s, re.S)
nums = [int(x) for x in re.findall(r'\d+', m.group(1))]
assert 14000832 not in nums
nums.append(14000832)
nums.sort()
rows = []
for i in range(0, len(nums), 10):
    rows.append('    ' + ', '.join(str(n) for n in nums[i:i + 10]))
body = (',' + nl).join(rows)
s = s[:m.start(1)] + nl + body + nl + s[m.end(1):]
s = re.sub(r'std::array<uint32, \d+> CoaManagedRacialSpells', 'std::array<uint32, %d> CoaManagedRacialSpells' % len(nums), s)
open(p, 'w', encoding='utf-8', newline='').write(s)
print('managed', len(nums))

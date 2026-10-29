import re
P = r'C:\CoA-Build\sources\shotboom\aetheria-client\integration\race-hooks.patch'
U = r'C:\CoA-Build\coa64\WoW\Source\Object\ObjectClient\Unit_C.cpp'
pat = open(P, encoding='utf-8', newline='').read().replace('\r', '').split('\n')
# hunk 4 of Unit_C.cpp: from its @@ line to the next @@
start = next(i for i, l in enumerate(pat) if l.startswith('@@ -3959,9 +3971,362 @@'))
end = next(i for i in range(start + 1, len(pat)) if pat[i].startswith('@@'))
h = pat[start + 1:end]
ctx = [i for i, l in enumerate(h) if not l.startswith('+')]
print('context idx', ctx[:12], len(h))
# context: 3 lines before (return 0; } blank), then additions A, then 3 context lines (VERIFY/OFFSET/header), additions B, then 3 context lines
first_ctx_after = [i for i in ctx if i > 3][0]
A = [l[1:] for l in h[:first_ctx_after] if l.startswith('+')]
second = [i for i in ctx if i > first_ctx_after + 2][0]
B = [l[1:] for l in h[first_ctx_after + 3:second] if l.startswith('+')]
print(len(A), len(B))
b = open(U, 'rb').read().decode('utf-8')
def ins_after(sig, add):
    global b
    i = b.index(sig); j = b.index('\n', i) + 1
    b = b[:j] + add.replace('\n', '\r\n') + b[j:]
ins_after('void CGUnit_C::AnimEndHandler(CM2Model* model, uint32_t boneId, uint32_t sequenceId, int32_t interrupted, uint32_t time) {',
'''    if (interrupted && model == this->GetObjectModel() && boneId == 0xFFFFFFFF &&
        this->m_kyrianFlightTransition == sequenceId) {
        KyrianFlightAnimation::Trace("interrupted", this, this->m_kyrianFlightTransition, this->m_animationState);
        this->m_kyrianFlightTransition = 0;
    }
''')
ins_after('void CGUnit_C::OnBaseAnimEnd(CM2Model* model, uint32_t boneId, uint32_t sequenceId) {',
'''    if (model == this->GetObjectModel() && boneId == 0xFFFFFFFF &&
        this->m_venthyrHoverTransition == sequenceId) {
        this->m_venthyrHoverTransition = 0;
        this->m_animationState &= ~0x40000u;
        this->m_animationState |= 0x70;
        this->UpdateBaseAnimation(16, -1);
        return;
    }
    if (this->UsesNativeKyrianFlightModel(model) && this->m_kyrianFlightTransition == sequenceId) {
        if (sequenceId == KyrianFlightAnimation::TakeoffFinish) {
            this->FinishKyrianTakeoff();
            return;
        }
        this->m_kyrianFlightTransition = 0;
        if (sequenceId == KyrianFlightAnimation::TakeoffStart &&
            this->HasAuraBySpellId(KyrianFlightAnimation::Wings) &&
            !this->HasAuraBySpellId(KyrianFlightAnimation::Descent)) {
            this->StartKyrianFlightTransition(KyrianFlightAnimation::TakeoffFinish);
            if (this->m_kyrianFlightTransition) return;
        }
        this->UpdateKyrianFlightTransition();
        if (!this->m_kyrianFlightTransition) this->UpdateBaseAnimation(16, -1);
        return;
    }
''')
old = '    if (time - CGUnit_C::s_flightStatusTime > 2000 && this->m_obj->m_guid'
assert old in b
b = b.replace(old, '    if (!this->m_kyrianTakeoffLiftState && time - CGUnit_C::s_flightStatusTime > 2000 && this->m_obj->m_guid', 1)
sig = '// VERIFY: fea936b3 manual\r\n// OFFSET: 0x007176F0\r\nuint32_t CGUnit_C::ResolveAnimForModel'
if sig not in b: sig = sig.replace('\r\n', '\n')
i = b.index(sig)
b = b[:i] + '\r\n'.join(A) + '\r\n' + b[i:]
j = b.index('uint32_t CGUnit_C::ResolveAnimForModel(uint32_t animId, CM2Model* model) {')
k = b.index('    int32_t tier = this->m_animTier;', j)
b = b[:k] + '\r\n'.join(B) + '\r\n' + b[k:]
open(U, 'wb').write(b.encode('utf-8'))
print('done')

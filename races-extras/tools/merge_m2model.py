"""Port the skin-parent (collection models sharing a parent skeleton) and attachment-frame machinery of Esteria's M2Model.cpp."""
import re, sys
sys.path.insert(0, r'C:\CoA-Build\kv')
from merge_util import Merger
STR = open(r'C:\CoA-Build\sources\esteria\wowdecomp\Engine\Source\Model2\M2Model.cpp', 'rb').read().decode('utf-8').replace('\r\n', '\n')
m = Merger(r'C:\CoA-Build\coa64\Engine\Source\Model2\M2Model.cpp')

# 1. helpers: the anonymous namespace after s_ribbonBlendModes
a = STR.index('namespace {\n// Keep optional collection state')
b = STR.index('// STATIC: 0x00D4118C')
helpers = STR[a:b]
m.replace('static const uint32_t s_ribbonBlendModes[] = { 0, 1, 2, 10, 3, 4, 5 };\n',
          'static const uint32_t s_ribbonBlendModes[] = { 0, 1, 2, 10, 3, 4, 5 };\n\n' + helpers.rstrip('\n') + '\n')
# includes
m.replace('#include "Engine/Source/Model2/M2Model.h"\n',
          '#include "Engine/Source/Model2/M2Model.h"\n#include <cmath>\n#include <map>\n#include <mutex>\n#include <vector>\n')
# 2. ClearSkinParents hooks
m.replace('void CM2Model::DetachFromParent() {\n', 'void CM2Model::DetachFromParent() {\n    ClearSkinParents(this);\n')
m.replace('        if (model->m_attachmentId == id) {\n            if (model->m_attachPrev) {',
          '        if (model->m_attachmentId == id) {\n            ClearSkinParents(model);\n            if (model->m_attachPrev) {')
m.replace('void CM2Model::FreeInternalResources() {\n', 'void CM2Model::FreeInternalResources() {\n    ClearSkinParents(this);\n')
m.replace('CM2Model::~CM2Model() {\n', 'CM2Model::~CM2Model() {\n    ClearSkinParents(this);\n')
# 3. pose copy + attachment frame call sites
m.replace('    *this->m_boneMatrices = this->m_matrixF4;\n\n    for (uint32_t i = 0; i < data->m_textureWeights.m_count; i++) {',
          '    *this->m_boneMatrices = this->m_matrixF4;\n\n    CopySkinParentPose(this);\n\n    for (uint32_t i = 0; i < data->m_textureWeights.m_count; i++) {')
m.replace('            boneMatrix.d3 = 1.0f;\n        }\n    }\n\n    for (uint32_t i = 0; i < data->m_colors.m_count; i++) {',
          '            boneMatrix.d3 = 1.0f;\n        }\n    }\n\n    CopySkinParentPose(this);\n\n    for (uint32_t i = 0; i < data->m_colors.m_count; i++) {')
m.replace('        if (child->m_fFlags & 0x1000) {\n            child->AnimateMTSimple(&matrix,',
          '        ApplyAttachmentFrame(child, this, matrix);\n        if (child->m_fFlags & 0x1000) {\n            child->AnimateMTSimple(&matrix,')
# 4. the four methods
a = STR.index('bool CM2Model::SetSkinParent(CM2Model* parent) {')
b = STR.index('// VERIFY: c5304a4f auto\n// OFFSET: 0x008317E0')
skin = STR[a:b]
a = STR.index('bool CM2Model::CopyAttachmentFrameFrom(const CM2Model* source) {')
b = STR.index('// VERIFY: 767a1eb1 auto\n// OFFSET: 0x004D8630')
frame = STR[a:b]
m.s = m.s.rstrip('\n') + '\n\n// ---- Esteria: collection models sharing a parent skeleton, and attachment frames\n' + skin + frame
m.save()

h = Merger(r'C:\CoA-Build\coa64\Engine\Source\Model2\M2Model.h')
h.replace('    void ReleaseBodyShapeBuffer();', '    bool SetSkinParent(CM2Model* parent);\n    bool HasSkinParent(const CM2Model* parent) const;\n    bool SetAttachmentFrame(uint32_t bone, const C3Vector& position, float scale = 1.0f);\n    bool CopyAttachmentFrameFrom(const CM2Model* source);\n    void ReleaseBodyShapeBuffer();', optional=True)
h.save()

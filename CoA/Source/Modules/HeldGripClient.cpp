#include "CoA/Source/Modules/HeldGripClient.h"
#include "CoA/Source/Modules/HeldGrip.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "Engine/Source/Model2/M2Model.h"
#include <unordered_set>

namespace CoA::HeldGripClient {
    namespace {
        const std::unordered_set<int32_t>& HeldDisplays() {
            static std::unordered_set<int32_t> s_displays;
            static int32_t s_scanned = -1;

            if (s_scanned != g_itemDB.m_numRecords) {
                s_displays.clear();

                for (int32_t i = 0; i < g_itemDB.m_numRecords; ++i) {
                    const ItemRec& item = g_itemDB.m_records[i];

                    if (HeldGrip::HeldLike(item.m_classID, item.m_subclassID, item.m_inventoryType)) {
                        s_displays.insert(item.m_displayInfoID);
                    }
                }

                s_scanned = g_itemDB.m_numRecords;
            }

            return s_displays;
        }
    }

    bool HeldDisplay(int32_t displayId) {
        return displayId > 0 && HeldDisplays().count(displayId) != 0;
    }

    GEOCOMPONENTLINKS OffHandLink(CM2Model* model, int32_t displayId) {
        const auto held = static_cast<GEOCOMPONENTLINKS>(HeldGrip::kLink);

        if (model && HeldDisplay(displayId) && model->HasAttachment(held)) {
            return held;
        }

        return ATTACH_HANDLEFT;
    }
}

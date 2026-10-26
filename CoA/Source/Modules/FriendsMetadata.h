#ifndef COA_SOURCE_MODULES_FRIENDSMETADATA_H
#define COA_SOURCE_MODULES_FRIENDSMETADATA_H

// AscFriendsMetadata.cpp: a name -> string map (unordered_map 0x10BE2520) the server fills with SMSG_FRIEND_ADDON_LIST
// (0x9C0, FUN_102388c0): u32 count, then count x { u32 len + name bytes, u32 len + value bytes }. A value that differs
// from the stored one -- an unknown name reads as "" (FUN_10238760) -- replaces it (FUN_10239080) and fires
// FRIEND_METADATA_CHANGED(name, value). Globals GetFriendsMetadata (FUN_10238590) and SetFriendsMetadata
// (FUN_10238e50, CMSG 0x556). Nothing clears the map.
namespace CoA::FriendsMetadata {
    void Init();
    // Tests only.
    void Reset();
}

#endif

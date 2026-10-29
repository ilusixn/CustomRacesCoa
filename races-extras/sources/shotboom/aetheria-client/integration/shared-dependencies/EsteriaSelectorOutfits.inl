// Native body fitting preserves the creature atlas and applies player clothing
// through a UV map. Hair and wings retain their authored replacement channels.
struct SelectorOutfit {
    std::array<unsigned,12> gear{};
    Fields fields{};
    std::shared_ptr<void> texture;
    std::shared_ptr<void> robeTexture;
    bool hasRobe=false;
    bool hasShoes=false;
    bool hasGreaves=false;
    bool hasGloves=false;
    unsigned gloveStyle=0;
    unsigned bootStyle=0;
    std::uint64_t hash=0;
};
auto& selectorOutfits=*new std::unordered_map<CCharacterComponent*,SelectorOutfit>;
auto& pendingSelectorBind=*new std::unordered_set<CCharacterComponent*>;

void TraceSelectorMaterials(CCharacterComponent* c, unsigned race) {
    static std::unordered_set<std::string> logged;
    auto* model=c->m_data.m_model;
    if(!model||!model->m_shared||!model->m_shared->m_data||!model->m_shared->m_skinData)return;
    auto* data=model->m_shared->m_data;auto* skin=model->m_shared->m_skinData;
    std::string key=model->m_shared->m_fileName;key.push_back('|');key+=std::to_string(race);
    if(!logged.insert(key).second)return;
    TraceSelector("snapshot race=%u component=%p model=%p file=%s textures=%u batches=%u sections=%u",race,c,model,
        model->m_shared->m_fileName,data->m_textures.m_count,skin->m_batches.m_count,skin->m_skinSections.m_count);
    auto relevant=[&](unsigned type,char const* path) {
        if(race==62)return type==1||type==11||type==12||type==14;
        if(race==46)return type==8;
        return path&&std::strstr(path,"FemaleJewelry");
    };
    for(unsigned i=0;i<data->m_textures.m_count;++i) {
        auto const& tex=data->m_textures[i];auto* path=tex.m_filename.Data();
        if(relevant(tex.m_type,path))TraceSelector("snapshot texture race=%u index=%u type=%u handle=%p path=%.*s",race,i,tex.m_type,
            model->m_textures[i],static_cast<int>(tex.m_filename.m_count),path?path:"");
    }
    for(unsigned i=0;i<skin->m_batches.m_count;++i) {
        auto const& batch=skin->m_batches[i];
        if(batch.m_skinSectionIndex>=skin->m_skinSections.m_count||batch.m_materialIndex>=data->m_materials.m_count
            ||batch.m_textureComboIndex+batch.m_textureCount>data->m_textureCombos.m_count)continue;
        auto const& section=skin->m_skinSections[batch.m_skinSectionIndex];
        unsigned id=section.m_skinSectionId&65535u;
        if(race==46&&id!=3801&&id!=3802&&id!=3803)continue;
        for(unsigned j=0;j<batch.m_textureCount;++j) {
            unsigned textureIndex=data->m_textureCombos[batch.m_textureComboIndex+j];
            if(textureIndex>=data->m_textures.m_count)continue;
            auto const& tex=data->m_textures[textureIndex];auto* path=tex.m_filename.Data();
            if(!relevant(tex.m_type,path))continue;
            TraceSelector("snapshot batch race=%u batch=%u section=%u geoset=%u texIndex=%u type=%u blend=%u uvCombo=%u path=%.*s",
                race,i,id,batch.m_geosetIndex,textureIndex,tex.m_type,data->m_materials[batch.m_materialIndex].m_blendMode,
                batch.m_textureCoordComboIndex,static_cast<int>(tex.m_filename.m_count),path?path:"");
        }
    }
}

bool SelectorLayerPixels(void* handle,AccessoryPixelSource& source) {
    TCTEXTUREINFO info{};
    if(!handle || !TextureCacheGetInfo(handle,info,1))return false;
    auto* entry=static_cast<CACHEENTRY*>(handle);
    if(!entry->m_data||entry->m_loadFailed||entry->m_size<148)return false;
    auto* bytes=reinterpret_cast<unsigned char const*>(entry->m_data);
    source.bytes.assign(bytes,bytes+entry->m_size);
    unsigned pitch=0;void* pixels=nullptr;
    return source.Mip(0,pitch,pixels);
}
void SelectorOver(unsigned char* dst,unsigned char const* src) {
    unsigned a=src[3],prior=dst[3],total=a*255+prior*(255-a);
    for(unsigned channel=0;channel<3;++channel)
        dst[channel]=static_cast<unsigned char>(total?(src[channel]*a*255+dst[channel]*prior*(255-a)+total/2)/total:0);
    dst[3]=static_cast<unsigned char>((total+127)/255);
}
bool SealNativeBootFoot(std::vector<unsigned char>& pixels) {
    // Extend the authored boot colors to the foot chart border. This prevents
    // a transparent edge from exposing the baked foot/shoe underneath the shell.
    std::array<int,64> rows{};rows.fill(-1);
    for(int y=0;y<64;++y) {
        std::array<int,256> valid{};unsigned count=0;
        auto* row=pixels.data()+4*((448+y)*512+256);
        for(int x=0;x<256;++x)if(row[4*x+3]>=128)valid[count++]=x;
        if(!count)continue;
        rows[y]=y;
        auto original=std::vector<unsigned char>(row,row+1024);
        for(int x=0;x<256;++x) {
            if(row[4*x+3]<128) {
                int nearest=valid[0];
                for(unsigned i=1;i<count;++i)if(std::abs(valid[i]-x)<std::abs(nearest-x))nearest=valid[i];
                std::copy_n(original.data()+4*nearest,3,row+4*x);
            }
            row[4*x+3]=255;
        }
    }
    bool any=false;for(auto row:rows)any=any||row>=0;if(!any)return false;
    for(int y=0;y<64;++y)if(rows[y]<0) {
        int nearest=-1;for(int i=0;i<64;++i)if(rows[i]>=0&&(nearest<0||std::abs(i-y)<std::abs(nearest-y)))nearest=i;
        std::copy_n(pixels.data()+4*((448+nearest)*512+256),1024,pixels.data()+4*((448+y)*512+256));
    }
    return true;
}
void ExtendNativeClothRegion(std::vector<unsigned char>& pixels,int left,int top,int width,int height) {
    std::vector<int> columns(width,-1);
    for(int x=0;x<width;++x) {
        std::vector<int> rows;
        for(int y=0;y<height;++y)if(pixels[4*((top+y)*512+left+x)+3]>=160)rows.push_back(y);
        if(rows.empty())continue;columns[x]=x;
        for(int y=0;y<height;++y) {
            auto* dst=pixels.data()+4*((top+y)*512+left+x);
            if(dst[3]>=160)continue;
            int nearest=rows[0];for(auto row:rows)if(std::abs(row-y)<std::abs(nearest-y))nearest=row;
            std::copy_n(pixels.data()+4*((top+nearest)*512+left+x),4,dst);dst[3]=255;
        }
    }
    for(int x=0;x<width;++x)if(columns[x]<0) {
        int nearest=-1;for(int i=0;i<width;++i)if(columns[i]>=0&&(nearest<0||std::abs(i-x)<std::abs(nearest-x)))nearest=i;
        if(nearest<0)continue;
        for(int y=0;y<height;++y)std::copy_n(pixels.data()+4*((top+y)*512+left+nearest),4,pixels.data()+4*((top+y)*512+left+x));
    }
}
// Native creatures have a different body UV atlas from player equipment. Bake
// the equipment into that atlas instead of substituting it for skin or wings.
bool SelectorNativeBodyPixels(CCharacterComponent* c,std::vector<unsigned char>& outfit,
    unsigned& width,unsigned& height,std::vector<unsigned char> const* gloveless=nullptr) {
    struct FittingMap {unsigned width=0,height=0;std::vector<unsigned> pixels;std::vector<unsigned char> coverage;};
    static std::array<FittingMap,4> maps;
    auto const& prefs=c->m_data.m_preferences;
    if((prefs.m_raceID!=60&&prefs.m_raceID!=61)||prefs.m_sexID>1)return false;
    auto& map=maps[(prefs.m_raceID-60)*2+prefs.m_sexID];
    if(map.pixels.empty()) {
        wchar_t path[128]{};
        swprintf_s(path,L"Data\\Retail\\Assets\\EquipmentFitting\\%u-%u.bin",prefs.m_raceID,prefs.m_sexID);
        FILE* file=CatalogFile(path);if(!file)return false;
        unsigned header[6]{};
        bool valid=std::fread(header,sizeof(header),1,file)==1&&header[0]==0x3146554e&&header[1]==1
            &&header[2]&&header[2]<=1024&&header[3]&&header[3]<=1024&&header[4]==512&&header[5]==512;
        if(valid) {
            map.width=header[2];map.height=header[3];map.pixels.resize(map.width*map.height);
            valid=std::fread(map.pixels.data(),sizeof(unsigned),map.pixels.size(),file)==map.pixels.size();
            valid=valid&&std::all_of(map.pixels.begin(),map.pixels.end(),[](unsigned p){
                return p==0xffffffffu||((p&65535u)<512&&(p>>16)<512);});
            if(valid&&((prefs.m_raceID==60&&prefs.m_sexID==1)||prefs.m_raceID==61)) {
                map.coverage.resize(map.pixels.size());
                if(std::fread(map.coverage.data(),1,map.coverage.size(),file)!=map.coverage.size()
                    || !std::all_of(map.coverage.begin(),map.coverage.end(),[](unsigned char value){return value<=4;}))map.coverage.clear();
            }
        }
        std::fclose(file);if(!valid){map=FittingMap{};return false;}
    }
    char const* skinPath=nullptr;
    for(auto const& record:RetailSelections(prefs.m_raceID,prefs.m_sexID)) {
        if(record.gender!=prefs.m_sexID||record.kind!=1||record.target!=11)continue;
        bool matches=true;
        for(unsigned key:record.choices) {
            if(key==0xffffffffu)continue;
            const unsigned option=key&65535u,value=key>>16;
            if(option==0)matches=matches&&value==unsigned(prefs.m_raceID==60 ? HighmountainFields(c)[0] : prefs.m_skinID);
            else matches=false;
        }
        if(matches) {
            skinPath=record.path;break;
        }
    }
    if(!skinPath)return false;
    AccessoryPixelSource skin;
    // Keep the authored skin detail and underwear beneath equipped clothing.
    skin.bytes=HaranirBlpData(skinPath);
    if(prefs.m_raceID==61&&c->m_itemDisplayID[ITEMSLOT_PANTS]) {
        // Only the baked trouser regions are replaced, using this palette's
        // own forearm skin detail. Face, torso and underwear retain their atlas.
        char bareLegs[160]{};
        sprintf_s(bareLegs,"custom\\equipment\\native\\legskin-v3-61-%u-%u.blp",prefs.m_sexID,unsigned(prefs.m_skinID));
        auto detailedSkin=HaranirBlpData(bareLegs);
        if(!detailedSkin.empty())skin.bytes=std::move(detailedSkin);
    }
    unsigned pitch=0;void* decoded=nullptr;
    if(!skin.Mip(0,pitch,decoded)||skin.Word(12)!=map.width||skin.Word(16)!=map.height)return false;
    std::vector<unsigned char> composed(map.width*map.height*4);
    auto* native=static_cast<unsigned char const*>(decoded);
    auto extended=outfit;
    if(!map.coverage.empty()) {
        if(c->m_itemDisplayID[ITEMSLOT_CHEST]) {
            ExtendNativeClothRegion(extended,256,0,256,192);
            ExtendNativeClothRegion(extended,0,0,256,128);
        }
        if(c->m_itemDisplayID[ITEMSLOT_PANTS])ExtendNativeClothRegion(extended,256,192,256,128);
        if(prefs.m_raceID==61&&c->m_itemDisplayID[ITEMSLOT_BOOTS])ExtendNativeClothRegion(extended,256,320,256,128);
    }
    unsigned long long tintSum[3]{};unsigned tintCount=0;
    if(prefs.m_raceID==61&&c->m_itemDisplayID[ITEMSLOT_BOOTS]) {
        for(unsigned y=448;y<512;++y)for(unsigned x=256;x<512;++x) {
            auto* color=outfit.data()+4*(y*512+x);if(color[3]<128)continue;
            for(unsigned k=0;k<3;++k)tintSum[k]+=color[k];++tintCount;
        }
    }
    for(unsigned y=0;y<map.height;++y) {
        std::copy_n(native+y*pitch,map.width*4,composed.data()+y*map.width*4);
        for(unsigned x=0;x<map.width;++x) {
            // Body albedo alpha stores Retail material data, not skin coverage.
            // The native body is opaque; uncovered armor pixels retain its RGB.
            composed[4*(y*map.width+x)+3]=255;
            unsigned sample=map.pixels[y*map.width+x];if(sample==0xffffffffu)continue;
            if(prefs.m_raceID==61&&!map.coverage.empty()&&map.coverage[y*map.width+x]==4&&c->m_itemDisplayID[ITEMSLOT_BOOTS]) {
                // Carry the greave's lower edge over the ankle until the native
                // shoe collar begins. Do not tint this band as bare skin.
                auto* ankle=extended.data()+4*(447*512+(sample&65535u));
                if(ankle[3])SelectorOver(composed.data()+4*(y*map.width+x),ankle);
                continue;
            }
            if(prefs.m_raceID==61&&((sample>>16)>=448||(!map.coverage.empty()&&map.coverage[y*map.width+x]==3))) {
                // Greaves stop at the original dress shoes. A restrained tint
                // follows the equipped boots while preserving shoe detail.
                if(tintCount) {
                    auto* color=composed.data()+4*(y*map.width+x);
                    unsigned shade=(unsigned(color[0])+color[1]+color[2])/3;
                    for(unsigned k=0;k<3;++k)color[k]=static_cast<unsigned char>(std::min(255ull,
                        (color[k]*65ull+shade*(tintSum[k]/tintCount)*35ull/128ull)/100ull));
                }
                continue;
            }
            auto const* layer=outfit.data()+4*((sample>>16)*512+(sample&65535u));
            if(gloveless&&c->m_itemDisplayID[ITEMSLOT_GLOVES]&&(sample&65535u)<256&&(sample>>16)>=128&&(sample>>16)<256) {
                float compression=prefs.m_raceID==60?.82f:.78f;
                int gloveY=256-int((256-int(sample>>16))/compression);
                if(gloveY>=128)layer=outfit.data()+4*(gloveY*512+(sample&65535u));
                else layer=gloveless->data()+4*((sample>>16)*512+(sample&65535u));
            }
            if(!map.coverage.empty()&&layer[3]<160) {
                auto mask=map.coverage[y*map.width+x];
                if((mask==1&&(sample>>16)<128&&c->m_itemDisplayID[ITEMSLOT_CHEST])||(mask==2&&c->m_itemDisplayID[ITEMSLOT_PANTS]))
                    layer=extended.data()+4*((sample>>16)*512+(sample&65535u));
            }
            if(layer[3])SelectorOver(composed.data()+4*(y*map.width+x),layer);
        }
    }
    width=map.width;height=map.height;outfit=std::move(composed);return true;
}
void FitNativePlayerHelmet(CCharacterComponent* c) {
    auto const& prefs=c->m_data.m_preferences;
    if ((prefs.m_raceID!=60&&prefs.m_raceID!=61)||prefs.m_sexID>1||!c->m_itemDisplayID[ITEMSLOT_HEAD])return;
    auto* parent=c->m_data.m_model;
    if (!parent||!(parent->m_fFlags&1)||!parent->m_shared||!parent->m_shared->m_data)return;
    auto* data=parent->m_shared->m_data;
    if(data->m_boneIndicesById.m_count<=6)return;
    unsigned head=data->m_boneIndicesById[6];if(head>=data->m_bones.m_count)return;
    auto const& pivot=data->m_bones[head].m_pivot;
    float scale=pivot.z/(prefs.m_sexID?1.7118653f:1.8430222f);
    C3Vector crown(pivot.x+(prefs.m_sexID?.08508668f:.0625562f)*scale,pivot.y,
        pivot.z+(prefs.m_sexID?.18511772f:.18413198f)*scale);
    for(auto* helmet=parent->m_attachList;helmet;helmet=helmet->m_attachNext) {
        if(helmet->m_attachmentId!=ATTACH_HELM)continue;
        // Keep attachment transforms in the model's native space. Setting the
        // attached model's world transform here can corrupt its parent-relative
        // matrix and crash when the roster recreates the character model.
        helmet->SetAttachmentFrame(head,crown);
    }
}
void FitNativePlayerShoulders(CCharacterComponent* c) {
    if(c->m_data.m_preferences.m_raceID!=60||!c->m_data.m_model)return;
    auto* parent=c->m_data.m_model;
    auto* data=parent->m_shared?parent->m_shared->m_data:nullptr;
    if(!data||data->m_bones.m_count!=251)return;
    float scale=data->m_bones[29].m_pivot.z/1.8430222f;
    for(auto* pad=parent->m_attachList;pad;pad=pad->m_attachNext) {
        if(pad->m_attachmentId!=5&&pad->m_attachmentId!=6)continue;
        bool left=pad->m_attachmentId==6;
        auto position=data->m_bones[left?25:28].m_pivot;
        position.x+=.020f*scale;position.y-=(left?1.f:-1.f)*.068f*scale;position.z+=.065f*scale;
        // Use the clavicle's native frame, avoiding the extra rotated cosmetic
        // socket bones. Both sides receive the same anatomical offset.
        pad->SetAttachmentFrame(left?17:19,position,c->m_data.m_preferences.m_sexID?1.59f:1.80f);
    }
}
void BindSelectorOutfit(CCharacterComponent* c) {
    auto race=c->m_data.m_preferences.m_raceID;
    if(race!=60&&race!=61&&race!=62&&race!=63&&race!=64&&race!=69&&race!=70&&!(race==72&&!c->m_data.m_preferences.m_sexID))return;
    bool nativeBody=race==60||race==61;
    unsigned textureType=nativeBody?11:race==62?1:14;
    FitNativePlayerHelmet(c);
    FitNativePlayerShoulders(c);
    static bool busy=false;if(busy)return;
    struct Guard{bool& value;Guard(bool& v):value(v){value=true;}~Guard(){value=false;}}guard(busy);
    auto& state=selectorOutfits[c];auto fields=HighmountainFields(c);
    std::array<unsigned,12> gear{};std::copy_n(c->m_itemDisplayID,12,gear.begin());
    ItemDisplayInfoRec chestRecord{},pantsRecord{};
    bool chestRobe=nativeBody&&gear[ITEMSLOT_CHEST]&&g_itemDisplayInfoDB.GetRecord(gear[ITEMSLOT_CHEST],&chestRecord)&&chestRecord.m_geosetGroup[2]!=0;
    bool legSkirt=nativeBody&&gear[ITEMSLOT_PANTS]&&g_itemDisplayInfoDB.GetRecord(gear[ITEMSLOT_PANTS],&pantsRecord)&&pantsRecord.m_geosetGroup[2]!=0;
    bool robe=chestRobe||legSkirt;
    bool shoes=nativeBody&&race==60&&gear[ITEMSLOT_BOOTS]&&(c->m_section[SECTION_FOOT].m_layerMask&1);
    bool greaves=nativeBody&&race==61&&gear[ITEMSLOT_BOOTS];
    bool gloves=nativeBody&&gear[ITEMSLOT_GLOVES];
    unsigned gloveStyle=0;ItemDisplayInfoRec gloveRecord{};
    if(gloves&&g_itemDisplayInfoDB.GetRecord(gear[ITEMSLOT_GLOVES],&gloveRecord))gloveStyle=std::min(3u,static_cast<unsigned>(gloveRecord.m_geosetGroup[0]));
    unsigned bootStyle=0;ItemDisplayInfoRec bootRecord{};
    if(nativeBody&&gear[ITEMSLOT_BOOTS]&&g_itemDisplayInfoDB.GetRecord(gear[ITEMSLOT_BOOTS],&bootRecord))
        bootStyle=std::min(4u,static_cast<unsigned>(bootRecord.m_geosetGroup[0]));
    unsigned upperLegGeo=nativeBody&&!robe&&pantsRecord.m_geosetGroup[0]?(pantsRecord.m_geosetGroup[0]>2?20104u:20102u):0;
    unsigned lowerLegGeo=nativeBody&&!robe&&!bootStyle&&pantsRecord.m_geosetGroup[1]?(pantsRecord.m_geosetGroup[1]>1?20113u:20112u):0;
    ItemDisplayInfoRec shirtRecord{};
    unsigned waistGeo=nativeBody&&!robe&&gear[ITEMSLOT_SHIRT]&&g_itemDisplayInfoDB.GetRecord(gear[ITEMSLOT_SHIRT],&shirtRecord)&&shirtRecord.m_geosetGroup[1]?20122u:0;
    ItemDisplayInfoRec beltRecord{};
    unsigned beltGeo=nativeBody&&gear[ITEMSLOT_BELT]&&g_itemDisplayInfoDB.GetRecord(gear[ITEMSLOT_BELT],&beltRecord)&&beltRecord.m_geosetGroup[0]?20132u:0;
    bool legMeshes=upperLegGeo||lowerLegGeo||waistGeo||beltGeo;
    auto bindLegMeshes=[&]() {
        if(!nativeBody)return;
        c->m_data.m_model->SetGeometryVisible(20100,20139,0);
        c->m_data.m_model->SetGeometryVisible(20200,20200,0);
        // Private anatomy split retains the original triangles when unrobed,
        // and hides only the leg surfaces covered by the animated skirt.
        c->m_data.m_model->SetGeometryVisible(20300,20300,robe?0:1);
        c->m_data.m_model->SetGeometryVisible(0,0,1);
        if(race==61&&(robe||std::any_of(gear.begin(),gear.end(),[](unsigned value){return value!=0;})))
            c->m_data.m_model->SetGeometryVisible(3101,3101,1);
        for(unsigned geo:{upperLegGeo,lowerLegGeo,waistGeo,beltGeo})if(geo)c->m_data.m_model->SetGeometryVisible(geo,geo,1);
    };
    bindLegMeshes();
    if(state.texture && state.gear==gear && state.fields==fields && state.hasRobe==robe && state.hasShoes==shoes && state.hasGreaves==greaves && state.bootStyle==bootStyle&&state.hasGloves==gloves&&state.gloveStyle==gloveStyle) {
        c->m_data.m_model->ReplaceTexture(textureType,reinterpret_cast<CTexture*>(state.texture.get()));
        if(nativeBody) {
            c->m_data.m_model->SetGeometryVisible(1302,1302,robe?1:0);
            c->m_data.m_model->SetGeometryVisible(20002,20034,0);
            if(shoes)c->m_data.m_model->SetGeometryVisible(20010+bootStyle,20010+bootStyle,1);
            if(greaves)c->m_data.m_model->SetGeometryVisible(20020+bootStyle,20020+bootStyle,1);
            if(gloves)c->m_data.m_model->SetGeometryVisible(20030+gloveStyle,20030+gloveStyle,1);
            if((robe||shoes||greaves||gloves||legMeshes)&&state.robeTexture)c->m_data.m_model->ReplaceTexture(15,reinterpret_cast<CTexture*>(state.robeTexture.get()));
        }
        pendingSelectorBind.erase(c);return;
    }
    unsigned dirty=c->m_dirtySections;c->m_dirtySections|=0xff;
    bool loaded=c->ItemsLoaded(1);c->m_dirtySections=dirty;
    if(!loaded){pendingSelectorBind.insert(c);static std::unordered_set<CCharacterComponent*> waiting;if(waiting.insert(c).second)TraceSelector("Sethrak wait component=%p",c);return;}
    std::vector<unsigned char> pixels(512*512*4);
    if(race==62) {
        auto body=HaranirLayerPath(c,0,0);
        AccessoryPixelSource base;if(body)base.bytes=HaranirBlpData(body);
        unsigned pitch=0;void* decoded=nullptr;
        if(!base.Mip(0,pitch,decoded)){TraceSelector("Sethrak base decode failed component=%p",c);pendingSelectorBind.insert(c);return;}
        auto* src=static_cast<unsigned char*>(decoded);
        for(unsigned y=0;y<512;++y)for(unsigned x=0;x<512;++x)
            std::copy_n(src+4*((y*base.Word(16)/512)*base.Word(12)+x*base.Word(12)/512),4,pixels.data()+4*(y*512+x));
    }
    auto gloveless=pixels;
    constexpr unsigned counts[]={2,7,1,5,7,3,6,1};
    for(unsigned region=0;region<8;++region) {
        auto const& row=c->m_section[region];
        auto const& rect=CCharacterComponent::s_sectionInfoRaw[region];
        bool legacyRobe=!nativeBody&&(c->m_flags&0x20)!=0;
        unsigned first=legacyRobe&&(region==5||region==6)?1:0;
        unsigned end=legacyRobe&&region==5?2:legacyRobe&&region==6?4:counts[region];
        for(unsigned layer=first;layer<end;++layer) {
            if(!(row.m_layerMask&(1u<<layer)))continue;
            // Skirt textures live in lower-leg layer 4. Read every native layer
            // and suppress the covered item by display ID, not a layer cutoff.
            if(nativeBody&&robe&&region==6&&row.m_layerItemDisplayId[layer]==gear[ITEMSLOT_BOOTS])continue;
            if(nativeBody&&chestRobe&&region==5&&row.m_layerItemDisplayId[layer]==gear[ITEMSLOT_PANTS])continue;
            AccessoryPixelSource source;
            if(!SelectorLayerPixels(row.m_layerTex[layer],source)){TraceSelector("Sethrak layer decode failed component=%p region=%u layer=%u",c,region,layer);pendingSelectorBind.insert(c);return;}
            auto const& data=source.levels[0];unsigned w=source.Word(12),h=source.Word(16);
            for(unsigned y=0;y<unsigned(rect.m_size.y);++y)for(unsigned x=0;x<unsigned(rect.m_size.x);++x) {
                SelectorOver(pixels.data()+4*((rect.m_pos.y+y)*512+rect.m_pos.x+x),
                    data.data()+4*((y*h/rect.m_size.y)*w+x*w/rect.m_size.x));
                if(!gear[ITEMSLOT_GLOVES]||row.m_layerItemDisplayId[layer]!=gear[ITEMSLOT_GLOVES])
                    SelectorOver(gloveless.data()+4*((rect.m_pos.y+y)*512+rect.m_pos.x+x),
                        data.data()+4*((y*h/rect.m_size.y)*w+x*w/rect.m_size.x));
            }
        }
    }
    if(nativeBody&&race==60&&gear[ITEMSLOT_BOOTS])SealNativeBootFoot(pixels);
    std::shared_ptr<void> robeTexture;
    if(robe||shoes||greaves||gloves||legMeshes) {
        auto garment=pixels;
        if(greaves&&!robe) {
            // Only seal the lower greave edge. Keep the item's upper alpha
            // silhouette so its cuff appears once, at the body's chart height.
            auto sealed=garment;
            ExtendNativeClothRegion(sealed,256,320,256,128);
            for(unsigned y=420;y<448;++y)
                std::copy_n(sealed.data()+4*(y*512+256),256*4,garment.data()+4*(y*512+256));
        }
        for(std::size_t i=0;i<garment.size();i+=4)std::swap(garment[i],garment[i+2]);
        std::uint64_t garmentHash=14695981039346656037ull;
        for(auto value:garment)garmentHash=(garmentHash^value)*1099511628211ull;
        char garmentPath[160]{};
        sprintf_s(garmentPath,"Data\\Retail\\Cache\\Appearance\\Equipment\\garment-v2-%016llx.blp",static_cast<unsigned long long>(garmentHash));
        if(!WriteHaranirBlp(garmentPath,garment,512,512,false)||(robeTexture=LoadTexture(garmentPath))==nullptr) {
            pendingSelectorBind.insert(c);return;
        }
    }
    unsigned width=512,height=512;
    if(nativeBody&&!SelectorNativeBodyPixels(c,pixels,width,height,&gloveless)) {
        pendingSelectorBind.insert(c);return;
    }
    // AccessoryPixelSource returns native BGRA, while WriteHaranirBlp accepts
    // RGBA and converts it to disk BGRA. Convert once before that writer.
    for(std::size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);
    std::uint64_t hash=14695981039346656037ull;
    for(auto value:pixels)hash=(hash^value)*1099511628211ull;
    char path[160]{};
    if(nativeBody)sprintf_s(path,"Data\\Retail\\Cache\\Appearance\\Equipment\\native-v3-%u-%u-%016llx.blp",race,c->m_data.m_preferences.m_sexID,static_cast<unsigned long long>(hash));
    else sprintf_s(path,"DHCache\\Outfits\\v2-%u-%016llx.blp",race,static_cast<unsigned long long>(hash));
    if(!WriteHaranirBlp(path,pixels,width,height,false)){TraceSelector("Sethrak write failed component=%p",c);pendingSelectorBind.insert(c);return;}
    auto texture=LoadTexture(path);if(!texture){TraceSelector("Sethrak load failed component=%p path=%s",c,path);pendingSelectorBind.insert(c);return;}
    state.gear=gear;state.fields=fields;state.texture=texture;state.robeTexture=robeTexture;state.hasRobe=robe;state.hasShoes=shoes;state.hasGreaves=greaves;state.bootStyle=bootStyle;state.hash=hash;
    state.hasGloves=gloves;state.gloveStyle=gloveStyle;
    c->m_data.m_model->ReplaceTexture(textureType,reinterpret_cast<CTexture*>(texture.get()));
    if(nativeBody) {
        c->m_data.m_model->SetGeometryVisible(1302,1302,robe?1:0);
        c->m_data.m_model->SetGeometryVisible(20002,20034,0);
        if(shoes)c->m_data.m_model->SetGeometryVisible(20010+bootStyle,20010+bootStyle,1);
        if(greaves)c->m_data.m_model->SetGeometryVisible(20020+bootStyle,20020+bootStyle,1);
        if(gloves)c->m_data.m_model->SetGeometryVisible(20030+gloveStyle,20030+gloveStyle,1);
        if(robeTexture)c->m_data.m_model->ReplaceTexture(15,reinterpret_cast<CTexture*>(robeTexture.get()));
    }
    pendingSelectorBind.erase(c);
    TraceSelector("Sethrak bound component=%p fields=%u,%u,%u,%u,%u type1=%p path=%s",c,fields[0],fields[1],fields[2],fields[3],fields[4],texture.get(),path);
}

bool BindSelectorStaticTextures(CCharacterComponent* c) {
    unsigned race=c->m_data.m_preferences.m_raceID;
    auto* model=c->m_data.m_model;auto* data=model->m_shared->m_data;
    if(race==4||race==21||race==46||race==62)TraceSelectorMaterials(c,race);
    if((race==4||race==21)&&c->m_data.m_preferences.m_sexID || race==52||race==53||race==46) {
        for(unsigned i=0;i<data->m_textures.m_count;++i) {
            auto const& record=data->m_textures[i];
            if(record.m_type || !record.m_filename.m_count)continue;
            auto* path=record.m_filename.Data();
            if(!path||!std::memchr(path,0,record.m_filename.m_count))continue;
            bool highmountainAccessory=race==46&&(std::strstr(path,"neutral.blp")||std::strstr(path,"armorreflect_hard.blp"));
            if(!highmountainAccessory&&!std::strstr(path,"FemaleJewelry")&&!std::strstr(path,"feather_color"))continue;
            // Retain usable native jewelry textures; the geometry repair chooses
            // the separate vine atlas for the two leafy variants.
            if(std::strstr(path,"FemaleJewelry") && model->m_textures[i]
                && TextureGetGxTex(model->m_textures[i],0,nullptr))continue;
            std::shared_ptr<void> texture;
            if(highmountainAccessory&&std::strstr(path,"neutral.blp")) {
                // The integration model substituted neutral.blp for Retail's
                // replaceable jewelry material (texture target 38 / M2 type 11).
                // Restore the sole authored Jewelry Color choice for each sex.
                auto* jewelry=c->m_data.m_preferences.m_sexID
                    ? "custom\\highmountain\\character\\highmountaintauren\\female\\highmountaintaurenfemale_jewelry_color_4252417.blp"
                    : "custom\\highmountain\\character\\highmountaintauren\\male\\highmountaintaurenmale_jewelry_color_4249745.blp";
                texture=LoadAssetTexture(jewelry);
            } else texture=LoadAssetTexture(path);
            if(!texture)continue;
            auto* handle=reinterpret_cast<CTexture*>(texture.get());
            // Failed optional textures must never block the entire body in
            // CM2Model::IsDrawable. Keep the current slot until upload succeeds.
            if(highmountainAccessory&&!TextureGetGxTex(handle,0,nullptr))continue;
            if(model->m_textures[i]==handle)continue;
            if(model->m_textures[i])HandleClose(reinterpret_cast<HOBJECT>(model->m_textures[i]));
            model->m_textures[i]=reinterpret_cast<CTexture*>(HandleDuplicate(reinterpret_cast<HOBJECT>(handle)));
            model->m_fFlags&=highmountainAccessory?~0x10u:~0x12u;
            if(highmountainAccessory)TraceSelector("HMT authored accessory component=%p slot=%u gender=%u texture=%p ready=%u",c,i,c->m_data.m_preferences.m_sexID,handle,TextureGetGxTex(handle,0,nullptr)?1:0);
        }
    }
    if(race==46) {
        auto const& p=c->m_data.m_preferences;
        auto* extra=c->GetSectionsRecord(VARIATION_SKIN,0,p.m_skinID,nullptr);
        if(extra&&*extra->m_textureName[1]) {
            auto texture=LoadAssetTexture(extra->m_textureName[1]);
            if(texture){
                auto* expected=reinterpret_cast<CTexture*>(texture.get());bool changed=false;
                for(unsigned i=0;i<data->m_textures.m_count;++i)if(data->m_textures[i].m_type==8)changed=model->m_textures[i]!=expected;
                model->ReplaceTexture(8,expected);pendingSelectorBind.erase(c);
                static std::unordered_map<CCharacterComponent*,std::uint64_t> last;
                std::uint64_t sig=std::hash<std::string>{}(extra->m_textureName[1])^reinterpret_cast<std::uintptr_t>(expected)^(std::uint64_t(p.m_skinID)<<32);
                if(changed||last[c]!=sig){last[c]=sig;TraceSelector("HMT extra component=%p gender=%u skin=%u path=%s texture=%p changed=%u",c,p.m_sexID,p.m_skinID,extra->m_textureName[1],expected,changed?1:0);}
                return true;
            }
            TraceSelector("HMT load failed component=%p gender=%u skin=%u path=%s",c,p.m_sexID,p.m_skinID,extra->m_textureName[1]);
        }
        if(!extra||!*extra->m_textureName[1])TraceSelector("HMT section missing component=%p gender=%u skin=%u",c,p.m_sexID,p.m_skinID);
        pendingSelectorBind.insert(c);
        return false;
    }
    return true;
}
